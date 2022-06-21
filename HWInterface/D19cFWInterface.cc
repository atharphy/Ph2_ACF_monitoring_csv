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
#include "D19cBackendAlignmentFWInterface.h"
#include "D19cDebugFWInterface.h"
#include "D19cFastCommandInterface.h"
#include "D19cI2CInterface.h"
#include "D19cL1ReadoutInterface.h"
#include "D19cLinkInterface.h"
#include "D19cOpticalInterface.h"
#include "D19cPSCounterFWInterface.h"
#include "D19cTriggerInterface.h"
#include "D19clpGBTSlowControlWorkerInterface.h"
#include <algorithm>
#include <chrono>
#include <time.h>
#include <uhal/uhal.hpp>
// #pragma GCC diagnostic ignored "-Wpedantic"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cFWInterface::D19cFWInterface(const char* puHalConfigFileName, uint32_t pBoardId)
    : BeBoardFWInterface(puHalConfigFileName, pBoardId), fBroadcastCbcId(0), fNReadoutChip(0), fNHybrids(0), fNCic(0), fFMCId(1)
{
    fResetAttempts = 0;
    // can only link one type of trigger + FC interface to this type of FW
    // so do it in the contructor
    // configure L1 readout interface
    if(fTriggerInterface == nullptr)
    {
        fTriggerInterface = new D19cTriggerInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cTriggerInterface ..." << RESET;
    }
    if(fFastCommandInterface == nullptr)
    {
        fFastCommandInterface = new D19cFastCommandInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cFastCommandInterface ..." << RESET;
    }
    if(fBackendAlignmentInterface == nullptr)
    {
        fBackendAlignmentInterface = new D19cBackendAlignmentFWInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cBackendAlignmentFWInterface ..." << RESET;
    }
    if(fDebugInterface == nullptr)
    {
        fDebugInterface = new D19cDebugFWInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cDebugFWInterface ..." << RESET;
    }
    if(flpGBTSlowControlWorkerInterface == nullptr)
    {
        flpGBTSlowControlWorkerInterface = new D19clpGBTSlowControlWorkerInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19clpGBTSlowControlWorkerInterface ..." << RESET;
    }
    fFEConfigurationInterface = nullptr;
    fL1ReadoutInterface       = nullptr;
}

D19cFWInterface::D19cFWInterface(const char* puHalConfigFileName, uint32_t pBoardId, FileHandler* pFileHandler)
    : BeBoardFWInterface(puHalConfigFileName, pBoardId), fFileHandler(pFileHandler), fBroadcastCbcId(0), fNReadoutChip(0), fNHybrids(0), fNCic(0), fFMCId(1)
{
    if(fFileHandler == nullptr)
        fSaveToFile = false;
    else
        fSaveToFile = true;
    fResetAttempts = 0;
    // can only link one type of trigger + FC interface to this type of FW
    // so do it in the contructor
    // configure L1 readout interface
    if(fTriggerInterface == nullptr)
    {
        fTriggerInterface = new D19cTriggerInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cTriggerInterface ..." << RESET;
    }
    if(fFastCommandInterface == nullptr)
    {
        fFastCommandInterface = new D19cFastCommandInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cFastCommandInterface ..." << RESET;
    }
    if(fBackendAlignmentInterface == nullptr)
    {
        fBackendAlignmentInterface = new D19cBackendAlignmentFWInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cBackendAlignmentFWInterface ..." << RESET;
    }
    if(fDebugInterface == nullptr)
    {
        fDebugInterface = new D19cDebugFWInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cDebugFWInterface ..." << RESET;
    }
    if(flpGBTSlowControlWorkerInterface == nullptr)
    {
        flpGBTSlowControlWorkerInterface = new D19clpGBTSlowControlWorkerInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19clpGBTSlowControlWorkerInterface ..." << RESET;
    }
    fFEConfigurationInterface = nullptr;
    fL1ReadoutInterface       = nullptr;
}

