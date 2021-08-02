/*!
  \file                  D19clpGBTInterface.cc
  \brief                 Interface to access and control the low-power Gigabit Transceiver chip
  \author                Younes Otarid
  \version               1.0
  \date                  03/03/20
  Support:               email to younes.otarid@cern.ch
*/

#include "D19clpGBTInterface.h"
#include <bitset>
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
#ifdef __SEH_USB__
    fTC_USB->set_SehSupply(fTC_USB->sehSupply_On);
    LOG(INFO) << BOLDRED << "Intitally switching on SEH for configuration" << RESET;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
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
}

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
            return fBoardFW->WriteOptoLinkRegister(pChip->getId(), pAddress, pValue, pVerifLoop);
    }
    else
    {
        // use PS-ROH test card USB interface
#ifdef __TCUSB__
        // use 2S_SEH test card USB interface
        // fTC_2SSEH.write_i2c(pAddress, static_cast<char>(pValue));
        fTC_USB->write_i2c(pAddress, static_cast<char>(pValue));
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
                cReadBack = fTC_USB->write_i2c(pAddress, static_cast<char>(pValue));
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
            return fBoardFW->ReadOptoLinkRegister(pChip->getId(), pAddress);
    }
    else
    {
// use PS-ROH test card USB interface
#ifdef __TCUSB__
        // return fTC_2SSEH.read_i2c(pAddress);
        return fTC_USB->read_i2c(pAddress);
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

/*-------------------------------*/
/* lpGBT configuration functions */
/*-------------------------------*/
void D19clpGBTInterface::SetPUSMDone(Ph2_HwDescription::Chip* pChip, bool pPllConfigDone, bool pDllConfigDone) { WriteChipReg(pChip, "POWERUP2", pDllConfigDone << 2 | pPllConfigDone << 1); }

void D19clpGBTInterface::ConfigureRxGroups(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, uint8_t pDataRate, uint8_t pTrackMode)
{
    for(const auto& cGroup: pGroups)
    {
        // Enable Rx Groups Channels and set Data Rate and Phase Tracking mode
        uint8_t cValueEnableRx = 0;
        for(const auto cChannel: pChannels) cValueEnableRx |= (1 << cChannel);
        std::string cRXCntrlReg = "EPRX" + std::to_string(cGroup) + "Control";
        WriteChipReg(pChip, cRXCntrlReg, (cValueEnableRx << 4) | (pDataRate << 2) | (pTrackMode << 0));
    }
}

void D19clpGBTInterface::ConfigureRxChannels(Ph2_HwDescription::Chip*    pChip,
                                             const std::vector<uint8_t>& pGroups,
                                             const std::vector<uint8_t>& pChannels,
                                             uint8_t                     pEqual,
                                             uint8_t                     pTerm,
                                             uint8_t                     pAcBias,
                                             uint8_t                     pInvert,
                                             uint8_t                     pPhase)
{
    for(const auto& cGroup: pGroups)
    {
        for(const auto& cChannel: pChannels)
        {
            // Configure Rx Channel Phase, Inversion, AcBias enabling, Termination enabling, Equalization enabling
            std::string cRXChnCntrReg = "EPRX" + std::to_string(cGroup) + std::to_string(cChannel) + "ChnCntr";
            WriteChipReg(pChip, cRXChnCntrReg, (pPhase << 4) | (pInvert << 3) | (pAcBias << 2) | (pTerm << 1) | (pEqual << 0));
        }
    }
}

void D19clpGBTInterface::ConfigureTxGroups(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, uint8_t pDataRate)
{
    for(const auto& cGroup: pGroups)
    {
        // Configure Tx Group Data Rate value for specified group
        uint8_t cValueDataRate = ReadChipReg(pChip, "EPTXDataRate");
        WriteChipReg(pChip, "EPTXDataRate", (cValueDataRate & ~(0x03 << 2 * cGroup)) | (pDataRate << 2 * cGroup));
        // Enable given channels for specified group
        std::string cEnableTxReg;
        if(cGroup == 0 || cGroup == 1)
            cEnableTxReg = "EPTX10Enable";
        else if(cGroup == 2 || cGroup == 3)
            cEnableTxReg = "EPTX32Enable";
        uint8_t cValueEnableTx = ReadChipReg(pChip, cEnableTxReg);
        for(const auto cChannel: pChannels) cValueEnableTx |= (1 << (cChannel + 4 * (cGroup % 2)));
        WriteChipReg(pChip, cEnableTxReg, cValueEnableTx);
    }
}

void D19clpGBTInterface::ConfigureTxChannels(Ph2_HwDescription::Chip*    pChip,
                                             const std::vector<uint8_t>& pGroups,
                                             const std::vector<uint8_t>& pChannels,
                                             uint8_t                     pDriveStr,
                                             uint8_t                     pPreEmphMode,
                                             uint8_t                     pPreEmphStr,
                                             uint8_t                     pPreEmphWidth,
                                             uint8_t                     pInvert)
{
    for(const auto& cGroup: pGroups)
    {
        for(const auto& cChannel: pChannels)
        {
            // Configure Tx Channel PreEmphasisStrenght, PreEmphasisMode, DriveStrength
            std::string cTXChnCntrl = "EPTX" + std::to_string(cGroup) + std::to_string(cChannel) + "ChnCntr";
            WriteChipReg(pChip, cTXChnCntrl, (pPreEmphStr << 5) | (pPreEmphMode << 3) | (pDriveStr << 0));

            // Configure Tx Channel PreEmphasisWidth, Inversion
            std::string cTXChn_Cntr;
            if(cChannel == 0 || cChannel == 1)
                cTXChn_Cntr = "EPTX" + std::to_string(cGroup) + "1_" + std::to_string(cGroup) + "0ChnCntr";
            else if(cChannel == 2 || cChannel == 3)
                cTXChn_Cntr = "EPTX" + std::to_string(cGroup) + "3_" + std::to_string(cGroup) + "2ChnCntr";
            uint8_t cValue_ChnCntr = ReadChipReg(pChip, cTXChn_Cntr);
            WriteChipReg(pChip, cTXChn_Cntr, (cValue_ChnCntr & ~(0x0F << 4 * (cChannel % 2))) | ((pInvert << 3 | pPreEmphWidth << 0) << 4 * (cChannel % 2)));
        }
    }
}

void D19clpGBTInterface::ConfigureClocks(Ph2_HwDescription::Chip*    pChip,
                                         const std::vector<uint8_t>& pClocks,
                                         uint8_t                     pFreq,
                                         uint8_t                     pDriveStr,
                                         uint8_t                     pInvert,
                                         uint8_t                     pPreEmphWidth,
                                         uint8_t                     pPreEmphMode,
                                         uint8_t                     pPreEmphStr)
{
    LOG(INFO) << BOLDMAGENTA << "Configuring Clocks" << RESET;
    for(const auto& cClock: pClocks)
    {
        // Configure Clocks Frequency, Drive Strength, Inversion, Pre-Emphasis Width, Pre-Emphasis Mode, Pre-Emphasis Strength
        std::string cClkHReg = "EPCLK" + std::to_string(cClock) + "ChnCntrH";
        std::string cClkLReg = "EPCLK" + std::to_string(cClock) + "ChnCntrL";
        WriteChipReg(pChip, cClkHReg, pInvert << 6 | pDriveStr << 3 | pFreq);
        WriteChipReg(pChip, cClkLReg, pPreEmphStr << 5 | pPreEmphMode << 3 | pPreEmphWidth);
    }
}

void D19clpGBTInterface::ConfigureHighSpeedPolarity(Ph2_HwDescription::Chip* pChip, uint8_t pOutPolarity, uint8_t pInPolarity)
{
    // Configure Highr Speed Link Rx and Tx polarity
    uint8_t cPolarity = (pOutPolarity << 7 | pInPolarity << 6);
    WriteChipReg(pChip, "ChipConfig", cPolarity);
}

void D19clpGBTInterface::ConfigureDPPattern(Ph2_HwDescription::Chip* pChip, uint32_t pPattern)
{
    // Configure Constant Pattern
    LOG(INFO) << BOLDMAGENTA << "Setting DP Pattern to " << std::bitset<32>(pPattern) << RESET;
    WriteChipReg(pChip, "DPDataPattern0", (pPattern & 0xFF));
    WriteChipReg(pChip, "DPDataPattern1", ((pPattern & 0xFF00) >> 8));
    WriteChipReg(pChip, "DPDataPattern2", ((pPattern & 0xFF0000) >> 16));
    WriteChipReg(pChip, "DPDataPattern3", ((pPattern & 0xFF000000) >> 24));
}

void D19clpGBTInterface::ConfigureRxPRBS(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, bool pEnable)
{
    // Configure build-in PRBS generators
    for(const auto& cGroup: pGroups)
    {
        std::string cPRBSReg;
        if(cGroup == 1 || cGroup == 0)
            cPRBSReg = "EPRXPRBS0";
        else if(cGroup == 3 || cGroup == 2)
            cPRBSReg = "EPRXPRBS1";
        else if(cGroup == 5 || cGroup == 4)
            cPRBSReg = "EPRXPRBS2";
        else if(cGroup == 6)
            cPRBSReg = "EPRXPRBS3";

        uint8_t cValueEnablePRBS = ReadChipReg(pChip, cPRBSReg);
        uint8_t cEnabledCh       = 0;
        for(const auto cChannel: pChannels) cEnabledCh |= pEnable << cChannel;
        WriteChipReg(pChip, cPRBSReg, (cValueEnablePRBS & ~(0xF << 4 * (cGroup % 2))) | (cEnabledCh << (4 * (cGroup % 2))));
    }
}

void D19clpGBTInterface::ConfigureRxSource(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, uint8_t pSource)
{
    // Configure Rx Data Source
    for(const auto& cGroup: pGroups)
    {
        if(pSource == 0)
            LOG(DEBUG) << BOLDBLUE << "Configuring Rx Group " << +cGroup << " Source to NORMAL " << RESET;
        else if(pSource == 1)
            LOG(DEBUG) << BOLDBLUE << "Configuring Rx Group " << +cGroup << " Source to PRBS7 " << RESET;
        else if(pSource == 4 || pSource == 5)
            LOG(DEBUG) << BOLDBLUE << "Configuring Rx Group " << +cGroup << " Source to Constant Pattern" << RESET;
        std::string cRxSourceReg;
        if(cGroup == 0 || cGroup == 1)
            cRxSourceReg = "ULDataSource1";
        else if(cGroup == 2 || cGroup == 3)
            cRxSourceReg = "ULDataSource2";
        else if(cGroup == 4 || cGroup == 5)
            cRxSourceReg = "ULDataSource3";
        else if(cGroup == 6)
            cRxSourceReg = "ULDataSource4";

        uint8_t cValueRxSource = ReadChipReg(pChip, cRxSourceReg);
        WriteChipReg(pChip, cRxSourceReg, (cValueRxSource & ~(0x7 << 3 * (cGroup % 2))) | (pSource << 3 * (cGroup % 2)));
    }
}

void D19clpGBTInterface::ConfigureTxSource(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, uint8_t pSource)
{
    // Configure Tx Data Source
    for(const auto& cGroup: pGroups)
    {
        if(pSource == 0)
            LOG(INFO) << BOLDBLUE << "Configuring Rx Group " << +cGroup << " Source to NORMAL " << RESET;
        else if(pSource == 1)
            LOG(INFO) << BOLDBLUE << "Configuring Rx Group " << +cGroup << " Source to PRBS7 " << RESET;
        else if(pSource == 2)
            LOG(INFO) << BOLDBLUE << "Configuring Rx Group " << +cGroup << " Source to Binary counter " << RESET;
        else if(pSource == 3)
            LOG(INFO) << BOLDBLUE << "Configuring Rx Group " << +cGroup << " Source to Constant Pattern" << RESET;
        uint8_t cULDataSrcValue = ReadChipReg(pChip, "ULDataSource5");
        cULDataSrcValue         = (cULDataSrcValue & ~(0x3 << (2 * cGroup))) | (pSource << (2 * cGroup));
        WriteChipReg(pChip, "ULDataSource5", cULDataSrcValue);
    }
}

void D19clpGBTInterface::ConfigureRxPhase(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel, uint8_t pPhase)
{
    // Configure Rx Channel Phase
    std::string cRegName      = "EPRX" + std::to_string(pGroup) + std::to_string(pChannel) + "ChnCntr";
    uint8_t     cValueChnCntr = ReadChipReg(pChip, cRegName);
    cValueChnCntr             = (cValueChnCntr & ~(0xF << 4)) | (pPhase << 4);
    WriteChipReg(pChip, cRegName, cValueChnCntr);
}

void D19clpGBTInterface::ConfigurePhShifter(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pClocks, uint8_t pFreq, uint16_t pDelay, uint8_t pDriveStr, uint8_t pEnFTune)
{
    LOG(INFO) << BOLDMAGENTA << "Configuring Phase Shifter" << RESET;
    // Configure Rx Phase Shifter
    for(const auto& cClock: pClocks)
    {
        std::string cDelayReg  = "PS" + std::to_string(cClock) + "Delay";
        std::string cConfigReg = "PS" + std::to_string(cClock) + "Config";
        WriteChipReg(pChip, cConfigReg, (((pDelay & 0x100) >> 8) << 7) | pEnFTune << 6 | pDriveStr << 3 | pFreq);
        WriteChipReg(pChip, cDelayReg, pDelay);
    }
}

/*----------------------------------*/
/* lpGBT specific routine functions */
/*----------------------------------*/

void D19clpGBTInterface::PhaseTrainRx(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, bool pTrain)
{
    LOG(INFO) << BOLDMAGENTA << "Starting phase training of Rx Groups" << RESET;
    // Launch Rx Phase Training
    for(const auto& cGroup: pGroups)
    {
        std::string cTrainRxReg;
        if(cGroup == 0 || cGroup == 1)
            cTrainRxReg = "EPRXTrain10";
        else if(cGroup == 2 || cGroup == 3)
            cTrainRxReg = "EPRXTrain32";
        else if(cGroup == 4 || cGroup == 5)
            cTrainRxReg = "EPRXTrain32";
        else if(cGroup == 6)
            cTrainRxReg = "EPRXTrain32";

        // Starting or Stopping training
        if(pTrain)
            WriteChipReg(pChip, cTrainRxReg, 0x0F << 4 * (cGroup % 2));
        else
            WriteChipReg(pChip, cTrainRxReg, 0x00 << 4 * (cGroup % 2));
    }
}


void D19clpGBTInterface::ContinuousPhaseAlignRx(Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels)
{
    // Configure Rx Phase Shifter

    D19clpGBTInterface::ConfigureRxGroups(pChip, pGroups, pChannels, 2, 2);
}


void D19clpGBTInterface::PhaseAlignRx(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels)
{
    uint8_t cChipRate = GetChipRate(pChip);
    // Phase Align Rx Channels
    // Set data source for channels 0,2 to PRBS
    ConfigureRxSource(pChip, pGroups, 1);
    // Turn ON PRBS for channels 0,2
    ConfigureRxPRBS(pChip, pGroups, pChannels, true);
    // Find Phase
    // Configure Rx Phase Shifter
    uint16_t cDelay = 0x00;
    uint8_t  cFreq  = (cChipRate == 5) ? 4 : 5; // 4 --> 320 MHz || 5 --> 640 MHz
    ConfigurePhShifter(pChip, {0, 1, 2, 3}, cFreq, cDelay);
    // Phase Train channels 0,2
    PhaseTrainRx(pChip, pGroups, true);
    for(const auto& cGroup: pGroups)
    {
        // Wait until channels lock
        LOG(INFO) << BOLDMAGENTA << "Phase Aligning Rx Group " << +cGroup << RESET;
        do
        {
            LOG(DEBUG) << "Locking state Group " << +cGroup << " 0b" << std::bitset<8>(ReadChipReg(pChip, "EPRX" + std::to_string(cGroup) + "Locked")) << RESET;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while(!IsRxLocked(pChip, cGroup, pChannels));
        LOG(INFO) << BOLDBLUE << "    Group " << +cGroup << BOLDGREEN << " LOCKED" << RESET;
        // Set new phase to channels 0,2
        for(const auto cChannel: pChannels)
        {
            uint8_t cCurrPhase = GetRxPhase(pChip, cGroup, cChannel);
            LOG(INFO) << BOLDBLUE << "    Channel " << +cChannel << " phase is " << +cCurrPhase << RESET;
            ConfigureRxPhase(pChip, cGroup, cChannel, cCurrPhase);
        }
    }
    PhaseTrainRx(pChip, pGroups, false);
    // Set back Rx groups to Fixed Phase tracking mode
    ConfigureRxGroups(pChip, pGroups, pChannels, 2, 0);
    // Turn off PRBS for channels 0,2
    ConfigureRxPRBS(pChip, pGroups, pChannels, false);
    // Set back Rx source to Normal data
    ConfigureRxSource(pChip, pGroups, 0);
}

/*------------------------*/
/* lpGBT status functions */
/*------------------------*/

void D19clpGBTInterface::PrintChipMode(Ph2_HwDescription::Chip* pChip)
{
    switch((ReadChipReg(pChip, "ConfigPins") & 0xF0) >> 4)
    {
    case 0: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 5Gbps __ TxEncoding : FEC5 __ lpGBT Mode : Off" << RESET; break;
    case 1: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 5Gbps __ TxEncoding : FEC5 __ lpGBT Mode : Simplex TX" << RESET; break;
    case 2: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 5Gbps __ TxEncoding : FEC5 __ lpGBT Mode : Simplex RX" << RESET; break;
    case 3: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 5Gbps __ TxEncoding : FEC5 __ lpGBT Mode : Transceiver" << RESET; break;
    case 4: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 5Gbps __ TxEncoding : FEC12 __ lpGBT Mode : Off" << RESET; break;
    case 5: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 5Gbps __ TxEncoding : FEC12 __ lpGBT Mode : Simplex TX" << RESET; break;
    case 6: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 5Gbps __ TxEncoding : FEC12 __ lpGBT Mode : Simplex RX" << RESET; break;
    case 7: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 5Gbps __ TxEncoding : FEC12 __ lpGBT Mode : Transceiver" << RESET; break;
    case 8: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 10Gbps __ TxEncoding : FEC5 __ lpGBT Mode : Off" << RESET; break;
    case 9: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 10Gbps __ TxEncoding : FEC5 __ lpGBT Mode : Simplex TX" << RESET; break;
    case 10: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 10Gbps __ TxEncoding : FEC5 __ lpGBT Mode : Simplex RX" << RESET; break;
    case 11: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 10Gbps __ TxEncoding : FEC5 __ lpGBT Mode : Transceiver" << RESET; break;
    case 12: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 10Gbps __ TxEncoding : FEC12 __ lpGBT Mode : Off" << RESET; break;
    case 13: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 10Gbps __ TxEncoding : FEC12 __ lpGBT Mode : Simplex TX" << RESET; break;
    case 14: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 10Gbps __ TxEncoding : FEC12 __ lpGBT Mode : Simplex RX" << RESET; break;
    case 15: LOG(INFO) << BOLDGREEN << "lpGBT CHIP INFO __ Tx Data Rate : 10Gbps __ TxEncoding : FEC12 __ lpGBT Mode : Transceiver" << RESET; break;
    }
}

uint8_t D19clpGBTInterface::GetChipRate(Ph2_HwDescription::Chip* pChip)
{
    if(((ReadChipReg(pChip, "ConfigPins") & 0xF0) >> 4) >= 8)
        return 10;
    else
        return 5;
}

uint8_t D19clpGBTInterface::GetPUSMStatus(Ph2_HwDescription::Chip* pChip)
{
    uint8_t cPUSMStatus = ReadChipReg(pChip, "PUSMStatus");
    LOG(INFO) << BOLDBLUE << "lpGBT PUSM Status : " << ((cPUSMStatus == 18) ? BOLDGREEN : BOLDRED) << fPUSMStatusMap[cPUSMStatus] << RESET;
    return cPUSMStatus;
}

bool D19clpGBTInterface::IsPUSMDone(Ph2_HwDescription::Chip* pChip) { return GetPUSMStatus(pChip) == 18; }

uint8_t D19clpGBTInterface::GetRxPhase(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel)
{
    // Get Rx Channel Phase
    std::string cRxPhaseReg;
    if(pChannel == 0 || pChannel == 1)
        cRxPhaseReg = "EPRX" + std::to_string(pGroup) + "CurrentPhase10";
    else if(pChannel == 3 || pChannel == 2)
        cRxPhaseReg = "EPRX" + std::to_string(pGroup) + "CurrentPhase32";
    uint8_t cRxPhaseRegValue = ReadChipReg(pChip, cRxPhaseReg);
    return ((cRxPhaseRegValue & (0x0F << 4 * (pChannel % 2))) >> 4 * (pChannel % 2));
}

bool D19clpGBTInterface::IsRxLocked(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, const std::vector<uint8_t>& pChannels)
{
    // Check Rx channels lock status
    std::string cRXLockedReg = "EPRX" + std::to_string(pGroup) + "Locked";
    uint8_t     cChannelMask = 0x00;
    for(auto cChannel: pChannels) cChannelMask += (1 << cChannel);
    return (((ReadChipReg(pChip, cRXLockedReg) & (cChannelMask << 4)) >> 4) == cChannelMask);
}

uint8_t D19clpGBTInterface::GetRxDllStatus(Ph2_HwDescription::Chip* pChip, uint8_t pGroup)
{
    // Gets Rx Group Delay-Locked-Loop status
    std::string cRXDllStatReg = "EPRX" + std::to_string(pGroup) + "DllStatus";
    return ReadChipReg(pChip, cRXDllStatReg);
}

uint8_t D19clpGBTInterface::GetI2CStatus(Ph2_HwDescription::Chip* pChip, uint8_t pMaster)
{
    // Gets I2C Master status
    std::string cI2CStatReg = "I2CM" + std::to_string(pMaster) + "Status";
    uint8_t     cStatus     = ReadChipReg(pChip, cI2CStatReg);
    LOG(INFO) << BOLDBLUE << "I2C Master " << +pMaster << " -- Status : " << fI2CStatusMap[cStatus] << RESET;
    LOG(INFO) << BOLDBLUE << "I2C Master " << +pMaster << " -- Status : " << +cStatus << RESET;
    return cStatus;
}

/*----------------------------*/
/* lpGBT I2C Master functions */
/*----------------------------*/

void D19clpGBTInterface::ResetI2C(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pMasters)
{
    LOG(INFO) << BOLDMAGENTA << "Reseting I2C Masters" << RESET;
    std::vector<uint8_t> cBitPosition = {2, 1, 0};
    uint8_t              cResetMask   = 0;
    for(const auto& cMaster: pMasters)
    {
        cResetMask |= (1 << cBitPosition[cMaster]);
        // generating reset pulse on dedicated register bit
    }
    WriteChipReg(pChip, "RST0", 0, true);
    ReadChipReg(pChip, "RST0");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    LOG(INFO) << BOLDRED << "reset mask " << +cResetMask << RESET;
    WriteChipReg(pChip, "RST0", cResetMask, true);
    ReadChipReg(pChip, "RST0");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    WriteChipReg(pChip, "RST0", 0, true);
    ReadChipReg(pChip, "RST0");
}

void D19clpGBTInterface::ConfigureI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMaster, uint8_t pFreq, uint8_t pNBytes, uint8_t pSCLDriveMode)
{
    // Configures I2C Masters
    // First let's write configuration data into the I2C Master Data register
    std::string cI2CCntrlReg = "I2CM" + std::to_string(pMaster) + "Data0";
    uint8_t     cValueCntrl  = (pFreq << 0) | (pNBytes << 2) | (pSCLDriveMode << 7);
    WriteChipReg(pChip, cI2CCntrlReg, cValueCntrl);

    // Now let's write Command (0x00) to the Command register to tranfer Configuration to the I2C Master Control register
    std::string cI2CCmdReg = "I2CM" + std::to_string(pMaster) + "Cmd";
    WriteChipReg(pChip, cI2CCmdReg, 0x00);
}

bool D19clpGBTInterface::WriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMaster, uint8_t pSlaveAddress, uint32_t pData, uint8_t pNBytes)
{
    // Write Data to Slave Address using I2C Master
    uint8_t cFreq = 3; // 1 MHz
    if(pMaster == 1) { cFreq = 2; }
    ConfigureI2C(pChip, pMaster, cFreq, (pNBytes > 1) ? pNBytes : 0, 0);

    // Prepare Address Register
    // Write Slave Address
    std::string cI2CAddressReg = "I2CM" + std::to_string(pMaster) + "Address";
    WriteChipReg(pChip, cI2CAddressReg, pSlaveAddress);

    // Write Data to Data Register
    for(uint8_t cByte = 0; cByte < 4; cByte++)
    {
        std::string cI2CDataReg = "I2CM" + std::to_string(pMaster) + "Data" + std::to_string(cByte);
        if(cByte < pNBytes)
            WriteChipReg(pChip, cI2CDataReg, (pData & (0xFF << 8 * cByte)) >> 8 * cByte);
        else
            WriteChipReg(pChip, cI2CDataReg, 0x00);
    }

    // Prepare Command Register
    std::string cI2CCmdReg = "I2CM" + std::to_string(pMaster) + "Cmd";
    // If Multi-Byte, write command to save data locally before transfer to slave
    // FIXME for now this only provides a maximum of 32 bits (4 Bytes) write
    // Write Command to launch I2C transaction
    if(pNBytes == 1)
        WriteChipReg(pChip, cI2CCmdReg, 0x2);
    else
    {
        WriteChipReg(pChip, cI2CCmdReg, 0x8);
        WriteChipReg(pChip, cI2CCmdReg, 0xC);
    }
    // wait until the transaction is done
    uint8_t cMaxIter = 10, cIter = 0;
    do
    {
        LOG(DEBUG) << BOLDBLUE << "Waiting for I2C Write transaction to finisih" << RESET;
        cIter++;
    } while(cIter < cMaxIter && !IsI2CSuccess(pChip, pMaster));
    if(cIter == cMaxIter)
    {
        LOG(INFO) << BOLDRED << "I2C Write Transaction FAILED" << RESET;
        // throw std::runtime_error(std::string("in D19clpGBTInterface::WriteI2C : I2C Transaction failed"));
        return false;
    }
    return true;
}

