#include "tools/PSCounterTest.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"

#include "HWDescription/BeBoardRegItem.h"
#include "HWDescription/Cbc.h"
#include "HWDescription/MPA2.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/D19cPSCounterFWInterface.h"
#include "HWInterface/FastCommandInterface.h"
#include "HWInterface/TriggerInterface.h"
#include "Utils/CBCChannelGroupHandler.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/EmptyContainer.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/Occupancy.h"
#include "Utils/SSAChannelGroupHandler.h"
#include "Utils/ThresholdAndNoise.h"
#include "Utils/Timer.h"
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

    // this->enableTestPulse(true); // For Testing Purposes

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

    for(auto cBoard: *fDetectorContainer)
    {
        if(!fWithSSA && !fWithMPA) continue;
        cBoard->setEventType(EventType::PSAS); // Sets up board to expect async input
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 1); 
                } // Tells chip to expect async
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
    Timer scanTimer;
    scanTimer.start();

    // uint16_t thrOffset = findValueInSettings<double>("PSCounterTest_thrOffset", 0);
    // RunFast(65 + thrOffset, 200 + thrOffset);

    for(uint16_t thrOffset = 0; thrOffset <= 30; thrOffset+=1)
    {
        RunFast(65 + thrOffset, 200 + thrOffset);
        // RunFast(thrOffset, thrOffset);
        // sleep(10);
    }

    scanTimer.stop();
    scanTimer.show("Scan time: ");

    LOG(INFO) << "Done with PSCounterTest.";
    Reset();
}

bool PSCounterTest::GetCounterData(BeBoard* theBoard, const std::string& theOutputFileName, int eventsPerPoint)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    uint32_t delayAfterFastReset = 100;
    uint32_t delayAfterTestPulse = 50;
    uint32_t delayBeforeNextPulse = 50;
    uint32_t afterClearCounters = 100;
    uint32_t afterCloseShutter = 50;
    uint32_t afterOpenShutter = 50;

    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren = " << fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren") << std::endl;
    
    std::vector<std::pair<std::string, uint32_t>> firstListOfBoardRegisters;
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.clear_counters", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.open_shutter", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cal_pulse", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.close_shutter", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.select_even", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.misc.initial_fast_reset_enable", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.sync_block.enable", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.ddr3_debug.stub_enable", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.ddr3_debug.scan_chain_enable", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 12});
    firstListOfBoardRegisters.push_back({"fc7_daq_ctrl.dio5_block.control.load_config", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", 1});

    fBeBoardInterface->WriteBoardMultReg(theBoard, firstListOfBoardRegisters);

    auto cDDR3Calibrated = (fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.ddr3_block.init_calib_done") == 1);
    while(!cDDR3Calibrated)
    {
        LOG(INFO) << BOLDYELLOW << "Waiting for DDR3 to finish initial calibration" << RESET;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        cDDR3Calibrated = (fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.ddr3_block.init_calib_done") == 1);
    }
    
    auto     cMultiplicity = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint32_t fNEvents      = eventsPerPoint * (cMultiplicity + 1);
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getTriggerInterface()->SetNTriggersToAccept(fNEvents);

    std::vector<std::pair<std::string, uint32_t>> secondListOfBoardRegisters;

    secondListOfBoardRegisters.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0});
    secondListOfBoardRegisters.push_back({"fc7_daq_ctrl.physical_interface_block.control.decoder_reset", 0x1});
    secondListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset", delayAfterFastReset});
    secondListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", delayAfterTestPulse});
    secondListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", delayBeforeNextPulse});
    secondListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_delay.after_clear_counters", afterClearCounters});
    secondListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_delay.after_close_shutter", afterCloseShutter});
    secondListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_delay.after_open_shutter", afterOpenShutter});

    fBeBoardInterface->WriteBoardMultReg(theBoard, secondListOfBoardRegisters);

    fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);

    uint32_t waitForDataCollection = (delayAfterFastReset + afterOpenShutter + afterClearCounters + afterCloseShutter + (delayAfterTestPulse + delayBeforeNextPulse) * eventsPerPoint) / 1000;
    std::this_thread::sleep_for(std::chrono::microseconds(waitForDataCollection));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    size_t startIterations    = 0;
    bool allCompleted;
    while(startIterations < 30)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        allCompleted = true;
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());

                auto cDecoderState  = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_decode.store_fsm_state");
                if(cDecoderState != 0x00)
                {
                    allCompleted = false;
                    break;
                }
            }
            if(!allCompleted) break;
        }
        if(allCompleted) break;
        startIterations++;
    }

    if(!allCompleted)
    {
        LOG(ERROR) << ERROR_FORMAT << "State machine did not run" << RESET;
        abort();
    }

    bool allStartPatternFound = true;

    for(auto theOpticalGroup: *theBoard)
    {
        for(auto theHybrid: *theOpticalGroup)
        {
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
            uint32_t startPatternNotFound = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_decode.start_pattern_not_found");
            if(startPatternNotFound == 1)
            {
                LOG(WARNING) << WARNING_FORMAT << "Start pattern not found for OpticalGroup " << +theOpticalGroup->getId() << " hybrid " << theHybrid->getId() % 2 << RESET;
                allStartPatternFound = false;
            }
        }
    }

    if(!allStartPatternFound)
    {
        LOG(WARNING) << WARNING_FORMAT << "Start pattern not found" << RESET;
        // sleep(10);
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    for(auto theOpticalGroup: *theBoard)
    {
        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren", 1 << theOpticalGroup->getId());
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] ddr3_wren = "  << fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren") << std::endl;
        auto cNFIFOentries = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.num_fifo_entry");
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] cNFIFOentries for opticalGroup " << theOpticalGroup->getId() << " = " << std::hex << cNFIFOentries << std::dec << std::endl;
        if(cNFIFOentries == 1) return false;
        // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren = " << fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren") << std::endl;
        // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en = 0x" << std::hex << fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en") << std::dec << std::endl;

        // sleep(5);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto   cDDR3state  = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state");
        size_t cIterations = 0;
        while((cDDR3state >> 3) != 1 && cIterations < 100) // while not in idle state
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cDDR3state     = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state");
            cIterations++;
        }
        if((cDDR3state >> 3) != 1)
        {
            LOG(ERROR) << ERROR_FORMAT << "Failed to read DDR3" << RESET;
            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state = 0x" << std::hex << cDDR3state << std::dec << std::endl;
            throw std::runtime_error("PSCounterTest::GetCounterData - DDR3 not ready!!");
        }
        
        auto cData = fBeBoardInterface->ReadBlockBoardReg(theBoard, "fc7_daq_ddr3", cNFIFOentries * 16, 0x20000 * theOpticalGroup->getId());

        std::string fullOutputFile = theOutputFileName + "_link" + std::to_string(theOpticalGroup->getId()) + ".txt";
        std::ofstream outfile(fullOutputFile);
        outfile << getPatternPrintout(cData, 1, false) << std::endl;
    }

    fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 0);
    fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren", 0);

    return true;
}


