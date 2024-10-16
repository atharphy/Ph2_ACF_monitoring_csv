#include "tools/PSCounterTest.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"

#include "HWDescription/BeBoardRegItem.h"
#include "HWDescription/Cbc.h"
#include "HWDescription/MPA2.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/D19cPSCounterFWInterface.h"
#include "Utils/CBCChannelGroupHandler.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/EmptyContainer.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/Occupancy.h"
#include "Utils/SSAChannelGroupHandler.h"
#include "Utils/ThresholdAndNoise.h"
#include "boost/format.hpp"
#include <math.h>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string PSCounterTest::fCalibrationDescription = "Insert brief calibration description here";

PSCounterTest::PSCounterTest() : Tool() {}

PSCounterTest::~PSCounterTest() {}

void PSCounterTest::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any
    std::cout << "Initializing" << std::endl;

    for(auto cBoard: *fDetectorContainer)
    {
        BeBoardRegMap cRegMap      = cBoard->getBeBoardRegMap();
        uint32_t      cTriggerFreq = cRegMap["fc7_daq_cnfg.fast_command_block.user_trigger_frequency"].fValue;

        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        cRegVec.clear();
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", cTriggerFreq});
        cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
        //        LOG(INFO) << BOLDYELLOW << "Noise measured on BeBoard#" << +cBoard->getId() << " with a trigger rate of " << cTriggerFreq << "kHz." << RESET;
    }
    fDisableStubLogic = false;

    this->enableTestPulse(true); // For Testing Purposes

    fWithSSA = false;
    fWithMPA = false;
    std::vector<FrontEndType> cAllFrontEndTypes;
    for(auto cBoard: *fDetectorContainer)
    {
        auto cFrontEndTypes = cBoard->connectedFrontEndTypes();
        fWithSSA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA2) != cFrontEndTypes.end();
        fWithMPA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA2) != cFrontEndTypes.end();
        for(auto cFrontEndType: cFrontEndTypes)
        {
            if(std::find(cAllFrontEndTypes.begin(), cAllFrontEndTypes.end(), cFrontEndType) == cAllFrontEndTypes.end()) cAllFrontEndTypes.push_back(cFrontEndType);
        }
    }

    for(auto cFrontEndType: cAllFrontEndTypes)
    {
        if(cFrontEndType == FrontEndType::SSA2)
        {
            SSAChannelGroupHandler theChannelGroupHandler;
            theChannelGroupHandler.setChannelGroupParameters(1, 1, NSSACHANNELS); // 16*2*8
            setChannelGroupHandler(theChannelGroupHandler, cFrontEndType);
        }
        else if(cFrontEndType == FrontEndType::MPA2)
        {
            MPAChannelGroupHandler theChannelGroupHandler;
            theChannelGroupHandler.setChannelGroupParameters(1, NMPAROWS, NSSACHANNELS); // 16*2*8
            setChannelGroupHandler(theChannelGroupHandler, cFrontEndType);
        }
    }

    // initializeRecycleBin();

    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        auto&                cBoardRegNap = fBoardRegContainer.getObject(cBoard->getId())->getSummary<BeBoardRegMap>();
        const BeBoardRegMap& cOrigRegMap  = static_cast<const BeBoard*>(cBoard)->getBeBoardRegMap();
        cBoardRegNap.insert(cOrigRegMap.begin(), cOrigRegMap.end());
    }

    // make sure register tracking is on
    for(auto board: *fDetectorContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    chip->setRegisterTracking(1);
                    chip->ClearModifiedRegisterMap();
                }
            }
        }
    }

    // for now.. force to use async mode here (does not track timing of hits), only used for testing
    bool cForcePSasync = true;
    // event types
    fEventTypes.clear();
    for(auto cBoard: *fDetectorContainer)
    {
        fEventTypes.push_back(cBoard->getEventType());
        if(!fWithSSA && !fWithMPA) continue;
        if(!cForcePSasync) continue;
        cBoard->setEventType(EventType::PSAS); // Sets up board to expect async input
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(cBoard))->InitializePSCounterFWInterface(cBoard);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid) { fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 1); } // Tells chip to expect async
            }
        }
    }

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramPSCounterTest.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void PSCounterTest::ConfigureCalibration() {}

void PSCounterTest::Running()
{
    LOG(INFO) << "Starting PSCounterTest measurement.";
    Initialise();
    RunFast();
    LOG(INFO) << "Done with PSCounterTest.";
    Reset();
}

