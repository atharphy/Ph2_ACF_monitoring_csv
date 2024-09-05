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
    runOTCBCtoCICecv();
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

std::map<std::pair<uint8_t, uint8_t>, std::pair<uint8_t, uint8_t>> OTCBCtoCICecv::phyPortAndlineToCbcIdAndStub()
{
    uint8_t numberOfLines = 4;
    uint8_t numberOfPhyPorts = 12;

    std::map<std::pair<uint8_t, uint8_t>, std::pair<uint8_t, uint8_t>> phyPortAndLineToCbcIdAndStubMap;

    for(auto theBoard: *fDetectorContainer)
    for(auto theOpticalGroup: *theBoard)
    for(auto theHybrid: *theOpticalGroup)
    {
        auto& theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
        for(uint8_t phyPort = 0; phyPort < numberOfPhyPorts; ++phyPort)
        for(uint8_t line = 0; line < numberOfLines; ++line)
        {
            std::pair<uint8_t, uint8_t> phyPortAndLine(phyPort, line);
            auto chipIdAndLine = fCicInterface->fromPhyPortAndChanneltoChipIdAndLine(theCic, phyPort, line);
            chipIdAndLine.first += 1; // first contain cbcId from 1 to 8
            //second contain Stub value from 1 to 5
            //second value == 0 means L1
            //this would be helpful when booking histograms
            phyPortAndLineToCbcIdAndStubMap[phyPortAndLine] = chipIdAndLine;
        }
        return phyPortAndLineToCbcIdAndStubMap;
    }

    return phyPortAndLineToCbcIdAndStubMap; // this return is not used, just for the compiler to remove warning
}

