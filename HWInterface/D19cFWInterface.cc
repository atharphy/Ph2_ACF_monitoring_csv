/*!

        \file                           D19cFWInterface.h
        \brief                          D19cFWInterface init/config of the FC7 and its Chip's
        \author                         G. Auzinger, K. Uchida, M. Haranko
        \version            1.0
        \date                           24.03.2017
        Support :                       mail to : georg.auzinger@SPAMNOT.cern.ch
                                                  mykyta.haranko@SPAMNOT.cern.ch

 */

#include "D19cFWInterface.h"
#include "../HWDescription/Hybrid.h"
#include "../HWDescription/OuterTrackerHybrid.h"
#include "../Utils/D19cSSAEvent.h"
#include "D19cFpgaConfig.h"
#include "GbtInterface.h"
#include <algorithm>
#include <chrono>
#include <time.h>
#include <uhal/uhal.hpp>
// #pragma GCC diagnostic ignored "-Wpedantic"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cFWInterface::D19cFWInterface(const char* puHalConfigFileName, uint32_t pBoardId)
    : BeBoardFWInterface(puHalConfigFileName, pBoardId), fpgaConfig(nullptr), fBroadcastCbcId(0), fNReadoutChip(0), fNHybrids(0), fNCic(0), fFMCId(1)
{
    fResetAttempts = 0;
}

D19cFWInterface::D19cFWInterface(const char* puHalConfigFileName, uint32_t pBoardId, FileHandler* pFileHandler)
    : BeBoardFWInterface(puHalConfigFileName, pBoardId), fpgaConfig(nullptr), fFileHandler(pFileHandler), fBroadcastCbcId(0), fNReadoutChip(0), fNHybrids(0), fNCic(0), fFMCId(1)
{
    if(fFileHandler == nullptr)
        fSaveToFile = false;
    else
        fSaveToFile = true;
    fResetAttempts = 0;
}

D19cFWInterface::D19cFWInterface(const char* pId, const char* pUri, const char* pAddressTable)
    : BeBoardFWInterface(pId, pUri, pAddressTable), fpgaConfig(nullptr), fFileHandler(nullptr), fBroadcastCbcId(0), fNReadoutChip(0), fNHybrids(0), fNCic(0), fFMCId(1)
{
    fResetAttempts = 0;
}

D19cFWInterface::D19cFWInterface(const char* pId, const char* pUri, const char* pAddressTable, FileHandler* pFileHandler)
    : BeBoardFWInterface(pId, pUri, pAddressTable), fpgaConfig(nullptr), fFileHandler(pFileHandler), fBroadcastCbcId(0), fNReadoutChip(0), fNHybrids(0), fNCic(0), fFMCId(1)
{
    if(fFileHandler == nullptr)
        fSaveToFile = false;
    else
        fSaveToFile = true;
    fResetAttempts = 0;
}

void D19cFWInterface::setFileHandler(FileHandler* pHandler)
{
    if(pHandler != nullptr)
    {
        fFileHandler = pHandler;
        fSaveToFile  = true;
    }
    else
        LOG(INFO) << "Error, can not set NULL FileHandler";
}
void D19cFWInterface::ReadErrors()
{
    int error_counter = ReadReg("fc7_daq_stat.general.global_error.counter");

    if(error_counter == 0)
        LOG(INFO) << "No Errors detected";
    else
    {
        std::vector<uint32_t> pErrors = ReadBlockRegValue("fc7_daq_stat.general.global_error.full_error", error_counter);

        for(auto& cError: pErrors)
        {
            int error_block_id = (cError & 0x0000000f);
            int error_code     = ((cError & 0x00000ff0) >> 4);
            LOG(ERROR) << "Block: " << BOLDRED << error_block_id << RESET << ", Code: " << BOLDRED << error_code << RESET;
        }
    }
}

std::string D19cFWInterface::getFMCCardName(uint32_t pFMCcode)
{
    std::string name      = "";
    auto        cIterator = fFMCMap.find(pFMCcode);
    if(cIterator != fFMCMap.end())
        return cIterator->second;
    else
        return "UNKNOWN";
}

std::string D19cFWInterface::getChipName(uint32_t pChipCode)
{
    auto cIterator = fChipNamesMap.find(pChipCode);
    if(cIterator != fChipNamesMap.end())
        return cIterator->second;
    else
        return "UNKNOWN";
}

FrontEndType D19cFWInterface::getFrontEndType(uint32_t pChipCode)
{
    auto cIterator = fFETypesMap.find(pChipCode);
    if(cIterator != fFETypesMap.end())
        return cIterator->second;
    else
        return FrontEndType::UNDEFINED;
}

uint32_t D19cFWInterface::getBoardInfo()
{
    // firmware info
    LOG(INFO) << GREEN << "============================" << RESET;
    LOG(INFO) << BOLDGREEN << "General Firmware Info" << RESET;

    int      implementation = ReadReg("fc7_daq_stat.general.info.implementation");
    int      chip_code      = ReadReg("fc7_daq_stat.general.info.chip_type");
    int      num_hybrids    = ReadReg("fc7_daq_stat.general.info.num_hybrids");
    int      num_chips      = ReadReg("fc7_daq_stat.general.info.num_chips");
    uint32_t fmc1_card_type = ReadReg("fc7_daq_stat.general.info.fmc1_card_type");
    uint32_t fmc2_card_type = ReadReg("fc7_daq_stat.general.info.fmc2_card_type");

    // int firmware_timestamp = ReadReg("fc7_daq_stat.general.firmware_timestamp");
    // LOG(INFO) << "Compiled on: " << BOLDGREEN << ((firmware_timestamp >> 27) & 0x1F) << "." << ((firmware_timestamp
    // >> 23) & 0xF) << "." << ((firmware_timestamp >> 17) & 0x3F) << " " << ((firmware_timestamp >> 12) & 0x1F) << ":"
    // << ((firmware_timestamp >> 6) & 0x3F) << ":" << ((firmware_timestamp >> 0) & 0x3F) << " (dd.mm.yy hh:mm:ss)" <<
    // RESET;

    if(implementation == 0)
        LOG(INFO) << "Implementation: " << BOLDGREEN << "Optical" << RESET;
    else if(implementation == 1)
        LOG(INFO) << "Implementation: " << BOLDGREEN << "Electrical" << RESET;
    else if(implementation == 2)
        LOG(INFO) << "Implementation: " << BOLDGREEN << "CBC3 Emulation" << RESET;
    else
        LOG(WARNING) << "Implementation: " << BOLDRED << "Unknown" << RESET;

    LOG(INFO) << BOLDBLUE << "FMC1 Card: " << RESET << getFMCCardName(fmc1_card_type);
    LOG(INFO) << BOLDBLUE << "FMC2 Card: " << RESET << getFMCCardName(fmc2_card_type);

    LOG(INFO) << "Chip Type: " << BOLDGREEN << getChipName(chip_code) << RESET;
    LOG(INFO) << "Number of Hybrids: " << BOLDGREEN << num_hybrids << RESET;
    LOG(INFO) << "Number of Chips per Hybrid: " << BOLDGREEN << num_chips << RESET;

    // temporary used for board status printing
    LOG(INFO) << YELLOW << "============================" << RESET;
    LOG(INFO) << BOLDBLUE << "Current Status" << RESET;

    ReadErrors();

    int    source_id      = ReadReg("fc7_daq_stat.fast_command_block.general.source");
    double user_frequency = ReadReg("fc7_daq_cnfg.fast_command_block.user_trigger_frequency");

    if(source_id == 1)
        LOG(INFO) << "Trigger Source: " << BOLDGREEN << "L1-Trigger" << RESET;
    else if(source_id == 2)
        LOG(INFO) << "Trigger Source: " << BOLDGREEN << "Stubs" << RESET;
    else if(source_id == 3)
        LOG(INFO) << "Trigger Source: " << BOLDGREEN << "User Frequency (" << user_frequency << " kHz)" << RESET;
    else if(source_id == 4)
        LOG(INFO) << "Trigger Source: " << BOLDGREEN << "TLU" << RESET;
    else if(source_id == 5)
        LOG(INFO) << "Trigger Source: " << BOLDGREEN << "Ext Trigger (DIO5)" << RESET;
    else if(source_id == 6)
        LOG(INFO) << "Trigger Source: " << BOLDGREEN << "Test Pulse Trigger" << RESET;
    else
        LOG(WARNING) << " Trigger Source: " << BOLDRED << "Unknown" << RESET;

    int state_id = ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");

    if(state_id == 0)
        LOG(INFO) << "Trigger State: " << BOLDGREEN << "Idle" << RESET;
    else if(state_id == 1)
        LOG(INFO) << "Trigger State: " << BOLDGREEN << "Running" << RESET;
    else if(state_id == 2)
        LOG(INFO) << "Trigger State: " << BOLDGREEN << "Paused. Waiting for readout" << RESET;
    else
        LOG(WARNING) << " Trigger State: " << BOLDRED << "Unknown" << RESET;

    int i2c_replies_empty = ReadReg("fc7_daq_stat.command_processor_block.i2c.reply_fifo.empty");

    if(i2c_replies_empty == 0)
        LOG(INFO) << "I2C Replies Available: " << BOLDGREEN << "Yes" << RESET;
    else
        LOG(INFO) << "I2C Replies Available: " << BOLDGREEN << "No" << RESET;

    LOG(INFO) << YELLOW << "============================" << RESET;

    uint32_t cVersionWord = 0;
    return cVersionWord;
}
void D19cFWInterface::configureCDCE_old(uint16_t pClockRate)
{
    uint32_t cRegister;
    if(pClockRate == 120)
        cRegister = 0xEB040321;
    else if(pClockRate == 160)
        cRegister = 0xEB020321;
    else if(pClockRate == 240)
        cRegister = 0xEB840321;
    else // 320
        cRegister = 0xEB820321;
    // out0, out1 , out2, out3, out4 , reg5 , reg6, reg7, reg8
    // 0xeb840320 - reg0 (out0=240MHz,LVDS, phase shift  0deg)
    // 0xEB840302 - reg2 (out2=240MHz,LVDS)
    // 0xEB840303 - reg3 (out3=240MHz,LVDS)
    // 0xEB140334 - reg4 (out4= 40MHz,LVDS, R4.1=1, ph4adjc=0)
    // 0x113C0CF5 - reg5 (3.4ns lockw, LVDS in, DC term, PRIM REF enable, SEC REF enable, smartMUX off, failsafe off
    // etc.) 0x33041BE6 - reg6 (VCO1, PS=4, FD=12, FB=1, ChargePump 50uA, Internal Filter, R6.20=0, AuxOut= enable;
    // AuxOut= Out2) 0xBD800DF7 - reg7 (C2=473.5pF, R2=98.6kR, C1=0pF, C3=0pF, R3=5kR etc, SEL_DEL2=1, SEL_DEL1=1)
    // 0x20009978 - reg8 (various)};
    std::vector<uint32_t> cRegisterValues = {0xeb840320, cRegister, 0xEB840302, 0xEB840303, 0xEB140334, 0x113C0CF5, 0x33041BE6, 0xBD800DF7, 0x20009978};
    // std::vector<uint32_t> cRegisterValues = { 0xeb840320 ,cRegister , 0xEB840302, 0xeb840303, 0xeb140334, 0x013c0cb5,
    // 0x33041be6, 0xbd800df7 };
    for(auto cRegisterValue: cRegisterValues)
    {
        uint32_t cSPICommand = 0x8FA38014;
        this->WriteReg("sysreg.spi.tx_data", cRegisterValue);
        this->WriteReg("sysreg.spi.command", cSPICommand);
        uint32_t cReadBack = this->ReadReg("sysreg.spi.rx_data");
        cReadBack          = this->ReadReg("sysreg.spi.rx_data");
        LOG(DEBUG) << BOLDBLUE << "Dummy read from SPI returns : " << cReadBack << RESET;

        uint32_t cReadCommandCDCE = 0x8E;
        this->WriteReg("sysreg.spi.tx_data", cReadCommandCDCE);
        this->WriteReg("sysreg.spi.command", cSPICommand);
        // dummy write
        this->WriteReg("sysreg.spi.tx_data", 0xAAAAAAAA);
        this->WriteReg("sysreg.spi.command", cSPICommand);
        cReadBack = this->ReadReg("sysreg.spi.rx_data");
        LOG(INFO) << BOLDBLUE << "\t\tCDCE Read returns 0x" << std::hex << +cReadBack << std::dec << RESET;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // store in EEprom
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    epromCDCE();
}

void D19cFWInterface::configureCDCE(uint16_t pClockRate, std::pair<std::string, float> pCDCEselect)
{
    LOG(INFO) << BOLDBLUE << "...Configuring CDCE clock generator via SPI" << RESET;
    uint32_t              cSPIcommand  = 0x8fa38014; // command to SPI block
    std::vector<uint32_t> cWriteBuffer = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0};
    // New values from Mykyta
    // this clock is not used, but can be used as another gbt clock
    cWriteBuffer[0] = 0xEB040320; // reg0 (out0=120mhz,lvds, phase shift  0deg)
    // gbt clock reference
    if(pClockRate == 120)
    {
        LOG(INFO) << BOLDBLUE << "...\tSetting mgt ref clock to 120MHz" << RESET;
        cWriteBuffer[1] = 0xEB040321; // reg1 (out1=120mhz,lvds, phase shift  0deg)
    }
    else if(pClockRate == 320)
    {
        LOG(INFO) << BOLDBLUE << "...\tSetting mgt ref clock to 320MHz" << RESET;
        cWriteBuffer[1] = 0xEB820321; // reg1 (out1=320mhz,lvds, phase shift  0deg)
    }
    else
    {
        LOG(ERROR) << BOLDRED << "...\tIncorrect MGT clock." << RESET;
        throw std::runtime_error("Incorrect MGT clock");
    }
    // ddr3 clock reference
    cWriteBuffer[2] = 0xEB840302; // reg2 (out2=240mhz,lvds  phase shift  0deg) 0xEB840302
    // two not used outputs
    cWriteBuffer[3] = 0xEA860303; //# reg3 (off)
    cWriteBuffer[4] = 0xEB140334; //# reg4 (off)  0x00860314
    // selecting the reference
    if(pCDCEselect.first == "sec")
    {
        cWriteBuffer[5] = 0x10000EB5; // reg5
        this->WriteReg("sysreg.ctrl.cdce_refsel", 0);
        LOG(INFO) << BOLDBLUE << "...\tSetting SECONDARY reference" << RESET;
    }
    else if(pCDCEselect.first == "pri")
    {
        cWriteBuffer[5] = 0x10000E75; // reg5
        this->WriteReg("sysreg.ctrl.cdce_refsel", 1);
        LOG(INFO) << BOLDBLUE << "...\tSetting PRIMARY reference" << RESET;
    }
    else
    {
        LOG(ERROR) << BOLDRED << "...\tIncorrect REFERENCE ID." << RESET;
        throw std::runtime_error("Incorrect REFERENCE ID");
    }
    // selecting the vco
    if(pCDCEselect.second == 40)
    {
        cWriteBuffer[6] = 0x030E02E6; // reg6
        LOG(INFO) << BOLDBLUE << "...\tCDCE Ref is 40MHz, selecting VCO1" << RESET;
    }
    else if(pCDCEselect.second > 40)
    {
        cWriteBuffer[6] = 0x030E02F6; // reg6
        LOG(INFO) << BOLDBLUE << "...\tCDCE Ref > 40MHz, selecting VCO2" << RESET;
    }
    else
    {
        LOG(ERROR) << BOLDRED << "...\tUnknown CDCE ref rate" << RESET;
        throw std::runtime_error("Unknown CDCE ref rate");
    }
    // rc network parameters, dont touch
    cWriteBuffer[7] = 0xBD800DF7; // # reg7
    // sync command configuration
    cWriteBuffer[8] = 0x20009978;
    // cWriteBuffer[8] = 0x80001808;// # trigger sync

    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    for(auto cBufferValue: cWriteBuffer)
    {
        this->WriteReg("sysreg.spi.tx_data", cBufferValue);
        this->WriteReg("sysreg.spi.command", cSPIcommand);

        uint32_t cReadBack = this->ReadReg("sysreg.spi.rx_data");
        cReadBack          = this->ReadReg("sysreg.spi.rx_data");
        LOG(DEBUG) << BOLDBLUE << "Dummy read from SPI returns : " << cReadBack << RESET;
    }
    // store in EEprom
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    epromCDCE();
}
void D19cFWInterface::syncCDCE()
{
    LOG(INFO) << BOLDBLUE << "\tCDCE Synchronization" << RESET;
    LOG(INFO) << BOLDBLUE << "\t\tDe-Asserting Sync" << RESET;
    this->WriteReg("sysreg.ctrl.cdce_sync", 0);
    LOG(INFO) << "\t\tAsserting Sync" << RESET;
    this->WriteReg("sysreg.ctrl.cdce_sync", 1);
}
// void D19cFWInterface::syncCDCE()
// {
//     uint32_t cDisableSync = 0;
//     uint32_t cEnableSync  = 1 - cDisableSync;

//     LOG(INFO) << BOLDBLUE << "\tCDCE Synchronization" << RESET;
//     this->WriteReg("sysreg.ctrl.cdce_ctrl_sel", 1);
//     uint32_t cReadBack = this->ReadReg("sysreg.ctrl.cdce_ctrl_sel");
//     LOG(INFO) << BOLDBLUE << "Read from CDCE ctrl returns : " << cReadBack << RESET;

//     // de-assert sync
//     this->WriteReg("sysreg.ctrl.cdce_sync", cDisableSync);
//     // two dummy reads
//     this->ReadReg("sysreg.ctrl");
//     this->ReadReg("sysreg.ctrl");
//     cReadBack = this->ReadReg("sysreg.ctrl");
//     LOG(INFO) << BOLDBLUE << "\t\tCDCE Sync De-Asserted : 0x" << std::hex << +cReadBack << std::dec << RESET;

//     // 0 --> secondary reference (internal)
//     // 1 --> primary reference (external)
//     uint32_t cExternalClock = this->ReadReg("fc7_daq_cnfg.clock.ext_clk_en");
//     this->WriteReg("sysreg.ctrl.cdce_refsel", cExternalClock);
//     cReadBack = this->ReadReg("sysreg.ctrl.cdce_refsel");
//     do
//     {
//         std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
//         cReadBack = this->ReadReg("sysreg.ctrl.cdce_refsel");
//     } while(cReadBack != cExternalClock);
//     LOG(INFO) << BOLDBLUE << "Read from CDCE ref sel returns : " << cReadBack << RESET;

//     // assert sync
//     this->WriteReg("sysreg.ctrl.cdce_sync", cEnableSync);
//     // two dummy reads
//     this->ReadReg("sysreg.ctrl");
//     this->ReadReg("sysreg.ctrl");
//     cReadBack = this->ReadReg("sysreg.ctrl");
//     LOG(INFO) << BOLDBLUE << "\t\tAsserting Sync : 0x" << std::hex << +cReadBack << std::dec << RESET;

//     // check sync done
//     do
//     {
//         LOG(DEBUG) << BOLDBLUE << "Read from CDCE sync done returns : " << cReadBack << RESET;
//         std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
//         cReadBack = this->ReadReg("sysreg.status.cdce_sync_done");
//     } while(cReadBack != 1);
//     LOG(INFO) << BOLDBLUE << "Read from CDCE Sync Done returns : " << cReadBack << RESET;

//     //
//     uint32_t cReadCommandCDCE = 0x8E;
//     uint32_t cSPICommand      = 0x8FA38014;
//     this->WriteReg("sysreg.spi.tx_data", cReadCommandCDCE);
//     this->WriteReg("sysreg.spi.command", cSPICommand);
//     // dummy write
//     this->WriteReg("sysreg.spi.tx_data", 0xAAAAAAAA);
//     this->WriteReg("sysreg.spi.command", cSPICommand);
//     cReadBack = this->ReadReg("sysreg.spi.rx_data");
//     LOG(INFO) << BOLDBLUE << "\t\tCDCE Read returns 0x" << std::hex << +cReadBack << std::dec << RESET;

//     cReadBack = this->ReadReg("sysreg.status.cdce_lock");
//     LOG(INFO) << BOLDBLUE << "Read from CDCE Sync Lock returns : " << cReadBack << RESET;
//     this->WriteReg("sysreg.ctrl.cdce_ctrl_sel", 0);
// }

void D19cFWInterface::epromCDCE()
{
    LOG(INFO) << BOLDBLUE << "\tStoring Configuration in EEPROM" << RESET;
    uint32_t cSPIcommand               = 0x8FA38014; // command to spi block
    uint32_t cWrite_to_eeprom_unlocked = 0x0000001F; // # write eeprom

    this->WriteReg("sysreg.spi.tx_data", cWrite_to_eeprom_unlocked);
    this->WriteReg("sysreg.spi.command", cSPIcommand);
    uint32_t cReadBack = this->ReadReg("sysreg.spi.rx_data");
    cReadBack          = this->ReadReg("sysreg.spi.rx_data");
    LOG(DEBUG) << BOLDBLUE << "Dummy read from SPI returns : " << cReadBack << RESET;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}
void D19cFWInterface::powerAllFMCs(bool pEnable)
{
    this->WriteReg("sysreg.fmc_pwr.pg_c2m", (int)pEnable);
    this->WriteReg("sysreg.fmc_pwr.l12_pwr_en", (int)pEnable);
    this->WriteReg("sysreg.fmc_pwr.l8_pwr_en", (int)pEnable);
}
void D19cFWInterface::ResetLink(uint8_t pLinkId)
{
    LOG(INFO) << BOLDBLUE << "Resetting Link#" << +pLinkId << RESET;
    // reset here for good measure
    uint32_t cCommand = (0x0 << 22) | ((pLinkId & 0x3f) << 26);
    this->WriteReg("fc7_daq_ctrl.optical_block.general", cCommand);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // get link status
    cCommand = (0x1 << 22) | ((pLinkId & 0x3f) << 26);
    this->WriteReg("fc7_daq_ctrl.optical_block.general", cCommand);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}
