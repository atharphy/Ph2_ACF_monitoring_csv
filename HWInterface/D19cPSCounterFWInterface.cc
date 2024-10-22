#include "HWInterface/D19cPSCounterFWInterface.h"
#include "HWDescription/BeBoard.h"
#include "HWDescription/Chip.h"
#include "HWDescription/ChipRegItem.h"
#include "HWDescription/Hybrid.h"
#include "HWDescription/OpticalGroup.h"
#include "HWInterface/FEConfigurationInterface.h"
#include "HWInterface/FastCommandInterface.h"
#include "HWInterface/RegManager.h"
#include "HWInterface/TriggerInterface.h"
#include "Utils/ConsoleColor.h"
#include "Utils/Utilities.h"
#include "Utils/easylogging++.h"
#include <fstream>
#include <numeric>
#include <thread>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cPSCounterFWInterface::D19cPSCounterFWInterface(RegManager* theRegManager) : L1ReadoutInterface(theRegManager)
{
    // handshake should always be off for this readout mode
    fHandshake = 0;
}

D19cPSCounterFWInterface::~D19cPSCounterFWInterface() {}

bool D19cPSCounterFWInterface::ResetReadout()
{
    LOG(INFO) << BOLDRED << "Nothing to reset for PS counter interface.." << RESET;
    return true;
}

void D19cPSCounterFWInterface::PS_Open_shutter()
{
    for(uint16_t numit = 0; numit < fFCDupe; numit++) fFastCommandInterface->SendGlobalL1A();
}

void D19cPSCounterFWInterface::PS_Close_shutter()
{
    for(uint16_t numit = 0; numit < fFCDupe; numit++) fFastCommandInterface->SendGlobalCounterReset();
}
void D19cPSCounterFWInterface::PS_Clear_counters()
{
    for(uint16_t numit = 0; numit < fFCDupe; numit++) fFastCommandInterface->SendGlobalCounterResetL1A();
}
void D19cPSCounterFWInterface::PS_Inject()
{
    for(uint16_t numit = 0; numit < fFCDupe; numit++) fFastCommandInterface->SendGlobalCalPulse();
}
void D19cPSCounterFWInterface::PS_Start_counters_read()
{
    for(uint16_t numit = 0; numit < fFCDupe; numit++) fFastCommandInterface->SendGlobalCounterResetResync();
}
void D19cPSCounterFWInterface::PS_Send_pulses(uint32_t pNtriggers, bool manual)
{
    if(manual)
    {
        for(uint16_t numit = 0; numit < pNtriggers; numit++) this->PS_Inject();
    }
    else
        fTriggerInterface->RunTriggerFSM();
}

// compose id for counter data
uint32_t D19cPSCounterFWInterface::Compose_Id(const BeBoard* pBoard, const OpticalGroup* pGroup, const Hybrid* pHybrid, const Chip* pChip)
{
    uint8_t  cType = (pChip->getFrontEndType() == FrontEndType::MPA2) ? 1 : 0;
    uint32_t cId   = (pBoard->getId() << (3 + 6 + 4 + 1 + 4)) | (pGroup->getId() << (3 + 6 + 4 + 1)) | (pHybrid->getId() << (3 + 6 + 1)) | (pChip->getId() << (3 + 1)) | cType;
    return cId;
}

// method to read counter from register
void D19cPSCounterFWInterface::SlowRead(const BeBoard* pBoard)
{
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                std::stringstream cChipType;
                cChip->printChipType(cChipType);

                // LOG(DEBUG) << BOLDBLUE << "Directly reading back counters from Chip#" << +cChip->getId() << RESET;
                std::vector<ChipRegItem> cRegItems;
                auto                     cId       = Compose_Id(pBoard, cOpticalGroup, cHybrid, cChip);
                auto                     cIterator = fPSCounterData.find(cId);
                if(cIterator != fPSCounterData.end()) cIterator->second.clear();
                for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                {
                    uint32_t cBaseRegisterLSB, cBaseRegisterMSB;
                    cBaseRegisterLSB = 0;
                    cBaseRegisterMSB = 0;
                    if(cChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        int cRowNumber   = 1 + cChnl / 120;
                        int cPixelNumber = 1 + cChnl % 120;

                        cBaseRegisterLSB = ((cRowNumber << 11) | (9 << 7) | cPixelNumber);
                        cBaseRegisterMSB = ((cRowNumber << 11) | (10 << 7) | cPixelNumber);
                        if(cChip->getFrontEndType() == FrontEndType::MPA2)
                        {
                            cBaseRegisterLSB -= 0x280;
                            cBaseRegisterMSB -= 0x280;
                        }
                    }
                    if(cChip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        cBaseRegisterLSB = 0x0580 + cChnl;
                        cBaseRegisterMSB = 0x0680 + cChnl;
                    }

                    // MSB
                    ChipRegItem cReg_Counters_MSB;
                    cReg_Counters_MSB.fPage    = 0x00;
                    cReg_Counters_MSB.fAddress = cBaseRegisterMSB;
                    cReg_Counters_MSB.fValue   = 0x00;
                    cRegItems.push_back(cReg_Counters_MSB);
                    // LSB
                    ChipRegItem cReg_Counters_LSB;
                    cReg_Counters_LSB.fPage    = 0x00;
                    cReg_Counters_LSB.fAddress = cBaseRegisterLSB;
                    cReg_Counters_LSB.fValue   = 0x00;
                    cRegItems.push_back(cReg_Counters_LSB);
                }
                if(!fFEConfigurationInterface->MultiRead(cChip, cRegItems)) continue;
                // LOG(DEBUG) << BOLDYELLOW << "Read-back " << cRegItems.size() << " counters from " << cChipType.str() << "#" << +cChip->getId() << "#" << +cId << RESET;
                // fill counter information
                for(auto cIter = cRegItems.begin(); cIter < cRegItems.end(); cIter += 2)
                {
                    auto cMSB = (*cIter).fValue;
                    auto cLSB = (*(cIter + 1)).fValue;
                    // if(fPSCounterData[cId].size() < 10)
                    //     LOG(DEBUG) << BOLDYELLOW << "\t.. Counter#" << fPSCounterData[cId].size() << " MSBs " << +cMSB << " LSBs " << +cLSB << " : " << ((cMSB << 8) | cLSB) << RESET;

                    fPSCounterData[cId].push_back((cMSB << 8) | cLSB);
                }
            } // chip loop
        } // hybrid loop
    } // board loop
    // PS_Clear_counters();
}
bool D19cPSCounterFWInterface::ReadPSCountersFast(uint8_t pRawMode, size_t pChipId, size_t pHybridId)
{
    bool                                          cSuccess = false;
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    uint32_t                                      cIteration    = 0;
    auto                                          cDecoderState = this->fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.state");
    // wait until fifo is ready to start readout of counters
    do {
        // LOG(DEBUG) << BOLDMAGENTA << "\t\t..D19cFWInterface::WaitForData DECODER State: " << +cDecoderState << "Running.. .Iteration#" << +cIteration << RESET;
        cDecoderState = this->fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.state");
        cIteration++;
    } while(cDecoderState != 0); // idle state is 0
    // LOG(DEBUG) << BOLDMAGENTA << "Decoder in IDLE state after " << +cIteration << " iterations." << RESET;

    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    size_t      cNbits    = 200e3 * 8 * 6;
    size_t      cNWords   = cNbits / 32; // number of 32-bit words to read from DDR3
    auto        cData     = fTheRegManager->ReadBlockRegOffset("fc7_daq_ddr3", cNWords, 0);
    std::string cDataWord = "";
    size_t      cIndx     = 0;
    auto        cIter     = cData.begin();
    uint16_t    cBxId     = 0;
    if(pRawMode == 0)
    {
        do {
            uint8_t cHeader = ((*cIter) & (0xF << 28)) >> 28;
            if(cHeader == 0x5)
            {
                cDataWord = "";
                cDataWord += std::bitset<32>(*cIter).to_string();
                cIter++;
                cIndx++;
                cDataWord += std::bitset<32>(*cIter).to_string();
                LOG(INFO) << BOLDBLUE << "Indx" << cIndx << " : Bx#" << +cBxId << " : " << cDataWord << RESET;
                cBxId++;
            }
            cIter++;
            cIndx++;
        } while(cIter < cData.end());
        return true;
    }
    else // raw counter readout - have to parse stubs in sw
    {
        // clear stub buffer
        fStubBuffer.clear();
        cIndx = 0;
        cIter += 4;
        std::vector<uint32_t> cBxCounter;
        do {
            std::stringstream cPacket512;
            for(uint8_t cFrag = 0; cFrag < 256 / 32; cFrag++)
            {
                if(cIter >= cData.end()) break;
                cPacket512 << std::bitset<32>(*cIter);
                // LOG (INFO) << BOLDGREEN << std::bitset<32>(*cIter);
                LOG(INFO) << BOLDGREEN << std::bitset<32>(*cIter);
                cIter++;
            }
            if(cIter < cData.end() && cPacket512.str().length() >= 80)
            {
                std::pair<std::string, std::string> cDataWrd;
                cDataWrd.first  = cPacket512.str().substr(0, 32);     //(uint32_t)std::stoi( cPacket512.str().substr(0,32), 0, 2) ;
                cDataWrd.second = cPacket512.str().substr(32, 6 * 8); // if 640 this needs to change
                for(size_t cClk = 0; cClk < 8; cClk++)
                {
                    fStubBuffer.push_back(static_cast<uint8_t>(std::stoi(cDataWrd.second.substr(6 * cClk, 6), 0, 2)));
                    // LOG (INFO) << BOLDBLUE << "Bx " << cDataWrd.first << " : " << std::bitset<6>(fStubBuffer[fStubBuffer.size()-1]) << RESET;
                    LOG(INFO) << BOLDBLUE << "Bx " << cDataWrd.first << " : " << std::bitset<6>(fStubBuffer[fStubBuffer.size() - 1]) << RESET;
                    // cBxCounter.push_back( static_cast<uint32_t>( std::stoi( cPacket512.str().substr(0,32), 0, 2 ) ) );
                }
            }
            cIndx++;
        } while(cIter < cData.end());
        cSuccess = CheckStartPattern();
    }
    return cSuccess;
}

