/*

        FileName :                     GbtInterface.cc
        Content :                      User Interface to the Cics
        Version :                      1.0
        Date of creation :             10/07/14

 */

#include "GbtInterface.h"
#include "BeBoardFWInterface.h"

#define DEV_FLAG 0
// #define COUNT_FLAG 0

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
GbtInterface::GbtInterface() {}

GbtInterface::~GbtInterface() {}

// GBTX ec
void GbtInterface::ecReset(BeBoardFWInterface* pInterface)
{
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_ctrl.optical_block.sca.start", 0x00});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca", 0x00});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.gbtx", 0x00});
    // cVecReg.push_back ({"fc7_daq_ctrl.optical_block.sca.reset",0x1});
    pInterface->WriteStackReg(cVecReg);
}
uint32_t GbtInterface::ecWrite(BeBoardFWInterface* pInterface, uint16_t pI2Cmaster, uint32_t pCommand, uint32_t pData)
{
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_ctrl.optical_block.sca.start", 0x00});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca", 0x00});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.gbtx", 0x00});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.address", 0x01});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.id", 0x01});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.channel", pI2Cmaster});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.cmd", pCommand});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.data", pData});
    pInterface->WriteStackReg(cVecReg);
    // std::this_thread::sleep_for (std::chrono::microseconds (10) );
    LOG(DEBUG) << BOLDBLUE << "GBTx EC write to I2C master " << +pI2Cmaster << " - data field : " << +pData << " [ command 0x" << std::hex << pCommand << std::dec << "]." << RESET;
    pInterface->WriteReg("fc7_daq_ctrl.optical_block.sca.start", 0x1);
    // check for error
    uint32_t cErrorCode = pInterface->ReadReg("fc7_daq_stat.optical_block.sca.error");
    return cErrorCode;
    // reset
    // ecReset(pInterface);
}
uint32_t GbtInterface::ecWrite(BeBoardFWInterface* pInterface, uint16_t pI2Cmaster, const std::vector<std::pair<uint32_t, uint32_t>>& pCommands)
{
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    for(auto pCommand: pCommands)
    {
        cVecReg.push_back({"fc7_daq_ctrl.optical_block.sca.start", 0x00});
        cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca", 0x00});
        cVecReg.push_back({"fc7_daq_cnfg.optical_block.gbtx", 0x00});
        cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.address", 0x01});
        cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.id", 0x01});
        cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.channel", pI2Cmaster});
        cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.cmd", pCommand.first});
        cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.data", pCommand.second});
        LOG(DEBUG) << BOLDBLUE << "GBTx EC write to I2C master " << +pI2Cmaster << " - data field : " << +pCommand.second << " [ command 0x" << std::hex << pCommand.first << std::dec << "]." << RESET;
    }
    pInterface->WriteStackReg(cVecReg);
    // std::this_thread::sleep_for (std::chrono::microseconds (10) );
    pInterface->WriteReg("fc7_daq_ctrl.optical_block.sca.start", 0x1);
    // std::this_thread::sleep_for (std::chrono::microseconds (100) );
    uint32_t cErrorCode = pInterface->ReadReg("fc7_daq_stat.optical_block.sca.error");
    // reset
    // ecReset(pInterface);
    return cErrorCode;
}