bool D19cFWInterface::GetLinkStatus(uint8_t pLinkId)
{
    bool cGBTxLocked = true;
    // read back status register
    LOG(INFO) << BOLDBLUE << "GBT Link Status..." << RESET;
    uint32_t cLinkStatus = this->ReadReg("fc7_daq_stat.optical_block");
    LOG(INFO) << BOLDBLUE << "GBT Link" << +pLinkId << " status " << std::bitset<32>(cLinkStatus) << RESET;
    std::vector<std::string> cStates = {"GBT TX Ready", "MGT Ready", "GBT RX Ready"};
    uint8_t                  cIndex  = 1;
    for(auto cState: cStates)
    {
        uint8_t cStatus = (cLinkStatus >> (3 - cIndex)) & 0x1;
        cGBTxLocked &= (cStatus == 1);
        if(cStatus == 1)
            LOG(INFO) << BOLDBLUE << "\t... " << cState << BOLDGREEN << "\t : LOCKED" << RESET;
        else
            LOG(INFO) << BOLDBLUE << "\t... " << cState << BOLDRED << "\t : FAILED" << RESET;
        cIndex++;
    }
    return cGBTxLocked;
}
bool D19cFWInterface::LinkLock(const BeBoard* pBoard)
{
    std::lock_guard<std::mutex> theGuard(fMutex);
    // check links are up
    std::vector<std::string> cStates      = {"GBT TX Ready", "MGT Ready", "GBT RX Ready"};
    bool                     cLinksLocked = true;
    uint8_t                  cMaxAttempts = 3;
    uint8_t                  cAttempCount = 0;
    do
    {
        cLinksLocked = true;
        for(auto cOpticalReadout: *pBoard)
        {
            uint8_t cLinkId = cOpticalReadout->getId();
            if(cOpticalReadout->getReset() == 1)
                ResetLink(cLinkId);
            else
                LOG(INFO) << BOLDYELLOW << "\t... will not reset Link#" << cOpticalReadout->getId() << " on BeBoard#" << +pBoard->getId() << RESET;

            bool cLocked = GetLinkStatus(cLinkId);
            cLinksLocked = cLinksLocked && cLocked;
        }
        if(cLinksLocked)
        {
            LOG(INFO) << BOLDGREEN << "All links locked." << RESET;
            break;
        }
        else
        {
            LOG(DEBUG) << BOLDRED << "Resetting lpGBT link .. no lock" << RESET;
            // No .. because now you are potentially resetting ALL links not just this channel
            /*
            if(fPowerSupplyClient != nullptr)
            {
                LOG(INFO) << BOLDRED << "Powercycling the module using the Power supply TCP server ..." << RESET;
                if(fPowerSupplyClient->sendAndReceivePacket("TurnOff,PowerSupplyId:MyRohdeSchwarz,MyKeithley:Front") == "Error") throw std::runtime_error(std::string("HV channel did not turned Off"));
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                if(fPowerSupplyClient->sendAndReceivePacket("TurnOff,PowerSupplyId:MyRohdeSchwarz,ChannelId:LV_Module1") == "Error")
                    throw std::runtime_error(std::string("LV channel 1 did not turned Off"));
                if(fPowerSupplyClient->sendAndReceivePacket("TurnOff,PowerSupplyId:MyRohdeSchwarz,ChannelId:LV_Module2") == "Error")
                    throw std::runtime_error(std::string("LV channel 2 did not turned Off"));

                std::this_thread::sleep_for(std::chrono::milliseconds(1000));

                if(fPowerSupplyClient->sendAndReceivePacket("TurnOn,PowerSupplyId:MyRohdeSchwarz,ChannelId:LV_Module1") == "Error")
                    throw std::runtime_error(std::string("LV channel 1 did not turned On"));
                if(fPowerSupplyClient->sendAndReceivePacket("TurnOn,PowerSupplyId:MyRohdeSchwarz,ChannelId:LV_Module2") == "Error")
                    throw std::runtime_error(std::string("LV channel 2 did not turned On"));
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                if(fPowerSupplyClient->sendAndReceivePacket("TurnOn,PowerSupplyId:MyRohdeSchwarz,MyKeithley:Front") == "Error") throw std::runtime_error(std::string("HV channel did not turned On"));
            }*/
            // reset lpGBT core
            this->WriteReg("fc7_daq_ctrl.optical_block.general", 0x1);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            this->WriteReg("fc7_daq_ctrl.optical_block.general", 0x0);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        cAttempCount++;
    } while(!cLinksLocked && cAttempCount < cMaxAttempts);
    return cLinksLocked;
}

bool D19cFWInterface::GBTLock(const BeBoard* pBoard)
{
    std::lock_guard<std::mutex> theGuard(fMutex);
    // get link Ids
    std::vector<uint8_t> cLinkIds;
    for(auto cOpticalReadout: *pBoard)
    {
        if(std::find(cLinkIds.begin(), cLinkIds.end(), cOpticalReadout->getId()) == cLinkIds.end()) cLinkIds.push_back(cOpticalReadout->getId());
    }

    // switch off SEH
    if(fPowerSupplyClient == nullptr)
    {
        LOG(INFO) << BOLDRED << "Please switch off the SEH... press any key to continue once you have done so..." << RESET;
        do
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while(std::cin.get() != '\n');
    }
    else
    {
        LOG(INFO) << BOLDRED << "Switching off the LV using Power Supply Server..." << RESET;
        fPowerSupplyClient->sendAndReceivePacket("TurnOff,PowerSupplyId:MyTTi,ChannelId:LV_Module");
        // fPowerSupplyClient->sendAndReceivePacket("TurnOff,PowerSupplyId:MyRohdeSchwarz,ChannelId:LV_Module3");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    // system("/home/modtest/Programming/power_supply/bin/TurnOff -c /home/modtest/Programming/power_supply/config/config.xml ");
    // std::this_thread::sleep_for (std::chrono::milliseconds (1000) );
    // resync CDCE
    // this->syncCDCE();
    // reset GBT-FPGA
    this->WriteReg("fc7_daq_ctrl.optical_block.general", 0x1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // reset GBT-FPGA
    // this->WriteReg("fc7_daq_ctrl.optical_block.general", 0x1);
    // std::this_thread::sleep_for (std::chrono::milliseconds (500) );
    // this->WriteReg("fc7_daq_ctrl.optical_block.general", 0x0);
    // std::this_thread::sleep_for (std::chrono::milliseconds (500) );
    bool cLinksLocked = true;

    // tell user to switch on SEH
    if(fPowerSupplyClient == nullptr)
    {
        LOG(INFO) << BOLDRED << "Please switch on the SEH... press any key to continue once you have done so..." << RESET;
        do
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while(std::cin.get() != '\n');
    }
    else
    {
        LOG(INFO) << BOLDRED << "Switching on the LV using Power Supply Server..." << RESET;
        fPowerSupplyClient->sendAndReceivePacket("TurnOn,PowerSupplyId:MyTTi,ChannelId:LV_Module");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    // system("/home/modtest/Programming/power_supply/bin/TurnOn -c /home/modtest/Programming/power_supply/config/config.xml ");
    // std::this_thread::sleep_for (std::chrono::milliseconds (1000) );

    for(auto cLinkId: cLinkIds)
    {
        // reset here for good measure
        uint32_t cCommand = (0x0 << 22) | ((cLinkId & 0x3f) << 26);
        // this->WriteReg("fc7_daq_ctrl.optical_block.general", cCommand ) ;
        // std::this_thread::sleep_for (std::chrono::milliseconds (2000) );
        //  get link status
        cCommand = ((0x1 & 0xf) << 22) | ((cLinkId & 0x3f) << 26);
        this->WriteReg("fc7_daq_ctrl.optical_block.general", cCommand);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        LOG(INFO) << BOLDBLUE << "GBT Link Status..." << RESET;
        uint32_t cLinkStatus = this->ReadReg("fc7_daq_stat.optical_block");
        LOG(INFO) << BOLDBLUE << "GBT Link" << +cLinkId << " status " << std::bitset<32>(cLinkStatus) << RESET;
        std::vector<std::string> cStates     = {"GBT TX Ready", "MGT Ready", "GBT RX Ready"};
        uint8_t                  cIndex      = 1;
        bool                     cGBTxLocked = true;
        for(auto cState: cStates)
        {
            uint8_t cStatus = (cLinkStatus >> (3 - cIndex)) & 0x1;
            cGBTxLocked &= (cStatus == 1);
            if(cStatus == 1)
                LOG(INFO) << BOLDBLUE << "\t... " << cState << BOLDGREEN << "\t : LOCKED" << RESET;
            else
                LOG(INFO) << BOLDBLUE << "\t... " << cState << BOLDRED << "\t : FAILED" << RESET;
            cIndex++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        LOG(INFO) << BOLDMAGENTA << RESET;
        cLinksLocked = cLinksLocked && cGBTxLocked;
        this->WriteReg("fc7_daq_ctrl.optical_block.general", 0x00);
    }
    return cLinksLocked;
}

void D19cFWInterface::configureLink(const BeBoard* pBoard)
{
    std::vector<uint8_t> cLinkIds(0);
    for(auto cOpticalReadout: *pBoard)
    {
        if(std::find(cLinkIds.begin(), cLinkIds.end(), cOpticalReadout->getId()) == cLinkIds.end()) cLinkIds.push_back(cOpticalReadout->getId());
    }
    // now configure SCA + GBTx
    GbtInterface cGBTx;
    // enable sca
    // configure sca
    for(auto cLinkId: cLinkIds)
    {
        this->selectLink(cLinkId);
        LOG(INFO) << BOLDBLUE << "Configuring GBTx on link " << +cLinkId << RESET;
        // configure SCA
        cGBTx.scaConfigure(this);
        uint8_t cSCAenabled = cGBTx.scaEnable(this);
        if(cSCAenabled == 1) LOG(INFO) << BOLDBLUE << "SCA enabled successfully." << RESET;
        cGBTx.scaConfigureGPIO(this);
        // configure GBTx
        bool cRisingEdge = true;
        // cGBTx.gbtxResetPhaseShifterClocks(this);
        cGBTx.gbtxConfigureChargePumps(this);
        cGBTx.gbtxResetPhaseShifterClocks(this);
        cGBTx.gbtxSetClocks(this, 0x3, 0xa); // 0xa
        cGBTx.gbtxConfigure(this);
        cGBTx.gbtxSetPhase(this, fGBTphase);
        cGBTx.gbtxSelectEdgeTx(this, cRisingEdge);
        cGBTx.gbtxSelectTerminationRx(this, false);
        cGBTx.gbtxSetDriveStrength(this, 0xa);
    }
}
std::pair<uint16_t, float> D19cFWInterface::readADC(std::string pValueToRead, bool pApplyCorrection)
{
    std::pair<uint16_t, float> cADCreading;
    GbtInterface               cGBTx;
    cADCreading.first  = cGBTx.readAdcChn(this, pValueToRead, pApplyCorrection);
    cADCreading.second = cGBTx.convertAdcReading(cADCreading.first, pValueToRead);
    return cADCreading;
}
void D19cFWInterface::selectLink(const uint8_t pLinkId, uint32_t cWait_ms)
{
    if(fOptical)
    {
        // LOG (INFO) << BOLDBLUE << "Selecting link mux " << +pLinkId << RESET;
        this->WriteReg("fc7_daq_cnfg.optical_block.mux", pLinkId);
        // std::this_thread::sleep_for(std::chrono::microseconds(fWait_us*10));
        this->WriteReg("fc7_daq_ctrl.optical_block.sca.reset", 0x1);
        // std::this_thread::sleep_for (std::chrono::microseconds (fWait_us*10) );
    }
}

void D19cFWInterface::ConfigureBoard(const BeBoard* pBoard)
{
    // unique link Ids
    std::vector<uint8_t> cLinkIds(0);
    for(auto cOpticalReadout: *pBoard)
    {
        if(std::find(cLinkIds.begin(), cLinkIds.end(), cOpticalReadout->getId()) == cLinkIds.end()) cLinkIds.push_back(cOpticalReadout->getId());
    }

    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    // this is where I should get all the clocking and FastCommandInterface settings
    BeBoardRegMap                                 cRegMap = pBoard->getBeBoardRegMap();
    std::bitset<12>                               cL8Enable(0);
    std::bitset<12>                               c12Enable(0);
    bool                                          cEnableDIO5 = false;
    std::vector<std::pair<std::string, uint32_t>> cBoardRegs;
    for(auto const& it: cRegMap)
    {
        cBoardRegs.push_back({it.first, it.second});
        if(it.first == "fc7_daq_cnfg.dio5_block.dio5_en") cEnableDIO5 = (bool)it.second;
        if(it.first == "fc7_daq_cnfg.optical_block.enable.l8") { cL8Enable = std::bitset<12>(it.second); }
        if(it.first == "fc7_daq_cnfg.optical_block.enable.l12") { c12Enable = std::bitset<12>(it.second); }
        if(it.first == "fc7_daq_cnfg.readout_block.global.zero_suppression_enable") { cBoardRegs.push_back({it.first, pBoard->getEventType() == EventType::ZS}); }
    }
    // configure CDCE - if needed
    std::pair<std::string, float> cCDCEselect;
    bool                          cSecondaryReference = false;
    for(auto const& it: cRegMap)
    {
        if(it.first == "fc7_daq_cnfg.clock.ext_clk_en") cSecondaryReference = cSecondaryReference | (it.second == 0);
        if(it.first == "fc7_daq_cnfg.ttc.ttc_enable") cSecondaryReference = cSecondaryReference | (it.second == 1);
    }
    LOG(INFO) << BOLDBLUE << "External clock " << ((cSecondaryReference) ? "Disabled" : "Enabled") << RESET;
    if(cSecondaryReference)
    {
        cCDCEselect.first  = "sec";
        cCDCEselect.second = 40;
    }
    else
    {
        cCDCEselect.first  = "pri";
        cCDCEselect.second = 40.;
    }
    auto cCDCEconfig = pBoard->configCDCE();
    if(cCDCEconfig.first)
    {
        // configureCDCE_old(cCDCEconfig.second);
        configureCDCE(cCDCEconfig.second, cCDCEselect);
        // sync CDCE
        syncCDCE();
    }

    // reset FC7 if not mux crate
    uint32_t fmc1_card_type = ReadReg("fc7_daq_stat.general.info.fmc1_card_type");
    uint32_t fmc2_card_type = ReadReg("fc7_daq_stat.general.info.fmc2_card_type");

    std::string cFMC1name = fFMCMap[fmc1_card_type];
    std::string cFMC2name = fFMCMap[fmc2_card_type];
    bool        cWithDIO5 = (cFMC1name == "DIO5" || cFMC2name == "DIO5"); // DIO5 in either slot

    LOG(INFO) << BOLDBLUE << "FMC1 Card: " << RESET << getFMCCardName(fmc1_card_type);
    LOG(INFO) << BOLDBLUE << "FMC2 Card: " << RESET << getFMCCardName(fmc2_card_type);
    if(pBoard->getReset() == 1)
    {
        if(getFMCCardName(fmc1_card_type) != "2S_FMC1" && getFMCCardName(fmc1_card_type) != "PS_FMC1")
        {
            if(getFMCCardName(fmc1_card_type) != "FMC_FE_FOR_PS_ROH_FMC1")
            {
                LOG(INFO) << BOLDBLUE << "Sending a global reset to the FC7 ..... " << RESET;
                WriteReg("fc7_daq_ctrl.command_processor_block.global.reset", 0x1);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }
    else
        LOG(INFO) << BOLDYELLOW << "Not sending a reset to BeBoard#" << +pBoard->getId() << RESET;

    // power on FMCs
    this->InitFMCPower();

    // configure FC7 after the fast reset
    LOG(INFO) << BOLDBLUE << "Configuring FC7..." << RESET;
    this->WriteStackReg(cBoardRegs);
    cBoardRegs.clear();
    // load dio5 configuration
    if(cEnableDIO5 && cWithDIO5)
    {
        LOG(INFO) << BOLDBLUE << "Loading DIO5 configuration.." << RESET;
        this->WriteReg("fc7_daq_ctrl.dio5_block.control.load_config", 0x1);
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
        auto cStatus = this->ReadReg("fc7_daq_stat.dio5_block.status.not_ready");
        auto cError  = this->ReadReg("fc7_daq_stat.dio5_block.status.error");
        LOG(INFO) << BOLDBLUE << "DIO5 status [not ready] : " << +cStatus << RESET;
        LOG(INFO) << BOLDBLUE << "DIO5 status [error] : " << +cError << RESET;
    }
    else if(cEnableDIO5)
    {
        LOG(INFO) << BOLDRED << "DID NOT ENABLE DIO5.. FW not configured for that option" << RESET;
        throw std::runtime_error(std::string("Trying to enable DIO5 when firmware isn't configured for that mezzanine!"));
    }

    // // set reference for CDCE
    // uint32_t cExternalClock = 0; // this->ReadReg("fc7_daq_cnfg.clock.ext_clk_en");
    // this->WriteReg("sysreg.ctrl.cdce_ctrl_sel", 1);
    // this->WriteReg("sysreg.ctrl.cdce_refsel", cExternalClock);
    // this->WriteReg("sysreg.ctrl.cdce_ctrl_sel", 0);
    // this->syncCDCE();

    // this->WriteReg("fc7_daq_cnfg.clock.ext_clk_en", 1);
    // this->WriteReg("clock_source_u8", 3);

    // check status of clocks
    bool cCheckLock = false;
    if(cCheckLock)
    {
        bool c40MhzLocked    = false;
        bool cRefClockLocked = false;
        int  cLockAttempts   = 0;
        while(cLockAttempts < 10)
        {
            c40MhzLocked = this->ReadReg("fc7_daq_stat.general.clock_generator.clk_40_locked") == 1;
            if(c40MhzLocked)
                LOG(INFO) << BOLDBLUE << "40 MHz clock in FC7 " << BOLDGREEN << " LOCKED!" << RESET;
            else
                LOG(INFO) << BOLDBLUE << "40 MHz clock in FC7 " << BOLDRED << " FAILED TO LOCK!" << RESET;

            cRefClockLocked = this->ReadReg("fc7_daq_stat.general.clock_generator.ref_clk_locked") == 1;
            if(cRefClockLocked)
                LOG(INFO) << BOLDBLUE << "Ref clock in FC7 " << BOLDGREEN << " LOCKED!" << RESET;
            else
                LOG(INFO) << BOLDBLUE << "Ref clock in FC7 " << BOLDRED << " FAILED TO LOCK!" << RESET;

            if(c40MhzLocked && cRefClockLocked) break;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            cLockAttempts++;
        };
        if(!c40MhzLocked || !cRefClockLocked)
        {
            LOG(ERROR) << BOLDRED << "One of the clocks failed to LOCK!" << RESET;
            exit(0);
        }
    }
    this->syncCDCE();

    // read info about current firmware
    uint32_t cFrontEndTypeCode = ReadReg("fc7_daq_stat.general.info.chip_type");
    LOG(INFO) << BOLDBLUE << "Front-end type code from firmware register : " << +cFrontEndTypeCode << RESET;
    std::string cChipName = getChipName(cFrontEndTypeCode);
    fFirmwareFrontEndType = getFrontEndType(cFrontEndTypeCode);
    fFWNHybrids           = ReadReg("fc7_daq_stat.general.info.num_hybrids");
    fFWNChips             = ReadReg("fc7_daq_stat.general.info.num_chips");
    fCBC3Emulator         = (ReadReg("fc7_daq_stat.general.info.implementation") == 2);
    fIsDDR3Readout        = (ReadReg("fc7_daq_stat.ddr3_block.is_ddr3_type") == 1);
    if(fIsDDR3Readout == 1) LOG(INFO) << BOLDBLUE << "DD3 Readout .... " << RESET;
    fI2CVersion = (ReadReg("fc7_daq_stat.command_processor_block.i2c.master_version"));
    fOptical    = pBoard->isOptical();
    fIs2S       = false;

    // LOG (INFO) << BOLDMAGENTA << "Disabling SFPS..." << RESET;
    // WriteReg("fc7_daq_cnfg.optical_block.enable.l8", 0xFF);
    // WriteReg("fc7_daq_cnfg.optical_block.enable.l12", 0x00);
    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // LOG (INFO) << BOLDMAGENTA << "Enabling SFPS..." << RESET;
    // WriteReg("fc7_daq_cnfg.optical_block.enable.l8", 0x00);
    // WriteReg("fc7_daq_cnfg.optical_block.enable.l12", 0xFF);

    bool cWithlpGBT = false;
    for(auto cOpticalGroup: *pBoard)
    {
        auto& clpGBT = cOpticalGroup->flpGBT;
        if(clpGBT != nullptr) cWithlpGBT = true;

        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid) { fIs2S = fIs2S || (cChip->getFrontEndType() == FrontEndType::CBC3); }
        }
    }
    if(cWithlpGBT) LOG(INFO) << BOLDBLUE << "D19cFWInterface::ConfigureBoard with lpGBT" << RESET;
    if(pBoard->isOptical()) LOG(INFO) << BOLDBLUE << "D19cFWInterface::ConfigureBoard for optical readout" << RESET;
    fOptical = pBoard->isOptical() && !cWithlpGBT;
    // fUseOpticalLink = pBoard->isOptical() && !fOptical;
    bool cWithGBTx = false;
    // if optical readout .. then configure links
    if(pBoard->isOptical() && !cWithlpGBT)
    {
        cWithGBTx = true;
        LOG(INFO) << BOLDBLUE << "Configuring optical link with GBTx" << RESET;
        bool cGBTlock = GBTLock(pBoard);
        if(!cGBTlock)
        {
            LOG(INFO) << BOLDRED << "GBT link failed to LOCK!" << RESET;
            exit(0);
        }
        // now configure SCA + GBTx
        configureLink(pBoard);
    }
    if(pBoard->isOptical() && cWithlpGBT)
    {
        bool cSkip = (pBoard->getLinkReset() == 0);
        if(!cSkip)
        {
            LOG(INFO) << BOLDMAGENTA << "Resetting lpGBT-FPGA core on BeBoard#" << +pBoard->getId() << RESET;
            // reset lpGBT core
            this->WriteReg("fc7_daq_ctrl.optical_block.general", 0x1);
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            this->WriteReg("fc7_daq_ctrl.optical_block.general", 0x0);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            bool clpGBTlock = LinkLock(pBoard);
            if(!clpGBTlock)
            {
                LOG(INFO) << BOLDRED << "lpGBT link failed to LOCK!" << RESET;
                exit(0);
            }
            // ResetOptoLink();
            ResetCPB();
        }
        else
        {
            LOG(INFO) << BOLDMAGENTA << "Skipping lpGBT link reset.." << RESET;
            bool cLinksLocked = true;
            for(auto cOpticalReadout: *pBoard)
            {
                uint8_t cLinkId = cOpticalReadout->getId();
                bool    cLocked = GetLinkStatus(cLinkId);
                cLinksLocked    = cLinksLocked && cLocked;
            }
        }
    }

    if((fI2CVersion >= 1 || cWithGBTx) && !cWithlpGBT)
    {
        fI2CSlaveMap.clear();
        fSlaveMap.clear();
        // assuming only one type of CIC per board ...
        for(auto cModule: *pBoard)
        {
            // default I2C map is for 8CBC3
            for(auto cFe: *cModule)
            {
                auto    cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cFe);
                auto&   cCic                = cOuterTrackerHybrid->fCic;
                uint8_t cBaseAddress;
                uint8_t cNBytes;
                std::cout << cCic << std::endl;
                if(cCic != NULL)
                {
                    for(auto cChip: *cFe)
                    {
                        // auto cReadoutChip = static_cast<ReadoutChip*>( cChip);
                        cBaseAddress = 0x41;
                        if(cChip->getFrontEndType() == FrontEndType::SSA) cBaseAddress = 0x20;
                        if(cChip->getFrontEndType() == FrontEndType::SSA2) cBaseAddress = 0x20;
                        if(cChip->getFrontEndType() == FrontEndType::MPA) cBaseAddress = 0x40;

                        cBaseAddress += cChip->getId() % 8;
                        cNBytes            = (cChip->getFrontEndType() == FrontEndType::SSA2 || cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::MPA) ? 2 : 1;
                        uint8_t cLastValue = 1;
                        if(fI2CSlaveMap.find(cChip->getId()) == fI2CSlaveMap.end())
                        {
                            std::vector<uint32_t> cOldI2CSlaveDescription = {cBaseAddress, cNBytes, 1, 1, 1, cLastValue, (uint32_t)(cChip->getId() % 8)};
                            std::vector<uint32_t> cI2CSlaveDescription    = {cBaseAddress, cNBytes, 1, 1, 1, cLastValue};

                            LOG(INFO) << BOLDBLUE << "Adding chip with address " << +cChip->getId() << " to I2C slave map.." << RESET;
                            fI2CSlaveMap[cChip->getId()] = cI2CSlaveDescription;
                            fSlaveMap.push_back(cOldI2CSlaveDescription);
                        }
                    } // chips
                    cBaseAddress                                  = 0x60;
                    cNBytes                                       = 2;
                    std::vector<uint32_t> cOldI2CSlaveDescription = {cBaseAddress, cNBytes, 1, 1, 1, 1, cCic->getId()};
                    std::vector<uint32_t> cI2CSlaveDescription    = {cBaseAddress, cNBytes, 1, 1, 1, 1};
                    fI2CSlaveMap[cCic->getId()]                   = cI2CSlaveDescription;
                    fSlaveMap.push_back(cOldI2CSlaveDescription);
                    LOG(INFO) << BOLDBLUE << "Adding chip with address " << +cCic->getId() << " to I2C slave map.." << RESET;
                }
                else
                {
                    for(auto cChip: *cFe)
                    {
                        cBaseAddress = 0x41;
                        if(cChip->getFrontEndType() == FrontEndType::SSA) cBaseAddress = 0x20;
                        if(cChip->getFrontEndType() == FrontEndType::SSA2) cBaseAddress = 0x20;
                        if(cChip->getFrontEndType() == FrontEndType::MPA) cBaseAddress = 0x40;
                        cBaseAddress += cChip->getId() % 8;
                        cNBytes            = (cChip->getFrontEndType() == FrontEndType::SSA2 || cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::MPA) ? 2 : 1;
                        uint8_t cLastValue = 1;
                        LOG(INFO) << BOLDBLUE << "Adding slave with I2C address 0x" << std::hex << +cBaseAddress << std::dec << RESET;

                        std::vector<uint32_t> cOldI2CSlaveDescription = {cBaseAddress, cNBytes, 1, 1, 1, cLastValue, (uint32_t)(cChip->getId() % 8)};
                        std::vector<uint32_t> cI2CSlaveDescription    = {cBaseAddress, cNBytes, 1, 1, 1, cLastValue};
                        fI2CSlaveMap[cChip->getId()]                  = cI2CSlaveDescription;
                        fSlaveMap.push_back(cOldI2CSlaveDescription);
                    } // chips
                }
            } // hybrids
        }     // modules
        // and then loop over map and write
        for(auto cIterator = fI2CSlaveMap.begin(); cIterator != fI2CSlaveMap.end(); cIterator++)
        {
            // auto cChipId = cIterator->first;
            auto cDescription = cIterator->second;
            // setting the params
            uint32_t shifted_i2c_address             = (cDescription[0]) << 25;
            uint32_t shifted_register_address_nbytes = cDescription[1] << 10;
            uint32_t shifted_data_wr_nbytes          = cDescription[2] << 5;
            uint32_t shifted_data_rd_nbytes          = cDescription[3] << 0;
            uint32_t shifted_stop_for_rd_en          = cDescription[4] << 24;
            uint32_t shifted_nack_en                 = cDescription[5] << 23;

            // writing the item to the firmware
            if(fFirmwareFrontEndType != FrontEndType::CBC3)
            {
                uint32_t    final_item = shifted_i2c_address + shifted_register_address_nbytes + shifted_data_wr_nbytes + shifted_data_rd_nbytes + shifted_stop_for_rd_en + shifted_nack_en;
                std::string curreg     = "fc7_daq_cnfg.command_processor_block.i2c_address_table.slave_" + std::to_string(std::distance(fI2CSlaveMap.begin(), cIterator)) + "_config";
                LOG(INFO) << BOLDMAGENTA << "Writing " << std::bitset<32>(final_item) << " to register " << curreg << RESET;
                this->WriteReg(curreg, final_item);
            }
        }
    }

    // resetting hard
    if(fFirmwareFrontEndType == FrontEndType::CIC || fFirmwareFrontEndType == FrontEndType::CIC2)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            if(pBoard->isOptical()) this->selectLink(cOpticalGroup->getId());
            this->ChipReset();
        }
    }
    else
    {
        this->ReadoutChipReset();
    }

    // modifying FC7 configuration based on CIC
    cVecReg.clear();
    if(fFirmwareFrontEndType == FrontEndType::CIC || fFirmwareFrontEndType == FrontEndType::CIC2)
    {
        // assuming only one type of CIC per board ...
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                auto  cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cFe);
                auto& cCic                = cOuterTrackerHybrid->fCic;
                if(cCic == nullptr) continue;
                std::vector<std::pair<std::string, uint32_t>> cVecReg;
                // make sure CIC is receiving clock
                // cVecReg.push_back( {"fc7_daq_cnfg.physical_interface_block.cic.clock_enable" , 1 } ) ;
                // disable stub debug
                cVecReg.push_back({"fc7_daq_cnfg.ddr3_debug.stub_enable", 0});
                std::string cFwRegName = "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable";
                std::string cRegName   = (cCic->getFrontEndType() == FrontEndType::CIC) ? "CBC_SPARSIFICATION_SEL" : "FE_CONFIG";
                ChipRegItem cRegItem   = static_cast<OuterTrackerHybrid*>(pBoard->at(0)->at(0))->fCic->getRegItem(cRegName);
                uint8_t     cRegValue  = (cCic->getFrontEndType() == FrontEndType::CIC) ? cRegItem.fValue : (cRegItem.fValue & 0x10) >> 4;
                LOG(INFO) << BOLDBLUE << "Sparsification set to " << +cRegValue << RESET;
                cVecReg.push_back({cFwRegName, (cCic->getFrontEndType() == FrontEndType::CIC) ? cRegItem.fValue : (cRegItem.fValue & 0x10) >> 4});
                for(auto cReg: cVecReg) LOG(INFO) << BOLDBLUE << "Setting firmware register " << cReg.first << " to " << +cReg.second << RESET;
                this->WriteStackReg(cVecReg);
                cVecReg.clear();
            }
        }
    }
    else
    {
        LOG(INFO) << BOLDBLUE << "Firmware NOT configured for a CIC" << RESET;
    }

    if(pBoard->isOptical() && cWithGBTx)
    {
        // read voltages on hybrid using ADC
        for(auto cLinkId: cLinkIds)
        {
            this->selectLink(cLinkId);
            LOG(INFO) << BOLDBLUE << "Reading monitoring values on SEH connected to link " << +cLinkId << RESET;
            std::pair<uint16_t, float> cVM1V5               = this->readADC("VM1V5", false);
            std::pair<uint16_t, float> cVM2V5               = this->readADC("VM2V5", false);
            std::pair<uint16_t, float> cVMIN                = this->readADC("VMIN", false);
            std::pair<uint16_t, float> cInternalTemperature = this->readADC("INT_TEMP", false);
            LOG(INFO) << BOLDBLUE << "\t.... Reading 1.5V monitor on SEH  : " << cVM1V5.second << " V." << RESET;
            LOG(INFO) << BOLDBLUE << "\t.... Reading 2.5V monitor on SEH : " << cVM2V5.second << " V." << RESET;
            LOG(INFO) << BOLDBLUE << "\t.... Reading Vmin monitor on SEH : " << cVMIN.second << " V." << RESET;
            LOG(INFO) << BOLDBLUE << "\t.... Reading internal temperature monitor on SEH : " << cInternalTemperature.second << " [ raw reading is " << cInternalTemperature.second << "]." << RESET;
        }
    }

    // now check that I2C communication is functioning
    LOG(INFO) << BOLDGREEN << "According to the Firmware status registers, it was compiled for: " << fFWNHybrids << " hybrid(s), " << fFWNChips << " " << cChipName << " chip(s) per hybrid" << RESET;
    this->CheckChipControl(pBoard);

    // adding an ReSync to align CBC L1A counters
    this->ChipReSync();

    // load trigger configuration
    this->WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);
    this->ResetReadout();
    // reset trigger
    this->WriteReg("fc7_daq_ctrl.fast_command_block.control.reset", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // // reset readout
    // ResetReadout();
}
void D19cFWInterface::CheckChipControl(const BeBoard* pBoard)
{
    fNReadoutChip                                               = 0;
    fNHybrids                                                   = 0;
    uint16_t                                      hybrid_enable = 0;
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.clear();
    bool cWithlpGBT = false;
    for(auto cOpticalGroup: *pBoard)
    {
        // check if there is an lpGBT
        if(cOpticalGroup->flpGBT != NULL) cWithlpGBT = true;
        for(auto cHybrid: *cOpticalGroup)
        {
            auto&                 cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            std::vector<uint32_t> cVec;
            std::vector<uint32_t> cReplies;
            uint8_t               cChipsEnable = 0x00;

            // if there is an lpGBT .. just enable everything
            if(cWithlpGBT)
            {
                fNCic += 1;
                hybrid_enable |= 1 << cHybrid->getId();
                fNHybrids++;
                cChipsEnable = 0xFF;
            }

            LOG(INFO) << BOLDBLUE << "Enabling FE hybrid : " << +cHybrid->getId() << " - link Id " << +cOpticalGroup->getId() << RESET;
            for(auto cChip: *cHybrid)
            {
                if(cWithlpGBT) continue;

                auto cReadoutChip = static_cast<ReadoutChip*>(cChip);
                LOG(INFO) << BOLDMAGENTA << "Trying to perform an I2C read to chip#" << +cReadoutChip->getId() << " on FE" << +cHybrid->getId() << RESET;
                cVec.clear();
                cReplies.clear();
                // find first non-zero register in the map
                size_t cIndex       = 0;
                auto   cRegisterMap = cReadoutChip->getRegMap();
                auto   cIterator    = cRegisterMap.begin();
                bool   cZeroDefVal  = ((*cIterator).second.fValue == 0);
                do
                {
                    cIndex++;
                    cIterator++;
                    cZeroDefVal = ((*cIterator).second.fValue == 0);
                } while(cZeroDefVal && cIndex < cRegisterMap.size());

                ChipRegItem cRegItem      = cReadoutChip->getRegItem((*cIterator).first);
                auto        cValueFromMap = cRegItem.fValue;
                bool        cWrite        = false;
                this->EncodeReg(cRegItem, cHybrid->getId(), cReadoutChip->getId(), cVec, true, cWrite);
                this->ReadChipBlockReg(cVec);
                uint8_t cChipId;
                bool    cRead   = false;
                bool    cFailed = false;
                this->DecodeReg(cRegItem, cChipId, cVec[0], cRead, cFailed);
                if(cRead && !cFailed && cValueFromMap == cRegItem.fValue)
                {
                    LOG(INFO) << BOLDGREEN << "Successful read from " << (*cIterator).first << " [first non-zero I2C register of readout chip] on hybrid " << +cHybrid->getId()
                              << " default value in map is 0x" << std::hex << +cValueFromMap << std::dec << " value read back from chip is 0x" << std::hex << +cRegItem.fValue << std::dec
                              << " .... Enabling chip " << +cReadoutChip->getId() << RESET;
                    cChipsEnable |= (1 << cReadoutChip->getId());
                    fNReadoutChip++;
                }
            }
            char name[50];
            std::sprintf(name, "fc7_daq_cnfg.global.chips_enable_hyb_%02d", cHybrid->getId());
            std::string name_str(name);
            cVecReg.push_back({name_str, cChipsEnable});
            LOG(INFO) << BOLDBLUE << "Setting chips enable register on hybrid" << +cHybrid->getId() << " to " << std::bitset<32>(cChipsEnable) << RESET;

            cVec.clear();
            cReplies.clear();

            if(cWithlpGBT) continue;

            // if(fFirmwareFrontEndType == FrontEndType::CIC || fFirmwareFrontEndType == FrontEndType::CIC2)
            // {
            //     LOG(INFO) << BOLDBLUE << "CIC " << +cCic->getId() << " on FE" << +cHybrid->getId() << RESET;
            //     size_t cIndex       = 0;
            //     auto   cRegisterMap = cCic->getRegMap();
            //     auto   cIterator    = cRegisterMap.begin();
            //     bool   cZeroDefVal  = ((*cIterator).second.fValue == 0);
            //     do
            //     {
            //         cIndex++;
            //         cIterator++;
            //         cZeroDefVal = ((*cIterator).second.fValue == 0);
            //     } while(cZeroDefVal && cIndex < cRegisterMap.size());

            //     ChipRegItem cRegItem      = cCic->getRegItem((*cIterator).first);
            //     auto        cValueFromMap = cRegItem.fValue;
            //     bool        cWrite        = false;
            //     this->EncodeReg(cRegItem, cHybrid->getId(), cCic->getId(), cVec, true, cWrite);
            //     this->ReadChipBlockReg(cVec);
            //     uint8_t cChipId;
            //     bool    cRead   = false;
            //     bool    cFailed = false;
            //     this->DecodeReg(cRegItem, cChipId, cVec[0], cRead, cFailed);
            //     if(cRead && !cFailed)
            //     {
            //         if(cValueFromMap == cRegItem.fValue)
            //         {
            //             LOG(INFO) << BOLDGREEN << "Successful read from " << (*cIterator).first << " [first non-zero I2C register of readout chip] on hybrid " << +cHybrid->getId()
            //                       << " default value in map is 0x" << std::hex << +cValueFromMap << std::dec << " value read back from chip is 0x" << std::hex << +cRegItem.fValue << std::dec
            //                       << " .... Enabling CIC " << +cHybrid->getId() << RESET;
            //             hybrid_enable |= 1 << cHybrid->getId();
            //             fNCic++;
            //         }
            //         else
            //             LOG(INFO) << BOLDRED << "Failed to read from " << (*cIterator).first << " [first non-zero I2C register of readout chip] on hybrid " << +cHybrid->getId()
            //                       << " default value in map is 0x" << std::hex << +cValueFromMap << std::dec << " value read back from chip is 0x" << std::hex << +cRegItem.fValue << std::dec
            //                       << " .... Enabling CIC " << +cHybrid->getId() << RESET;
            //     }

            //     // ChipRegItem cRegItem = cCic->getRegItem((*cIterator).first);
            //     // bool        cWrite   = false;
            //     // this->EncodeReg(cRegItem, cHybrid->getId(), cCic->getId(), cVec, true, cWrite);
            //     // bool cWriteSuccess = !this->WriteI2C(cVec, cReplies, true, false);
            //     // if(cWriteSuccess)
            //     // {
            //     //     LOG(INFO) << BOLDGREEN << "Successful read from " << (*cIterator).first << " [first non-zero I2C register of CIC] on hybrid " << +cHybrid->getId() << " .... Enabling CIC"
            //     //               << +cCic->getId() << RESET;
            //     //     hybrid_enable |= 1 << cHybrid->getId();
            //     //     fNCic++;
            //     // }
            // }
            if(fFirmwareFrontEndType == FrontEndType::CIC || fFirmwareFrontEndType == FrontEndType::CIC2)
            {
                LOG(INFO) << BOLDBLUE << "CIC " << +cCic->getId() << " on FE" << +cHybrid->getId() << RESET;
                size_t cIndex       = 0;
                auto   cRegisterMap = cCic->getRegMap();
                auto   cIterator    = cRegisterMap.begin();
                do
                {
                    cIndex++;
                    if((*cIterator).second.fValue != 0) cIterator++;
                } while((*cIterator).second.fValue != 0 && cIndex < cRegisterMap.size());
                ChipRegItem cRegItem = cCic->getRegItem((*cIterator).first);
                bool        cWrite   = false;
                this->EncodeReg(cRegItem, cHybrid->getId(), cCic->getId(), cVec, true, cWrite);
                bool cWriteSuccess = !this->WriteI2C(cVec, cReplies, true, false);
                if(cWriteSuccess)
                {
                    LOG(INFO) << BOLDGREEN << "Successful read from " << (*cIterator).first << " [first non-zero I2C register of CIC] on hybrid " << +cHybrid->getId() << " .... Enabling CIC"
                              << +cCic->getId() << RESET;
                    hybrid_enable |= 1 << cHybrid->getId();
                    fNCic++;
                }
            }
            else if(fNReadoutChip == cHybrid->size())
            {
                hybrid_enable |= 1 << cHybrid->getId();
                fNHybrids += 1;
            }
        }
    }
    LOG(INFO) << BOLDBLUE << +fNCic << " CIC(s) enabled on this BeBoard" << RESET;
    cVecReg.push_back({"fc7_daq_cnfg.global.hybrid_enable", hybrid_enable});
    LOG(INFO) << BOLDBLUE << "Setting hybrid enable register to " << std::bitset<32>(hybrid_enable) << RESET;
    this->WriteStackReg(cVecReg);
    cVecReg.clear();
}
void D19cFWInterface::InitFMCPower()
{
    uint32_t fmc1_card_type = ReadReg("fc7_daq_stat.general.info.fmc1_card_type");
    uint32_t fmc2_card_type = ReadReg("fc7_daq_stat.general.info.fmc2_card_type");

    std::string cFMC1name = fFMCMap[fmc1_card_type];
    std::string cFMC2name = fFMCMap[fmc2_card_type];
    bool        cWithIB   = (cFMC1name == "MPA_SSA");
    bool        cWithDIO5 = (cFMC1name == "DIO5" || cFMC2name == "DIO5");       // DIO5 in either slot
    bool        cPSMux    = (cFMC1name == "PS_FMC1" && cFMC2name == "PS_FMC2"); // PS Mux Crate
    cPSMux                = cPSMux || (cFMC1name == "PS_FMC2" && cFMC2name == "PS_FMC1");
    bool c2SMux           = (cFMC1name == "2S_FMC1" && cFMC2name == "2S_FMC2"); // 2S Mux Crate
    c2SMux                = c2SMux || (cFMC1name == "2S_FMC2" && cFMC2name == "2S_FMC1");
    if(cWithDIO5 || cPSMux || c2SMux) this->WriteReg("sysreg.fmc_pwr.pg_c2m", 0x1);

    bool cEnableL12 = (cFMC1name == "DIO5");
    cEnableL12      = cEnableL12 || (cFMC1name.find("PS_FMC") != std::string::npos);
    cEnableL12      = cEnableL12 || (cFMC1name.find("2S_FMC") != std::string::npos);
    bool cEnableL8  = (cFMC2name == "DIO5");
    cEnableL8       = cEnableL8 || (cFMC2name.find("PS_FMC") != std::string::npos);
    cEnableL8       = cEnableL8 || (cFMC2name.find("2S_FMC") != std::string::npos);
    if(cWithDIO5)
    {
        if(cFMC1name == "DIO5")
            LOG(INFO) << BOLDGREEN << "Powering on DIO5 at L12..." << RESET;
        else
            LOG(INFO) << BOLDGREEN << "Powering on DIO5 at L8..." << RESET;
    }
    else if(cPSMux || c2SMux)
    {
        LOG(INFO) << BOLDGREEN << "Powering FMCs in multiplexing setup" << RESET;
    }

    std::vector<std::string> cRegNames  = {"sysreg.fmc_pwr.l12_pwr_en", "sysreg.fmc_pwr.l8_pwr_en"};
    std::vector<bool>        cFMCStates = {cEnableL12, cEnableL8};
    std::vector<uint8_t>     cFMCIds    = {12, 8};
    for(size_t cIndx = 0; cIndx < cRegNames.size(); cIndx++)
    {
        if(cFMCStates[cIndx] == false) continue;
        if(cFMC1name == "DIO5" || cFMC2name == "DIO5") this->PowerOnDIO5(cFMCIds[cIndx]);
    }

    // if(!(cWithDIO5 || cPSMux || c2SMux || cWithIB)) LOG(ERROR) << "Enabling of FMC power for this setup is not required, check configuration file..";
    if(cWithIB)
    {
        LOG(INFO) << BOLDBLUE << "Enabling Interface board for MPA-SSA communication" << RESET;
        // power cycle board - jic
        this->PSInterfaceBoard_PowerOff_SSA();
        this->ReadPower_SSA();
        this->PSInterfaceBoard_PowerOn_SSA(1.25, 1.0, 1.25, 0.3, 0.0, 145);
        this->ReadPower_SSA();
    }
}

void D19cFWInterface::PowerOnDIO5(uint8_t pFMCId)
{
    std::string cRegName = (pFMCId == 12) ? "sysreg.fmc_pwr.l12_pwr_en" : "sysreg.fmc_pwr.l8_pwr_en";
    uint8_t     cSel     = (pFMCId == 12) ? 1 : 0;

    LOG(INFO) << BOLDGREEN << "Powering on DIO5" << RESET;
    // define constants
    uint8_t i2c_slv = 0x2f;
    uint8_t wr      = 1;
    // uint8_t rd = 0;
    // uint8_t p3v3 = 0xff - 0x09;
    uint8_t p2v5 = 0xff - 0x2b;
    // uint8_t p1v8 = 0xff - 0x67;

    // disable power
    WriteReg(cRegName, 0x0);

    // enable i2c
    WriteReg("sysreg.i2c_settings.i2c_bus_select", 0x0);
    WriteReg("sysreg.i2c_settings.i2c_prescaler", 1000);
    WriteReg("sysreg.i2c_settings.i2c_enable", 0x1);
    // uint32_t i2c_settings_reg_command = (0x1 << 15) | (0x0 << 10) | 1000;
    // WriteReg("sysreg.i2c_settings", i2c_settings_reg_command);

    // set value
    uint8_t  reg_addr        = (cSel << 7) + 0x08;
    uint8_t  wrdata          = p2v5;
    uint32_t sys_i2c_command = ((1 << 24) | (wr << 23) | (i2c_slv << 16) | (reg_addr << 8) | (wrdata));

    WriteReg("sysreg.i2c_command", sys_i2c_command | 0x80000000);
    WriteReg("sysreg.i2c_command", sys_i2c_command);

    int status       = 0; // 0 - busy, 1 -done, 2 - error
    int attempts     = 0;
    int max_attempts = 1000;
    while(status == 0 && attempts < max_attempts)
    {
        uint32_t i2c_status = ReadReg("sysreg.i2c_reply.status");
        attempts            = attempts + 1;
        //
        if((int)i2c_status == 1)
            status = 1;
        else if((int)i2c_status == 0)
            status = 0;
        else
            status = 2;

        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    }

    // disable i2c
    WriteReg("sysreg.i2c_settings.i2c_enable", 0x0);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

    // enable power
    WriteReg(cRegName, 0x1);
}