D19cFWInterface::D19cFWInterface(const char* pId, const char* pUri, const char* pAddressTable)
    : BeBoardFWInterface(pId, pUri, pAddressTable), fFileHandler(nullptr), fBroadcastCbcId(0), fNReadoutChip(0), fNHybrids(0), fNCic(0), fFMCId(1)
{
    LOG(INFO) << BOLDYELLOW << "D19cFWInterface Constructor" << RESET;
    std::cout << pId << "\t" << pUri << "\t" << pAddressTable << "\n";
    fResetAttempts = 0;
    // can only link one type of trigger + FC interface to this type of FW
    // so do it in the contructor
    // configure L1 readout interface
    if(fTriggerInterface == nullptr)
    {
        fTriggerInterface = new D19cTriggerInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cTriggerInterface ..." << RESET;
    }
    if(fFastCommandInterface == nullptr)
    {
        fFastCommandInterface = new D19cFastCommandInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cFastCommandInterface ..." << RESET;
    }
    if(fBackendAlignmentInterface == nullptr)
    {
        fBackendAlignmentInterface = new D19cBackendAlignmentFWInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cBackendAlignmentFWInterface ..." << RESET;
    }
    if(fDebugInterface == nullptr)
    {
        fDebugInterface = new D19cDebugFWInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cDebugFWInterface ..." << RESET;
    }
    if(flpGBTSlowControlWorkerInterface == nullptr)
    {
        flpGBTSlowControlWorkerInterface = new D19clpGBTSlowControlWorkerInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19clpGBTSlowControlWorkerInterface ..." << RESET;
    }
    fFEConfigurationInterface = nullptr;
    fL1ReadoutInterface       = nullptr;
}

