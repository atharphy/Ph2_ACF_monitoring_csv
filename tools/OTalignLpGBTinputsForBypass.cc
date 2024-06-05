#include "tools/OTalignLpGBTinputsForBypass.h"
#include "HWInterface/CbcInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include "Utils/Utilities.h"
#include <bitset>

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
    fNumberOfIterations = findValueInSettings<double>("OTalignLpGBTinputsForBypass_NumberOfIterations", 1000);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTalignLpGBTinputsForBypass.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTalignLpGBTinputsForBypass::ConfigureCalibration() {}

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

void OTalignLpGBTinputsForBypass::Pause() {}

void OTalignLpGBTinputsForBypass::Resume() {}

void OTalignLpGBTinputsForBypass::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTalignLpGBTinputsForBypass::AlignLpGBTinputs()
{
    LOG(INFO) << BOLDYELLOW << "OTalignLpGBTinputsForBypass::AlignLpGBTinputs ... start LpGBT phase scan with CIC in bypass mode" << RESET;

    auto theFWinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    auto    firstModule   = fDetectorContainer->getFirstObject()->getFirstObject();
    bool    isPS          = firstModule->getFrontEndType() == FrontEndType::OuterTrackerPS;
    uint8_t numberOfLines = 4;

    if(isPS) prepareForLpGBTalignmentPS();

    for(uint8_t phyPort = 0; phyPort < 12; ++phyPort)
    {
        LOG(INFO) << BOLDGREEN << "    Measuring phyPort " << +phyPort << RESET;

        setCICBypass(phyPort);

        if(!isPS)
        {
            if(phyPort < 10)
                prepareForLpGBTalignment2Sstubs();
            else
                prepareForLpGBTalignment2SL1();
        }

        DetectorDataContainer matchingEfficiencyContainer;
        ContainerFactory::copyAndInitHybrid<GenericDataArray<float, 4, 15>>(*fDetectorContainer, matchingEfficiencyContainer);

        DetectorDataContainer bestPhaseContainer;
        ContainerFactory::copyAndInitHybrid<GenericDataArray<uint8_t, 4>>(*fDetectorContainer, bestPhaseContainer);

        for(uint8_t lpgbtPhase = 0; lpgbtPhase < 15; ++lpgbtPhase)
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
                    for(auto& theRegister: readRegister) { theRegister.second = (theRegister.second & 0x0F) | (lpgbtPhase << 4); }

                    flpGBTInterface->WriteChipMultReg(thelpGBT, readRegister);
                    for(auto theHybrid: *theOpticalGroup)
                    {
                        std::vector<std::vector<uint32_t>> phyPortDataVector(numberOfLines);
                        fBeBoardInterface->WriteBoardReg(
                            fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
                        fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);

                        for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
                        {
                            if(!isPS && phyPort >= 10)
                            {
                                fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);
                                usleep(10);
                                fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);
                            }

                            auto lineOutputVector = theFWinterface->StubDebug(true, numberOfLines, false);
                            for(uint8_t line = 0; line < numberOfLines; ++line)
                            {
                                phyPortDataVector[line].insert(phyPortDataVector[line].end(), lineOutputVector[line].begin(), lineOutputVector[line].end());
                            }
                        }

                        for(uint8_t line = 0; line < numberOfLines; ++line)
                        {
                            auto& matchingEfficiency =
                                matchingEfficiencyContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<GenericDataArray<float, 4, 15>>()[line][lpgbtPhase];
                            if(!isPS && phyPort >= 10) // L1 for 2S case
                            {
                                matchingEfficiency = getMatchingEfficiency2SL1(phyPortDataVector[line]);
                            }
                            else
                            {
                                uint8_t thePattern;
                                if(isPS)
                                    thePattern = fShiftRegisterPatternMPA;
                                else { thePattern = fStubPattern2S[(phyPort * 4 + line) % 5]; }
                                auto possiblePatternList = getPossiblePatterns(thePattern, static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10);
                                matchingEfficiency       = countMatchingBits(phyPortDataVector[line], possiblePatternList);
                            }
                        }
                    }
                }
                fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);
            }
        }

        for(auto theBoard: bestPhaseContainer)
        {
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    auto phyPortEfficiencyScanList =
                        matchingEfficiencyContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<GenericDataArray<float, 4, 15>>();
                    for(uint8_t line = 0; line < numberOfLines; ++line)
                    {
                        theHybrid->getSummary<GenericDataArray<uint8_t, 4>>()[line] =
                            getBestPhase(phyPortEfficiencyScanList[line], fDetectorContainer->getObject(theBoard->getId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId()), line);
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

void OTalignLpGBTinputsForBypass::prepareForLpGBTalignmentPS()
{
    auto        MPAqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string theMPAqueryFunctionString = "MPAqueryFunction";
    fDetectorContainer->addReadoutChipQueryFunction(MPAqueryFunction, theMPAqueryFunctionString);
    auto thePSinterface = static_cast<PSInterface*>(fReadoutChipInterface)->fTheMPA2Interface;
    setSameDac("LFSR_data", fShiftRegisterPatternMPA);

    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto theMPA: *theHybrid)
                {
                    thePSinterface->WriteChipRegBits(theMPA, "Control_1", 0x2, "Mask", 0x03); // Enable shift register
                    thePSinterface->WriteChipRegBits(theMPA, "ConfSLVS", 7, "Mask", 0x07);    // set slvs current to the maximum
                }
            }
        }
    }

    fDetectorContainer->removeReadoutChipQueryFunction(theMPAqueryFunctionString);
}