void D19cFWInterface::TriggerConfiguration()
{
    auto cConfiguredSrc = ReadReg("fc7_daq_stat.fast_command_block.general.source");
    auto cSource        = this->ReadReg("fc7_daq_cnfg.fast_command_block.trigger_source");
    auto cRate          = this->ReadReg("fc7_daq_cnfg.fast_command_block.user_trigger_frequency");
    auto cMultiplicity  = this->ReadReg("fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    if(cSource != 6 && cSource != 10) LOG(DEBUG) << BOLDMAGENTA << "Trigger Rate is : " << +cRate << RESET;
    LOG(DEBUG) << BOLDMAGENTA << "Trigger Multiplicity is : " << +cMultiplicity << RESET;
    if(cConfiguredSrc != cSource)
    {
        LOG(ERROR) << BOLDRED << "Mismatch in trigger source configuration... going to reload and check again " << RESET;
        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        LOG(INFO) << BOLDRED << "Re-configuring trigger source to be " << +cSource << RESET;
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cSource});
        cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        this->WriteStackReg(cRegVec);
        TriggerConfiguration();
    }
}
void D19cFWInterface::SendNTriggers(uint16_t pNtriggers)
{
    // count triggers sent to the CIC
    bool   cAllTriggersSent = false;
    size_t cAttempt         = 0;
    size_t cMaxAttempts     = 10;
    this->ResetTriggerFSM();
    auto cNTriggersSent = this->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
    do
    {
        this->Start();
        do
        {
            cNTriggersSent   = this->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
            cAllTriggersSent = (cNTriggersSent >= pNtriggers);
        } while(!cAllTriggersSent);
        this->ResetTriggerFSM();
        cAttempt++;
    } while(cNTriggersSent == 0 && cAttempt < cMaxAttempts);
}
void D19cFWInterface::Start()
{
    auto cTriggerState = GetTriggerState();
    if(cTriggerState == 1)
    {
        LOG(INFO) << BOLDMAGENTA << "Triggers have already been started... stop them " << RESET;
        this->Stop();
    }

    // first lets
    // reset the readout
    this->ResetReadout();
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
    cTriggerState = GetTriggerState();
    // get handshake mode
    // this changes how I check if I've actually started
    auto cHandshake = ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable");
    bool cBreak     = false;
    do
    {
        LOG(DEBUG) << BOLDBLUE << "D19cFWInterface::Start Trigger state is " << cTriggerState << RESET;
        // this stops triggers  + resets
        this->ResetTriggerFSM();
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
        this->TriggerConfiguration();

        // here open the shutter for the stub counter block (for some reason self clear doesn't work, that why we have to
        // clear the register manually)
        WriteReg("fc7_daq_ctrl.stub_counter_block.general.shutter_open", 0x1);
        WriteReg("fc7_daq_ctrl.stub_counter_block.general.shutter_open", 0x0);
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

        WriteReg("fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

        // prints to debug and also checks that things are ok
        this->TriggerConfiguration();
        cTriggerState = GetTriggerState();
        LOG(DEBUG) << BOLDBLUE << "D19cFWInterface::Start Trigger state is " << cTriggerState << RESET;
        
        // now check if I should try and start again
        if(cHandshake)
        {
            auto cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
            cBreak           = (cReadoutReq == 1);
            if(cBreak)
                LOG(DEBUG) << BOLDMAGENTA << "Hand-shakee is on .. readout-request after start is " << +cReadoutReq << " - triggers have started and I've got all the events I've asked for " << RESET;
        }
        else
            cBreak = (cTriggerState != 0);
        if(!cBreak) LOG(INFO) << BOLDRED << "Triggers failed to START - trying again" << RESET;
    } while(!cBreak);
    LOG(DEBUG) << BOLDBLUE << "D19cFWInterface::Start Trigger state at the end of start is " << +cTriggerState << RESET;
}

void D19cFWInterface::Stop()
{
    // LOG (INFO) << BOLDBLUE << "D19cFWInterface::Stop" << RESET;
    // here close the shutter for the stub counter block
    WriteReg("fc7_daq_ctrl.stub_counter_block.general.shutter_close", 0x1);
    WriteReg("fc7_daq_ctrl.stub_counter_block.general.shutter_close", 0x0);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

    auto cTriggerState = GetTriggerState();
    do
    {
        LOG(DEBUG) << BOLDBLUE << "D19cFWInterface::Stop Trigger state is " << cTriggerState << RESET;
        WriteReg("fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
        cTriggerState = GetTriggerState();
    } while(cTriggerState == 1);
}
// reconfigure trigger
void D19cFWInterface::ResetTriggerFSM()
{
    // LOG (INFO) << BOLDBLUE << "D19cFWInterface::ResetTriggerFSM" << RESET;
    // stop trigger
    this->Stop();

    // reset trigger
    this->WriteReg("fc7_daq_ctrl.fast_command_block.control.reset", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 1));
    // load new trigger configuration
    this->WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 1));

    // // check trigger source and rate
    // // print out config
    // LOG (DEBUG) << BOLDMAGENTA << "Verifying trigger configuration after reset..." << RESET;
    this->TriggerConfiguration();
}
void D19cFWInterface::Pause()
{
    LOG(INFO) << BOLDBLUE << "................................ Pausing run ... " << RESET;
    WriteReg("fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
}

void D19cFWInterface::Resume()
{
    LOG(INFO) << BOLDBLUE << "Reseting readout before resuming run ... " << RESET;
    this->ResetReadout();

    LOG(INFO) << BOLDBLUE << "................................ Resuming run ... " << RESET;
    WriteReg("fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
}

void D19cFWInterface::ResetReadout()
{
    // LOG (INFO) << BOLDBLUE << "Resetting readout..." << RESET;
    auto cPkgDelay = this->ReadReg("fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay");
    LOG(DEBUG) << "Package delay is set to " << +cPkgDelay << RESET;
    WriteReg("fc7_daq_ctrl.readout_block.control.readout_reset", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

    WriteReg("fc7_daq_ctrl.readout_block.control.readout_reset", 0x0);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

    if(ReadReg("fc7_daq_stat.ddr3_block.is_ddr3_type"))
    {
        LOG(DEBUG) << BOLDBLUE << "Reseting DDR3 " << RESET;
        fDDR3Offset     = 0;
        fDDR3Calibrated = (ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
        bool i          = false;
        while(!fDDR3Calibrated)
        {
            if(i == false) LOG(DEBUG) << "Waiting for DDR3 to finish initial calibration";
            i = true;
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
            fDDR3Calibrated = (ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
        }
    }
}

void D19cFWInterface::DDR3SelfTest()
{
    // opened issue: without this time delay the self-test doesn't examine entire 4Gb address space of the chip(reason
    // not obvious)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if(ReadReg("fc7_daq_stat.ddr3_block.is_ddr3_type") && fDDR3Calibrated)
    {
        // trigger the self check
        WriteReg("fc7_daq_ctrl.ddr3_block.control.traffic_str", 0x1);

        bool cDDR3Checked = (ReadReg("fc7_daq_stat.ddr3_block.self_check_done") == 1);
        bool j            = false;
        LOG(INFO) << GREEN << "============================" << RESET;
        LOG(INFO) << BOLDGREEN << "DDR3 Self-Test" << RESET;

        while(!cDDR3Checked)
        {
            if(j == false) LOG(INFO) << "Waiting for DDR3 to finish self-test";
            j = true;
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
            cDDR3Checked = (ReadReg("fc7_daq_stat.ddr3_block.self_check_done") == 1);
        }

        if(cDDR3Checked)
        {
            int num_errors = ReadReg("fc7_daq_stat.ddr3_block.num_errors");
            int num_words  = ReadReg("fc7_daq_stat.ddr3_block.num_words");
            LOG(DEBUG) << "Number of checked words " << num_words;
            LOG(DEBUG) << "Number of errors " << num_errors;
            if(num_errors == 0) { LOG(INFO) << "DDR3 self-test ->" << BOLDGREEN << " PASSED" << RESET; }
            else
                LOG(ERROR) << "DDR3 self-test ->" << BOLDRED << " FAILED" << RESET;
        }
        LOG(INFO) << GREEN << "============================" << RESET;
    }
}

void D19cFWInterface::ConfigureFastCommandBlock(const BeBoard* pBoard)
{
    // last, loop over the variable registers from the HWDescription.xml file
    // this is where I should get all the clocking and FastCommandInterface settings
    BeBoardRegMap                                 cRegMap = pBoard->getBeBoardRegMap();
    std::vector<std::pair<std::string, uint32_t>> cVecReg;

    for(auto const& it: cRegMap)
    {
        auto cRegName = it.first;
        if(cRegName.find("fc7_daq_cnfg.fast_command_block.") != std::string::npos)
        {
            // LOG (DEBUG) << BOLDBLUE << "Setting " << cRegName << " : " << it.second << RESET;
            cVecReg.push_back({it.first, it.second});
        }
    }
    this->WriteStackReg(cVecReg);
    cVecReg.clear();
    // load trigger configuration
    WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);
}

std::string D19cFWInterface::L1ADebug(uint8_t pWait_ms, bool pPrint)
{
    // this->ConfigureTriggerFSM(0, 750, 3);
    // // use generic fast command block to send ReSync + L1A
    // this->ResetFCMDBram();
    // std::vector<uint8_t> cFastCommands(0);cFastCommands.clear();
    // size_t cL1toClear = 10;
    // size_t cAfterClear = 5000;
    // size_t cDelayAfterReSync = this->ReadReg("fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset");
    // size_t cDelayAfterTP     = this->ReadReg("fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    // size_t cDelayToNext      = this->ReadReg("fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse");
    // LOG (INFO) << BOLDMAGENTA << "Delay after ReSync : " << cDelayAfterReSync << " Delay after TP : " << cDelayAfterTP << RESET;
    // for(size_t cIndx=0; cIndx < 14000 ; cIndx++)
    // {
    //     if( cIndx == 0 ) cFastCommands.push_back( 0xC3 ); // BC0 to reset L1 capture
    //     else if( cIndx == cL1toClear )  cFastCommands.push_back( 0xC9 ); // flush L1A FIFO
    //     else if( cIndx ==  cL1toClear+cAfterClear ) cFastCommands.push_back( 0xD3 ); // send a ReSync+BC0
    //     else if ( cIndx == cL1toClear+cAfterClear+cDelayAfterReSync ) cFastCommands.push_back( 0xC5 ); // send a TP injection
    //     else if ( cIndx == cL1toClear+cAfterClear+cDelayAfterReSync+cDelayAfterTP ) cFastCommands.push_back( 0xC9 ); // send an L1A
    //     else if ( cIndx == cL1toClear+cAfterClear+cDelayAfterReSync+cDelayAfterTP+cDelayToNext ) cFastCommands.push_back( 0xC9 ); // send another L1A
    //     else cFastCommands.push_back( 0xC1 );
    // }
    // ConfigureFCMDBram(cFastCommands);
    // // repeat the sequence N times
    // this->WriteReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x00);
    // this->WriteReg("fc7_daq_cnfg.readout_block.packet_nbr", 1);
    // this->WriteReg("fc7_daq_cnfg.fast_command_block.generic_fcmd.number_of_repetitions", 1);
    // // make sure fast command duration is 0
    // this->WriteReg("fc7_daq_ctrl.fast_command_block.control.fast_duration", 0x0);
    // // make sure all triggers are accepted
    // this->WriteReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept", 1);
    // ResetTriggerFSM();

    // //this->Compose_fast_command(fFastCommandDuration, 0, 0, 0, 1);
    // // start generic  - ctrl signal high
    // this->WriteReg("fc7_daq_ctrl.fast_command_block.control.start_generic", 0x1);
    // this->WriteReg("fc7_daq_ctrl.fast_command_block.control.start_generic", 0x0);

    // disable back-pressure
    this->WriteReg("fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0);
    this->Start();
    // std::this_thread::sleep_for(std::chrono::microseconds(pWait_ms * 1000));
    this->Stop();

    LOG(DEBUG) << BOLDMAGENTA << "First header found after " << this->ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.first_header_delay") << " clock cycles." << RESET;
    auto cWords = ReadBlockReg("fc7_daq_stat.physical_interface_block.l1a_debug", 50);
    LOG(DEBUG) << BOLDBLUE << "Hits debug ...." << RESET;
    std::string cBuffer   = "";
    size_t      cLineIndx = 0;
    for(auto cWord: cWords)
    {
        auto                     cString = std::bitset<32>(cWord).to_string();
        std::vector<std::string> cOutputWords(0);
        for(size_t cIndex = 0; cIndex < 4; cIndex++) { cOutputWords.push_back(cString.substr(cIndex * 8, 8)); }
        std::string cOutput = "";
        for(auto cIt = cOutputWords.end() - 1; cIt >= cOutputWords.begin(); cIt--)
        {
            cOutput += *cIt + " ";
            cBuffer += *cIt;
        }
        if(pPrint) LOG(INFO) << BOLDBLUE << "#" << +cLineIndx << ":" << cOutput << RESET;
        cLineIndx++;
    }
    return cBuffer;
    /*
    // search for L1 headers
    size_t cSearch = 0;
    size_t cPos = 0;
    auto   cFound = cBuffer.find("111111111111111111111110",cPos);
    std::stringstream cL1DataHeaders;
    do
    {
        cSearch=cBuffer.find("111111111111111111111110",cPos);
        auto cHeader = cBuffer.substr( cSearch - 8  , 32 );
        size_t cCount1s = std::count_if( cHeader.begin(), cHeader.end(), []( char c ){return c =='1';});
        auto cStatus = cBuffer.substr( cSearch - 8 + 32 , 9 );
        auto cL1Id   = cBuffer.substr( cSearch - 8 + 32 + 9 , 9 );
        cL1DataHeaders << cHeader << "[" <<  cCount1s << "]" << cStatus << "-" << std::stoi(cL1Id,0,2) << ":" ;
        //LOG (INFO) << BOLDYELLOW << cHeader << " - " << cStatus << " - " << cL1Id << RESET;
        cPos = cSearch - 8 + 32;
        cFound = cBuffer.find("111111111111111111111110",cPos);
    }while( cFound != std::string::npos );
    LOG (INFO) << BOLDYELLOW << cL1DataHeaders.str() << RESET;
    */

    // size_t cOffset = 28;
    // auto cHeader = cBuffer.substr(4, cOffset); cOffset+=4;
    // auto cStatus = cBuffer.substr(cOffset, 9);cOffset+=9;
    // auto cL1Id = std::stoi( cBuffer.substr(cOffset, 9), 0 ,2 );cOffset+=9;
    // auto cCbcErr = cBuffer.substr(cOffset, 2); cOffset+=2;
    // auto cPipeAddr = std::stoi( cBuffer.substr(cOffset, 9),0,2); cOffset+=9;
    // auto cL1IdCbc = std::stoi( cBuffer.substr(cOffset, 9),0,2);cOffset+=9;
    // LOG (INFO) << BOLDMAGENTA << "Header is " << cHeader
    //     << " Status is " << cStatus
    //     << " L1Id is " << cL1Id
    //     << " CBC Error is " << cCbcErr
    //     << " Pipeaddress is " << cPipeAddr
    //     << " L1Id CBC is " << cL1IdCbc << RESET;
    this->ResetReadout();
}
std::vector<std::string> D19cFWInterface::StubDebug(bool pWithTestPulse, uint8_t pNlines)
{
    this->ResetReadout();
    if(pWithTestPulse)
        this->ChipTestPulse();
    else
        this->Trigger(0);

    auto                     cWords = ReadBlockReg("fc7_daq_stat.physical_interface_block.stub_debug", 80);
    std::vector<std::string> cLines(0);
    size_t                   cLine = 0;
    // int cStrLength=0;
    do
    {
        std::vector<std::string> cOutputWords(0);
        for(size_t cIndex = 0; cIndex < 5; cIndex++)
        {
            auto cWord   = cWords[cLine * 10 + cIndex];
            auto cString = std::bitset<32>(cWord).to_string();
            for(size_t cOffset = 0; cOffset < 4; cOffset++) { cOutputWords.push_back(cString.substr(cOffset * 8, 8)); }
        }

        std::string cOutput_wSpace = "";
        std::string cOutput        = "";
        for(auto cIt = cOutputWords.end() - 1; cIt >= cOutputWords.begin(); cIt--)
        {
            cOutput_wSpace += *cIt + " ";
            cOutput += *cIt;
        }
        LOG(INFO) << BOLDBLUE << "Line " << +cLine << " : " << cOutput_wSpace << RESET;
        cLines.push_back(cOutput);
        // cStrLength = cOutput.length();
        cLine++;
    } while(cLine < pNlines);
    this->ResetReadout();
    return cLines;
}
std::vector<std::string> D19cFWInterface::ScopeStubLines(bool pWithTestPulse)
{
    uint8_t cNlines = 5;
    if(pWithTestPulse)
        this->ChipTestPulse();
    else
        this->Trigger(0);

    auto                     cWords = ReadBlockReg("fc7_daq_stat.physical_interface_block.stub_debug", 80);
    std::vector<std::string> cLines(0);
    size_t                   cLine = 0;
    // int cStrLength=0;
    do
    {
        std::vector<std::string> cOutputWords(0);
        for(size_t cIndex = 0; cIndex < cNlines; cIndex++)
        {
            auto cWord   = cWords[cLine * 10 + cIndex];
            auto cString = std::bitset<32>(cWord).to_string();
            for(size_t cOffset = 0; cOffset < 4; cOffset++) { cOutputWords.push_back(cString.substr(cOffset * 8, 8)); }
        }

        std::string cOutput_wSpace = "";
        std::string cOutput        = "";
        for(auto cIt = cOutputWords.end() - 1; cIt >= cOutputWords.begin(); cIt--)
        {
            cOutput_wSpace += *cIt + " ";
            cOutput += *cIt;
        }
        LOG(DEBUG) << BOLDBLUE << "Line " << +cLine << " : " << cOutput_wSpace << RESET;
        cLines.push_back(cOutput);
        // cStrLength = cOutput.length();
        cLine++;
    } while(cLine < cNlines);
    this->ResetReadout();
    return cLines;
}

// tuning of L1A lines
bool D19cFWInterface::L1PhaseTuning(const BeBoard* pBoard, bool pScope)
{
    bool cSuccess = true;
    LOG(INFO) << BOLDBLUE << "Aligning the back-end to properly sample L1A data coming from the front-end objects." << RESET;
    // original reg map
    BeBoardRegMap cRegisterMap = pBoard->getBeBoardRegMap();

    if(pScope) this->L1ADebug();

    // configure triggers
    // make sure you're only sending one trigger at a time
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    // configure trigger
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", 10});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 3});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0});
    this->ReconfigureTriggerFSM(cVecReg);

    // check if you have CIC2
    bool cWithCIC2 = false;
    for(auto cOpticalGroup: *pBoard)
    {
        if(cWithCIC2) continue;
        for(auto cHybrid: *cOpticalGroup)
        {
            if(cWithCIC2) continue;
            // uint8_t cBitslip=0;
            selectLink(cOpticalGroup->getId());
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            if(cCic->getFrontEndType() == FrontEndType::CIC2) cWithCIC2 = true;
        }
    }
    if(!cWithCIC2) this->Start();

    LOG(INFO) << BOLDBLUE << "Aligning the back-end to properly decode L1A data coming from the front-end objects." << RESET;
    PhaseTuner pTuner;
    // back-end tuning on l1 lines
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            // uint8_t cBitslip=0;
            selectLink(cOpticalGroup->getId());
            auto& cCic    = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            int   cChipId = cCic->getId();
            // need to know the address
            // here in case you want to look at the L1A by scoping the lines in firmware - useful when debuging
            // if( cHybrid->getId() > 0 )
            //   this->WriteReg( "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select" , cHybrid->getId()) ;
            uint8_t cLineId = 0;
            // tune phase on l1A line - don't have t do anything on the FEs
            if(fOptical) { LOG(INFO) << BOLDBLUE << "Optical readout .. don't have to do anything here" << RESET; }
            else
            {
                this->ChipReSync();
                LOG(INFO) << BOLDBLUE << "Performing phase tuning [in the back-end] to prepare for receiving CIC L1A data ...: FE " << +cHybrid->getId() << " Chip" << +cChipId << RESET;
                uint16_t cPattern = 0xAA;
                // configure pattern
                pTuner.SetLineMode(this, cHybrid->getId(), 0, cLineId, 0);
                pTuner.SetLinePattern(this, cHybrid->getId(), 0, cLineId, cPattern, 8);
                // start phase aligner
                pTuner.SendControl(this, cHybrid->getId(), 0, cLineId, "PhaseAlignment");
                this->Start();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                this->Stop();
            }
        }
    }
    if(!cWithCIC2) this->Stop();

    if(pScope) this->L1ADebug();

    // reconfigure original trigger configu
    cVecReg.clear();
    for(auto const& it: cRegisterMap)
    {
        auto cRegName = it.first;
        if(cRegName.find("fc7_daq_cnfg.fast_command_block.") != std::string::npos)
        {
            // LOG (DEBUG) << BOLDBLUE << "Setting " << cRegName << " : " << it.second << RESET;
            cVecReg.push_back({it.first, it.second});
        }
    }
    this->ReconfigureTriggerFSM(cVecReg);
    return cSuccess;
}
bool D19cFWInterface::L1WordAlignment(const OpticalGroup* pOpticalGroup, bool pScope)
{
    bool cAllowZeroBitslip = true;
    fBeL1Delays.clear();
    fBeL1Bitslips.clear();

    LOG(INFO) << BOLDBLUE << "Aligning the back-end to properly decode L1A data coming from the front-end objects." << RESET;
    PhaseTuner pTuner;
    bool       cSuccess = true;

    // configure triggers
    // make sure you're only sending one trigger at a time
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.clear();
    std::vector<std::string> cFcmdRegs{"misc.trigger_multiplicity", "user_trigger_frequency", "trigger_source", "misc.backpressure_enable", "triggers_to_accept"};
    std::vector<uint16_t>    cFcmdRegVals{0, 100, 3, 0, 0};
    std::vector<uint8_t>     cFcmdRegOrigVals(0);
    for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    {
        std::string cRegName = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
        cFcmdRegOrigVals.push_back(this->ReadReg(cRegName));
        cVecReg.push_back({cRegName, cFcmdRegVals[cIndx]});
    }
    this->ReconfigureTriggerFSM(cVecReg);
    cVecReg.clear();
    for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    {
        std::string cRegName = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
        cVecReg.push_back({cRegName, cFcmdRegOrigVals[cIndx]});
    }

    // back-end tuning on l1 lines
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic    = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        int   cChipId = cCic->getId();
        // if( cHybrid->getId() > 0 )
        uint8_t cLineId = 0;
        this->ChipReSync();
        LOG(INFO) << BOLDBLUE << "Performing word alignment [in the back-end] to prepare for receiving CIC L1A data ...: FE " << +cHybrid->getId() << " Chip" << +cChipId << RESET;
        uint16_t cPattern = 0xFE;
        // select lines for slvs debug
        this->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
        this->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
        // configure pattern
        pTuner.SetLineMode(this, cHybrid->getId(), 0, cLineId, 0);
        uint32_t cFrontEndTypeCode = ReadReg("fc7_daq_stat.general.info.chip_type");
        bool     cWithCIC          = (getFrontEndType(cFrontEndTypeCode) == FrontEndType::CIC || getFrontEndType(cFrontEndTypeCode) == FrontEndType::CIC2);
        if(cWithCIC)
        {
            for(uint16_t cPatternLength = 40; cPatternLength < 41; cPatternLength++)
            {
                pTuner.SetLinePattern(this, cHybrid->getId(), 0, cLineId, cPattern, cPatternLength);
                // start word aligner
                pTuner.SendControl(this, cHybrid->getId(), 0, cLineId, "WordAlignment");
                this->Start();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                this->Stop();
                uint8_t cLineStatus = pTuner.GetLineStatus(this, cHybrid->getId(), 0, cLineId);
                LOG(DEBUG) << BOLDBLUE << "Line status is " << +cLineStatus << RESET;
                cSuccess = pTuner.fDone;
            }
            // if the above doesn't work.. try and find the correct bitslip manually in software
            if(!cSuccess)
            {
                LOG(INFO) << BOLDBLUE << "Going to try and align manually in software..." << RESET;
                const uint8_t cMaxIters  = 10;
                uint8_t       cIterCount = 0;
                do
                {
                    LOG(INFO) << BOLDBLUE << "\t\t Alignment attempt#" << +cIterCount << RESET;
                    for(uint8_t cBitslip = 0; cBitslip < 8; cBitslip++)
                    {
                        LOG(INFO) << BOLDMAGENTA << "Manually setting bitslip to " << +cBitslip << RESET;
                        pTuner.SetLineMode(this, cHybrid->getId(), 0, cLineId, 2, pTuner.fDelay, cBitslip, 0, 0);
                        this->Start();
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        this->Stop();

                        auto        cWords   = ReadBlockReg("fc7_daq_stat.physical_interface_block.l1a_debug", 50);
                        std::string cBuffer  = "";
                        bool        cAligned = false;
                        std::string cOutput  = "\n";
                        for(auto cWord: cWords)
                        {
                            auto                     cString = std::bitset<32>(cWord).to_string();
                            std::vector<std::string> cOutputWords(0);
                            for(size_t cIndex = 0; cIndex < 4; cIndex++)
                            {
                                auto c8bitWord = cString.substr(cIndex * 8, 8);
                                cOutputWords.push_back(c8bitWord);
                                cAligned = (cAligned | (std::stoi(c8bitWord, nullptr, 2) == cPattern));
                            }
                            for(auto cIt = cOutputWords.end() - 1; cIt >= cOutputWords.begin(); cIt--) { cOutput += *cIt + " "; }
                            cOutput += "\n";
                        }
                        if(cAligned)
                        {
                            LOG(INFO) << BOLDGREEN << cOutput << RESET;
                            this->ResetReadout();
                            pTuner.fBitslip = cBitslip;
                            cSuccess        = cAllowZeroBitslip ? true : (cBitslip != 0);
                        }
                        else
                            LOG(INFO) << BOLDRED << cOutput << RESET;
                        this->ResetReadout();
                    }
                    cIterCount++;
                } while(cIterCount < cMaxIters && !cSuccess);
            }
        }
        else if(getFrontEndType(cFrontEndTypeCode) == FrontEndType::CBC3)
        {
        }
        else if(getFrontEndType(cFrontEndTypeCode) == FrontEndType::SSA)
        {
        }
        else
        {
            LOG(INFO) << BOLDBLUE << "Word alignment in the back-end not implemented for this firmware type.." << RESET;
        }
        fBeL1Delays.push_back(pTuner.fDelay);
        fBeL1Bitslips.push_back(pTuner.fBitslip);

        if(pScope) this->L1ADebug();
    }

    // reconfigure original trigger configu
    this->ReconfigureTriggerFSM(cVecReg);
    return cSuccess;
}
bool D19cFWInterface::L1WordAlignment(const BeBoard* pBoard, bool pScope)
{
    LOG(INFO) << BOLDBLUE << "Aligning the back-end to properly decode L1A data coming from the front-end objects." << RESET;
    bool cSuccess = true;
    for(auto cOpticalGroup: *pBoard)
    {
        if(!cSuccess) continue;
        cSuccess = this->L1WordAlignment(cOpticalGroup, pScope);
    }
    return cSuccess;
}
// tuning of L1A lines
bool D19cFWInterface::L1Tuning(const BeBoard* pBoard, bool pScope)
{
    LOG(DEBUG) << BOLDBLUE << "PHASE" << RESET;

    bool cSuccess = this->L1PhaseTuning(pBoard, pScope);
    if(cSuccess)
    {
        LOG(DEBUG) << BOLDBLUE << "WORD" << RESET;
        cSuccess = this->L1WordAlignment(pBoard, pScope);
    }
    return cSuccess;
}
// tuning of stub lines
bool D19cFWInterface::StubTuning(const OpticalGroup* pOpticalGroup, bool pScope, uint8_t pNlines)
{
    PhaseTuner pTuner;
    bool       cSuccess = true;

    // back-end tuning on stub lines
    uint8_t cNlines = pNlines;
    selectLink(pOpticalGroup->getId());
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        if(cCic == NULL) continue;

        // this->WriteReg( "fc7_daq_cnfg.physical_interface_block.cic.debug_select" , cHybrid->getId()) ;
        this->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
        this->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
        // if(pScope) this->StubDebug(true, cNlines);

        LOG(INFO) << BOLDBLUE << "Performing word alignment [in the back-end] to prepare for receiving CIC stub data ...: FE " << +cHybrid->getId() << " Chip" << +cCic->getId() << RESET;
        for(uint8_t cLineId = 1; cLineId < 1 + cNlines; cLineId += 1)
        {
            if(fOptical)
            {
                LOG(INFO) << BOLDBLUE << "\t..... running word alignment...." << RESET;
                pTuner.AlignWord(this, cHybrid->getId(), 0, cLineId, 0xEA, 8, true);
                // pTuner.TuneLine(this, cHybrid->getId(), 0, cLineId, 0xEA, 8, true);
                cSuccess = cSuccess && pTuner.fDone;
            }
            else
            {
                bool   cSuccessThisLine = false;
                size_t cAttempts        = 0;
                do
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
                    pTuner.TuneLine(this, cHybrid->getId(), 0, cLineId, 0xEA, 8, true);
                    cSuccessThisLine = pTuner.fDone && pTuner.fBitslip != 0;
                    if(pTuner.fBitslip == 0) { LOG(DEBUG) << BOLDBLUE << "Trying to reset alignment .... don't like bit slip of 0!" << RESET; }
                    cAttempts++;
                } while(!cSuccessThisLine && cAttempts < 10);
                cSuccess = cSuccess && cSuccessThisLine;
            }
            // if(pTuner.fDone != 1)
            // {
            //     LOG(ERROR) << BOLDRED << "FAILED " << BOLDBLUE << " to tune stub line " << +(cLineId - 1) << " in the back-end." << RESET;
            //     exit(0);
            // }
        }

        if(pScope) this->StubDebug(true, cNlines);
    }
    return cSuccess;
}
bool D19cFWInterface::StubTuning(const BeBoard* pBoard, bool pScope, uint8_t pNlines)
{
    bool cSuccess = true;
    for(auto cOpticalGroup: *pBoard)
    {
        LOG(INFO) << BOLDMAGENTA << "D19cFWInterface::StubTuning OG#" << +cOpticalGroup->getId() << RESET;
        cSuccess = cSuccess && StubTuning(cOpticalGroup, pScope, pNlines);
    }
    return cSuccess;
}

bool D19cFWInterface::PhaseTuning(BeBoard* pBoard, uint8_t pFeId, uint8_t pChipId, uint8_t pLineId, uint16_t pPattern, uint16_t pPatternPeriod)
{
    LOG(DEBUG) << BOLDBLUE << "Phase and word alignement on BeBoard" << +pBoard->getId() << " FE" << +pFeId << " CBC" << +pChipId << " - line " << +pLineId << RESET;
    PhaseTuner pTuner;
    this->ChipReSync();

    // Not sure why this needs to be here -- otherwise occasioinally get unsync data (to check)
    if(fFirmwareFrontEndType == FrontEndType::MPA)
    {
        this->Align_out();
        return true;
    }
    pTuner.SetLineMode(this, pFeId, pChipId, pLineId, 2, 0, 0, 0, 0);

    bool         cSuccess  = false;
    unsigned int cAttempts = 0;
    do
    {
        cSuccess = pTuner.TuneLine(this, pFeId, pChipId, pLineId, pPattern, pPatternPeriod, true);

        // pTuner.GetLineStatus(this,  pFeId , pChipId , pLineId );
        // if( pTuner.fBitslip == 0 )
        // cSuccess = false;
        LOG(DEBUG) << BOLDBLUE << "Automated phase tuning attempt" << cAttempts << " : " << ((cSuccess) ? "Worked" : "Failed") << RESET;

        cAttempts++;
    } while(!cSuccess && cAttempts < 10);
    if(pLineId == 1 && (fFirmwareFrontEndType == FrontEndType::CBC3 || fFirmwareFrontEndType == FrontEndType::SSA || fFirmwareFrontEndType == FrontEndType::MPA))
    {
        uint8_t cEnableL1 = 0;
        LOG(INFO) << BOLDBLUE << "Forcing L1A line to match alignment result for first stub line." << RESET;
        // force L1A line to match phase tuning result for first stub lines to match
        uint8_t pDelay   = pTuner.fDelay;
        uint8_t cMode    = 2;
        uint8_t cBitslip = pTuner.fBitslip + (uint8_t)(fFirmwareFrontEndType == FrontEndType::SSA || fFirmwareFrontEndType == FrontEndType::MPA);
        pTuner.SetLineMode(this, pFeId, pChipId, 0, cMode, pDelay, cBitslip, cEnableL1, 0);
    }
    return cSuccess;
}
// uint32_t D19cFWInterface::CountFwEvents(BeBoard* pBoard, std::vector<uint32_t>& pData)
// {
//     uint32_t cNEvents       = 0;
//     if(pData.size() == 0) return cNEvents;
//     auto     cEventIterator = pData.begin();
//     do
//     {
//         uint32_t cEventSize = (0x0000FFFF & (*cEventIterator)) * 4; // event size is given in 128 bit words
//         // for now .. print the data out here
//         for(size_t cOffset = 0; cOffset < cEventSize; cOffset++)
//         {
//             if(cOffset < 4)
//                 LOG(DEBUG) << BOLDMAGENTA << "HEADER : " << std::bitset<32>(*(cEventIterator + cOffset)) << RESET;
//             else

//                 LOG(DEBUG) << BOLDBLUE << "\t...DATA\t.. : " << std::bitset<32>(*(cEventIterator + cOffset)) << RESET;
//         }
//         cEventIterator += cEventSize;
//         cNEvents++;
//     } while(cEventIterator < pData.end());
//     return cNEvents;
// }
// modified to check for header
// and remove dummy words from the event
uint32_t D19cFWInterface::CountFwEvents(BeBoard* pBoard, std::vector<uint32_t>& pData)
{
    uint32_t cNEvents = 0;
    if(pData.size() == 0) return cNEvents;

    // size_t cOriginalDataSize = pData.size();
    std::vector<uint32_t> cValidData(0);
    cValidData.clear();
    auto   cEventIterator = pData.begin();
    bool   cFoundEmpty    = false;
    size_t cOffset        = 0;
    size_t cCorr          = 0;
    do
    {
        // check event header
        uint32_t cFirstWord = *cEventIterator;
        uint32_t cHeader    = ((0xFFFF << 16) & cFirstWord) >> 16;
        if(cHeader != 0xFFFF)
        {
            if(!cFoundEmpty && cFirstWord == 0)
            {
                cFoundEmpty = true;
                cCorr       = cOffset; // how far from start
            }
            LOG(INFO) << BOLDMAGENTA << "Event header " << std::bitset<16>(cHeader) << " not EXPECTED" << RESET;
            cEventIterator = pData.end();
        }
        else
        {
            uint32_t cEventSize = (0x0000FFFF & (*cEventIterator)) * 4; // event size is given in 128 bit words
            // uint32_t cDummyCount = (0xFF & (*(cEventIterator + 1))) * 4;
            // LOG(DEBUG) << BOLDMAGENTA << "Valid event header .. copying over "
            //           << " event is made up of " << +cEventSize << " 32 bit words "
            //           << " of which " << +cDummyCount << " are dummy words." << RESET;
            // for(size_t cIndx = 0; cIndx < cEventSize; cIndx++) LOG(DEBUG) << BOLDYELLOW << "\t..." << std::bitset<32>(*(cEventIterator + cIndx)) << RESET;
            std::copy(pData.begin() + cOffset, pData.begin() + cOffset + cEventSize, std::back_inserter(cValidData));
            cEventIterator += cEventSize;
            cOffset += cEventSize;
            cNEvents++;
        }
    } while(cEventIterator < pData.end());
    cCorr = (cFoundEmpty) ? pData.size() - cCorr : cCorr;
    pData.clear();
    // LOG (INFO) << BOLDMAGENTA << "Original data vector has " << cOriginalDataSize
    //     << " 32-bit words..  found empty word in position "
    //     << cCorr
    //     << " from end of vector"
    //     << RESET;
    // adjust offset to first empty slot in DDR3
    if(cFoundEmpty)
    {
        LOG(INFO) << BOLDMAGENTA << "Found an empty event .. all 0s .. resetting DDR3 offset" << RESET;
        // fDDR3Offset = (cFoundEmpty) ? cFoundEmpty - cOffset : fDDR3Offset;
    }
    if(cValidData.size() == 0) return 0;
    std::move(cValidData.begin(), cValidData.end(), std::back_inserter(pData));
    LOG(DEBUG) << BOLDMAGENTA << "Returning a data vector with " << +pData.size() << " valid 32 bit words which are " << +cNEvents << " events." << RESET;
    return cNEvents;
}