uint32_t GbtInterface::ecRead(BeBoardFWInterface* pInterface, uint16_t pI2Cmaster, uint32_t pCommand, uint32_t pData)
{
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_ctrl.optical_block.sca.start", 0x00});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca", 0x00});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.gbtx", 0x00});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.address", 0x01});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.id", 0x02});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.channel", pI2Cmaster});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.cmd", pCommand});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.sca.data", pData});
    pInterface->WriteStackReg(cVecReg);
    pInterface->WriteReg("fc7_daq_ctrl.optical_block.sca.start", 0x1);
    uint32_t cRead = pInterface->ReadReg("fc7_daq_stat.optical_block.sca.data");
    LOG(DEBUG) << BOLDBLUE << "GBTx EC read returns : " << std::bitset<32>(cRead) << RESET;
    // ecReset(pInterface);
    return cRead;
}
// GBTx ic
void GbtInterface::icReset(BeBoardFWInterface* pInterface)
{
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_ctrl.optical_block.ic", 0x00});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.ic", 0x00});
    cVecReg.push_back({"fc7_daq_cnfg.optical_block.gbtx", 0x00});
    pInterface->WriteStackReg(cVecReg);
}
void GbtInterface::icWrite(BeBoardFWInterface* pInterface, uint32_t pAddress, uint32_t pData)
{
    // config
    pInterface->WriteReg("fc7_daq_cnfg.optical_block.gbtx.address", fGBTxAddress);
    pInterface->WriteReg("fc7_daq_cnfg.optical_block.gbtx.data", pData);
    pInterface->WriteReg("fc7_daq_cnfg.optical_block.ic.register", pAddress);
    // perform operation
    pInterface->WriteReg("fc7_daq_ctrl.optical_block.ic.write", 0x01);
    pInterface->WriteReg("fc7_daq_ctrl.optical_block.ic.write", 0x00);
    //
    pInterface->WriteReg("fc7_daq_ctrl.optical_block.ic.start_write", 0x01);
    pInterface->WriteReg("fc7_daq_ctrl.optical_block.ic.start_write", 0x00);
    icReset(pInterface);
}
uint32_t GbtInterface::icRead(BeBoardFWInterface* pInterface, uint32_t pAddress, uint32_t pNwords)
{
    // config
    pInterface->WriteReg("fc7_daq_cnfg.optical_block.gbtx.address", fGBTxAddress);
    pInterface->WriteReg("fc7_daq_cnfg.optical_block.ic.register", pAddress);
    pInterface->WriteReg("fc7_daq_cnfg.optical_block.ic.nwords", pNwords);
    // perform operation
    pInterface->WriteReg("fc7_daq_ctrl.optical_block.ic.start_read", 0x01);
    pInterface->WriteReg("fc7_daq_ctrl.optical_block.ic.start_read", 0x00);
    //
    pInterface->WriteReg("fc7_daq_ctrl.optical_block.ic.read", 0x01);
    pInterface->WriteReg("fc7_daq_ctrl.optical_block.ic.read", 0x00);
    //
    uint32_t cRead = pInterface->ReadReg("fc7_daq_stat.optical_block.ic.data");
    // reset
    icReset(pInterface);
    return cRead;
}
// SCA - enable I2C master interfaces, GPIO, ADC
uint8_t GbtInterface::scaEnable(BeBoardFWInterface* pInterface, uint16_t cI2Cmaster)
{
    uint32_t cErrorCode = ecWrite(pInterface, cI2Cmaster, 0x02, 0x04000000);
    if(cErrorCode != 0)
    {
        LOG(INFO) << BOLDBLUE << "SCA Error code : " << +cErrorCode << RESET;
        return 0;
    }
    cErrorCode = ecWrite(pInterface, cI2Cmaster, 0x04, 0x00000000);
    if(cErrorCode != 0)
    {
        LOG(INFO) << BOLDBLUE << "SCA Error code : " << +cErrorCode << RESET;
        return 0;
    }
    cErrorCode = ecWrite(pInterface, cI2Cmaster, 0x06, 0x16000000);
    if(cErrorCode != 0)
    {
        LOG(INFO) << BOLDBLUE << "SCA Error code : " << +cErrorCode << RESET;
        return 0;
    }

    return (cErrorCode == 0);
}
void GbtInterface::scaConfigure(BeBoardFWInterface* pInterface)
{
    LOG(INFO) << BOLDBLUE << "Set all registers involved in GBT-SCA communication, as instructed on page 66 of gbtx manual" << RESET;
    // dll
    icWrite(pInterface, 231, 0x00dd);
    icWrite(pInterface, 232, 0x000d);
    icWrite(pInterface, 233, 0x0070);
    for(uint16_t cRegister = 237; cRegister < 246; cRegister += 4) icWrite(pInterface, cRegister, 0x0000);
    icWrite(pInterface, 248, 0x0007);
    icWrite(pInterface, 251, 0x0000);
    icWrite(pInterface, 254, 0x0070);
    icWrite(pInterface, 257, 0x0000);
    icWrite(pInterface, 273, 0x0020);
}
bool GbtInterface::scaSetGPIO(BeBoardFWInterface* pInterface, uint8_t cChannel, uint8_t cLevel)
{
    uint32_t cMask      = (1 << cChannel);
    cMask               = (~cMask & 0xFFFFFFFF);
    uint8_t cSCAchannel = 0x02;
    if(cChannel < 31)
    {
        uint32_t cValue     = ecRead(pInterface, cSCAchannel, 0x11);
        uint8_t  cErrorCode = ecWrite(pInterface, cSCAchannel, 0x10, (cLevel << cChannel) | (cValue & cMask));
        return (cErrorCode == 0);
    }
    return false;
}
uint8_t GbtInterface::scaStatus(BeBoardFWInterface* pInterface, uint8_t pMaster)
{
    uint32_t cValue  = ecRead(pInterface, pMaster, 0x11);
    uint8_t  cStatus = ((cValue & 0xFF000000) >> 3 * 8);
    LOG(DEBUG) << BOLDBLUE << "SCA status : " << std::bitset<8>(cStatus) << RESET;
    return cStatus;
}
// configure gpio [sca]
void GbtInterface::scaConfigureGPIO(BeBoardFWInterface* pInterface)
{
    uint32_t cMask      = (1 << 31) | (1 << 30) | (1 << 3) | (1 << 2);
    cMask               = (~cMask & 0xFFFFFFFF);
    uint8_t  cMaster    = 0x02;
    uint32_t cData      = (0 << 31) | (1 << 30) | (0 << 3) | (1 << 2);
    uint32_t cErrorCode = ecWrite(pInterface, cMaster, 0x10, 0x40000004);
    if(cErrorCode != 0) exit(0);
    //
    uint32_t cValue = ecRead(pInterface, cMaster, 0x21);
    cData           = ((1 << 31) | (1 << 30) | (1 << 3) | (1 << 2));
    ecWrite(pInterface, cMaster, 0x20, cData | (cValue & cMask));
    //
    cValue = ecRead(pInterface, cMaster, 0x31);
    cData  = ((0 << 31) | (0 << 30) | (0 << 3) | (0 << 2));
    ecWrite(pInterface, cMaster, 0x30, cData | (cValue & cMask));
    //
    cValue = ecRead(pInterface, cMaster, 0x11);
    cData  = (1 << 31) | (0 << 30) | (1 << 3) | (0 << 2);
    ecWrite(pInterface, cMaster, 0x10, cData | (cValue & cMask));
}
uint16_t GbtInterface::readAdcChn(BeBoardFWInterface* pInterface, std::string pValueToRead, bool pConvertRawReading)
{
    uint32_t cADCslave = fScaAdcChnMap[pValueToRead];
    LOG(DEBUG) << BOLDBLUE << "Using ADC on GBTx to read " << pValueToRead << " -- which is ADC slave " << cADCslave << RESET;
    uint32_t cMaster = 0x14;
    uint32_t cErrorMux, cErrorGo;
    if(pValueToRead == "EXT_TEMP") // turn on current source for external temperature sensor
    {
        cErrorMux = ecWrite(pInterface, cMaster, 0x60, (1 << 25));
        if(cErrorMux != 0) LOG(INFO) << BOLDYELLOW << "Error setting SCA AdcMuxSelect" << RESET;
    }
    // Write gain correction factor (if needed)
    // TODO: we have to move it out from here in order not to have it hardcoded
    // int cCorrection = 3528;
    // cErrorMux = ecWrite(pInterface, cMaster, 0x10, cCorrection);
    // Choose channel to perform next measurement
    cErrorMux = ecWrite(pInterface, cMaster, 0x50, cADCslave);
    if(cErrorMux != 0) LOG(INFO) << BOLDYELLOW << "Error setting SCA AdcMuxSelect" << RESET;
    // Measure in chosen channel
    cErrorGo = ecWrite(pInterface, cMaster, 0x02, 0x00000001);
    if(cErrorGo != 0) LOG(INFO) << BOLDYELLOW << "Error asking SCA AdcGo for starting conversion" << RESET;
    // Read back measurement result (converted or unconverted)
    uint32_t cAdcValue = ecRead(pInterface, cMaster, ((pConvertRawReading) ? 0x21 : 0x31), 0x0);
    LOG(DEBUG) << BLUE << "SCA ADC chn: " << cADCslave << " reads" << cAdcValue << " for pConvertRawReading=" << pConvertRawReading << RESET;

    if(pValueToRead == "EXT_TEMP") // turn off current source for external temperature sensor
    {
        cErrorMux = ecWrite(pInterface, cMaster, 0x60, 0x00);
        if(cErrorMux != 0) LOG(INFO) << BOLDYELLOW << "Error setting SCA AdcMuxSelect" << RESET;
    }
    return (uint16_t)(cAdcValue);
}
uint32_t GbtInterface::readAdcCalibration(BeBoardFWInterface* pInterface)
{
    uint32_t cCmdCalib = 0x11;
    uint16_t cAdcChnId = 0x14;
    uint32_t cAdcValue = ecRead(pInterface, cAdcChnId, cCmdCalib);
    return cAdcValue;
}
float GbtInterface::convertAdcReading(uint16_t pReading, std::string pValueToRead)
{
    /*!
     * \brief Transform ADC reading from 0-1 V range to real value based on resistances in voltage divider
     * \param pReading : SCA ADC raw/ temp. corrected value uint16_t
     * \param pR1 : resistor 1 int
     * \param pR2 : resistor 2 int
     */
    auto  cResistances = fScaAdcVoltageDeviderMap[pValueToRead];
    float cConverted   = float(pReading) / (std::pow(2, 12) - 1) * (cResistances.first + cResistances.second) / float(cResistances.second);
    if(pValueToRead == "EXT_TEMP")
    {
        // convert to temperature value for thermistor NCP15XM331J03RC
        cConverted = convAdcToTemp(cConverted, "NCP15XM331J03RC");
    }
    return cConverted;
}
// GBTx configuration
void GbtInterface::gbtxSelectEdgeTx(BeBoardFWInterface* pInterface, bool pRising)
{
    uint32_t cReadBack = icRead(pInterface, 244, 1);
    uint32_t pValue    = (pRising) ? 7 : 0;
    uint32_t cRegValue = (cReadBack & 0xC7) | (pValue << 3);
    icWrite(pInterface, 244, cRegValue);
}