void PSCounterTest::RunFast(uint16_t stripThreshold, uint16_t pixelThreshold)
{
    std::cout << "Running" << std::endl;
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] stripThreshold = " << stripThreshold << std::endl;
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] pixelThreshold = " << pixelThreshold << std::endl;
    int eventsPerPoint = 254;
    // int eventsPerPoint = 0xff;

    for(auto cBoard: *fDetectorContainer)
    {
        cBoard->setEventType(EventType::PSAS);
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(cBoard))->InitializePSCounterFWInterface(cBoard);
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
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 70);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", pixelThreshold);
                        }
                        else
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 70);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", stripThreshold);
                            fReadoutChipInterface->WriteChipReg(cChip, "AsyncDelay", 0x3bf2);
                        }
                    }
                }
            }
        }

        // measureData(eventsPerPoint);

        

        std::cout << "Measuring Data" << std::endl;
        std::string outputFileName  = fDirectoryName + "/fastCounter_StripTh_" + std::to_string(stripThreshold) + "_PixelTh_" + std::to_string(pixelThreshold);

        int maxNumberOfIterations = 30;
        int iteration = 0;
        while(iteration < maxNumberOfIterations)
        {
            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Getting counters iteration " << iteration++ << std::endl;
            if(GetCounterData(cBoard, outputFileName, eventsPerPoint)) break;
        }
        if(iteration >= maxNumberOfIterations) abort();
        uint16_t mpa_lsb11 =
            fReadoutChipInterface->ReadChipReg(cBoard->getFirstObject()->getFirstObject()->getObject(8), MPA2::getPixelRegisterName("ReadCounter_LSB", 1, 1));
        uint16_t mpa_msb11 =
            fReadoutChipInterface->ReadChipReg(cBoard->getFirstObject()->getFirstObject()->getObject(8), MPA2::getPixelRegisterName("ReadCounter_MSB", 1, 1));

        std::cout << "MPA=" << std::dec << (mpa_lsb11 | (mpa_msb11 << 8)) << std::endl;

        uint16_t ssa_lsb1 = fReadoutChipInterface->ReadChipReg(cBoard->getFirstObject()->getFirstObject()->getObject(0), SSA2::getStripRegisterName("AC_ReadCounterLSB", 1));
        uint16_t ssa_msb1 = fReadoutChipInterface->ReadChipReg(cBoard->getFirstObject()->getFirstObject()->getObject(0), SSA2::getStripRegisterName("AC_ReadCounterMSB", 1));

        std::cout << "SSA=" << std::dec << (ssa_lsb1 | (ssa_msb1 << 8)) << std::endl;
    }
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