uint32_t D19clpGBTInterface::ReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMaster, uint8_t pSlaveAddress, uint8_t pNBytes)
{
    // Read Data from Slave Address using I2C Master
    uint8_t cFreq = 3; // 1 MHz
    if(pMaster == 1) { cFreq = 2; }
    ConfigureI2C(pChip, pMaster, cFreq, pNBytes, 0);
    // Prepare Address Register
    std::string cI2CAddressReg = "I2CM" + std::to_string(pMaster) + "Address";
    // Write Slave Address
    WriteChipReg(pChip, cI2CAddressReg, pSlaveAddress);

    // Prepare Command Register
    std::string cI2CCmdReg = "I2CM" + std::to_string(pMaster) + "Cmd";
    // Write Read Command and then Read from Read Data Register
    // Procedure and registers depend on number on Bytes

    if(pNBytes == 1) { WriteChipReg(pChip, cI2CCmdReg, 0x3); }
    else
    {
        WriteChipReg(pChip, cI2CCmdReg, 0xD);
    }
    // wait until the transaction is done
    uint8_t cMaxIter = 100, cIter = 0;
    do
    {
        LOG(DEBUG) << BOLDBLUE << "Waiting for I2C Read transaction to finisih" << RESET;
        cIter++;
    } while(cIter < cMaxIter && !IsI2CSuccess(pChip, pMaster));
    if(cIter == cMaxIter)
    {
        LOG(INFO) << BOLDRED << "I2C Read Transaction FAILED" << RESET;
        // throw std::runtime_error(std::string("in D19clpGBTInterface::ReadI2C : I2C Transaction failed"));
    }
    // return read back value
    if(pNBytes == 1)
    {
        std::string cI2CDataReg = "I2CM" + std::to_string(pMaster) + "ReadByte";
        return ReadChipReg(pChip, cI2CDataReg);
    }
    else
    {
        uint32_t cReadData = 0;
        for(uint8_t cByte = 0; cByte < pNBytes; cByte++)
        {
            std::string cI2CDataReg = "I2CM" + std::to_string(pMaster) + "Read" + std::to_string(15 - cByte);
            cReadData |= ((uint32_t)ReadChipReg(pChip, cI2CDataReg) << cByte);
        }
        return cReadData;
    }
}

