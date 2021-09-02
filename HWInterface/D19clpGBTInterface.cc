/*!
  \file                  D19clpGBTInterface.cc
  \brief                 Interface to access and control the low-power Gigabit Transceiver chip
  \author                Younes Otarid
  \version               1.0
  \date                  03/03/20
  Support:               email to younes.otarid@cern.ch
*/

#include "D19clpGBTInterface.h"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
bool D19clpGBTInterface::ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop, uint32_t pBlockSize)
{
    LOG(INFO) << BOLDMAGENTA << "Configuring lpGBT" << RESET;
    setBoard(pChip->getBeBoardId());
    SetConfigMode(pChip, fUseOpticalLink, fUseCPB);
    // Load register map from configuration file
    if(!fUseOpticalLink)
    {
        ChipRegMap clpGBTRegMap = pChip->getRegMap();
        for(const auto& cRegItem: clpGBTRegMap)
        {
            if(cRegItem.second.fAddress < 0x13c)
            {
                LOG(INFO) << BOLDBLUE << "\tWriting 0x" << std::hex << +cRegItem.second.fValue << std::dec << " to " << cRegItem.first << " [0x" << std::hex << +cRegItem.second.fAddress << std::dec
                          << "]" << RESET;
                WriteReg(pChip, cRegItem.second.fAddress, cRegItem.second.fValue);
            }
        }
    }
    PrintChipMode(pChip);
    SetPUSMDone(pChip, true, true);
    uint16_t cIter = 0, cMaxIter = 200;
    while(!IsPUSMDone(pChip) && cIter < cMaxIter)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cIter++;
    }
    if(cIter == cMaxIter) throw std::runtime_error(std::string("lpGBT Power-Up State Machine NOT DONE"));
    LOG(INFO) << BOLDGREEN << "lpGBT Configured [READY]" << RESET;
#ifdef __ROH_USB__
    ConfigurePSROH(pChip);
#elif __SEH_USB__
    Configure2SSEH(pChip);
#endif
    return true;
} //

/*---------------------------------*/
/* Read/Write LpGBT chip registers */
/*---------------------------------*/

bool D19clpGBTInterface::WriteChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop)
{
    LOG(DEBUG) << BOLDBLUE << "\t Writing 0x" << std::hex << +pValue << std::dec << " to " << pRegNode << " [0x" << std::hex << +pChip->getRegItem(pRegNode).fAddress << std::dec << "]" << RESET;
    return WriteReg(pChip, pChip->getRegItem(pRegNode).fAddress, pValue, pVerifLoop);
}

uint16_t D19clpGBTInterface::ReadChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode)
{
    uint8_t cReadBack = ReadReg(pChip, pChip->getRegItem(pRegNode).fAddress);
    LOG(DEBUG) << BOLDWHITE << "\t Reading 0x" << std::hex << +cReadBack << std::dec << " from " << pRegNode << " [0x" << std::hex << +pChip->getRegItem(pRegNode).fAddress << std::dec << "]" << RESET;
    return cReadBack;
}

bool D19clpGBTInterface::WriteReg(Ph2_HwDescription::Chip* pChip, uint16_t pAddress, uint16_t pValue, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    uint8_t cReadBack = 0;
    // Make sure the value is not > 8 bits
    if(pValue > 0xFF)
    {
        LOG(ERROR) << "LpGBT registers are 8 bits, impossible to write " << pValue << " to address " << pAddress;
        return false;
    }
    if(pAddress >= 0x13c)
    {
        LOG(ERROR) << "LpGBT read-write registers end at 0x13c ... impossible to write to " << +pAddress;
        return false;
    }
    // Now pick one configuration mode
    if(fUseOpticalLink)
    {
        if(fUseCPB)
            return fBoardFW->WriteLpGBTRegister(pAddress, pValue, pVerifLoop);
        else
            return fBoardFW->WriteOptoLinkRegister(pChip->getId(), static_cast<lpGBT*>(pChip)->getChipAddress(), pAddress, pValue, pVerifLoop);
    }
    else
    {
        // use PS-ROH test card USB interface
#ifdef __TCUSB__
        // use 2S_SEH test card USB interface
#ifdef __TCP_SERVER__
        throw std::runtime_error(std::string("lpGBT slave I2C not available in TCP mode!"));
#else
        fTC_USB->write_i2c(pAddress, static_cast<char>(pValue));
#endif
#endif
    }
    return true;
    // FIXME USB interface needs verification loop here or library ?
    if(!pVerifLoop)
        // Verify success of Write
        if(!fUseOpticalLink)
        {
            uint8_t cIter = 0, cMaxIter = 50;
            while(cReadBack != pValue && cIter < cMaxIter)
            {
                // Now pick one configuration mode
                // use PS-ROH test card USB interface
#ifdef __TCUSB__
                // Dont really see the point here. Write_i2c does not return a read back???
                // cReadBack = fTC_2SSEH.write_i2c(pAddress, static_cast<char>(pValue));
#ifdef __TCP_SERVER__
                throw std::runtime_error(std::string("lpGBT slave I2C not available in TCP mode!"));
#else
                cReadBack = fTC_USB->read_i2c(pAddress);
#endif
#endif
                cIter++;
            }
            if(cIter == cMaxIter) throw std::runtime_error(std::string("lpGBT register write mismatch"));
        }
    return true;
}

