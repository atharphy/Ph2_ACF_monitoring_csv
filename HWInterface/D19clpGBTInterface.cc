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
bool D19clpGBTInterface::ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerify, uint32_t pBlockSize)
{
    std::stringstream cOutput;
    setBoard(pChip->getBeBoardId());
    pChip->printChipType(cOutput);
    uint8_t cChipVersion = static_cast<lpGBT*>(pChip)->getVersion();
    LOG(INFO) << BOLDBLUE << cOutput.str() << "...Configuring chip with Id[" << +pChip->getId() << "] , Version[" << +cChipVersion << "]" << RESET;
    PrintChipMode(pChip);
    // Waiting for at least PauseForDllConfig state before configuring chip. If state beyond, then I can still configure
    uint16_t cIter = 0, cMaxIter = 200;
    for(auto& ele: fPUSMStatusMap[cChipVersion]) revertedPUSMStatusMap[ele.second] = ele.first;
    uint8_t cPUSMState = 0;
    do
    {
        cPUSMState = GetPUSMStatus(pChip);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cIter++;
    } while((cPUSMState < revertedPUSMStatusMap["PAUSE_FOR_DLL_CONFIG"]) && (cIter < cMaxIter));
    if(cIter == cMaxIter) { throw std::runtime_error(std::string("lpGBT Power-Up State Machine Stuck at state" + fPUSMStatusMap[cChipVersion][cPUSMState])); }
    // Configuring chip
    bool cReconfigure = false;
    if(cReconfigure)
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
    }
    // Setting PUSM Done bits
    SetPUSMDone(pChip, true, true);
    // Checking if lpGBT reaches Ready state
    bool cReady = false;
    cIter = 0, cMaxIter = 200;
    while(!cReady && cIter < cMaxIter)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cReady = IsPUSMDone(pChip);
        cIter++;
    }
    if(cReady) { LOG(INFO) << BOLDGREEN << "lpGBT Configured [READY]" << RESET; }
    else
    {
        throw std::runtime_error(std::string("lpGBT Power-Up State Machine NOT DONE"));
    }
    // Reset I2C Masters
    ResetI2C(pChip, {0, 1, 2});
    return cReady;
} //

/*-----------------------*/
/* OT specific functions */
/*-----------------------*/