bool D19clpGBTInterface::IsI2CSuccess(Ph2_HwDescription::Chip* pChip, uint8_t pMaster) { return (GetI2CStatus(pChip, pMaster) == 4); }

/*-------------------------*/
/* lpGBT ADC-DAC functions */
/*-------------------------*/

void D19clpGBTInterface::ConfigureADC(Ph2_HwDescription::Chip* pChip, uint8_t pGainSelect, bool pADCEnable, bool pStartConversion)
{
    WriteChipReg(pChip, "ADCConfig", pStartConversion << 7 | pADCEnable << 2 | pGainSelect);
}

void D19clpGBTInterface::ConfigureCurrentDAC(Ph2_HwDescription::Chip* pChip, const std::vector<std::string>& pCurrentDACChannels, uint8_t pCurrentDACOutput)
{
    // Enables current DAC without changing the voltage DAC
    uint8_t cDACConfigH = ReadChipReg(pChip, "DACConfigH");
    WriteChipReg(pChip, "DACConfigH", cDACConfigH | 0x40);
    // Sets output current for the current DAC. Current = CURDACSelect * ~3.5 uA.
    WriteChipReg(pChip, "CURDACValue", pCurrentDACOutput);
    // Setting Nth bit in this register attaches current DAC to ADCN pin. Current source can be attached to any number of channels
    uint8_t cCURDACCHN = 0;
    uint8_t cADCInput;
    for(auto cCurrentDACChannel: pCurrentDACChannels)
    {
        cADCInput = fADCInputMap[cCurrentDACChannel];
        cCURDACCHN += 1 << cADCInput;
        WriteChipReg(pChip, "CURDACCHN", cCURDACCHN);
    }
}

