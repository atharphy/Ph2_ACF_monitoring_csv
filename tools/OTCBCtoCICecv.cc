#include "tools/OTCBCtoCICecv.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include <algorithm>
#include <bitset>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTCBCtoCICecv::fCalibrationDescription = "Run electric chain validation test between CBC and CIC";

OTCBCtoCICecv::OTCBCtoCICecv() : OTCicBypassTest() {}

OTCBCtoCICecv::~OTCBCtoCICecv() {}

void OTCBCtoCICecv::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

    fNumberOfIterations    = findValueInSettings<double>("OTCBCtoCICecv_NumberOfIterations", 1000);
    fShiftRegisterPattern  = findValueInSettings<double>("OTCBCtoCICecv_ShiftRegisterPattern", 0xAA);
    fListOfCBCslvsCurrents = convertStringToFloatList(findValueInSettings<std::string>("OTCBCtoCICecv_ListOfCBCslvsCurrents", "1, 4, 7"));

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTCBCtoCICecv.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTCBCtoCICecv::ConfigureCalibration() {}

void OTCBCtoCICecv::Running()
{
    LOG(INFO) << "Starting OTCBCtoCICecv measurement.";
    Initialise();
    //setCBCshiftRegister();   //from MPA to CIC ecv
    //runElectricChainValidation();  //from MPA to CIC ecv
    //phaseAlignment();
    //itrOverCICStrength();
    //RunCICbypassTest();
    //printCICStrengthAndPhase();
    //itrOverCBCStrength();
    LOG(INFO) << BOLDRED << "========== Starting OTCBC2CICalignment test ==========" << RESET;
    runOTCBCtoCICecv();
    LOG(INFO) << BOLDRED << "========== End OTCBC2CICalignment test ==========" << RESET;
    LOG(INFO) << "Done with OTCBCtoCICecv.";
    Reset();
}

void OTCBCtoCICecv::Stop(void)
{
    LOG(INFO) << "Stopping OTCBCtoCICecv measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTCBCtoCICecv.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTCBCtoCICecv stopped.";
}

void OTCBCtoCICecv::Pause() {}

void OTCBCtoCICecv::Resume() {}

void OTCBCtoCICecv::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTCBCtoCICecv::setCBCshiftRegister()
{

    auto        CBCqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::CBC3); };
    std::string theCBCqueryFunctionString = "CBCqueryFunction";
    fDetectorContainer->addReadoutChipQueryFunction(CBCqueryFunction, theCBCqueryFunctionString);

    setSameDac("LFSR_data", fShiftRegisterPattern);

    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto theCBC: *theHybrid)
                {
                    std::cout << "CBC" << theCBC->getId() << std::endl;
                    //thePSinterface->WriteChipRegBits(theCBC, "Control_1", 0x2, "Mask", 0x03); // Enable shift register
                }
            }
        }
    }
    fDetectorContainer->removeReadoutChipQueryFunction(theCBCqueryFunctionString);
}