uint16_t D19clpGBTInterface::ReadReg(Ph2_HwDescription::Chip* pChip, uint16_t pAddress)
{
    setBoard(pChip->getBeBoardId());
    if(fUseOpticalLink)
    {
        if(fUseCPB)
            return fBoardFW->ReadLpGBTRegister(pAddress);
        else
            return fBoardFW->ReadOptoLinkRegister(pChip->getId(), static_cast<lpGBT*>(pChip)->getChipAddress(), pAddress);
    }
    else
    {
// use PS-ROH test card USB interface
#ifdef __TCUSB__
#ifdef __TCP_SERVER__
        throw std::runtime_error(std::string("lpGBT slave I2C not avilable in TCP mode!"));
#else
        return fTC_USB->read_i2c(pAddress);
#endif
#endif
    }
    return 0;
}

bool D19clpGBTInterface::WriteChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& pRegVec, bool pVerifLoop)
{
    bool writeGood = true;
    for(const auto& cReg: pRegVec) writeGood = WriteChipReg(pChip, cReg.first, cReg.second);
    return writeGood;
}

/*-----------------------*/
/* OT specific functions */
/*-----------------------*/

void D19clpGBTInterface::SetConfigMode(Ph2_HwDescription::Chip* pChip, bool pUseOpticalLink, bool pUseCPB, bool pToggleTC)
{
    if(pUseOpticalLink)
    {
#ifdef __TCUSB__
#ifdef __ROH_USB__
        LOG(INFO) << BOLDBLUE << "Toggling Test Card" << RESET;
        if(pToggleTC) fTC_USB->toggle_SCI2C();
#endif
#endif
        LOG(INFO) << BOLDGREEN << "Using Serial Interface configuration mode" << RESET;
        fUseOpticalLink = true;
        if(pUseCPB)
        {
            LOG(INFO) << BOLDGREEN << "Using Command Processor Block" << RESET;
            fUseCPB = true;
        }
    }
    else
    {
        LOG(INFO) << BOLDGREEN << "Using I2C Slave Interface configuration mode" << RESET;
        fUseOpticalLink = false;
        fUseCPB         = false;
    }
}

#ifdef __TCUSB__
void D19clpGBTInterface::InitialiseTCUSBHandler()
{
#ifdef __ROH_USB__
    fTC_USB = new TC_PSROH();
    LOG(INFO) << BOLDGREEN << "Initialised PS-ROH TestCard USB Handler" << RESET;
#elif __SEH_USB__
#ifdef __TCP_SERVER__
#else
    fTC_USB = new TC_2SSEH();
#endif
    LOG(INFO) << BOLDGREEN << "Initialised 2S-SEH TestCard USB Handler" << RESET;
#endif
}
#endif

