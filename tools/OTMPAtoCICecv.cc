#include "tools/OTMPAtoCICecv.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include <algorithm>
#include <bitset>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTMPAtoCICecv::fCalibrationDescription = "Run electric chain validation test between MPA and CIC";

OTMPAtoCICecv::OTMPAtoCICecv() : Tool() {}

OTMPAtoCICecv::~OTMPAtoCICecv() {}

void OTMPAtoCICecv::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

    fNumberOfIterations   = findValueInSettings<double>("OTMPAtoCICecv_NumberOfIterations", 1000);
    fShiftRegisterPattern = findValueInSettings<double>("OTMPAtoCICecv_ShiftRegisterPattern", 0xAA);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTMPAtoCICecv.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTMPAtoCICecv::ConfigureCalibration() {}

void OTMPAtoCICecv::Running()
{
    LOG(INFO) << "Starting OTMPAtoCICecv measurement.";
    Initialise();
    setMPAshiftRegister();
    runElectricChainValidation();
    LOG(INFO) << "Done with OTMPAtoCICecv.";
    Reset();
}

void OTMPAtoCICecv::Stop(void)
{
    LOG(INFO) << "Stopping OTMPAtoCICecv measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTMPAtoCICecv.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTMPAtoCICecv stopped.";
}

void OTMPAtoCICecv::Pause() {}

void OTMPAtoCICecv::Resume() {}

void OTMPAtoCICecv::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTMPAtoCICecv::setMPAshiftRegister()
{
    auto thePSinterface = static_cast<PSInterface*>(fReadoutChipInterface)->fTheMPA2Interface;

    auto        MPAqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string theMPAqueryFunctionString = "MPAqueryFunction";
    fDetectorContainer->addReadoutChipQueryFunction(MPAqueryFunction, theMPAqueryFunctionString);

    setSameDac("LFSR_data", fShiftRegisterPattern);

    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto theMPA: *theHybrid)
                {
                    thePSinterface->WriteChipRegBits(theMPA, "Control_1", 0x2, "Mask", 0x03); // Enable shift register
                    // if(theMPA->getId() == 8) thePSinterface->WriteChipReg(theMPA, "LFSR_data", fShiftRegisterPattern);
                    // else thePSinterface->WriteChipReg(theMPA, "LFSR_data", 0);
                }
            }
        }
    }
    fDetectorContainer->removeReadoutChipQueryFunction(theMPAqueryFunctionString);
}

void OTMPAtoCICecv::runElectricChainValidation()
{
    auto        MPAqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string theMPAqueryFunctionString = "MPAqueryFunction";
    fDetectorContainer->addReadoutChipQueryFunction(MPAqueryFunction, theMPAqueryFunctionString);

    LOG(INFO) << BOLDYELLOW << "OTMPAtoCICecv::runElectricChainValidation ... start electric chain validation test" << RESET;
    auto theFWinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    auto thePSinterface = static_cast<PSInterface*>(fReadoutChipInterface)->fTheMPA2Interface;

    for(uint8_t slvsCurrent = 1; slvsCurrent < 8; ++slvsCurrent)
    {
        LOG(INFO) << BOLDGREEN << "    Measuring slvs current " << +slvsCurrent << RESET;
        for(auto theBoard: *fDetectorContainer)
        {
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    for(auto theMPA: *theHybrid)
                    {
                        thePSinterface->WriteChipRegBits(theMPA, "ConfSLVS", slvsCurrent, "Mask", 0x07); // Enable shift register
                        // if(theMPA->getId() == 8) thePSinterface->WriteChipRegBits(theMPA, "ConfSLVS", slvsCurrent, "Mask", 0x07); // Enable shift register
                        // else thePSinterface->WriteChipRegBits(theMPA, "ConfSLVS", 0, "Mask", 0x07);
                    }
                }
            }
        }

        for(uint8_t phase = 0; phase < 15; ++phase)
        {
            LOG(INFO) << BOLDGREEN << "        Measuring phase " << +phase << RESET;
            DetectorDataContainer theMatchingEfficiencyContainer;
            ContainerFactory::copyAndInitChip<GenericDataArray<float, 6>>(*fDetectorContainer, theMatchingEfficiencyContainer);

            for(auto theBoard: *fDetectorContainer)
            {
                for(auto theOpticalGroup: *theBoard)
                {
                    auto possiblePatternList = getPossiblePatterns(static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10);
                    for(auto theHybrid: *theOpticalGroup)
                    {
                        auto theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                        // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Hybrid " << +theHybrid->getId() << std::endl;

                        std::vector<std::pair<std::string, uint16_t>> phaseRegisterVector;
                        for(uint8_t phyPortPair = 0; phyPortPair < 6; ++phyPortPair)
                        {
                            for(uint8_t channel = 0; channel < 4; ++channel)
                            {
                                std::stringstream phaseRegisterName;
                                phaseRegisterName << "scPhaseSelectB" << +channel << "i" << +(phyPortPair);
                                phaseRegisterVector.push_back({phaseRegisterName.str(), phase | phase << 4});
                            }
                        }
                        // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] phaseRegisterVector " << std::hex << phaseRegisterVector[0].second << std::dec << std::endl;
                        fCicInterface->WriteChipMultReg(theCic, phaseRegisterVector);

                        for(uint8_t phyPort = 0; phyPort < 12; ++phyPort)
                        {
                            // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] phyPort " << +phyPort << std::endl;
                            auto phyPortDataVector = readCICbypassOutput(theHybrid, theFWinterface, phyPort);
                            for(size_t line = 0; line < 4; ++line)
                            {
                                float matchingEfficiency = countMatchingBits(phyPortDataVector[line], possiblePatternList);
                                auto  chipIdAndLine      = fCicInterface->fromPhyPortAndChanneltoChipIdAndLine(theCic, phyPort, line);
                                // if(matchingEfficiency<1)
                                // {
                                // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Chip " << +chipIdAndLine.first << " line " << +chipIdAndLine.second << std::hex;
                                // for(auto word: phyPortDataVector[line]) std::cout << " " << word;
                                // std::cout << std::dec << std::endl;
                                // }
                                try // Handle disable chip
                                {
                                    theMatchingEfficiencyContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), chipIdAndLine.first + 8)
                                        ->getSummary<GenericDataArray<float, 6>>()[chipIdAndLine.second] = matchingEfficiency;
                                }
                                catch(const std::exception& e)
                                {
                                    continue;
                                }
                            }
                        }
                    }
                }
            }