void PSCounterTest::RunFast()
{
    std::cout << "Running" << std::endl;
    // fWithSSA = false;
    // fWithMPA = false;
    // std::vector<FrontEndType> cAllFrontEndTypes;
    // for(auto cBoard: *fDetectorContainer)
    // {
    // auto cFrontEndTypes = cBoard->connectedFrontEndTypes();
    // fWithSSA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA2) != cFrontEndTypes.end();
    // fWithMPA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA2) != cFrontEndTypes.end();
    // for(auto cFrontEndType: cFrontEndTypes)
    // {
    // if(std::find(cAllFrontEndTypes.begin(), cAllFrontEndTypes.end(), cFrontEndType) == cAllFrontEndTypes.end()) cAllFrontEndTypes.push_back(cFrontEndType);
    // }
    // }
    // int fNEventsPerBurst    = 0x5555;
    for(auto cBoard: *fDetectorContainer)
    {
        if(fWithSSA || fWithMPA)
        {
            // Allow for different SSA and MPA injection amplitudes
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto cType = cChip->getFrontEndType();

                        if(cType == FrontEndType::MPA2)
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 255);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", 225);
                        }
                        else
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 255);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", 111);
                        }
                    }
                }
            }
        }

        else
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "TestPulsePotNodeSel", 145);
    }
    std::cout << "Measuring Data" << std::endl;
    auto theBoard = fDetectorContainer->getFirstObject();





    // std::vector<uint32_t> fData;


    // LOG(INFO) << BOLDGREEN << "Fast counter mode readout" << RESET;
    // fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 1);
    // fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en.select_even", 1);
    // fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.ddr3_debug.stub_enable", 0);
    // fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 1);
    // fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.trigger_source", 12);
    // fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", 0);

    // LOG(INFO) << BOLDBLUE << "Reseting DDR3 " << RESET;
    // auto cDDR3Calibrated = (fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.ddr3_block.init_calib_done") == 1);
    // while(!cDDR3Calibrated)
    // {
    //     // LOG(DEBUG) << "Waiting for DDR3 to finish initial calibration";
    //     LOG(INFO) << BOLDYELLOW << "Waiting for DDR3 to finish initial calibration" << RESET;
    //     std::this_thread::sleep_for(std::chrono::microseconds(100));
    //     cDDR3Calibrated = (fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.ddr3_block.init_calib_done") == 1);
    // }
    // LOG(INFO) << BOLDBLUE << "DDR3 Initial Calibration Complete " << RESET;

    // std::vector<std::pair<std::string, uint32_t>> cTriggerConfig;
    // cTriggerConfig.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0x10});

    // // reset trigger
    // fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control.reset", 0x1);
    // std::this_thread::sleep_for(std::chrono::microseconds(100));
    // // configure
    // fBeBoardInterface->WriteBoardMultReg(theBoard, cTriggerConfig);
    // std::this_thread::sleep_for(std::chrono::microseconds(100));
    // // load new trigger configuration
    // fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);
    // std::this_thread::sleep_for(std::chrono::microseconds(100));

    // auto   cDecoderState  = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_decode.store_fsm_state");
    // auto   cCountersReady = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_decode.chip_counters_done");
    // size_t cIterations    = 0;
    // do {
    //     if(cDecoderState == 0x00 && cCountersReady == 0x00)
    //     {
    //         fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control", 0x90000);
    //         fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);
    //     }
    //     std::this_thread::sleep_for(std::chrono::microseconds(100));
    //     cDecoderState  = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_decode.store_fsm_state");
    //     cCountersReady = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_decode.chip_counters_done");

    //     cIterations++;
    // } while(cIterations < 1000 && !(cDecoderState == 0x00 && cCountersReady == 0x01));

    // auto   cDDR3state  = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state");
    // do {
    //     cDDR3state = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state");
    // } while(cDDR3state != 0x1); // while not in idle state

    // auto cNFIFOentries = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.num_fifo_entry");
    // LOG(INFO) << BOLDYELLOW << "DDR3 State Set to 0x" << std::hex << +cDDR3state << std::dec << RESET;
    // LOG(INFO) << BOLDYELLOW << "#FIFO Entries 0x" << std::hex << +cNFIFOentries << std::dec << RESET;

    // // Testing using ReadPSCountersFast
    // LOG(INFO) << BOLDRED << "Trying ReadPSCountersFast" << RESET;
    // // ReadPSCountersFast(1, 8, 0); // Raw Mode, Chip ID, Hybrid ID

    // LOG(INFO) << BOLDRED << "Printing DDR3 Content" << RESET;
    // auto cData = fBeBoardInterface->getFirmwareInterface(theBoard)->ReadBlockRegOffset("fc7_daq_ddr3", cNFIFOentries * 1024 / 32, 0);

    // std::cout << std::endl << std::endl;

    // std::cout << getPatternPrintout(cData, 1, false) << std::endl;

    // std::cout << std::endl << std::endl;






        // auto theBoard = fDetectorContainer->getFirstObject();

        // auto readFSMstatus = [this, theBoard]()
        // {
        //     std::cout << "FSM state = " << this->fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.fast_command_block.general.fsm_state") << std::endl;
        // };

        // auto writeWithComment = [this, theBoard](const std::string& registerName, uint32_t registerValue, const std::string& comment)
        // {
        //     std::cout << comment << std::endl;
        //     std::cout << "Writing register " << registerName << " value 0x" << std::hex << registerValue << std::dec << std::endl;
        //     this->fBeBoardInterface->WriteBoardReg(theBoard, registerName, registerValue);
        // };

        // // writeWithComment("fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 0x1, "");
        // // writeWithComment("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 0x1, "");
        // // writeWithComment("fc7_daq_cnfg.fast_command_block.ps_async_en.select_even", 0x1, "");
        // // writeWithComment("fc7_daq_cnfg.fast_command_block.trigger_source", 0xc, "");
        // // writeWithComment("fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0x10, "");
        // // writeWithComment("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1, "");




        // writeWithComment("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", 0, "Set raw mode");

        // writeWithComment("fc7_daq_cnfg.fast_command_block.trigger_source", 0xC, "Set trigger source");

        // writeWithComment("fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0x10, "Set trigger to accept");

        // writeWithComment("fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset", 0x8, "Set delay after fast reset");

        // writeWithComment("fc7_daq_cnfg.fast_command_block.ps_async_delay", 0x80808080, "Set all delays");

        // writeWithComment("fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", 0x80, "Set delay after test pulse");

        // writeWithComment("fc7_daq_cnfg.fast_command_block.ps_async_en", 0x1b, "Set ps async enable"); // maybe 7b

        // writeWithComment("fc7_daq_cnfg.fast_command_block.misc", 0x4, "Set ps async enable"); // maybe 7b

        // readFSMstatus();

        // writeWithComment("fc7_daq_ctrl.fast_command_block.control", 0x90000, "Send reset resync");

        // writeWithComment("fc7_daq_ctrl.fast_command_block.control.start_trigger", 1, "Set start trigger to 1");

        // readFSMstatus();

        // for(int sleepSec = 0; sleepSec < 5; ++sleepSec)
        // {
        //     std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] sleeping 1 sec..." << std::endl;
        //     usleep(1000000);
        // }

        // writeWithComment("fc7_daq_ctrl.fast_command_block.control.start_trigger", 0, "Set start trigger to 0");
        // writeWithComment("fc7_daq_ctrl.fast_command_block.control.stop_trigger", 1, "Set stop trigger to 1");

        // readFSMstatus();

        // writeWithComment("fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0, "Set stop trigger to 0");

        // readFSMstatus();

    int fEventsPerPoint     = 300;
    // this->measureData(fEventsPerPoint, fNEventsPerBurst);
    // auto theBoard = fDetectorContainer->getFirstObject();
    auto theFWinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
    auto dL1Interface = static_cast<D19cPSCounterFWInterface*>(theFWinterface->getL1ReadoutInterface());

    // fBeBoardInterface->getFirmwareInterface(theBoard)->WriteReg("fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);
    // fBeBoardInterface->getFirmwareInterface(theBoard)->WriteReg("fc7_daq_cnfg.fast_command_block.trigger_source",12);

    dL1Interface->setNEvents(fEventsPerPoint);
    dL1Interface->ReadEvents(theBoard);
    uint16_t mpa_lsb11 = fReadoutChipInterface->ReadChipReg(fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(8), MPA2::getPixelRegisterName("ReadCounter_LSB",1,1));
    uint16_t mpa_msb11 = fReadoutChipInterface->ReadChipReg(fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(8), MPA2::getPixelRegisterName("ReadCounter_MSB",1,1));

    std::cout<<"MPA="<< std::dec << (mpa_lsb11 | (mpa_msb11<<8)) <<std::endl;

}

void PSCounterTest::Stop(void)
{
    LOG(INFO) << "Stopping PSCounterTest measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramPSCounterTest.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "PSCounterTest stopped.";
}

void PSCounterTest::Pause() {}

void PSCounterTest::Resume() {}

void PSCounterTest::Reset() { fRegisterHelper->restoreSnapshot(); }