void GbtInterface::gbtxSelectEdge(BeBoardFWInterface* pInterface, bool pRising)
{
    uint32_t cReadBack = icRead(pInterface, 244, 1);
    uint32_t cRegValue = (cReadBack & 0xC0);
    for(size_t cIndex = 0; cIndex < 6; cIndex++) { cRegValue = cRegValue | ((uint8_t)pRising << cIndex); }
    LOG(INFO) << BOLDBLUE << "GBTx default configuration " << std::bitset<8>(cReadBack) << " -- will be set to " << std::bitset<8>(cRegValue) << RESET;
    // icWrite(pInterface, 244 , cRegValue ) ;
}
void GbtInterface::gbtxSetPhase(BeBoardFWInterface* pInterface, uint8_t pPhase)
{
    uint16_t cReg = 62;
    // set phase mode to static
    uint8_t  cPhaseSelectMode = 0x00;
    uint32_t cReadBack        = icRead(pInterface, cReg, 1);
    uint32_t cWrite           = (cReadBack & 0xc0) | ((cPhaseSelectMode << 4) | (cPhaseSelectMode << 2) | (cPhaseSelectMode << 0));
    icWrite(pInterface, cReg, cWrite);
    // reset phase
    for(size_t cIndex = 0; cIndex < 7; cIndex++)
    {
        uint16_t cChannelReg = 84 + 24 * cIndex;
        LOG(DEBUG) << BOLDBLUE << "Setting register " << cChannelReg << " to 0xFF" << RESET;
        icWrite(pInterface, cChannelReg, 0xFF);
        icWrite(pInterface, cChannelReg + 1, 0xFF);
        icWrite(pInterface, cChannelReg + 2, 0xFF);
        cReadBack = icRead(pInterface, cChannelReg, 1);
        LOG(DEBUG) << BOLDBLUE << "\t...register set to " << cChannelReg << " to 0x" << std::hex << cReadBack << std::dec << RESET;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        LOG(DEBUG) << BOLDBLUE << "Setting register " << cChannelReg << " to 0x00" << RESET;
        icWrite(pInterface, cChannelReg, 0x00);
        icWrite(pInterface, cChannelReg + 1, 0x00);
        icWrite(pInterface, cChannelReg + 2, 0x00);
    }
    // set phase
    std::vector<uint16_t> cRegisters = {66, 90, 114, 138, 162, 186, 210};
    for(auto cChannelReg: cRegisters)
    {
        for(size_t cIndex = 0; cIndex < 12; cIndex++) { icWrite(pInterface, cChannelReg + cIndex, (pPhase << 4) | (pPhase << 0)); }
    }
}
void GbtInterface::gbtxSetDriveStrength(BeBoardFWInterface* pInterface, uint8_t pStrength)
{
    std::vector<uint16_t> cRegs{327, 328, 329, 330, 331};
    for(auto cReg: cRegs) { icWrite(pInterface, cReg, (pStrength << 4) || pStrength); }
}
void GbtInterface::gbtxConfigureChargePumps(BeBoardFWInterface* pInterface, uint8_t pStrength)
{
    LOG(INFO) << BOLDBLUE << "Setting DLLs charge-pump control registers to " << std::dec << +pStrength << RESET;
    for(uint16_t cRegister = 16; cRegister < 24; cRegister++)
    {
        uint32_t cReadBack = icRead(pInterface, cRegister, 1);
        icWrite(pInterface, cRegister, (cReadBack & 0xF0) | (pStrength << 0));
    }
    LOG(INFO) << BOLDBLUE << "Configuring PLL ..." << RESET;
    // Programing the phase-shifter channels’ frequency
    uint32_t cReadBack = icRead(pInterface, 16, 1);
    icWrite(pInterface, 16, (cReadBack & 0xF0) | (0x0F << 0));
    // charge current + resistor for PLL
    cReadBack = icRead(pInterface, 26, 1);
    // icWrite(pInterface, 26, (cReadBack & 0x8F) | 0xF ) ;
    icWrite(pInterface, 26, (0x7 << 4) | 0xF);
    // configure the power-up state machine for the pll watchdog
    // cReadBack = icRead( pInterface, 52 , 1); //watchdog
    // icWrite(pInterface, 52, (cReadBack & 0xC0) | (0x7 << 3) |  (0x7 << 0) ) ;
    // icWrite(pInterface, 52, (cReadBack & 0xC0) | (0x7 << 3) |  (0x0 << 0) ) ;
    cReadBack = icRead(pInterface, 52, 1); // watchdog
    LOG(INFO) << BOLDBLUE << "Watchdog timeout set to " << std::bitset<8>(cReadBack) << RESET;
}
// set clocks
void GbtInterface::gbtxSetClocks(BeBoardFWInterface* pInterface, uint8_t pFrequency, uint8_t cDriveStrength, uint8_t cCoarsePhase, uint8_t cFinePhase)
{
    LOG(INFO) << BOLDBLUE << "Set clock frequency on GBTx channels 0 to 7 to : " << 40 * std::pow(2, (int)pFrequency) << " MHz." << RESET;
    // Set frequencies to 320 MHz
    // was 0x30 here .. but is 0x34 in Christian's
    for(uint16_t cRegister = 16; cRegister < 24; cRegister++)
    {
        uint32_t cReadBack = icRead(pInterface, cRegister, 1);
        icWrite(pInterface, cRegister, (cReadBack & 0xCF) | (pFrequency << 4));
    }
    // set drive strength
    for(uint16_t cRegister = 269; cRegister < 274; cRegister++)
    {
        if(cRegister < 273)
            icWrite(pInterface, cRegister,
                    (cDriveStrength << 4) | (cDriveStrength << 0)); // set all channels to the same value ... each register holds two channels
        else
        {
            // for ec channel
            uint32_t cReadBack = icRead(pInterface, cRegister, 1);
            icWrite(pInterface, cRegister, (cReadBack & 0xF0) | (cDriveStrength << 0) | (1 << 5));
        }
    }
    // read back drive strength
    for(uint16_t cRegister = 269; cRegister < 273; cRegister++)
    {
        uint32_t cReadBack = icRead(pInterface, cRegister, 1);
        LOG(DEBUG) << BOLDBLUE << "Drive strength register " << std::hex << cRegister << std::dec << " set to 0x" << std::hex << +cReadBack << " in GBTx" << RESET;
    }
    // configure coarse+fine phase
    // coarse phase
    for(uint16_t cRegister = 8; cRegister < 16; cRegister++)
    {
        uint32_t cReadBack = icRead(pInterface, cRegister, 1);
        icWrite(pInterface, cRegister, (cCoarsePhase) | (cReadBack & 0xE0));
    }
    // fine phase
    for(uint16_t cRegister = 4; cRegister < 8; cRegister++) { icWrite(pInterface, cRegister, (cFinePhase << 4) | (cFinePhase << 0)); }
}
// read clocks
void GbtInterface::gbtxReadClocks(BeBoardFWInterface* pInterface)
{
    std::vector<uint16_t> cRegisters = {16, 17, 18, 19, 20, 21, 22, 23, 26, 52};
    for(auto cRegister: cRegisters)
    {
        uint32_t cReadBack = icRead(pInterface, cRegister, 1);
        LOG(DEBUG) << BOLDBLUE << "Clock Register 0x" << std::hex << cRegister << std::dec << " set to 0x" << std::hex << +cReadBack << " in GBTx" << RESET;
    }
}
// reset clock plls
void GbtInterface::gbtxResetPhaseShifterClocks(BeBoardFWInterface* pInterface)
{
    // first .. reset PLLs
    LOG(INFO) << BOLDBLUE << "Resetting PLLs on GBTx.." << RESET;
    icWrite(pInterface, 25, 0x00);
    icWrite(pInterface, 25, 0x01);
    // wait for 10 us
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // reset DLLs
    LOG(INFO) << BOLDBLUE << "Resetting DLLs on GBTx.." << RESET;
    icWrite(pInterface, 24, 0x00);
    icWrite(pInterface, 24, 0xFF);
    // wait for more than 50 us
    std::this_thread::sleep_for(std::chrono::microseconds(100));
}
// configure e-links
void GbtInterface::gbtxConfigureTxMode(BeBoardFWInterface* pInterface, std::vector<uint8_t> cGroups,
                                       uint8_t pDataRate) // 0 -- disable, 1 -- 80 Mb/s , 2 -- 160 Mbps , 3 -- 320 Mbps
{
    // Registers holding the clock rates for the e-link tx
    for(auto cGroup: cGroups) // uint8_t cGroup = 0 ; cGroup < 5 ; cGroup++)
    {
        uint16_t cRegister = 254 + cGroup * 3;
        LOG(DEBUG) << BOLDBLUE << "Setting all output e-links clock rates for group " << +cGroup << " to 0x" << std::hex << +pDataRate << std::dec << RESET;
        std::vector<uint16_t> cOffsets = {0, 78, 15};
        for(size_t cIndex2 = 0; cIndex2 < cOffsets.size(); cIndex2++)
        {
            cRegister += cOffsets[cIndex2];
            // 0 - disable, 1 - 80, 2 - 160, 3 = 320
            uint32_t cReadBack = icRead(pInterface, cRegister, 1);
            uint8_t  cValue    = (cReadBack & 0xFC) | (pDataRate << 0);
            icWrite(pInterface, cRegister, cValue);
        }
    }
}
void GbtInterface::gbtxConfigureTxClocks(BeBoardFWInterface* pInterface, std::vector<uint8_t> cGroups,
                                         uint8_t pDataRate) // 0 -- disable, 1 -- 80 Mb/s , 2 -- 160 Mbps , 3 -- 320 Mbps
{
    // Registers holding the clock rates for the e-link tx
    for(auto cGroup: cGroups)
    {
        uint16_t cRegister = 254 + cGroup * 3;
        LOG(DEBUG) << BOLDBLUE << "Setting all output e-links clock rates for group " << +cGroup << " to 0x" << std::hex << +pDataRate << std::dec << RESET;
        std::vector<uint16_t> cOffsets = {0, 78, 15};
        for(size_t cIndex = 0; cIndex < cOffsets.size(); cIndex++)
        {
            cRegister += cOffsets[cIndex];
            // 0 - 40, 1 - 80, 2 - 160, 3 = 320
            uint32_t cReadBack = icRead(pInterface, cRegister, 1);
            uint8_t  cValue    = (cReadBack & 0xF3) | (pDataRate << 2);
            icWrite(pInterface, cRegister, cValue);
        }
    }
}
void GbtInterface::gbtxConfigureRxClocks(BeBoardFWInterface* pInterface, std::vector<uint8_t> cGroups,
                                         uint8_t pDataRate) // 0 -- disable, 1 -- 80 Mb/s , 2 -- 160 Mbps , 3 -- 320 Mbps
{
    // Registers holding the clock rate for the e-link rx
    for(auto cGroup: cGroups)
    {
        uint16_t cRegister = 63 + cGroup * 24;
        LOG(DEBUG) << BOLDBLUE << "Setting all input e-link clock rates for group " << +cGroup << " [register " << +cRegister << " ] to 0x00" << RESET;
        icWrite(pInterface, cRegister, (pDataRate << 4) | (pDataRate << 2) | (pDataRate << 0));
    }
}
void GbtInterface::gbtxEnableTxChannel(BeBoardFWInterface* pInterface, uint8_t pGroup, std::vector<uint8_t> pChannels)
{
    uint16_t cRegister = 256 + pGroup * 3;
    LOG(DEBUG) << BOLDBLUE << "Enabling  output e-links for group " << +pGroup << RESET;
    std::vector<uint16_t> cOffsets = {0, 78, 15};
    for(size_t cIndex2 = 0; cIndex2 < cOffsets.size(); cIndex2++)
    {
        cRegister += cOffsets[cIndex2];
        uint8_t cValue = 0;
        for(auto cChannel: pChannels)
        {
            LOG(DEBUG) << BOLDBLUE << "\t.. Register " << cRegister << " used to enable Tx channel " << +cChannel << RESET;
            cValue += (1 << cChannel);
        }
        icWrite(pInterface, cRegister, cValue);
    }
}
void GbtInterface::gbtxEnableRxChannel(BeBoardFWInterface* pInterface, uint8_t pGroup, std::vector<uint8_t> pChannels)
{
    LOG(DEBUG) << BOLDBLUE << "Enabling input e-links for group " << +pGroup << RESET;
    for(size_t cIndex = 0; cIndex < 3; cIndex++)
    {
        uint16_t cRegister = 81 + pGroup * 24 + cIndex;
        LOG(DEBUG) << BOLDBLUE << "\t... Register " << cRegister << RESET;
        uint8_t cValue = 0;
        for(auto cChannel: pChannels)
        {
            cValue += (1 << cChannel);
            LOG(DEBUG) << BOLDBLUE << "\t.. Register " << cRegister << " used to enable Rx channel " << +cChannel << RESET;
        }
        icWrite(pInterface, cRegister, cValue);
    }
}
void GbtInterface::gbtxConfigureLinks(BeBoardFWInterface* pInterface)
{
    // set all clocks to 40 MHz [ Tx]
    std::vector<uint8_t> cTxGroups = {0, 1, 2, 3, 4};
    gbtxConfigureTxClocks(pInterface, cTxGroups, 3);
    // enable Fast command links [Tx] for left and right hand side
    // hybrids
    std::vector<uint8_t> cTxHybrid = {2, 3};
    gbtxConfigureTxMode(pInterface, cTxHybrid, 3);
    gbtxEnableTxChannel(pInterface, 2, {4});
    gbtxEnableTxChannel(pInterface, 3, {0});
    std::vector<uint8_t> cRxGroups = {0, 1, 2, 3, 4, 5, 6, 7};
    gbtxConfigureRxClocks(pInterface, cRxGroups, 3); // 0 -- disable, 1 -- 80 Mb/s , 2 -- 160 Mbps , 3 -- 320 Mbps
    gbtxEnableRxChannel(pInterface, 0, {0, 4});
    gbtxEnableRxChannel(pInterface, 1, {4});
    gbtxEnableRxChannel(pInterface, 2, {0, 4});
    gbtxEnableRxChannel(pInterface, 3, {0, 4});
    gbtxEnableRxChannel(pInterface, 4, {0, 4});
    gbtxEnableRxChannel(pInterface, 5, {1, 4});
    gbtxEnableRxChannel(pInterface, 6, {0, 4});
}
void GbtInterface::gbtxDisableAllLinks(BeBoardFWInterface* pInterface)
{
    LOG(INFO) << BOLDBLUE << "Disabling all input/output groups on GBTx.." << RESET;
    // Registers holding the clock rates for the e-link tx
    std::vector<uint8_t> cTxGroups = {0, 1, 2, 3, 4};
    gbtxConfigureTxMode(pInterface, cTxGroups, 0); // 0 -- disable, 1 -- 80 Mb/s , 2 -- 160 Mbps , 3 -- 320 Mbps
    std::vector<uint8_t> cRxGroups = {0, 1, 2, 3, 4, 5, 6, 7};
    gbtxConfigureRxClocks(pInterface, cRxGroups, 0); // 0 -- disable, 1 -- 80 Mb/s , 2 -- 160 Mbps , 3 -- 320 Mbps
}
void GbtInterface::gbtxResetFrameAlignerDLL(BeBoardFWInterface* pInterface, std::vector<uint8_t> pGroups)
{
    // for e-links
    for(auto cGroup: pGroups)
    {
        uint16_t cRegister = 65 + cGroup * 24;
        uint32_t cReadBack = icRead(pInterface, cRegister, 1);
        icWrite(pInterface, cRegister, (cReadBack & 0x8F) | (0x7 << 4));
        icWrite(pInterface, cRegister, (cReadBack & 0x8F) | (0x0 << 4));
    }
    // then for EC port
    uint16_t cRegister = 232;
    uint32_t cReadBack = icRead(pInterface, cRegister, 1);
    icWrite(pInterface, cRegister, (cReadBack & 0x8F) | (0x7 << 4));
    icWrite(pInterface, cRegister, (cReadBack & 0x8F) | (0x0 << 4));
}
void GbtInterface::gbtxSelectTerminationRx(BeBoardFWInterface* pInterface, bool pEnable)
{
    // for e-links
    uint8_t cRegValue = (pEnable) ? 0xFF : 0x00;
    for(int cGroup = 0; cGroup < 7; cGroup++)
    {
        uint16_t cRegister = 320 + cGroup;
        icWrite(pInterface, cRegister, cRegValue);
    }
    // then for EC port
    // uint16_t cRegister = 232;
}
void GbtInterface::gbtxFrameAlignerDLL(BeBoardFWInterface* pInterface, std::vector<uint8_t> pGroups, uint8_t pDLLcurrent, uint8_t pLockMode)
{
    for(auto cGroup: pGroups)
    {
        uint16_t cRegister = 64 + cGroup * 24;
        // registers A + B
        icWrite(pInterface, cRegister, (pDLLcurrent << 4) | (pDLLcurrent << 0));
        // register C
        uint32_t cReadBack = icRead(pInterface, cRegister + 1, 1);
        icWrite(pInterface, cRegister + 1, (cReadBack & 0xF0) | pDLLcurrent);
    }
    uint32_t cReadBack = icRead(pInterface, 233, 1);
    icWrite(pInterface, 233, (cReadBack & 0x8F) | (pLockMode << 4));
}
void GbtInterface::gbtxConfigure(BeBoardFWInterface* pInterface, uint8_t pDLLcurrent, uint8_t pDLLlockMode)
{
    // disable everything
    gbtxDisableAllLinks(pInterface);
    gbtxConfigureLinks(pInterface);
    // Configure input frame aligner's DLLs
    gbtxFrameAlignerDLL(pInterface, {0, 1, 2, 3, 4, 5, 6}, pDLLcurrent, pDLLlockMode);
    // reset input frame aligner DLLs
    gbtxResetFrameAlignerDLL(pInterface, {0, 1, 2, 3, 4, 5, 6});
}