// Added For FastReadout
void D19cPSCounterFWInterface::FastRead(const BeBoard* pBoard)
{
    // LOG(DEBUG) << BOLDBLUE << "D19cL1ReadoutInterface Resetting readout..." << RESET;
    // fTheRegManager->WriteReg("fc7_daq_ctrl.readout_block.control.readout_reset", 0x1);
    // std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    // fTheRegManager->WriteReg("fc7_daq_ctrl.readout_block.control.readout_reset", 0x0);
    // std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

    // auto     cDDR3Calibrated = (fTheRegManager->ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
    // uint16_t cAttempts       = 0;
    // uint16_t cMaxAttempts    = 1000;

    // while(!cDDR3Calibrated && (cAttempts < cMaxAttempts))
    // {
    //     LOG(INFO) << "Waiting for DDR3 to finish initial calibration";
    //     std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    //     cDDR3Calibrated = (fTheRegManager->ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
    //     cAttempts++;
    // }

    fPSCounterData.clear();

    fTheRegManager->WriteReg("fc7_daq_ctrl.physical_interface_block.control.decoder_reset", 0x1);

    LOG(INFO) << BOLDGREEN << "FastRead() Starts" << RESET;

    fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset", 100);
    fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", 50);
    fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", 50);

    fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_delay.after_clear_counters", 100);
    fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_delay.after_close_shutter", 50);
    fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_delay.after_open_shutter", 50);

    std::string cStartPattern = "111111111111111";
    auto        cCicVeto      = fTheRegManager->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto");
    auto        cEvenSel      = fTheRegManager->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en.select_even");
    auto        cDDR3Src      = fTheRegManager->ReadReg("fc7_daq_cnfg.ddr3_debug");
    auto        cRawMode      = fTheRegManager->ReadReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en");

    // Added for debugging:
    auto cFastCommandFSM = fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");
    auto cTriggerIn      = fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
    auto cAsyncDone      = fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.antenna_async_done");

    // if( cRawMode ) LOG (DEBUG) << BOLDGREEN << "PS counter capture in RAW mode" << RESET;
    //  else LOG (DEBUG) << BOLDGREEN << "PS counter capture in DECODE mode" << RESET;
    //   LOG (DEBUG) << BOLDYELLOW << "CIC veto : 0x" << std::hex << +cCicVeto << std::dec << RESET;
    //   LOG (DEBUG) << BOLDYELLOW << "Even SEL : 0x" << std::hex << +cEvenSel << std::dec << RESET;
    //    LOG (DEBUG) << BOLDYELLOW << "DDR Debug src : 0x" << std::hex << +cDDR3Src << std::dec << RESET;

    if(cRawMode)
        LOG(INFO) << BOLDGREEN << "PS counter capture in RAW mode" << RESET;
    else
        LOG(INFO) << BOLDGREEN << "PS counter capture in DECODE mode" << RESET;
    LOG(INFO) << BOLDYELLOW << "CIC veto : 0x" << std::hex << +cCicVeto << std::dec << RESET;
    LOG(INFO) << BOLDYELLOW << "Even SEL : 0x" << std::hex << +cEvenSel << std::dec << RESET;
    LOG(INFO) << BOLDYELLOW << "DDR Debug src : 0x" << std::hex << +cDDR3Src << std::dec << RESET;

    // Added for debugging:
    LOG(INFO) << BOLDYELLOW << "FastCommandFSM : 0x" << std::hex << +cFastCommandFSM << std::dec << RESET;
    LOG(INFO) << BOLDYELLOW << "cTriggerIn : 0x" << std::hex << +cTriggerIn << std::dec << RESET;
    LOG(INFO) << BOLDYELLOW << "cAsyncDone : 0x" << std::hex << +cAsyncDone << std::dec << RESET;

    //    for( int cHybrid = 0 ; cHybrid < 1 ; cHybrid++)
    // for( int cHybrid = 0 ; cHybrid < 2 ; cHybrid++)
    {
        // fTheRegManager->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid);
        auto   cDecoderState  = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.store_fsm_state");
        auto   cCountersReady = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.chip_counters_done");
        size_t cIterations    = 0;
        do {
            if(cDecoderState == 0x00 && cCountersReady == 0x00)
            {
                // PS_Start_counters_read();
                // fFastCommandInterface->SendGlobalReSync();
                // fFastCommandInterface->SendGlobalCounterReset();
                fTheRegManager->WriteReg("fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);
                //                LOG(INFO) << BOLDGREEN << "Manually Send PS Start Counter Readout" << RESET;
            }
            if(cIterations % 100 == 0)
                LOG(DEBUG) << BOLDYELLOW << "[Iter#" << +cIterations
                           << "] Hybrid#"
                           // << cHybrid
                           << " Decoder state : 0x" << std::hex << cDecoderState << std::dec << " Counters Ready : 0x" << std::hex << cCountersReady << std::dec << RESET;
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
            cDecoderState  = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.store_fsm_state");
            cCountersReady = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.chip_counters_done");

            // Added for debugging:
            auto cFastCommandFSM = fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");
            auto cTriggerIn      = fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
            auto cAsyncDone      = fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.antenna_async_done");
            if(cIterations % 100 == 0)
            {
                LOG(INFO) << BOLDYELLOW << "FastCommandFSM : 0x" << std::hex << +cFastCommandFSM << std::dec << RESET;
                LOG(INFO) << BOLDYELLOW << "cTriggerIn : 0x" << std::hex << +cTriggerIn << std::dec << RESET;
                LOG(INFO) << BOLDYELLOW << "cAsyncDone : 0x" << std::hex << +cAsyncDone << std::dec << RESET;
            }
            cIterations++;
        } while(cIterations < 1000 && !(cDecoderState == 0x00 && cCountersReady == 0x01));
        // }while(!(cDecoderState==0x00 && cCountersReady == 0x01 ) );

        auto cStrtPtrn = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.start_pattern_not_found");
        // LOG (DEBUG) << BOLDYELLOW << "Decoder Block start pattern not found : " <<  +cStrtPtrn << RESET;
        LOG(INFO) << BOLDYELLOW << "Decoder Block start pattern not found : " << +cStrtPtrn << RESET;
        // check if start pattern has been received
        auto cRxdStart  = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.received_start");
        auto cNPkgsDcdr = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_stats.package");
        auto cNClksDcdr = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_stats.clocks");
        LOG(DEBUG) << BOLDYELLOW << "Decoder Block rxd start : " << +cRxdStart << " clock cycles after reset " << RESET;
        LOG(DEBUG) << BOLDYELLOW << "Decoder Block rxd  : " << +cNPkgsDcdr << " package  " << RESET;
        LOG(DEBUG) << BOLDYELLOW << "Decoder Block rxd  data for " << +cNClksDcdr << " clk cycles  " << RESET;

        //        PS_Start_counters_read();
    }

    // Manually stop triggers
    // fTheRegManager->WriteReg("fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);

    // LOG (INFO) << BOLDYELLOW << "Decoder Block Start_PTRN_NT_FOUND 0x" << std::hex << +cStartPattern << std::dec << RESET;
    // check number of words in ddr3

    auto cNFIFOentries = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.num_fifo_entry");
    auto cNBoxcars     = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.num_boxcar_rxd");
    //    LOG (DEBUG) << BOLDYELLOW << "DDR3 packer block : # FIFO entries 0x" << std::hex << +cNFIFOentries << std::dec << RESET;
    //    LOG (DEBUG) << BOLDYELLOW << "DDR3 packer block : # boxcars received 0x" << std::hex << +cNBoxcars << std::dec << RESET;
    LOG(INFO) << BOLDYELLOW << "DDR3 packer block : # FIFO entries 0x" << std::hex << +cNFIFOentries << std::dec << RESET;
    LOG(INFO) << BOLDYELLOW << "DDR3 packer block : # boxcars received 0x" << std::hex << +cNBoxcars << std::dec << RESET;

    auto   cDDR3state  = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state");
    size_t cIterations = 0;
    LOG(INFO) << BOLDYELLOW << " DDR3 packer block : starting FSM state 0x" << std::hex << +cDDR3state << std::dec << RESET;

    auto cDecoderState  = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.store_fsm_state");
    auto cCountersReady = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.chip_counters_done");

    std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] async_counter_decode = 0x" << std::hex << fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode")
              << std::dec << std::endl;

    //    LOG (INFO) << BOLDRED << "Writing fc7_daq_ctrl.fast_command_block.control.fast_reset and fc7_daq_ctrl.fast_command_block.control.fast_orbit_reset to 1" << RESET;

    //    fTheRegManager->WriteReg("fc7_daq_ctrl.fast_command_block.control.fast_reset", 1);
    //    fTheRegManager->WriteReg("fc7_daq_ctrl.fast_command_block.control.fast_orbit_reset", 1);

    // auto cFastReset = fTheRegManager->ReadReg("fc7_daq_ctrl.fast_command_block.control.fast_reset");
    // auto cFastOrbitReset = fTheRegManager->ReadReg("fc7_daq_ctrl.fast_command_block.control.fast_orbit_reset");

    do {
        // for(size_t hybridId = 0; hybridId<2; ++hybridId)
        // {
        //     LOG(INFO) << BOLDGREEN << "Selected hybrid " << hybridId << RESET;
        //     fTheRegManager->WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_select_hybrid", hybridId);
        //     LOG(INFO) << BOLDGREEN << "async_counter_decode = 0x" << std::hex << fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode") << std::dec << RESET;
        //     LOG(INFO) << BOLDGREEN << "start_pattern_not_found = 0x" << std::hex << fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.start_pattern_not_found") <<
        //     std::dec << RESET; LOG(INFO) << BOLDGREEN << "ready = 0x" << std::hex << fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.ready") << std::dec <<
        //     RESET; LOG(INFO) << BOLDGREEN << "state = 0x" << std::hex << fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.state") << std::dec << RESET; LOG(INFO)
        //     << BOLDGREEN << "received_start = 0x" << std::hex << fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.received_start") << std::dec << RESET; LOG(INFO)
        //     << BOLDGREEN << "chip_counters_done = 0x" << std::hex << fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.chip_counters_done") << std::dec << RESET;
        //     LOG(INFO) << BOLDGREEN << "store_fsm_state = 0x" << std::hex << fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.store_fsm_state") << std::dec <<
        //     RESET; LOG(INFO) << BOLDGREEN << "start_pattern_not_found = 0x" << std::hex <<
        //     fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.start_pattern_not_found") << std::dec << RESET;
        // }
        if(cIterations % 100 == 0) LOG(INFO) << BOLDYELLOW << "Iteration #" << cIterations << " DDR3 packer block : FSM state 0x" << std::hex << +cDDR3state << std::dec << RESET;
        if(cIterations % 100 == 0) LOG(INFO) << BOLDGREEN << "Iteration #" << cIterations << " DDR3 packer block : FIFO entries 0x" << std::hex << +cNFIFOentries << std::dec << RESET;
        if(cIterations % 100 == 0) LOG(INFO) << BOLDRED << "Iteration #" << cIterations << " DDR3 packer block : # boxcars recieved 0x" << std::hex << +cNBoxcars << std::dec << RESET;
        if(cIterations % 100 == 0)
            LOG(INFO) << BOLDYELLOW << "Iter#" << +cIterations << " Decoder state : 0x" << std::hex << cDecoderState << std::dec << " Counters Ready : 0x" << std::hex << cCountersReady << std::dec
                      << RESET;
        // if(cIterations%100==0) LOG (INFO) << BOLDRED << "Iteration #" << cIterations
        // << " FastReset Flag: 0x" << std::hex << +cFastReset << std::dec << RESET;
        // if(cIterations%100==0) LOG (INFO) << BOLDRED << "Iteration #" << cIterations
        // << " FastOrbitReset Flag: 0x" << std::hex << +cFastOrbitReset << std::dec << RESET;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        cDDR3state     = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state");
        cNBoxcars      = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.num_boxcar_rxd");
        cNFIFOentries  = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.num_fifo_entry");
        cDecoderState  = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.store_fsm_state");
        cCountersReady = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.chip_counters_done");
        // cFastReset  = fTheRegManager->ReadReg("fc7_daq_ctrl.fast_command_block.control.fast_reset");
        // cFastOrbitReset  = fTheRegManager->ReadReg("fc7_daq_ctrl.fast_command_block.control.fast_orbit_reset");

        // Added for debugging:
        //        auto cFastCommandFSM = fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");
        //        auto cTriggerIn = fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        //        auto cAsyncDone = fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.antenna_async_done");
        //        if(cIterations%100==0)
        //        {
        //            LOG (INFO) << BOLDYELLOW << "FastCommandFSM : 0x" << std::hex << +cFastCommandFSM << std::dec << RESET;
        //            LOG (INFO) << BOLDYELLOW << "cTriggerIn : 0x" << std::hex << +cTriggerIn << std::dec << RESET;
        //            LOG (INFO) << BOLDYELLOW << "cAsyncDone : 0x" << std::hex << +cAsyncDone << std::dec << RESET;
        //        }

        cIterations++;
    } while(cDDR3state != 0x1); // while not in idle state
    // }while( cDDR3state != 0x1  && cIterations < 2000);// while not in idle state

    LOG(INFO) << BOLDYELLOW << "DDR3 State Set to 0x" << std::hex << +cDDR3state << std::dec << RESET;
    LOG(INFO) << BOLDYELLOW << "#FIFO Entries 0x" << std::hex << +cNFIFOentries << std::dec << RESET;

    // Testing using ReadPSCountersFast
    LOG(INFO) << BOLDRED << "Trying ReadPSCountersFast" << RESET;
    // ReadPSCountersFast(1, 8, 0); // Raw Mode, Chip ID, Hybrid ID

    LOG(INFO) << BOLDRED << "Printing DDR3 Content" << RESET;
    auto cData = fTheRegManager->ReadBlockRegOffset("fc7_daq_ddr3", cNFIFOentries * 1024 / 32, 0);
    // for(auto data: cData)
    // {
    // std::cout<<std::hex<<data<< " " <<std::dec;
    // }

    // fTheRegManager->WriteReg("fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);

    std::ofstream outfile(fOutputFile);
    outfile << getPatternPrintout(cData, 1, false) << std::endl;

    //    LOG (INFO) << BOLDRED << "Trying ReadPSSCCountersFast" << RESET;
    //    std::vector<uint32_t> pData;
    //    ReadPSSCCountersFast(pBoard, pData, cRawMode);

    auto cIter = cData.begin();
    cIter += +32; // offset header
    size_t            cPrintUntil = 10;
    std::stringstream cDataH1;
    std::stringstream cDataH0;
    LOG(INFO) << BOLDGREEN << "Parsing Entries" << RESET;
    for(size_t cEntry = 1; cEntry < cNFIFOentries - 1; cEntry++) // without trailer
    {
        std::stringstream cHeaderW0, cHeaderW1;
        for(size_t cNibble = 0; cNibble < 2; cNibble++)
        {
            for(size_t cOffset = 0; cOffset < 4; cOffset++)
            {
                LOG(DEBUG) << BOLDBLUE << "Entry#: " << cEntry << " Nibble#: " << cNibble << " Offset: " << cOffset << RESET;
                if(cNibble % 2 == 0)
                    cHeaderW1 << std::hex << std::setw(8) << std::setfill('0') << *cIter << std::dec;
                else
                    cHeaderW0 << std::hex << std::setw(8) << std::setfill('0') << *cIter << std::dec;
                cIter++;
            }
        }
        std::stringstream cHeader;
        cHeader << cHeaderW0.str() << cHeaderW1.str();

        std::vector<std::pair<std::string, uint8_t>>  cHdrFlds;
        std::vector<std::pair<std::string, uint64_t>> cHdrVals;
        cHdrFlds.push_back(std::make_pair("DDR3Hdr", 46));
        cHdrFlds.push_back(std::make_pair("ClkCntr1", 4));
        cHdrFlds.push_back(std::make_pair("ClkCntr0", 4));
        cHdrFlds.push_back(std::make_pair("PkgCntr1", 4));
        cHdrFlds.push_back(std::make_pair("PkgCntr0", 4));
        cHdrFlds.push_back(std::make_pair("HybridId1", 1));
        cHdrFlds.push_back(std::make_pair("HybridId0", 1));
        size_t cStrOffset = 0;
        for(auto cHdrFld: cHdrFlds)
        {
            std::stringstream cFld;
            cFld << std::hex << cHeader.str().substr(cStrOffset, cHdrFld.second);
            unsigned int cVal;
            cFld >> cVal;
            cStrOffset += cHdrFld.second;
            cHdrVals.push_back(std::make_pair(cHdrFld.first, cVal));
        }
        if(cEntry < cPrintUntil)
        {
            LOG(DEBUG) << BOLDYELLOW << cHeader.str() << RESET;
            for(auto cData: cHdrVals)
            {
                if(cData.first == "DDR3Hdr") continue;
                LOG(DEBUG) << BOLDYELLOW << cData.first << " : " << cData.second << RESET;
            }
        }

        std::stringstream cPayload;
        for(size_t cWord = 0; cWord < 3; cWord++)
        {
            std::stringstream cPayloadW0, cPayloadW1;
            for(size_t cNibble = 0; cNibble < 2; cNibble++)
            {
                for(size_t cOffset = 0; cOffset < 4; cOffset++)
                {
                    if(cNibble % 2 == 0)
                        cPayloadW1 << std::hex << std::setw(8) << std::setfill('0') << *cIter << std::dec;
                    else
                        cPayloadW0 << std::hex << std::setw(8) << std::setfill('0') << *cIter << std::dec;
                    cIter++;
                }
            }
            cPayload << cPayloadW0.str() << cPayloadW1.str();
        }
        std::vector<std::pair<std::string, uint8_t>>     cPayloadFlds;
        std::vector<std::pair<std::string, std::string>> cPayloadVals;
        //        LOG (DEBUG) << BOLDYELLOW << "Payload Set" << RESET;
        auto cDataLength = cPayload.str().length() / 2;
        cPayloadFlds.push_back(std::make_pair("Data1", cDataLength));
        cPayloadFlds.push_back(std::make_pair("Data0", cDataLength));
        cStrOffset = 0;
        for(auto cPayloadFld: cPayloadFlds)
        {
            std::string cVal = cPayload.str().substr(cStrOffset, cPayloadFld.second);
            cStrOffset += cPayloadFld.second;
            cPayloadVals.push_back(std::make_pair(cPayloadFld.first, cVal));
        }

        cStrOffset = 0;
        for(size_t cBoxCar = 0; cBoxCar < 8; cBoxCar++)
        {
            std::stringstream cFld1;
            cFld1 << std::hex << cPayloadVals[1].second.substr(cStrOffset, 12); // 48 bits
            std::stringstream cFld0;
            cFld0 << std::hex << cPayloadVals[0].second.substr(cStrOffset, 12); // 48 bits

            if(cEntry < cPrintUntil)
            {
                LOG(DEBUG) << BOLDYELLOW << cFld1.str() << RESET;
                LOG(DEBUG) << BOLDYELLOW << cFld0.str() << RESET;
            }
            uint64_t cVal1;
            cFld1 >> cVal1;
            uint64_t cVal0;
            cFld0 >> cVal0;

            std::stringstream cBinaryValue1;
            cBinaryValue1 << std::bitset<48>(cVal1);
            std::stringstream cBinaryValue0;
            cBinaryValue0 << std::bitset<48>(cVal0);
            size_t cStrOffset2 = 0;
            for(size_t cBxId = 0; cBxId < 8; cBxId++)
            {
                std::string cRev1 = cBinaryValue1.str().substr(cStrOffset2, 6); // reverse( cRev1.begin(), cRev1.end() );
                std::string cRev0 = cBinaryValue0.str().substr(cStrOffset2, 6); // reverse( cRev0.begin(), cRev0.end() );
                cDataH1 << cRev1;                                               // 6 bits per bx
                cDataH0 << cRev0;                                               // 6 bits per bx
                if(cEntry < cPrintUntil) { LOG(DEBUG) << BOLDMAGENTA << "\t.." << cRev1 << "\t..." << cRev0 << RESET; }
                cStrOffset2 += 6;
            }
            cStrOffset += 12; // 12 hex words -- 48 bits
        }
    }
    std::vector<std::string> cDataFrmHybs{cDataH1.str(), cDataH0.str()};
    size_t                   cNHybrids = 0;
    LOG(INFO) << BOLDGREEN << "Testing FastRead" << RESET;
    fSuccessFastRead = true;
    for(auto cData: cDataFrmHybs)
    {
        if(cNHybrids > 0) continue;
        std::vector<uint8_t> cFeMappingPSR{6, 7, 3, 2, 1, 0, 4, 5}; // Index hybrid FE Id , Value CIC FE Id
        std::vector<uint8_t> cFeMappingPSL{1, 0, 4, 5, 6, 7, 3, 2}; // Index hybrid FE Id , Value CIC FE Id
        std::vector<uint8_t> cFeMapping = (cNHybrids == 0) ? cFeMappingPSR : cFeMappingPSL;

        std::size_t cPSHbit1 = cData.find('1');
        LOG(DEBUG) << cPSHbit1 << ":" << cData.substr(cPSHbit1, 6 * 8 * 8) << "\t" << cData.length() << RESET;
        std::vector<std::pair<std::string, uint8_t>> cHdrFlds;
        cHdrFlds.push_back(std::make_pair("CnfgBit", 1));
        cHdrFlds.push_back(std::make_pair("Status", 9));
        cHdrFlds.push_back(std::make_pair("BxId", 12));
        cHdrFlds.push_back(std::make_pair("NbStubs", 6));
        std::vector<std::pair<std::string, uint8_t>> cStbFlds;
        cStbFlds.push_back(std::make_pair("Offset", 3));
        cStbFlds.push_back(std::make_pair("FeId", 3));
        cStbFlds.push_back(std::make_pair("Stub", 15));
        size_t cStrOffset = cPSHbit1;
        size_t cNPkts     = 0;
        fTheRegManager->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cNHybrids);
        auto                                     cNPkgsDcdr      = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_stats.package");
        size_t                                   cFrstPktWthStbs = 0;
        size_t                                   cLstPktWthStbs  = 0;
        std::map<uint8_t, std::vector<uint32_t>> cCounters;
        bool                                     cStartPatternRxd = true;
        do {
            size_t                          cPktLngth = 0;
            std::map<std::string, uint16_t> cHdrVals;
            for(auto cHdrFld: cHdrFlds)
            {
                auto cSubStr            = cData.substr(cStrOffset, cHdrFld.second);
                cHdrVals[cHdrFld.first] = std::stoi(cSubStr, 0, 2);
                cStrOffset += cHdrFld.second;
                cPktLngth += cHdrFld.second;
                if(cHdrFld.first == "NbStubs")
                {
                    auto cStubPkt = cData.substr(cStrOffset, 6 * 8 * 8 - cPktLngth);
                    // if( cNPkts < 20)
                    // LOG (DEBUG) << BOLDYELLOW << "Pkt#" << cNPkts <<  " BxId " << cHdrVals["BxId"] << ", "  << cHdrVals["NbStubs"] <<  RESET;
                    LOG(INFO) << BOLDYELLOW << "Pkt#" << cNPkts << " BxId " << cHdrVals["BxId"] << ", " << cHdrVals["NbStubs"] << RESET;
                    if(cHdrVals["NbStubs"] > 0 && cFrstPktWthStbs == 0) cFrstPktWthStbs = cNPkts;
                    if(cHdrVals["NbStubs"] > 0) cLstPktWthStbs = cNPkts;
                    if(cStubPkt.length() < 21 * cHdrVals["NbStubs"]) break;

                    for(size_t cStbIndx = 0; cStbIndx < (size_t)cHdrVals[cHdrFld.first]; cStbIndx++)
                    {
                        auto cStubBits = cStubPkt.substr(cStbIndx * 21, 21);
                        if(cFrstPktWthStbs != 0 && (cFrstPktWthStbs == cLstPktWthStbs)) cStartPatternRxd = cStartPatternRxd && cStubBits.substr(6, 15) == cStartPattern;
                        // if( cNPkts < 10 ) LOG (DEBUG) << BOLDYELLOW << " Stub#" << cStbIndx << " : " << cStubBits << RESET;
                        if(cNPkts < 10) LOG(INFO) << BOLDYELLOW << " Stub#" << cStbIndx << " : " << cStubBits << RESET;
                        size_t                          cStbOffst = 0;
                        std::map<std::string, uint16_t> cStubVals;
                        for(auto cStbFld: cStbFlds)
                        {
                            auto cStr = cStubBits.substr(cStbOffst, cStbFld.second);
                            if(cStbFld.first == "Stub")
                            {
                                uint16_t cCounterValue = std::stoi(cStr.substr(8, 6) + cStr.substr(0, 7), 0, 2);
                                if(cStartPatternRxd && (cFrstPktWthStbs != cLstPktWthStbs)) cCounters[cStubVals["FeId"]].push_back(cCounterValue);
                                // if( cNPkts%10 == 0 )  LOG (DEBUG) << BOLDGREEN << "FeId#" << cStubVals["FeId"] << " Offset " << cStubVals["Offset"] << " Stub : " << cStr << " -- " << cCounterValue
                                // << RESET;
                                if(cNPkts % 10 == 0)
                                    LOG(INFO) << BOLDGREEN << "FeId#" << cStubVals["FeId"] << " Offset " << cStubVals["Offset"] << " Stub : " << cStr << " -- " << cCounterValue << RESET;
                            }
                            else
                                cStubVals[cStbFld.first] = std::stoi(cStr, 0, 2);
                            cStbOffst += cStbFld.second;
                        }
                    }
                }
            }
            cStrOffset = cPSHbit1 + 6 * 8 * 8 * (1 + cNPkts);
            cNPkts++;
        } while(cNPkts < cNPkgsDcdr); // cStrOffset < cDataH1.str().length()   );
        //        LOG (DEBUG) << BOLDYELLOW << "First packet with a stub " << cFrstPktWthStbs << RESET;
        //        LOG (DEBUG) << BOLDYELLOW << "Last packet with a stub " << cLstPktWthStbs << RESET;
        //        LOG (DEBUG) << BOLDYELLOW << "Number of FEs with counters read-out " << cCounters.size() << RESET;
        LOG(INFO) << BOLDYELLOW << "First packet with a stub " << cFrstPktWthStbs << RESET;
        LOG(INFO) << BOLDYELLOW << "Last packet with a stub " << cLstPktWthStbs << RESET;
        LOG(INFO) << BOLDYELLOW << "Number of FEs with counters read-out " << cCounters.size() << RESET;

        // Temporary Commented
        //        fSuccessFastRead = fSuccessFastRead && (cCounters.size() == 8 );
        if(!cStartPatternRxd) LOG(DEBUG) << BOLDRED << "Start pattern not found" << RESET;
        for(auto cCounter: cCounters)
        {
            uint8_t cIdHybrid = cFeMapping[cCounter.first];
            LOG(DEBUG) << BOLDYELLOW << "FE#" << +cCounter.first << " [ MPA#" << +cIdHybrid << " ] - " << cCounter.second.size() << " counters received." << RESET;
            size_t                cCntrIndx = 0;
            std::vector<uint32_t> cSSACntrs;
            std::vector<uint32_t> cMPACntrs;
            for(auto cCntrInfo: cCounter.second)
            {
                if(cCntrIndx < 120 * 16 - 1)
                    cMPACntrs.push_back(cCntrInfo - 1);
                else
                    cSSACntrs.push_back(cCntrInfo - 1);
                cCntrIndx++;
            }
            // clear vector holding counter data for this MPA
            uint8_t  cType     = 1; // MPA is type 1
            uint32_t cId       = (pBoard->getId() << (3 + 6 + 4 + 1 + 4)) | (0 << (3 + 6 + 4 + 1)) | (cNHybrids << (3 + 6 + 1)) | (cIdHybrid << (3 + 1)) | cType;
            auto     cIterator = fPSCounterData.find(cId);
            if(cIterator != fPSCounterData.end()) cIterator->second.clear();
            // force first pixel in MPA to be off
            fPSCounterData[cId].push_back(0); // only valid for MPA1
            for(auto cMPACntr: cMPACntrs) { fPSCounterData[cId].push_back(cMPACntr); }
            fSuccessFastRead = fSuccessFastRead && (fPSCounterData[cId].size() == 16 * 120);
            auto cMeanMPA    = std::accumulate(fPSCounterData[cId].begin(), fPSCounterData[cId].end(), 0.0) / fPSCounterData[cId].size();
            // clear vector holding counter data for this SSA
            cType     = 0; // SSA is type 0
            cId       = (pBoard->getId() << (3 + 6 + 4 + 1 + 4)) | (0 << (3 + 6 + 4 + 1)) | (cNHybrids << (3 + 6 + 1)) | (cIdHybrid << (3 + 1)) | cType;
            cIterator = fPSCounterData.find(cId);
            if(cIterator != fPSCounterData.end()) cIterator->second.clear();
            for(auto cSSACntr: cSSACntrs) { fPSCounterData[cId].push_back(cSSACntr); }
            if(fPSCounterData[cId].size() < 120) LOG(DEBUG) << BOLDRED << "DID NOT RX ALL SSA CNTRS - readback " << fPSCounterData[cId].size() << " counters." << RESET;
            // for(size_t cIndex = fPSCounterData[cId].size(); cIndex < 120; cIndex++)
            // {
            //     fPSCounterData[cId].push_back(0);
            // }
            fSuccessFastRead = fSuccessFastRead && (fPSCounterData[cId].size() == 1 * 120);
            auto cMeanSSA    = std::accumulate(fPSCounterData[cId].begin(), fPSCounterData[cId].end(), 0.0) / fPSCounterData[cId].size();
            if(fSuccessFastRead)
                LOG(DEBUG) << BOLDGREEN << "\t[MPA#" << +cIdHybrid << "] fPSCounterData holds " << fPSCounterData[cId].size() << " elements, Mean value  " << cMeanMPA << " ]" << BOLDCYAN
                           << "\t[SSA] fPSCounterData holds " << fPSCounterData[cId].size() << " elements, Mean value " << cMeanSSA << RESET;
            else
                LOG(DEBUG) << BOLDRED << "\t[MPA#" << +cIdHybrid << "] fPSCounterData holds " << fPSCounterData[cId].size() << " elements, Mean value  " << cMeanMPA << " ]" << BOLDCYAN
                           << "\t[SSA] fPSCounterData holds " << fPSCounterData[cId].size() << " elements, Mean value " << cMeanSSA << RESET;
        }
        cNHybrids++;
    }
    //    if( fSuccessFastRead ) LOG (DEBUG) << BOLDGREEN << "Succesful decoding of fast counters " << RESET;
    //    else LOG (DEBUG) << BOLDRED << "FAILED to decode fast counters " << RESET;
    if(fSuccessFastRead)
        LOG(INFO) << BOLDGREEN << "Succesful decoding of fast counters " << RESET;
    else
        LOG(INFO) << BOLDRED << "FAILED to decode fast counters " << RESET;
}
// Added For FastReadout (End)