void OTalignLpGBTinputsForBypass::prepareForLpGBTalignment2Sstubs()
{
    auto theCbcInterface = static_cast<CbcInterface*>(fReadoutChipInterface);
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto theChip: *theHybrid)
                {
                    // switch on HitOr
                    fReadoutChipInterface->WriteChipReg(theChip, "HitOr", 1);
                    // set PtCut to maximum
                    fReadoutChipInterface->WriteChipReg(theChip, "PtCut", 14);
                    // disable cluster cut
                    fReadoutChipInterface->WriteChipReg(theChip, "ClusterCut", 4);
                    theCbcInterface->selectLogicMode(theChip, "Sampled", true, true);

                    std::vector<std::pair<std::string, uint16_t>> theRegisterVector;
                    theRegisterVector.push_back({"Bend7", 0x0A}); // forcing Bend7 (bending = 0) to ouput 0xA
                    // theRegisterVector.push_back({"Bend8", 0x0C});              // forcing Bend8 (bending = 1) to ouput 0xC
                    theRegisterVector.push_back({"CoincWind&Offset12", 0x00}); // set stub window offset to 0
                    theRegisterVector.push_back({"CoincWind&Offset34", 0x00}); // set stub window offset to 0
                    fReadoutChipInterface->WriteChipMultReg(theChip, theRegisterVector);

                    std::vector<std::pair<uint8_t, int>> stubSeedAndBend{{fStubPattern2S[0], 0}, {fStubPattern2S[1], 0}, {fStubPattern2S[2], 0}};
                    theCbcInterface->injectStubs(theChip, stubSeedAndBend);
                }
            }
        }
    }
}

