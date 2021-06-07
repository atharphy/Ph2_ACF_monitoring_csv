/*!
  \file                  D19clpGBTIn+erface.cc
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
    std::stringstream cOutput;
    setBoard(pChip->getBeBoardId());
    pChip->printChipType(cOutput);
    LOG(INFO) << BOLDBLUE << cOutput.str() << "...Configuring chip with Id[" << +pChip->getId() << "]" << RESET;
    LOG(INFO) << BOLDMAGENTA << "Configuring lpGBT" << RESET;

    SetConfigMode(pChip, pChip->isOptical(), fUseCPB);
    // configure CPB - do this here rather than in SystemController? Not sure
    CPBconfig cCPBconfig;
    cCPBconfig.fEnable       = fUseCPB;
    cCPBconfig.fI2CFrequency = 3;
    cCPBconfig.fWait_us      = 0;   // TO-DO - make configurable from xml
    cCPBconfig.fReTry        = 1;   // TO-DO - make configurable from xml
    cCPBconfig.fVerbose      = 0;   // TO-DO - make configurable from xml
    cCPBconfig.fMaxAttempts  = 100; // TO-DO - make configurable from xml
    fBoardFW->ConfigureCPB(cCPBconfig);
    // Configure High Speed Link Tx Rx Polarity
    // do this before doing anything else
    ConfigureHighSpeedPolarity(pChip, 1, 0);
    bool cReconfigure = false; // if using I2C interface maybe I want to confiugre?
    if(cReconfigure)           // by de
    {
        ChipRegMap                                    clpGBTRegMap = pChip->getRegMap();
        std::vector<std::pair<std::string, uint16_t>> cRegVec;
        cRegVec.clear();
        for(const auto& cRegItem: clpGBTRegMap)
        {
            if(cRegItem.second.fAddress <= 0x13c && cRegItem.first.find("ChipConfig") == std::string::npos) cRegVec.push_back(std::make_pair(cRegItem.first, cRegItem.second.fValue));
        } // get read/write registers
        for(const auto& cReg: cRegVec)
        {
            LOG(DEBUG) << BOLDBLUE << "\tWriting 0x" << std::hex << +cReg.second << std::dec << " to " << cReg.first << RESET;
            WriteChipReg(pChip, cReg.first, cReg.second);
        }
        SetPUSMDone(pChip, true, true);
    }
    uint16_t cIter = 0, cMaxIter = 200;
    bool     cReady = false;
    while(!cReady && cIter < cMaxIter)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cReady = IsPUSMDone(pChip);
        cIter++;
    }
    if(cReady) LOG(INFO) << BOLDGREEN << "lpGBT Configured [READY]" << RESET;
    if(!cReady) throw std::runtime_error(std::string("lpGBT Power-Up State Machine NOT DONE"));
    // PrintChipMode(pChip);
    return cReady;
} //

/*-----------------------*/
/* OT specific functions */
/*-----------------------*/