// Temp replacement for new CPB
uint8_t GbtInterface::configI2C(BeBoardFWInterface* pInterface, uint16_t pMaster, uint8_t pNBytes, uint8_t pSclMode, int pFrequency)
{
    LOG(DEBUG) << BOLDBLUE << "Configuring I2C [SCA] to read " << +pNBytes << " in SCL mode [ " << +pSclMode << "] at a frequency of " << pFrequency << RESET;
    uint32_t cRegister  = (std::min(3, static_cast<int>(pFrequency / 200)) << 0 | pNBytes << 2 | pSclMode << 7) << 3 * 8;
    uint32_t cErrorCode = ecWrite(pInterface, pMaster, 0x30, cRegister);
    if(cErrorCode != 0)
    {
        LOG(INFO) << BOLDBLUE << "SCA Error code : " << +cErrorCode << RESET;
        return cErrorCode;
    }
    return cErrorCode;
}
uint32_t GbtInterface::readI2C(BeBoardFWInterface* pInterface, uint16_t pMaster, uint8_t pSlave, uint8_t pNBytes)
{
    configI2C(pInterface, pMaster, pNBytes);
    uint32_t pData = ecRead(pInterface, pMaster, (pNBytes == 1) ? 0x86 : 0xDE, (pSlave << 3 * 8));
    return ((pData & 0x00FF0000) >> 2 * 8);
}
uint8_t GbtInterface::writeI2C(BeBoardFWInterface* pInterface, uint16_t pMaster, uint8_t pSlave, uint32_t pData, uint8_t pNBytes)
{
    configI2C(pInterface, pMaster, pNBytes);
    if(pNBytes == 1)
    {
        uint32_t cData      = (pSlave << 3 * 8) | (pData << 2 * 8);
        uint8_t  cErrorCode = ecWrite(pInterface, pMaster, 0x82, cData);
        if(cErrorCode != 0)
            throw std::runtime_error(std::string("Error writing I2C [fpga error]..."));
        else
        {
            return cErrorCode;
        }
    }
    else
    {
        // upload data bytes to send in the DATA register
        uint32_t cErrorCode = ecWrite(pInterface, pMaster, 0x40, pData);
        if(cErrorCode != 0)
            throw std::runtime_error(std::string("Error writing I2C [fpga error]..."));
        else
        {
            cErrorCode = ecWrite(pInterface, pMaster, 0xDA, (pSlave << 3 * 8));
            return cErrorCode;
        }
    }
}