void OTalignLpGBTinputsForBypass::prepareForLpGBTalignment2SL1()
{
    uint32_t triggerFrequency        = 1000; // do not change or it will not match padding 0s
    uint8_t  fakeHeaderChannelNumber = 24;
    fPattern2SL1.clear();
    fPattern2SL1.addToPattern(0x3, 0x3, 2); // CBC header
    fPattern2SL1.addToPattern(0x0, 0x0, 2); // error flags
    fPattern2SL1.addToPattern(0x0, 0x0, 9); // pipe address
    fPattern2SL1.addToPattern(0x0, 0x0, 9); // L1 counter

    for(uint8_t fakeHeaderChannel = 0; fakeHeaderChannel < fakeHeaderChannelNumber; ++fakeHeaderChannel)
    {
        fPattern2SL1.addToPattern(0x1, 0x1, 1); // fake channel header
    }

    for(uint8_t alternatedChannels = fakeHeaderChannelNumber; alternatedChannels < NCHANNELS; ++alternatedChannels)
    {
        fPattern2SL1.addToPattern((alternatedChannels + 1) % 2, 0x1, 1); // enable even numbers
    }

    uint32_t bitsBetweenConsecutiveTriggers = 40000 / triggerFrequency * 8;

    for(uint16_t paddingZeros = fPattern2SL1.getNumberOfPatternBits(); paddingZeros < bitsBetweenConsecutiveTriggers; ++paddingZeros)
    {
        fPattern2SL1.addToPattern(0, 0x1, 1); // padding zeros
    }

    auto theCbcInterface = static_cast<CbcInterface*>(fReadoutChipInterface);
    for(auto theBoard: *fDetectorContainer)
    {
        std::vector<std::pair<std::string, uint32_t>> registerVector;
        registerVector.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 3});
        registerVector.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
        registerVector.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", triggerFrequency});
        registerVector.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        fBeBoardInterface->WriteBoardMultReg(theBoard, registerVector);
        // fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);

        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto theChip: *theHybrid)
                {
                    fReadoutChipInterface->WriteChipReg(theChip, "ClusterCut", 4);
                    fReadoutChipInterface->WriteChipReg(theChip, "VCth", 1023);
                    theCbcInterface->selectLogicMode(static_cast<ReadoutChip*>(theChip), "Sampled", true, true);

                    auto cChannelMask = std::make_shared<ChannelGroup<1, NCHANNELS>>();
                    cChannelMask->disableAllChannels();
                    for(uint8_t cChannel = 0; cChannel < NCHANNELS; cChannel += 2) cChannelMask->enableChannel(0, cChannel); // generate a hit in every Nth channel
                    for(uint8_t cChannel = 0; cChannel < fakeHeaderChannelNumber; ++cChannel)
                        cChannelMask->enableChannel(0, cChannel); // generate a hit in the first 32 channels to create a sort of fake header
                    fReadoutChipInterface->maskChannelGroup(static_cast<ReadoutChip*>(theChip), cChannelMask);
                }
            }
        }
    }
}

void OTalignLpGBTinputsForBypass::setCICBypass(uint8_t phyPort)
{
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                auto& theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                fCicInterface->SelectOutput(theCic, false);
                fCicInterface->WriteChipReg(theCic, "MUX_CTRL", 0x10 | phyPort);
            }
        }
    }
}