// method to read SSA counters over I2C
void D19cFWInterface::ReadSSACounters(BeBoard* pBoard, std::vector<uint32_t>& pData)
{
    // get event type
    // EventType cEventType = pBoard->getEventType();
    // if(cEventType == EventType::SCAS)
    // {
    pData.clear();
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cFe: *cOpticalGroup)
        {
            for(auto cChip: *cFe)
            {
                LOG(DEBUG) << BOLDBLUE << "Directly reading back counters from SSA" << +cChip->getId() << RESET;
                bool                  cWrite = false;
                std::vector<uint32_t> cVec;
                cVec.clear();
                std::vector<uint32_t> cReplies;
                cReplies.clear();
                for(uint8_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                {
                    // MSB
                    ChipRegItem cReg_Counters_MSB;
                    cReg_Counters_MSB.fPage = 0x00;
                    if(cChip->getFrontEndType() == FrontEndType::SSA)
                        cReg_Counters_MSB.fAddress = 0x0801 + cChnl;
                    else
                        cReg_Counters_MSB.fAddress = 0x0680 + cChnl;
                    cReg_Counters_MSB.fValue = 0x00;
                    this->EncodeReg(cReg_Counters_MSB, cFe->getId(), cChip->getId(), cVec, true, cWrite);
                    // this->ReadChipBlockReg( cVec );
                    // cReplies.push_back(cVec[0]);
                    // cVec.clear();
                    // LSB
                    ChipRegItem cReg_Counters_LSB;
                    cReg_Counters_LSB.fPage = 0x00;
                    if(cChip->getFrontEndType() == FrontEndType::SSA)
                        cReg_Counters_LSB.fAddress = 0x0901 + cChnl;
                    else
                        cReg_Counters_LSB.fAddress = 0x0580 + cChnl;
                    cReg_Counters_LSB.fValue = 0x00;
                    this->EncodeReg(cReg_Counters_LSB, cFe->getId(), cChip->getId(), cVec, true, cWrite);
                    // this->ReadChipBlockReg( cVec );
                    // cReplies.push_back(cVec[0]);
                    // cVec.clear();
                }
                // read back
                this->ReadChipBlockReg(cVec);
                // set in data vector
                uint32_t cDataWord    = 0x0000;
                uint32_t cWordCounter = 0;
                uint16_t cIndx        = 0;
                for(uint8_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                {
                    uint8_t     cSSAId;
                    bool        cFailed = false;
                    bool        cRead;
                    ChipRegItem cReg_Counters_MSB;
                    cReg_Counters_MSB.fPage = 0x00;
                    if(cChip->getFrontEndType() == FrontEndType::SSA)
                        cReg_Counters_MSB.fAddress = 0x0801 + cChnl;
                    else
                        cReg_Counters_MSB.fAddress = 0x0680 + cChnl;
                    cReg_Counters_MSB.fValue = 0x00;
                    ChipRegItem cReg_Counters_LSB;
                    cReg_Counters_LSB.fPage = 0x00;
                    if(cChip->getFrontEndType() == FrontEndType::SSA)
                        cReg_Counters_LSB.fAddress = 0x0901 + cChnl;
                    else
                        cReg_Counters_LSB.fAddress = 0x0580 + cChnl;
                    cReg_Counters_LSB.fValue = 0x00;
                    this->DecodeReg(cReg_Counters_MSB, cSSAId, cVec[cIndx], cRead, cFailed);
                    this->DecodeReg(cReg_Counters_LSB, cSSAId, cVec[cIndx + 1], cRead, cFailed);
                    cIndx += 2;
                    uint16_t cCounterValue = ((cReg_Counters_MSB.fValue & 0xFF) << 8) | (cReg_Counters_LSB.fValue & 0xFF);
                    if(cChnl < 10)
                    {
                        LOG(DEBUG) << BOLDMAGENTA << "Strip#" << +cChnl << " : " << +cCounterValue << " hits."
                                   << " LSB " << +(cReg_Counters_LSB.fValue & 0xFF) << " MSB " << +(cReg_Counters_MSB.fValue & 0xFF) << RESET;
                    }
                    cDataWord = (cDataWord) | (cCounterValue << (cWordCounter & 0x1) * 16);
                    if((cWordCounter & 0x1) == 1)
                    {
                        pData.push_back(cDataWord);
                        cDataWord = 0x0000;
                    }
                    cWordCounter++;
                }
            } // chip loop
        }     // hybrid loop
    }         // hybrid loop
    // clear counters after they have been read
    this->PS_Clear_counters(fFastCommandDuration);
    // }
    // else
    // {
    //     LOG(ERROR) << BOLDRED << "Trying to read SSA counters when EventType does not match..." << RESET;
    //     throw std::runtime_error(std::string("Trying to read SSA counters when EventType does not match..."));
    // }
}
void D19cFWInterface::ReadPSCounters(BeBoard* pBoard, std::vector<uint32_t>& pData, bool pFast, bool pRawMode)
{
    // get event type
    EventType cEventType = pBoard->getEventType();
    if(cEventType == EventType::MPAAS or cEventType == EventType::SSAAS or cEventType == EventType::PSAS)
    {
        pData.clear();
        if(pRawMode && cEventType == EventType ::PSAS)
        {
            LOG(DEBUG) << BOLDMAGENTA << "Decoding fast counter readout from uDTC" << RESET;
            for(auto cOpticalGroup: *pBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    uint32_t cPSModuleId   = (pBoard->getId() << 16) | (cOpticalGroup->getId() << 8) | cHybrid->getId();
                    auto     cPSModuleIter = fPSModulesCounterData.find(cPSModuleId);
                    auto     cIter         = cPSModuleIter->second.begin();
                    do
                    {
                        LOG(DEBUG) << BOLDBLUE << "Decoded " << cIter->second.size() << " counters from FE" << std::bitset<4>(+cIter->first) << RESET;
                        for(auto cCounterInfo: cIter->second)
                        {
                            // LOG (INFO) << BOLDBLUE << "\t..Counter#" << +cCounterId << " : " << cIter->second[cCounterId] << RESET;
                            pData.push_back(cPSModuleId);
                            pData.push_back((cIter->first << 16) | cCounterInfo);
                        }
                        cIter++;
                    } while(cIter != cPSModuleIter->second.end());
                }
            }
            LOG(DEBUG) << BOLDMAGENTA << "Read-back " << pData.size() << " 32 bit words from FC7" << RESET;
        }
        // to-do.. add capture for parsed counter mode
        else
        {
            for(auto cOpticalGroup: *pBoard)
            {
                auto& clpGBT = cOpticalGroup->flpGBT;
                for(auto cFe: *cOpticalGroup)
                {
                    for(auto cChip: *cFe)
                    {
                        // uint8_t cMaster  = (cChip->getHybridId() % 2 == 0) ? 2 : 0;
                        bool cWithMPA = cChip->getFrontEndType() == FrontEndType::MPA;
                        bool cWithSSA = cChip->getFrontEndType() == FrontEndType::SSA;

                        if(cEventType == EventType::MPAAS && !cWithMPA) continue;
                        if(cEventType == EventType::SSAAS && !cWithSSA) continue;

                        // if( !cWithMPA ) continue;
                        if(clpGBT == nullptr)
                        {
                            LOG(DEBUG) << BOLDBLUE << "Directly reading back counters from MPA" << +cChip->getId() << RESET;
                            bool                  cWrite = false;
                            std::vector<uint32_t> cVec;
                            cVec.clear();
                            std::vector<ChipRegItem> cRegValues(2 * cChip->size());
                            size_t                   cIndx = 0;
                            for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                            {
                                int              cRowNumber       = cWithMPA ? 1 + cChnl / 120 : 1;
                                int              cPixelNumber     = cWithMPA ? 1 + cChnl % 120 : 0;
                                int              cBaseRegisterLSB = cWithMPA ? ((cRowNumber << 11) | (9 << 7) | cPixelNumber) : 0x0901 + cChnl;
                                int              cBaseRegisterMSB = cWithMPA ? ((cRowNumber << 11) | (10 << 7) | cPixelNumber) : 0x0801 + cChnl;
                                std::vector<int> cRegs{cBaseRegisterMSB, cBaseRegisterLSB};
                                for(auto cReg: cRegs)
                                {
                                    ChipRegItem cReg_Counters;
                                    cReg_Counters.fPage    = 0x00;
                                    cReg_Counters.fAddress = cReg;
                                    this->EncodeReg(cReg_Counters, cFe->getId(), cChip->getId(), cVec, true, cWrite);
                                    cRegValues[cIndx] = cReg_Counters;
                                    cIndx++;
                                }
                            }
                            // read back all the registers
                            this->ReadChipBlockReg(cVec);
                            // then decode read back registers
                            cIndx                  = 0;
                            uint16_t cCounterValue = 0;
                            size_t   cWordCounter  = 0;
                            uint32_t cDataWord     = 0x0000;
                            for(auto cReg: cRegValues)
                            {
                                uint8_t cChipId = 0;
                                bool    cFailed, cRead;
                                this->DecodeReg(cReg, cChipId, cVec[cIndx], cRead, cFailed);
                                if(cIndx % 2 == 0)
                                    cCounterValue = ((cReg.fValue & 0xFF) << 8);
                                else
                                {
                                    cCounterValue = cCounterValue | (cReg.fValue & 0xFF);
                                    cDataWord     = (cDataWord) | (cCounterValue << (cWordCounter & 0x1) * 16);
                                    if((cWordCounter & 0x1) == 1)
                                    {
                                        LOG(INFO) << BOLDBLUE << "Word#" << +cWordCounter << std::bitset<32>(cDataWord) << RESET;
                                        pData.push_back(cDataWord);
                                        cDataWord = 0x0000;
                                    }
                                    cWordCounter++;
                                }
                                cIndx++;
                            }
                        }
                        else
                        {
                            // // I2C configuration
                            // uint8_t cFreq         = 3;
                            // uint8_t cSCLDriveMode = 0;
                            //
                            uint32_t cDataWord    = 0x0000;
                            uint32_t cWordCounter = 0;
                            // uint8_t  cBaseSlaveAddrs = cWithMPA ? 0x40 : 0x20;
                            // uint8_t  cSlaveAddress   = cBaseSlaveAddrs + cChip->getId();
                            // int cPrintDebug = cWithMPA ? 250 : 25;
                            for(uint16_t cChnl = 0; cChnl < 10; cChnl++) // cChip->size()
                            {
                                // uint32_t cRow = (pPixelNum == 0 ) ? 0 : 1 + cPixNum/120 ;
                                // uint32_t cColumn = (pPixelNum == 0 ) ? 0 : 1 + cPixNum%120 ;
                                int cRowNumber       = cWithMPA ? 1 + cChnl / 120 : 1;
                                int cPixelNumber     = cWithMPA ? 1 + cChnl % 120 : 0;
                                int cBaseRegisterLSB = cWithMPA ? ((cRowNumber << 11) | (9 << 7) | cPixelNumber) : 0x0901 + cChnl;
                                int cBaseRegisterMSB = cWithMPA ? ((cRowNumber << 11) | (10 << 7) | cPixelNumber) : 0x0801 + cChnl;
                                // int cBaseRegisterLSB = cWithMPA ? ((12 + 8 * (cChnl / 120)) << 8) + 0x81 : 0x0901 + cChnl;
                                // int cBaseRegisterMSB = cWithMPA ? cBaseRegisterLSB + 128 : 0x0801 + cChnl;
                                std::vector<int> cRegs{cBaseRegisterMSB, cBaseRegisterLSB};
                                std::vector<int> cValues(0);
                                for(auto cReg: cRegs)
                                {
                                    ChipRegItem cReg_Counters_MSB;
                                    cReg_Counters_MSB.fPage    = 0x00;
                                    cReg_Counters_MSB.fAddress = cReg;
                                    cValues.push_back(ReadFERegister(cChip, cReg));
                                }

                                if(cChip->getFrontEndType() == FrontEndType::SSA)
                                {
                                    auto cReadBackMode = ReadFERegister(cChip, 0x1000);
                                    if(cReadBackMode != 1) LOG(INFO) << BOLDRED << "ReadoutMode for SSA not set to 1" << RESET;
                                }
                                uint16_t cCounterValue = ((cValues[0] & 0xFF) << 8) | (cValues[1] & 0xFF);
                                if(cChnl < 10)
                                {
                                    if(cWithMPA)
                                        LOG(DEBUG) << BOLDMAGENTA << "\t\tMPA_" << +cChip->getId() << "_Pix#" << +cChnl << " : " << +cCounterValue << " hits."
                                                   << " LSB " << +(cValues[1]) << " MSB " << +(cValues[0]) << " MSB address 0x" << std::hex << +cRegs[0] << std::dec << " LSB address 0x" << std::hex
                                                   << +cRegs[1] << std::dec << " channel number " << +cChnl << " row number " << +cRowNumber << " pixel number " << +cPixelNumber << RESET;
                                    else
                                        LOG(DEBUG) << BOLDYELLOW << "\t\t\tSSA_" << +cChip->getId() << "_Strip#" << +cChnl << " : " << +cCounterValue << " hits."
                                                   << " LSB " << +(cValues[1]) << " MSB " << +(cValues[0]) << " MSB address 0x" << std::hex << +cRegs[0] << std::dec << " LSB address 0x" << std::hex
                                                   << +cRegs[1] << std::dec << RESET;
                                }
                                cDataWord = (cDataWord) | (cCounterValue << (cWordCounter & 0x1) * 16);
                                if((cWordCounter & 0x1) == 1)
                                {
                                    pData.push_back(cDataWord);
                                    cDataWord = 0x0000;
                                }
                                cWordCounter++;
                            }
                            // if (cChip->getFrontEndType() == FrontEndType::MPA)
                            // {

                            //     // set in data vector
                            //     uint32_t cDataWord     = 0x0000;
                            //     uint32_t cWordCounter  = 0;
                            //     uint8_t  cSlaveAddress = 0x40 + cChip->getId();
                            //     for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                            //     {

                            //         // MSB, then LSB
                            //         int cBaseRegisterLSB = ((12 + 8 * (cChnl / 120)) << 8) + 0x81;
                            //         int cBaseRegisterMSB = cBaseRegisterLSB + 128;
                            //         std::vector<int> cRegs{cBaseRegisterMSB, cBaseRegisterLSB};
                            //         std::vector<int> cValues(0);
                            //         for(auto cReg: cRegs)
                            //         {

                            //             ChipRegItem cReg_Counters_MSB;
                            //             cReg_Counters_MSB.fPage    = 0x00;
                            //             cReg_Counters_MSB.fAddress = cReg;
                            //             cValues.push_back(Why(clpGBT,cSlaveAddress,cMaster,cReg_Counters_MSB.fAddress));
                            //         }
                            //         uint16_t cCounterValue = ((cValues[0] & 0xFF) << 8) | (cValues[1] & 0xFF);
                            //         if(cChnl % 100 == 0)
                            //         {
                            //              LOG(INFO) << BOLDMAGENTA << "Pix#" << +cChnl << " : " << +cCounterValue << " hits."
                            //                       << " LSB " << +(cValues[1]) << " MSB " << +(cValues[0]) << RESET;
                            //         }
                            //         cDataWord = (cDataWord) | (cCounterValue << (cWordCounter & 0x1) * 16);
                            //         if((cWordCounter & 0x1) == 1)
                            //         {
                            //             pData.push_back(cDataWord);
                            //             cDataWord = 0x0000;
                            //         }
                            //          cWordCounter++;
                            //         //cDataWord = (cDataWord) | (cCounterValue << (cWordCounter & 0x1) * 16);
                            //         //if((cWordCounter & 0x1) == 1)
                            //         //{
                            //         //   pData.push_back(cDataWord);
                            //         //   cDataWord = 0x0000;
                            //         //}
                            //     }
                            // }
                            // if (cChip->getFrontEndType() == FrontEndType::SSA)
                            // {
                            //     // set in data vector
                            //     uint32_t cDataWord     = 0x0000;
                            //     uint32_t cWordCounter  = 0;

                            //     uint8_t  cSlaveAddress = 0x20 + cChip->getId();
                            //     for(uint8_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                            //     {
                            //         // MSB, then LSB
                            //         std::vector<int> cRegs{0x0801 + cChnl, 0x0901 + cChnl};
                            //         std::vector<int> cValues(0);
                            //         for(auto cReg: cRegs)
                            //         {

                            //             ChipRegItem cReg_Counters_MSB;
                            //             cReg_Counters_MSB.fPage    = 0x00;
                            //             cReg_Counters_MSB.fAddress = cReg;

                            //             // cReg_Counters_MSB.fValue = cValue;
                            //             cValues.push_back(Why(clpGBT,cSlaveAddress,cMaster,cReg_Counters_MSB.fAddress));
                            //         }
                            //         uint16_t cCounterValue = ((cValues[0] & 0xFF) << 8) | (cValues[1] & 0xFF);
                            //         if(cChnl % 100 == 0)
                            //         {
                            //             LOG(INFO) << BOLDMAGENTA << "Strip#" << +cChnl << " : " << +cCounterValue << " hits."
                            //                       << " LSB " << +(cValues[1]) << " MSB " << +(cValues[0]) << RESET;
                            //         }
                            //         cDataWord = (cDataWord) | (cCounterValue << (cWordCounter & 0x1) * 16);
                            //         if((cWordCounter & 0x1) == 1)
                            //         {
                            //             pData.push_back(cDataWord);
                            //             cDataWord = 0x0000;
                            //         }
                            //         cWordCounter++;
                            //     } // chnl loop
                            // }
                        }
                    } // chip loop
                }     // hybrid loop
            }         // hybrid loop
        }
    }
    else
    {
        throw std::runtime_error(std::string("Trying to read MPA counters when EventType does not match..."));
    }
}

uint32_t D19cFWInterface::GetData(BeBoard* pBoard, std::vector<uint32_t>& pData)
{
    // LOG(INFO) << BOLDBLUE << "Retreiving data from the FC7..." << RESET;
    EventType cEventType = pBoard->getEventType();
    bool      cAsync     = (cEventType == EventType::SSAAS || cEventType == EventType::MPAAS || cEventType == EventType::PSAS);
    bool      cWithMPA   = false;
    bool      cWithSSA   = false;
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cFe: *cOpticalGroup)
        {
            for(auto cChip: *cFe)
            {
                cWithMPA = cWithMPA || (cChip->getFrontEndType() == FrontEndType::MPA);
                cWithSSA = cWithSSA || (cChip->getFrontEndType() == FrontEndType::SSA);
            } // chips
        }     // hybrids
    }         // opticalGroup
    uint32_t cNEvents = 0;
    uint32_t cNWords  = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
    LOG(DEBUG) << BOLDYELLOW << "D19cFWInterface::GetData have " << +cNWords << " 32-bit words in the readout" << RESET;
    if(ReadReg("fc7_daq_stat.ddr3_block.is_ddr3_type") && !cAsync)
    {
        // this->Stop();
        if(ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") == 0x1)
        {
            size_t cCounter    = 0;
            auto   cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
            do
            {
                std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
                if(cCounter % 10 == 0) LOG(DEBUG) << BOLDRED << "D19cFWInterface::GetData ReadoutReq is " << +cReadoutReq << RESET;
                cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
                cCounter++;
            } while(cReadoutReq == 0 && cCounter < 100);
            if(cReadoutReq == 0) { LOG(INFO) << BOLDRED << "Readout request 0 [i.e words missing in the readout] ... " << RESET; }
            else
                LOG(DEBUG) << BOLDGREEN << "ReadoutReq fullfilled.... " << RESET;
        }
        else
            LOG(DEBUG) << BOLDBLUE << "Data handshake not enabled" << RESET;

        // LOG(INFO) << BOLDRED << +cNWords << " words in the reaodut." << RESET;
        pData = ReadBlockRegOffsetValue("fc7_daq_ddr3", cNWords, fDDR3Offset);
        // for(auto cWord: pData) LOG(INFO) << BOLDGREEN << std::bitset<32>(cWord) << RESET;
        // figure out how many events I've got
        // LOG (INFO) << BOLDMAGENTA << "D19cFWInterface::GetData " << +pData.size() << " words in the readout." << RESET;
        cNEvents = this->CountFwEvents(pBoard, pData);
        if(cNEvents == 0) LOG(INFO) << BOLDMAGENTA << "Read back " << +pData.size() << " valid words with " << +cNWords << " in the readout." << RESET;
        // uint32_t cNtriggers = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        // if( cNEvents != cNtriggers )
        //     LOG (INFO) << BOLDRED << "[D19cFWInterface::GetData] Trigger in counter is " << +cNtriggers
        //             << " number of events in readout is " << +cNEvents << RESET;
        LOG(DEBUG) << BOLDBLUE << "D19cFWInterface has received ... " << +cNEvents << " ... events from DDR3.."
                   << " data size is " << +pData.size() << " 32 bit words." << RESET;
        // fDDR3Offset=0;
    }
    else if(cAsync)
    {
        if(cWithMPA or cWithSSA) { this->ReadPSCounters(pBoard, pData, true, true); }
        else
        {
            LOG(INFO) << BOLDRED << "Trying to read AsyncCounter wihtout an MPA/SSA.." << RESET;
            throw Exception("Trying to read AsyncCounter wihtout an MPA/SSA..stopping herer");
        }
        // uint32_t its = 0;
        // while(pData.size() == 0 and its < 5)
        // {
        //     if(its > 0) LOG(INFO) << BOLDRED << "Retrying..." << RESET;
        //     if(cWithMPA or cWithSSA) this->ReadPSCounters(pBoard, pData, false);
        //     its += 1;
        // }
        cNEvents = 1; // this->ReadReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept");
    }
    else
    {
        pData = ReadBlockRegValue("fc7_daq_ctrl.readout_block.readout_fifo", cNWords);
    }

    if(pData.size() == 0)
    {
        LOG(INFO) << BOLDRED << "After GetData have " << +pData.size() << " 32-bit words in the readout .. fail!" << RESET;
        // throw Exception("No data to retrieve..stopping here");
    }

    return cNEvents;
}
// uint32_t D19cFWInterface::ReadData(BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait)
// {
//     uint32_t cNWords        = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
//     uint32_t data_handshake = ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable");

//     bool      pFailed    = false;
//     int       cCounter   = 0;
//     EventType cEventType = pBoard->getEventType();
//     bool      cAsync     = (cEventType == EventType::SSAAS || cEventType == EventType::MPAAS || cEventType == EventType::PSAS);

//     while(cNWords == 0 && cCounter < 1000 && !cAsync)
//     {
//         cNWords = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
//         if(cNWords == 0 && cCounter % 100 == 0 && cCounter > 0)
//             LOG(INFO) << BOLDRED << "Zero events in FIFO, waiting for the triggers" << RESET;
//         else if(cNWords > 0)
//             LOG(INFO) << BOLDGREEN << "Iter#" << +cCounter << " " << +cNWords << " events in FIFO.. going to readout data" << RESET;
//         cCounter++;

//         if(!pWait) return cNWords;
//     }
//     if(cCounter == 0 && !cAsync) LOG(INFO) << BOLDGREEN << "Iter#" << +cCounter << " " << +cNWords << " words in FIFO.. going to readout data" << RESET;

//     pFailed = (!cAsync) ? (cNWords == 0 || cCounter >= 1000) : false;

//     uint32_t cNEvents        = 0;
//     uint32_t cNtriggers      = 0;
//     uint32_t cNtriggers_prev = cNtriggers;

//     if(data_handshake == 1 && !pFailed && !cAsync)
//     {
//         cNWords         = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
//         cNtriggers      = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
//         cNtriggers_prev = cNtriggers;
//         // uint32_t cNWords_prev = cNWords;
//         uint32_t cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
//         cCounter             = 0;
//         while(cReadoutReq == 0)
//         {
//             if(!pWait) { return 0; }
//             // cNWords_prev = cNWords;
//             cNtriggers_prev = cNtriggers;
//             cReadoutReq     = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
//             cNWords         = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
//             cNtriggers      = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
//             LOG(DEBUG) << BOLDBLUE << "Received " << +cNtriggers << " --- have " << +cNWords << " in the readout." << RESET;

//             if(cNtriggers == cNtriggers_prev && cCounter > 0)
//             {
//                 if(cCounter % 100 == 0) LOG(DEBUG) << BOLDRED << " ..... waiting for more triggers .... got " << +cNtriggers << " so far." << RESET;
//             }
//             cCounter++;
//             std::this_thread::sleep_for(std::chrono::microseconds(10));
//         }
//         cNWords = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");

//         // read all the words
//         cNEvents = this->GetData(pBoard, pData);
//     }
//     else if(!pFailed)
//     {
//         if(pBoard->getEventType() == EventType::ZS)
//         {
//             LOG(ERROR) << "ZS Event only with handshake!!! Exiting...";
//             exit(1);
//         }
//         cNEvents = this->GetData(pBoard, pData);
//         // read all the words
//         if(fIsDDR3Readout)
//         {
//             // readout_req high when buffer is almost full
//             uint32_t cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
//             if(cReadoutReq == 1)
//             {
//                 LOG(INFO) << BOLDGREEN << "Resetting the address in the DDR3 to zero " << RESET;
//                 fDDR3Offset = 0;
//             }
//         }
//     }

//     if(pFailed)
//     {
//         pData.clear();

//         LOG(INFO) << BOLDRED << "Re-starting the run and resetting the readout" << RESET;

//         this->Stop();
//         std::this_thread::sleep_for(std::chrono::milliseconds(500));
//         LOG(INFO) << BOLDGREEN << " ... Run Stopped, current trigger FSM state: " << +ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << RESET;

//         // RESET the readout
//         this->ResetReadout();
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));

//         this->Start();
//         std::this_thread::sleep_for(std::chrono::milliseconds(500));
//         LOG(INFO) << BOLDGREEN << " ... Run Started, current trigger FSM state: " << +ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << RESET;

//         LOG(INFO) << BOLDRED << " ... trying to read data again .... " << RESET;
//         cNEvents = this->ReadData(pBoard, pBreakTrigger, pData, pWait);
//     }
//     if(fSaveToFile) fFileHandler->setData(pData);