void D19clpGBTInterface::SetConfigMode(Ph2_HwDescription::Chip* pChip, bool pUseOpticalLink, bool pUseCPB, bool pToggleTC)
{
    if(pUseOpticalLink)
    {
        LOG(INFO) << BOLDGREEN << "Using Serial Interface configuration mode" << RESET;
#ifdef __ROH_USB__
        LOG(INFO) << BOLDBLUE << "Toggling Test Card" << RESET;
        if(pToggleTC) fExternalInterface.getInterface().toggle_SCI2C();
#endif
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

// Preliminary
void D19clpGBTInterface::Configure2SSEH(Ph2_HwDescription::Chip* pChip)
{
    uint8_t cChipRate = GetChipRate(pChip);
    LOG(INFO) << BOLDGREEN << "Applying 2S-SEH lpGBT configuration for " << +cChipRate << "G module." << RESET;

    // Clocks - by default all are off
    std::vector<uint8_t> cClocks  = {fClock_RHS_Hybrid, fClock_LHS_Hybrid}; // Reduced number of clocks and only 320 MHz
    uint8_t              cClkFreq = 0, cClkDriveStr = 7, cClkInvert = 1;
    uint8_t              cClkPreEmphWidth = 0, cClkPreEmphMode = 0, cClkPreEmphStr = 0;
    ConfigureClocks(pChip, cClocks, cClkFreq, cClkDriveStr, cClkInvert, cClkPreEmphWidth, cClkPreEmphMode, cClkPreEmphStr);
    // Tx Groups and Channels
    std::vector<uint8_t> cTxGroups = {0, 2}, cTxChannels = {0};
    uint8_t              cTxDataRate = 3, cTxDriveStr = 7, cTxPreEmphMode = 1, cTxPreEmphStr = 4, cTxPreEmphWidth = 0, cTxInvert = 0;
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
    uint8_t              cRxDataRate = 2, cRxTrackMode = 0; // manual mode by default
    ConfigureRxGroups(pChip, cRxGroups, cRxChannels, cRxDataRate, cRxTrackMode);
    // Configure Rx Channels
    uint8_t cRxEqual = 1, cRxTerm = 1, cRxAcBias = 1, cRxInvert = 0, cRxPhase = 12;
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
    // Reset I2C Masters
    ResetI2C(pChip, {0, 1, 2});
    // Setting GPIO levels Uncomment this for Skeleton test
    // Setting GPIO levels for Skeleton test
    ConfigureGPIODirection(pChip, {fReset_LHS_CIC, fReset_LHS_CBC, fReset_RHS_CIC, fReset_RHS_CBC}, 1);
    ConfigureGPIOLevel(pChip, {fReset_LHS_CIC, fReset_LHS_CBC, fReset_RHS_CIC, fReset_RHS_CBC}, 1);
    // hold resets
    for(uint8_t cSide = 0; cSide < 2; cSide++)
    {
        this->cbcReset(pChip, true, cSide);
        this->cicReset(pChip, true, cSide);
    }
}

void D19clpGBTInterface::ConfigurePSROH(Ph2_HwDescription::Chip* pChip)
{
    uint8_t cChipRate = GetChipRate(pChip);
    LOG(INFO) << BOLDGREEN << "Applying PS-ROH-" << +cChipRate << "G lpGBT configuration" << RESET;
    // Clocks
    std::vector<uint8_t> cClocks  = {1, 6, 11, 26};
    uint8_t              cClkFreq = (cChipRate == 5) ? 4 : 5, cClkDriveStr = 7, cClkInvert = 1;
    uint8_t              cClkPreEmphWidth = 0, cClkPreEmphMode = 0, cClkPreEmphStr = 0;
    cClkFreq = 0;
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
    uint8_t              cRxDataRate = 2, cRxTrackMode = 0;
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
    ConfigureGPIODirection(pChip, {fReset_LHS_CIC, fReset_LHS_MPA, fReset_LHS_SSA, fReset_RHS_CIC, fReset_RHS_MPA, fReset_RHS_SSA}, 1);
    ConfigureGPIOLevel(pChip, {fReset_LHS_CIC, fReset_LHS_MPA, fReset_LHS_SSA, fReset_RHS_CIC, fReset_RHS_MPA, fReset_RHS_SSA}, 1);
    // hold resets
    for(uint8_t cSide = 0; cSide < 2; cSide++)
    {
        this->ssaReset(pChip, true, cSide);
        this->mpaReset(pChip, true, cSide);
        this->cicReset(pChip, true, cSide);
    }
    LOG(INFO) << BOLDGREEN << "PS-ROH-" << +cChipRate << "G lpGBT configuration APPLIED" << RESET;
}

// void D19clpGBTInterface::ConfigurePSROH(Ph2_HwDescription::Chip* pChip)
// {
//     uint8_t cChipRate = GetChipRate(pChip);
//     LOG(INFO) << BOLDGREEN << "Applying PS-ROH-" << +cChipRate << "G lpGBT configuration" << RESET;
//     // Configure High Speed Link Tx Rx Polarity
//     ConfigureHighSpeedPolarity(pChip, 1, 0);
//     // Clocks

//     //std::vector<uint8_t> cClocks  = {fClock_RHS_Hybrid , fClock_LHS_Hybrid , fClock_RHS_CIC, fClock_LHS_CIC };
//     uint8_t              cClkFreq = (cChipRate == 5) ? 4 : 5, cClkDriveStr = 7, cClkInvert = 1;
//     uint8_t              cClkPreEmphWidth = 0, cClkPreEmphMode = 0, cClkPreEmphStr = 0;
//     // disable all clocks
//     // by setting frequency to 0
//     // cClkFreq = 0 ;
//     // and by setting drive strength to 0
//     // cClkDriveStr = 0 ;
//     // CIC
//     ConfigureClocks(pChip, {fClock_RHS_CIC, fClock_LHS_CIC }, cClkFreq, cClkDriveStr, cClkInvert, cClkPreEmphWidth, cClkPreEmphMode, cClkPreEmphStr);
//     // SSA
//     ConfigureClocks(pChip, {fClock_RHS_Hybrid , fClock_LHS_Hybrid }, cClkFreq, cClkDriveStr, cClkInvert, cClkPreEmphWidth, cClkPreEmphMode, cClkPreEmphStr);

//     // Tx Groups and Channels
//     std::vector<uint8_t> cTxGroups = {0, 1, 2, 3}, cTxChannels = {0};
//     uint8_t              cTxDataRate = 3, cTxDriveStr = 4, cTxPreEmphMode = 0, cTxPreEmphStr = 4, cTxPreEmphWidth = 0, cTxInvert = 0;
//     ConfigureTxGroups(pChip, cTxGroups, cTxChannels, cTxDataRate);
//     // uint8_t              cTxDataRate = 3, cTxDriveStr = 7, cTxPreEmphMode = 1, cTxPreEmphStr = 4, cTxPreEmphWidth = 0, cTxInvert = 0;
//     // ConfigureTxGroups(pChip, cTxGroups, cTxChannels, cTxDataRate);
//     for(const auto& cGroup: cTxGroups)
//     {
//         cTxInvert = (cGroup % 2 == 0) ? 1 : 0;
//         for(const auto& cChannel: cTxChannels) ConfigureTxChannels(pChip, {cGroup}, {cChannel}, cTxDriveStr, cTxPreEmphMode, cTxPreEmphStr, cTxPreEmphWidth, cTxInvert);
//     }
//     // Rx configuration and Phase Align
//     // Configure Rx Groups
//     std::vector<uint8_t> cRxGroups = {0, 1, 2, 3, 4, 5, 6}, cRxChannels = {0, 2};
//     uint8_t              cRxDataRate = 2, cRxTrackMode = 1;
//     ConfigureRxGroups(pChip, cRxGroups, cRxChannels, cRxDataRate, cRxTrackMode);
//     // Configure Rx Channels
//     uint8_t cRxEqual = 0, cRxTerm = 1, cRxAcBias = 0, cRxInvert = 0, cRxPhase = 13;
//     // 320
//     //uint8_t cRxEqual = 1, cRxTerm = 1, cRxAcBias = 1, cRxInvert = 0, cRxPhase = 10;
//     for(const auto& cGroup: cRxGroups)
//     {
//         for(const auto cChannel: cRxChannels)
//         {
//             // Right Hybrid
//             if(cGroup == 0 && cChannel == 0)
//                 cRxInvert = 1;
//             else if(cGroup == 4 || cGroup == 5 || cGroup == 6)
//                 cRxInvert = 0;
//             // Left Hybrid
//             else if(cGroup == 1 && cChannel == 0)
//                 cRxInvert = 1;
//             else if(cGroup == 3 && cChannel == 2)
//                 cRxInvert = 1;
//             else if(cGroup == 2)
//                 cRxInvert = 0;
//             ConfigureRxChannels(pChip, {cGroup}, {cChannel}, cRxEqual, cRxTerm, cRxAcBias, cRxInvert, cRxPhase);
//         }
//     }
//     // configure phase shifter
//     uint16_t cDelay = 0x00;
//     uint8_t  cFreq = (cChipRate == 5) ? 4 : 5; // 4 --> 320 MHz || 5 --> 640 MHz
//     ConfigurePhShifter(pChip, {0, 1, 2, 3}, cFreq, cDelay);
//     //PhaseAlignRx(pChip, cRxGroups, cRxChannels);
//     cRxTrackMode=0;
//     ConfigureRxGroups(pChip, cRxGroups, cRxChannels, cRxDataRate, cRxTrackMode);
//     // Turn off PRBS for channels 0,2
//     ConfigureRxPRBS(pChip, cRxGroups, cRxChannels, false);
//     // Set back Rx source to Normal data
//     ConfigureRxSource(pChip, cRxGroups, 0);

//     // Reset I2C Masters
//     ResetI2C(pChip, {0, 1, 2});
//     // Setting GPIO levels for Skeleton test
//     std::vector<uint8_t> cResetPinsRHS{fReset_RHS_SSA, fReset_RHS_MPA, fReset_RHS_CIC};
//     std::vector<uint8_t> cResetPinsLHS{fReset_LHS_SSA, fReset_LHS_MPA, fReset_LHS_CIC};
//     //
//     ConfigureGPIODirection(pChip, cResetPinsRHS, 1);
//     //ConfigureGPIODriverStrength(pChip, cResetPinsRHS , 0x1);
//     //
//     ConfigureGPIODirection(pChip, cResetPinsLHS, 1);
//     //ConfigureGPIODriverStrength(pChip, cResetPinsLHS , 0x1);

//     // // test GPIO
//     // {
//     //     uint8_t cEnable=1;
//     //     uint8_t cUpDown=1;
//     //     ConfigureGPIODirection(pChip, cResetPinsRHS, 0);
//     //     ConfigureGPIOPull(pChip, cResetPinsRHS, cEnable, cUpDown);

//     //     ConfigureGPIODirection(pChip, cResetPinsLHS, 0);
//     //     ConfigureGPIOPull(pChip, cResetPinsLHS, cEnable, cUpDown);

//     //     uint8_t cReadIOstateH = this->ReadChipReg(pChip,"PIOInH");
//     //     uint8_t cReadIOstateL = this->ReadChipReg(pChip,"PIOInL");
//     //     // check RHS
//     //     for(auto cResetPin : cResetPinsRHS )
//     //     {
//     //         if( cResetPin < 8 )
//     //         {
//     //             uint8_t cVal = (cReadIOstateL & (0x1 << cResetPin)) >> cResetPin;
//     //             if( cVal == 0 )
//     //             {
//     //                 LOG (INFO) << BOLDGREEN << "RHS RST pin " << +cResetPin << " connected - IO state L 0x"
//     //                     << std::hex << (+cReadIOstateL)  << std::dec
//     //                     << " - " << +cVal << RESET;
//     //             }
//     //             else
//     //             {
//     //                 LOG (INFO) << BOLDRED << "RHS RST pin " << +cResetPin << "  not connected - IO state L 0x"
//     //                     << std::hex << (+cReadIOstateL)  << std::dec
//     //                     << " - " << +cVal << RESET;
//     //             }
//     //         }
//     //         else
//     //         {
//     //             uint8_t cVal = (cReadIOstateH & (0x1 << cResetPin)) >> cResetPin;
//     //             if( cVal == 0 )
//     //             {
//     //                 LOG (INFO) << BOLDGREEN << "RHS RST pin " << +cResetPin << " connected - IO state L 0x"
//     //                     << std::hex << (+cReadIOstateH)  << std::dec
//     //                     << " - " << +cVal << RESET;
//     //             }
//     //             else
//     //             {
//     //                 LOG (INFO) << BOLDRED << "RHS RST pin " << +cResetPin << "  not connected - IO state L 0x"
//     //                     << std::hex << (+cReadIOstateH)  << std::dec
//     //                     << " - " << +cVal << RESET;
//     //             }
//     //         }
//     //     }
//     //     // check LHS
//     //     for(auto cResetPin : cResetPinsLHS )
//     //     {
//     //         if( cResetPin < 8 )
//     //         {
//     //             uint8_t cVal = (cReadIOstateL & (0x1 << cResetPin)) >> cResetPin;
//     //             if( cVal == 0 )
//     //             {
//     //                 LOG (INFO) << BOLDGREEN << "LHS RST pin " << +cResetPin << " - IO state L 0x"
//     //                     << std::hex << (+cReadIOstateL)  << std::dec
//     //                     << " - " << +cVal << RESET;
//     //             }
//     //             else
//     //             {
//     //                 LOG (INFO) << BOLDRED << "LHS RST pin " << +cResetPin << " - IO state L 0x"
//     //                     << std::hex << (+cReadIOstateL)  << std::dec
//     //                     << " - " << +cVal << RESET;
//     //             }
//     //         }
//     //         else
//     //         {
//     //             uint8_t cVal = (cReadIOstateH & (0x1 << cResetPin)) >> cResetPin;
//     //             if( cVal == 0 )
//     //             {
//     //                 LOG (INFO) << BOLDGREEN << "LHS RST pin " << +cResetPin << " - IO state H 0x"
//     //                     << std::hex << (+cReadIOstateH)  << std::dec
//     //                     << " - " << +cVal << RESET;
//     //             }
//     //             else
//     //             {
//     //                 LOG (INFO) << BOLDRED << "LHS RST pin " << +cResetPin << " - IO state H 0x"
//     //                     << std::hex << (+cReadIOstateH)  << std::dec
//     //                     << " - " << +cVal << RESET;
//     //             }
//     //         }
//     //     }

//     //     // // set back
//     //     // cEnable=0;
//     //     // cUpDown=0;
//     //     ConfigureGPIOPull(pChip, cResetPinsRHS, 0, 0);
//     //     ConfigureGPIODirection(pChip, cResetPinsRHS, 1);
//     //     ConfigureGPIOPull(pChip, cResetPinsLHS, 0, 0);
//     //     ConfigureGPIODirection(pChip, cResetPinsLHS, 1);
//     // }

//     // Reset all ASICs at least once
//     // for(size_t cSide=0; cSide<2; cSide++)
//     // {
//     //     resetSSA(pChip, cSide);
//     //     resetMPA(pChip, cSide);
//     //     resetCic(pChip, cSide);
//     // }

//     // Keep resets active after start-up
//     for(size_t cSide=0; cSide<2; cSide++)
//     {
//         ssaReset(pChip, false, cSide);
//         mpaReset(pChip, false, cSide);
//         cicReset(pChip, false, cSide);
//     }
// }

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