uint16_t D19clpGBTInterface::ReadADC(Ph2_HwDescription::Chip* pChip, const std::string& pADCInputP, const std::string& pADCInputN, uint8_t pGain)
{
    // Read differential (converted) data on two ADC inputs
    uint8_t cADCInputP = fADCInputMap[pADCInputP];
    uint8_t cADCInputN = fADCInputMap[pADCInputN];
    LOG(INFO) << BOLDBLUE << "Reading ADC value from " << pADCInputP << RESET;
    // Select ADC Input
    WriteChipReg(pChip, "ADCSelect", cADCInputP << 4 | cADCInputN << 0);
    // Enable ADC Input without starting conversion
    ConfigureADC(pChip, pGain, true, false);
    // Enable Internal VREF
    WriteChipReg(pChip, "VREFCNTR", 1 << 7);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // Start ADC conversion
    ConfigureADC(pChip, pGain, true, true);
    // Check conversion status
    uint8_t cMaxIter = 100, cIter = 0;
    bool    cSuccess = false;
    do
    {
        LOG(DEBUG) << BOLDBLUE << "Waiting for ADC conversion to end" << RESET;
        cSuccess = IsReadADCDone(pChip);
        cIter++;
    } while(cIter < cMaxIter && !cSuccess);
    if(cIter == cMaxIter) throw std::runtime_error(std::string("ADC conversion timed out"));
    // Read ADC value
    uint8_t cADCvalue1 = ReadChipReg(pChip, "ADCStatusH") & 0x3;
    uint8_t cADCvalue2 = ReadChipReg(pChip, "ADCStatusL");
    // Clear ADC conversion bit and disable ADC
    ConfigureADC(pChip, pGain, false, false);
    return (cADCvalue1 << 8 | cADCvalue2);
}

