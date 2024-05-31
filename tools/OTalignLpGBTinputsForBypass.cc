#include "tools/OTalignLpGBTinputsForBypass.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/Utilities.h"
#include "Utils/GenericDataArray.h"
#include "HWInterface/D19cFWInterface.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTalignLpGBTinputsForBypass::fCalibrationDescription = "Optimize LpGBT Rx phases to properly decode the inputs from the CICs when set in bypass mode";

OTalignLpGBTinputsForBypass::OTalignLpGBTinputsForBypass() : Tool() {}

OTalignLpGBTinputsForBypass::~OTalignLpGBTinputsForBypass() {}

void OTalignLpGBTinputsForBypass::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTalignLpGBTinputsForBypass.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTalignLpGBTinputsForBypass::ConfigureCalibration()
{

}

void OTalignLpGBTinputsForBypass::Running()
{
    LOG(INFO) << "Starting OTalignLpGBTinputsForBypass measurement.";
    Initialise();
    AlignLpGBTinputs();
    LOG(INFO) << "Done with OTalignLpGBTinputsForBypass.";
    Reset();
}

void OTalignLpGBTinputsForBypass::Stop(void)
{
    LOG(INFO) << "Stopping OTalignLpGBTinputsForBypass measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTalignLpGBTinputsForBypass.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTalignLpGBTinputsForBypass stopped.";
}

void OTalignLpGBTinputsForBypass::Pause()
{

}

void OTalignLpGBTinputsForBypass::Resume()
{

}

void OTalignLpGBTinputsForBypass::Reset()
{
    fRegisterHelper->restoreSnapshot();
}

void OTalignLpGBTinputsForBypass::AlignLpGBTinputs()
{

    LOG(INFO) << BOLDYELLOW << "OTalignLpGBTinputsForBypass::AlignLpGBTinputs ... start LpGBT phase scan with CIC in bypass moe" << RESET;

    auto theFWinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    auto firstModule = fDetectorContainer->getFirstObject()->getFirstObject();
    bool isPS = firstModule->getFrontEndType() == FrontEndType::OuterTrackerPS;
    uint8_t numberOfLines = 4;

    for(uint8_t phyPort=0; phyPort<12; ++phyPort)
    {
        LOG(INFO) << BOLDGREEN << "    Measuring phyPort " << +phyPort << RESET;

        if(isPS) prepareForLoGBTalignmentPS(phyPort);
        else prepareForLoGBTalignment2S(phyPort);
        DetectorDataContainer matchingEfficiencyContainer;
        ContainerFactory::copyAndInitHybrid<GenericDataArray<float, 4, 15>>(*fDetectorContainer, matchingEfficiencyContainer);

        DetectorDataContainer bestPhaseContainer;
        ContainerFactory::copyAndInitHybrid<GenericDataArray<uint8_t, 4>>(*fDetectorContainer, bestPhaseContainer);

        for(uint8_t lpgbtPhase=0; lpgbtPhase <15; ++lpgbtPhase)
        {
            for(auto theBoard: *fDetectorContainer)
            {
                for(auto theOpticalGroup: *theBoard)
                {
                    auto& thelpGBT = theOpticalGroup->flpGBT;

                    std::vector<std::string> listOfPhaseRegister = {"EPRX00ChnCntr",
                    "EPRX02ChnCntr",
                    "EPRX10ChnCntr",
                    "EPRX12ChnCntr",
                    "EPRX20ChnCntr",
                    "EPRX22ChnCntr",
                    "EPRX30ChnCntr",
                    "EPRX32ChnCntr",
                    "EPRX40ChnCntr",
                    "EPRX42ChnCntr",
                    "EPRX50ChnCntr",
                    "EPRX52ChnCntr",
                    "EPRX60ChnCntr",
                    "EPRX62ChnCntr"};

                    auto readRegister = flpGBTInterface->ReadChipMultReg(thelpGBT, listOfPhaseRegister);
                    for(auto& theRegister : readRegister)
                    {
                        theRegister.second = (theRegister.second & 0x0F) | (lpgbtPhase << 4);
                    }

                    flpGBTInterface->WriteChipMultReg(thelpGBT, readRegister);
                    auto possiblePatternList = getPossiblePatterns(fShiftRegisterPattern, static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10);
                    for(auto theHybrid: *theOpticalGroup)
                    {
                        std::vector<std::vector<uint32_t>> phyPortDataVector(numberOfLines);
                        fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
                        fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);

                        for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
                        {
                            auto lineOutputVector = theFWinterface->StubDebug(true, numberOfLines, false);
                            for(uint8_t line = 0; line < numberOfLines; ++line) { phyPortDataVector[line].insert(phyPortDataVector[line].end(), lineOutputVector[line].begin(), lineOutputVector[line].end()); }
                        }

                        for(uint8_t line = 0; line < numberOfLines; ++line)
                        {
                            matchingEfficiencyContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<GenericDataArray<float, 4, 15>>()[line][lpgbtPhase] = countMatchingBits(phyPortDataVector[line], possiblePatternList);
                        }
                    }
                }
            }
        }

        for(auto theBoard: bestPhaseContainer)
        {
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    auto phyPortEfficiencyScanList =  matchingEfficiencyContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<GenericDataArray<float, 4, 15>>();
                    for(uint8_t line = 0; line < numberOfLines; ++line)
                    {
                        theHybrid->getSummary<GenericDataArray<uint8_t, 4>>()[line] = getBestPhase(phyPortEfficiencyScanList[line], fDetectorContainer->getObject(theBoard->getId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId()), line);
                        // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] line = " << +line << " best phase " << theHybrid->getSummary<GenericDataArray<uint8_t, 4>>()[line] << std::endl;
                    }
                }
            }
        }

    #ifdef __USE_ROOT__
        fDQMHistogramOTalignLpGBTinputsForBypass.fillMatchingEfficiency(matchingEfficiencyContainer, phyPort);
        fDQMHistogramOTalignLpGBTinputsForBypass.fillBestPhase(bestPhaseContainer, phyPort);
    #else
        if(fDQMStreamer)
        {
            ContainerSerialization theMatchingEfficiencySerialization("OTalignLpGBTinputsForBypassMatchingEfficiency");
            theMatchingEfficiencySerialization.streamByHybridContainer(fDQMStreamer, matchingEfficiencyContainer, phyPort);

            ContainerSerialization theBestPhaseSerialization("OTalignLpGBTinputsForBypassBestPhase");
            theBestPhaseSerialization.streamByHybridContainer(fDQMStreamer, bestPhaseContainer, phyPort);
        }
    #endif
    }

}