uint8_t OTalignLpGBTinputsForBypass::getBestPhase(const GenericDataArray<float, 15>& thePhaseEfficiencyList, Hybrid* theHybrid, uint8_t line)
{
    bool    firstMinimumFound      = false;
    uint8_t locationOfFirstOne     = 15;
    uint8_t locationOfLastOne      = 15;
    float   maximumEfficiency      = -1;
    uint8_t maximumEfficiencyPhase = 15;
    float   minimumEfficiency      = 1;
    uint8_t minimumEfficiencyPhase = 15;

    for(uint8_t lpgbtPhase = 0; lpgbtPhase < 15; ++lpgbtPhase)
    {
        if(thePhaseEfficiencyList[lpgbtPhase] > maximumEfficiency)
        {
            maximumEfficiency      = thePhaseEfficiencyList[lpgbtPhase];
            maximumEfficiencyPhase = lpgbtPhase;
        }
        if(thePhaseEfficiencyList[lpgbtPhase] < minimumEfficiency)
        {
            minimumEfficiency      = thePhaseEfficiencyList[lpgbtPhase];
            minimumEfficiencyPhase = lpgbtPhase;
        }
        if(!firstMinimumFound)
        {
            if(thePhaseEfficiencyList[lpgbtPhase] < 1) { firstMinimumFound = true; }
            else
                continue;
        }
        else
        {
            if(locationOfFirstOne == 15 && thePhaseEfficiencyList[lpgbtPhase] == 1) locationOfFirstOne = lpgbtPhase;
            if(locationOfFirstOne != 15 && locationOfLastOne == 15 && thePhaseEfficiencyList[lpgbtPhase] < 1)
            {
                locationOfLastOne = lpgbtPhase - 1;
                break;
            }
        }
    }

    if(!firstMinimumFound)
    {
        uint8_t defaultPhase = 10;
        LOG(WARNING) << BOLDYELLOW << "OTalignLpGBTinputsForBypass::getBestPhase - WARNING: all phases work for Board " << +theHybrid->getBeBoardId() << " OpticalGroup "
                     << +theHybrid->getOpticalGroupId() << " Hybrid " << +theHybrid->getId() << " line " << +line << ", using phase default phase " << +defaultPhase << RESET;
        return defaultPhase;
    }
    if(firstMinimumFound && locationOfLastOne == 15) // just one minimum found
    {
        uint8_t phaseShift = 4;
        if(minimumEfficiencyPhase + phaseShift < 15)
            return minimumEfficiencyPhase + phaseShift;
        else
            return minimumEfficiencyPhase - phaseShift;
    }
    if(maximumEfficiency < 1)
    {
        LOG(WARNING) << BOLDYELLOW << "OTalignLpGBTinputsForBypass::getBestPhase - WARNING: no 100% efficiency found for Board " << +theHybrid->getBeBoardId() << " OpticalGroup "
                     << +theHybrid->getOpticalGroupId() << " Hybrid " << +theHybrid->getId() << " line " << +line << ", using phase with maximum efficiency" << RESET;
        return maximumEfficiencyPhase;
    }

    uint8_t plateauWidth = locationOfLastOne - locationOfFirstOne;

    if(plateauWidth % 2 == 0) // even difference, odd number of plateau phases
    {
        return locationOfFirstOne + plateauWidth / 2; // return the center of the plateau;
    }
    else // odd difference, even number of plateau phases
    {
        return locationOfFirstOne + (plateauWidth) / 2 + (thePhaseEfficiencyList[locationOfFirstOne - 1] > thePhaseEfficiencyList[locationOfLastOne + 1] ? 0 : 1);
    }
}

float OTalignLpGBTinputsForBypass::getMatchingEfficiency2SL1(std::vector<uint32_t> inputDataVector)
{
    float   totalEfficiency             = 0;
    uint8_t numberOfWordsPerAcquisition = 10; // 10 32-bit-words per acquisition;
    float   numberOfAcquisitions        = inputDataVector.size() / numberOfWordsPerAcquisition;
    for(size_t acquisitionNumber = 0; acquisitionNumber < numberOfAcquisitions; ++acquisitionNumber)
    {
        size_t                firstIndex = acquisitionNumber * numberOfWordsPerAcquisition;
        size_t                lastIndex  = firstIndex + numberOfWordsPerAcquisition;
        std::vector<uint32_t> singleAcquisitionInputDataVector(inputDataVector.begin() + firstIndex, inputDataVector.begin() + lastIndex);
        auto                  reorderedSingleAcquisitionInputDataVector = reorderPattern(singleAcquisitionInputDataVector, 1);
        auto                  maximumEfficiency                         = fPattern2SL1.getNumberOfMatchingBitsForAllBitshifts<320>(reorderedSingleAcquisitionInputDataVector);
        totalEfficiency += maximumEfficiency;
    }

    auto numberOfUnmaskedBits = fPattern2SL1.getNumberOfMaskedBits();
    return totalEfficiency / (numberOfAcquisitions * numberOfUnmaskedBits);
}