// Preliminary
void D19clpGBTInterface::Configure2SSEH(Ph2_HwDescription::Chip* pChip)
{
    // uint8_t cChipRate = GetChipRate(pChip);
    LOG(INFO) << BOLDGREEN << "Applying 2S-SEH 5G lpGBT configuration" << RESET;
    // Configure High Speed Link Tx Rx Polarity
    ConfigureHighSpeedPolarity(pChip, 1, 0);

    // Clocks
    std::vector<uint8_t> cClocks  = {1, 11}; // Reduced number of clocks and only 320 MHz
    uint8_t              cClkFreq = 4, cClkDriveStr = 7, cClkInvert = 1;
    uint8_t              cClkPreEmphWidth = 0, cClkPreEmphMode = 0, cClkPreEmphStr = 0;
    ConfigureClocks(pChip, cClocks, cClkFreq, cClkDriveStr, cClkInvert, cClkPreEmphWidth, cClkPreEmphMode, cClkPreEmphStr);
    // Tx Groups and Channels
    std::vector<uint8_t> cTxGroups =
                             {
                                 0,
                                 2,
                             },
                         cTxChannels = {0};
    uint8_t cTxDataRate = 3, cTxDriveStr = 7, cTxPreEmphMode = 1, cTxPreEmphStr = 4, cTxPreEmphWidth = 0, cTxInvert = 0;
    ConfigureTxGroups(pChip, cTxGroups, cTxChannels, cTxDataRate);
    for(const auto& cGroup: cTxGroups)
    {
        if(cGroup == 0) cTxInvert = 1;
        if(cGroup == 2) cTxInvert = 0;
        for(const auto& cChannel: cTxChannels) ConfigureTxChannels(pChip, {cGroup}, {cChannel}, cTxDriveStr, cTxPreEmphMode, cTxPreEmphStr, cTxPreEmphWidth, cTxInvert);
    }
    // Rx configuration and Phase Align
    // Configure Rx Groups
    std::vector<uint8_t> cRxGroups = {0, 1, 2, 3, 4, 5, 6}, cRxChannels = {0, 2};
    uint8_t              cRxDataRate = 2, cRxTrackMode = 1;
    ConfigureRxGroups(pChip, cRxGroups, cRxChannels, cRxDataRate, cRxTrackMode);
    // Configure Rx Channels
    uint8_t cRxEqual = 0, cRxTerm = 1, cRxAcBias = 0, cRxInvert = 0, cRxPhase = 12;
    for(const auto& cGroup: cRxGroups)
    {
        for(const auto cChannel: cRxChannels)
        {
            if(cGroup == 6 && cChannel == 0)
                cRxInvert = 0;
            else if(cGroup == 5 && cChannel == 0)
                cRxInvert = 0;
            else
                cRxInvert = 1;
            if(!((cGroup == 6 && cChannel == 2) || (cGroup == 3 && cChannel == 0))) ConfigureRxChannels(pChip, {cGroup}, {cChannel}, cRxEqual, cRxTerm, cRxAcBias, cRxInvert, cRxPhase);
        }
    }
#ifdef __TCUSB__
    ContinuousPhaseAlignRx(pChip, cRxGroups, cRxChannels);
#endif
    // Reset I2C Masters
    ResetI2C(pChip, {0, 1, 2});
    // Setting GPIO levels Resets are high
    ConfigureGPIODirection(pChip, {0, 3, 6, 8}, 1);
    ConfigureGPIOLevel(pChip, {0, 3, 6, 8}, 1);
    ConfigureCurrentDAC(pChip, std::vector<std::string>{"ADC4"}, 0x1c); // current chosen according to measurement range
}
void D19clpGBTInterface::ContinuousPhaseAlignRx(Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels)
{
    // Configure Rx Phase Shifter

    D19clpGBTInterface::ConfigureRxGroups(pChip, pGroups, pChannels, 2, 2);
}