bool D19clpGBTInterface::IsReadADCDone(Ph2_HwDescription::Chip* pChip) { return (((ReadChipReg(pChip, "ADCStatusH") & 0x40) >> 6) == 1); }

/*------------------------------*/
/* General Purpose Input Output */
/*------------------------------*/

void D19clpGBTInterface::ConfigureGPIODirection(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pDir)
{
    uint8_t cDirH = ReadChipReg(pChip, "PIODirH");
    uint8_t cDirL = ReadChipReg(pChip, "PIODirL");
    for(auto cGPIO: pGPIOs)
    {
        if(cGPIO < 8)
            cDirL = (cDirL & ~(1 << cGPIO)) | (pDir << cGPIO);
        else
            cDirH = (cDirH & ~(1 << (cGPIO - 8))) | (pDir << (cGPIO - 8));
    }
    WriteChipReg(pChip, "PIODirH", cDirH);
    WriteChipReg(pChip, "PIODirL", cDirL);
}

void D19clpGBTInterface::ConfigureGPIOLevel(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pOut)
{
    uint8_t cOutH = ReadChipReg(pChip, "PIOOutH");
    uint8_t cOutL = ReadChipReg(pChip, "PIOOutL");
    for(auto cGPIO: pGPIOs)
    {
        if(cGPIO < 8)
            cOutL = (cOutL & ~(1 << cGPIO)) | (pOut << cGPIO);
        else
            cOutH = (cOutH & ~(1 << (cGPIO - 8))) | (pOut << (cGPIO - 8));
    }
    WriteChipReg(pChip, "PIOOutH", cOutH);
    WriteChipReg(pChip, "PIOOutL", cOutL);
}