void OTCBCtoCICecv::runElectricChainValidation()
{
    auto        CBCqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::CBC3); };
    std::string theCBCqueryFunctionString = "CBCqueryFunction";
    fDetectorContainer->addReadoutChipQueryFunction(CBCqueryFunction, theCBCqueryFunctionString);

    LOG(INFO) << BOLDYELLOW << "OTCBCtoCICecv::runElectricChainValidation ... start electric chain validation test" << RESET;

    //auto thePSinterface = static_cast<PSInterface*>(fReadoutChipInterface)->fTheCBC3Interface;

    for(auto slvsCurrent: fListOfCBCslvsCurrents)
    {
        LOG(INFO) << BOLDGREEN << "    Measuring slvs current " << +slvsCurrent << RESET;
        for(auto theBoard: *fDetectorContainer)
        {
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    for(auto theCBC: *theHybrid)
                    {
                        std::cout << "CBC" << theCBC->getId() << std::endl;
                        //thePSinterface->WriteChipRegBits(theCBC, "ConfSLVS", slvsCurrent, "Mask", 0x07); // set slvs current
                    }
                }
            }
        }

        for(uint8_t phase = 0; phase < 15; ++phase)
        {
            if(phase == 2 || phase == 3) continue;
            LOG(INFO) << BOLDGREEN << "        Measuring phase " << +phase << RESET;
            DetectorDataContainer theMatchingEfficiencyContainer;
            ContainerFactory::copyAndInitChip<GenericDataArray<float, 6>>(*fDetectorContainer, theMatchingEfficiencyContainer);

            for(auto theBoard: *fDetectorContainer)
            {
                auto theFWinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
                for(auto theOpticalGroup: *theBoard)
                {
                    auto possiblePatternList = getPossiblePatterns(fShiftRegisterPattern, static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10);
                    for(auto theHybrid: *theOpticalGroup)
                    {
                        auto theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;

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
                            for(uint8_t line = 0; line < 4; ++line)
                            {
                                auto bestPhase       = theCic->getLpGBTphaseForCICbypass(phyPort, line);
                                auto groupAndChannel = theOpticalGroup->getGroupAndChannel(theHybrid->getId(), line + 1); // stub lines start from 1, line 0 is L1
                                flpGBTInterface->ConfigureRxPhase(theOpticalGroup->flpGBT, groupAndChannel.first, groupAndChannel.second, bestPhase);
                            }
                            auto phyPortDataVector = readCICbypassOutput(theHybrid, theFWinterface, phyPort);
                            for(size_t line = 0; line < 4; ++line)
                            {
                                float matchingEfficiency = countMatchingBits(phyPortDataVector[line], possiblePatternList);
                                auto  chipIdAndLine      = fCicInterface->fromPhyPortAndChanneltoChipIdAndLine(theCic, phyPort, line);
                                // if(matchingEfficiency < 1 && matchingEfficiency>0.95)
                                // {
                                //     size_t patternSize = 10;
                                //     size_t numberOfPatterns = phyPortDataVector[line].size() / patternSize;
                                //     for(size_t patternCounter = 0; patternCounter<numberOfPatterns; ++patternCounter)
                                //     {
                                //         auto first = phyPortDataVector[line].begin() + patternCounter * patternSize;
                                //         auto last  = first + patternSize;
                                //         std::vector<uint32_t> patternVector(first, last);
                                //         bool matching = true;
                                //         for(auto word : patternVector)
                                //         {
                                //             if(word != 0x33333333 && word != 0x66666666 && word != 0x99999999 && word != 0xCCCCCCCC)
                                //             {
                                //                 matching = false;
                                //                 break;
                                //             }
                                //         }
                                //         if(!matching) std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Iteration = " << patternCounter << " data = " << getPatternPrintout(patternVector, 2)
                                //         << std::endl;
                                //     }
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
            fDQMHistogramOTCBCtoCICecv.fillPhaseScanMatchingEfficiency(theMatchingEfficiencyContainer, phase, slvsCurrent);
#else
            if(fDQMStreamerEnabled)
            {
                ContainerSerialization thePhaseScanMatchingEfficiencySerialization("OTCBCtoCICecvPhaseScanMatchingEfficiency");
                thePhaseScanMatchingEfficiencySerialization.streamByHybridContainer(fDQMStreamer, theMatchingEfficiencyContainer, phase, slvsCurrent);
            }
#endif
        }
    }

    fDetectorContainer->removeReadoutChipQueryFunction(theCBCqueryFunctionString);
}

std::vector<std::vector<uint32_t>> OTCBCtoCICecv::readCICbypassOutput(Hybrid* theHybrid, D19cFWInterface* theFWinterface, uint8_t phyPort)
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
        auto lineOutputVector = theFWinterface->StubDebug(true, cNlines, false);
        for(size_t line = 0; line < cNlines; ++line) { phyPortDataVector[line].insert(phyPortDataVector[line].end(), lineOutputVector[line].begin(), lineOutputVector[line].end()); }
    }

    return phyPortDataVector;
}