//     // need to return the number of events read
//     return cNEvents;
// }
uint32_t D19cFWInterface::GetTriggerState()
{
    int cState = ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");
    if(cState == 0)
        LOG(DEBUG) << "Trigger State: " << BOLDGREEN << "Idle" << RESET;
    else if(cState == 1)
        LOG(DEBUG) << "Trigger State: " << BOLDGREEN << "Running" << RESET;
    else if(cState == 2)
        LOG(DEBUG) << "Trigger State: " << BOLDGREEN << "Paused. Waiting for readout" << RESET;
    else
        LOG(WARNING) << " Trigger State: " << BOLDRED << "Unknown" << RESET;
    return cState;
}
uint32_t D19cFWInterface::ReadData(BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait)
{
    // LOG(INFO) << BOLDYELLOW << "ReadData D19cFWInterface" << RESET;
    uint32_t cNWords        = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
    uint32_t cNtriggers     = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
    uint32_t data_handshake = ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable");

    bool      pFailed    = false;
    EventType cEventType = pBoard->getEventType();
    bool      cAsync     = (cEventType == EventType::SSAAS || cEventType == EventType::MPAAS);

    // here check if the trigger state machine is running
    // if it is not .. stop it , reset and start again
    if(GetTriggerState() == 0) // 0, idle - 1 running
    {
        LOG(DEBUG) << BOLDRED << "Triggers not running.. no data to read " << RESET;
        return 0;
    }

    // don't wait
    // check what happens in system controller
    if(pWait)
    {
        // first ... wait to see 'some' triggers
        size_t   cCounter  = 0;
        uint32_t cTrigPrev = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
            cNtriggers = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
            if((1 + cCounter) % 1000 == 0) LOG(INFO) << BOLDYELLOW << "\t.. after " << +cCounter << " waits have counted " << +cNtriggers << " received by the FC7." << RESET;
            cCounter++;
        } while(cCounter < 1000 && (cTrigPrev == cNtriggers));
        cNtriggers = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        // LOG(INFO) << BOLDGREEN << "Number of triggers received = " << cNtriggers << RESET;

        // now wait until there are some words in the readout
        cCounter = 0;
        cNWords  = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
        while(cNWords == 0)
        {
            if((1 + cCounter) % 100 == 0)
                LOG(INFO) << BOLDYELLOW << "\t.. after " << +cCounter << " waits have counted " << +cNtriggers << " received by the FC7 "
                          << " and " << +cNWords << " in the readout " << RESET;
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
            cNtriggers = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
            cNWords    = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
            cCounter++;
            break;
        }
        cNtriggers = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        cNWords    = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");

        // failed to read any data
        pFailed = (!cAsync) ? (cNWords == 0) : false;
        if(pFailed) LOG(INFO) << BOLDYELLOW << "ReadData has found " << +cNtriggers << " triggers received by FC7 and " << +cNWords << " words in the readout." << RESET;
    }
    uint32_t cNEvents = 0;
    if(!pWait && data_handshake == 0)
    {
        pData.clear();
        // LOG(INFO) << BOLDMAGENTA << "D19cFWInterface::ReadData with DataHandshake OFF and no WAIT" << RESET;
        cNWords = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
        if(cNWords == 0) return 0;
        auto cNewEvents = this->GetData(pBoard, pData);
        pFailed         = (cNewEvents == 0);
        if(!pFailed) cNEvents += cNewEvents;
        // check if if the DD3 is almost full
        uint32_t cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
        if(cReadoutReq == 1) // DD3 almost full ? check this!!
        {
            LOG(INFO) << BOLDMAGENTA << "D19cFWInterface::ReadData resetting readout ... " << RESET;
            Pause();
            ResetReadout();
            Resume();
        }
    }
    else if(data_handshake == 1 && !pFailed && !cAsync) // check this properly!!
    {
        pData.clear();
        LOG(DEBUG) << BOLDMAGENTA << "D19cFWInterface::ReadData with DataHandshake ON" << RESET;
        cNWords = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
        if(cNWords == 0) return 0;

        uint32_t cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
        do
        {
            cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
        } while(cReadoutReq == 0);
        auto cNewEvents = this->GetData(pBoard, pData);
        pFailed         = (cNewEvents == 0);
        if(cReadoutReq == 1)
        {
            this->ResetReadout();
            cNEvents += cNewEvents;
        }
        // size_t   cCounter    = 0;
        // do
        // {
        //     std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
        //     std::vector<uint32_t> cData(0);
        //     auto cNewEvents = this->GetData(pBoard, cData);
        //     pFailed = (cNewEvents == 0 );
        //     if( pFailed ) continue;

        //     cNEvents += cNewEvents;
        //     std::move(cData.begin(), cData.end(), std::back_inserter(pData));
        //     cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
        //     // if(cReadoutReq == 0 && cCounter%10 == 0 )
        //     //     LOG (INFO) << BOLDMAGENTA << "\t... waiting to clear ReadoutReq .. Trial#" << +cCounter << RESET;
        //     cCounter++;
        // }while( cReadoutReq == 0 && cCounter < 1000 && !pFailed);
        // if( cReadoutReq == 1 )
        // {
        //     //LOG (INFO) << BOLDMAGENTA << "Readout request fullfilled.. reset offset" << RESET;
        //     fDDR3Offset=0;
        // }
        // uint32_t cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
        // size_t   cCounter    = 0;
        // while(cReadoutReq == 0)
        // {
        //     if(!pWait) { return 0; }

        //     cNWords = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
        //     LOG(INFO) << BOLDRED << " ..... waiting for more words .... got " << +cNWords << " so far." << RESET;
        //     cCounter++;
        //     std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
        // }
        // cNWords = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
        // LOG(DEBUG) << BOLDRED << " Final word count is " << +cNWords << " so far." << RESET;
        // // read all the words
        // cNEvents = this->GetData(pBoard, pData);
    }
    else if(!pFailed)
    {
        if(pBoard->getEventType() == EventType::ZS)
        {
            LOG(ERROR) << "ZS Event only with handshake!!! Exiting...";
            exit(1);
        }
        cNEvents = this->GetData(pBoard, pData);
        // read all the words
        if(ReadReg("fc7_daq_stat.ddr3_block.is_ddr3_type"))
        {
            // readout_req high when buffer is almost full
            uint32_t cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
            if(cReadoutReq == 1) // DD3 almost full ? check this!!
            {
                Pause();
                ResetReadout();
                Resume();
            }
        }
    }

    if(pFailed)
    {
        pData.clear();

        LOG(INFO) << BOLDRED << "Resetting the readout" << RESET;

        // this->Stop();
        // std::this_thread::sleep_for(std::chrono::milliseconds(500));
        // LOG(INFO) << BOLDGREEN << " ... Run Stopped, current trigger FSM state: " << +ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << RESET;

        // RESET the readout
        this->ResetReadout();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // this->Start();
        // std::this_thread::sleep_for(std::chrono::milliseconds(500));
        // LOG(INFO) << BOLDGREEN << " ... Run Started, current trigger FSM state: " << +ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << RESET;

        LOG(INFO) << BOLDRED << " ... trying to read data again .... " << RESET;
        cNEvents = this->ReadData(pBoard, pBreakTrigger, pData, pWait);
    }
    if(fSaveToFile) fFileHandler->setData(pData);

    // need to return t

    //    uint32_t cNEvents = 0;
    //    if(data_handshake == 1 && !pFailed && !cAsync)
    //    {
    //        LOG(DEBUG) << BOLDYELLOW << "Data handshake enabled with ReadData" << RESET;
    //        uint32_t cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
    //        size_t cCounter=0;
    //        while( cReadoutReq == 0 && cCounter < 1000 )
    //        {
    //            std::vector<uint32_t> cData(0);
    //            cNEvents += this->GetData(pBoard, cData);
    //            for(const auto data : cData) pData.emplace_back(data);
    //                std::this_thread::sleep_for(std::chrono::microseconds(10));
    //            LOG(INFO) << BOLDRED << " ..... waiting for to clear ReadoutReq... Iter#" << +cCounter << RESET;
    //            cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
    //            cCounter++;
    //        }
    // /*size_t   cCounter    = 0;
    //        while(cReadoutReq == 0)
    //        {
    //            if(!pWait) { return 0; }

    //            cNWords = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
    //            LOG(INFO) << BOLDRED << " ..... waiting for more words .... got " << +cNWords << " so far." << RESET;
    //            cCounter++;
    //            std::this_thread::sleep_for(std::chrono::microseconds(10));
    //        }
    //        cNWords = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
    //        LOG(DEBUG) << BOLDRED << " Final word count is " << +cNWords << " so far." << RESET;

    //        // read all the words
    //        cNEvents = this->GetData(pBoard, pData);*/
    //    }
    //    else if(!pFailed)
    //    {
    //        if(pBoard->getEventType() == EventType::ZS)
    //        {
    //            LOG(ERROR) << "ZS Event only with handshake!!! Exiting...";
    //            throw std::runtime_error("ZS Event can only be used with handshake");
    //        }
    //        uint32_t cNWords    = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
    //        uint32_t cNtriggers = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
    //        LOG(INFO) << BOLDGREEN << "Number of triggers received = " << cNtriggers << RESET;
    //        // LOG(INFO) << BOLDGREEN << "cNWords = " << cNWords << RESET;
    //        if(cNWords == 0) return 0;
    //        cNEvents = this->GetData(pBoard, pData);
    //        // read all the words
    //        if(fIsDDR3Readout)
    //        {
    //            // readout_req high when buffer is almost full
    //            uint32_t cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
    //            if(cReadoutReq == 1)
    //            {
    //                LOG(INFO) << BOLDGREEN << "Resetting the address in the DDR3 to zero " << RESET;
    //                fDDR3Offset = 0;
    //            }
    //        }
    //    }

    //    if(pFailed)
    //    {
    //        // pData.clear();

    //        LOG(INFO) << BOLDRED << "Re-starting the run and resetting the readout" << RESET;

    //        // this->Stop();
    //        // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    //        // LOG(INFO) << BOLDGREEN << " ... Run Stopped, current trigger FSM state: " << +ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << RESET;

    //        // RESET the readout
    //        this->ResetReadout();
    //        // std::this_thread::sleep_for(std::chrono::milliseconds(100));

    //        // this->Start();
    //        // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    //        // LOG(INFO) << BOLDGREEN << " ... Run Started, current trigger FSM state: " << +ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") << RESET;

    //        // LOG(INFO) << BOLDRED << " ... trying to read data again .... " << RESET;
    //        // cNEvents = this->ReadData(pBoard, pBreakTrigger, pData, pWait);
    //    }
    //    if(fSaveToFile) fFileHandler->setData(pData);

    // update local event counter
    fEventCounter += cNEvents;
    // need to return the number of events read
    return cNEvents;
}
void D19cFWInterface::ReadASEvent(BeBoard* pBoard, std::vector<uint32_t>& pData)
{
    uint32_t raw_mode_en = 0;
    WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", raw_mode_en);
    uint32_t ps_counters_ready = ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");
    // ps_counters_ready = ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");
    // std::cout<<"ps_counters_ready "<<ps_counters_ready<<std::endl;

    std::chrono::milliseconds cWait(10);

    uint32_t chans = 0;

    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            if(fFirmwareFrontEndType == FrontEndType::SSA) chans += NSSACHANNELS * cHybrid->size();
            if(fFirmwareFrontEndType == FrontEndType::MPA) chans += NMPACHANNELS * cHybrid->size();
        }
    }

    std::vector<uint32_t> count(chans, 0);

    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    // cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.fast_reset", 1});
    // cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.fast_orbit_reset", 1});
    this->WriteStackReg(cVecReg);

    PS_Start_counters_read();
    // ps_counters_ready = ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");

    // std::cout<<"ps_counters_ready "<<ps_counters_ready<<std::endl;

    uint32_t timeout = 0;

    while((ps_counters_ready == 0) & (timeout < 50))
    {
        std::this_thread::sleep_for(cWait);
        ps_counters_ready = ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");

        timeout += 1;
    }
    if(timeout >= 50)
    {
        std::cout << "fail" << std::endl;
        return;
    }

    if(raw_mode_en == 1)
    {
        uint32_t cycle = 0;
        for(int i = 0; i < 20000; i++)
        {
            uint32_t fifo1_word = ReadReg("fc7_daq_ctrl.physical_interface_block.fifo1_data");
            uint32_t fifo2_word = ReadReg("fc7_daq_ctrl.physical_interface_block.fifo2_data");

            uint32_t line1 = (fifo1_word & 0x0000FF) >> 0;  // to_number(fifo1_word,8,0)
            uint32_t line2 = (fifo1_word & 0x00FF00) >> 8;  // to_number(fifo1_word,16,8)
            uint32_t line3 = (fifo1_word & 0xFF0000) >> 16; //  to_number(fifo1_word,24,16)

            uint32_t line4 = (fifo2_word & 0x0000FF) >> 0; // to_number(fifo2_word,8,0)
            uint32_t line5 = (fifo2_word & 0x00FF00) >> 8; // to_number(fifo2_word,16,8)

            if(((line1 & 0x80) == 128) && ((line4 & 0x80) == 128))
            {
                uint32_t temp = ((line2 & 0x20) << 9) | ((line3 & 0x20) << 8) | ((line4 & 0x20) << 7) | ((line5 & 0x20) << 6) | ((line1 & 0x10) << 6) | ((line2 & 0x10) << 5) | ((line3 & 0x10) << 4) |
                                ((line4 & 0x10) << 3) | ((line5 & 0x80) >> 1) | ((line1 & 0x40) >> 1) | ((line2 & 0x40) >> 2) | ((line3 & 0x40) >> 3) | ((line4 & 0x40) >> 4) | ((line5 & 0x40) >> 5) |
                                ((line1 & 0x20) >> 5);
                // LOG (INFO) << BOLDBLUE <<"temp "<<temp - 1 << RESET;
                if(temp != 0)
                {
                    count[cycle] = temp - 1;

                    cycle += 1;
                }
            }
        }
    }
    else
    {
        pData = ReadBlockRegValue("fc7_daq_ctrl.physical_interface_block.fifo2_data", chans);
    }
    std::this_thread::sleep_for(cWait);
    ps_counters_ready = ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");

    if(fSaveToFile) fFileHandler->setData(pData);
}
bool D19cFWInterface::DecodeRawCounterDataPS(PSCounterData& pFeCounters, std::vector<uint8_t> pIds)
{
    std::string                                   cStartPattern = "111111111111111";
    size_t                                        cBxId         = 0;
    std::vector<std::pair<uint32_t, std::string>> cBxCars;
    auto                                          cStubBufferIter = fStubBuffer.begin();
    std::string                                   cStubPkt        = "";
    size_t                                        cPktLength      = 0;
    size_t                                        cStubPktCounter = 0;
    size_t                                        cNFEs           = 0;
    // find first packet with more than 0 stubs
    bool                       cStartPatternFound = true;
    size_t                     cNcountersDecoded  = 0;
    size_t                     cMaxSSACounters    = NSSACHANNELS - 1;
    size_t                     cMaxMPACounters    = NMPACOLS * NSSACHANNELS - 1;
    size_t                     cMaxCountersSize   = (cMaxSSACounters + cMaxMPACounters) * pIds.size();
    std::map<uint8_t, uint8_t> cMPAdoneMap;
    std::map<uint8_t, uint8_t> cSSAdoneMap;
    for(auto cId: pIds)
    {
        cMPAdoneMap[cId] = 0;
        cSSAdoneMap[cId] = 0;
    }
    bool cStubZeroFound = false;
    do
    {
        // if( cStubPktCounter == 1 ) cMaxCountersSize = ( cNFEs == 0 ) ? (16*120 + 120 ) *8 : (16*120 + 120 ) *cNFEs;
        for(size_t cClk = 0; cClk < 8; cClk++)
        {
            LOG(DEBUG) << BOLDMAGENTA << "Bx" << +cBxId << " : " << std::bitset<6>(*cStubBufferIter & 0x3F) << RESET;
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
                // if( cNstubs < 8 ) //there should be at most 8 stubs here
                // if( cNstubs >= pIds.size() && cNstubs < 8 ) //there should be at most 8 stubs here
                //{
                LOG(DEBUG) << BOLDYELLOW << cStream.str() << " : " << RESET;
                for(size_t cStubId = 0; cStubId < cNstubs; cStubId++)
                {
                    std::vector<std::pair<std::string, uint8_t>> cStubFlds;
                    cStubFlds.push_back(std::make_pair("Offset", 3));
                    cStubFlds.push_back(std::make_pair("FeId", 3));
                    cStubFlds.push_back(std::make_pair("Stub", 15));
                    size_t cMinPktLength = 3 + 3 + 15;
                    if(cStubPkt.length() < cMinPktLength)
                    {
                        LOG(DEBUG) << BOLDMAGENTA << "!!" << cStubPkt.length() << " -- " << cMinPktLength << RESET;
                        continue;
                    }
                    std::stringstream cStubOutput;
                    int               cFeId        = -1;
                    int               cDecodedFeId = cFeId;
                    for(auto cFld: cStubFlds)
                    {
                        if(cStubZeroFound) continue;
                        auto cSubStr = cStubPkt.substr(cShft, cFld.second);
                        LOG(DEBUG) << BOLDYELLOW << cFld.first << ":" << cSubStr << RESET;
                        if(cFld.first == "FeId")
                        {
                            cDecodedFeId = std::stoi(cSubStr, 0, 2);
                            cFeId        = cDecodedFeId;
                        }
                        if(cFld.first == "Stub" && cSubStr != "000000000000000")
                        {
                            if(cStubPktCounter == 0)
                            {
                                cStartPatternFound = cStartPatternFound && (cSubStr == cStartPattern); // first stub needs to be all 1's
                                cNFEs++;
                            }
                            if(cStartPatternFound && cStubPktCounter > 0)
                            {
                                // if( std::find( pIds.begin(), pIds.end() , cDecodedFeId ) == pIds.end() ) continue;
                                uint16_t cCounterValue = std::stoi(cSubStr.substr(8, 6) + cSubStr.substr(0, 7), 0, 2) - 1;
                                auto     cFindCounters = pFeCounters.find(cFeId);
                                if(cFindCounters == pFeCounters.end()) // for the MPA
                                {
                                    std::vector<uint16_t> cCountersThisFe;
                                    cCountersThisFe.clear();
                                    pFeCounters[cFeId] = cCountersThisFe;
                                }
                                else
                                {
                                    // MPA done .. can prepare SSA
                                    if(cMPAdoneMap[cDecodedFeId] == 1)
                                    {
                                        cFeId         = cDecodedFeId | (1 << 3);
                                        cFindCounters = pFeCounters.find(cFeId);
                                        if(cFindCounters == pFeCounters.end()) // for the MPA
                                        {
                                            LOG(DEBUG) << BOLDYELLOW << "Starting to fill SSA counters from FeId" << +cDecodedFeId << RESET;
                                            std::vector<uint16_t> cCountersThisFe;
                                            cCountersThisFe.clear();
                                            pFeCounters[cFeId] = cCountersThisFe;
                                        }
                                    }
                                }
                                uint8_t cWithMPA = (cFeId >> 3 == 0);
                                if(cWithMPA == 1 && cMPAdoneMap[cDecodedFeId] == 0)
                                {
                                    // if( pFeCounters[cFeId].size() == 0 || pFeCounters[cFeId].size() == cMaxMPACounters-1)
                                    if(pFeCounters[cFeId].size() % 75 == 0 && pFeCounters[cFeId].size() > 0)
                                        LOG(DEBUG) << BOLDMAGENTA << "MPA [FeId " << +cFeId << " ] counter#" << +pFeCounters[cFeId].size() << " --> " << cCounterValue << RESET;
                                    pFeCounters[cFeId].push_back(cCounterValue);
                                    if(pFeCounters[cFeId].size() == cMaxMPACounters) cMPAdoneMap[cDecodedFeId] = 1;
                                }

                                if(cWithMPA == 0 && cMPAdoneMap[cDecodedFeId] == 1 && cSSAdoneMap[cDecodedFeId] == 0)
                                {
                                    // if( pFeCounters[cFeId].size() == 0 )//|| pFeCounters[cFeId].size() == cMaxSSACounters-1)
                                    if(pFeCounters[cFeId].size() % 10 == 0)
                                        LOG(DEBUG) << BOLDYELLOW << "SSA [FeId " << +cFeId << " ] counter#" << +pFeCounters[cFeId].size() << " --> " << cCounterValue << RESET;
                                    pFeCounters[cFeId].push_back(cCounterValue);
                                    if(pFeCounters[cFeId].size() == cMaxSSACounters) cSSAdoneMap[cDecodedFeId] = 1;
                                }
                            }
                        }
                        if(cFld.first == "Stub" && cSubStr == "000000000000000")
                        {
                            cStubZeroFound = true;
                            LOG(DEBUG) << BOLDRED << "Found a stub from MPA that is all 00s.. this should not happen and is a decoder problem.. will look into it!!" << RESET;
                        }
                        cShft += cFld.second;
                    }
                    if(cStubZeroFound) continue;
                }
                //}
                cStubPktCounter += (cNstubs > 0) ? 1 : 0;
                cPktLength = 0;
                cStubPkt   = "";
            }
            cStubBufferIter++;
        }
        // check how many counters have been decoded
        cNcountersDecoded = 0;
        for(auto cId: pIds)
        {
            // auto cFindCounters = pFeCounters.find(cId);
            // if( cFindCounters == pFeCounters.end() ) continue;

            cNcountersDecoded += pFeCounters[cId].size();
        }
        // expect this to increment every 8 Bx?
        LOG(DEBUG) << BOLDYELLOW << "Decoded " << cNcountersDecoded << " counters from enabled FEs" << RESET;
        cBxId++;
    } while(cStubBufferIter < fStubBuffer.end() && cNcountersDecoded < cMaxCountersSize && !cStubZeroFound);

    bool cAllCountersReceived = true;
    for(auto cId: pIds)
    {
        auto cFindCounters = pFeCounters.find(cId);
        if(cFindCounters == pFeCounters.end())
        {
            cAllCountersReceived = false;
            continue;
        }

        auto& cCounters_MPA = pFeCounters[cId];
        auto& cCounters_SSA = pFeCounters[(1 << 3) | cId];

        LOG(DEBUG) << BOLDMAGENTA << "Mean hit count - MPA#" << +cId << " : " << std::accumulate(cCounters_MPA.begin(), cCounters_MPA.end(), 0.) / cCounters_MPA.size() << RESET;
        LOG(DEBUG) << BOLDYELLOW << "Mean hit count - SSA#" << +cId << " : " << std::accumulate(cCounters_SSA.begin(), cCounters_SSA.end(), 0.) / cCounters_SSA.size() << RESET;
        // if( pFeCounters[(1<<3)|cId].size() != cMaxSSACounters )
        //{
        LOG(DEBUG) << BOLDYELLOW << "FECounters from FeId" << +cId << " --  from MPA " << pFeCounters[cId].size() << " --  from SAA " << pFeCounters[(1 << 3) | cId].size() << RESET;
        //}
        cAllCountersReceived = cAllCountersReceived && (cMPAdoneMap[cId] == 1 && cSSAdoneMap[cId] == 1);
    }
    return cStartPatternFound && cAllCountersReceived;
}
bool D19cFWInterface::CheckStartPattern()
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
    do
    {
        for(size_t cClk = 0; cClk < 8; cClk++)
        {
            LOG(DEBUG) << BOLDMAGENTA << "Bx" << +cBxId << " : " << std::bitset<6>(*cStubBufferIter & 0x3F) << RESET;
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
                cStubFlds.push_back(std::make_pair("FeId", 3));
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
                    if(cStartPatternFound)
                        LOG(DEBUG) << BOLDGREEN << "D19cFWInterface::CheckStartPattern CheckForStartPattern from PS counters - Bx " << std::stoi(cHdrVals[2], 0, 2) << "\t stub#" << +cStubId << " : "
                                   << cStubOutput.str() << RESET;
                    else
                        LOG(DEBUG) << BOLDRED << "Bx " << std::stoi(cHdrVals[2], 0, 2) << "\t stub#" << +cStubId << " : " << cStubOutput.str() << RESET;
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
bool D19cFWInterface::GetCounterData(uint8_t pRawMode, size_t pChipId, size_t pHybridId)
{
    bool                                          cSuccess = false;
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    fFastCommandDuration   = 0;
    uint32_t cIteration    = 0;
    auto     cDecoderState = this->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.state");
    // wait until fifo is ready to start readout of counters
    do
    {
        LOG(DEBUG) << BOLDMAGENTA << "\t\t..D19cFWInterface::WaitForData DECODER State: " << +cDecoderState << "Running.. .Iteration#" << +cIteration << RESET;
        cDecoderState = this->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.state");
        cIteration++;
    } while(cDecoderState != 0); // idle state is 0
    LOG(DEBUG) << BOLDMAGENTA << "Decoder in IDLE state after " << +cIteration << " iterations." << RESET;

    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    size_t      cNbits    = 200e3 * 8 * 6;
    size_t      cNWords   = cNbits / 32; // number of 32-bit words to read from DDR3
    auto        cData     = ReadBlockRegOffsetValue("fc7_daq_ddr3", cNWords, 0);
    std::string cDataWord = "";
    size_t      cIndx     = 0;
    auto        cIter     = cData.begin();
    uint16_t    cBxId     = 0;
    if(pRawMode == 0)
    {
        do
        {
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
        do
        {
            std::stringstream cPacket512;
            for(uint8_t cFrag = 0; cFrag < 256 / 32; cFrag++)
            {
                if(cIter >= cData.end()) break;
                cPacket512 << std::bitset<32>(*cIter);
                // LOG (INFO) << BOLDGREEN << std::bitset<32>(*cIter);
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
                    // cBxCounter.push_back( static_cast<uint32_t>( std::stoi( cPacket512.str().substr(0,32), 0, 2 ) ) );
                }
            }
            cIndx++;
        } while(cIter < cData.end());
        cSuccess = CheckStartPattern();
        // now grab counter information
    }
    return cSuccess;
}
bool D19cFWInterface::WaitForData(BeBoard* pBoard)
{
    // LOG(INFO) << BOLDBLUE << "Waiting for data from the FC7.... Attempt#" << fReadoutAttempts << RESET;

    bool cFailed        = false;
    auto cNevents       = this->ReadReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept");
    auto cTriggerSource = this->ReadReg("fc7_daq_cnfg.fast_command_block.trigger_source"); // trigger source
    // cTriggerSource = 42;
    // in kHz .. if external trigger assume 1 Hz or TP assume lowest possible rate
    auto     cTriggerRate          = (cTriggerSource == 5 || cTriggerSource == 6) ? (1e-6) : this->ReadReg("fc7_daq_cnfg.fast_command_block.user_trigger_frequency");
    uint32_t cTimeSingleTrigger_us = std::ceil(1.5 / (cTriggerRate));
    auto     cMultiplicity         = this->ReadReg("fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");

    EventType                                     cEventType = pBoard->getEventType();
    bool                                          cAsync     = (cEventType == EventType::SSAAS || cEventType == EventType::MPAAS || cEventType == EventType::PSAS);
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNevents * (cMultiplicity + 1)});

    if(cEventType == EventType::PSAS)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                LOG(DEBUG) << BOLDBLUE << "Capturing RAW PS counter data in uDTC for hybrid#" << +cHybrid->getId() << RESET;
                // clear stub buffer
                fStubBuffer.clear();
                // count number of chips expected
                this->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
                this->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);

                // make sure uDTC vetos fast commands to CIC in this mode
                auto cVetoCIC = this->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto");
                this->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.select_even", 0x1);

                uint8_t cTriggerForPS = 12;
                LOG(DEBUG) << BOLDMAGENTA << "CIC fast command VETO set to " << +this->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto") << RESET;
                LOG(DEBUG) << BOLDMAGENTA << "Injecting " << +cNevents << " times." << RESET;
                LOG(DEBUG) << BOLDMAGENTA << "Trigger multiplicity was set to " << +cMultiplicity << RESET;
                cVecReg.clear();

                cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", 0});
                cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNevents});
                cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
                cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_timeout_enable", 0});
                cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerForPS});
                this->ReconfigureTriggerFSM(cVecReg);
                cVecReg.clear();

                std::vector<uint8_t> cFeMappingPSR{6, 7, 3, 2, 1, 0, 4, 5}; //  Index Hybrid FE Id , Value CIC FE Id
                std::vector<uint8_t> cFeMappingPSL{1, 0, 4, 5, 6, 7, 3, 2}; // Index hybrid FE Id , Value CIC FE Id
                std::vector<uint8_t> cMapping      = (cHybrid->getId() % 2 == 0) ? cFeMappingPSR : cFeMappingPSL;
                uint32_t             cPSModuleId   = (pBoard->getId() << 16) | (cOpticalGroup->getId() << 8) | cHybrid->getId();
                auto                 cPSModuleIter = fPSModulesCounterData.find(cPSModuleId);
                // bool cAllCountersReceived=false;
                // uint16_t cNominalOffset = 120*16*8-3*8+1;
                uint8_t cReadoutAttempt        = 0;
                bool    cCounterReadoutSuccess = false;
                uint8_t cMaxReadoutAttempts    = 5;
                do
                {
                    if(cPSModuleIter != fPSModulesCounterData.end())
                    {
                        PSCounterData cDummy;
                        cDummy.clear();
                        fPSModulesCounterData[cPSModuleId] = cDummy;
                    }

                    this->ResetTriggerFSM();
                    this->Stop();

                    this->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 0x1);
                    // configure DDR3 readout for counters
                    this->WriteReg("fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 0x1);
                    this->WriteReg("fc7_daq_cnfg.ddr3_debug.stub_enable", 0x0);
                    // configure raw mode
                    this->WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", 1);

                    size_t   cAttempt = 0;
                    uint32_t cStartReceived;
                    do
                    {
                        auto cTriggerState = this->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");
                        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
                        LOG(DEBUG) << BOLDBLUE << "Attempt#" << +cAttempt << " Async SSA [trigger source == " << +this->ReadReg("fc7_daq_cnfg.fast_command_block.trigger_source")
                                   << " ] [ number of injections is " << +this->ReadReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept") << " ]"
                                   << "Trigger state before start is " << +cTriggerState << RESET;

                        this->Start();
                        uint32_t cIteration = 0;
                        do
                        {
                            auto cNtriggers = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
                            cTriggerState   = this->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");
                            LOG(DEBUG) << BOLDBLUE << "D19cFWInterface::WaitForData TriggerSource 12 Trigger State: " << +cTriggerState << "Running.. .Iteration#" << +cIteration << " ... received "
                                       << +cNtriggers << " triggers." << RESET;
                            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
                            cIteration++;
                        } while(cTriggerState && cIteration < 1000);
                        uint32_t cNInjections = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
                        LOG(DEBUG) << BOLDBLUE << "Trigger state after end is " << +cTriggerState << " - fast command core counted " << cNInjections << " injections." << RESET;

                        cStartReceived         = this->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.received_start");
                        cCounterReadoutSuccess = (GetCounterData(1, cHybrid->getId(), 0));
                        if(!cCounterReadoutSuccess) LOG(DEBUG) << BOLDRED << "Counter readout failed.. to receive start pattern .. will try again..." << RESET;
                        cAttempt++;
                    } while(!cCounterReadoutSuccess && cAttempt < 10);
                    if(!cCounterReadoutSuccess)
                    {
                        LOG(INFO) << BOLDRED << "Failed to receive start pattern from Hybrid#" << +cHybrid->getId() << " after 10 attempts" << RESET;
                        throw Exception("Too many failures when attempting to receive start pattern from MPAs..something is wrong!");
                    }
                    else
                        LOG(DEBUG) << BOLDMAGENTA << "PS data capture block received start signal after " << +cStartReceived << " 40 MHz clock cycles."
                                   << " [ readout attempt#" << +(cAttempt - 1) << " ]" << RESET;

                    // reconfigure original veto
                    this->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", cVetoCIC);
                    // disable DDR3 dump of counters
                    this->WriteReg("fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 0x0);
                    this->WriteReg("fc7_daq_cnfg.ddr3_debug.stub_enable", 0x0);

                    // decode raw counter data
                    // expected number of stubs
                    std::vector<uint8_t> pIds(0);
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
                        pIds.push_back(cMapping[cChip->getId() % 8]);
                        // cNFEs += (cChip->getFrontEndType()==FrontEndType::MPA)?1:0;
                    }
                    cPSModuleIter->second.clear();
                    cCounterReadoutSuccess = DecodeRawCounterDataPS(cPSModuleIter->second, pIds);
                    if(!cCounterReadoutSuccess) LOG(INFO) << BOLDRED << "Not all counters from all FEs have been received.. trying again.." << RESET;
                    cReadoutAttempt++;
                } while(!cCounterReadoutSuccess && cReadoutAttempt < cMaxReadoutAttempts);
                if(!cCounterReadoutSuccess)
                {
                    LOG(INFO) << BOLDRED << "Failed to receive all counters from Hybrid#" << +cHybrid->getId() << " after " << +(1 + cMaxReadoutAttempts) << " attempts" << RESET;
                    // throw Exception("Too many failures when attempting to read-back all PS counters from a hybrid..something is wrong!");
                }

                // now add caveat for MPA1/SSA 1
                for(auto& cFECounters: cPSModuleIter->second)
                {
                    auto    cFeId    = cFECounters.first;
                    uint8_t cFromSSA = (cFeId >> 3);
                    if(cFECounters.second.size() == 0) continue;

                    for(auto cChip: *cHybrid)
                    {
                        auto& cIdCIC = cMapping[cChip->getId() % 8];
                        if(cFromSSA && cChip->getFrontEndType() == FrontEndType::MPA) continue;
                        if(!cFromSSA && cChip->getFrontEndType() == FrontEndType::SSA) continue;
                        if((cFeId & 0x7) != cIdCIC) continue;

                        // SSA1 last strip is missing from the fast counter readout
                        // MPA1 first pixel is 0 when read over the fast counter readout
                        // MPA1 last pixel is missing when read over the fast counter readout
                        std::vector<uint16_t> cChnls(0);
                        cChnls.push_back((cChip->getFrontEndType() == FrontEndType::MPA) ? 0 : 120 - 1);
                        if(cChip->getFrontEndType() == FrontEndType::MPA) cChnls.push_back(120 * 16 - 1);
                        std::vector<uint16_t> cCounterValues(0);
                        for(auto cChnl: cChnls)
                        {
                            int              cRowNumber       = (cChip->getFrontEndType() == FrontEndType::MPA) ? 1 + cChnl / 120 : 1;
                            int              cPixelNumber     = (cChip->getFrontEndType() == FrontEndType::MPA) ? 1 + cChnl % 120 : 0;
                            int              cBaseRegisterLSB = (cChip->getFrontEndType() == FrontEndType::MPA) ? ((cRowNumber << 11) | (9 << 7) | cPixelNumber) : 0x0901 + cChnl;
                            int              cBaseRegisterMSB = (cChip->getFrontEndType() == FrontEndType::MPA) ? ((cRowNumber << 11) | (10 << 7) | cPixelNumber) : 0x0801 + cChnl;
                            std::vector<int> cRegs{cBaseRegisterMSB, cBaseRegisterLSB};
                            std::vector<int> cValues(0);
                            for(auto cReg: cRegs)
                            {
                                ChipRegItem cReg_Counters_MSB;
                                cReg_Counters_MSB.fPage    = 0x00;
                                cReg_Counters_MSB.fAddress = cReg;
                                cValues.push_back(ReadFERegister(cChip, cReg));
                            }
                            cCounterValues.push_back(((cValues[0] & 0xFF) << 8) | (cValues[1] & 0xFF));
                            LOG(DEBUG) << BOLDYELLOW << "Hit counter from Chnl#" << +cChnl << " read-back over I2C .. value is " << cCounterValues[cCounterValues.size() - 1] << RESET;
                        }

                        // MPA1 first pixel is 0 when read over the fast counter readout
                        // MPA1 last pixel is missing when read over the fast counter readout
                        if(cChip->getFrontEndType() == FrontEndType::MPA)
                        {
                            cFECounters.second[0] = cCounterValues[0];
                            cFECounters.second.push_back(cCounterValues[1]);
                            // LOG (INFO) << BOLDMAGENTA << "ROC#" << +cChip->getId() << " read-back " << cFECounters.second.size() << " hit counters." << RESET;
                        }
                        // SSA1 last strip is missing from the fast counter readout
                        else
                        {
                            cFECounters.second.push_back(cCounterValues[0]);
                            LOG(DEBUG) << BOLDYELLOW << "ROC#" << +cChip->getId() << " read-back " << cFECounters.second.size() << " hit counters - last counter (I2C) is " << cCounterValues[0]
                                       << " - last counter stub lines is " << cFECounters.second[cFECounters.second.size() - 1] << RESET;
                        }
                        // LOG (INFO) << BOLDBLUE << "ROC#" << +cChip->getId() << " read-back " << cFECounters.second.size() << " hit counters." << RESET;
                        // LOG (DEBUG) << BOLDYELLOW << "ROC#" << +cChip->getId() << " Id in CIC should be " << +cIdCIC << " " << cFECounters.second.size() << " counters read back over stub lines" <<
                        // RESET;
                    }
                }
                // now check SSA counters
                bool   cCheckSSAs      = false;
                size_t cMaxSSACounters = NSSACHANNELS;
                for(auto& cFECounters: cPSModuleIter->second)
                {
                    auto    cFeId    = cFECounters.first & 0x7;
                    uint8_t cFromSSA = (cFECounters.first >> 3);
                    if(cFECounters.second.size() == 0) continue;

                    // only check SSAs
                    if(cFromSSA == 0) continue;
                    for(auto cChip: *cHybrid)
                    {
                        auto& cIdCIC = cMapping[cChip->getId() % 8];
                        if(cChip->getFrontEndType() == FrontEndType::MPA) continue;
                        if(cFeId != cIdCIC) continue;

                        bool cReadI2C = cCheckSSAs || cFECounters.second.size() != cMaxSSACounters;
                        if(!cReadI2C) continue;

                        LOG(DEBUG) << BOLDYELLOW << "CIC FeId " << +cFeId << " SSA Id on hybrid" << +(cChip->getId() % 8) << " -- id from map " << +cIdCIC << RESET;
                        std::vector<float>                         cDifference(0);
                        std::vector<float>                         cI2C(0);
                        std::vector<float>                         cSLVS(0);
                        std::vector<std::pair<uint16_t, uint16_t>> cMissed(0);
                        for(uint8_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                        {
                            int                   cRowNumber       = (cChip->getFrontEndType() == FrontEndType::MPA) ? 1 + cChnl / 120 : 1;
                            int                   cPixelNumber     = (cChip->getFrontEndType() == FrontEndType::MPA) ? 1 + cChnl % 120 : 0;
                            int                   cBaseRegisterLSB = (cChip->getFrontEndType() == FrontEndType::MPA) ? ((cRowNumber << 11) | (9 << 7) | cPixelNumber) : 0x0901 + cChnl;
                            int                   cBaseRegisterMSB = (cChip->getFrontEndType() == FrontEndType::MPA) ? ((cRowNumber << 11) | (10 << 7) | cPixelNumber) : 0x0801 + cChnl;
                            std::vector<uint16_t> cI2CVals(0);
                            for(int cAttempt = 0; cAttempt < 1; cAttempt++)
                            {
                                std::vector<int> cRegs{cBaseRegisterMSB, cBaseRegisterLSB};
                                std::vector<int> cValues(0);
                                for(auto cReg: cRegs)
                                {
                                    ChipRegItem cReg_Counters_MSB;
                                    cReg_Counters_MSB.fPage    = 0x00;
                                    cReg_Counters_MSB.fAddress = cReg;
                                    cValues.push_back(ReadFERegister(cChip, cReg));
                                }
                                cI2CVals.push_back(((cValues[0] & 0xFF) << 8) | (cValues[1] & 0xFF));
                                if(cChnl == 0) LOG(DEBUG) << BOLDYELLOW << cI2CVals[cI2CVals.size() - 1] << RESET;
                            }
                            uint16_t cCounterI2C = cI2CVals[0];
                            cI2C.push_back(cCounterI2C);
                            cSLVS.push_back(cFECounters.second[cChnl]);
                            if(cFECounters.second[cChnl] < cNevents)
                            {
                                std::pair<uint16_t, uint16_t> cPr;
                                cPr.first  = cCounterI2C;
                                cPr.second = cFECounters.second[cChnl];
                                cMissed.push_back(cPr);
                            }
                            if(cCounterI2C != cFECounters.second[cChnl])
                            {
                                LOG(DEBUG) << BOLDRED << "SSA#" << +cChip->getId() << " Mismatch in counter#" << +cChnl << " : " << cFECounters.second[cChnl] << " , " << cCounterI2C << RESET;
                                cDifference.push_back(cCounterI2C - cFECounters.second[cChnl]);
                                cFECounters.second[cChnl] = cCounterI2C;
                            }
                            else
                            {
                                LOG(DEBUG) << BOLDGREEN << "SSA#" << +cChip->getId() << " Match in counter#" << +cChnl << " : " << cFECounters.second[cChnl] << " , " << cCounterI2C << RESET;
                            }
                        }
                        if(cDifference.size() > 0)
                        {
                            float cMeanMismatch = std::accumulate(cDifference.begin(), cDifference.end(), 0.) / cDifference.size();
                            float cI2CAvg       = std::accumulate(cI2C.begin(), cI2C.end(), 0.) / cI2C.size();
                            float cSLVSCAvg     = std::accumulate(cSLVS.begin(), cSLVS.end(), 0.) / cSLVS.size();
                            LOG(DEBUG) << BOLDRED << "\t\t..CIC FeId " << +cFeId << " SSA Id on hybrid" << +(cChip->getId() % 8) << " decoded " << cFECounters.second.size()
                                       << " counters from SLVS lines.."
                                       << " -- found " << +cDifference.size() << " mismatches between stub lines and I2C"
                                       << " mean mismatch [I2C - SLVS] " << cMeanMismatch << " mean I2C value " << cI2CAvg << " mean slvs value " << cSLVSCAvg
                                       << " number of times I've seen a count < Ninjected " << cMissed.size() << RESET;
                            for(auto cLowerVal: cMissed) LOG(DEBUG) << BOLDRED << "\t\t.. counted [I2C] " << cLowerVal.first << " [SLVS] " << cLowerVal.second << " injected " << cNevents << RESET;
                        }
                    }
                }

                // check that counter information is correct
                // bool cAllFound=true;
                for(auto cFECounters: cPSModuleIter->second)
                {
                    auto    cFeId    = cFECounters.first;
                    uint8_t cFromSSA = (cFeId >> 3);

                    // if( !cAllFound ) continue;
                    for(auto cChip: *cHybrid)
                    {
                        auto& cIdCIC = cMapping[cChip->getId() % 8];
                        if(cFromSSA && cChip->getFrontEndType() == FrontEndType::MPA) continue;
                        if(!cFromSSA && cChip->getFrontEndType() == FrontEndType::SSA) continue;
                        if((cFeId & 0x7) != cIdCIC) continue;

                        if(cChip->getFrontEndType() == FrontEndType::MPA)
                        {
                            LOG(DEBUG) << BOLDMAGENTA << "ROC#" << +cChip->getId() << " read-back " << cFECounters.second.size() << " hit counters." << RESET;
                            // if( cFECounters.second.size() != cMaxMPACounters ) cAllFound = false;
                        }
                        // SSA1 last strip is missing from the fast counter readout
                        else
                        {
                            LOG(DEBUG) << BOLDYELLOW << "ROC#" << +cChip->getId() << " read-back " << cFECounters.second.size() << " hit counters." << RESET;
                            // if( cFECounters.second.size() != cMaxSSACounters ) cAllFound = false;
                        }
                    }
                }

                // configure DDR3 readout for counters
                this->WriteReg("fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 0x0);
                this->WriteReg("fc7_daq_cnfg.ddr3_debug.stub_enable", 0x0);
                // configure raw mode
                this->WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", 0);
            }
        }
    }
    else if(cAsync && cTriggerSource == 3)
    {
        this->ReconfigureTriggerFSM(cVecReg);
        cVecReg.clear();

        LOG(INFO) << BOLDBLUE << "Async SSA [trigger source == 3]" << RESET;
        LOG(INFO) << BOLDBLUE << "Going to open shutter for " << +cNevents << " ms." << RESET;
        // resync
        this->ChipReSync();
        // clear counters
        this->PS_Clear_counters(fFastCommandDuration);
        // open shutter
        this->PS_Open_shutter(fFastCommandDuration);
        // sleep
        std::this_thread::sleep_for(std::chrono::microseconds(cNevents));
        // close shutter
        this->PS_Close_shutter(fFastCommandDuration);
    }
    // not async antenna trigger
    else if(cTriggerSource != 10 && cTriggerSource != 42)
    {
        // configure trigger
        // data handshake has to be enabled in this mode
        // read data handshake mode
        cVecReg.push_back({"fc7_daq_cnfg.readout_block.packet_nbr", cNevents * (cMultiplicity + 1) - 1});
        cVecReg.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x1});
        // test pulse and async
        // just make sure that the fast resync
        // and the shutter are disabled
        // and the L1 are disabled
        // really only want to inject
        if(cTriggerSource == 6 && cAsync)
        {
            LOG(DEBUG) << BOLDBLUE << "Async SSA [trigger source == 6]" << RESET;
            cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", 0});
            cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_l1a", 0});
            cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_shutter", 0});
            cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset", 1});
            cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", 1});
            cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", 100});
        }
        this->ReconfigureTriggerFSM(cVecReg);
        cVecReg.clear();
        if(cTriggerSource == 6 && cAsync)
        {
            this->PS_Clear_counters(fFastCommandDuration);
            this->PS_Open_shutter(fFastCommandDuration);
        }
        // start triggering machine which will collect N events
        LOG(DEBUG) << BOLDBLUE << "Starting to send triggers with uDTC FSM" << RESET;
        this->Start();
        if(!cAsync)
        {
            bool cCountTriggers = true;
            // send triggers until the readout request flag is '1'
            uint32_t cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
            uint32_t cNtriggers  = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
            uint32_t cNWords     = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");

            uint32_t cTimeoutValue = 100;
            if(cCountTriggers) // use trigger_in_counter to check state of trigger FSM
            {
                // wait until all triggers received
                uint32_t cNtriggersPrev = cNtriggers;
                size_t   cFoundSame     = 0;
                size_t   cCounter       = 0;
                do
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
                    cNtriggers = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
                    cFoundSame += (cNtriggers == cNtriggersPrev) ? 1 : 0;
                    cNtriggersPrev = cNtriggers;
                    if(cCounter % 100 == 0) LOG(DEBUG) << BOLDRED << "D19cFWInterface::WaitForData Number of triggers received is " << +cNtriggers << RESET;
                    cCounter++;
                } while(cNtriggers < cNevents * (1 + cMultiplicity) && cFoundSame < cTimeoutValue);
                cFailed = !(cNtriggers >= cNevents * (1 + cMultiplicity));
                if(cFailed)
                {
                    auto cState = this->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");
                    LOG(INFO) << BOLDRED << "Trigger FSM failed to receive all triggers .. expected " << +cNevents * (1 + cMultiplicity) << " and received " << +cNtriggers << " FSM state is "
                              << +cState << " .. re-trying" << RESET;
                }
                if(!cFailed)
                {
                    // wait until words in the readout have stopped inreasing
                    // LOG (INFO) << BOLDMAGENTA << "D19cFWInterface::WaitForData Now checking words from the FC7" << RESET;
                    cNWords                 = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
                    uint32_t cNWordsPrev    = cNWords;
                    bool     cStopIncrement = false;
                    cCounter                = 0;
                    do
                    {
                        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
                        cNWords        = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
                        cStopIncrement = (cNWords == cNWordsPrev);
                        // cNWordsPrev    = cNWords;
                        // if(cCounter % 100 == 0) LOG(INFO) << BOLDRED << "D19cFWInterface::WaitForData Number of words received is " << +cNWords << RESET;
                        cCounter++;
                    } while(!cStopIncrement);
                    LOG(DEBUG) << BOLDGREEN << "Trigger FSM received all triggers .. expected " << +cNevents * (1 + cMultiplicity) << " and received " << +cNtriggers << " .. continuing" << RESET;
                    LOG(DEBUG) << BOLDGREEN << "Trigger to accept was = " << this->ReadReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept") << RESET;
                }
            }
            else // send triggers until the readout request flag is '1'
            {
                uint32_t cTimeoutCounter = 0;
                // uint32_t cFailures       = 0;
                uint32_t cPause           = cNevents * static_cast<uint32_t>(cTimeSingleTrigger_us);
                uint32_t cNWords_previous = cNWords;
                uint32_t cAttempt         = 0;
                // also possible to keep counting until trigger_in_counter has stopped incrementing
                // try this
                do
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(cPause));

                    cNtriggers  = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
                    cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
                    cNWords     = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
                    cTimeoutCounter += ((cNWords == 0 || (cNWords - cNWords_previous) == 0)) ? 1 : 0;
                    if((cNWords == 0 || (cNWords - cNWords_previous) == 0))
                        LOG(INFO) << MAGENTA << "Waiting for data.. attempt#" << +cAttempt << " ... ReadoutReq," << cReadoutReq << " Ntriggers," << cNtriggers << " NWords," << cNWords
                                  << " [ timeout ==  " << +cTimeoutValue << " ]" << RESET;
                    // cFailures += ((cNtriggers == 0));// || ( (cNWords_previous==cNWords) &&cReadoutReq==0) );
                    cNWords_previous = cNWords;
                    cAttempt++;
                } while(cReadoutReq == 0 && (cTimeoutCounter < cTimeoutValue)); // && (cFailures<5));
                // fails if either one of these is true
                cFailed = (cNWords == 0 || cTimeoutCounter >= cTimeoutValue);
                // pFailed = (cReadoutReq == 0) || (cNWords == 0);
                // pFailed = ((cReadoutReq == 0 && cNtriggers < cNevents * (cMultiplicity + 1)) || (cNWords == 0));
            }
            cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
            cNtriggers  = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
            cNWords     = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");

            // LOG(INFO) << MAGENTA << "cReadoutReq " << +cReadoutReq << RESET;
            // LOG(INFO) << MAGENTA << "cNtriggers " << +cNtriggers << RESET;
            // LOG(INFO) << MAGENTA << "cNWords " << +cNWords << RESET;

            if((cReadoutReq == 0 && cNtriggers < cNevents * (cMultiplicity + 1)) && cNWords != 0)
            {
                LOG(INFO) << BOLDRED << "\t...Readout request not cleared... Trigger in counter is " << cNtriggers << " asked for " << cNevents * (cMultiplicity + 1) << " events and have " << cNWords
                          << " words in the readout... Re-trying point" << RESET;
            }
            else if(cNWords == 0)
            {
                LOG(INFO) << BOLDRED << "\t...No data in the readout ... Trigger in counter is " << cNtriggers << " asked for " << cNevents * (cMultiplicity + 1) << " events and have " << cNWords
                          << " words in the readout... Re-trying point" << RESET;
            }
            else
            {
                LOG(DEBUG) << BOLDGREEN << "\t...Have data in the readout ... Trigger in counter is " << cNtriggers << " asked for " << cNevents * (cMultiplicity + 1) << " events and have " << cNWords
                           << " words in the readout... reading out data." << RESET;
            }
        }
        else
        {
            uint32_t cIterations = 0;
            do
            {
                LOG(DEBUG) << "Trigger State: " << BOLDGREEN << "Running" << RESET;
                std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
                cIterations++;
            } while(this->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") && cIterations < 10);
            cFailed = (this->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") || cIterations == 10);
            this->PS_Close_shutter(fFastCommandDuration);
        }
        // stop
        this->Stop();
    }
    else if(cTriggerSource == 42)
    {
        LOG(INFO) << BOLDBLUE << "Trigger source is 42.. messing around" << RESET;
        // send a resync
        uint8_t cReSync   = 0;
        uint8_t cL1A      = 0;
        uint8_t cCalPulse = 0;
        uint8_t cBC0      = 0;
        this->Compose_fast_command(fFastCommandDuration, cReSync, cL1A, cCalPulse, cBC0);
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));

        cReSync = 0;
        cBC0    = 0;
        cL1A    = 1;
        // funny trigger source
        LOG(INFO) << BOLDBLUE << "Reading Nevent with single triggers sent from SW" << RESET;
        for(uint32_t cIndx = 0; cIndx < (cNevents); cIndx++)
        {
            this->Compose_fast_command(fFastCommandDuration, cReSync, cL1A, cCalPulse, cBC0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        cFailed = false;
    }
    else if(cEventType == EventType::MPAAS || cEventType == EventType::SSAAS)
    {
        this->PS_Close_shutter(fFastCommandDuration);
        this->PS_Clear_counters(fFastCommandDuration);
        // LOG (DEBUG) << BOLDGREEN << "Manual injection..." << RESET;
        // this->ChipReSync();
        // this->PS_Clear_counters(fFastCommandDuration);
        // this->PS_Open_shutter(fFastCommandDuration);
        // for( size_t cAttempt=0; cAttempt<cNevents; cAttempt++)
        // {
        //     this->ChipTestPulse();
        //     std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
        // }
        // this->PS_Close_shutter(fFastCommandDuration);
        // this->PS_Start_counters_read(fFastCommandDuration); // start signal for readout block
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));

        ResetTriggerFSM();
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", 0});
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNevents});
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_timeout_enable", 0});
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 12});
        this->ReconfigureTriggerFSM(cVecReg);
        cVecReg.clear();

        this->Stop();
        ResetReadout();
        auto cTriggerState = this->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
        LOG(DEBUG) << BOLDBLUE << "Async SSA [trigger source == " << +this->ReadReg("fc7_daq_cnfg.fast_command_block.trigger_source") << " ] [ number of injections is "
                   << +this->ReadReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept") << " ]"
                   << "Trigger state before start is " << +cTriggerState << RESET;

        this->Start();
        uint32_t cIteration = 0;
        do
        {
            auto cNtriggers = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
            cTriggerState   = this->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");
            LOG(DEBUG) << BOLDBLUE << "D19cFWInterface::WaitForData TriggerSource 12 Trigger State: " << +cTriggerState << "Running.. .Iteration#" << +cIteration << " ... received " << +cNtriggers
                       << " triggers." << RESET;
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
            cIteration++;
        } while(cTriggerState && cIteration < 1000);
        uint32_t cNInjections = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        LOG(DEBUG) << BOLDBLUE << "Trigger state after end is " << +cTriggerState << " - fast command core counted " << cNInjections << " injections." << RESET;
    }

    if(cFailed)
        LOG(INFO) << BOLDRED << "D19cFWInterface::WaitForData FAILED" << RESET;
    else
        LOG(DEBUG) << BOLDGREEN << "D19cFWInterface::WaitForData Succeeded" << RESET;
    return cFailed;
}
void D19cFWInterface::ReadNEvents(BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait)
{
    // write number of triggers to accept
    // in the handshake mode offset is cleared after each handshake
    // fDDR3Offset = 0;
    auto cHandshakeMode     = ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable");
    auto cNtriggersToAccept = ReadReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept");
    this->WriteReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNEvents);
    LOG(DEBUG) << BOLDMAGENTA << "D19cFWInterface::ReadNEvents asking for " << +pNEvents << " events "
               << " number of triggers to accept is currently " << +cNtriggersToAccept << " handshake mode is currently " << +cHandshakeMode << RESET;
    bool cFailed = WaitForData(pBoard);
    if(!cFailed)
    {
        LOG(DEBUG) << BOLDGREEN << "D19cFWInterface::ReadNEvents WaitForData Succeeded now going to try and GetData" << RESET;
        // if trigger multiplicity is not 0 check
        auto cMultiplicity = this->ReadReg("fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
        pNEvents           = (cMultiplicity != 0) ? pNEvents * (cMultiplicity + 1) : pNEvents;
        auto cNevents      = this->GetData(pBoard, pData);
        LOG(DEBUG) << BOLDYELLOW << "D19cFWInterface::ReadNEvents GetData returned " << cNevents << RESET;
        EventType cEventType = pBoard->getEventType();
        bool      cAsync     = (cEventType == EventType::SSAAS || cEventType == EventType::MPAAS || cEventType == EventType::PSAS);
        if(cNevents != pNEvents && !cAsync)
        {
            if(fReadoutAttempts < 10)
            {
                ResetTriggerFSM();
                std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
                // reset the readout
                this->ResetReadout();
                std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
                this->TriggerConfiguration();
                fReadoutAttempts++;
                LOG(INFO) << BOLDRED << "D19cFWInterface::ReadNEvents Failed to read back correct number of words from FC7.. Will try again" << RESET;
                this->ReadNEvents(pBoard, pNEvents, pData);
            }
            else
            {
                LOG(INFO) << BOLDRED << "After " << +fReadoutAttempts << " attempts at reading out data .. I'm giving up! " << RESET;
                throw Exception("Too many failures when attempting to read data from the FC7..somethign is wrong!");
            }
        }
        WriteReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable", cHandshakeMode);
        WriteReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNtriggersToAccept);
        // fDDR3Offset = 0;
    }
    // again check if failed to re-run in case
    else if(fReadoutAttempts < 10)
    {
        LOG(INFO) << BOLDRED << "Failed to readout all events..... Retrying..." << RESET;

        uint32_t cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
        uint32_t cNtriggers  = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        uint32_t cNWords     = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
        LOG(INFO) << BOLDRED << "Read back " << +cNWords << " from FC7... readout request is " << +cReadoutReq << RESET;
        LOG(INFO) << BOLDGREEN << "Waiting for data:" << fReadoutAttempts << "sec - triggers received:" << +cNtriggers << RESET;

        pData.clear();
        this->Stop();
        ResetTriggerFSM();
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
        // reset the readout
        this->ResetReadout();
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
        this->TriggerConfiguration();

        fReadoutAttempts++;
        // // send a ReSync
        // // if this helps then the problem is a system level one
        // // and now simply a FW/back-end one
        // this->ChipReSync();
        WriteReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable", cHandshakeMode);
        // try again
        this->ReadNEvents(pBoard, pNEvents, pData);
    }
    else
    {
        LOG(INFO) << BOLDRED << "After " << +fReadoutAttempts << " attempts at reading out data .. I'm giving up! " << RESET;
        throw Exception("Too many failures when attempting to read data from the FC7..somethign is wrong!");
    }
    if(fSaveToFile) fFileHandler->setData(pData);
    // reset readout attempts
    fReadoutAttempts = 0;
}