void D19clpGBTInterface::SetConfigMode(bool pUseOpticalLink, bool pUseCPB, bool pToggleTC)
{
    if(pUseOpticalLink)
    {
        LOG(INFO) << BOLDGREEN << "Using Serial Interface configuration mode" << RESET;
#if defined(__TC_USB__) && defined(__ROH_USB__)
        LOG(INFO) << BOLDBLUE << "Toggling Test Card" << RESET;
        if(pToggleTC && fExternalController != nullptr) fExternalController->getInterface().toggle_SCI2C();
#endif
        fUseOpticalLink = true;
        if(pUseCPB)
        {
            LOG(INFO) << BOLDGREEN << "Using Command Processor Block" << RESET;
            fUseCPB = true;
        }
        else
            LOG(INFO) << BOLDRED << "Not using Command Processor Block" << RESET;
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
    // Forcing driver attenuation to be 1
    uint8_t cEQAttenuation = 3; 
    WriteChipReg(pChip, "EQConfig", cEQAttenuation << 3);
    // Clocks - by default all are off
    std::vector<uint8_t> cClocks  = {fClock_RHS_Hybrid, fClock_LHS_Hybrid}; // Reduced number of clocks and only 320 MHz
    uint8_t              cClkFreq = 0, cClkDriveStr = 7, cClkInvert = 1;
    uint8_t              cClkPreEmphWidth = 0, cClkPreEmphMode = 0, cClkPreEmphStr = 0;
    ConfigureClocks(pChip, cClocks, cClkFreq, cClkDriveStr, cClkInvert, cClkPreEmphWidth, cClkPreEmphMode, cClkPreEmphStr);
    // LOG(INFO) << BOLDGREEN << "Clocks configured" << RESET;
    // getchar();
    // Tx Groups and Channels
    std::vector<uint8_t> cTxGroups = {0, 2}, cTxChannels = {0};
    uint8_t              cTxDataRate = 3, cTxDriveStr = 7, cTxPreEmphMode = 0, cTxPreEmphStr = 0, cTxPreEmphWidth = 0, cTxInvert = 0;
    ConfigureTxGroups(pChip, cTxGroups, cTxChannels, cTxDataRate);
    for(const auto& cGroup: cTxGroups)
    {
        if(cGroup == 0) cTxInvert = 1;
        if(cGroup == 2) cTxInvert = 0;
        for(const auto& cChannel: cTxChannels) ConfigureTxChannels(pChip, {cGroup}, {cChannel}, cTxDriveStr, cTxPreEmphMode, cTxPreEmphStr, cTxPreEmphWidth, cTxInvert);
    }
    // LOG(INFO) << BOLDGREEN << "TX configured" << RESET;
    // getchar();
    // Rx configuration and Phase Align
    // Configure Rx Groups
    // WriteChipReg(pChip, "EPRXDllConfig", , false);
    std::vector<uint8_t> cRxGroups = {0, 1, 2, 3, 4, 5, 6}, cRxChannels = {0, 2};
    uint8_t              cRxDataRate = 2, cRxTrackMode = 0; // manual mode by default
    ConfigureRxGroups(pChip, cRxGroups, cRxChannels, cRxDataRate, cRxTrackMode);
    // Configure Rx Channels
    // module/skeleton
    uint8_t cRxEqual = 0, cRxTerm = 1, cRxAcBias = 0, cRxInvert = 0, cRxPhase = 5;
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
    // LOG(INFO) << BOLDGREEN << "RX configured" << RESET;
    // getchar();
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
#if defined(__TCUSB__)
    // // ContinuousPhaseAlignRx(pChip, cRxGroups, cRxChannels);
    std::vector<uint8_t> cEportGroups = {4, 4, 5, 5, 6, 0};
    std::vector<uint8_t> cEportChnls  = {0, 2, 0, 2, 0, 0};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);
    cEportGroups = {0, 1, 1, 2, 2, 3};
    cEportChnls  = {2, 0, 2, 0, 2, 2};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);
    ConfigureCurrentDAC(pChip, std::vector<std::string>{"ADC4"}, 0x1c); // current chosen according to measurement range
#endif

} // namespace Ph2_HwInterface
void D19clpGBTInterface::ContinuousPhaseAlignRx(Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels)
{
    // Configure Rx Phase Shifter

    D19clpGBTInterface::ConfigureRxGroups(pChip, pGroups, pChannels, 2, 2);
}

void D19clpGBTInterface::InitialPhaseAlignRx(Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels)
{
    std::vector<uint8_t> cOptimalTaps = {};
    AutoPhaseAlignRx(pChip, pGroups, pChannels);
    // find mode
    for(size_t cIndx = 0; cIndx < pGroups.size(); cIndx++) { cOptimalTaps.push_back(GetPhaseTap(pChip, pGroups[cIndx], pChannels[cIndx])); }
    std::vector<uint8_t> cTapsHist(15, 0);
    for(auto cItem: cOptimalTaps) cTapsHist[cItem]++;
    // return cTapsHist;
    auto cTapMode = std::max_element(cTapsHist.begin(), cTapsHist.end()) - cTapsHist.begin();
    LOG(INFO) << BOLDGREEN << "Applying Phase " << cTapMode << RESET;
    if(cTapMode != 15)
        for(size_t cIndx = 0; cIndx < pGroups.size(); cIndx++) { ConfigureRxPhase(pChip, pGroups[cIndx], pChannels[cIndx], cTapMode); }
}

