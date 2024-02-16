#include "tools/OTverifyCICdataWord.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "HWDescription/BeBoard.h"
#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "Utils/Utilities.h"
#include "HWInterface/CbcInterface.h"
#include <bitset>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTverifyCICdataWord::fCalibrationDescription = "Insert brief calibration description here";

OTverifyCICdataWord::OTverifyCICdataWord() : Tool() {}

OTverifyCICdataWord::~OTverifyCICdataWord() {}

void OTverifyCICdataWord::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

    fNumberOfIterations = 1;
#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTverifyCICdataWord.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTverifyCICdataWord::ConfigureCalibration()
{

}

void OTverifyCICdataWord::Running()
{
    LOG(INFO) << "Starting OTverifyCICdataWord measurement.";
    Initialise();
    runIntegrityTest();
    LOG(INFO) << "Done with OTverifyCICdataWord.";
    Reset();
}

void OTverifyCICdataWord::Stop(void)
{
    LOG(INFO) << "Stopping OTverifyCICdataWord measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTverifyCICdataWord.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTverifyCICdataWord stopped.";
}

void OTverifyCICdataWord::Pause()
{

}


void OTverifyCICdataWord::Resume()
{

}

void OTverifyCICdataWord::Reset()
{
    fRegisterHelper->restoreSnapshot();
}

void OTverifyCICdataWord::runIntegrityTest()
{
    LOG(INFO) << BOLDYELLOW << "OTverifyCICdataWord::runIntegrityTest ... start integrity test" << RESET;
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    D19cDebugFWInterface* theDebugInterface = cInterface->getDebugInterface();

    for(auto theBoard: *fDetectorContainer)
    {
        runStubIntegrityTest(theBoard, theDebugInterface);
        runL1IntegrityTest(theBoard, theDebugInterface);
    }
}


void OTverifyCICdataWord::runL1IntegrityTest(BeBoard* theBoard, D19cDebugFWInterface* theDebugInterface)
{
    // LOG(INFO) << BOLDMAGENTA << "Running runL1IntegrityTest" << RESET;
    // // Set board trigger configuration for L1 alignment
    // std::vector<std::pair<std::string, uint32_t>> cVecReg;
    // cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    // cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0});
    // cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", 100});
    // cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
    // cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    // cVecReg.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    // cVecReg.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x1});
    // fBeBoardInterface->WriteBoardMultReg(theBoard, cVecReg);

    // for(auto theOpticalGroup: *theBoard)
    // {
    //     uint8_t numberOfBytesInSinglePacket = getNumberOfBytesInSinglePacket(theOpticalGroup);
    //     for(auto theHybrid: *theOpticalGroup)
    //     {
    //         auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
    //         if(cCic == nullptr) continue;

    //         auto& theHybridPatternMatchingEfficiency =
    //             fPatternMatchingEfficiencyContainer.getObject(theBoard->getId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<float>>();

    //         // select lines for slvs debug
    //         fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
    //         fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
    //         for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
    //         {
    //             auto lineOutputVector = theDebugInterface->L1ADebug(1, false);
    //             LOG(ERROR) << BOLDRED << "Line " << lineIndex << " -> " << getPatternPrintout(lineOutputVector.second[lineIndex], numberOfBytesInSinglePacket) << RESET;
    //     }
    // }
}


void OTverifyCICdataWord::runStubIntegrityTest(BeBoard* theBoard, D19cDebugFWInterface* theDebugInterface)
{
    LOG(INFO) << BOLDMAGENTA << "Running runStubIntegrityTest" << RESET;

    for(auto theOpticalGroup: *theBoard)
    {
        bool isA2Smodule = theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S;
        uint8_t numberOfBytesInSinglePacket = (static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10) ? 2 : 1;;
        for(auto theHybrid: *theOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
            fCicInterface->SelectOutput(cCic, false);
            fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
            fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
            auto theFeConfigRegisterValue = fCicInterface->ReadChipReg(cCic, "FE_CONFIG");
            theFeConfigRegisterValue |= 0x04; //Force bending to be sent out in the stub stream
            fCicInterface->WriteChipReg(cCic, "FE_CONFIG", theFeConfigRegisterValue);
            for(auto theChip: *theHybrid)
            {
                if(theChip->getFrontEndType() == FrontEndType::SSA2) continue;
                // mask all other chips
                // for(auto theOtherChip: *theHybrid)
                // {
                //     if(theOtherChip->getId()%8 == theChip->getId()) continue;
                //     fReadoutChipInterface->MaskAllChannels(theOtherChip, true);
                // }
                auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);

                fCicInterface->EnableFEs(cCic, {uint8_t(theChip->getId()%8)}, true);
                if(isA2Smodule) injectStubs2S(theChip, theDebugInterface, numberOfBytesInSinglePacket);
                else            injectStubsPS(theChip, theDebugInterface, numberOfBytesInSinglePacket);
                break;
                // fReadoutChipInterface->MaskAllChannels(theChip, true);
            }
        }
    }
}