/** compute the block size according to the number of CBC's on this board
 * this will have to change with a more generic FW */
uint32_t D19cFWInterface::computeEventSize(BeBoard* pBoard)
{
    uint32_t cFrontEndTypeCode = ReadReg("fc7_daq_stat.general.info.chip_type");
    fFirmwareFrontEndType      = getFrontEndType(cFrontEndTypeCode);
    uint32_t cNFe              = pBoard->getNFe();
    uint32_t cNChips           = 0;

    uint32_t cNEventSize32 = 0;
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup) { cNChips += cHybrid->size(); }
    }
    if(fNCic != 0)
    {
        uint32_t cSparsified = ReadReg("fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable");
        LOG(DEBUG) << BOLDBLUE << "CIC sparsification expected to be : " << +cSparsified << RESET;
    }
    else
    {
        if(fFirmwareFrontEndType == FrontEndType::CBC3) cNEventSize32 = D19C_EVENT_HEADER1_SIZE_32_CBC3 + cNChips * D19C_EVENT_SIZE_32_CBC3;
        if(fFirmwareFrontEndType == FrontEndType::MPA) cNEventSize32 = D19C_EVENT_HEADER1_SIZE_32 + cNFe * D19C_EVENT_HEADER2_SIZE_32 + cNChips * D19C_EVENT_SIZE_32_MPA;
        if(fFirmwareFrontEndType == FrontEndType::SSA) cNEventSize32 = D19C_EVENT_HEADER1_SIZE_32 + cNFe * D19C_EVENT_HEADER2_SIZE_32 + cNChips * D19C_EVENT_SIZE_32_SSA;
    }
    if(ReadReg("fc7_daq_stat.ddr3_block.is_ddr3_type"))
    {
        uint32_t cNEventSize32_divided_by_8 = ((cNEventSize32 >> 3) << 3);
        if(!(cNEventSize32_divided_by_8 == cNEventSize32)) { cNEventSize32 = cNEventSize32_divided_by_8 + 8; }
    }
    return cNEventSize32;
}

std::vector<uint32_t> D19cFWInterface::ReadBlockRegValue(const std::string& pRegNode, const uint32_t& pBlocksize) { return ReadBlockReg(pRegNode, pBlocksize); }

std::vector<uint32_t> D19cFWInterface::ReadBlockRegOffsetValue(const std::string& pRegNode, const uint32_t& pBlocksize, const uint32_t& pBlockOffset)
{
    std::vector<uint32_t> vBlock = ReadBlockRegOffset(pRegNode, pBlocksize, pBlockOffset);
    LOG(DEBUG) << BOLDGREEN << +pBlocksize << " words read back from memory " << RESET;
    if(ReadReg("fc7_daq_stat.ddr3_block.is_ddr3_type"))
    {
        fDDR3Offset += pBlocksize;
        LOG(DEBUG) << BOLDGREEN << "\t... " << +fDDR3Offset << " current offset in DDR3 " << RESET;
    }
    return vBlock;
}

bool D19cFWInterface::WriteBlockReg(const std::string& pRegNode, const std::vector<uint32_t>& pValues)
{
    bool cWriteCorr = RegManager::WriteBlockReg(pRegNode, pValues);
    // std::this_thread::sleep_for (std::chrono::microseconds (fWait_us) );
    return cWriteCorr;
}

///////////////////////////////////////////////////////
//      CBC Methods                                 //
/////////////////////////////////////////////////////
// TODO: check what to do with fFMCid and if I need it!
// this is clearly for addressing individual CBCs, have to see how to deal with broadcast commands

void D19cFWInterface::EncodeReg(const ChipRegItem& pRegItem, Chip* pChip, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite)
{
    uint8_t pCbcId       = pChip->getId();
    uint8_t pLinkId      = pChip->getOpticalId();
    uint8_t pFeId        = pChip->getHybridId();
    auto    cMapIterator = fI2CSlaveMap.find(pCbcId);
    bool    cFound       = (cMapIterator != fI2CSlaveMap.end());
    if(cFound)
    {
        // remember .. encoded command the chip id is .. the index and not the id!!
        uint8_t pIndex = std::distance(fI2CSlaveMap.begin(), cMapIterator);
        // LOG(INFO) << BOLDGREEN << "Encoding register from chip " << +pCbcId << " which is index " << +pIndex << " in I2C map " << RESET;
        // use fBroadcastCBCId for broadcast commands
        bool pUseMask = false;
        if(fOptical)
        {
            this->selectLink(pLinkId);
            // new command consists of one word if its read command, and of two words if its write. first word is always
            uint32_t cWord = (pLinkId << 29) | (0 << 28) | (0 << 27) | (pFeId << 23) | (pCbcId << 18) | (pReadBack << 17) | ((!pWrite) << 16) | (pRegItem.fPage << 8) | (pRegItem.fAddress << 0);
            pVecReq.push_back(cWord);
            // only for write commands
            if(pWrite)
            {
                cWord = (pLinkId << 29) | (0 << 28) | (0 << 27) | (pFeId << 23) | (pCbcId << 18) | (pRegItem.fValue << 0);
                pVecReq.push_back(cWord);
            }
        }
        else if(fI2CVersion >= 1)
        {
            // new command consists of one word if its read command, and of two words if its write. first word is always
            // the same
            pVecReq.push_back((0 << 28) | (0 << 27) | (pFeId << 23) | (pIndex << 18) | (pReadBack << 17) | ((!pWrite) << 16) | (pRegItem.fPage << 8) | (pRegItem.fAddress << 0));
            // only for write commands
            if(pWrite) pVecReq.push_back((0 << 28) | (pWrite << 27) | (pRegItem.fValue << 0));
        }
        else
        {
            pVecReq.push_back((0 << 28) | (pFeId << 24) | (pCbcId << 20) | (pReadBack << 19) | (pUseMask << 18) | ((pRegItem.fPage) << 17) | ((!pWrite) << 16) | (pRegItem.fAddress << 8) |
                              pRegItem.fValue);
        }
    }
    else
    {
        LOG(INFO) << BOLDRED << "Could not find address in I2C map.. " << RESET;
    }
}

void D19cFWInterface::EncodeReg(const ChipRegItem& pRegItem, uint8_t pCbcId, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite)
{
    // use fBroadcastCBCId for broadcast commands
    bool    pUseMask = false;
    uint8_t pFeId    = 0;
    pVecReq.push_back((0 << 28) | (pFeId << 24) | (pCbcId << 20) | (pReadBack << 19) | (pUseMask << 18) | ((pRegItem.fPage) << 17) | ((!pWrite) << 16) | (pRegItem.fAddress << 8) | pRegItem.fValue);
}
void D19cFWInterface::EncodeReg(const ChipRegItem& pRegItem, uint8_t pFeId, uint8_t pCbcId, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite)
{
    auto cMapIterator = fI2CSlaveMap.find(pCbcId);
    bool cFound       = (cMapIterator != fI2CSlaveMap.end());
    if(cFound)
    {
        // remember .. encoded command the chip id is .. the index and not the id!!
        uint8_t pIndex = std::distance(fI2CSlaveMap.begin(), cMapIterator);
        // LOG(INFO) << BOLDGREEN << "Encoding register from chip " << +pCbcId << " which is index " << +pIndex << " in I2C map " << RESET;
        // use fBroadcastCBCId for broadcast commands
        bool pUseMask = false;
        if(fOptical)
        {
            uint8_t pLinkId = 0; // placeholder .. eventually should have the link here
            // new command consists of one word if its read command, and of two words if its write. first word is always

            // the same
            uint32_t cWord = (pLinkId << 29) | (0 << 28) | (0 << 27) | (pFeId << 23) | (pCbcId << 18) | (pReadBack << 17) | ((!pWrite) << 16) | (pRegItem.fPage << 8) | (pRegItem.fAddress << 0);
            pVecReq.push_back(cWord);
            // only for write commands
            if(pWrite)
            {
                cWord = (pLinkId << 29) | (0 << 28) | (0 << 27) | (pFeId << 23) | (pCbcId << 18) | (pRegItem.fValue << 0);
                pVecReq.push_back(cWord);
            }
        }
        else if(fI2CVersion >= 1)
        {
            // new command consists of one word if its read command, and of two words if its write. first word is always
            // the same
            pVecReq.push_back((0 << 28) | (0 << 27) | (pFeId << 23) | (pIndex << 18) | (pReadBack << 17) | ((!pWrite) << 16) | (pRegItem.fPage << 8) | (pRegItem.fAddress << 0));
            // only for write commands
            if(pWrite) pVecReq.push_back((0 << 28) | (pWrite << 27) | (pRegItem.fValue << 0));
        }
        else
        {
            pVecReq.push_back((0 << 28) | (pFeId << 24) | (pCbcId << 20) | (pReadBack << 19) | (pUseMask << 18) | ((pRegItem.fPage) << 17) | ((!pWrite) << 16) | (pRegItem.fAddress << 8) |
                              pRegItem.fValue);
        }
    }
    else
    {
        LOG(INFO) << BOLDRED << "Could not find address in I2C map.. " << RESET;
    }
}

void D19cFWInterface::BCEncodeReg(const ChipRegItem& pRegItem, uint8_t pNCbc, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite)
{
    // use fBroadcastCBCId for broadcast commands
    bool pUseMask = false;
    pVecReq.push_back((2 << 28) | (pReadBack << 19) | (pUseMask << 18) | ((pRegItem.fPage) << 17) | ((!pWrite) << 16) | (pRegItem.fAddress << 8) | pRegItem.fValue);
}

void D19cFWInterface::DecodeReg(ChipRegItem& pRegItem, uint8_t& pCbcId, uint32_t pWord, bool& pRead, bool& pFailed)
{
    if(fI2CVersion >= 1)
    {
        // pFeId    =  ( ( pWord & 0x07800000 ) >> 27) ;
        pCbcId            = ((pWord & 0x007c0000) >> 22);
        pFailed           = 0;
        pRegItem.fPage    = 0;
        pRead             = true;
        pRegItem.fAddress = (pWord & 0x0000FF00) >> 8;
        pRegItem.fValue   = (pWord & 0x000000FF);
    }
    else
    {
        // pFeId    =  ( ( pWord & 0x00f00000 ) >> 24) ;
        pCbcId            = ((pWord & 0x00f00000) >> 20);
        pFailed           = 0;
        pRegItem.fPage    = ((pWord & 0x00020000) >> 17);
        pRead             = (pWord & 0x00010000) >> 16;
        pRegItem.fAddress = (pWord & 0x0000FF00) >> 8;
        pRegItem.fValue   = (pWord & 0x000000FF);
    }
}

bool D19cFWInterface::ReadI2C(uint32_t pNReplies, std::vector<uint32_t>& pReplies)
{
    bool cFailed(false);

    uint32_t single_WaitingTime = SINGLE_I2C_WAIT * pNReplies;
    uint32_t max_Attempts       = 100;
    uint32_t counter_Attempts   = 0;

    // read the number of received replies from ndata and use this number to compare with the number of expected replies
    // and to read this number 32-bit words from the reply FIFO
    uint32_t cNReplies = 0;
    while(cNReplies != pNReplies)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(single_WaitingTime));
        cNReplies = ReadReg("fc7_daq_stat.command_processor_block.i2c.nreplies");

        if(counter_Attempts > max_Attempts)
        {
            LOG(INFO) << "Error: Read " << cNReplies << " I2C replies whereas " << pNReplies << " are expected!";
            ReadErrors();
            cFailed = true;
            break;
        }
        counter_Attempts++;
    }

    try
    {
        pReplies = ReadBlockRegValue("fc7_daq_ctrl.command_processor_block.i2c.reply_fifo", cNReplies);
    }
    catch(Exception& except)
    {
        throw except;
    }

    // reset the i2c controller here?
    return cFailed;
}

bool D19cFWInterface::WriteI2C(std::vector<uint32_t>& pVecSend, std::vector<uint32_t>& pReplies, bool pReadback, bool pBroadcast)
{
    std::lock_guard<std::mutex> theGuard(fMutex);
    bool                        cFailed(false);
    if(fOptical)
    {
        // LOG (INFO) << BOLDBLUE << "D19cFWInterface::WriteI2C GBTx" << RESET;
        GbtInterface cGBTx;
        // assume that they are all the same just to test - multibyte write for CBC
        // uint8_t cFirstChip = (pVecSend[0] & (0x1F << 18) ) >> 18;
        uint8_t cWriteReq = !((pVecSend[0] & (0x1 << 16)) >> 16);
        if(cWriteReq == 1)
        {
            // still being tested - WIP
            cFailed = !cGBTx.i2cWrite(this, pVecSend, pReplies, true);
        }
        else
        {
            auto cIterator = pVecSend.begin();
            while(cIterator < pVecSend.end())
            {
                uint32_t cWord     = *cIterator;
                uint8_t  cWrite    = !((cWord & (0x1 << 16)) >> 16);
                uint8_t  cAddress  = (cWord & 0xFF);
                uint8_t  cPage     = (cWord & (0xFF << 8)) >> 8;
                uint8_t  cChipId   = (cWord & (0x1F << 18)) >> 18;
                uint8_t  cFeId     = (cWord & (0xF << 23)) >> 23;
                uint32_t cReadback = 0;
                if(cWrite == 0)
                {
                    // LOG (DEBUG) << BOLDBLUE << "I2C : FE" << +(cFeId%2) << " Chip" << +cChipId << " register address
                    // 0x" << std::hex << +cAddress << std::dec << " on page : " << +cPage << RESET;
                    if(cChipId < 8) { cReadback = cGBTx.cbcRead(this, cFeId % 2, cChipId, cPage + 1, cAddress); }
                    else
                    {
                        cReadback = cGBTx.cicRead(this, cFeId % 2, cAddress);
                    }
                    uint32_t cReply = (cFeId << 27) | (cChipId << 22) | (cAddress << 8) | (cReadback & 0xFF);
                    pReplies.push_back(cReply);
                }
                /*else
                {
                    cReadback = (cWord & (0x1 << 17)) >> 17;
                    cIterator++;
                    cWord = *cIterator;
                    uint8_t cValue = (cWord & 0xFF);
                    if( cChipId < 8 )
                    {
                      cFailed = !cGBTx.cbcWrite(this, cFeId%2, cChipId, cPage+1, cAddress, cValue , (cReadback == 1) );
                    }
                    else
                        cFailed = !cGBTx.cicWrite(this, cFeId%2, cAddress, cValue , (cReadback == 1) );
                }*/
                cIterator++;
            }
        }
    }
    else
    {
        // reset the I2C controller
        WriteReg("fc7_daq_ctrl.command_processor_block.i2c.control.reset_fifos", 0x1);
        // usleep (10);
        try
        {
            WriteBlockReg("fc7_daq_ctrl.command_processor_block.i2c.command_fifo", pVecSend);
        }
        catch(Exception& except)
        {
            throw except;
        }

        uint32_t cNReplies = 0;
        for(auto word: pVecSend)
        {
            // if read or readback for write == 1, then count
            if(fI2CVersion >= 1)
            {
                // uint32_t cWord = (pLinkId << 29) | (0 << 28) | (0 << 27) | (pFeId << 23) | (pCbcId << 18) | (pReadBack << 17) | ((!pWrite) << 16) | (pRegItem.fPage << 8) | (pRegItem.fAddress << 0);
                if((((word & 0x08000000) >> 27) == 0) && ((((word & 0x00010000) >> 16) == 1) or (((word & 0x00020000) >> 17) == 1)))
                {
                    if(pBroadcast)
                        cNReplies += fNReadoutChip;
                    else
                        cNReplies += 1;
                }
            }
            else
            {
                if((((word & 0x00010000) >> 16) == 1) or (((word & 0x00080000) >> 19) == 1))
                {
                    if(pBroadcast)
                        cNReplies += fNReadoutChip;
                    else
                        cNReplies += 1;
                }
            }
        }
        // std::this_thread::sleep_for (std::chrono::microseconds (fWait_us) );
        // usleep (20);
        cFailed = ReadI2C(cNReplies, pReplies);
    }
    return cFailed;
}

bool D19cFWInterface::WriteChipBlockReg(std::vector<uint32_t>& pVecReg, uint8_t& pWriteAttempts, bool pReadback)
{
    uint8_t cMaxWriteAttempts = 5;
    // the actual write & readback command is in the vector
    std::vector<uint32_t> cReplies;
    bool                  cSuccess = !WriteI2C(pVecReg, cReplies, pReadback, false);

    // here make a distinction: if pReadback is true, compare only the read replies using the binary predicate
    // else, just check that info is 0 and thus the CBC acqnowledged the command if the writeread is 0
    std::vector<uint32_t> cWriteAgain;

    if(pReadback)
    {
        // now use the Template from BeBoardFWInterface to return a vector with all written words that have been read
        // back incorrectly
        cWriteAgain = get_mismatches(pVecReg.begin(), pVecReg.end(), cReplies.begin(), D19cFWInterface::cmd_reply_comp);

        // now clear the initial cmd Vec and set the read-back
        pVecReg.clear();
        pVecReg = cReplies;
    }
    else
    {
        // since I do not read back, I can safely just check that the info bit of the reply is 0 and that it was an
        // actual write reply then i put the replies in pVecReg so I can decode later in CBCInterface cWriteAgain =
        // get_mismatches (pVecReg.begin(), pVecReg.end(), cReplies.begin(), D19cFWInterface::cmd_reply_ack);
        pVecReg.clear();
        pVecReg = cReplies;
    }

    // now check the size of the WriteAgain vector and assert Success or not
    // also check that the number of write attempts does not exceed cMaxWriteAttempts
    if(cWriteAgain.empty())
        cSuccess = true;
    else
    {
        cSuccess = false;

        // if the number of errors is greater than 100, give up
        if(cWriteAgain.size() < 100 && pWriteAttempts < cMaxWriteAttempts)
        {
            if(pReadback)
                LOG(INFO) << BOLDRED << "(WRITE#" << std::to_string(pWriteAttempts) << ") There were " << cWriteAgain.size() << " Readback Errors -trying again!" << RESET;
            else
                LOG(INFO) << BOLDRED << "(WRITE#" << std::to_string(pWriteAttempts) << ") There were " << cWriteAgain.size() << " CBC CMD acknowledge bits missing -trying again!" << RESET;

            pWriteAttempts++;
            this->WriteChipBlockReg(cWriteAgain, pWriteAttempts, true);
        }
        else if(pWriteAttempts >= cMaxWriteAttempts)
        {
            cSuccess       = false;
            pWriteAttempts = 0;
        }
        else
            throw Exception("Too many CBC readback errors - no functional I2C communication. Check the Setup");
    }

    return cSuccess;
}

bool D19cFWInterface::BCWriteChipBlockReg(std::vector<uint32_t>& pVecReg, bool pReadback)
{
    std::lock_guard<std::mutex> theGuard(fMutex);

    std::vector<uint32_t> cReplies;
    bool                  cSuccess = !WriteI2C(pVecReg, cReplies, false, true);

    // just as above, I can check the replies - there will be NCbc * pVecReg.size() write replies and also read replies
    // if I chose to enable readback this needs to be adapted
    if(pReadback)
    {
        // TODO: actually, i just need to check the read write and the info bit in each reply - if all info bits are 0,
        // this is as good as it gets, else collect the replies that faild for decoding - potentially no iterative
        // retrying
        // TODO: maybe I can do something with readback here - think about it
        for(auto& cWord: cReplies)
        {
            // it was a write transaction!
            if(((cWord >> 16) & 0x1) == 0)
            {
                // infor bit is 0 which means that the transaction was acknowledged by the CBC
                // if ( ( (cWord >> 20) & 0x1) == 0)
                cSuccess = true;
                // else cSuccess == false;
            }
            else
                cSuccess = false;

            // LOG(INFO) << std::bitset<32>(cWord) ;
        }

        // cWriteAgain = get_mismatches (pVecReg.begin(), pVecReg.end(), cReplies.begin(),
        // Cbc3Fc7FWInterface::cmd_reply_ack);
        pVecReg.clear();
        pVecReg = cReplies;
    }

    return cSuccess;
}

void D19cFWInterface::ReadChipBlockReg(std::vector<uint32_t>& pVecReg)
{
    std::vector<uint32_t> cReplies;
    // it sounds weird, but ReadI2C is called inside writeI2c, therefore here I have to write and disable the readback.
    // The actual read command is in the words of the vector, no broadcast, maybe I can get rid of it
    WriteI2C(pVecReg, cReplies, false, false);
    pVecReg.clear();
    pVecReg = cReplies;
}

void D19cFWInterface::ChipI2CRefresh() { WriteReg("fc7_daq_ctrl.fast_command_block.control.fast_i2c_refresh", 0x1); }
void D19cFWInterface::ReadoutChipReset()
{
    // for CBCs
    // LOG (DEBUG) << BOLDBLUE << "Sending hard reset to all read-out chips..." << RESET;
    if(fOptical)
    {
        GbtInterface         cGBTx;
        std::vector<uint8_t> cChannels = {30, 2};
        for(auto cChannel: cChannels)
        {
            cGBTx.scaSetGPIO(this, cChannel, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(fResetMinPeriod_ms));
            cGBTx.scaSetGPIO(this, cChannel, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(fResetMinPeriod_ms));
        }
    }
    else
    {
        WriteReg("fc7_daq_ctrl.physical_interface_block.control.chip_hard_reset", 0x1);
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    }
}
void D19cFWInterface::ChipReset()
{
    // for CBCs
    ReadoutChipReset();
    // for CICs
    // LOG (DEBUG) << BOLDBLUE << "Sending hard reset to CICs..." << RESET;
    if(fOptical)
    {
        GbtInterface         cGBTx;
        std::vector<uint8_t> cChannels = {31, 3};
        for(auto cChannel: cChannels)
        {
            cGBTx.scaSetGPIO(this, cChannel, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(fResetMinPeriod_ms));
            cGBTx.scaSetGPIO(this, cChannel, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(fResetMinPeriod_ms));
        }
    }
    else
    {
        std::vector<std::pair<std::string, uint32_t>> cVecReg;
        // for CBCs
        cVecReg.push_back({"fc7_daq_ctrl.physical_interface_block.control.chip_hard_reset", 0x1});
        cVecReg.push_back({"fc7_daq_ctrl.physical_interface_block.control.cic_hard_reset", 0x1});
        this->WriteStackReg(cVecReg);
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
    }
}
void D19cFWInterface::Compose_fast_command(uint32_t duration, uint32_t resync_en, uint32_t l1a_en, uint32_t cal_pulse_en, uint32_t bc0_en)
{
    uint32_t encode_resync    = resync_en << 16;
    uint32_t encode_cal_pulse = cal_pulse_en << 17;
    uint32_t encode_l1a       = l1a_en << 18;
    uint32_t encode_bc0       = bc0_en << 19;
    uint32_t encode_duration  = duration << 28;

    uint32_t final_command = encode_resync + encode_l1a + encode_cal_pulse + encode_bc0 + encode_duration;
    WriteReg("fc7_daq_ctrl.fast_command_block.control", final_command);
}
void D19cFWInterface::ChipReSync()
{
    uint8_t cReSync   = 1;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 0;
    // in CIC case always send fast reset with an orbit reset
    uint32_t cFrontEndTypeCode = ReadReg("fc7_daq_stat.general.info.chip_type");
    bool     cWithCIC          = (getFrontEndType(cFrontEndTypeCode) == FrontEndType::CIC || getFrontEndType(cFrontEndTypeCode) == FrontEndType::CIC2);
    uint8_t  cBC0              = (cWithCIC && fIs2S) ? 1 : 0;
    this->Compose_fast_command(fFastCommandDuration, cReSync, cL1A, cCalPulse, cBC0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
void D19cFWInterface::ChipTestPulse()
{
    uint8_t cReSync   = 0;
    uint8_t cCalPulse = 1;
    uint8_t cL1A      = 0;
    uint8_t cBC0      = 0;
    this->Compose_fast_command(fFastCommandDuration, cReSync, cL1A, cCalPulse, cBC0);
}

void D19cFWInterface::ChipTrigger() { this->Trigger(fFastCommandDuration); }
void D19cFWInterface::Trigger(uint8_t pDuration)
{
    uint8_t cReSync   = 0;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 1;
    uint8_t cBC0      = 0;
    this->Compose_fast_command(pDuration, cReSync, cL1A, cCalPulse, cBC0);
}

bool D19cFWInterface::Bx0Alignment()
{
    bool     cSuccess   = false;
    auto     cPkgDelay  = this->ReadReg("fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay");
    uint32_t cStubDebug = this->ReadReg("fc7_daq_cnfg.ddr3_debug.stub_enable");
    if(cStubDebug)
    {
        LOG(INFO) << BOLDBLUE << "Stub debug enable set to " << cStubDebug << "..... so disabling it!!." << RESET;
        this->WriteReg("fc7_daq_cnfg.ddr3_debug.stub_enable", 0x00);
    }
    // send a resync and reset readout
    bool    cWait     = true;
    uint8_t cAttempts = 0;
    cSuccess          = false;
    // reset decoder
    size_t cMaxAttempts = 20;
    size_t cWaitTime    = fWait_us * 1; // was 100
    this->WriteReg("fc7_daq_ctrl.physical_interface_block.control.decoder_reset", 0x1);
    this->WriteReg("fc7_daq_ctrl.physical_interface_block.control.decoder_reset", 0x0);
    do
    {
        if(cWait) std::this_thread::sleep_for(std::chrono::microseconds(cWaitTime));
        // pause after reset
        // send a resync then wait
        this->ChipReSync();
        if(cWait) std::this_thread::sleep_for(std::chrono::microseconds(cWaitTime));
        // check state of bx0 alignment block
        uint32_t cValue = this->ReadReg("fc7_daq_stat.physical_interface_block.cic_decoder.bx0_alignment_state");
        if(cValue == 8)
        {
            LOG(DEBUG) << BOLDBLUE << "Resetting decoder in back-end " << BOLDGREEN << " SUCCEEDED!"
                       << "\t... Stub package delay set to : " << +cPkgDelay << RESET;
            cSuccess = true;
            /*
            // definitely works with
            // figure out which one of these is needed
            // resync after bx0 alignment worked
            this->ChipReSync();
            if(cWait) std::this_thread::sleep_for(std::chrono::microseconds(cWaitTime));
            */
            // // reset the readout as well
            // this->ResetReadout();
            // if(cWait) std::this_thread::sleep_for(std::chrono::microseconds(cWaitTime));
        }
        else
        {
            LOG(INFO) << BOLDBLUE << "Resetting decoder in back-end " << BOLDRED << " FAILED!" << RESET;
            this->WriteReg("fc7_daq_ctrl.physical_interface_block.control.decoder_reset", 0x1);
            this->WriteReg("fc7_daq_ctrl.physical_interface_block.control.decoder_reset", 0x0);
        }
        cAttempts++;
    } while(cAttempts < cMaxAttempts && !cSuccess);
    if(!cSuccess) LOG(INFO) << BOLDRED << "Could not re-set decoder ..." << RESET;
    this->ResetReadout();

    return cSuccess;
}

// reconfigure trigger
void D19cFWInterface::ReconfigureTriggerFSM(std::vector<std::pair<std::string, uint32_t>> pTriggerConfig)
{
    // reset trigger
    this->WriteReg("fc7_daq_ctrl.fast_command_block.control.reset", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    // configure
    this->WriteStackReg(pTriggerConfig);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    // load new trigger configuration
    this->WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    // and reset the readout
    this->ResetReadout();
}
// configure trigger FSMs on the fly ...
void D19cFWInterface::ConfigureTestPulseFSM(uint16_t pDelayAfterFastReset, uint16_t pDelayAfterTP, uint16_t pDelayBeforeNextTP, uint8_t pEnableFastReset, uint8_t pEnableTP, uint8_t pEnableL1A)
{
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    // configure trigger
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 6});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset", pDelayAfterFastReset});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", pDelayAfterTP});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", pDelayBeforeNextTP});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", pEnableFastReset});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_test_pulse", pEnableTP});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_l1a", pEnableL1A});

    this->ReconfigureTriggerFSM(cVecReg);
}
void D19cFWInterface::ConfigureAntennaFSM(uint16_t pNtriggers, uint16_t pTriggerRate, uint16_t pL1Delay)
{
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNtriggers});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", pTriggerRate});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 7});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.antenna_trigger_delay_value", pL1Delay});
    this->ReconfigureTriggerFSM(cVecReg);
}

// periodic triggers
void D19cFWInterface::ConfigureTriggerFSM(uint16_t pNtriggers, uint16_t pTriggerRate, uint8_t pSource, uint8_t pStubsMask, uint8_t pStubLatency)
{
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.initial_fast_reset_enable", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNtriggers});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", pTriggerRate});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", pSource});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.stubs_mask", pStubsMask});
    this->ReconfigureTriggerFSM(cVecReg);
}

