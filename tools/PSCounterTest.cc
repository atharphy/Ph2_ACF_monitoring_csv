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
    // for(auto theBoard: *fDetectorContainer)
    // {
    //     for(auto theOpticalGroup: *theBoard)
    //     {
    //         for(auto theHybrid: *theOpticalGroup)
    //         {
    //             auto& theCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
    //             fCicInterface->WriteChipReg(theCic, "EXT_BX0_DELAY", 0x2A);
    //             fCicInterface->WriteChipReg(theCic, "BX0_ALIGN_CONFIG", 0x80);        
    //         }
    //     }
    // }
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
                    // if(cChip->getFrontEndType() == FrontEndType::SSA2) continue;
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

    // uint32_t startDelay = 0x0;
    // uint32_t delaySteps = 0xFFFF;
    // uint32_t startDelay = 0x3bf2;
    // uint32_t delaySteps = 50;

    // uint16_t thrOffset = findValueInSettings<double>("PSCounterTest_thrOffset", 0);
    // RunFast(65 + thrOffset, 200 + thrOffset);
    // for(uint32_t delay = startDelay; delay <= startDelay + delaySteps; delay += 100)
    // {
    //     for(auto subDelay = 0; subDelay < 8; ++subDelay)
    //     {
    //         RunFast(65 + thrOffset, 200 + thrOffset, delay + subDelay);

    //     }
    // }

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

bool PSCounterTest::GetCounterData(Ph2_HwInterface::D19cFWInterface* theFWinterface, const std::string& theOutputFileName, int eventsPerPoint)
{
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren = " << theFWinterface->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren") << std::endl;
    
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 0);

    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.clear_counters", 1);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.open_shutter", 1);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cal_pulse", 1);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.close_shutter", 1);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 1);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.select_even", 1);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.misc.initial_fast_reset_enable", 0);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren", 0);
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren = " << theFWinterface->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren") << std::endl;

    theFWinterface->WriteReg("fc7_daq_cnfg.sync_block.enable", 0);

    theFWinterface->WriteReg("fc7_daq_cnfg.ddr3_debug.stub_enable", 0);
    theFWinterface->WriteReg("fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 1);
    theFWinterface->WriteReg("fc7_daq_cnfg.ddr3_debug.scan_chain_enable", 0);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.trigger_source", 12);
    theFWinterface->WriteReg("fc7_daq_ctrl.dio5_block.control.load_config", 0);
    usleep(100);
    theFWinterface->WriteReg("fc7_daq_ctrl.dio5_block.control.load_config", 1);
    usleep(100);

    theFWinterface->WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", 1);

    auto cDDR3Calibrated = (theFWinterface->ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
    while(!cDDR3Calibrated)
    {
        LOG(INFO) << BOLDYELLOW << "Waiting for DDR3 to finish initial calibration" << RESET;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        cDDR3Calibrated = (theFWinterface->ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
    }
    
    auto     cMultiplicity = theFWinterface->ReadReg("fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint32_t fNEvents      = eventsPerPoint * (cMultiplicity + 1);
    theFWinterface->getTriggerInterface()->SetNTriggersToAccept(fNEvents);
    theFWinterface->WriteReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0);

    theFWinterface->WriteReg("fc7_daq_ctrl.physical_interface_block.control.decoder_reset", 0x1);

    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset", 100);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", 50);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", 50);

    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_delay.after_clear_counters", 100);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_delay.after_close_shutter", 50);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_delay.after_open_shutter", 50);

    theFWinterface->WriteReg("fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);

    size_t startIterations    = 0;
    while(startIterations < 30)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto cDecoderState  = theFWinterface->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.store_fsm_state");
        auto cCountersReady = theFWinterface->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.chip_counters_done");
        if(cDecoderState == 0x00 && cCountersReady == 0x01) break;
        startIterations++;
    }

    theFWinterface->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", 1);
    uint32_t startPatternFound = theFWinterface->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.start_pattern_not_found");
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] async_counter_decode = " << std::hex << theFWinterface->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode") << std::dec << std::endl;
    
    if(startPatternFound == 1)
    {
        LOG(WARNING) << WARNING_FORMAT << "Start pattern not found" << RESET;
        // sleep(5);
        return false;
    }

    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << " waiting" << std::endl;
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren = " << theFWinterface->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren") << std::endl;
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en = 0x" << std::hex << theFWinterface->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en") << std::dec << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto cNFIFOentries = theFWinterface->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.num_fifo_entry");
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] cNFIFOentries = " << std::hex << cNFIFOentries << std::dec << std::endl;
    if(cNFIFOentries == 1) abort();

    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren", 1);
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren = " << theFWinterface->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren") << std::endl;
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en = 0x" << std::hex << theFWinterface->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en") << std::dec << std::endl;

    // sleep(5);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto   cDDR3state  = theFWinterface->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state");
    size_t cIterations = 0;
    while(cDDR3state != 0xA && cIterations < 100) // while not in idle state
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cDDR3state     = theFWinterface->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state");
        cIterations++;
    }
    if(cDDR3state != 0xA)
    {
        LOG(ERROR) << ERROR_FORMAT << "Failed to DDR3" << RESET;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state = 0x" << std::hex << cDDR3state << std::dec << std::endl;
        throw std::runtime_error("PSCounterTest::GetCounterData - DDR3 not ready!!");
    }
    
    auto cData = theFWinterface->ReadBlockRegOffset("fc7_daq_ddr3", cNFIFOentries * 1024 / 32, 0);

    std::ofstream outfile(theOutputFileName);
    outfile << getPatternPrintout(cData, 1, false) << std::endl;

    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 0);
    theFWinterface->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren", 0);

    return true;
}