void OTverifyCICdataWord::injectStubs2S(ReadoutChip* theChip, D19cDebugFWInterface* theDebugInterface, uint8_t numberOfBytesInSinglePacket)
{
    fReadoutChipInterface->WriteChipReg(theChip, "HitOr", 1);
    fReadoutChipInterface->WriteChipReg(theChip, "PtCut", 14);
    fReadoutChipInterface->WriteChipReg(theChip, "ClusterCut", 4);
    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(theChip, "Sampled", true, true);

    std::vector<std::pair<std::string, uint16_t>> theRegisterVector;
    theRegisterVector.push_back({"Bend7", fBendingAndCode[0]}); // bendind = 0 will ouput 5
    theRegisterVector.push_back({"Bend8", fBendingAndCode[2]}); // bendind = 2 will ouput A
    theRegisterVector.push_back({"Bend9", fBendingAndCode[4]}); // bendind = 4 will ouput F
    theRegisterVector.push_back({"CoincWind&Offset12", 0x00}); // set stub window offset to 0
    theRegisterVector.push_back({"CoincWind&Offset34", 0x00}); // set stub window offset to 0
    fReadoutChipInterface->WriteChipMultReg(theChip, theRegisterVector);

    // inject stubs on CBC to CIC stub lines 0 (first stub address) lines 1 (second stub address), line 3 (first and second stub bend)
    std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVectorFirstPattern {{0x0A, 0}, {0xA0, 2}, {0xAA, 4}};
    // std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVectorFirstPattern {{0x7F, 4}};
    std::vector<uint8_t> lineToMatchListFirstPattern {0, 1, 3};
    injectAndMatch2SstubPatterns(theChip, theDebugInterface, numberOfBytesInSinglePacket, stubSeedAndBendingVectorFirstPattern, lineToMatchListFirstPattern);

    // // inject stubs on CBC to CIC stub lines 2 (third stub address), line 4 (thirt stub bend)
    // std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVectorSecondPattern {{0x0A, 4}, {0xA0, 0}, {0xAA, 2}};
    // std::vector<uint8_t> lineToMatchListSecondPattern {2, 4};
    // injectAndMatch2SstubPatterns(theChip, theDebugInterface, numberOfBytesInSinglePacket, stubSeedAndBendingVectorSecondPattern, lineToMatchListSecondPattern);
    
    return;
}

void OTverifyCICdataWord::injectAndMatch2SstubPatterns(ReadoutChip* theChip, D19cDebugFWInterface* theDebugInterface, uint8_t numberOfBytesInSinglePacket, std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVector, std::vector<uint8_t> lineToMatchList)
{
    size_t cNlines = 5;
    bool isKickoff = true;
    fReadoutChipInterface->MaskAllChannels(theChip, true);
    static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theChip, stubSeedAndBendingVector);

    auto lineDataAndMaskWordVector = reproduce2SstubPattern(theChip->getId(), stubSeedAndBendingVector);
    for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
    {
        // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] itearation = " << iteration << std::endl;
        std::bitset<800> theFullBiset{0};
        auto lineOutputVector = theDebugInterface->StubDebug(true, cNlines, false);
        for(size_t line=0; line<cNlines; ++line)
        {
            auto reorderedLineOuput = reorderBytes<160>(lineOutputVector[line], numberOfBytesInSinglePacket);
            for(size_t i=0; i<160; ++i)
            {
                theFullBiset[i * cNlines + (cNlines - 1 - line)] = reorderedLineOuput[i];
            }
            std::cout << "Line         " << +line  << " -> 0x" << std::hex << reorderedLineOuput.to_string() << std::dec << std::endl;
        }
        std::cout << "Full data "  << std::hex << theFullBiset.to_string() << std::dec << std::endl;

        // for(size_t line=0; line<cNlines; ++line)
        // {
        //     std::cout << "Line         " << +line  << " -> 0x" << std::hex << getPatternPrintout(lineOutputVector[line], numberOfBytesInSinglePacket) << std::dec << std::endl;
        // }
        for(const auto line: lineToMatchList)
        {
            if(isKickoff && ((theChip->getHybridId() % 2) == 0) && (line == 4)) { continue; } // CIC_OUT_4_R will always fail for kick-off SEH, ignore here to keep allowing noise measurements
            auto reorderedLineOuput = reorderBytes<160>(lineOutputVector[line], numberOfBytesInSinglePacket);
            if(isPatternFound(lineDataAndMaskWordVector[line], reorderedLineOuput))
            {
               LOG(INFO) << BOLDGREEN <<  "Matched pattern on stub line " << line + 1 << " in iteration number " << +iteration << RESET;
            }
            else
            {
                LOG(ERROR) << BOLDRED << "Error on stub line " << line + 1 << " occurred in iteration number " << +iteration << RESET;
            }
        }
    }
}