// method to read SSA/MPA counters over stub lines on single chip cards
// void D19cPSCounterFWInterface::ReadPSSCCountersFast(BeBoard* pBoard, std::vector<uint32_t>& pData, uint8_t pRawMode)
void D19cPSCounterFWInterface::ReadPSSCCountersFast(const BeBoard* pBoard, std::vector<uint32_t>& pData, uint8_t pRawMode)
{
    this->fTheRegManager->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", 0x0);
    this->fTheRegManager->WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", pRawMode);
    this->fTheRegManager->WriteReg("fc7_daq_cnfg.physical_interface_block.first_counter_delay", fPSCounterDelay);
    pData.clear();
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                uint8_t cPairId = (cChip->getId() % 2 == 0) ? 1 : 0;
                uint8_t cChipId = (fPairSelect) ? cPairId : cChip->getId();

                this->fTheRegManager->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cChipId);
                auto cStatus = this->fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");
                // LOG(DEBUG) << BOLDBLUE << "Fast SSA counter readback... Chip#" << +cChip->getId() << " PS counters status [pre-start] is " << +cStatus << " [ offset is " << +fPSCounterDelay << "]"
                //            << RESET;
                PS_Start_counters_read();
                do {
                    // LOG(DEBUG) << BOLDBLUE << "PS counters status is " << +cStatus << RESET;
                    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
                    cStatus = this->fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");
                } while(cStatus == 0);

                LOG(DEBUG) << BOLDBLUE << "PS counters " << BOLDGREEN << " READY " << RESET;
                uint32_t cDataWord    = 0x0000;
                uint32_t cWordCounter = 0;
                for(int cChannelId = 0; cChannelId < (int)cChip->size(); cChannelId++)
                {
                    if(pRawMode == 1) // moved over from old MPA method .. needs to be checked/generatlized for both SSA/MPA case
                    {
                        uint32_t cycle = 0;
                        // MPA will output 16*120 + 120 counters
                        // SSA witll output 120 counters
                        size_t                cNCounters = (cChip->getFrontEndType() == FrontEndType::MPA2) ? 2040 : cChip->size();
                        std::vector<uint16_t> count(cNCounters, 0);
                        for(int i = 0; i < 20000; i++)
                        {
                            uint32_t fifo1_word = fTheRegManager->ReadReg("fc7_daq_ctrl.physical_interface_block.fifo1_data");
                            uint32_t fifo2_word = fTheRegManager->ReadReg("fc7_daq_ctrl.physical_interface_block.fifo2_data");

                            uint32_t line1 = (fifo1_word & 0x0000FF) >> 0;  // to_number(fifo1_word,8,0)
                            uint32_t line2 = (fifo1_word & 0x00FF00) >> 8;  // to_number(fifo1_word,16,8)
                            uint32_t line3 = (fifo1_word & 0xFF0000) >> 16; //  to_number(fifo1_word,24,16)

                            uint32_t line4 = (fifo2_word & 0x0000FF) >> 0; // to_number(fifo2_word,8,0)
                            uint32_t line5 = (fifo2_word & 0x00FF00) >> 8; // to_number(fifo2_word,16,8)

                            if(((line1 & 0x80) == 128) && ((line4 & 0x80) == 128))
                            {
                                uint32_t temp = ((line2 & 0x20) << 9) | ((line3 & 0x20) << 8) | ((line4 & 0x20) << 7) | ((line5 & 0x20) << 6) | ((line1 & 0x10) << 6) | ((line2 & 0x10) << 5) |
                                                ((line3 & 0x10) << 4) | ((line4 & 0x10) << 3) | ((line5 & 0x80) >> 1) | ((line1 & 0x40) >> 1) | ((line2 & 0x40) >> 2) | ((line3 & 0x40) >> 3) |
                                                ((line4 & 0x40) >> 4) | ((line5 & 0x40) >> 5) | ((line1 & 0x20) >> 5);
                                if(temp != 0)
                                {
                                    count[cycle] = temp - 1;
                                    LOG(INFO) << BOLDYELLOW << "Count#" << cycle << ": " << count[cycle] << RESET;
                                    cycle += 1;
                                }
                            }
                        }
                    }
                    else
                    {
                        uint32_t fifo2_word = fTheRegManager->ReadReg("fc7_daq_ctrl.physical_interface_block.fifo2_data");
                        cDataWord           = (cDataWord) | (fifo2_word << (cWordCounter & 0x1) * 16);
                        if(cChannelId < 5 || cChannelId > 115)
                        {
                            LOG(INFO) << BOLDGREEN << "Chip#" << +cChip->getId() << " Pair#" << +cPairId << " Chnl#" << +cChannelId << "\t\t" << std::bitset<32>(fifo2_word) << " [ " << fifo2_word
                                      << " ] " << RESET;
                        }
                        if((cWordCounter & 0x1) == 1)
                        {
                            pData.push_back(cDataWord);
                            cDataWord = 0x0000;
                        }
                        cWordCounter++;
                    }
                }
            }
        }
    }
}