void OTCBCtoCICecv::printCICStrengthAndPhase()
{
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                auto slvsCurrent = fCicInterface->ReadChipReg(cCic, "SLVS_PADS_CONFIG");
                LOG(INFO) << "kpal: for FEH " << theHybrid->getId() << ", slvsCurrent = 0x" << std::hex << slvsCurrent << std::dec << RESET;
                //LOG(INFO) << "kpal: Set Phase Once: " << RESET;
                //fCicInterface->SetStaticPhaseAlignment(cCic);
                //LOG(INFO) << "kpal: Set Phase:  " << RESET;
                //fCicInterface->SetAutomaticPhaseAlignment(cCic);
                //LOG(INFO) << "kpal: Iterating over CIC phase" << RESET;
                //fCicInterface->SetAllPhaseValue(cCic);
            }
        }
    }
}

void OTCBCtoCICecv::itrOverCICStrength()
{
    uint8_t cicSLVSStrengthStart    = 1, cicSLVSStrengthEnd     = 5 ;
    for(auto theBoard: *fDetectorContainer)
        for(auto theOpticalGroup: *theBoard)
            for(uint8_t cicStrength = cicSLVSStrengthStart; cicStrength <= cicSLVSStrengthEnd; ++cicStrength)
                for(auto theHybrid: *theOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                    LOG(INFO) << "Setting CIC strength to FEH" << theHybrid->getId() << RESET;
                    fCicInterface->ConfigureDriveStrength(cCic, cicStrength);
                }
}

void OTCBCtoCICecv::itrOverCBCStrength()
{
    uint16_t cReg;
    bool pVerifyBit;
    uint8_t writeBit;
    for (uint16_t i = 0x01; i < 0x1F; i += 0x10)
    {
        for(auto theBoard: *fDetectorContainer)
            for(auto theOpticalGroup: *theBoard)
                for(auto theHybrid: *theOpticalGroup)
                {
                    LOG(INFO) << "kpal: for FEH " << theHybrid->getId() << RESET;
                    for(auto theCBC: *theHybrid)
                    {
                        // itr over BetaMult&SLVS from 0x01 to 0xF1 with sum of 0x10
                        writeBit = uint8_t(i);
                        pVerifyBit= fReadoutChipInterface->WriteChipReg(theCBC, "BetaMult&SLVS", writeBit, true);
                        cReg      = fReadoutChipInterface->ReadChipReg(theCBC, "BetaMult&SLVS");
                        LOG(INFO) << "kpal: CBC number: " << theCBC->getId() << " BetaMult&SLVS: 0x" << std::hex << cReg << std::dec << " pVerifyBit: " << pVerifyBit << RESET;
                    }
                }
    //calcEfficiencyBypassingCIC();
    }
}

