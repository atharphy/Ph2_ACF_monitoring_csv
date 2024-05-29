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

    fNumberOfIterations = findValueInSettings<double>("OTMPAtoCICecv_NumberOfIterations", 1000);

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

    for(uint8_t phase = 0; phase < 15; ++phase)
    {
        LOG(INFO) << BOLDGREEN << "    Measuring phase " << +phase << RESET;
        DetectorDataContainer theMatchingEfficiencyContainer;
        ContainerFactory::copyAndInitChip<GenericDataArray<float, 6>>(*fDetectorContainer, theMatchingEfficiencyContainer);

        for(auto theBoard: *fDetectorContainer)
        {
            for(auto theOpticalGroup: *theBoard)
            {
                uint8_t numberOfBytesInSinglePacket = (static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10) ? 2 : 1;
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
                    fCicInterface->WriteChipMultReg(theCic, phaseRegisterVector);

                    for(uint8_t phyPort = 0; phyPort < 12; ++phyPort)
                    {
                        // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] phyPort " << +phyPort << std::endl;
                        auto phyPortDataVector = readCICbypassOutput(theHybrid, theFWinterface, phyPort, numberOfBytesInSinglePacket);
                        for(size_t line = 0; line < 4; ++line)
                        {
                            float matchingEfficiency = countMatchingBits(phyPortDataVector[line]);
                            auto  chipIdAndLine      = fCicInterface->fromPhyPortAndChanneltoChipIdAndLine(theCic, phyPort, line);
                            // if(matchingEfficiency<1)
                            // {
                            //     std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Chip " << +chipIdAndLine.first << " line " << +chipIdAndLine.second << std::hex;
                            //     for(auto word: phyPortDataVector[line]) std::cout << " " << word;
                            //     std::cout << std::dec << std::endl;
                            // }
                            theMatchingEfficiencyContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), chipIdAndLine.first + 8)
                                ->getSummary<GenericDataArray<float, 6>>()[chipIdAndLine.second] = matchingEfficiency;
                        }
                    }
                }
            }
        }

#ifdef __USE_ROOT__
        fDQMHistogramOTMPAtoCICecv.fillPhaseScanMatchingEfficiency(theMatchingEfficiencyContainer, phase);
#else
        if(fDQMStreamerEnabled)
        {
            ContainerSerialization thePhaseScanMatchingEfficiencySerialization("OTMPAtoCICecvPhaseScanMatchingEfficiency");
            thePhaseScanMatchingEfficiencySerialization.streamByHybridContainer(fDQMStreamer, theMatchingEfficiencyContainer, phase);
        }
#endif
    }

    fDetectorContainer->removeReadoutChipQueryFunction(theMPAqueryFunctionString);
}

std::vector<std::vector<uint32_t>> OTMPAtoCICecv::readCICbypassOutput(Hybrid* theHybrid, D19cFWInterface* theFWinterface, uint8_t phyPort, uint8_t numberOfBytesInSinglePacket)
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

float OTMPAtoCICecv::countMatchingBits(std::vector<uint32_t> incomingData)
{
    uint32_t expectedPattern         = fShiftRegisterPattern | fShiftRegisterPattern << 8 | fShiftRegisterPattern << 16 | fShiftRegisterPattern << 24;
    uint32_t invertedExpectedPattern = ~expectedPattern;

    float matchingEfficiency = 0;

    for(auto word: incomingData)
    {
        std::bitset<32> bitsetWord(word);
        auto            expectedPatternXOR = word ^ expectedPattern;
        std::bitset<32> expectedPatternXORbitset(expectedPatternXOR);
        auto            invertedExpectedPatternXOR = word ^ invertedExpectedPattern;
        std::bitset<32> invertedExpectedPatternXORbitset(invertedExpectedPatternXOR);
        matchingEfficiency += std::max(expectedPatternXORbitset.count(), invertedExpectedPatternXORbitset.count());
    }

    matchingEfficiency /= 32 * incomingData.size();

    return matchingEfficiency;
}