void D19clpGBTInterface::ConfigurePSROH(Ph2_HwDescription::Chip* pChip)
{
    uint8_t cChipRate = GetChipRate(pChip);
    LOG(INFO) << BOLDGREEN << "Applying PS-ROH-" << +cChipRate << "G lpGBT configuration" << RESET;
    // Configure High Speed Link Tx Rx Polarity
    ConfigureHighSpeedPolarity(pChip, 1, 0);
    // Clocks
    std::vector<uint8_t> cClocks  = {1, 6, 11, 26};
    uint8_t              cClkFreq = (cChipRate == 5) ? 4 : 5, cClkDriveStr = 7, cClkInvert = 1;
    uint8_t              cClkPreEmphWidth = 0, cClkPreEmphMode = 0, cClkPreEmphStr = 0;
    ConfigureClocks(pChip, cClocks, cClkFreq, cClkDriveStr, cClkInvert, cClkPreEmphWidth, cClkPreEmphMode, cClkPreEmphStr);
    // Tx Groups and Channels
    std::vector<uint8_t> cTxGroups = {0, 1, 2, 3}, cTxChannels = {0};
    uint8_t              cTxDataRate = 3, cTxDriveStr = 7, cTxPreEmphMode = 1, cTxPreEmphStr = 4, cTxPreEmphWidth = 0, cTxInvert = 0;
    ConfigureTxGroups(pChip, cTxGroups, cTxChannels, cTxDataRate);
    for(const auto& cGroup: cTxGroups)
    {
        cTxInvert = (cGroup % 2 == 0) ? 1 : 0;
        for(const auto& cChannel: cTxChannels) ConfigureTxChannels(pChip, {cGroup}, {cChannel}, cTxDriveStr, cTxPreEmphMode, cTxPreEmphStr, cTxPreEmphWidth, cTxInvert);
    }
    // Rx configuration and Phase Align
    // Configure Rx Groups
    std::vector<uint8_t> cRxGroups = {0, 1, 2, 3, 4, 5, 6}, cRxChannels = {0, 2};
    uint8_t              cRxDataRate = 2, cRxTrackMode = 1;
    ConfigureRxGroups(pChip, cRxGroups, cRxChannels, cRxDataRate, cRxTrackMode);
    // Configure Rx Channels
    uint8_t cRxEqual = 0, cRxTerm = 1, cRxAcBias = 0, cRxInvert = 0, cRxPhase = 7;
    for(const auto& cGroup: cRxGroups)
    {
        for(const auto cChannel: cRxChannels)
        {
            // Right Hybrid
            if(cGroup == 0 && cChannel == 0)
                cRxInvert = 1;
            else if(cGroup == 4 || cGroup == 5 || cGroup == 6)
                cRxInvert = 0;
            // Left Hybrid
            else if(cGroup == 1 && cChannel == 0)
                cRxInvert = 1;
            else if(cGroup == 3 && cChannel == 2)
                cRxInvert = 1;
            else if(cGroup == 2)
                cRxInvert = 0;
            ConfigureRxChannels(pChip, {cGroup}, {cChannel}, cRxEqual, cRxTerm, cRxAcBias, cRxInvert, cRxPhase);
        }
    }
    // InternalPhaseAlignRx(pChip, cRxGroups, cRxChannels);
    // Reset I2C Masters
    ResetI2C(pChip, {0, 1, 2});
    // Setting GPIO levels for Skeleton test
    ConfigureGPIODirection(pChip, {0, 1, 3, 6, 9, 12}, 1);
    ConfigureGPIOLevel(pChip, {0, 1, 3, 6, 9, 12}, 1);
}
bool D19clpGBTInterface::cicWrite(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry)
{
    LOG(DEBUG) << BOLDBLUE << "CIC Writing 0x" << std::hex << +pRegisterValue << std::dec << " to [0x" << std::hex << +pRegisterAddress << std::dec << "]" << RESET;
    uint16_t cInvertedRegister = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
    WriteI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, 0x60, (pRegisterValue << 16) | cInvertedRegister, 3);
    if(pRetry)
    {
        uint8_t cReadBack = cicRead(pChip, pFeId, pRegisterAddress);
        uint8_t cIter = 0, cMaxIter = 10;
        while(cReadBack != pRegisterValue && cIter < cMaxIter)
        {
            LOG(INFO) << BOLDRED << "CIC I2C ReadBack Mismatch in hybrid " << +pFeId << " register 0x" << std::hex << +pRegisterAddress << std::dec << RESET;
            WriteI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, 0x60, (pRegisterValue << 16) | cInvertedRegister, 3);
            cReadBack = cicRead(pChip, pFeId, pRegisterAddress);
            cIter++;
        }
        if(cReadBack != pRegisterValue) { throw std::runtime_error(std::string("CIC readback mismatch")); }
    }
    return true;
}

uint32_t D19clpGBTInterface::cicRead(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint16_t pRegisterAddress)
{
    uint16_t cInvertedRegister = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
    WriteI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, 0x60, cInvertedRegister, 2);
    uint8_t cReadBack = ReadI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, 0x60, 1);
    LOG(DEBUG) << BOLDYELLOW << "CIC Reading 0x" << std::hex << +cReadBack << std::dec << " from [0x" << std::hex << +pRegisterAddress << std::dec << "]" << RESET;
    return cReadBack;
}