D19cFWInterface::D19cFWInterface(const char* pId, const char* pUri, const char* pAddressTable, FileHandler* pFileHandler)
    : BeBoardFWInterface(pId, pUri, pAddressTable), fFileHandler(pFileHandler), fBroadcastCbcId(0), fNReadoutChip(0), fNHybrids(0), fNCic(0), fFMCId(1)
{
    if(fFileHandler == nullptr)
        fSaveToFile = false;
    else
        fSaveToFile = true;
    fResetAttempts = 0;
    // can only link one type of trigger + FC interface to this type of FW
    // so do it in the contructor
    // configure L1 readout interface
    if(fTriggerInterface == nullptr)
    {
        fTriggerInterface = new D19cTriggerInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cTriggerInterface ..." << RESET;
    }
    if(fFastCommandInterface == nullptr)
    {
        fFastCommandInterface = new D19cFastCommandInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cFastCommandInterface ..." << RESET;
    }
    if(fBackendAlignmentInterface == nullptr)
    {
        fBackendAlignmentInterface = new D19cBackendAlignmentFWInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cBackendAlignmentFWInterface ..." << RESET;
    }
    if(fDebugInterface == nullptr)
    {
        fDebugInterface = new D19cDebugFWInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19cDebugFWInterface ..." << RESET;
    }
    if(flpGBTSlowControlWorkerInterface == nullptr)
    {
        flpGBTSlowControlWorkerInterface = new D19clpGBTSlowControlWorkerInterface(this->getId(), this->getUri(), this->getAddressTable());
        LOG(INFO) << BOLDYELLOW << "Created D19clpGBTSlowControlWorkerInterface ..." << RESET;
    }
    fFEConfigurationInterface = nullptr;
    fL1ReadoutInterface       = nullptr;
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

    int firmware_timestamp = ReadReg("fc7_daq_stat.general.firmware_timestamp");
    LOG(INFO) << "Compiled on: " << BOLDGREEN << ((firmware_timestamp >> 27) & 0x1F) << "." << ((firmware_timestamp >> 23) & 0xF) << "." << ((firmware_timestamp >> 17) & 0x3F) << " "
              << ((firmware_timestamp >> 12) & 0x1F) << ":" << ((firmware_timestamp >> 6) & 0x3F) << ":" << ((firmware_timestamp >> 0) & 0x3F) << " (dd.mm.yy hh:mm:ss)" << RESET;

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
void D19cFWInterface::InitializePSCounterFWInterface(const BeBoard* pBoard)
{
    fL1ReadoutInterface = nullptr;
    delete fL1ReadoutInterface;
    fL1ReadoutInterface = new D19cPSCounterFWInterface(this->getId(), this->getUri(), this->getAddressTable());
    static_cast<D19cPSCounterFWInterface*>(fL1ReadoutInterface)->LinkFEConfigurationInterface(fFEConfigurationInterface);
    LOG(INFO) << BOLDYELLOW << "Initialized D19cPSCounterFWInterface ..." << fL1ReadoutInterface << RESET;
    fL1ReadoutInterface->LinkTriggerInterface(fTriggerInterface);
    fL1ReadoutInterface->LinkFastCommandInterface(fFastCommandInterface);
}
void D19cFWInterface::IniitalizeL1ReadoutInterface(const BeBoard* pBoard)
{
    fL1ReadoutInterface = nullptr;
    delete fL1ReadoutInterface;
    fL1ReadoutInterface = new D19cL1ReadoutInterface(this->getId(), this->getUri(), this->getAddressTable());
    LOG(INFO) << BOLDYELLOW << "Initialized D19cL1ReadoutInterface ..." << fL1ReadoutInterface << RESET;
    fL1ReadoutInterface->LinkTriggerInterface(fTriggerInterface);
    fL1ReadoutInterface->LinkFastCommandInterface(fFastCommandInterface);
}
void D19cFWInterface::ConfigureInterfaces(const BeBoard* pBoard)
{
    if(fLinkInterface == nullptr && pBoard->isOptical())
    {
        LOG(INFO) << BOLDBLUE << "Optical readout . initializing link control interface" << RESET;
        fLinkInterface = new D19cLinkInterface(this->getId(), this->getUri(), this->getAddressTable());
    }
    if(fFEConfigurationInterface == nullptr)
    {
        Configuration cConfiguration;
        if(!pBoard->isOptical())
        {
            LOG(INFO) << BOLDYELLOW << "Electrical readout.. initialize I2C interface" << RESET;
            fFEConfigurationInterface = new D19cI2CInterface(this->getId(), this->getUri(), this->getAddressTable());
            (static_cast<D19cI2CInterface*>(fFEConfigurationInterface))->ConfigureI2CMap(pBoard);
            cConfiguration.fRetry       = 0;
            cConfiguration.fVerify      = 0;
            cConfiguration.fMaxAttempts = 100;
        }
        else
        {
            LOG(INFO) << BOLDBLUE << "Optical readout . initializing Optical interface for FE configuration" << RESET;
            fFEConfigurationInterface   = new D19cOpticalInterface(this->getId(), this->getUri(), this->getAddressTable());
            cConfiguration.fRetryIC     = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.ic_retry");
            cConfiguration.fMaxRetryIC  = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.max_ic_retry");
            cConfiguration.fRetryI2C    = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.i2c_retry");
            cConfiguration.fMaxRetryI2C = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.max_i2c_retry");
            cConfiguration.fRetryFE     = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.fe_retry");
            cConfiguration.fMaxRetryFE  = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.max_fe_retry");
            static_cast<D19cOpticalInterface*>(fFEConfigurationInterface)->LinkLpGBTSlowControlWorkerInterface(flpGBTSlowControlWorkerInterface);
        }
    }

    // configure L1 readout interface
    // this depends on the event type
    if(fL1ReadoutInterface == nullptr)
    {
        if(pBoard->getEventType() == EventType::PSAS)
            InitializePSCounterFWInterface(pBoard);
        else
            IniitalizeL1ReadoutInterface(pBoard);
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
    if(pBoard->isOptical())
    {
        LOG(INFO) << BOLDBLUE << "D19cFWInterface::ConfigureBoard for optical readout" << RESET;
        LOG(INFO) << BOLDYELLOW << "Configuring BackEndAligner assuming maximum 3 bits for bitslop " << RESET;
        fBackendAlignmentInterface->setNbits(3);
    }
    else
    {
        LOG(INFO) << BOLDYELLOW << "Configuring BackEndAligner assuming maximum 4 bits for bitslop " << RESET;
        fBackendAlignmentInterface->setNbits(4);
    }
    fOptical = pBoard->isOptical() && !cWithlpGBT;
    // if optical readout .. then configure links
    if(pBoard->isOptical() && cWithlpGBT)
    {
        bool    cSkip         = (pBoard->getLinkReset() == 0);
        uint8_t cLpGbtVersion = static_cast<lpGBT*>(pBoard->at(0)->flpGBT)->getVersion();
        this->WriteReg("fc7_daq_cnfg.optical_block.lpgbt.version", cLpGbtVersion);
        LOG(INFO) << BOLDYELLOW << "Setting firmware lpGBT version to lpGBT-v" << +this->ReadReg("fc7_daq_cnfg.optical_block.lpgbt.version") << RESET;
        if(!cSkip)
        {
            LOG(INFO) << BOLDMAGENTA << "Resetting lpGBT-FPGA core on BeBoard#" << +pBoard->getId() << RESET;
            flpGBTSlowControlWorkerInterface->Reset();
            fLinkInterface->GeneralLinkReset(pBoard);
        }
        else
        {
            LOG(INFO) << BOLDMAGENTA << "Skipping lpGBT link reset.." << RESET;
            for(auto cOpticalReadout: *pBoard)
            {
                uint8_t cLinkId = cOpticalReadout->getId();
                if(!fLinkInterface->GetLinkStatus(cLinkId))
                {
                    LOG(ERROR) << BOLDRED << "Link#" << +cLinkId << " not locked" RESET;
                    throw Exception("Link not locked...");
                }
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
            for(auto cHybrid: *cOpticalGroup)
            {
                auto  cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
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

    // Enable hybrids + Chips for readout
    LOG(INFO) << BOLDGREEN << "According to the Firmware status registers, it was compiled for: " << fFWNHybrids << " hybrid(s), " << fFWNChips << " " << cChipName << " chip(s) per hybrid" << RESET;
    this->EnableFrontEnds(pBoard);

    // adding an ReSync to align CBC L1A counters
    this->ChipReSync();

    // load trigger configuration
    // this->WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);
    fTriggerInterface->ResetTriggerFSM();
    fL1ReadoutInterface->ResetReadout();
    // reset trigger
    this->WriteReg("fc7_daq_ctrl.fast_command_block.control.reset", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
}
void D19cFWInterface::EnableFrontEnds(const Ph2_HwDescription::BeBoard* pBoard)
{
    fNCic                                                       = 0;
    fNReadoutChip                                               = 0;
    fNHybrids                                                   = 0;
    uint16_t                                      hybrid_enable = 0;
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.clear();
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto&   cCic         = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            uint8_t cChipsEnable = 0x00;
            fNCic += (cCic != nullptr) ? 1 : 0;
            hybrid_enable |= 1 << cHybrid->getId();
            fNHybrids++;
            LOG(INFO) << BOLDBLUE << "Enabling FE hybrid : " << +cHybrid->getId() << " - link Id " << +cOpticalGroup->getId() << RESET;
            for(auto cChip: *cHybrid)
            {
                cChipsEnable |= (1 << cChip->getId());
                fNReadoutChip++;
            }
            char name[50];
            std::sprintf(name, "fc7_daq_cnfg.global.chips_enable_hyb_%02d", cHybrid->getId());
            std::string name_str(name);
            cVecReg.push_back({name_str, cChipsEnable});
            LOG(INFO) << BOLDBLUE << "Setting chips enable register on hybrid" << +cHybrid->getId() << " to " << std::bitset<32>(cChipsEnable) << RESET;
        }
    }
    LOG(INFO) << BOLDBLUE << +fNCic << " CIC(s) enabled on this BeBoard" << RESET;
    LOG(INFO) << BOLDBLUE << +fNHybrids << " Hybrids(s) enabled on this BeBoard" << RESET;
    LOG(INFO) << BOLDBLUE << +fNReadoutChip << " ReadoutChip(s) enabled on this BeBoard" << RESET;
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

void D19cFWInterface::Start() { fTriggerInterface->Start(); }

void D19cFWInterface::Stop() { fTriggerInterface->Stop(); }
void D19cFWInterface::Pause() { fTriggerInterface->Pause(); }

void D19cFWInterface::Resume() { fTriggerInterface->Resume(); }

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

uint32_t D19cFWInterface::ReadData(BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait)
{
    pData.clear();
    uint32_t cNEvents = 0;
    LOG(DEBUG) << BOLDYELLOW << "D19cFWInterface::ReadData L1ReadoutInterface " << fL1ReadoutInterface << RESET;
    if(fL1ReadoutInterface == nullptr)
    {
        LOG(INFO) << BOLDRED << "L1ReadoutInterface is a nullptr.." << RESET;
        return cNEvents;
    }

    if(fL1ReadoutInterface->PollReadoutData(pBoard, pWait))
    {
        pData    = fL1ReadoutInterface->getData();
        cNEvents = fL1ReadoutInterface->getNReadoutEvents();
    }
    else
    {
        // if triggers are still running throw an exception
        if(fTriggerInterface->GetTriggerState() == 1)
        {
            LOG(INFO) << BOLDRED << "Failed to poll readout-data from BeBoard" << RESET;
            throw Exception("Failed to poll readout-data from BeBoard");
        }
        return cNEvents;
    }

    if(fSaveToFile && pData.size() > 0) fFileHandler->setData(pData);
    // update local event counter
    fEventCounter += cNEvents;
    // need to return the number of events read
    return cNEvents;
}
void D19cFWInterface::ReadNEvents(BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait)
{
    pData.clear();
    LOG(DEBUG) << BOLDYELLOW << "D19cFWInterface::ReadNEvent L1ReadoutInterface " << fL1ReadoutInterface << RESET;
    if(fL1ReadoutInterface == nullptr) LOG(INFO) << BOLDRED << "L1ReadoutInterface is a nullptr.." << RESET;

    fL1ReadoutInterface->setNEvents(pNEvents);
    if(fL1ReadoutInterface->ReadEvents(pBoard))
        pData = fL1ReadoutInterface->getData();
    else
    {
        LOG(INFO) << BOLDRED << "Failed to ReadNEvents" << RESET;
        throw Exception("Failed to ReadNEvents....");
    }
    if(fSaveToFile) fFileHandler->setData(pData);
}

/** compute the block size according to the number of CBC's on this board
 * this will have to change with a more generic FW */
uint32_t D19cFWInterface::computeEventSize(BeBoard* pBoard)
{
    uint32_t cFrontEndTypeCode = ReadReg("fc7_daq_stat.general.info.chip_type");
    fFirmwareFrontEndType      = getFrontEndType(cFrontEndTypeCode);
    uint32_t cNHybrid          = pBoard->getNHybrid();
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
        if(fFirmwareFrontEndType == FrontEndType::MPA) cNEventSize32 = D19C_EVENT_HEADER1_SIZE_32 + cNHybrid * D19C_EVENT_HEADER2_SIZE_32 + cNChips * D19C_EVENT_SIZE_32_MPA;
        if(fFirmwareFrontEndType == FrontEndType::SSA) cNEventSize32 = D19C_EVENT_HEADER1_SIZE_32 + cNHybrid * D19C_EVENT_HEADER2_SIZE_32 + cNChips * D19C_EVENT_SIZE_32_SSA;
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

void D19cFWInterface::ReadoutChipReset()
{
    // std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    LOG(INFO) << BOLDRED << "Sending HARD RESET to ReadoutChips" << RESET;
    WriteReg("fc7_daq_ctrl.physical_interface_block.control.chip_hard_reset", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
}
void D19cFWInterface::ChipReset()
{
    // for CBCs
    ReadoutChipReset();
    // for CICs
    LOG(INFO) << BOLDRED << "Sending HARD RESET to CIC" << RESET;
    WriteReg("fc7_daq_ctrl.physical_interface_block.control.cic_hard_reset", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
}
void D19cFWInterface::ChipReSync()
{
    std::vector<FastCommand> cFastCmds;
    FastCommand              cFastCmd;
    cFastCmd.resync_en     = 1;
    auto cFrontEndTypeCode = ReadReg("fc7_daq_stat.general.info.chip_type");
    bool cWithCIC          = (getFrontEndType(cFrontEndTypeCode) == FrontEndType::CIC || getFrontEndType(cFrontEndTypeCode) == FrontEndType::CIC2);
    cFastCmd.bc0_en        = (cWithCIC && fIs2S) ? 1 : 0;
    cFastCmds.push_back(cFastCmd);
    fFastCommandInterface->SendGlobalCustomFastCommands(cFastCmds);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
void D19cFWInterface::ChipTestPulse() { fFastCommandInterface->SendGlobalCalPulse(); }

void D19cFWInterface::ChipTrigger() { fFastCommandInterface->SendGlobalL1A(); }

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
    // reset the readout
    // this->ResetReadout();
    // reset decoder
    size_t cMaxAttempts = 20;
    size_t cWaitTime    = fWait_us * 100; // was 100
    this->WriteReg("fc7_daq_ctrl.physical_interface_block.control.decoder_reset", 0x1);
    this->WriteReg("fc7_daq_ctrl.physical_interface_block.control.decoder_reset", 0x0);
    // number of triggers to accept
    do
    {
        if(cWait) std::this_thread::sleep_for(std::chrono::microseconds(cWaitTime));
        // pause after reset
        // send a resync then wait
        fFastCommandInterface->SendGlobalReSync();
        // this->ChipReSync();
        if(cWait) std::this_thread::sleep_for(std::chrono::microseconds(cWaitTime));
        // check state of bx0 alignment block
        uint32_t cValue = this->ReadReg("fc7_daq_stat.physical_interface_block.cic_decoder.bx0_alignment_state");
        if(cValue == 8)
        {
            LOG(DEBUG) << BOLDBLUE << "Resetting decoder in back-end " << BOLDGREEN << " SUCCEEDED!"
                       << "\t... Stub package delay set to : " << +cPkgDelay << RESET;
            cSuccess = true;

            // // definitely works with
            // // figure out which one of these is needed
            // // resync after bx0 alignment worked
            // this->ChipReSync();
            // if(cWait) std::this_thread::sleep_for(std::chrono::microseconds(cWaitTime));

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

    return cSuccess;
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

    fTriggerInterface->ReconfigureTriggerFSM(cVecReg);
}
void D19cFWInterface::ConfigureAntennaFSM(uint16_t pNtriggers, uint16_t pTriggerRate, uint16_t pL1Delay)
{
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNtriggers});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", pTriggerRate});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 7});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.antenna_trigger_delay_value", pL1Delay});
    fTriggerInterface->ReconfigureTriggerFSM(cVecReg);
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
    fTriggerInterface->ReconfigureTriggerFSM(cVecReg);
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
    fTriggerInterface->ReconfigureTriggerFSM(cVecReg);
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

// ##########################################
// # Read/Write registers with CPB I2C functions #
// #########################################
uint8_t D19cFWInterface::SingleRegisterRead(Chip* pChip, ChipRegItem& pItem)
{
    if(pItem.fControlReg == 1) return 0;

    std::lock_guard<std::recursive_mutex> theGuard(fMutex); // Fabio:: I  do not like this lock
    uint8_t                               cValue = 0;
    if(fFEConfigurationInterface->SingleRead(pChip, pItem))
    {
        cValue = pItem.fValue;
        // update map
        auto cRegisterMap = pChip->getRegMap();
        auto cIterator    = find_if(cRegisterMap.begin(), cRegisterMap.end(), [&pItem](const ChipRegPair& obj) { return obj.second.fAddress == pItem.fAddress && obj.second.fPage == pItem.fPage; });
        if(cIterator != cRegisterMap.end())
        {
            auto cPreviousValue = cIterator->second.fValue;
            pChip->setReg(cIterator->first, pItem.fValue);
            pItem = pChip->getRegItem(cIterator->first);
            LOG(DEBUG) << BOLDGREEN << " D19cFWInterface::SingleRegisterRead successful read from 0x" << std::hex << +pItem.fValue << std::dec << " to " << cIterator->first
                       << "\t.. value in register is now 0x" << std::hex << +pChip->getReg(cIterator->first) << std::dec << " it was 0x" << std::hex << +cPreviousValue << std::dec << RESET;
        }
        else if(pItem.fStatusReg == 0x00)
            LOG(INFO) << BOLDRED << "D19cFWInterface::SingleRegisterRead Register 0x" << std::hex << +pItem.fAddress << " not in register map " << RESET;
    }
    else
        LOG(ERROR) << BOLDRED << "D19cFWInterface::SingleRegisterRead Register 0x" << std::hex << +pItem.fAddress << " FAILED " << RESET;
    return cValue;
}

bool D19cFWInterface::SingleRegisterWrite(Chip* pChip, ChipRegItem& pItem, bool pVerify)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex); // Fabio:: I  do not like this lock
    if(pVerify && pItem.fControlReg == 0) return SingleRegisterWriteRead(pChip, pItem);

    auto cRegisterMap = pChip->getRegMap();
    auto cIterator    = find_if(cRegisterMap.begin(), cRegisterMap.end(), [&pItem](const ChipRegPair& obj) { return obj.second.fAddress == pItem.fAddress && obj.second.fPage == pItem.fPage; });
    if(cIterator != cRegisterMap.end())
    {
        if(fFEConfigurationInterface->SingleWrite(pChip, pItem))
        {
            // update map
            auto cPreviousValue = cIterator->second.fValue;
            pChip->setReg(cIterator->first, pItem.fValue);
            LOG(DEBUG) << BOLDGREEN << " D19cFWInterface::SingleRegisterWrite successful write of 0x" << std::hex << +pItem.fValue << std::dec << " to " << cIterator->first
                       << "\t.. value in register is now 0x" << std::hex << +pChip->getReg(cIterator->first) << std::dec << " it was 0x" << std::hex << +cPreviousValue << std::dec << RESET;
            pItem = pChip->getRegItem(cIterator->first);
        }
        else
            LOG(ERROR) << BOLDRED << "D19cFWInterface::SingleRegisterWrite FAILEd to write to Register " << cIterator->first << RESET;
        return true;
    }
    else
        LOG(INFO) << BOLDRED << "D19cFWInterface::SingleRegisterWrite Could not find register address in register map " << RESET;
    return false;
}

bool D19cFWInterface::SingleRegisterWriteRead(Chip* pChip, ChipRegItem& pItem)
{
    if(pItem.fControlReg == 1)
    {
        LOG(INFO) << BOLDYELLOW << "D19cFWInterface::SingleRegisterWriteRead Control register..." << RESET;
        return false;
    }
    std::lock_guard<std::recursive_mutex> theGuard(fMutex); // Fabio:: I  do not like this lock
    auto                                  cRegisterMap = pChip->getRegMap();
    auto cIterator = find_if(cRegisterMap.begin(), cRegisterMap.end(), [&pItem](const ChipRegPair& obj) { return obj.second.fAddress == pItem.fAddress && obj.second.fPage == pItem.fPage; });
    if(cIterator != cRegisterMap.end())
    {
        auto cPreviousValue = cIterator->second.fValue;
        if(fFEConfigurationInterface->SingleWriteRead(pChip, pItem))
        {
            // update map
            pChip->setReg(cIterator->first, pItem.fValue);
            LOG(DEBUG) << BOLDGREEN << " D19cFWInterface::SingleRegisterWriteRead successful write of 0x" << std::hex << +pItem.fValue << std::dec << " to " << cIterator->first
                       << "\t.. value in register is now 0x" << std::hex << +pChip->getReg(cIterator->first) << std::dec << " it was 0x" << std::hex << +cPreviousValue << std::dec << RESET;
            pItem = pChip->getRegItem(cIterator->first);
            return true;
        }
        else
            LOG(ERROR) << BOLDRED << "D19cFWInterface::SingleRegisterWriteRead FAILED to write to Register " << cIterator->first << RESET;
    }
    else
    {
        LOG(INFO) << BOLDRED << "D19cFWInterface::SingleRegisterWriteRead Could not find register address " << std::hex << +pItem.fAddress << std::dec << " in register map " << RESET;
    }
    return false;
}

std::vector<uint8_t> D19cFWInterface::MultiRegisterRead(Chip* pChip, std::vector<ChipRegItem>& pItems)
{
    std::vector<uint8_t> cValues(0);
    if(pItems.size() == 0) return cValues;

    if(fFEConfigurationInterface->MultiRead(pChip, pItems))
    {
        // update map
        auto cRegisterMap = pChip->getRegMap();
        for(auto cItem: pItems)
        {
            auto cIterator = find_if(cRegisterMap.begin(), cRegisterMap.end(), [&cItem](const ChipRegPair& obj) { return obj.second.fAddress == cItem.fAddress && obj.second.fPage == cItem.fPage; });
            if(cIterator == cRegisterMap.end() && cItem.fStatusReg == 0x0)
                LOG(INFO) << BOLDRED << "Could not find " << cIterator->first << " addresss 0x" << std::hex << cItem.fAddress << std::dec << RESET;
            else
            {
                // LOG (INFO) << BOLDGREEN << "Found " << cIterator->first << " addresss 0x" << std::hex << cItem.fAddress << std::dec << RESET;
                if(cItem.fStatusReg == 0x00)
                {
                    pChip->setReg(cIterator->first, cItem.fValue);
                    cValues.push_back(pChip->getReg(cIterator->first));
                    LOG(DEBUG) << BOLDYELLOW << "D19cFWInterface::MultiRegisterRead Register " << cIterator->first << " 0x" << std::hex << +cItem.fAddress << std::dec << " set to 0x" << std::hex
                               << +cValues.at(cValues.size() - 1) << std::dec << RESET;
                }
                else
                {
                    cValues.push_back(cItem.fValue);
                }
            } // update map
        }
    }
    else
        LOG(ERROR) << BOLDRED << "D19cFWInterface::MultiRegisterRead Register FAILED " << RESET;
    return cValues;
}

bool D19cFWInterface::MultiRegisterWrite(Chip* pChip, std::vector<ChipRegItem>& pItems, bool pVerify)
{
    if(pItems.size() == 0) return true;

    std::lock_guard<std::recursive_mutex> theGuard(fMutex); // Fabio:: I  do not like this lock
    if(pVerify) return MultiRegisterWriteRead(pChip, pItems);

    if(fFEConfigurationInterface->MultiWrite(pChip, pItems))
    {
        LOG(DEBUG) << BOLDGREEN << "D19cFWInterface::MultiRegisterWrite successful write to " << pItems.size() << " registers" << RESET;
        // update map
        auto cRegisterMap = pChip->getRegMap();
        for(auto& cItem: pItems)
        {
            auto cIterator = find_if(cRegisterMap.begin(), cRegisterMap.end(), [&cItem](const ChipRegPair& obj) { return obj.second.fAddress == cItem.fAddress && obj.second.fPage == cItem.fPage; });
            if(cIterator != cRegisterMap.end()) // if item is in the map
            {
                auto cPreviousValue = cIterator->second.fValue;
                pChip->setReg(cIterator->first, cItem.fValue);
                LOG(DEBUG) << BOLDGREEN << " D19cFWInterface::MultiRegisterWrite successful write of 0x" << std::hex << +cItem.fValue << std::dec << " to " << cIterator->first
                           << "\t.. value in register is now 0x" << std::hex << +pChip->getReg(cIterator->first) << std::dec << " it was 0x" << std::hex << +cPreviousValue << std::dec << RESET;
                cItem = pChip->getRegItem(cIterator->first);
            }
            else
                LOG(INFO) << BOLDRED << "D19cFWInterface::MultiRegisterWrite Register 0x" << std::hex << +cItem.fAddress << " not in register map " << RESET;
        }
        return true;
    }
    else
        LOG(ERROR) << BOLDRED << "D19cFWInterface::MultiRegisterWrite FAILED" << RESET;
    return false;
}

bool D19cFWInterface::MultiRegisterWriteRead(Chip* pChip, std::vector<ChipRegItem>& pItems)
{
    if(pItems.size() == 0) return true;

    std::lock_guard<std::recursive_mutex> theGuard(fMutex); // Fabio:: I  do not like this lock
    if(fFEConfigurationInterface->MultiWriteRead(pChip, pItems))
    {
        auto cRegisterMap = pChip->getRegMap();
        for(auto cItem: pItems)
        {
            auto cIterator = find_if(cRegisterMap.begin(), cRegisterMap.end(), [&cItem](const ChipRegPair& obj) { return obj.second.fAddress == cItem.fAddress && obj.second.fPage == cItem.fPage; });
            if(cIterator != cRegisterMap.end())
            {
                auto cPreviousValue = cIterator->second.fValue;
                pChip->setReg(cIterator->first, cItem.fValue);
                LOG(DEBUG) << BOLDGREEN << " D19cFWInterface::MultiRegisterWriteRead successful write of 0x" << std::hex << +cItem.fValue << std::dec << " to " << cIterator->first
                           << "\t.. value in register is now 0x" << std::hex << +pChip->getReg(cIterator->first) << std::dec << " it was 0x" << std::hex << +cPreviousValue << std::dec << RESET;
            }
            else
                LOG(INFO) << BOLDRED << "D19cFWInterface::SingleRegisterWriteRead Could not find register address in register map " << RESET;
        }
        return true;
    } // update map
    else
        LOG(ERROR) << BOLDRED << "D19cFWInterface::MultiRegisterWriteRead FAILED to write to " << pItems.size() << " registers." << RESET;
    return false;
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