// conescutive triggers
void D19cFWInterface::ConfigureConsecutiveTriggerFSM(uint16_t pNtriggers, uint16_t pDelayBetween, uint16_t pDelayToNext)
{
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 8});
    cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.fast_duration", 15});                        // number of triggers  to accept
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNtriggers});                   // number of triggers  to accept
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.delay_between_two_consecutive", pDelayBetween});     // delay between two
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", pDelayToNext}); // delay between pairs of triggers
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset", 0});             //
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", 0});                      //
    this->ReconfigureTriggerFSM(cVecReg);
}
// measures the occupancy of the 2S chips
bool D19cFWInterface::Measure2SOccupancy(uint32_t pNEvents, uint8_t**& pErrorCounters, uint8_t***& pChannelCounters)
{
    // this will anyway be constant
    const int COUNTER_WIDTH_BITS = 8;    // we have 8bit counters currently
    const int BIT_MASK           = 0xFF; // for counter widht 8

    // check the amount of events
    if(pNEvents > pow(2, COUNTER_WIDTH_BITS) - 1)
    {
        LOG(ERROR) << "Requested more events, that counters could fit";
        return false;
    }

    // set the configuration of the fast command (number of events)
    WriteReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNEvents);
    WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);

    // disable the readout backpressure (no one cares about readout)
    uint32_t cBackpressureOldValue = ReadReg("fc7_daq_cnfg.fast_command_block.misc.backpressure_enable");
    WriteReg("fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0x0);

    // reset the counters fsm
    // WriteReg ("fc7_daq_ctrl.calibration_2s_block.control.reset_fsm", 0x1); // self reset
    // usleep (1);

    // finally start the loop
    WriteReg("fc7_daq_ctrl.calibration_2s_block.control.start", 0x1);

    // now loop till the machine is not done
    bool cLastPackage = false;
    while(!cLastPackage)
    {
        // loop waiting for the counters
        while(ReadReg("fc7_daq_stat.calibration_2s_block.general.counters_ready") == 0)
        {
            // just wait
            // uint32_t cFIFOEmpty = ReadReg ("fc7_daq_stat.calibration_2s_block.general.fifo_empty");
            // LOG(INFO) << "FIFO Empty: " << cFIFOEmpty;
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
        }
        cLastPackage = ((ReadReg("fc7_daq_stat.calibration_2s_block.general.fsm_done") == 1) && (ReadReg("fc7_daq_stat.calibration_2s_block.general.counters_ready") == 1));

        // so the counters are ready let's read the fifo
        uint32_t header = ReadReg("fc7_daq_ctrl.calibration_2s_block.counter_fifo");
        if(((header >> 16) & 0xFFFF) != 0xFFFF)
        {
            LOG(ERROR) << "Something bad with counters header";
            return false;
        }
        uint32_t cEventSize = (header & 0x0000FFFF);
        // LOG(INFO) << "Stub Counters Event size is: " << cEventSize;

        std::vector<uint32_t> counters_data = ReadBlockRegValue("fc7_daq_ctrl.calibration_2s_block.counter_fifo", cEventSize - 1);
        // for(auto word : counters_data) std::cout << std::hex << word << std::dec << std::endl;

        uint32_t cParserOffset = 0;
        while(cParserOffset < counters_data.size())
        {
            // get chip header
            uint32_t chipHeader = counters_data.at(cParserOffset);
            // check it
            if(((chipHeader >> 28) & 0xF) != 0xA)
            {
                LOG(ERROR) << "Something bad with chip header";
                return false;
            }
            // get hybrid chip id
            uint8_t cHybridId       = (chipHeader >> 20) & 0xFF;
            uint8_t cChipId         = (chipHeader >> 16) & 0xF;
            uint8_t cErrorCounter   = (chipHeader >> 8) & 0xFF;
            uint8_t cTriggerCounter = (chipHeader >> 0) & 0xFF;
            // LOG(INFO) << "\tHybrid: " << +cHybridId << ", Chip: " << +cChipId << ", Error Counter: " <<
            // +cErrorCounter << ", Trigger Counter: " << +cTriggerCounter;
            if(cTriggerCounter != pNEvents)
            {
                LOG(ERROR) << "Number of triggers does not match the requested amount";
                return false;
            }

            // now parse the counters
            pErrorCounters[cHybridId][cChipId] = cErrorCounter;
            for(uint8_t ch = 0; ch < NCHANNELS; ch++)
            {
                uint8_t cWordId                          = cParserOffset + 1 + (uint8_t)ch / (32 / COUNTER_WIDTH_BITS); // 1 for header, ch/4 because we have 4 counters per word
                uint8_t cBitOffset                       = ch % (32 / COUNTER_WIDTH_BITS) * COUNTER_WIDTH_BITS;
                pChannelCounters[cHybridId][cChipId][ch] = (counters_data.at(cWordId) >> cBitOffset) & BIT_MASK;
            }

            // increment the offset
            cParserOffset += (1 + (NCHANNELS + (4 - NCHANNELS % 4)) / 4);
        }
    }

    // debug out
    // for(uint8_t ch = 0; ch < NCHANNELS; ch++) std::cout << "Ch: " << +ch << ", Counter: " <<
    // +pChannelCounters[0][0][ch] << std::endl;

    // just in case write back the old backrepssure valie
    WriteReg("fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", cBackpressureOldValue);

    // return
    return true;
}

// method to remove the arrays
void D19cFWInterface::Manage2SCountersMemory(uint8_t**& pErrorCounters, uint8_t***& pChannelCounters, bool pAllocate)
{
    // this will anyway be constant
    const unsigned int NCHIPS_PER_HYBRID_COUNTERS = 8;           // data from one CIC
    const unsigned int HYBRIDS_TOTAL              = fFWNHybrids; // for allocation

    if(pAllocate)
    {
        // allocating the array
        if(pChannelCounters == nullptr && pErrorCounters == nullptr)
        {
            // allocate
            pChannelCounters = new uint8_t**[HYBRIDS_TOTAL];
            pErrorCounters   = new uint8_t*[HYBRIDS_TOTAL];
            for(uint32_t h = 0; h < HYBRIDS_TOTAL; h++)
            {
                pChannelCounters[h] = new uint8_t*[NCHIPS_PER_HYBRID_COUNTERS];
                pErrorCounters[h]   = new uint8_t[NCHIPS_PER_HYBRID_COUNTERS];
                for(uint32_t c = 0; c < NCHIPS_PER_HYBRID_COUNTERS; c++) { pChannelCounters[h][c] = new uint8_t[NCHANNELS]; }
            }

            // set to zero
            for(uint32_t h = 0; h < HYBRIDS_TOTAL; h++)
            {
                for(uint32_t c = 0; c < NCHIPS_PER_HYBRID_COUNTERS; c++)
                {
                    for(int32_t ch = 0; ch < NCHANNELS; ch++) { pChannelCounters[h][c][ch] = 0; }
                }
            }
        }
    }
    else
    {
        // deleting all the array
        for(uint32_t h = 0; h < HYBRIDS_TOTAL; h++)
        {
            for(uint32_t c = 0; c < NCHIPS_PER_HYBRID_COUNTERS; c++) delete pChannelCounters[h][c];
            delete pChannelCounters[h];
            delete pErrorCounters[h];
        }
        delete pChannelCounters;
        delete pErrorCounters;
    }
}

void D19cFWInterface::FlashProm(const std::string& strConfig, const char* pstrFile)
{
    checkIfUploading();

    fpgaConfig->runUpload(strConfig, pstrFile);
}

void D19cFWInterface::JumpToFpgaConfig(const std::string& strConfig)
{
    checkIfUploading();

    fpgaConfig->jumpToImage(strConfig);
}

void D19cFWInterface::DownloadFpgaConfig(const std::string& strConfig, const std::string& strDest)
{
    checkIfUploading();
    fpgaConfig->runDownload(strConfig, strDest.c_str());
}

std::vector<std::string> D19cFWInterface::getFpgaConfigList()
{
    checkIfUploading();
    return fpgaConfig->getFirmwareImageNames();
}

void D19cFWInterface::DeleteFpgaConfig(const std::string& strId)
{
    checkIfUploading();
    fpgaConfig->deleteFirmwareImage(strId);
}

void D19cFWInterface::checkIfUploading()
{
    if(fpgaConfig && fpgaConfig->getUploadingFpga() > 0) throw Exception("This board is uploading an FPGA configuration");

    if(!fpgaConfig) fpgaConfig = new D19cFpgaConfig(this);
}

void D19cFWInterface::RebootBoard()
{
    if(!fpgaConfig) fpgaConfig = new D19cFpgaConfig(this);

    fpgaConfig->resetBoard();
}

bool D19cFWInterface::cmd_reply_comp(const uint32_t& cWord1, const uint32_t& cWord2) { return true; }

bool D19cFWInterface::cmd_reply_ack(const uint32_t& cWord1, const uint32_t& cWord2)
{
    // if it was a write transaction (>>17 == 0) and
    // the CBC id matches it is false
    if(((cWord2 >> 16) & 0x1) == 0 && (cWord1 & 0x00F00000) == (cWord2 & 0x00F00000))
        return true;
    else
        return false;
}

void D19cFWInterface::PSInterfaceBoard_PowerOn_SSA(float VDDPST, float DVDD, float AVDD, float VBF, float BG, uint8_t ENABLE)
{
    // this->getBoardInfo();
    this->PSInterfaceBoard_PowerOn(0, 0);

    uint32_t write   = 0;
    uint32_t SLOW    = 2;
    uint32_t i2cmux  = 0;
    uint32_t pcf8574 = 1;
    uint32_t dac7678 = 4;
    std::this_thread::sleep_for(std::chrono::milliseconds(750));

    PSInterfaceBoard_SetSlaveMap();
    PSInterfaceBoard_ConfigureI2CMaster(1, SLOW);
    std::this_thread::sleep_for(std::chrono::milliseconds(750));

    float Vc = 0.0003632813;

    LOG(INFO) << "ssa vdd on";

    float Vlimit = 1.32;
    if(VDDPST > Vlimit) VDDPST = Vlimit;
    float    diffvoltage = 1.5 - VDDPST;
    uint32_t setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;

    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01); // to SCO on PCA9646
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x33, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    LOG(INFO) << "ssa vddD on";
    Vlimit = 1.32;
    if(DVDD > Vlimit) DVDD = Vlimit;
    diffvoltage = 1.5 - DVDD;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01); // to SCO on PCA9646
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x31, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    LOG(INFO) << "ssa vddA on";
    Vlimit = 1.32;
    if(AVDD > Vlimit) AVDD = Vlimit;
    diffvoltage = 1.5 - AVDD;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01); // to SCO on PCA9646
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x35, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    LOG(INFO) << "ssa BG on";
    Vlimit = 1.32;
    if(BG > Vlimit) BG = Vlimit;
    float Vc2  = 4095 / 1.5;
    setvoltage = int(round(BG * Vc2));
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01); // to SCO on PCA9646
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x36, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    LOG(INFO) << "ssa VBF on";
    Vlimit = 0.5;
    if(VBF > Vlimit) VBF = Vlimit;
    setvoltage = int(round(VBF * Vc2));
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01); // to SCO on PCA9646
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x37, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    uint32_t VAL = (ENABLE);
    LOG(INFO) << BOLDRED << VAL << "  writeme!" << RESET;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x02);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    PSInterfaceBoard_SendI2CCommand(pcf8574, 0, write, 0, VAL); // set reset bit

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    PSInterfaceBoard_ConfigureI2CMaster(0, SLOW);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

////// MPA/SSA Methods:

// COMS:
void D19cFWInterface::PSInterfaceBoard_SetSlaveMap()
{
    std::vector<std::vector<uint32_t>> i2c_slave_map;
    i2c_slave_map.push_back({0x70, 0, 1, 1, 0, 1}); // 0  PCA9646
    i2c_slave_map.push_back({0x20, 0, 1, 1, 0, 1}); // 1  PCF8574
    i2c_slave_map.push_back({0x24, 0, 1, 1, 0, 1}); // 2  PCF8574
    i2c_slave_map.push_back({0x14, 0, 2, 3, 0, 1}); // 3  LTC2487
    i2c_slave_map.push_back({0x48, 1, 2, 2, 0, 0}); // 4  DAC7678
    i2c_slave_map.push_back({0x40, 1, 2, 2, 0, 1}); // 5  INA226
    i2c_slave_map.push_back({0x41, 1, 2, 2, 0, 1}); // 6  INA226
    i2c_slave_map.push_back({0x42, 1, 2, 2, 0, 1}); // 7  INA226
    i2c_slave_map.push_back({0x44, 1, 2, 2, 0, 1}); // 8  INA226
    i2c_slave_map.push_back({0x45, 1, 2, 2, 0, 1}); // 9  INA226
    i2c_slave_map.push_back({0x46, 1, 2, 2, 0, 1}); // 10  INA226
    i2c_slave_map.push_back({0x40, 2, 1, 1, 1, 0}); // 11  ????
    i2c_slave_map.push_back({0x20, 2, 1, 1, 1, 0}); // 12  ????
    i2c_slave_map.push_back({0x0, 0, 1, 1, 0, 0});  // 13  ????
    i2c_slave_map.push_back({0x0, 0, 1, 1, 0, 0});  // 14  ????
    i2c_slave_map.push_back({0x5F, 1, 1, 1, 1, 0}); // 15  CBC3

    LOG(INFO) << "Updating the Slave ID Map (mpa ssa board) ";

    for(int ism = 0; ism < 16; ism++)
    {
        uint32_t    shifted_i2c_address             = i2c_slave_map[ism][0] << 25;
        uint32_t    shifted_register_address_nbytes = i2c_slave_map[ism][1] << 6;
        uint32_t    shifted_data_wr_nbytes          = i2c_slave_map[ism][2] << 4;
        uint32_t    shifted_data_rd_nbytes          = i2c_slave_map[ism][3] << 2;
        uint32_t    shifted_stop_for_rd_en          = i2c_slave_map[ism][4] << 1;
        uint32_t    shifted_nack_en                 = i2c_slave_map[ism][5] << 0;
        uint32_t    final_command = shifted_i2c_address + shifted_register_address_nbytes + shifted_data_wr_nbytes + shifted_data_rd_nbytes + shifted_stop_for_rd_en + shifted_nack_en;
        std::string curreg        = "fc7_daq_cnfg.mpa_ssa_board_block.slave_" + std::to_string(ism) + "_config";
        WriteReg(curreg, final_command);
    }
}

void D19cFWInterface::PSInterfaceBoard_ConfigureI2CMaster(uint32_t pEnabled = 1, uint32_t pFrequency = 4)
{
    // wait for all commands to be executed
    std::chrono::microseconds cWait(fWait_us);
    while(!ReadReg("fc7_daq_stat.command_processor_block.i2c.command_fifo.empty")) { std::this_thread::sleep_for(cWait); }

    if(pEnabled > 0)
        LOG(INFO) << "Enabling the MPA SSA Board I2C master";
    else
        LOG(INFO) << "Disabling the MPA SSA Board I2C master";

    // setting the values
    WriteReg("fc7_daq_cnfg.physical_interface_block.i2c.master_en", int(not pEnabled));
    WriteReg("fc7_daq_cnfg.mpa_ssa_board_block.i2c_master_en", pEnabled);
    WriteReg("fc7_daq_cnfg.mpa_ssa_board_block.i2c_freq", pFrequency);
    std::this_thread::sleep_for(cWait);

    // resetting the fifos and the board
    WriteReg("fc7_daq_ctrl.command_processor_block.i2c.control.reset", 1);
    WriteReg("fc7_daq_ctrl.command_processor_block.i2c.control.reset_fifos", 1);
    WriteReg("fc7_daq_ctrl.mpa_ssa_board_block.reset", 1);
    std::this_thread::sleep_for(cWait);
}

void D19cFWInterface::PSInterfaceBoard_SendI2CCommand(uint32_t slave_id, uint32_t board_id, uint32_t read, uint32_t register_address, uint32_t data)
{
    std::chrono::microseconds cWait(fWait_us);

    uint32_t shifted_command_type     = 1 << 31;
    uint32_t shifted_word_id_0        = 0;
    uint32_t shifted_slave_id         = slave_id << 21;
    uint32_t shifted_board_id         = board_id << 20;
    uint32_t shifted_read             = read << 16;
    uint32_t shifted_register_address = register_address;

    uint32_t shifted_word_id_1 = 1 << 26;
    uint32_t shifted_data      = data;

    uint32_t word_0 = shifted_command_type + shifted_word_id_0 + shifted_slave_id + shifted_board_id + shifted_read + shifted_register_address;
    uint32_t word_1 = shifted_command_type + shifted_word_id_1 + shifted_data;

    WriteReg("fc7_daq_ctrl.command_processor_block.i2c.command_fifo", word_0);
    std::this_thread::sleep_for(cWait);
    WriteReg("fc7_daq_ctrl.command_processor_block.i2c.command_fifo", word_1);
    std::this_thread::sleep_for(cWait);

    int readempty = ReadReg("fc7_daq_stat.command_processor_block.i2c.reply_fifo.empty");
    while(readempty > 0)
    {
        std::this_thread::sleep_for(cWait);
        readempty = ReadReg("fc7_daq_stat.command_processor_block.i2c.reply_fifo.empty");
    }

    int reply_err  = ReadReg("fc7_daq_ctrl.command_processor_block.i2c.mpa_ssa_i2c_reply.err");
    int reply_data = ReadReg("fc7_daq_ctrl.command_processor_block.i2c.mpa_ssa_i2c_reply.data");
    if(reply_err == 1)
        LOG(ERROR) << "Error code: " << std::hex << reply_data << std::dec;
    else
    {
        if(read == 1)
            LOG(INFO) << BOLDBLUE << "Data that was read is: " << reply_data << RESET;
        else
            LOG(DEBUG) << BOLDBLUE << "Successful write transaction" << RESET;
    }
}

uint32_t D19cFWInterface::PSInterfaceBoard_SendI2CCommand_READ(uint32_t slave_id, uint32_t board_id, uint32_t read, uint32_t register_address, uint32_t data)
{
    std::chrono::microseconds cWait(fWait_us);

    uint32_t shifted_command_type     = 1 << 31;
    uint32_t shifted_word_id_0        = 0;
    uint32_t shifted_slave_id         = slave_id << 21;
    uint32_t shifted_board_id         = board_id << 20;
    uint32_t shifted_read             = read << 16;
    uint32_t shifted_register_address = register_address;

    uint32_t shifted_word_id_1 = 1 << 26;
    uint32_t shifted_data      = data;

    uint32_t word_0 = shifted_command_type + shifted_word_id_0 + shifted_slave_id + shifted_board_id + shifted_read + shifted_register_address;
    uint32_t word_1 = shifted_command_type + shifted_word_id_1 + shifted_data;

    WriteReg("fc7_daq_ctrl.command_processor_block.i2c.command_fifo", word_0);
    std::this_thread::sleep_for(cWait);
    WriteReg("fc7_daq_ctrl.command_processor_block.i2c.command_fifo", word_1);
    std::this_thread::sleep_for(cWait);

    int readempty = ReadReg("fc7_daq_stat.command_processor_block.i2c.reply_fifo.empty");
    LOG(INFO) << BOLDBLUE << readempty << RESET;
    while(readempty > 0)
    {
        std::cout << ".";
        std::this_thread::sleep_for(cWait);
        readempty = ReadReg("fc7_daq_stat.command_processor_block.i2c.reply_fifo.empty");
    }
    std::cout << std::endl;

    uint32_t reply = ReadReg("fc7_daq_ctrl.command_processor_block.i2c.mpa_ssa_i2c_reply");
    // LOG (INFO) << BOLDRED << std::hex << reply << std::dec << RESET;
    uint32_t reply_err  = ReadReg("fc7_daq_ctrl.command_processor_block.i2c.mpa_ssa_i2c_reply.err");
    uint32_t reply_data = ReadReg("fc7_daq_ctrl.command_processor_block.i2c.mpa_ssa_i2c_reply.data");

    if(reply_err == 1)
        LOG(ERROR) << "Error code: " << std::hex << reply_data << std::dec;
    else
    {
        if(read == 1)
        {
            LOG(INFO) << BOLDBLUE << "Data that was read is: " << std::hex << reply_data << std::dec << "   ecode: " << reply_err << RESET;
            return reply & 0xFFFFFF;
        }
        else
            LOG(DEBUG) << BOLDBLUE << "Successful write transaction" << RESET;
    }

    return 0;
}

void D19cFWInterface::PS_Open_shutter(uint32_t pDuration)
{
    uint8_t cReSync   = 0;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 1;
    uint8_t cBC0      = 0;
    this->Compose_fast_command(pDuration, cReSync, cL1A, cCalPulse, cBC0);
}

void D19cFWInterface::PS_Close_shutter(uint32_t pDuration)
{
    uint8_t cReSync   = 0;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 0;
    uint8_t cBC0      = 1;
    this->Compose_fast_command(pDuration, cReSync, cL1A, cCalPulse, cBC0);
}

// some overlap for now...
void D19cFWInterface::Send_pulses(uint32_t pNtriggers, bool manual)
{
    if(manual)
    {
        // LOG(INFO) << "Send_pulses";
        for(uint16_t numit = 0; numit < pNtriggers; numit++) this->ChipTestPulse();
    }
    else
    {
        this->WriteReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNtriggers);
        this->WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);

        usleep(10);

        this->WriteReg("fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);
        uint32_t nsleeps   = 0;
        uint32_t maxsleeps = 1000;
        while(ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") and (nsleeps < maxsleeps))
        {
            nsleeps += 1;
            usleep(10);
        }
        if(nsleeps == maxsleeps)
        {
            LOG(INFO) << "Cal pulses timeout";
            PS_Clear_counters();
            this->WriteReg("fc7_daq_ctrl.fast_command_block.control.reset", 0x1);
            usleep(10);
            this->WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);
            usleep(10);
            Send_pulses(pNtriggers, manual);
        }
        WriteReg("fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);
    }
}

void D19cFWInterface::PS_Clear_counters(uint32_t pDuration)
{
    uint8_t cReSync   = 0;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 1;
    uint8_t cBC0      = 1;
    // clear
    this->Compose_fast_command(pDuration, cReSync, cL1A, cCalPulse, cBC0);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
}
void D19cFWInterface::PS_Start_counters_read(uint32_t pDuration)
{
    uint8_t cReSync   = 1;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 0;
    uint8_t cBC0      = 1;
    this->Compose_fast_command(pDuration, cReSync, cL1A, cCalPulse, cBC0);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
}

void D19cFWInterface::KillI2C()
{
    PSInterfaceBoard_SendI2CCommand(0, 0, 0, 0, 0x04);
    PSInterfaceBoard_ConfigureI2CMaster(0);
}

// POWER:
void D19cFWInterface::PSInterfaceBoard_PowerOn(uint8_t mpaid, uint8_t ssaid)
{
    uint32_t write       = 0;
    uint32_t SLOW        = 2;
    uint32_t i2cmux      = 0;
    uint32_t powerenable = 2;

    PSInterfaceBoard_SetSlaveMap();

    LOG(INFO) << "Interface Board Power ON";

    PSInterfaceBoard_ConfigureI2CMaster(1, SLOW);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x02);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    PSInterfaceBoard_SendI2CCommand(powerenable, 0, write, 0, 0x00); // There is an inverter! Be Careful!
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    PSInterfaceBoard_ConfigureI2CMaster(0, SLOW);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void D19cFWInterface::SSAEqualizeDACs(uint8_t pChipId)
{
    uint32_t write   = 0;
    uint32_t read    = 1;
    uint32_t SLOW    = 2;
    uint32_t i2cmux  = 0;
    uint32_t ltc2487 = 3;

    uint16_t chipSelect = 0x0;
    if(pChipId == 1) { chipSelect = 0xb180; }
    if(pChipId == 0) { chipSelect = 0xb080; }
    PSInterfaceBoard_SetSlaveMap();
    PSInterfaceBoard_ConfigureI2CMaster(1, SLOW);
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    PSInterfaceBoard_SendI2CCommand(ltc2487, 0, write, 0, chipSelect);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    uint32_t readSSA = PSInterfaceBoard_SendI2CCommand_READ(ltc2487, 0, read, 0x0, 0); // read value in reg:
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    readSSA = (readSSA >> 6) & 0x0000FFFF;
    LOG(INFO) << RED << "Value read back: " << (float(readSSA) / 43371.0) << RESET;

    ReadPower_SSA();
}

void D19cFWInterface::PSInterfaceBoard_PowerOff()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint32_t write       = 0;
    uint32_t SLOW        = 2;
    uint32_t i2cmux      = 0;
    uint32_t powerenable = 2;

    PSInterfaceBoard_SetSlaveMap();

    LOG(INFO) << "Interface Board Power OFF";

    PSInterfaceBoard_ConfigureI2CMaster(1, SLOW);
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x02);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    PSInterfaceBoard_SendI2CCommand(powerenable, 0, write, 0, 0x01);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    PSInterfaceBoard_ConfigureI2CMaster(0, SLOW);
}

void D19cFWInterface::ReadPower_SSA(uint8_t mpaid, uint8_t ssaid)
{
    uint32_t read     = 1;
    uint32_t write    = 0;
    uint32_t SLOW     = 2;
    uint32_t i2cmux   = 0;
    uint32_t ina226_7 = 7;
    uint32_t ina226_6 = 6;
    uint32_t ina226_5 = 5;

    LOG(INFO) << BOLDBLUE << "power information:" << RESET;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    PSInterfaceBoard_SetSlaveMap();
    PSInterfaceBoard_ConfigureI2CMaster(1, SLOW);

    LOG(INFO) << BOLDBLUE << " - - - VDD:" << RESET;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x08);
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    uint32_t dread2 = PSInterfaceBoard_SendI2CCommand_READ(ina226_7, 0, read, 0x02, 0);
    LOG(INFO) << BOLDRED << "BIT VAL OF VDD = " << dread2 << RESET;
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    float    vret   = float(dread2) * 0.00125;
    uint32_t dread1 = PSInterfaceBoard_SendI2CCommand_READ(ina226_7, 0, read, 0x01, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    float iret = float(dread1) * 0.00250 / 0.1;
    float pret = vret * iret;
    LOG(INFO) << BOLDGREEN << "V = " << vret << "V, I = " << iret << "mA, P = " << pret << "mW" << RESET;

    LOG(INFO) << BOLDBLUE << " - - - Digital:" << RESET;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x08);
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    dread2 = PSInterfaceBoard_SendI2CCommand_READ(ina226_6, 0, read, 0x02, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    vret   = float(dread2) * 0.00125;
    dread1 = PSInterfaceBoard_SendI2CCommand_READ(ina226_6, 0, read, 0x01, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    iret = float(dread1) * 0.00250 / 0.1;
    pret = vret * iret;
    LOG(INFO) << BOLDGREEN << "V = " << vret << "V, I = " << iret << "mA, P = " << pret << "mW" << RESET;

    LOG(INFO) << BOLDBLUE << " - - - Analog:" << RESET;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x08);
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    dread2 = PSInterfaceBoard_SendI2CCommand_READ(ina226_5, 0, read, 0x02, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    vret   = float(dread2) * 0.00125;
    dread1 = PSInterfaceBoard_SendI2CCommand_READ(ina226_5, 0, read, 0x01, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    iret = float(dread1) * 0.00250 / 0.1;
    pret = vret * iret;
    LOG(INFO) << BOLDGREEN << "V = " << vret << "V, I = " << iret << "mA, P = " << pret << "mW" << RESET;

    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x04);
    PSInterfaceBoard_ConfigureI2CMaster(0);
}

void D19cFWInterface::PSInterfaceBoard_PowerOn_MPA(float VDDPST, float DVDD, float AVDD, float VBG, uint8_t mpaid, uint8_t ssaid)
{
    uint32_t                  write   = 0;
    uint32_t                  SLOW    = 2;
    uint32_t                  i2cmux  = 0;
    uint32_t                  pcf8574 = 1;
    uint32_t                  dac7678 = 4;
    std::chrono::milliseconds cWait(10);
    this->getBoardInfo();
    this->PSInterfaceBoard_PowerOn(0, 0);
    PSInterfaceBoard_SetSlaveMap();
    PSInterfaceBoard_ConfigureI2CMaster(1, SLOW);

    float Vc = 0.0003632813;

    LOG(INFO) << "mpa vdd on";

    float Vlimit = 1.32;
    if(VDDPST > Vlimit) VDDPST = Vlimit;
    float    diffvoltage = 1.5 - VDDPST;
    uint32_t setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;

    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x34, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "mpa vddD on";
    Vlimit = 1.2;
    if(DVDD > Vlimit) DVDD = Vlimit;
    diffvoltage = 1.5 - DVDD;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x30, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "mpa vddA on";
    Vlimit = 1.32;
    if(AVDD > Vlimit) AVDD = Vlimit;
    diffvoltage = 1.5 - AVDD;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x32, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "mpa VBG on";
    Vlimit = 0.5;
    if(VBG > Vlimit) VBG = Vlimit;
    float Vc2  = 4095 / 1.5;
    setvoltage = int(round(VBG * Vc2));
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x36, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "mpa enable";
    uint32_t val2 = (mpaid << 5) + 16;
    // uint32_t val2 = (mpaid << 5) + (ssaid << 1) + 1; // reset bit for MPA
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x02);  // route to 2nd PCF8574
    PSInterfaceBoard_SendI2CCommand(pcf8574, 0, write, 0, val2); // set reset bit
    std::this_thread::sleep_for(cWait);

    // disable the i2c master at the end (first set the mux to the chip
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x04);
    PSInterfaceBoard_ConfigureI2CMaster(0, SLOW);
}

void D19cFWInterface::PSInterfaceBoard_PowerOn_MPASSA(float VDDPST, float DVDD, float AVDD, float VBG, float VBF, uint8_t mpaid, uint8_t ssaid)
{
    this->getBoardInfo();
    this->PSInterfaceBoard_PowerOn(0, 0);

    uint32_t write   = 0;
    uint32_t SLOW    = 2;
    uint32_t i2cmux  = 0;
    uint32_t pcf8574 = 1;
    uint32_t dac7678 = 4;
    std::this_thread::sleep_for(std::chrono::milliseconds(750));
    PSInterfaceBoard_SetSlaveMap();
    PSInterfaceBoard_ConfigureI2CMaster(1, SLOW);
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    float Vc = 0.0003632813;

    LOG(INFO) << "mpa vdd on";

    float Vlimit = 1.32;
    if(VDDPST > Vlimit) VDDPST = Vlimit;
    float    diffvoltage = 1.5 - VDDPST;
    uint32_t setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;

    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x34, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    LOG(INFO) << "ssa vdd on";

    Vlimit = 1.32;
    if(VDDPST > Vlimit) VDDPST = Vlimit;
    diffvoltage = 1.5 - VDDPST;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;

    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01); // to SCO on PCA9646
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x33, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    LOG(INFO) << "mpa vddD on";
    Vlimit = 1.2;
    if(DVDD > Vlimit) DVDD = Vlimit;
    diffvoltage = 1.5 - DVDD;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x30, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    LOG(INFO) << "ssa vddD on";
    Vlimit = 1.32;
    if(DVDD > Vlimit) DVDD = Vlimit;
    diffvoltage = 1.5 - DVDD;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01); // to SCO on PCA9646
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x31, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    LOG(INFO) << "mpa vddA on";
    Vlimit = 1.32;
    if(AVDD > Vlimit) AVDD = Vlimit;
    diffvoltage = 1.5 - AVDD;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x32, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    LOG(INFO) << "ssa vddA on";
    Vlimit = 1.32;
    if(AVDD > Vlimit) AVDD = Vlimit;
    diffvoltage = 1.5 - AVDD;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01); // to SCO on PCA9646
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x35, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    LOG(INFO) << "mpa VBG on";
    Vlimit = 0.5;
    if(VBG > Vlimit) VBG = Vlimit;
    float Vc2  = 4095 / 1.5;
    setvoltage = int(round(VBG * Vc2));
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x36, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    /*LOG(INFO) << "ssa VBG on";
    Vlimit = 1.32;
    if(VBG > Vlimit) VBG = Vlimit;
    Vc2  = 4095 / 1.5;
    setvoltage = int(round(VBG * Vc2));
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01); // to SCO on PCA9646
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x36, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));*/

    LOG(INFO) << "ssa VBF on";
    Vlimit = 0.5;
    if(VBF > Vlimit) VBF = Vlimit;
    setvoltage = int(round(VBF * Vc2));
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01); // to SCO on PCA9646
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x37, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x02);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    PSInterfaceBoard_SendI2CCommand(pcf8574, 0, write, 0, 145); // set reset bit

    /*LOG(INFO) << "mpa enable";
    //uint32_t val2 = (mpaid << 5) + 16;
    uint32_t val2 = (mpaid << 5) + (ssaid << 1) + 1; // reset bit for MPA
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x02);  // route to 2nd PCF8574
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    PSInterfaceBoard_SendI2CCommand(pcf8574, 0, write, 0, val2); // set reset bit
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));*/

    // disable the i2c master at the end (first set the mux to the chip
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x04);
    PSInterfaceBoard_ConfigureI2CMaster(0, SLOW);
}

void D19cFWInterface::PSInterfaceBoard_PowerOff_SSA(uint8_t mpaid, uint8_t ssaid)
{
    uint32_t                  write   = 0;
    uint32_t                  SLOW    = 2;
    uint32_t                  i2cmux  = 0;
    uint32_t                  pcf8574 = 1; // MPA and SSA address and reset 8 bit port
    uint32_t                  dac7678 = 4;
    float                     Vc      = 0.0003632813; // V/Dac step
    std::chrono::milliseconds cWait(1500);

    PSInterfaceBoard_SetSlaveMap();
    PSInterfaceBoard_ConfigureI2CMaster(1, SLOW);

    LOG(INFO) << "ssa disable";
    uint32_t val = (mpaid << 5) + (ssaid << 1);                 // reset bit for MPA
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x02); // route to 2nd PCF8574
    PSInterfaceBoard_SendI2CCommand(pcf8574, 0, write, 0, val); // set reset bit
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "ssa VBF off";
    uint32_t setvoltage = 0;
    setvoltage          = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x37, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "ssa vddA off";
    float diffvoltage = 1.5;
    setvoltage        = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);  // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x35, 0); // tx to DAC C
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "ssa vddD off";
    diffvoltage = 1.5;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);  // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x31, 0); // tx to DAC C
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "ssa vdd off";
    diffvoltage = 1.5;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);  // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x33, 0); // tx to DAC C
    std::this_thread::sleep_for(cWait);

    this->PSInterfaceBoard_PowerOff();
}

void D19cFWInterface::PSInterfaceBoard_PowerOff_MPA(uint8_t mpaid, uint8_t ssaid)
{
    uint32_t                  write   = 0;
    uint32_t                  SLOW    = 2;
    uint32_t                  i2cmux  = 0;
    uint32_t                  pcf8574 = 1; // MPA and SSA address and reset 8 bit port
    uint32_t                  dac7678 = 4;
    float                     Vc      = 0.0003632813; // V/Dac step
    std::chrono::milliseconds cWait(1000);

    PSInterfaceBoard_SetSlaveMap();
    PSInterfaceBoard_ConfigureI2CMaster(1, SLOW);

    LOG(INFO) << "mpa disable";
    uint32_t val = (mpaid << 5) + (ssaid << 1);                 // reset bit for MPA
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x02); // route to 2nd PCF8574
    PSInterfaceBoard_SendI2CCommand(pcf8574, 0, write, 0, val); // set reset bit
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "mpa VBG off";
    uint32_t setvoltage = 0;
    setvoltage          = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x36, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "mpa vddA off";
    float diffvoltage = 1.5;
    setvoltage        = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x32, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "mpa vddA off";
    diffvoltage = 1.5;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x30, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(cWait);

    LOG(INFO) << "mpa vdd off";
    diffvoltage = 1.5;
    setvoltage  = int(round(diffvoltage / Vc));
    if(setvoltage > 4095) setvoltage = 4095;
    setvoltage = setvoltage << 4;
    PSInterfaceBoard_SendI2CCommand(i2cmux, 0, write, 0, 0x01);           // to SCO on PCA9646
    PSInterfaceBoard_SendI2CCommand(dac7678, 0, write, 0x34, setvoltage); // tx to DAC C
    std::this_thread::sleep_for(cWait);
}

// disconnect setup with multiplexing backplane
// disconnect setup with multiplexing backplane
void D19cFWInterface::DisconnectMultiplexingSetup(uint8_t pWait_ms)
{
    LOG(INFO) << BOLDBLUE << "Disconnect multiplexing set-up" << RESET;

    bool L12Power = (ReadReg("sysreg.fmc_pwr.l12_pwr_en") == 1);
    bool L8Power  = (ReadReg("sysreg.fmc_pwr.l8_pwr_en") == 1);
    bool PGC2M    = (ReadReg("sysreg.fmc_pwr.pg_c2m") == 1);
    if(!L12Power)
    {
        LOG(ERROR) << RED << "Power on L12 is not enabled" << RESET;
        throw std::runtime_error("FC7 power is not enabled!");
    }
    if(!L8Power)
    {
        LOG(ERROR) << RED << "Power on L8 is not enabled" << RESET;
        throw std::runtime_error("FC7 power is not enabled!");
    }
    if(!PGC2M)
    {
        LOG(ERROR) << RED << "PG C2M is not enabled" << RESET;
        throw std::runtime_error("FC7 power is not enabled!");
    }

    bool BackplanePG   = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.backplane_powergood") == 1);
    bool CardPG        = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.card_powergood") == 1);
    bool SystemPowered = false;
    if(BackplanePG && CardPG)
    {
        LOG(INFO) << BOLDBLUE << "Back-plane power good and card power good." << RESET;
        WriteReg("fc7_daq_ctrl.physical_interface_block.multiplexing_bp.setup_disconnect", 0x1);
        SystemPowered = true;
    }
    else
    {
        LOG(INFO) << GREEN << "============================" << RESET;
        LOG(INFO) << BOLDGREEN << "Setup is disconnected" << RESET;
    }
    if(SystemPowered)
    {
        bool CardsDisconnected      = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.cards_disconnected") == 1);
        bool c                      = false;
        bool BackplanesDisconnected = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.backplanes_disconnected") == 1);
        bool b                      = false;
        LOG(INFO) << GREEN << "============================" << RESET;
        LOG(INFO) << BOLDGREEN << "Disconnecting setup" << RESET;

        while(!CardsDisconnected)
        {
            if(c == false) LOG(INFO) << "Disconnecting cards";
            c = true;
            std::this_thread::sleep_for(std::chrono::microseconds(pWait_ms * 1000));
            CardsDisconnected = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.cards_disconnected") == 1);
            LOG(DEBUG) << BOLDBLUE << "Set-up scanned : " << +ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_scanned") << RESET;
        }

        while(!BackplanesDisconnected)
        {
            if(b == false) LOG(INFO) << "Disconnecting backplanes";
            b = true;
            std::this_thread::sleep_for(std::chrono::microseconds(pWait_ms * 1000));
            BackplanesDisconnected = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.backplanes_disconnected") == 1);
            LOG(DEBUG) << BOLDBLUE << "Set-up scanned : " << +ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_scanned") << RESET;
        }

        if(CardsDisconnected && BackplanesDisconnected)
        {
            LOG(INFO) << GREEN << "============================" << RESET;
            LOG(INFO) << BOLDGREEN << "Setup is disconnected" << RESET;
        }
    }
}