void D19clpGBTInterface::ConfigureGPIODriverStrength(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pDriveStr)
{
    uint8_t cDriveStrH = ReadChipReg(pChip, "PIODriveStrengthH");
    uint8_t cDriveStrL = ReadChipReg(pChip, "PIODriveStrengthL");
    for(auto cGPIO: pGPIOs)
    {
        if(cGPIO < 8)
            cDriveStrL = (cDriveStrL & ~(1 << cGPIO)) | (pDriveStr << cGPIO);
        else
            cDriveStrH = (cDriveStrH & ~(1 << (cGPIO - 8))) | (pDriveStr << (cGPIO - 8));
    }
    WriteChipReg(pChip, "PIODriveStrengthH", cDriveStrH);
    WriteChipReg(pChip, "PIODriveStrengthL", cDriveStrL);
}

void D19clpGBTInterface::ConfigureGPIOPull(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pEnable, uint8_t pUpDown)
{
    uint8_t cPullEnH = ReadChipReg(pChip, "PIOPullEnaH"), cPullEnL = ReadChipReg(pChip, "PIOPullEnaL");
    uint8_t cUpDownH = ReadChipReg(pChip, "PIOUpDownH"), cUpDownL = ReadChipReg(pChip, "PIOUpDownL");
    for(auto cGPIO: pGPIOs)
    {
        if(cGPIO < 8)
        {
            cPullEnL = (cPullEnL & ~(1 << cGPIO)) | (pEnable << cGPIO);
            cUpDownL = (cUpDownL & ~(1 << cGPIO)) | (pUpDown << cGPIO);
        }
        else
        {
            cPullEnH = (cPullEnH & ~(1 << (cGPIO - 8))) | (pEnable << (cGPIO - 8));
            cUpDownH = (cUpDownH & ~(1 << (cGPIO - 8))) | (pUpDown << (cGPIO - 8));
        }
    }
    WriteChipReg(pChip, "PIOPullEnaH", cPullEnH);
    WriteChipReg(pChip, "PIOPullEnaL", cPullEnL);
    WriteChipReg(pChip, "PIOUpDownH", cUpDownH);
    WriteChipReg(pChip, "PIOUpDownL", cUpDownL);
}
bool D19clpGBTInterface::ReadGPIO(Ph2_HwDescription::Chip* pChip, const uint8_t& pGPIO)
{
    LOG(INFO) << BOLDBLUE << "Reading GPIO value from " << std::to_string(pGPIO) << RESET;
    uint8_t cPIOInH = ReadChipReg(pChip, "PIOInH");
    uint8_t cPIOInL = ReadChipReg(pChip, "PIOInL");
    return ((cPIOInH << 8 | cPIOInL) >> pGPIO) & 1;
}