void D19cPSCounterFWInterface::GetCounterData(const BeBoard* pBoard)
{
    // LOG(DEBUG) << BOLDYELLOW << "D19cPSCounterFWInterface::GetCounterData" << RESET;
    LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::GetCounterData" << RESET;
    auto cFrontEndTypes = pBoard->connectedFrontEndTypes();
    // LOG(DEBUG) << BOLDYELLOW << cFrontEndTypes.size() << " different types of Chips connected to BeBoard#" << +pBoard->getId() << RESET;
    LOG(INFO) << BOLDYELLOW << cFrontEndTypes.size() << " different types of Chips connected to BeBoard#" << +pBoard->getId() << RESET;
    if(fPSCounterFast == 0) // readout over registers
    {
        SlowRead(pBoard);
    }
    else // readout over fast interface
    {
        LOG(INFO) << BOLDGREEN << "Running FastRead()" << RESET;
        FastRead(pBoard);
    }
}
void D19cPSCounterFWInterface::FillData()
{
    // use fPSCounterData to fill 32-bit word vector
    // this should match what you expect in the event decoder
    fData.clear();
    // counter data will be filled into data vector
    // each 32-bit word contains 2 counters (30 bits)
    // will first fill in MPA data .. then SSA data
    // order of MPAs/SSAs will be the same as that defined
    // MSB indicates if its an MPA/SSA
    // 1 for MPA, 0 for SSA
    // by the hybrid node in the xml
    for(auto cCountersFromFE: fPSCounterData)
    {
        // LOG(DEBUG) << BOLDYELLOW << "D19cPSCounterFWInterface::FillData Filling data vector with counter information from Id" << cCountersFromFE.first << RESET;
        for(auto cIter = cCountersFromFE.second.begin(); cIter < cCountersFromFE.second.end(); cIter += 2)
        {
            uint32_t cValue = (cCountersFromFE.first << 31) | (*(cIter + 1) << 15) | (*cIter);
            // LOG (DEBUG) << BOLDYELLOW << "\t... First counter 0x" << std::hex << (*cIter)
            //     << " .. second counter is 0x" << *(cIter+1)
            //     << " .. value saved in 32-bit word is 0x" << cValue
            //     << std::dec
            //     << RESET;
            fData.push_back(cValue);
        }
    }
}
bool D19cPSCounterFWInterface::WaitForNTriggers()
{
    // fTriggerInterface->ResetTriggerFSM();
    // make sure counters have been cleared and reset
    // not sure its needed but.. to be safe

    // PS_Open_shutter();
    // for(size_t cIndx=0; cIndx < fNEvents; cIndx++) PS_Inject();
    // PS_Close_shutter();
    // return true;

    // fData.clear();
    // auto cMultiplicity = fTheRegManager->ReadReg("fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    // fNEvents           = fNEvents * (cMultiplicity + 1);
    // fTriggerInterface->SetNTriggersToAccept(fNEvents);

    // // wait for trigger state machine to send all triggers
    auto cTriggerSource = this->fTheRegManager->ReadReg("fc7_daq_cnfg.fast_command_block.trigger_source"); // trigger source
    // LOG(DEBUG) << BOLDYELLOW << "D19cPSCounterFWInterface::WaitForData After resetting trigger FSM.. trigger source is " << cTriggerSource << RESET;
    LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::WaitForData After resetting trigger FSM.. trigger source is " << cTriggerSource << RESET;

    if(cTriggerSource == 10 || cTriggerSource == 12)
    {
        // LOG(DEBUG) << BOLDYELLOW << "D19cPSCounterFWInterface::WaitForData Running Trigger FSM ..." << RESET;
        LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::WaitForData Running Trigger FSM ..." << RESET;
        return fTriggerInterface->RunTriggerFSM();
    }
    else
    {
        LOG(INFO) << BOLDRED << "D19cPSCounterFWInterface::WaitForData  USING WRONG TRIGGER SOURCE FOR THIS TEST... " << cTriggerSource << RESET;
        return false; // wrong trigger source for this type of readout
    }
}
bool D19cPSCounterFWInterface::WaitForReadout()
{
    LOG(INFO) << BOLDRED << "D19cPSCounterFWInterface::WaitForData.. .no real data readout" << RESET;
    return false;
}
bool D19cPSCounterFWInterface::PollReadoutData(const Ph2_HwDescription::BeBoard* pBoard, bool pWait)
{
    LOG(INFO) << BOLDRED << "D19cPSCounterFWInterface::WaitForData.. .no real data readout" << RESET;
    return false;
}
bool D19cPSCounterFWInterface::ReadEvents(const BeBoard* pBoard)
{
    // Added For FastReadout
    // fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 0);
    // fFastCommandInterface->SendGlobalReSync();

    LOG(INFO) << BOLDGREEN << "D19cPSCounterFWInterface::Manually Enable FastReadout Mode" << RESET;
    SetPSCounterMode(1); // Enables Fast Readout Mode (Manually)
    //    SetPSCounterMode(0); //Disables Fast Readout Mode (Manually)
    if(fPSCounterFast)
    {
        fFastCommandInterface->SendGlobalReSync();
        std::cout << "FSM state = " << fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << std::endl;

        LOG(INFO) << BOLDGREEN << "Fast counter mode readout" << RESET;
        fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.clear_counters", 1);
        fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.open_shutter", 1);
        fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cal_pulse", 1);
        fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.close_shutter", 1);
        fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 1);
        fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.select_even", 1);
        fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.misc.initial_fast_reset_enable", 0);
        fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren", 1);
        fTheRegManager->WriteReg("fc7_daq_cnfg.sync_block.enable", 0);

        fTheRegManager->WriteReg("fc7_daq_cnfg.ddr3_debug.stub_enable", 0);
        fTheRegManager->WriteReg("fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 1);
        fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.trigger_source", 12);
        fTheRegManager->WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", 1);
        // fTheRegManager->WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en",0);

        //        LOG(DEBUG) << BOLDBLUE << "Reseting DDR3 " << RESET;
        LOG(INFO) << BOLDBLUE << "Reseting DDR3 " << RESET;
        auto cDDR3Calibrated = (fTheRegManager->ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
        while(!cDDR3Calibrated)
        {
            // LOG(DEBUG) << "Waiting for DDR3 to finish initial calibration";
            LOG(INFO) << BOLDYELLOW << "Waiting for DDR3 to finish initial calibration" << RESET;
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
            cDDR3Calibrated = (fTheRegManager->ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
        }
        LOG(INFO) << BOLDBLUE << "DDR3 Initial Calibration Complete " << RESET;
    }
    // Added For FastReadout (End)
    // clear data vector
    fData.clear();
    // make sure trigger mult is taken into account
    auto cMultiplicity = fTheRegManager->ReadReg("fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    fNEvents           = fNEvents * (cMultiplicity + 1);
    std::cout << "FSM state = " << fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << std::endl;

    fTriggerInterface->SetNTriggersToAccept(fNEvents);
    std::cout << "FSM state = " << fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << std::endl;

    // make sure handshake is configured
    fTheRegManager->WriteReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable", fHandshake);
    bool byrow   = false;
    bool bypixel = false;
    bool success = true;
    // fTriggerInterface->ResetTriggerFSM();
    // make sure counters have been cleared and reset
    // not sure its needed but.. to be safe

    // FIXME
    // LOG(INFO) << BOLDYELLOW << "Send Global Resync" << RESET;	//Debug Print Statement
    // PS_Close_shutter();
    // fFastCommandInterface->SendGlobalReSync();
    // PS_Clear_counters();
    // LOG(INFO) << BOLDYELLOW << "Global Resync Sent" << RESET;	//Debug Print Statement

    if(byrow or bypixel)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        ChipRegItem cReg_maskall;
                        cReg_maskall.fPage    = 0x00;
                        cReg_maskall.fAddress = 0x00;
                        cReg_maskall.fValue   = 0x00;

                        ChipRegItem cReg_unmaskall;
                        cReg_unmaskall.fPage    = 0x00;
                        cReg_unmaskall.fAddress = 0x801;
                        cReg_unmaskall.fValue   = 0x00;

                        fFEConfigurationInterface->SingleRead(cChip, cReg_unmaskall);

                        cReg_unmaskall.fAddress = 0x00;
                        cReg_maskall.fValue     = (cReg_unmaskall.fValue & 0x9E);

                        for(size_t cRow = 1; cRow < 17; cRow++)
                        {
                            if(byrow)
                            {
                                std::vector<ChipRegItem> cRegItems{cReg_maskall};

                                ChipRegItem cReg_unmaskrow;
                                cReg_unmaskrow.fPage    = 0x00;
                                cReg_unmaskrow.fAddress = (cRow << 11);
                                cReg_unmaskrow.fValue   = cReg_unmaskall.fValue;

                                // LOG(INFO) << BOLDRED << "NEW ROW " <<int(cRow)<< RESET;

                                cRegItems.push_back(cReg_unmaskrow);
                                fFEConfigurationInterface->MultiWrite(cChip, cRegItems);
                                WaitForNTriggers();

                                fFEConfigurationInterface->SingleWrite(cChip, cReg_unmaskall);
                            }
                            else if(bypixel)
                            {
                                for(size_t cCol = 1; cCol < 121; cCol++)
                                {
                                    // fReadoutChipInterface->maskPixel(0,0);
                                    // fReadoutChipInterface->maskRowCol(cRow,0,1);
                                    std::vector<ChipRegItem> cRegItems{cReg_maskall};

                                    ChipRegItem cReg_unmaskrow;
                                    cReg_unmaskrow.fPage    = 0x00;
                                    cReg_unmaskrow.fAddress = (cRow << 11) + cCol;
                                    cReg_unmaskrow.fValue   = cReg_unmaskall.fValue;

                                    // LOG(INFO) << BOLDRED << "NEW ROW " <<int(cRow)<< " NEW COL " <<int(cCol)<< RESET;
                                    // LOG(INFO) << BOLDRED << "ADDR " <<cReg_unmaskrow.fAddress<<" VAL "<<+cReg_unmaskall.fValue<< RESET;

                                    cRegItems.push_back(cReg_unmaskrow);
                                    fFEConfigurationInterface->MultiWrite(cChip, cRegItems);
                                    WaitForNTriggers();

                                    // LOG(INFO) << BOLDRED << "Done NEW TRIGs " << RESET;

                                    fFEConfigurationInterface->SingleWrite(cChip, cReg_unmaskall);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else
        //    	LOG(INFO) << BOLDRED << "WaitForNTriggers Commented Out" << RESET;	//Debug Print Statement
        if(!fPSCounterFast)
        {
            WaitForNTriggers();
            LOG(INFO) << BOLDRED << "WaitForNTriggers Done" << RESET; // Debug Print Statement
        }
    //    if(success)
    //    {
    //        LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::ReadEvents triggers succesfully sent" << RESET;
    //        GetCounterData(pBoard);

    //        FillData();
    //        LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::ReadEvents filled data vector with " << fData.size() << " 32-bit words" << RESET;
    //        return (fData.size() > 0);
    //    }
    if(success)
    {
        LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::ReadEvents triggers succesfully sent" << RESET;
        if(fPSCounterFast) // Added For FastReadout
        {
            LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::fPSCounterFast was set to True" << RESET;
            LOG(DEBUG) << BOLDYELLOW << "Running PSCounterFast" << RESET;
            fSuccessFastRead  = false;
            size_t cIteration = 0;
            do {
                std::cout << "FSM state = " << fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << std::endl;
                LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::Trying GetCounterData" << RESET;
                GetCounterData(pBoard);
                if(!fSuccessFastRead)
                {
                    LOG(INFO) << BOLDRED << "Fast read failed.. trying triggers again .. " << RESET;
                    std::this_thread::sleep_for(std::chrono::microseconds(1500));
                    WaitForNTriggers();
                }
                cIteration++;
            } while(!fSuccessFastRead);
            if(fSuccessFastRead) LOG(INFO) << BOLDGREEN << " Succesful read of fast counters after " << (cIteration) << " attempts." << RESET;
        }
        // Added For FastReadout (End)
        else
        {
            std::cout << "FSM state = " << fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << std::endl;
            GetCounterData(pBoard);
            std::cout << "FSM state = " << fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << std::endl;
        }
        FillData();
        LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::ReadEvents filled data vector with " << fData.size() << " 32-bit words" << RESET;
        return (fData.size() > 0);
    }
    else
        LOG(INFO) << BOLDRED << "D19cPSCounterFWInterface::ReadEvents did not receive all triggers..." << RESET;
    std::cout << "FSM state = " << fTheRegManager->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << std::endl;

    return false;
}
bool D19cPSCounterFWInterface::CheckStartPattern()
{
    std::string                                   cStartPattern = "111111111111111";
    size_t                                        cBxId         = 0;
    std::vector<std::pair<uint32_t, std::string>> cBxCars;
    auto                                          cStubBufferIter = fStubBuffer.begin();
    std::string                                   cStubPkt        = "";
    size_t                                        cPktLength      = 0;
    // std::vector<uint16_t> cCounters(0);
    size_t cStubCounter = 0;
    bool   cStartFound  = false;
    // find first packet with more than 0 stubs
    do {
        for(size_t cClk = 0; cClk < 8; cClk++)
        {
            // LOG(DEBUG) << BOLDMAGENTA << "Bx" << +cBxId << " : " << std::bitset<6>(*cStubBufferIter & 0x3F) << RESET;
            if(((*cStubBufferIter & 0x3F) >> 5) == 1 || cPktLength > 0) // configuration bit is 1
            {
                std::stringstream cStream;
                cStream << std::bitset<6>(*cStubBufferIter & 0x3F);
                cStubPkt += cStream.str();
                cPktLength += 6;
            }

            if(cPktLength == 6 * 8 * 8)
            {
                // std::pair<uint32_t,std::string> cBxCar;
                // cBxCar.first = *( cBxCounter.begin()  + std::distance( cStubBuffer.begin(), cStubBufferIter) ) ;
                // cBxCar.second = cStubPkt;
                std::vector<uint8_t>                         cSizes{1, 9, 12, 6}; // Cnfg, Status, BxId, Nstubs
                std::vector<std::pair<std::string, uint8_t>> cHdrFlds;
                cHdrFlds.push_back(std::make_pair("Cnfg", 1));
                cHdrFlds.push_back(std::make_pair("Status", 9));
                cHdrFlds.push_back(std::make_pair("BxId", 12));
                cHdrFlds.push_back(std::make_pair("Nstbs", 6));
                size_t                   cShft = 0;
                std::stringstream        cStream;
                std::vector<std::string> cHdrVals;
                for(auto cFld: cHdrFlds)
                {
                    auto cSubStr = cStubPkt.substr(cShft, cFld.second);
                    cHdrVals.push_back(cSubStr);
                    if(cFld.first == "BxId" || cFld.first == "Nstbs") { cStream << BOLDYELLOW << "\t" << cFld.first << "=" << std::stoi(cSubStr, 0, 2) << "\t"; }
                    else
                        cStream << BOLDYELLOW << "\t" << cFld.first << "=" << cSubStr << "\t";
                    cShft += cFld.second;
                }
                size_t cNstubs = std::stoi(cHdrVals[3], 0, 2);
                // LOG (INFO) << BOLDBLUE << cStream.str() << "\t" << cStubPkt.substr(0,cShft) << ":" << cStubPkt.substr(cShft, 8*21) << RESET;
                std::vector<uint32_t>                        cStubs(0);
                std::vector<std::pair<std::string, uint8_t>> cStubFlds;
                cStubFlds.push_back(std::make_pair("Offset", 3));
                cStubFlds.push_back(std::make_pair("HybridId", 3));
                cStubFlds.push_back(std::make_pair("Stub", 15));
                size_t cSizeAvailable = cStubPkt.length() - cShft;
                if(cSizeAvailable < (3 + 3 + 15) * cNstubs) continue;
                for(size_t cStubId = 0; cStubId < cNstubs; cStubId++)
                {
                    if(cStartFound) continue;
                    std::stringstream cStubOutput;
                    bool              cStartPatternFound = false;
                    for(auto cFld: cStubFlds)
                    {
                        auto cSubStr = cStubPkt.substr(cShft, cFld.second);
                        if(cFld.first != "Stub") { cStubOutput << BOLDBLUE << "\t" << cFld.first << "\t" << cSubStr << RESET; }
                        else
                        {
                            cStartPatternFound     = (cSubStr == cStartPattern); // first stub needs to be all 1's
                            uint16_t cCounterValue = std::stoi(cSubStr.substr(8, 6) + cSubStr.substr(0, 7), 0, 2) - 1;
                            cStubOutput << BOLDBLUE << "\t" << cFld.first << "\t" << cSubStr << " [ " << cCounterValue << " ] " << RESET;
                            // cCounters.push_back( cCounterValue );
                        }
                        cShft += cFld.second;
                    }
                    // if(cStartPatternFound)
                    //     LOG(DEBUG) << BOLDGREEN << "D19cPSCounterFWInterface::CheckStartPattern CheckForStartPattern from PS counters - Bx " << std::stoi(cHdrVals[2], 0, 2) << "\t stub#" <<
                    //     +cStubId
                    //                << " : " << cStubOutput.str() << RESET;
                    // else
                    //     LOG(DEBUG) << BOLDRED << "Bx " << std::stoi(cHdrVals[2], 0, 2) << "\t stub#" << +cStubId << " : " << cStubOutput.str() << RESET;
                    cStartFound = cStartPatternFound;
                    cStubCounter++;
                }
                cPktLength = 0;
                cStubPkt   = "";
                // cBxCars.push_back(cBxCar);
            }
            cStubBufferIter++;
        }
        cBxId++;
    } while(cStubBufferIter < fStubBuffer.end() && !cStartFound);
    return cStartFound;
}
} // namespace Ph2_HwInterface