void OTalignLpGBTinputsForBypass::prepareForLoGBTalignmentPS(uint8_t phyPort)
{

    auto        MPAqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string theMPAqueryFunctionString = "MPAqueryFunction";
    fDetectorContainer->addReadoutChipQueryFunction(MPAqueryFunction, theMPAqueryFunctionString);
    auto thePSinterface = static_cast<PSInterface*>(fReadoutChipInterface)->fTheMPA2Interface;
    setSameDac("LFSR_data", fShiftRegisterPattern);

    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                auto& theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                // enable alignment output
                fCicInterface->SelectOutput(theCic, false);
                fCicInterface->WriteChipReg(theCic, "MUX_CTRL", 0x10 | phyPort);
                for(auto theMPA: *theHybrid)
                {
                    thePSinterface->WriteChipRegBits(theMPA, "Control_1", 0x2, "Mask", 0x03); // Enable shift register
                    thePSinterface->WriteChipRegBits(theMPA, "ConfSLVS", 7, "Mask", 0x07); // set slvs current to the maximum
                }
                uint8_t phase = 9; // close enough to the best phase to make the procedure work
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
            }
        }
    }

    fDetectorContainer->removeReadoutChipQueryFunction(theMPAqueryFunctionString);
}

void OTalignLpGBTinputsForBypass::prepareForLoGBTalignment2S(uint8_t phyPort)
{
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Not implemented" << std::endl;
    abort();
}

uint8_t OTalignLpGBTinputsForBypass::getBestPhase(const GenericDataArray<float, 15>& thePhaseEfficiencyList, Hybrid* theHybrid, uint8_t line)
{
    bool firstMinimumFound = false;
    uint8_t locationOfFirstOne = 15;
    uint8_t locationOfLastOne = 15;
    float maximumEfficiency = -1;
    uint8_t maximumEfficiencyPhase = 15;

    for(uint8_t lpgbtPhase=0; lpgbtPhase <15; ++lpgbtPhase)
    {
        if(thePhaseEfficiencyList[lpgbtPhase] > maximumEfficiency)
        {
            maximumEfficiency = thePhaseEfficiencyList[lpgbtPhase];
            maximumEfficiencyPhase = lpgbtPhase;
        }
        if(!firstMinimumFound)
        {
            if(thePhaseEfficiencyList[lpgbtPhase] < 1)
            {
                firstMinimumFound = true;
            }
            else continue;
        }
        else
        {
            if(locationOfFirstOne == 15 && thePhaseEfficiencyList[lpgbtPhase] == 1) locationOfFirstOne = lpgbtPhase;
            if(locationOfFirstOne != 15 && locationOfLastOne == 15  &&  thePhaseEfficiencyList[lpgbtPhase] < 1)
            {
                locationOfLastOne = lpgbtPhase-1;
                break;
            }
        }
    }

    if(locationOfFirstOne == 15 || locationOfLastOne == 15)
    {
        LOG(ERROR) << BOLDRED << "OTalignLpGBTinputsForBypass::getBestPhase - ERROR: could not find working point for Board " << +theHybrid->getBeBoardId() << " OpticalGroup " << +theHybrid->getOpticalGroupId() << " Hybrid " << +theHybrid->getId() << " line " << +line << ", using phase with maximum efficiency" << RESET;
        return maximumEfficiencyPhase;
    }

    uint8_t plateauWidth = locationOfLastOne - locationOfFirstOne;

    if(plateauWidth%2 == 0) // even difference, odd number of plateau phases
    {
        return locationOfFirstOne + plateauWidth/2; // return the center of the plateau;
    }
    else // odd difference, even number of plateau phases
    {
        return locationOfFirstOne + (plateauWidth)/2 + (thePhaseEfficiencyList[locationOfFirstOne-1] > thePhaseEfficiencyList[locationOfLastOne+1] ? 0 : 1);
    }
}