// scan setup with multiplexing backplane
// scan setup with multiplexing backplane
uint32_t D19cFWInterface::ScanMultiplexingSetup(uint8_t pWait_ms)
{
    int AvailableBackplanesCards = 0;
    this->DisconnectMultiplexingSetup();

    LOG(INFO) << BOLDBLUE << "Sending a global reset to the FC7 ..... " << RESET;
    this->WriteReg("fc7_daq_ctrl.command_processor_block.global.reset", 0x1);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    bool ConfigurationRequired = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.configuration_required") == 1);
    bool SystemNotConfigured   = false;
    if(ConfigurationRequired)
    {
        SystemNotConfigured = true;
        WriteReg("fc7_daq_ctrl.physical_interface_block.multiplexing_bp.setup_scan", 0x1);
    }

    if(SystemNotConfigured == true)
    {
        bool SetupScanned = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_scanned") == 1);
        bool s            = false;
        LOG(INFO) << GREEN << "============================" << RESET;
        LOG(INFO) << BOLDGREEN << "Scan setup" << RESET;
        while(!SetupScanned)
        {
            if(s == false) LOG(INFO) << "Scanning setup";
            s = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(pWait_ms));
            SetupScanned = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_scanned") == 1);
        }

        if(SetupScanned)
        {
            LOG(INFO) << GREEN << "============================" << RESET;
            LOG(INFO) << BOLDGREEN << "Setup is scanned" << RESET;
            AvailableBackplanesCards = ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.available_backplanes_cards");
        }
    }
    return AvailableBackplanesCards;
}

// configure setup with multiplexing backplane
void D19cFWInterface::ConfigureMultiplexingSetup(int BackplaneNum, int CardNum, uint8_t pWait_ms)
{
    this->DisconnectMultiplexingSetup();
    WriteReg("fc7_daq_cnfg.physical_interface_block.multiplexing_bp.backplane_num", 0xF & ~(1 << (3 - BackplaneNum)));
    WriteReg("fc7_daq_cnfg.physical_interface_block.multiplexing_bp.card_num", 0xF & ~(1 << (3 - CardNum)));
    std::this_thread::sleep_for(std::chrono::milliseconds(pWait_ms));
    bool ConfigurationRequired = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.configuration_required") == 1);
    bool SystemNotConfigured   = false;
    if(ConfigurationRequired)
    {
        SystemNotConfigured = true;
        WriteReg("fc7_daq_ctrl.physical_interface_block.multiplexing_bp.setup_configure", 0x1);
        std::this_thread::sleep_for(std::chrono::microseconds(pWait_ms * 1000));
    }

    if(SystemNotConfigured == true)
    {
        bool SetupScanned   = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_scanned") == 1);
        bool BackplaneValid = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.backplane_valid") == 1);
        bool CardValid      = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.card_valid") == 1);
        if(SetupScanned)
        {
            if(BackplaneValid) { LOG(INFO) << BLUE << "Backplane configuration VALID" << RESET; }
            else
            {
                LOG(ERROR) << RED << "Backplane configuration is NOT VALID" << RESET;
                throw std::runtime_error(std::string("Backplane configuration is NOT VALID"));
            }
            if(CardValid) { LOG(INFO) << BLUE << "Card configuration VALID" << RESET; }
            else
            {
                LOG(ERROR) << RED << "Card configuration is NOT VALID" << RESET;
                throw std::runtime_error(std::string("Card configuration is NOT VALID"));
            }
        }
        else
            LOG(ERROR) << RED << "First you must scan the setup! Map of present backplanes and cards is not available!" << RESET;

        bool SetupConfigured = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_configured") == 1);
        bool c               = false;
        if(BackplaneValid && CardValid)
        {
            LOG(INFO) << GREEN << "============================" << RESET;
            LOG(INFO) << BOLDGREEN << "Configure setup" << RESET;
            const auto MAXNRETRY = 100;
            auto       NTrials   = 0;
            while(!SetupConfigured && NTrials < MAXNRETRY)
            {
                if(c == false) LOG(INFO) << "Configuring setup";
                c = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(pWait_ms));
                SetupConfigured = (ReadReg("fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_configured") == 1);
                NTrials++;
            }

            if(SetupConfigured)
            {
                LOG(INFO) << GREEN << "============================" << RESET;
                LOG(INFO) << BOLDGREEN << "Setup with backplane " << BackplaneNum << " and card " << CardNum << " is configured" << RESET;
            }
            else
            {
                LOG(INFO) << GREEN << "============================" << RESET;
                LOG(INFO) << BOLDRED << "Setup is not configured. Problems with card power good signal! Check the HW!" << RESET;
            }
        }
    }
}
// MPA specific
void D19cFWInterface::Pix_write_MPA(Chip* cMPA, ChipRegItem cRegItem, uint32_t row, uint32_t pixel, uint32_t data)
{
    uint8_t cWriteAttempts = 0;

    ChipRegItem rowreg = cRegItem;
    rowreg.fAddress    = ((row & 0x0001f) << 11) | ((cRegItem.fAddress & 0x000f) << 7) | (pixel & 0xfffffff);
    rowreg.fValue      = data;
    std::vector<uint32_t> cVecReq;
    cVecReq.clear();
    this->EncodeReg(rowreg, cMPA->getHybridId(), cMPA->getId(), cVecReq, false, true);
    this->WriteChipBlockReg(cVecReq, cWriteAttempts, false);
}

uint32_t D19cFWInterface::Pix_read_MPA(Chip* cMPA, ChipRegItem cRegItem, uint32_t row, uint32_t pixel)
{
    uint8_t  cWriteAttempts = 0;
    uint32_t rep;

    std::vector<uint32_t> cVecReq;
    cVecReq.clear();
    this->EncodeReg(cRegItem, cMPA->getHybridId(), cMPA->getId(), cVecReq, false, false);
    this->WriteChipBlockReg(cVecReq, cWriteAttempts, false);
    // std::chrono::milliseconds cShort( 1 );
    // uint32_t readempty = ReadReg ("fc7_daq_stat.command_processor_block.i2c.reply_fifo.empty");
    // while (readempty == 0)
    //  {
    //  std::cout<<"RE:"<<readempty<<std::endl;
    //  //ReadStatus()
    //  std::this_thread::sleep_for( cShort );
    //  readempty = ReadReg ("fc7_daq_stat.command_processor_block.i2c.reply_fifo.empty");
    //  }
    // uint32_t forcedreply = ReadReg("fc7_daq_ctrl.command_processor_block.i2c.reply_fifo");
    rep = ReadReg("fc7_daq_ctrl.command_processor_block.i2c.mpa_ssa_i2c_reply.data");

    return rep;
}

void D19cFWInterface::Align_out()
{
    int cCounter     = 0;
    int cMaxAttempts = 10;

    uint32_t hardware_ready = 0;

    while(hardware_ready < 1)
    {
        if(cCounter++ > cMaxAttempts)
        {
            uint32_t delay5_done_cbc0     = ReadReg("fc7_daq_stat.physical_interface_block.delay5_done_cbc0");
            uint32_t serializer_done_cbc0 = ReadReg("fc7_daq_stat.physical_interface_block.serializer_done_cbc0");
            uint32_t bitslip_done_cbc0    = ReadReg("fc7_daq_stat.physical_interface_block.bitslip_done_cbc0");

            uint32_t delay5_done_cbc1     = ReadReg("fc7_daq_stat.physical_interface_block.delay5_done_cbc1");
            uint32_t serializer_done_cbc1 = ReadReg("fc7_daq_stat.physical_interface_block.serializer_done_cbc1");
            uint32_t bitslip_done_cbc1    = ReadReg("fc7_daq_stat.physical_interface_block.bitslip_done_cbc1");
            LOG(INFO) << "Clock Data Timing tuning failed after " << cMaxAttempts << " attempts with value - aborting!";
            LOG(INFO) << "Debug Info CBC0: delay5 done: " << delay5_done_cbc0 << ", serializer_done: " << serializer_done_cbc0 << ", bitslip_done: " << bitslip_done_cbc0;
            LOG(INFO) << "Debug Info CBC1: delay5 done: " << delay5_done_cbc1 << ", serializer_done: " << serializer_done_cbc1 << ", bitslip_done: " << bitslip_done_cbc1;
            uint32_t tuning_state_cbc0 = ReadReg("fc7_daq_stat.physical_interface_block.state_tuning_cbc0");
            uint32_t tuning_state_cbc1 = ReadReg("fc7_daq_stat.physical_interface_block.state_tuning_cbc1");
            LOG(INFO) << "tuning state cbc0: " << tuning_state_cbc0 << ", cbc1: " << tuning_state_cbc1;
            throw std::runtime_error("Clock Data Timing tuning failed");
        }

        this->ChipReSync();
        usleep(10);
        // reset  the timing tuning
        WriteReg("fc7_daq_ctrl.physical_interface_block.control.cbc3_tune_again", 0x1);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        hardware_ready = ReadReg("fc7_daq_stat.physical_interface_block.hardware_ready");
    }
}

// ##########################################
// # Low level opto-link read and write
// #########################################
void D19cFWInterface::ResetOptoLink() { this->WriteStackReg({{"fc7_daq_ctrl.optical_block.ic", 0x00}, {"fc7_daq_cnfg.optical_block.ic", 0x00}, {"fc7_daq_cnfg.optical_block.gbtx", 0x00}}); }

bool D19cFWInterface::WriteOptoLpGBTRegister(const uint32_t linkNumber, const uint32_t pAddress, const uint32_t pData, const bool pVerifLoop)
{
    LOG(DEBUG) << BOLDMAGENTA << "D19cFWInterface::WriteOptoLpGBTRegister" << RESET;
    // Reset
    ResetOptoLink();
    selectLink(linkNumber);
    // Config transaction register
    this->WriteStackReg({{"fc7_daq_cnfg.optical_block.gbtx.address", flpGBTAddress}, {"fc7_daq_cnfg.optical_block.gbtx.data", pData}, {"fc7_daq_cnfg.optical_block.ic.register", pAddress}});
    // Perform transaction
    this->WriteStackReg({{"fc7_daq_ctrl.optical_block.ic.write", 0x01}, {"fc7_daq_ctrl.optical_block.ic.write", 0x00}});
    //
    this->WriteStackReg({{"fc7_daq_ctrl.optical_block.ic.start_write", 0x01}, {"fc7_daq_ctrl.optical_block.ic.start_write", 0x00}});

    if(!pVerifLoop) return true;
    uint8_t cReadBack = ReadOptoLpGBTRegister(linkNumber, pAddress);
    uint8_t cIter = 0, cMaxIter = 50;
    while(cReadBack != pData && cIter < cMaxIter)
    {
        LOG(INFO) << BOLDRED << "[D19cFWInterface::WriteOptoLinkRegister] : lpGBT register write mismatch... retrying" << RESET;
        // Config transaction register
        this->WriteStackReg({{"fc7_daq_cnfg.optical_block.gbtx.address", flpGBTAddress}, {"fc7_daq_cnfg.optical_block.gbtx.data", pData}, {"fc7_daq_cnfg.optical_block.ic.register", pAddress}});
        // Perform transaction
        this->WriteStackReg({{"fc7_daq_ctrl.optical_block.ic.write", 0x01}, {"fc7_daq_ctrl.optical_block.ic.write", 0x00}});
        //
        this->WriteStackReg({{"fc7_daq_ctrl.optical_block.ic.start_write", 0x01}, {"fc7_daq_ctrl.optical_block.ic.start_write", 0x00}});
        cReadBack = ReadOptoLpGBTRegister(linkNumber, pAddress);
        cIter++;
    }
    if(cIter == cMaxIter) throw std::runtime_error(std::string("lpGBT register write mismatch"));
    return true;
}

uint32_t D19cFWInterface::ReadOptoLpGBTRegister(const uint32_t linkNumber, const uint32_t pAddress)
{
    // Reset
    ResetOptoLink();
    selectLink(linkNumber);
    // Config transaction register
    this->WriteStackReg({{"fc7_daq_cnfg.optical_block.gbtx.address", flpGBTAddress}, {"fc7_daq_cnfg.optical_block.ic.register", pAddress}, {"fc7_daq_cnfg.optical_block.ic.nwords", 0x01}});
    // Perform transaction
    this->WriteStackReg({{"fc7_daq_ctrl.optical_block.ic.start_read", 0x01}, {"fc7_daq_ctrl.optical_block.ic.start_read", 0x00}});
    //
    this->WriteStackReg({{"fc7_daq_ctrl.optical_block.ic.read", 0x01}, {"fc7_daq_ctrl.optical_block.ic.read", 0x00}});
    uint32_t cReadBack = this->ReadReg("fc7_daq_stat.optical_block.ic.data");
    LOG(DEBUG) << BOLDWHITE << "\t Reading 0x" << std::hex << +cReadBack << std::dec << " from [0x" << std::hex << +pAddress << std::dec << "]" << RESET;
    return cReadBack;
}

// ##########################################
// # Read/Write new Command Processor Block #
// #########################################
void D19cFWInterface::ResetCPB()
{
    LOG(DEBUG) << BOLDBLUE << "Resetting CPB" << RESET;
    // Soft reset the GBT-SC worker
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    uint8_t cWorkerId = 0, cFunctionId = 2;
    // reset shoudl be 0x00020010
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | 16 << 0);
    WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", cCommandVector);
    ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", 10);
    std::this_thread::sleep_for(std::chrono::microseconds(fCPBConfig.fWait_us));
}

void D19cFWInterface::WriteCommandCPB(const std::vector<uint32_t>& pCommandVector)
{
    uint8_t cWordIndex = 0;
    if(fCPBConfig.fVerbose)
    {
        for(auto cCommandWord: pCommandVector)
        {
            LOG(INFO) << GREEN << "\t Write command word " << +cWordIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cCommandWord << std::dec << RESET;
            cWordIndex++;
        }
    }
    WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", pCommandVector);
    std::this_thread::sleep_for(std::chrono::microseconds(fCPBConfig.fWait_us));
}

std::vector<uint32_t> D19cFWInterface::ReadReplyCPB(uint8_t pNWords)
{
    std::vector<uint32_t> cReplyVector = ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", pNWords);
    uint8_t               cFifoIndex   = 0;
    if(fCPBConfig.fVerbose)
    {
        for(auto cReplyWord: cReplyVector)
        {
            LOG(INFO) << YELLOW << "\t Read reply word " << +cFifoIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cReplyWord << std::dec << RESET;
            cFifoIndex++;
        }
        LOG(INFO) << "\t lpgbtsc FSM state : 0b" << std::bitset<8>(ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state")) << RESET;
    }
    return cReplyVector;
}

// ##########################################
// # Read/Write lpGBT registers with CPB #
// #########################################

bool D19cFWInterface::WriteLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerifLoop)
{
    if(fCPBConfig.fVerbose) LOG(INFO) << BOLDMAGENTA << "D19cFWInterface::WriteLpGBTRegister" << RESET;
    size_t cExpectedReplySize = 10 * 1;
    this->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    if(fCPBConfig.fResetEn) ResetCPB();
    // Use new Command Processor Block
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 3;
    if(fCPBConfig.fVerbose) LOG(INFO) << BOLDMAGENTA << "WriteLpGBTRegister to Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;

    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pRegisterAddress << 0);
    cCommandVector.push_back(pRegisterValue << 0);
    WriteCommandCPB(cCommandVector);
    std::vector<uint32_t> cReplyVector     = ReadReplyCPB(cExpectedReplySize);
    uint8_t               cParityCheck     = cReplyVector[2] & 0xFF;
    uint8_t               cReadBack        = cReplyVector[7] & 0xFF;
    uint16_t              cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
    // always check parity
    size_t cIter             = 0;
    bool   cValidTransaction = (cParityCheck == 1);
    while(!cValidTransaction && fCPBConfig.fReTry && cIter < fCPBConfig.fMaxAttempts)
    {
        if(fCPBConfig.fVerbose)
            LOG(INFO) << BOLDRED << "[Iter# " << cIter << "/" << fCPBConfig.fMaxAttempts
                      << " of D19cFWInterface::WriteLpGBTRegister] : Received corrupted reply (mismatch in readbacks or failed parity check) from command processor block ... retrying" << RESET;
        ResetCPB();
        cReplyVector.clear();
        WriteCommandCPB(cCommandVector);
        cReplyVector      = ReadReplyCPB(cExpectedReplySize);
        cParityCheck      = cReplyVector[2] & 0xFF;
        cReadBackRegAddr  = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
        cReadBack         = cReplyVector[7] & 0xFF;
        cValidTransaction = (cParityCheck == 1);
        if(cValidTransaction && pVerifLoop)
        {
            cValidTransaction = cValidTransaction && ((cReadBack == pRegisterValue) && (cReadBackRegAddr == pRegisterAddress));
            if(fCPBConfig.fVerbose && !cValidTransaction)
                LOG(INFO) << BOLDRED << "[Iter# " << cIter << "/" << fCPBConfig.fMaxAttempts
                          << " of D19cFWInterface::WriteLpGBTRegister] : Received corrupted reply (mismatch in readbacks) from command processor block ... retrying" << RESET;
        }
        cIter++;
    }
    // throw exception based on failure
    if(cIter > 1) LOG(INFO) << BOLDYELLOW << "D19cFWInterface::WriteLpGBTRegister had to try " << cIter << "/" << fCPBConfig.fMaxAttempts << " possible attempts to complete transaction." << RESET;
    if(cParityCheck != 1) throw std::runtime_error(std::string("[D19cFWInterface::WriteLpGBTRegister] : Received corrupted reply from command processor block - failed parity check"));
    if(pVerifLoop && cReadBack != pRegisterValue)
        throw std::runtime_error(std::string("[D19cFWInterface::WriteLpGBTRegister] : Received corrupted reply from command processor block - mismatch in read-back lpGBT register value"));
    if(pVerifLoop && cReadBackRegAddr != pRegisterAddress)
        throw std::runtime_error(std::string("[D19cFWInterface::WriteLpGBTRegister] : Received corrupted reply from command processor block - mismatch in read-back lpGBT register address"));
    return (pVerifLoop) ? ((cReadBack == pRegisterValue) && (cReadBackRegAddr == pRegisterAddress)) : (cParityCheck == 1);
}

uint8_t D19cFWInterface::ReadLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress)
{
    size_t cExpectedReplySize = 10 * 1;
    this->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    if(fCPBConfig.fResetEn) ResetCPB();
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 2;
    if(fCPBConfig.fVerbose) LOG(INFO) << BOLDMAGENTA << "ReadLpGBTRegister from Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pRegisterAddress << 0);
    WriteCommandCPB(cCommandVector);
    std::vector<uint32_t> cReplyVector     = ReadReplyCPB(cExpectedReplySize);
    uint8_t               cReadBack        = cReplyVector[7] & 0xFF;
    uint16_t              cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
    // uint8_t               cParityCheck     = cReplyVector[2] & 0xFF;
    size_t cIter = 0;
    while((cReadBackRegAddr != pRegisterAddress) && cIter < fCPBConfig.fMaxAttempts && fCPBConfig.fReTry)
    {
        if(fCPBConfig.fResetEn) ResetCPB();
        if(fCPBConfig.fVerbose)
            LOG(INFO) << BOLDRED << "[Iter# " << cIter << "/" << fCPBConfig.fMaxAttempts
                      << " of D19cFWInterface::ReadLpGBTRegister] : Received corrupted reply from command processor block ... retrying" << RESET;
        cReplyVector.clear();
        WriteCommandCPB(cCommandVector);
        cReplyVector     = ReadReplyCPB(cExpectedReplySize);
        cReadBack        = cReplyVector[7] & 0xFF;
        cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
        cIter++;
    };
    if(cIter == (size_t)fCPBConfig.fMaxAttempts) throw std::runtime_error(std::string("[D19cFWInterface::ReadLpGBTRegister] : Received corrupted reply from command processor block"));
    // LOG(DEBUG) << BOLDWHITE << "\t Reading 0x" << std::hex << +cReadBack << std::dec << " from [0x" << std::hex << +pRegisterAddress << std::dec << "]" << RESET;
    return cReadBack;
}

// ##########################################
// # Read/Write registers over optical link #
// #########################################

bool D19cFWInterface::WriteOptoLinkRegister(const Ph2_HwDescription::Chip* pChip, const uint32_t pAddress, const uint32_t pData, const bool pVerifLoop)
{
    if(fCPBConfig.fEnable)
        return WriteLpGBTRegister(pChip->getOpticalId(), pAddress, pData, pVerifLoop);
    else
        return WriteOptoLpGBTRegister(pChip->getOpticalId(), pAddress, pData, pVerifLoop);
}
uint32_t D19cFWInterface::ReadOptoLinkRegister(const Ph2_HwDescription::Chip* pChip, const uint32_t pAddress)
{
    if(fCPBConfig.fEnable)
        return ReadLpGBTRegister(pChip->getOpticalId(), pAddress);
    else
        return ReadOptoLpGBTRegister(pChip->getOpticalId(), pAddress);
}

// ##########################################
// # Read/Write registers with CPB I2C functions #
// #########################################
bool D19cFWInterface::I2CWrite(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint32_t pSlaveData, uint8_t pNBytes)
{
    this->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    if(fCPBConfig.fResetEn) ResetCPB();
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 5, cMasterConfig = (pNBytes << 2) | fCPBConfig.fI2CFrequency;
    if(fCPBConfig.fVerbose) LOG(INFO) << BOLDMAGENTA << "I2C write to Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 8 | pSlaveAddress << 0);
    cCommandVector.push_back(cMasterConfig << 24 | pSlaveData << 0);
    WriteCommandCPB(cCommandVector);
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(10);
    fI2Cstatus                         = cReplyVector[7] & 0xFF;
    size_t cIter = 0, cMaxIter = fCPBConfig.fMaxAttempts;
    while(fI2Cstatus != 4 && cIter < cMaxIter && fCPBConfig.fReTry)
    {
        if(fI2Cstatus != 4)
            LOG(DEBUG) << BOLDMAGENTA << "[D19cFWInterface::I2CWrite] Iter#" << +cIter << " I2CM" << +pMasterId << " status indicates a failure 0x" << std::hex << +fI2Cstatus << std::dec
                       << " transaction was to write " << +pNBytes << " to slave address " << +pSlaveAddress << " with data 0x" << std::hex << pSlaveData << std::dec << RESET;
        if(cIter == cMaxIter - 1) LOG(INFO) << BOLDRED << "[D19cFWInterface::I2CWrite] : I2CM" << +pMasterId << " Transaction Failed" << RESET;
        ResetCPB();
        if(cIter == cMaxIter - 1) LOG(INFO) << BOLDRED << "[D19cFWInterface::I2CWrite] : Received corrupted reply from command processor block ... retrying" << RESET;
        cReplyVector.clear();
        WriteCommandCPB(cCommandVector);
        cReplyVector = ReadReplyCPB(10);
        fI2Cstatus   = cReplyVector[7] & 0xFF;
        cIter++;
    }
    fI2CWriteCount += (1 + cIter);
    if(cIter == cMaxIter)
    {
        LOG(INFO) << BOLDRED << "[D19cFWInterface::I2CWrite] Iter#" << +cIter << " I2CM" << +pMasterId << " status indicates a failure 0x" << std::hex << +fI2Cstatus << std::dec
                  << " transaction was to write " << +pNBytes << " to slave address " << +pSlaveAddress << " with data 0x" << std::hex << pSlaveData << std::dec << RESET;
    }
    if(fI2Cstatus != 4) LOG(INFO) << BOLDRED << "[D19cFWInterface::I2CWrite] I2CM" << +pMasterId << " status is 0x" << std::hex << +fI2Cstatus << std::dec << RESET;
    return (fI2Cstatus == 4);
}

uint8_t D19cFWInterface::I2CRead(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint8_t pNBytes)
{
    this->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    if(fCPBConfig.fResetEn) ResetCPB();
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 4, cMasterConfig = (pNBytes << 2) | fCPBConfig.fI2CFrequency;
    if(fCPBConfig.fVerbose) LOG(INFO) << BOLDMAGENTA << "I2C Read to Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 8 | pSlaveAddress << 0);
    cCommandVector.push_back(cMasterConfig << 24);
    WriteCommandCPB(cCommandVector);
    std::vector<uint32_t> cReplyVector     = ReadReplyCPB(10);
    uint8_t               cReadBack        = cReplyVector[7] & 0xFF;
    uint16_t              cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
    size_t                cIter = 0, cMaxIter = fCPBConfig.fMaxAttempts;
    uint16_t              cI2CReadByteRegAddr = 0;
    // pick correct register address to check
    if(pMasterId == 2) cI2CReadByteRegAddr = 0x018d;
    if(pMasterId == 1) cI2CReadByteRegAddr = 0x178;
    if(pMasterId == 0) cI2CReadByteRegAddr = 0x0163;
    // check reply
    bool cCheckReadByte = true;
    bool cFail          = cCheckReadByte ? (cReadBackRegAddr != cI2CReadByteRegAddr) : false;
    cFail               = cFail && (cI2CReadByteRegAddr && cIter < cMaxIter && fCPBConfig.fReTry);
    while(cFail)
    {
        if(cIter == cMaxIter - 1) LOG(INFO) << BOLDRED << "[D19cFWInterface::I2CRead] : Received corrupted reply from command processor block ... retrying" << RESET;
        ResetCPB();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        cReplyVector.clear();
        WriteCommandCPB(cCommandVector);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        cReplyVector = ReadReplyCPB(10);
        // std::this_thread::sleep_for(std::chrono::microseconds(10));
        cReadBack        = cReplyVector[7] & 0xFF;
        cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
        cFail            = cCheckReadByte ? (cReadBackRegAddr != cI2CReadByteRegAddr) : false;
        cFail            = cFail && (cI2CReadByteRegAddr && cIter < cMaxIter && fCPBConfig.fReTry);
        if(cIter == cMaxIter - 1) LOG(INFO) << BOLDRED << "[D19cFWInterface::I2CRead] : Corrupted CPB reply frame" << RESET;
        cIter++;
    };
    fI2CReadCount += (1 + cIter);
    if(cIter == cMaxIter) throw std::runtime_error(std::string("[D19cFWInterface::I2CRead] : Corrupted CPB reply frame"));
    return cReadBack;
}

// ##########################################
// # Read/Write FE ASIC registers over I2C #
// #########################################

bool D19cFWInterface::WriteFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerify)
{
    auto cLinkId = pChip->getOpticalId();
    // uint8_t cMasterId = ((pChip->getHybridId() % 2) == 0) ? 2 : 0;
    uint8_t cMasterId = pChip->getMasterId();
    // if(cLpGBTI2CHack && cMasterId == 0) cMasterId = 1;

    LOG(DEBUG) << BOLDBLUE << " Writing 0x" << std::hex << +pRegisterValue << std::dec << " to [0x" << std::hex << +pRegisterAddress << std::dec << "] I2C master" << +cMasterId << RESET;
    uint8_t cChipId = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : pChip->getId();
    if(pChip->getFrontEndType() == FrontEndType::MPA) cChipId = cChipId % 8;
    uint8_t cChipAddress = fFEAddressMap[pChip->getFrontEndType()] + cChipId;
    // +1 for CBC address
    cChipAddress += (pChip->getFrontEndType() == FrontEndType::CBC3) ? 1 : 0;
    // LOG (INFO) << BOLDMAGENTA << "D19cFWInterface::WriteFERegister ChipAddress " << std::hex << +cChipAddress << std::dec << " on link" << +cLinkId << RESET;
    // CBC addresses are only 8 bits
    uint32_t cSlaveData = 0x00;
    uint8_t  cNbytes    = 3;
    if(pChip->getFrontEndType() != FrontEndType::CBC3)
    {
        uint16_t cInvertedRegister = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
        cSlaveData                 = (pRegisterValue << 16) | cInvertedRegister;
    }
    else
    {
        cNbytes    = 2;
        cSlaveData = (pRegisterValue << 8) | (pRegisterAddress & 0xFF);
    }
    fI2CWriteCount     = 0;
    fI2CReadMismatches = 0;
    bool cSuccess      = I2CWrite(cLinkId, cMasterId, cChipAddress, cSlaveData, cNbytes);
    pChip->updateWriteCount(fI2CWriteCount);
    if(fI2CWriteCount != 1)
    {
        std::stringstream cOutput;
        pChip->printChipType(cOutput);
        LOG(INFO) << BOLDYELLOW << "\t\t... Pre-verfication - took " << +fI2CWriteCount << " I2C writes to succeed in writing " << +pRegisterValue << " on register " << +pRegisterAddress << " for "
                  << cOutput.str() << "#" << +pChip->getId() << " on Hybrid#" << +pChip->getHybridId() << RESET;
    }
    if(pVerify && cSuccess)
    {
        uint8_t cReadBack = ReadFERegister(pChip, pRegisterAddress);
        uint8_t cIter = 0, cMaxIter = 100;
        while(cReadBack != pRegisterValue && cIter < cMaxIter)
        {
            if(cIter == cMaxIter - 1)
            {
                LOG(INFO) << BOLDRED << "I2C ReadBack Mismatch in hybrid " << +pChip->getHybridId() << " Chip " << +cChipId << " register 0x" << std::hex << +pRegisterAddress << std::dec
                          << " asked to write 0x" << std::hex << +pRegisterValue << std::dec << " and read back 0x" << std::hex << +cReadBack << std::dec << RESET;
            }
            // dont re-write  - just try and read again
            cReadBack = ReadFERegister(pChip, pRegisterAddress);

            // this was repeating both the write and the read
            // cSuccess = I2CWrite(cLinkId, cMasterId, cChipAddress, cSlaveData, cNbytes);
            // if(cSuccess) { cReadBack = ReadFERegister(pChip, pRegisterAddress); }
            fI2CReadMismatches++;
            cIter++;
        }
        if(cReadBack != pRegisterValue) { throw std::runtime_error(std::string("I2C readback mismatch")); }
    }
    else if(!cSuccess)
    {
        std::stringstream cErrorMsg;
        cErrorMsg << "I2C Write FAILED on Hybrid#" << +pChip->getHybridId() << " Link#" << +cLinkId << " Chip#" << +pChip->getId() << " - I2C status is " << +fI2Cstatus;
        LOG(INFO) << BOLDRED << cErrorMsg.str() << RESET;
        throw std::runtime_error(std::string(cErrorMsg.str()));
    }

    if(fI2CReadMismatches != 0)
    {
        std::stringstream cOutput;
        pChip->printChipType(cOutput);
        LOG(INFO) << BOLDYELLOW << "\t\t\t ...Post-verfication - took " << +fI2CReadMismatches << "attempts to read-back written value from " << +pRegisterValue << " on register " << +pRegisterAddress
                  << " for " << cOutput.str() << "#" << +pChip->getId() << " on Hybrid#" << +pChip->getHybridId() << RESET;
    }
    pChip->updateRBMismatchCount(fI2CReadMismatches);
    pChip->updateRegWriteCount();
    return cSuccess;
}

uint8_t D19cFWInterface::ReadFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress)
{
    auto    cLinkId   = pChip->getOpticalId();
    uint8_t cMasterId = pChip->getMasterId();
    // uint8_t cMasterId = ((pChip->getHybridId() % 2) == 0) ? 2 : 0;
    // cMasterId = pChip->getMasterId();
    // if(cLpGBTI2CHack && cMasterId == 0) cMasterId = 1;

    // LOG (INFO) << BOLDGREEN << "Reading FE register on link " << +cLinkId << RESET;
    uint8_t cChipId = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : pChip->getId();
    if(pChip->getFrontEndType() == FrontEndType::MPA) cChipId = cChipId % 8;
    uint8_t cChipAddress = fFEAddressMap[pChip->getFrontEndType()] + cChipId;
    // +1 for CBC address
    cChipAddress += (pChip->getFrontEndType() == FrontEndType::CBC3) ? 1 : 0;
    uint8_t  cNbytes    = 2;
    uint32_t cSlaveData = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
    if(pChip->getFrontEndType() != FrontEndType::CBC3)
        cSlaveData = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
    else
    {
        cNbytes    = 1;
        cSlaveData = (pRegisterAddress & 0xFF);
    }

    fI2CWriteCount = 0;
    I2CWrite(cLinkId, cMasterId, cChipAddress, cSlaveData, cNbytes);
    pChip->updateWriteCount(fI2CWriteCount);
    fI2CReadCount      = 0;
    uint32_t cReadBack = I2CRead(cLinkId, cMasterId, cChipAddress, 1);
    pChip->updateReadCount(fI2CReadCount);
    pChip->updateRegReadCount();
    return cReadBack;
}

void D19cFWInterface::ResetFCMDBram()
{
    LOG(DEBUG) << BOLDBLUE << "Resetting FCMD BRAM from sw.... started" << RESET;
    uint16_t                                      cBRAMdepth = 0x3FFF;
    std::vector<std::pair<std::string, uint32_t>> cRegs;
    for(uint16_t cBx = 0; cBx < cBRAMdepth; cBx++)
    {
        cRegs.push_back({"fc7_daq_cnfg.fast_command_block.generic_fcmd_data", 0x00});
        cRegs.push_back({"fc7_daq_cnfg.fast_command_block.generic_fcmd_addr", cBx});
        cRegs.push_back({"fc7_daq_ctrl.fast_command_block.control.write_generic", 0x1});
        cRegs.push_back({"fc7_daq_ctrl.fast_command_block.control.write_generic", 0x0});
        if(cBx % (cBRAMdepth / 10) == 0) LOG(DEBUG) << BOLDBLUE << "\t... Bx..." << +cBx << RESET;
    }
    this->WriteStackReg(cRegs);
    LOG(DEBUG) << BOLDBLUE << "Resetting FCMD BRAM from sw..... done" << RESET;
}
void D19cFWInterface::ConfigureFCMDBram(std::vector<uint8_t> pFastCommands)
{
    LOG(DEBUG) << BOLDBLUE << "Configuring FCMD BRAM from sw.." << RESET;
    uint16_t                                      cBRAMdepth = 0x3FFF;
    uint32_t                                      cWait      = fWait_us * 10;
    std::vector<std::pair<std::string, uint32_t>> cRegs;
    for(size_t cBx = 0; cBx < pFastCommands.size(); cBx++)
    {
        if(cBx >= cBRAMdepth)
        {
            LOG(INFO) << BOLDMAGENTA << "Maximum BRAM depth is " << +cBRAMdepth << RESET;
            LOG(INFO) << BOLDMAGENTA << "All fast commands following this will be ignored ... " << RESET;
            continue;
        }
        // fast command BRAM data and address
        // bram only takes the fcmd code (so not the header and not the trailer)
        uint8_t cCode = (pFastCommands[cBx] & (0xF << 1)) >> 1;
        cRegs.push_back({"fc7_daq_cnfg.fast_command_block.generic_fcmd_data", cCode});
        cRegs.push_back({"fc7_daq_cnfg.fast_command_block.generic_fcmd_addr", 1 + cBx});
        cRegs.push_back({"fc7_daq_ctrl.fast_command_block.control.write_generic", 0x1});
        cRegs.push_back({"fc7_daq_ctrl.fast_command_block.control.write_generic", 0x0});

        LOG(DEBUG) << BOLDBLUE << "\t..Fast command from sw is " << std::bitset<8>(pFastCommands[cBx]) << " writing " << std::bitset<4>(cCode) << " to generic fast command player in address  "
                   << (1 + cBx) << RESET;
    } // configure fast command bram
    this->WriteStackReg(cRegs);
    // make sure the last address written to the configuration register is 0
    this->WriteReg("fc7_daq_cnfg.fast_command_block.generic_fcmd_data", 0x00);
    this->WriteReg("fc7_daq_cnfg.fast_command_block.generic_fcmd_addr", 0x00);
    std::this_thread::sleep_for(std::chrono::microseconds(cWait));
    this->WriteReg("fc7_daq_ctrl.fast_command_block.control.write_generic", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(cWait));
    this->WriteReg("fc7_daq_ctrl.fast_command_block.control.write_generic", 0x0);
    std::this_thread::sleep_for(std::chrono::microseconds(cWait));
    // configure number of fast commands to  play
    this->WriteReg("fc7_daq_cnfg.fast_command_block.generic_fcmd.number_of_cmds_to_play", pFastCommands.size());
    this->WriteReg("fc7_daq_cnfg.fast_command_block.generic_fcmd.number_of_repetitions", 0x0);
    LOG(DEBUG) << BOLDBLUE << "Configuring FCMD BRAM from sw..... done" << RESET;
}

} // namespace Ph2_HwInterface