uint8_t GbtInterface::cbcGetPageRegister(BeBoardFWInterface* pInterface, uint8_t pFeId, uint8_t pChipId)
{
    uint8_t cAddress   = (0x40 | (pChipId + 1));
    uint8_t cErrorCode = writeI2C(pInterface, fSCAMaster + pFeId, cAddress, 0x00, 1);
    if(cErrorCode == 0)
    {
        uint8_t cValue = readI2C(pInterface, fSCAMaster + pFeId, cAddress, 1);
        LOG(DEBUG) << BOLDGREEN << "\t... page is currently set to " << std::hex << +cValue << std::dec << RESET;
        return cValue;
    }
    LOG(INFO) << BOLDYELLOW << "Error reading CBC page register." << RESET;
    return cErrorCode;
}
uint8_t GbtInterface::cbcSetPage(BeBoardFWInterface* pInterface, uint8_t pFeId, uint8_t pChipId, uint8_t pPage)
{
    LOG(DEBUG) << BOLDGREEN << "\t... setting page to " << +pPage << RESET;
    uint8_t cPageRegister = cbcGetPageRegister(pInterface, pFeId, pChipId);
    uint8_t cNewRegValue  = ((~pPage & 0x01) << 7) | (cPageRegister & 0x7F);
    LOG(DEBUG) << BOLDGREEN << "\t... setting page register to 0x" << std::hex << std::bitset<8>(+cNewRegValue) << std::dec << RESET;
    // uint32_t cValue  = (0x00 << 8*3) | (cNewRegValue << 8*2)  ;
    uint8_t cErrorCode = writeI2C(pInterface, fSCAMaster + pFeId, 0x40 | (1 + pChipId), (0x00 << 8 * 3) | (cNewRegValue << 8 * 2), 2);
    return cErrorCode;
}
uint32_t GbtInterface::cbcRead(BeBoardFWInterface* pInterface, uint8_t pFeId, uint8_t pChipId, uint8_t pPage, uint8_t pRegisterAddress)
{
    uint8_t cErrorCode = cbcSetPage(pInterface, pFeId, pChipId, pPage);
    if(cErrorCode != 0)
    {
        LOG(INFO) << BOLDYELLOW << "Error setting CBC page register." << RESET;
        return cErrorCode;
    }
    cErrorCode = writeI2C(pInterface, fSCAMaster + pFeId, 0x40 | (1 + pChipId), pRegisterAddress, 1);
    if(cErrorCode != 0) return cErrorCode;
    uint32_t cValue   = readI2C(pInterface, fSCAMaster + pFeId, 0x40 | (1 + pChipId), 1);
    uint8_t  cStatus  = this->scaStatus(pInterface, fSCAMaster + pFeId);
    bool     cSuccess = (((cStatus & 0x4) >> 2) == 1);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Error in read-back of I2C from CBC.." << RESET;
        throw std::runtime_error(std::string("I2C error with CBC on front-end hybrid.. "));
    }
    LOG(DEBUG) << BOLDBLUE << "Read back 0x" << std::hex << cValue << std::dec << " when reading register 0x" << std::hex << +pRegisterAddress << std::dec << " from page " << +pPage << " on CBC"
               << +pChipId << RESET;
    return cValue;
}
bool GbtInterface::cbcWrite(BeBoardFWInterface* pInterface, uint8_t pFeId, uint8_t pChipId, uint8_t pPage, uint8_t pRegisterAddress, uint8_t pRegisterValue, bool pReadBack, bool pSetPage)
{
    uint8_t cErrorCode = 0;
    if(pSetPage) { cErrorCode = cbcSetPage(pInterface, pFeId, pChipId, pPage); }
    cErrorCode = writeI2C(pInterface, fSCAMaster + pFeId, 0x40 | (1 + pChipId), (pRegisterAddress << 8 * 3) | (pRegisterValue << 8 * 2), 2);
    if(pReadBack && cErrorCode == 0)
    {
        uint8_t cStatus = this->scaStatus(pInterface, fSCAMaster + pFeId);
        // serial interface & error register is special
        // figure out how to avoid this if statement here
        uint32_t cReadBack = (pRegisterAddress == 0x1D) ? cbcRead(pInterface, pFeId, pChipId, pPage, pRegisterAddress) : pRegisterValue;
        LOG(DEBUG) << BOLDBLUE << "Read back " << +cReadBack << " when asking to write " << +pRegisterValue;
        bool cSuccess = (((cStatus & 0x4) >> 2) == 1);
        if(!cSuccess)
        {
            LOG(INFO) << BOLDRED << "SCA status " << std::bitset<8>(cStatus) << " for hybrid " << +pFeId << " readout chip " << +pChipId << " register 0x" << std::hex << +pRegisterAddress << std::dec
                      << " [CBC]." << RESET;
            throw std::runtime_error(std::string("SCA status reporting error..."));
        }
        if(cReadBack != pRegisterValue)
        {
            LOG(INFO) << BOLDRED << "I2C readback failed.. "
                      << " for hybrid " << +pFeId << " readout chip " << +pChipId << " register 0x" << std::hex << +pRegisterAddress << std::dec << " [CBC]." << RESET;
            throw std::runtime_error(std::string("I2C readback mismatch..."));
        }
        cSuccess = cSuccess && (cReadBack == pRegisterValue);
        return cSuccess;
    }
    else
        return (cErrorCode == 0);
}
bool GbtInterface::i2cWrite(BeBoardFWInterface* pInterface, const std::vector<uint32_t>& pVecSend, std::vector<uint32_t>& pReplies, bool pReadBack)
{
    // number of bytes to write at a time
    uint8_t cNBytes = 2;
    // uint8_t cNSimWrites = 1;
    // work in progress
    std::map<uint8_t, std::vector<uint32_t>> cI2C;
    cI2C.clear();

    auto cIterator = pVecSend.begin();
    // size_t cRegisters=0;
    SCAI2C cSCAcommand;
    while(cIterator < pVecSend.end())
    {
        uint32_t cWord = *cIterator;
        uint8_t  cFeId = (cWord & (0xF << 23)) >> 23;
        cI2C[(cFeId % 2) + fSCAMaster].push_back(cWord);
        cIterator++;
    }
    auto cI2Citerator = cI2C.begin();
    while(cI2Citerator != cI2C.end())
    {
        auto cMaster = cI2Citerator->first;
        LOG(DEBUG) << BOLDBLUE << " I2C master " << +cMaster << " :: " << cI2Citerator->second.size() << " registers to write to." << RESET;
        std::map<uint8_t, std::vector<uint32_t>> cI2Cw;
        std::map<uint8_t, std::vector<uint32_t>> cI2Cr;
        //
        cIterator = cI2Citerator->second.begin();
        while(cIterator < cI2Citerator->second.end())
        {
            uint32_t cWord    = *cIterator;
            uint8_t  cWrite   = !((cWord & (0x1 << 16)) >> 16);
            uint8_t  cLinkId  = (cWord & (0x3 << 29)) >> 29;
            uint8_t  cChipId  = (cWord & (0x1F << 18)) >> 18;
            uint8_t  cAddress = (cWord & 0xFF);
            uint8_t  cPage    = (cWord & (0xFF << 8)) >> 8;
            uint8_t  cValue   = 0;
            if(cWrite == 1)
            {
                cIterator++;
                cWord  = *cIterator;
                cValue = (cWord & 0xFF);
                cI2Cw[cLinkId].push_back((cPage << 20) | (cChipId << 16) | (cAddress << 8) | (cValue << 0));
            }
            else
                cI2Cr[cLinkId].push_back((cPage << 20) | (cChipId << 16) | (cAddress << 8) | (cValue << 0));

            cIterator++;
        }
        auto cWriteIterator = cI2Cw.begin();
        while(cWriteIterator != cI2Cw.end())
        {
            // select link
            LOG(DEBUG) << BOLDBLUE << " Link " << +cWriteIterator->first << " :: " << cWriteIterator->second.size() << " registers to write to." << RESET;
            pInterface->WriteReg("fc7_daq_cnfg.optical_block.mux", cWriteIterator->first);
            // std::this_thread::sleep_for (std::chrono::microseconds (100) );
            std::map<uint8_t, std::vector<uint32_t>> cI2Cpages;
            cIterator = cWriteIterator->second.begin();
            while(cIterator < cWriteIterator->second.end())
            {
                // cI2Cw[cLinkId].push_back( (cPage << 19 ) | (cChipId << 16) | (cAddress << 8) | (cValue << 0) );
                uint32_t cWord    = *cIterator;
                uint8_t  cPage    = (cWord & (0x3 << 20)) >> 20;
                uint8_t  cChipId  = (cWord & (0xF << 16)) >> 16;
                uint8_t  cAddress = (cWord & (0xFF << 8)) >> 8;
                uint8_t  cValue   = (cWord & (0xFF << 0)) >> 0;
                cI2Cpages[cPage].push_back((cChipId << 16) | (cAddress << 8) | (cValue << 0));
                cIterator++;
            }
            auto cPageIterator = cI2Cpages.begin();
            while(cPageIterator != cI2Cpages.end())
            {
                auto cPage = cPageIterator->first;
                LOG(DEBUG) << BOLDBLUE << "\t\t...Page " << +cPage << " :: " << cPageIterator->second.size() << " registers to write to." << RESET;
                std::map<uint8_t, std::vector<uint32_t>> cI2Cchip;
                cIterator = cPageIterator->second.begin();
                while(cIterator < cPageIterator->second.end())
                {
                    uint32_t cWord    = *cIterator;
                    uint8_t  cChipId  = (cWord & (0xF << 16)) >> 16;
                    uint8_t  cAddress = (cWord & (0xFF << 8)) >> 8;
                    uint8_t  cValue   = (cWord & (0xFF << 0)) >> 0;
                    cI2Cchip[cChipId].push_back((cAddress << 8) | (cValue << 0));
                    cIterator++;
                }
                auto cChipIterator = cI2Cchip.begin();
                while(cChipIterator != cI2Cchip.end())
                {
                    auto&   cChipId    = cChipIterator->first;
                    auto&   cRegisters = cChipIterator->second;
                    uint8_t cSlave     = 0;
                    if(cChipId < 8) // CBC
                    {
                        cSlave  = 0x40 | (cChipId + 1);
                        cNBytes = 2;
                        this->cbcSetPage(pInterface, (cMaster - fSCAMaster), cChipId, cPage + 1);
                    }
                    else
                    {
                        cSlave  = 0x60;
                        cNBytes = 3;
                    }

                    // configure I2C master to write  Nbytes at a time
                    LOG(DEBUG) << BOLDBLUE << "\t\t...Chip " << +cChipId << " :: " << std::hex << +cSlave << std::dec << " slave address " << cRegisters.size() << " registers to write [ " << +cNBytes
                               << " bytes per register]" << RESET;

                    cIterator = cRegisters.begin();
                    while(cIterator < cRegisters.end())
                    {
                        uint32_t cWord = *cIterator;
                        // upload data bytes to send in the DATA register
                        uint8_t cAddress = (cWord & (0xFF << 8)) >> 8;
                        uint8_t cValue   = (cWord & (0xFF << 0)) >> 0;
                        if(cChipId < 8)
                            this->cbcWrite(pInterface, (cMaster - fSCAMaster), cChipId, cPage + 1, cAddress, cValue, pReadBack);
                        else
                            this->cicWrite(pInterface, (cMaster - fSCAMaster), cAddress, cValue, pReadBack);
                        cIterator++;
                    }
                    cChipIterator++;
                }
                cPageIterator++;
            }
            cWriteIterator++;
        }
        auto cReadIterator = cI2Cr.begin();
        while(cReadIterator != cI2Cr.end())
        {
            LOG(INFO) << BOLDBLUE << " Link " << +cReadIterator->first << " :: " << cReadIterator->second.size() << " registers to read from." << RESET;
            pInterface->WriteReg("fc7_daq_cnfg.optical_block.mux", cReadIterator->first);
            std::this_thread::sleep_for(std::chrono::microseconds(100));

            cReadIterator++;
        }
        cI2Citerator++;
    }
    return true;
}