std::vector<std::pair<std::bitset<160>, std::bitset<160>>> OTverifyCICdataWord::reproduce2SstubPattern(uint8_t chipId, std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVector)
{
    size_t  numberOfLines = 5;
    uint8_t maximumStubNumber = 16;
    uint8_t chipType = 0;

    // Order stub by bending
    std::map<uint8_t, uint8_t> orderedStubBendingCodeAndSeedVector;
    for(const auto& stubSeedAndBending: stubSeedAndBendingVector)
    {
        orderedStubBendingCodeAndSeedVector[fBendingAndCode.at(stubSeedAndBending.second)] = stubSeedAndBending.first;
    }
    if(orderedStubBendingCodeAndSeedVector.size() != stubSeedAndBendingVector.size())
    {
        std::cerr << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] orderedStubBendingCodeAndSeedVector and stubSeedAndBendingVector sizes to not match!" << std::endl;
        abort();
    }

    uint8_t numberOfStubs = stubSeedAndBendingVector.size() * 8; // times 8 because the packet contains stubs from 8 BXs
    uint16_t status = 0x0;
    if(numberOfStubs>maximumStubNumber)
    {
        status = 0x1;
        numberOfStubs = maximumStubNumber;
    }

    std::vector<std::tuple<std::bitset<320>, size_t, bool>> valueAndShiftVector;
    valueAndShiftVector.push_back(std::make_tuple<std::bitset<320>, size_t, bool>(chipType, 1, true)); // chip type
    valueAndShiftVector.push_back(std::make_tuple<std::bitset<320>, size_t, bool>(status, 9, true)); // status
    valueAndShiftVector.push_back(std::make_tuple<std::bitset<320>, size_t, bool>(0x000, 12, false)); // Bx Id
    valueAndShiftVector.push_back(std::make_tuple<std::bitset<320>, size_t, bool>(numberOfStubs, 6, true)); // number of stubs

    auto addStubPackage = [&valueAndShiftVector, chipId](uint8_t bxShift, uint8_t stubAddress, uint8_t stubBend)
    {
        valueAndShiftVector.push_back(std::make_tuple<std::bitset<320>, size_t, bool>(bxShift, 3, false)); // Bx shift
        valueAndShiftVector.push_back(std::make_tuple<std::bitset<320>, size_t, bool>(chipId, 3, true)); // Chip Address
        valueAndShiftVector.push_back(std::make_tuple<std::bitset<320>, size_t, bool>(stubAddress, 8, true)); // Stub seed
        valueAndShiftVector.push_back(std::make_tuple<std::bitset<320>, size_t, bool>(stubBend, 4, true)); // Stub bend
    };


    size_t totalNumberOfStubs = 0;
    for(const auto& stubBendingCodeAndSeed: orderedStubBendingCodeAndSeedVector)
    {
        for(size_t bx=0; bx<8; ++bx)
        {
            addStubPackage(bx, stubBendingCodeAndSeed.second, stubBendingCodeAndSeed.first);
            ++totalNumberOfStubs;
            if(totalNumberOfStubs >= maximumStubNumber) break;
        }
        if(totalNumberOfStubs >= maximumStubNumber) break;
    }

    std::bitset<320> dataWord{0};
    std::bitset<320> maskWord{0};
    maskWord = maskWord.flip();

    size_t bitShift = 320;
    for(const auto& valueAndShift: valueAndShiftVector)
    {
        bitShift -= std::get<1>(valueAndShift);
        dataWord |= (std::get<0>(valueAndShift) << bitShift);
        if(!std::get<2>(valueAndShift))
        {
            std::bitset<320> tmpMask {~(~0u << std::get<1>(valueAndShift))};
            tmpMask = tmpMask << bitShift;
            tmpMask = tmpMask.flip();
            maskWord &= tmpMask;
        }
    }

    std::cout << dataWord.to_string() << std::endl;
    std::cout << maskWord.to_string() << std::endl;

    std::vector<std::pair<std::bitset<160>, std::bitset<160>>> lineDataAndMaskWordVector(numberOfLines, {0,0});

    for(size_t package=0; package<320/numberOfLines; ++package)
    {
        for(size_t line=0; line<numberOfLines; ++line)
        {
            lineDataAndMaskWordVector[line].first[320/numberOfLines - 1 - package] = dataWord[320 - 1 -(package*numberOfLines + line)];
            lineDataAndMaskWordVector[line].second[320/numberOfLines - 1 - package] = maskWord[320 - 1 -(package*numberOfLines + line)];
        }
    }

    return lineDataAndMaskWordVector;

}