void OTCBCtoCICecv::runOTCBCtoCICecv()
{
    uint8_t cbcStrengthStart = 0, cbcStrengthEnd  = 15 ;
    uint8_t cicPhaseStart    = 0, cicPhaseEnd     = 14 ;
    //uint8_t cbcStrengthStart = 0, cbcStrengthEnd  = 0 ;
    //uint8_t cicPhaseStart    = 0, cicPhaseEnd     = 1 ;

    uint8_t numberOfLines = 4;
    uint8_t numberOfPhyPorts = 12;
    uint8_t defaultBetaMult = 1;
    uint8_t pVerifyBit;
    uint8_t BetaMultAndSLVSbyte;

    //get default SLVS from BetaMult&SLVS stored in settings/CbcFiles/CBC3_default.txt
    for(auto theBoard: *fDetectorContainer)
    for(auto theOpticalGroup: *theBoard)
    for(auto theHybrid: *theOpticalGroup)
    {
        for(auto theCBC: *theHybrid)
        {
            uint8_t cReg      = fReadoutChipInterface->ReadChipReg(theCBC, "BetaMult&SLVS");
            defaultBetaMult = cReg & 0xF0;
            LOG(INFO) << "Deafult BetaMult&SLVS: 0x" << std::hex << +cReg << std::dec << " defaultBetaMult: " << std::hex << +defaultBetaMult << std::dec <<RESET;
            break;
        }
        break;
    }

    auto phyPortAndLineToCbcIdAndStubMap = phyPortAndlineToCbcIdAndStub();

        for(uint8_t cicPhase = cicPhaseStart; cicPhase <= cicPhaseEnd; cicPhase++)  // phase 0 to 14
        {

            if(cicPhase == 2 || cicPhase == 3) continue; //since Phase 2,3 doesn't work
            for(auto theBoard: *fDetectorContainer)
            for(auto theOpticalGroup: *theBoard)
            for(auto theHybrid: *theOpticalGroup)
            {
                auto& theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;

                //setting CIC phase
                std::vector<std::pair<std::string, uint16_t>> cicPhaseRegisterVector;
                for(uint8_t phyPortPair = 0; phyPortPair < 6; ++phyPortPair)
                {
                    for(uint8_t channel = 0; channel < 4; ++channel)
                    {
                        std::stringstream cicPhaseRegisterName;
                        cicPhaseRegisterName << "scPhaseSelectB" << +channel << "i" << +(phyPortPair);
                        cicPhaseRegisterVector.push_back({cicPhaseRegisterName.str(), cicPhase | cicPhase << 4});
                    }
                }
                fCicInterface->WriteChipMultReg(theCic, cicPhaseRegisterVector);
                LOG(INFO) << "Successfully set cicPhase value of 0x" << std::hex << +cicPhase << std::dec << " for CIC " << theHybrid->getId() << RESET;
            }

            for(uint8_t cbcStrength = cbcStrengthStart; cbcStrength <= cbcStrengthEnd; cbcStrength++)  //itr over BetaMult&SLVS from 0x?0 to 0x?F with sum of 0x10
            {
                for(auto theBoard: *fDetectorContainer)
                for(auto theOpticalGroup: *theBoard)
                for(auto theHybrid: *theOpticalGroup)
                for(auto theCBC: *theHybrid)
                {
                    BetaMultAndSLVSbyte = (defaultBetaMult | cbcStrength);
                    //setting CBC strength
                    pVerifyBit = fReadoutChipInterface->WriteChipReg(theCBC, "BetaMult&SLVS", BetaMultAndSLVSbyte , true);
                    if(!pVerifyBit)
                        LOG(ERROR) << "Error in setting BetaMult&SLVS value of 0x" << std::hex << +BetaMultAndSLVSbyte << std::dec << " for CIC " << theHybrid->getId() << ", CBC " << theCBC->getId() << "[ cbc strength set to " << +cbcStrength << " ]" <<  RESET;

                    if(pVerifyBit && theCBC->getId() == 7)
                        LOG(INFO) << "Successfully set BetaMult&SLVS value of 0x" << std::hex << +BetaMultAndSLVSbyte << std::dec << " for CIC " << theHybrid->getId() << ", CBC 0-" << theCBC->getId() << "[ cbc strength set to " << +cbcStrength << " ]" <<  RESET;
                }

                for(uint8_t phyPort = 0; phyPort < numberOfPhyPorts; ++phyPort)
                {
                    if(phyPort < 10)
                        prepareForLpGBTalignment2Sstubs();
                    else
                        prepareForLpGBTalignment2SL1();
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
                                auto& theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                                // choosing best LpGBT phase for bypassing CIC
                                std::cout << "Best LpGBT phase for hybrid " << theHybrid->getId() << ", phyPort " << +phyPort;
                                for(uint8_t line = 0; line < 4; ++line)
                                {
                                    auto bestPhase       = theCic->getLpGBTphaseForCICbypass(phyPort, line);
                                    std::cout << ", line " << +line << " = " << +bestPhase;
                                    auto groupAndChannel = theOpticalGroup->getGroupAndChannel(theHybrid->getId(), line + 1); // stub lines start from 1, line 0 is L1
                                    flpGBTInterface->ConfigureRxPhase(theOpticalGroup->flpGBT, groupAndChannel.first, groupAndChannel.second, bestPhase);
                                }
                                std::cout << std::endl;

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
                                        matchingEfficiency = getMatchingEfficiency2SL1(phyPortDataVector[line]); // PatternMatcher::countMatchingBits
                                    }
                                    else
                                    {
                                        uint8_t thePattern;
                                        thePattern = fStubPattern2S[(phyPort * 4 + line) % 5];
                                        auto possiblePatternList = getPossiblePatterns(thePattern, static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10);
                                        //std::cout << "length of phyPortDataVector[" << +line << "] = " << phyPortDataVector[line].size()*32 << " bits" << std::endl;
                                        std::cout << "phyPort " << +phyPort << ", line " << +line << " CBC" << +phyPortAndLineToCbcIdAndStubMap[{phyPort, line}].first << "_stub" << phyPortAndLineToCbcIdAndStubMap[{phyPort, line}].second - 1 << std::endl;
                                        matchingEfficiency       = countMatchingBits(phyPortDataVector[line], possiblePatternList); // Utilities::countMatchingBits
                                    }
                                    //LOG(INFO) << "kpal: phyPort: " << +phyPort << " channel: " << +line << " CBC" << +phyPortAndLineToCbcIdAndStubMap[{phyPort, line}].first << "_stub" << +phyPortAndLineToCbcIdAndStubMap[{phyPort, line}].second << " matchingEfficiency: " << matchingEfficiency << RESET;
                                }
                            }
                        }
                        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);
                    }

#ifdef __USE_ROOT__
                    //LOG(INFO) << "Using ROOT to save OTCBCtoCICecv matching efficiency." << RESET;
                    fDQMHistogramOTCBCtoCICecv.fillMatchingEfficiency(matchingEfficiencyContainer, phyPort, cicCurrent, cicPhase, cbcStrength, phyPortAndLineToCbcIdAndStubMap);
#else
                    if(fDQMStreamer)
                    {
                        //LOG(INFO) << "Using DQMStreamer to save OTCBCtoCICecv matching efficiency." << RESET;
                        ContainerSerialization theMatchingEfficiencySerialization("OTCBCtoCICecvMatchingEfficiency");
                        theMatchingEfficiencySerialization.streamByHybridContainer(fDQMStreamer, matchingEfficiencyContainer, phyPort);
                    }
#endif
                }
            }
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