uint32_t GbtInterface::cicRead(BeBoardFWInterface* pInterface, uint8_t pFeId, uint8_t pRegisterAddress)
{
    writeI2C(pInterface, fSCAMaster + pFeId, 0x60, (pRegisterAddress << 16), 2);
    uint32_t cValue   = readI2C(pInterface, fSCAMaster + pFeId, 0x60, 1);
    uint8_t  cStatus  = this->scaStatus(pInterface, fSCAMaster + pFeId);
    bool     cSuccess = (((cStatus & 0x4) >> 2) == 1);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Error in read-back of I2C from CIC.." << RESET;
        throw std::runtime_error(std::string("I2C error with CIC on front-end hybrid.. "));
    }
    LOG(DEBUG) << BOLDBLUE << "Read back 0x" << std::hex << cValue << std::dec << " when reading register 0x" << std::hex << +pRegisterAddress << std::dec << " on CIC on FE" << +pFeId << RESET;
    return cValue;
}
bool GbtInterface::cicWrite(BeBoardFWInterface* pInterface, uint8_t pFeId, uint8_t pRegisterAddress, uint8_t pRegisterValue, bool pReadBack)
{
    uint8_t cWrite = writeI2C(pInterface, fSCAMaster + pFeId, 0x60, (pRegisterAddress << 16) | (pRegisterValue << 8), 3);
    if(pReadBack && cWrite == 0)
    {
        uint8_t cStatus = this->scaStatus(pInterface, fSCAMaster + pFeId);
        // serial interface & error register is special
        // figure out how to avoid this if statement here
        uint32_t cReadBack = this->cicRead(pInterface, pFeId, pRegisterAddress);
        // uint32_t cReadBack = ( pRegisterAddress == 0x1D) ? cbcRead(pInterface, pFeId, pChipId, pPage ,
        // pRegisterAddress) : pRegisterValue ;
        LOG(DEBUG) << BOLDBLUE << "Read back " << +cReadBack << " after trying to write... " << +pRegisterValue;
        bool cSuccess = (((cStatus & 0x4) >> 2) == 1);
        if(!cSuccess)
        {
            LOG(INFO) << BOLDRED << "SCA status " << std::bitset<8>(cStatus) << " for hybrid " << +pFeId << " register 0x" << std::hex << +pRegisterAddress << std::dec << " [CIC]." << RESET;
            throw std::runtime_error(std::string("SCA status reporting error..."));
        }
        if(cReadBack != pRegisterValue)
        {
            LOG(INFO) << BOLDRED << "I2C readback failed.. "
                      << " for hybrid " << +pFeId << " register 0x" << std::hex << +pRegisterAddress << std::dec << " [CIC]." << RESET;
            throw std::runtime_error(std::string("I2C readback mismatch..."));
        }
        cSuccess = cSuccess && (cReadBack == pRegisterValue);
        return cSuccess;
    }
    else
        return (cWrite == 0);
}

float GbtInterface::convAdcToTemp(float pAdcValue, std::string pThermistor)
{
    std::tuple<int, int, int> cThermistor = fNTCThermistorMap[pThermistor];
    // calculate resistance (current source is 100uA)
    float cResistance = pAdcValue / 0.0001;
    return 1 / (1. / std::get<0>(cThermistor) + log(float(cResistance) / std::get<1>(cThermistor)) / (1.0 * std::get<2>(cThermistor))) - 273;
}

} // namespace Ph2_HwInterface