void OTverifyCICdataWord::injectStubsPS(ReadoutChip* theChip, D19cDebugFWInterface* theDebugInterface, uint8_t numberOfBytesInSinglePacket)
{
    size_t cNlines = 6;
    // fReadoutChipInterface->WriteChipReg(theChip, "HitOr", 1);
    // fReadoutChipInterface->WriteChipReg(theChip, "PtCut", 14);
    // fReadoutChipInterface->WriteChipReg(theChip, "ClusterCut", 4);
    // static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(theChip, "Sampled", true, true);

    // std::vector<std::pair<std::string, uint16_t>> theRegisterVector;
    // theRegisterVector.push_back({"Bend7", 0x0F}); // bendind = 0 will ouput 0
    // theRegisterVector.push_back({"Bend8", 0x05}); // bendind = 2 will ouput 5
    // theRegisterVector.push_back({"Bend9", 0x0A}); // bendind = 4 will ouput A
    // theRegisterVector.push_back({"CoincWind&Offset12", 0x00}); // set stub window offset to 0
    // theRegisterVector.push_back({"CoincWind&Offset34", 0x00}); // set stub window offset to 0
    // fReadoutChipInterface->WriteChipMultReg(theChip, theRegisterVector);

    // std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVector {{0x7F, 0}};
    // std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVector {{0x0A, 2}, {0xA0, 2}, {0xAA, 4}};
    
    fReadoutChipInterface->MaskAllChannels(theChip, true);
    // static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theChip, stubSeedAndBendingVector);

    for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
    {
        // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] itearation = " << iteration << std::endl;
        auto lineOutputVector = theDebugInterface->StubDebug(true, cNlines, false);
        for(size_t lineIndex = 0; lineIndex < lineOutputVector.size(); ++lineIndex)
        {
            LOG(ERROR) << BOLDRED << "Line " << lineIndex << " -> " << getPatternPrintout(lineOutputVector[lineIndex], numberOfBytesInSinglePacket) << RESET;
        }
    }

    return;
}

bool OTverifyCICdataWord::isPatternFound(std::pair<std::bitset<160>, std::bitset<160>> theExpectedPatternAndMask, std::bitset<160> theLinePattern)
{
    size_t numberBytesCharactersToMatch = 0 ;
    size_t maximumBitShift = 160/8 - numberBytesCharactersToMatch;

    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << theExpectedPatternAndMask.first.to_string() << std::endl;
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << theExpectedPatternAndMask.second.to_string() << std::endl;
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << theLinePattern.to_string() << std::endl;

    for(size_t byteShift = 0; byteShift < maximumBitShift; ++byteShift)
    {
        size_t bitShift = byteShift * 8;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << ((theLinePattern >> bitShift) & theExpectedPatternAndMask.second).to_string() << std::endl;
        if(((theLinePattern >> bitShift) & theExpectedPatternAndMask.second) == (theExpectedPatternAndMask.first & theExpectedPatternAndMask.second) )
        {
            return true;
        }
    }
    return false;
}