void OTCBCtoCICecv::runOTCBCtoCICecv()
{
    uint8_t cicSLVSCurrentStart    = 1, cicSLVSCurrentEnd     = 5 ;
    uint8_t cbcStrengthStart        = 0, cbcStrengthEnd         = 15 ;
    uint8_t numberOfLines = 4;
    uint8_t numberOfPhyPorts = 12;
    uint8_t defaultSLVS = 1;
    uint8_t pVerifyBit;
    uint8_t BetaMultAndSLVSbyte;

    //get default SLVS from BetaMult&SLVS stored in settings/CbcFiles/CBC3_default.txt
    for(auto theBoard: *fDetectorContainer)
    for(auto theOpticalGroup: *theBoard)
    for(auto theHybrid: *theOpticalGroup)
    {
        LOG(INFO) << "kpal: for FEH " << theHybrid->getId() << RESET;
        for(auto theCBC: *theHybrid)
        {
            // itr over BetaMult&SLVS from 0x01 to 0xF1 with sum of 0x10
            uint8_t cReg      = fReadoutChipInterface->ReadChipReg(theCBC, "BetaMult&SLVS");
            defaultSLVS = cReg & 0x0F;
            LOG(INFO) << "kpal: CBC number: " << theCBC->getId() << " BetaMult&SLVS: 0x" << std::hex << +cReg << std::dec << " defaultSLVS: " << std::hex << +defaultSLVS << std::dec <<RESET;
            break;
        }
        break;
    }

    prepareForLpGBTalignment2Sstubs();
    prepareForLpGBTalignment2SL1();

    for(uint8_t cicSlvsCurrent = cicSLVSCurrentStart; cicSlvsCurrent <= cicSLVSCurrentEnd; cicSlvsCurrent++)
    for(uint8_t cbcStrength = cbcStrengthStart; cbcStrength <= cbcStrengthEnd; cbcStrength++)  //itr over BetaMult&SLVS from 0x0? to 0xF? with sum of 0x10
    for(uint8_t phyPort = 0; phyPort < numberOfPhyPorts; ++phyPort)
    {
        setCICBypass(phyPort);
        DetectorDataContainer matchingEfficiencyContainer;
        ContainerFactory::copyAndInitHybrid<GenericDataArray<float, 4>>(*fDetectorContainer, matchingEfficiencyContainer);
        for(auto theBoard: *fDetectorContainer)
        {
            auto theFWinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    //setting CIC strength
                    auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                    fCicInterface->ConfigureDriveStrength(cCic, cicSlvsCurrent);

                    //setting CBC strength
                    for(auto theCBC: *theHybrid)
                    {
                        BetaMultAndSLVSbyte = (cbcStrength << 4 | defaultSLVS);
                        pVerifyBit = fReadoutChipInterface->WriteChipReg(theCBC, "BetaMult&SLVS", BetaMultAndSLVSbyte , true);
                        if(!pVerifyBit)
                            LOG(ERROR) << "Error in setting cbcStrength " << +cbcStrength << ", BetaMult&SLVS value of 0x" << std::hex << +BetaMultAndSLVSbyte << std::dec << " for CIC,CBC: " << theHybrid->getId() << "," << theCBC->getId() << "\t Current phyPort: " << +phyPort << RESET;

                        if(pVerifyBit && theCBC->getId() == 7)
                            LOG(INFO) << "Successfully set cbcStrength " << +cbcStrength << ", BetaMult&SLVS value of 0x" << std::hex << +BetaMultAndSLVSbyte << std::dec << " for CIC,CBC: " << theHybrid->getId() << "," << theCBC->getId() << "\t Current phyPort: " << +phyPort << RESET;
                    }

                    std::vector<std::vector<uint32_t>> phyPortDataVector(numberOfLines);
                    fBeBoardInterface->WriteBoardReg(
                        fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
                    fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);

                    for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
                    {
                        if(phyPort >= 10)
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
                            matchingEfficiencyContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<GenericDataArray<float, 5>>()[line];
                        if(phyPort >= 10) // L1 for 2S case
                        {
                            matchingEfficiency = getMatchingEfficiency2SL1(phyPortDataVector[line]);
                            LOG(INFO) << "kpal: phyPort: " << +phyPort << " line: " << +line << " matchingEfficiency: " << matchingEfficiency << RESET;
                        }
                        else
                        {
                            uint8_t thePattern;
                            thePattern = fStubPattern2S[(phyPort * 4 + line) % 5];
                            auto possiblePatternList = getPossiblePatterns(thePattern, static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10);
                            matchingEfficiency       = countMatchingBits(phyPortDataVector[line], possiblePatternList);
                            LOG(INFO) << "kpal: phyPort: " << +phyPort << " line: " << +line << " matchingEfficiency: " << matchingEfficiency << RESET;
                        }
                    }
                }
            }
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);
        }

#ifdef __USE_ROOT__
        LOG(INFO) << "Using ROOT to save OTCBCtoCICecv matching efficiency." << RESET;
        fDQMHistogramOTCBCtoCICecv.fillMatchingEfficiency(matchingEfficiencyContainer, phyPort, cbcStrength, cicSlvsCurrent);
#else
        if(fDQMStreamer)
        {
            LOG(INFO) << "Using DQMStreamer to save OTCBCtoCICecv matching efficiency." << RESET;
            ContainerSerialization theMatchingEfficiencySerialization("OTCBCtoCICecvMatchingEfficiency");
            theMatchingEfficiencySerialization.streamByHybridContainer(fDQMStreamer, matchingEfficiencyContainer, phyPort);
        }
#endif
    }
}

void OTCBCtoCICecv::setCICBypass(uint8_t phyPort)
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

void OTCBCtoCICecv::prepareForLpGBTalignment2Sstubs()
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

void OTCBCtoCICecv::prepareForLpGBTalignment2SL1()
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

float OTCBCtoCICecv::getMatchingEfficiency2SL1(std::vector<uint32_t> inputDataVector)
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