bool D19clpGBTInterface::ssaWrite(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t pChipId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry)
{
    bool cWriteOnlyReg = (pRegisterAddress & 0x7f) == 0x00;
    LOG(DEBUG) << BOLDBLUE << "SSA Writing 0x" << std::hex << +pRegisterValue << std::dec << " to [0x" << std::hex << +pRegisterAddress << std::dec << "]" << RESET;
    uint16_t cInvertedRegister = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
    WriteI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, 0x20 + pChipId, (pRegisterValue << 16) | cInvertedRegister, 3);

    if(cWriteOnlyReg) return true;

    if(pRetry)
    {
        uint8_t cReadBack = ssaRead(pChip, pFeId, pChipId, pRegisterAddress);
        uint8_t cIter = 0, cMaxIter = 10;
        while(cReadBack != pRegisterValue && cIter < cMaxIter)
        {
            LOG(INFO) << BOLDRED << "SSA I2C ReadBack Mismatch in hybrid " << +pFeId << " Chip " << +pChipId << " register 0x" << std::hex << +pRegisterAddress << std::dec << RESET;
            WriteI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, 0x20 + pChipId, (pRegisterValue << 16) | cInvertedRegister, 3);
            cReadBack = ssaRead(pChip, pFeId, pChipId, pRegisterAddress);
            cIter++;
        }
        if(cReadBack != pRegisterValue) { throw std::runtime_error(std::string("SSA readback mismatch")); }
    }
    return true;
}

uint32_t D19clpGBTInterface::ssaRead(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t pChipId, uint16_t pRegisterAddress)
{
    uint16_t cInvertedRegister = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
    WriteI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, 0x20 + pChipId, cInvertedRegister, 2);
    uint8_t cReadBack = ReadI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, 0x20 + pChipId, 1);
    LOG(DEBUG) << BOLDYELLOW << "SSA Reading 0x" << std::hex << +cReadBack << std::dec << " from [0x" << std::hex << +pRegisterAddress << std::dec << "]" << RESET;
    return cReadBack;
}

bool D19clpGBTInterface::mpaWrite(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t pChipId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry)
{
    uint8_t cSlaveAddress = (0x2 << 5) + pChipId;
    LOG(DEBUG) << BOLDBLUE << "MPA Write : SlaveAddress 0x" << std::hex << +cSlaveAddress << std::dec << " Register address : 0x" << std::hex << +pRegisterAddress << std::dec << " Register value : 0x"
               << std::hex << +pRegisterValue << std::dec << RESET;
    uint16_t cInvertedRegister = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
    WriteI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, cSlaveAddress, (pRegisterValue << 16) | cInvertedRegister, 3);
    if(pRetry)
    {
        uint8_t cReadBack = mpaRead(pChip, pFeId, pChipId, pRegisterAddress);
        uint8_t cIter = 0, cMaxIter = 10;
        while(cReadBack != pRegisterValue && cIter < cMaxIter)
        {
            LOG(INFO) << BOLDRED << "MPA I2C ReadBack Mismatch in hybrid " << +pFeId << " Chip " << +pChipId << " register 0x" << std::hex << +pRegisterAddress << std::dec << RESET;
            WriteI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, cSlaveAddress, (pRegisterValue << 16) | cInvertedRegister, 3);
            cReadBack = mpaRead(pChip, pFeId, pChipId, pRegisterAddress);
            cIter++;
        }
        if(cReadBack != pRegisterValue) { throw std::runtime_error(std::string("MPA readback mismatch")); }
    }
    return true;
}

uint32_t D19clpGBTInterface::mpaRead(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t pChipId, uint16_t pRegisterAddress)
{
    uint8_t  cSlaveAddress     = (0x2 << 5) + pChipId;
    uint16_t cInvertedRegister = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
    WriteI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, cSlaveAddress, cInvertedRegister, 2);
    uint32_t cReadBack = ReadI2C(pChip, ((pFeId % 2) == 0) ? 2 : 0, cSlaveAddress, 1);
    LOG(DEBUG) << BOLDYELLOW << "MPA Reading 0x" << std::hex << +cReadBack << std::dec << " from [0x" << std::hex << +pRegisterAddress << std::dec << "]" << RESET;
    return cReadBack;
}
} // namespace Ph2_HwInterface