/*---------------------------------*/
/* Bit Error Rate Tester functions */
/*---------------------------------*/
void D19clpGBTInterface::ConfigureBERT(Ph2_HwDescription::Chip* pChip, uint8_t pCoarseSource, uint8_t pFineSource, uint8_t pMeasTime, bool pSkipDisable)
{
    WriteChipReg(pChip, "BERTSource", (pCoarseSource << 4) | pFineSource);
    WriteChipReg(pChip, "BERTConfig", (pMeasTime << 4) | (pSkipDisable << 1));
}

void D19clpGBTInterface::StartBERT(Ph2_HwDescription::Chip* pChip, bool pStartBERT)
{
    uint8_t cRegisterValue = ReadChipReg(pChip, "BERTConfig");
    WriteChipReg(pChip, "BERTConfig", (cRegisterValue & ~(0x1 << 0)) | (pStartBERT << 0));
}

void D19clpGBTInterface::ConfigureBERTPattern(Ph2_HwDescription::Chip* pChip, uint32_t pPattern)
{
    LOG(INFO) << BOLDMAGENTA << "Setting BERT Pattern to " << std::bitset<32>(pPattern) << RESET;
    WriteChipReg(pChip, "BERTDataPattern0", (pPattern & (0xFF << 0)) >> 0);
    WriteChipReg(pChip, "BERTDataPattern1", (pPattern & (0xFF << 8)) >> 8);
    WriteChipReg(pChip, "BERTDataPattern2", (pPattern & (0xFF << 16)) >> 16);
    WriteChipReg(pChip, "BERTDataPattern3", (pPattern & (0xFF << 24)) >> 24);
}

uint8_t D19clpGBTInterface::GetBERTStatus(Ph2_HwDescription::Chip* pChip) { return ReadChipReg(pChip, "BERTStatus"); }

bool D19clpGBTInterface::IsBERTDone(Ph2_HwDescription::Chip* pChip) { return (GetBERTStatus(pChip) & 0x1) == 1; }

bool D19clpGBTInterface::IsBERTEmptyData(Ph2_HwDescription::Chip* pChip) { return ((GetBERTStatus(pChip) & (0x1 << 2)) >> 2) == 1; }