#ifdef __USE_ROOT__
            fDQMHistogramOTMPAtoCICecv.fillPhaseScanMatchingEfficiency(theMatchingEfficiencyContainer, phase, slvsCurrent);
#else
            if(fDQMStreamerEnabled)
            {
                ContainerSerialization thePhaseScanMatchingEfficiencySerialization("OTMPAtoCICecvPhaseScanMatchingEfficiency");
                thePhaseScanMatchingEfficiencySerialization.streamByHybridContainer(fDQMStreamer, theMatchingEfficiencyContainer, phase, slvsCurrent);
            }
#endif
        }
    }

    fDetectorContainer->removeReadoutChipQueryFunction(theMPAqueryFunctionString);
}

std::vector<std::vector<uint32_t>> OTMPAtoCICecv::readCICbypassOutput(Hybrid* theHybrid, D19cFWInterface* theFWinterface, uint8_t phyPort)
{
    size_t                             cNlines = 4;
    std::vector<std::vector<uint32_t>> phyPortDataVector(cNlines);

    auto theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
    fCicInterface->SelectOutput(theCic, false);
    uint8_t registerValue = 0x10 + phyPort;
    fCicInterface->WriteChipReg(theCic, "MUX_CTRL", registerValue);
    fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
    fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);

    for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
    {
        auto lineOutputVector = theFWinterface->StubDebug(true, 4, false);
        for(size_t line = 0; line < cNlines; ++line) { phyPortDataVector[line].insert(phyPortDataVector[line].end(), lineOutputVector[line].begin(), lineOutputVector[line].end()); }
    }

    return phyPortDataVector;
}

float OTMPAtoCICecv::countMatchingBits(const std::vector<uint32_t>& incomingData, const std::vector<uint32_t>& possiblePatternList)
{
    float maximumMatchingEfficiency = -1;
    for(auto possiblePattern: possiblePatternList)
    {
        float currentEfficiency = 0;
        for(auto word: incomingData)
        {
            auto            possiblePatternXOR = word ^ possiblePattern;
            std::bitset<32> possiblePatternXORbitset(possiblePatternXOR);
            possiblePatternXORbitset.flip();
            currentEfficiency += possiblePatternXORbitset.count();
        }
        if(currentEfficiency > maximumMatchingEfficiency) maximumMatchingEfficiency = currentEfficiency;
    }

    return maximumMatchingEfficiency / (incomingData.size() * 32);
}

std::vector<uint32_t> OTMPAtoCICecv::getPossiblePatterns(bool is10Gmodule)
{
    uint64_t fullPattern = 0;
    if(is10Gmodule)
    {
        uint16_t doubleDigitShiftRegisterPattern = 0;
        for(uint8_t bit = 0; bit < 8; ++bit)
        {
            uint16_t singleBit = (fShiftRegisterPattern >> bit) & 0x1;
            doubleDigitShiftRegisterPattern |= ((singleBit << (2 * bit)) | singleBit << (2 * bit + 1));
        }
        for(uint8_t bitShift = 0; bitShift < 4; ++bitShift) { fullPattern |= (uint64_t(doubleDigitShiftRegisterPattern) << (16 * bitShift)); }
    }
    else
    {
        for(uint8_t bitShift = 0; bitShift < 8; ++bitShift) { fullPattern |= (uint64_t(fShiftRegisterPattern) << (8 * bitShift)); }
    }

    std::vector<uint32_t> possiblePatternList;
    for(uint8_t bitShift = 0; bitShift < 32; ++bitShift) { possiblePatternList.push_back((fullPattern >> bitShift) & 0xFFFFFFFF); }

    // remove duplicates
    sort(possiblePatternList.begin(), possiblePatternList.end());
    possiblePatternList.erase(unique(possiblePatternList.begin(), possiblePatternList.end()), possiblePatternList.end());

    // for(auto pattern: possiblePatternList) std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] pattern = " << std::hex << pattern << std::dec << std::endl;

    return possiblePatternList;
}
