#include "tools/OTverifyCICdataWord.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "HWDescription/BeBoard.h"
#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "Utils/Utilities.h"
#include "HWInterface/CbcInterface.h"

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

    fNumberOfIterations = 10;
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
    bool isKickoff = true;
    size_t cNlines = 5;
    fReadoutChipInterface->WriteChipReg(theChip, "HitOr", 1);
    fReadoutChipInterface->WriteChipReg(theChip, "PtCut", 14);
    fReadoutChipInterface->WriteChipReg(theChip, "ClusterCut", 4);
    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(theChip, "Sampled", true, true);

    std::vector<std::pair<std::string, uint16_t>> theRegisterVector;
    theRegisterVector.push_back({"Bend7", 0x0F}); // bendind = 0 will ouput 0
    theRegisterVector.push_back({"Bend8", 0x05}); // bendind = 2 will ouput 5
    theRegisterVector.push_back({"Bend9", 0x0A}); // bendind = 4 will ouput A
    theRegisterVector.push_back({"CoincWind&Offset12", 0x00}); // set stub window offset to 0
    theRegisterVector.push_back({"CoincWind&Offset34", 0x00}); // set stub window offset to 0
    fReadoutChipInterface->WriteChipMultReg(theChip, theRegisterVector);

    std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVector {{0x7F, 0}};
    // std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVector {{0x0A, 2}, {0xA0, 2}, {0xAA, 4}};
    
    fReadoutChipInterface->MaskAllChannels(theChip, true);
    static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theChip, stubSeedAndBendingVector);

    for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
    {
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] itearation = " << iteration << std::endl;
        auto lineOutputVector = theDebugInterface->StubDebug(true, cNlines, false);
        for(size_t lineIndex = 0; lineIndex < lineOutputVector.second.size(); ++lineIndex)
        {
            if(isKickoff && ((theChip->getHybridId() % 2) == 0) && (lineIndex == 4)) { continue; } // CIC_OUT_4_R will always fail for kick-off SEH, ignore here to keep allowing noise measurements
            LOG(ERROR) << BOLDRED << "Line " << lineIndex << " -> " << getPatternPrintout(lineOutputVector.second[lineIndex], numberOfBytesInSinglePacket) << RESET;
        }
    }

    return;
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
        for(size_t lineIndex = 0; lineIndex < lineOutputVector.second.size(); ++lineIndex)
        {
            LOG(ERROR) << BOLDRED << "Line " << lineIndex << " -> " << getPatternPrintout(lineOutputVector.second[lineIndex], numberOfBytesInSinglePacket) << RESET;
        }
    }

    return;
}