uint64_t D19clpGBTInterface::GetBERTErrors(Ph2_HwDescription::Chip* pChip)
{
    LOG(DEBUG) << BOLDMAGENTA << "Retrieving BERT result" << RESET;
    uint64_t cResult0 = ReadChipReg(pChip, "BERTResult0");
    uint64_t cResult1 = ReadChipReg(pChip, "BERTResult1");
    uint64_t cResult2 = ReadChipReg(pChip, "BERTResult2");
    uint64_t cResult3 = ReadChipReg(pChip, "BERTResult3");
    uint64_t cResult4 = ReadChipReg(pChip, "BERTResult4");
    return ((cResult4 << 32) | (cResult3 << 24) | (cResult2 << 16) | (cResult1 << 8) | cResult0);
}

float D19clpGBTInterface::GetBERTResult(Ph2_HwDescription::Chip* pChip)
{
    // make sure BERT is stopped
    StartBERT(pChip, false);
    // start BERT
    StartBERT(pChip, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Wait for BERT to end
    while(!IsBERTDone(pChip))
    {
        LOG(INFO) << BOLDBLUE << "\tBERT still running ... " << RESET;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // Throw error if empty data
    if(IsBERTEmptyData(pChip))
    {
        // stop BERT
        StartBERT(pChip, false);
        LOG(INFO) << BOLDRED << "BERT : All zeros at input ... exiting" << RESET;
        throw std::runtime_error(std::string("BERT : All zeros at input"));
    }
    // Get BERT Error counters
    LOG(DEBUG) << BOLDBLUE << "\t\tReading BERT counter" << RESET;
    uint64_t cErrors = GetBERTErrors(pChip);
    // Compute number of bits checked
    uint8_t  cMeasTime         = (ReadChipReg(pChip, "BERTConfig") & (0xF << 4)) >> 4;
    uint64_t cNClkCycles       = std::pow(2, 5 + cMeasTime * 2);
    uint8_t  cNBitsPerClkCycle = (GetChipRate(pChip) == 5) ? 8 : 16; // 5G(320MHz) == 8 bits/clk, 10G(640MHz) == 16 bits/clk
    uint64_t cBitsChecked      = cNClkCycles * cNBitsPerClkCycle;
    // Stop BERT
    StartBERT(pChip, false);
    LOG(INFO) << BOLDWHITE << "\tBits checked  : " << +cBitsChecked << RESET;
    LOG(INFO) << BOLDWHITE << "\tBits in error : " << +cErrors << RESET;
    // return fraction of errors
    return (float)cErrors / cBitsChecked;
}

/*-------------------------------*/
/* Eye Opening Monitor functions */
/*-------------------------------*/
void D19clpGBTInterface::ConfigureEOM(Ph2_HwDescription::Chip* pChip, uint8_t pEndOfCountSelect, bool pByPassPhaseInterpolator, bool pEnableEOM)
{
    // configure EOM parameters and enable the block
    WriteChipReg(pChip, "EOMConfigH", pEndOfCountSelect << 4 | pByPassPhaseInterpolator << 2 | pEnableEOM << 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5)); // wait for few ms for voltages to stabilize
}

void D19clpGBTInterface::StartEOM(Ph2_HwDescription::Chip* pChip, bool pStartEOM)
{
    // Start/Stop EOM
    uint8_t cRegisterValue = ReadChipReg(pChip, "EOMConfigH");
    WriteChipReg(pChip, "EOMConfigH", (cRegisterValue & ~(0x1 << 1)) | (pStartEOM << 1));
}

void D19clpGBTInterface::SelectEOMPhase(Ph2_HwDescription::Chip* pChip, uint8_t pPhase)
{
    // Select EOM Sampling Phase
    WriteChipReg(pChip, "EOMConfigL", pPhase);
}

void D19clpGBTInterface::SelectEOMVof(Ph2_HwDescription::Chip* pChip, uint8_t pVof)
{
    // Select EOM comparator voltage
    WriteChipReg(pChip, "EOMvofSel", pVof);
}

uint8_t D19clpGBTInterface::GetEOMStatus(Ph2_HwDescription::Chip* pChip)
{
    // Get EOM status
    uint8_t cEOMStatus = ReadChipReg(pChip, "EOMStatus");
    LOG(DEBUG) << BOLDBLUE << "Eye Opening Monitor status : " << fEOMStatusMap[(cEOMStatus & (0x3 << 2)) >> 2] << RESET;
    return cEOMStatus;
}

uint16_t D19clpGBTInterface::GetEOMCounter(Ph2_HwDescription::Chip* pChip)
{
    // Get EOM tick counters
    return (ReadChipReg(pChip, "EOMCounterValueH") << 8 | ReadChipReg(pChip, "EOMCounterValueL") << 0);
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
    fTC_USB = new TC_2SSEH();
    LOG(INFO) << BOLDGREEN << "Initialised 2S-SEH TestCard USB Handler" << RESET;
#endif
}
#endif

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
    PhaseAlignRx(pChip, cRxGroups, cRxChannels);
    // Reset I2C Masters
    ResetI2C(pChip, {0, 1, 2});
    // Setting GPIO levels for Skeleton test
    ConfigureGPIODirection(pChip, {0, 1, 3, 6, 9, 12}, 1);
    ConfigureGPIOLevel(pChip, {0, 1, 3, 6, 9, 12}, 1);
}

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

    ContinuousPhaseAlignRx(pChip, cRxGroups, cRxChannels);
    // Reset I2C Masters
    ResetI2C(pChip, {0, 1, 2});
    // Setting GPIO levels Resets are high
    ConfigureGPIODirection(pChip, {0, 3, 6, 8}, 1);
    ConfigureGPIOLevel(pChip, {0, 3, 6, 8}, 1);
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
    // FIX ME : check the register if its write only or not
    // should be evident from the address
    // bool    cWriteOnlyReg = (pRegisterAddress & 0x7f) == 0x00;
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