void PSCounterTest::RunFast(uint16_t stripThreshold, uint16_t pixelThreshold, uint16_t delay)
{
    std::cout << "Running" << std::endl;
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] stripThreshold = " << stripThreshold << std::endl;
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] pixelThreshold = " << pixelThreshold << std::endl;
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] delay = " << std::hex << delay << std::dec << std::endl;
    
    auto theBoard = fDetectorContainer->getFirstObject();
    auto theFWinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
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
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 70);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", pixelThreshold);
                        }
                        else
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 70);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", stripThreshold);
                            fReadoutChipInterface->WriteChipReg(cChip, "AsyncDelay", delay);
                        }
                    }
                }
            }
        }
    }
    std::cout << "Measuring Data" << std::endl;

    int         eventsPerPoint = 254;
    std::string outputFileName  = fDirectoryName + "/fastCounter_StripTh_" + std::to_string(stripThreshold) + "_PixelTh_" + std::to_string(pixelThreshold) + "_delay_" + std::to_string(delay) + ".txt";

    bool local = true;
    if(local)
    {
        int maxNumberOfIterations = 30;
        int iteration = 0;
        while(iteration < maxNumberOfIterations)
        {
            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Getting counters iteration " << iteration++ << std::endl;
            if(GetCounterData(theFWinterface, outputFileName, eventsPerPoint)) break;
        }
        if(iteration >= maxNumberOfIterations) abort();
    }
    else
    {
        auto dL1Interface = static_cast<D19cPSCounterFWInterface*>(theFWinterface->getL1ReadoutInterface());
        dL1Interface->setOutputFile(outputFileName);
        dL1Interface->setNEvents(eventsPerPoint);
        dL1Interface->ReadEvents(theBoard);
    }

    uint16_t mpa_lsb11 =
        fReadoutChipInterface->ReadChipReg(fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(8), MPA2::getPixelRegisterName("ReadCounter_LSB", 1, 1));
    uint16_t mpa_msb11 =
        fReadoutChipInterface->ReadChipReg(fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(8), MPA2::getPixelRegisterName("ReadCounter_MSB", 1, 1));

    std::cout << "MPA=" << std::dec << (mpa_lsb11 | (mpa_msb11 << 8)) << std::endl;

    uint16_t ssa_lsb1 = fReadoutChipInterface->ReadChipReg(fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(0), SSA2::getStripRegisterName("AC_ReadCounterLSB", 1));
    uint16_t ssa_msb1 = fReadoutChipInterface->ReadChipReg(fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(0), SSA2::getStripRegisterName("AC_ReadCounterMSB", 1));

    std::cout << "SSA=" << std::dec << (ssa_lsb1 | (ssa_msb1 << 8)) << std::endl;

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