void D19clpGBTInterface::ConfigurePSROH(Ph2_HwDescription::Chip* pChip)
{
    uint8_t cChipRate = GetChipRate(pChip);
    LOG(INFO) << BOLDGREEN << "Applying PS-ROH-" << +cChipRate << "G lpGBT configuration" << RESET;
    // Clocks
    std::vector<uint8_t> cClocks = {fClock_LHS_Hybrid, fClock_LHS_CIC, fClock_RHS_Hybrid, fClock_RHS_CIC};
    // clock frequency set to 0 to disable it at first and only later configure what is needed
    uint8_t cClkFreq = 0, cClkDriveStr = 1, cClkInvert = 1;
    uint8_t cClkPreEmphWidth = 0, cClkPreEmphMode = 0, cClkPreEmphStr = 0;
    ConfigureClocks(pChip, cClocks, cClkFreq, cClkDriveStr, cClkInvert, cClkPreEmphWidth, cClkPreEmphMode, cClkPreEmphStr);
    // Tx Groups and Channels
    std::vector<uint8_t> cTxGroups = {0, 1, 2, 3}, cTxChannels = {0};
    // uint8_t              cTxDataRate = 3, cTxDriveStr = 4  , cTxPreEmphMode = 1, cTxPreEmphStr = 4, cTxPreEmphWidth = 0, cTxInvert = 0;
    uint8_t cTxDataRate = 3, cTxDriveStr = 4, cTxPreEmphMode = 1, cTxPreEmphStr = 4, cTxPreEmphWidth = 0, cTxInvert = 0;
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
    uint8_t cRxEqual = 0, cRxTerm = 1, cRxAcBias = 0, cRxPhase = 9; // cRxInvert = 0 ;
    // uint8_t cRxEqual = 1, cRxTerm = 1, cRxAcBias = 1, cRxInvert = 0, cRxPhase = 10;
    std::vector<uint8_t> cGrpsLeft{0, 1, 1, 2, 2, 3, 3};
    std::vector<uint8_t> cChnlsLeft{2, 0, 2, 0, 2, 0, 2};
    std::vector<uint8_t> cInvrtLeft{1, 1, 0, 1, 1, 1, 1};
    for(size_t cIndx = 0; cIndx < cInvrtLeft.size(); cIndx++)
    {
        uint8_t cGroup    = cGrpsLeft[cIndx];
        uint8_t cChannel  = cChnlsLeft[cIndx];
        uint8_t cRxInvert = cInvrtLeft[cIndx];
        ConfigureRxChannels(pChip, {cGroup}, {cChannel}, cRxEqual, cRxTerm, cRxAcBias, cRxInvert, cRxPhase);
    }
    std::vector<uint8_t> cGrpsRight{4, 4, 5, 5, 6, 6, 0};
    std::vector<uint8_t> cChnlsRight{2, 0, 2, 0, 2, 0, 0};
    std::vector<uint8_t> cInvrtRight{0, 0, 0, 0, 0, 0, 1};
    for(size_t cIndx = 0; cIndx < cInvrtLeft.size(); cIndx++)
    {
        uint8_t cGroup    = cGrpsRight[cIndx];
        uint8_t cChannel  = cChnlsRight[cIndx];
        uint8_t cRxInvert = cInvrtRight[cIndx];
        ConfigureRxChannels(pChip, {cGroup}, {cChannel}, cRxEqual, cRxTerm, cRxAcBias, cRxInvert, cRxPhase);
    }
    // InternalPhaseAlignRx(pChip, cRxGroups, cRxChannels);
    // Reset I2C Masters
    ResetI2C(pChip, {0, 1, 2});
    // Setting GPIO levels for PS ROH
    ConfigureGPIODirection(pChip, {fReset_LHS_CIC, fReset_LHS_MPA, fReset_LHS_SSA, fReset_RHS_CIC, fReset_RHS_MPA, fReset_RHS_SSA}, 1);
    ConfigureGPIOLevel(pChip, {fReset_LHS_CIC, fReset_LHS_MPA, fReset_LHS_SSA, fReset_RHS_CIC, fReset_RHS_MPA, fReset_RHS_SSA}, 1);
    // hold resets
    for(uint8_t cSide = 0; cSide < 2; cSide++)
    {
        this->ssaReset(pChip, true, cSide);
        this->mpaReset(pChip, true, cSide);
        this->cicReset(pChip, true, cSide);
    }
#if defined(__TCUSB__)
    // ContinuousPhaseAlignRx(pChip, cRxGroups, cRxChannels);
    std::vector<uint8_t> cEportGroups = {4, 4, 5, 5, 6, 6, 0};
    std::vector<uint8_t> cEportChnls  = {0, 2, 0, 2, 0, 2, 0};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);
    cEportGroups = {0, 1, 1, 2, 2, 3, 3};
    cEportChnls  = {2, 0, 2, 0, 2, 0, 2};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);

#endif
    LOG(INFO) << BOLDGREEN << "PS-ROH-" << +cChipRate << "G lpGBT configuration APPLIED" << RESET;
}
} // namespace Ph2_HwInterface
