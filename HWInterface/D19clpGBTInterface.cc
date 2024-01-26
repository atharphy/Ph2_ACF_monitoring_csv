/*!
  \file                  D19clpGBTIn+erface.cc
  \brief                 Interface to access and control the low-power Gigabit Transceiver chip
  \author                Younes Otarid
  \version               1.0
  \date                  03/03/20
  Support:               email to younes.otarid@cern.ch
*/

#include "HWInterface/D19clpGBTInterface.h"
#include "HWDescription/lpGBT.h"
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
    if(cIter == cMaxIter) { throw std::runtime_error(std::string("lpGBT Power-Up State Machine Stuck at state " + fPUSMStatusMap[cChipVersion][cPUSMState])); }

    // Configuring chip
    ChipRegMap                                    clpGBTRegMap = pChip->getRegMap();
    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    cRegVec.clear();
    uint16_t maximumWritableRegister = (static_cast<lpGBT*>(pChip)->getVersion() == 0) ? 0x13C : 0x14F;

    for(const auto& cRegItem: clpGBTRegMap)
    {
        if(cRegItem.second.fAddress <= maximumWritableRegister) cRegVec.push_back(std::make_pair(cRegItem.first, cRegItem.second.fValue));
    } // get read/write registers

    WriteChipMultReg(pChip, cRegVec);

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
    // LoadCalibrationData(pChip, 0x00244200);
    if(cChipVersion == 1)
    {
        LOG(INFO) << BOLDBLUE << "Load calibration data and automatically tune vref (repeat if temperature changes)!" << RESET;

        LoadCalibrationData(static_cast<lpGBT*>(pChip), ReadChipID(pChip, cChipVersion));

        // Fabio's comment: auto tune should not be done here because if it gets calibrated and you try to reload again the registers
        // it will be overwritten
        // AutoTuneVref(pChip);

        // LOG(INFO) << BOLDBLUE << "Reading ADC channels" << RESET;
        // LOG(INFO) << BOLDGREEN << "AdcGetVin(pChip, \"ADC0\", \"VREF/2\", 0) " << RESET;
        // LOG(INFO) << BOLDGREEN << AdcGetVin(pChip, "ADC0", "VREF/2", 0) << " V" << RESET;
        // LOG(INFO) << BOLDGREEN << "AdcGetVin(pChip, \"ADC1\", \"VREF/2\", 0) " << RESET;
        // LOG(INFO) << BOLDGREEN << AdcGetVin(pChip, "ADC1", "VREF/2", 0) << " V" << RESET;
        // LOG(INFO) << BOLDGREEN << "AdcGetVin(pChip, \"ADC2\", \"VREF/2\", 0) " << RESET;
        // LOG(INFO) << BOLDGREEN << AdcGetVin(pChip, "ADC2", "VREF/2", 0) << " V" << RESET;
        // LOG(INFO) << BOLDGREEN << "AdcGetVin(pChip, \"ADC3\", \"VREF/2\", 0) " << RESET;
        // LOG(INFO) << BOLDGREEN << AdcGetVin(pChip, "ADC3", "VREF/2", 0) << " V" << RESET;
        // LOG(INFO) << BOLDGREEN << "AdcGetVin(pChip, \"ADC4\", \"VREF/2\", 0) " << RESET;
        // LOG(INFO) << BOLDGREEN << AdcGetVin(pChip, "ADC4", "VREF/2", 0) << " V" << RESET;
        // LOG(INFO) << BOLDGREEN << "AdcGetVin(pChip, \"ADC5\", \"VREF/2\", 0) " << RESET;
        // LOG(INFO) << BOLDGREEN << AdcGetVin(pChip, "ADC5", "VREF/2", 0) << " V" << RESET;
        // LOG(INFO) << BOLDGREEN << "AdcGetVin(pChip, \"ADC6\", \"VREF/2\", 0) " << RESET;
        // LOG(INFO) << BOLDGREEN << AdcGetVin(pChip, "ADC6", "VREF/2", 0) << " V" << RESET;
        // LOG(INFO) << BOLDGREEN << "AdcGetVin(pChip, \"ADC7\", \"VREF/2\", 0) " << RESET;
        // LOG(INFO) << BOLDGREEN << AdcGetVin(pChip, "ADC7", "VREF/2", 0) << " V" << RESET;
        // // Example on how to use the current source to measure resistance
        // // Only for OT-2S
        // // if(pChip->getFrontEndType() == FrontEndType::OuterTracker2S) {
        // // CdacSetCurrent(pChip, "ADC4", _CdacCodeToCurrent(pChip, "ADC4", 0xaa));
        // // LOG(INFO) << BOLDGREEN << "MeasureResistance(pChip,\"ADC4\", 1000, false) " << RESET;
        // // LOG(INFO) << BOLDGREEN << MeasureResistance(pChip, "ADC4", 1000, false) << " Ohms" << RESET;}
        // LOG(INFO) << BOLDGREEN << "MeasureTemperature(pChip) " << RESET;
        // LOG(INFO) << BOLDGREEN << MeasureTemperature(pChip) << " C" << RESET;

        // LOG(INFO) << BOLDGREEN << "MeasurePowerSupplyVoltage(pChip, \"VDDTX\")" << RESET;
        // LOG(INFO) << BOLDGREEN << MeasurePowerSupplyVoltage(pChip, "VDDTX") << " V" << RESET;
        // LOG(INFO) << BOLDGREEN << "MeasurePowerSupplyVoltage(pChip, \"VDDRX\")" << RESET;
        // LOG(INFO) << BOLDGREEN << MeasurePowerSupplyVoltage(pChip, "VDDRX") << " V" << RESET;
        // LOG(INFO) << BOLDGREEN << "MeasurePowerSupplyVoltage(pChip, \"VDD\")" << RESET;
        // LOG(INFO) << BOLDGREEN << MeasurePowerSupplyVoltage(pChip, "VDD") << " V" << RESET;
        // LOG(INFO) << BOLDGREEN << "MeasurePowerSupplyVoltage(pChip, \"VDDA\")" << RESET;
        // LOG(INFO) << BOLDGREEN << MeasurePowerSupplyVoltage(pChip, "VDDA") << " V" << RESET;
    }
    return cReady;
} //

/*-----------------------*/
/* OT specific functions */
/*-----------------------*/

bool D19clpGBTInterface::WriteChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& pRegVec, bool pVerify)
{
    // first, identify the correct BeBoardFWInterface
    setBoard(pChip->getBeBoardId());
    auto                     cRegMap = pChip->getRegMap();
    std::vector<ChipRegItem> cRegItems;
    for(auto cReq: pRegVec)
    {
        auto cIterator = cRegMap.find(cReq.first);
        if(cIterator == cRegMap.end())
        {
            LOG(ERROR) << BOLDRED << "D19clpGBTInterface::WriteChipMultReg trtying to write to a register that doesn't exist in the map : " << cReq.first << RESET;
            continue;
        }

        ChipRegItem cItem = cIterator->second;
        cItem.fValue      = cReq.second;
        cRegItems.push_back(cItem);
    }
    return fBoardFW->MultiRegisterWrite(pChip, cRegItems, pVerify);
}

std::vector<std::pair<std::string, uint16_t>> D19clpGBTInterface::ReadChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::string>& theRegisterList)
{
    setBoard(pChip->getBeBoardId());
    auto                     cRegMap = pChip->getRegMap();
    std::vector<ChipRegItem> cRegItems;
    for(auto cReq: theRegisterList)
    {
        auto cIterator = cRegMap.find(cReq);
        if(cIterator == cRegMap.end())
        {
            LOG(ERROR) << BOLDRED << "D19clpGBTInterface::WriteChipMultReg trtying to write to a register that doesn't exist in the map : " << cReq << RESET;
            abort();
        }

        ChipRegItem cItem = cIterator->second;
        cRegItems.push_back(cItem);
    }

    fBoardFW->MultiRegisterRead(pChip, cRegItems);

    std::vector<std::pair<std::string, uint16_t>> theRegisterValues;
    for(size_t i = 0; i < theRegisterList.size(); ++i) theRegisterValues.push_back(std::make_pair(theRegisterList[i], cRegItems[i].fValue));
    return theRegisterValues;
}

void D19clpGBTInterface::SetConfigMode(bool pOptical, bool pToggleTC)
{
    if(pOptical)
    {
        LOG(INFO) << BOLDGREEN << "Using Serial Interface configuration mode" << RESET;
#if defined(__TC_USB__)
        LOG(INFO) << BOLDBLUE << "Toggling Test Card" << RESET;
        if(pToggleTC && fTC_PSROH != nullptr) fTC_PSROH->toggle_SCI2C();
#endif
        fOptical = true;
    }
    else
    {
        LOG(INFO) << BOLDGREEN << "Using I2C Slave Interface configuration mode" << RESET;
        fOptical = false;
    }
}

void D19clpGBTInterface::hold2SModuleResets(Ph2_HwDescription::Chip* pChip)
{
    // Reset I2C Masters
    ResetI2C(pChip, {0, 1, 2});
    // hold resets
    for(uint8_t cSide = 0; cSide < 2; cSide++)
    {
        this->cbcReset(pChip, true, cSide);
        this->cicReset(pChip, true, cSide);
    }

    // Fabio: I do not think this part should be here, but I keep it for consistency with the previous code
#if defined(__TCUSB__)
    std::vector<uint8_t> cEportGroups = {4, 4, 5, 5, 6, 0};
    std::vector<uint8_t> cEportChnls  = {0, 2, 0, 2, 0, 0};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);
    cEportGroups = {0, 1, 1, 2, 2, 3};
    cEportChnls  = {2, 0, 2, 0, 2, 2};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);
    ConfigureCurrentDAC(pChip, std::vector<std::string>{"ADC4"}, 0x1c); // current chosen according to measurement range
#endif
}

void D19clpGBTInterface::holdPSModuleResets(Ph2_HwDescription::Chip* pChip)
{
    // Reset I2C Masters
    ResetI2C(pChip, {0, 1, 2});
    // hold resets
    for(uint8_t cSide = 0; cSide < 2; cSide++)
    {
        this->ssaReset(pChip, true, cSide);
        this->mpaReset(pChip, true, cSide);
        this->cicReset(pChip, true, cSide);
    }

    // Fabio: I do not think this part should be here, but I keep it for consistency with the previous code
#if defined(__TCUSB__)
    std::vector<uint8_t> cEportGroups = {4, 4, 5, 5, 6, 6, 0};
    std::vector<uint8_t> cEportChnls  = {0, 2, 0, 2, 0, 2, 0};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);
    cEportGroups = {0, 1, 1, 2, 2, 3, 3};
    cEportChnls  = {2, 0, 2, 0, 2, 0, 2};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);
#endif
}

void D19clpGBTInterface::configureClockSettings(Ph2_HwDescription::Chip* pChip, uint8_t pClk, lpGBTClockConfig pClkCnfg)
{
    fClkConfig.fClkFreq         = pClkCnfg.fClkFreq;
    fClkConfig.fClkInvert       = pClkCnfg.fClkInvert;
    fClkConfig.fClkDriveStr     = pClkCnfg.fClkDriveStr;
    fClkConfig.fClkPreEmphWidth = pClkCnfg.fClkPreEmphWidth;
    fClkConfig.fClkPreEmphMode  = pClkCnfg.fClkPreEmphMode;
    fClkConfig.fClkPreEmphStr   = pClkCnfg.fClkPreEmphStr;

    std::string cClkHReg = "EPCLK" + std::to_string(pClk) + "ChnCntrH";
    std::string cClkLReg = "EPCLK" + std::to_string(pClk) + "ChnCntrL";
    std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] writing " << cClkHReg << " to 0x" << std::hex << (fClkConfig.fClkInvert << 6 | fClkConfig.fClkDriveStr << 3 | fClkConfig.fClkFreq)
              << std::dec << std::endl;
    std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] writing " << cClkLReg << " to 0x" << std::hex
              << (fClkConfig.fClkPreEmphStr << 5 | fClkConfig.fClkPreEmphMode << 3 | fClkConfig.fClkPreEmphWidth) << std::dec << std::endl;
    WriteChipReg(pChip, cClkHReg, fClkConfig.fClkInvert << 6 | fClkConfig.fClkDriveStr << 3 | fClkConfig.fClkFreq);
    WriteChipReg(pChip, cClkLReg, fClkConfig.fClkPreEmphStr << 5 | fClkConfig.fClkPreEmphMode << 3 | fClkConfig.fClkPreEmphWidth);
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

    // Tx Groups and Channels

    uint8_t cTxDataRate = 3, cTxDriveStr = 7, cTxPreEmphMode = 0, cTxPreEmphStr = 0, cTxPreEmphWidth = 0;
    for(const auto& TxProperty: static_cast<lpGBT*>(pChip)->getTxProperties()) { lpGBTInterface::ConfigureTxGroup(pChip, TxProperty.Group, TxProperty.Channel, cTxDataRate); }

    for(const auto& TxProperty: static_cast<lpGBT*>(pChip)->getTxProperties())
    { ConfigureTxChannel(pChip, TxProperty.Group, TxProperty.Channel, cTxDriveStr, cTxPreEmphMode, cTxPreEmphStr, cTxPreEmphWidth, TxProperty.Polarity); }

    // Rx configuration and Phase Align
    // Configure Rx Groups
    // WriteChipReg(pChip, "EPRXDllConfig", , false);

    uint8_t cRxDataRate = 2, cRxTrackMode = 0; // manual mode by default
    for(const auto& RxProperty: static_cast<lpGBT*>(pChip)->getRxProperties()) { lpGBTInterface::ConfigureRxGroup(pChip, RxProperty.Group, RxProperty.Channel, cRxDataRate, cRxTrackMode); }

    uint8_t cRxEqual = 0, cRxTerm = 1, cRxAcBias = 0, cRxPhase = 5;
    for(const auto& RxProperty: static_cast<lpGBT*>(pChip)->getRxProperties())
    { ConfigureRxChannel(pChip, RxProperty.Group, RxProperty.Channel, cRxEqual, cRxTerm, cRxAcBias, RxProperty.Polarity, cRxPhase); }
    // Configuring I2C Master pull-ups for VTRx+
    WriteChipReg(pChip, "I2CM1Config", 1 << 4 | 1 << 6);
    // Reset I2C Masters
    ResetI2C(pChip, {0, 1, 2});
    // Setting GPIO levels Uncomment this for Skeleton test
    // Setting GPIO levels for Skeleton test
    ConfigureGPIODirection(pChip, {fReset_LHS_CIC, fReset_LHS_CBC, fReset_RHS_CIC, fReset_RHS_CBC}, 1);
    ConfigureGPIOLevel(pChip, {fReset_LHS_CIC, fReset_LHS_CBC, fReset_RHS_CIC, fReset_RHS_CBC, fReset_VTRx}, 1);
    // hold resets
    for(uint8_t cSide = 0; cSide < 2; cSide++)
    {
        this->cbcReset(pChip, true, cSide);
        this->cicReset(pChip, true, cSide);
    }
#if defined(__TCUSB__)
    std::vector<uint8_t> cEportGroups = {4, 4, 5, 5, 6, 0};
    std::vector<uint8_t> cEportChnls  = {0, 2, 0, 2, 0, 0};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);
    cEportGroups = {0, 1, 1, 2, 2, 3};
    cEportChnls  = {2, 0, 2, 0, 2, 2};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);
    ConfigureCurrentDAC(pChip, std::vector<std::string>{"ADC4"}, 0x1c); // current chosen according to measurement range
#endif
}
void D19clpGBTInterface::Add2SSEHeLinkProperties(Ph2_HwDescription::Chip* pChip)
{
    std::vector<uint8_t> cRxGroups = {0, 1, 2, 3, 4, 5, 6}, cRxChannels = {0, 2};
    uint8_t              cRxInvert = 0;
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

            if(!((cGroup == 6 && cChannel == 2) || (cGroup == 3 && cChannel == 0))) { static_cast<lpGBT*>(pChip)->addRxProperty(cGroup, cChannel, cRxInvert); }
        }
    }
    std::vector<uint8_t> cTxGroups = {0, 2};
    uint8_t              cTxInvert = 0;

    for(const auto& cGroup: cTxGroups)
    {
        if(cGroup == 0) cTxInvert = 1;
        if(cGroup == 2) cTxInvert = 0;

        static_cast<lpGBT*>(pChip)->addTxProperty(cGroup, 0, cTxInvert);
    }
}

void D19clpGBTInterface::InitialPhaseAlignRx(Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels)
{
    std::vector<uint8_t> cOptimalTaps = {};
    PhaseAlignRx(pChip, pGroups, pChannels);
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

uint8_t D19clpGBTInterface::PhaseAlignRx(Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels)
{
    LOG(INFO) << BOLDBLUE << "Aligning lpGBT#" << +pChip->getId() << RESET;
    const uint8_t cChipRate = lpGBTInterface::GetChipRate(pChip);

    // Configure Rx Phase Shifter
    uint16_t cDelay = 0;
    uint8_t  cFreq = (cChipRate == 5) ? 4 : 5, cEnFTune = 0, cDriveStr = 3; // 4 --> 320 MHz || 5 --> 640 MHz
    lpGBTInterface::ConfigurePhShifter(pChip, {0, 2}, cFreq, cDriveStr, cEnFTune, cDelay);

    // // Set data source for channels 0,2 to PRBS
    // lpGBTInterface::ConfigureRxSource(pChip, pGroups, lpGBTconstants::PATTERN_PRBS);
    // // Turn ON PRBS for channels 0,2
    // lpGBTInterface::ConfigureRxPRBS(pChip, pGroups, pChannels, true);
    std::vector<uint8_t> cAligned(pGroups.size(), 0);
    bool                 cSuccess = true;
    std::vector<uint8_t> cOptimalTaps(0);

    for(size_t cIndx = 0; cIndx < pGroups.size(); cIndx++)
    {
        uint8_t cGroup   = pGroups[cIndx];
        uint8_t cChannel = pChannels[cIndx];

        cFreq         = 2;
        uint8_t cMode = 1; // Initial training mode
        lpGBTInterface::ConfigureRxGroup(pChip, cGroup, cChannel, cFreq, cMode);
        std::string cTrainRxReg;
        if(cGroup == 0 || cGroup == 1)
            cTrainRxReg = "EPRXTrain10";
        else if(cGroup == 2 || cGroup == 3)
            cTrainRxReg = "EPRXTrain32";
        else if(cGroup == 4 || cGroup == 5)
            cTrainRxReg = "EPRXTrain54";
        else if(cGroup == 6)
            cTrainRxReg = "EPRXTrainEc6";

        std::vector<uint8_t> cPhases(0);
        std::vector<uint8_t> cUniquePhases(0);
        LOG(DEBUG) << BOLDYELLOW << "Group#" << +cGroup << " Channel#" << +cChannel << "...checking phase aligner" << RESET;
        size_t cMaxAttempts = 5;
        for(size_t cAttempt = 0; cAttempt < cMaxAttempts; cAttempt++)
        {
            uint8_t cChipVersion = static_cast<lpGBT*>(pChip)->getVersion();
            if(cChipVersion == 0) { ResetRxDll(pChip, {cGroup}); }
            // Enable training
            uint8_t cTrainingShift = cChannel + 4 * (cGroup % 2);

            // Assumption: write is stable enough that it sohuld never fail in normal condition
            // If the fail occurs it means that the Chip has a major issue and this check avoids to be stuck in this loop for a very long time
            bool writeSucceded = WriteChipReg(pChip, cTrainRxReg, (0x1 << cTrainingShift));
            if(!writeSucceded) return 15;
            std::this_thread::sleep_for(std::chrono::microseconds((lpGBTconstants::DEEPSLEEP) / 10));
            WriteChipReg(pChip, cTrainRxReg, (0x0 << cTrainingShift));
            std::this_thread::sleep_for(std::chrono::microseconds((lpGBTconstants::DEEPSLEEP) / 10));
            // Check for lock
            std::string cRXLockedReg = "EPRX" + std::to_string(cGroup) + "Locked";
            uint8_t     cLockShift   = cChannel + 4;
            auto        cLock        = 0;
            uint8_t     cCurrPhase   = lpGBTInterface::GetRxPhase(pChip, cGroup, cChannel);
            bool        cContinue    = (cLock == 0);
            uint8_t     cMaxIters    = 10;
            uint8_t     cIter        = 0;
            do
            {
                std::this_thread::sleep_for(std::chrono::microseconds((lpGBTconstants::DEEPSLEEP) / 10));
                cLock     = (ReadChipReg(pChip, cRXLockedReg) & (1 << cLockShift)) >> cLockShift;
                cContinue = cLock == 0;
                cIter++;
            } while(cContinue && cIter < cMaxIters);
            if(cLock) cAligned[cIndx] += 1;
            WriteChipReg(pChip, cTrainRxReg, (0x0 << cTrainingShift));
            std::this_thread::sleep_for(std::chrono::microseconds((lpGBTconstants::DEEPSLEEP) / 10));
            cCurrPhase = lpGBTInterface::GetRxPhase(pChip, cGroup, cChannel);
            LOG(DEBUG) << BOLDGREEN << "\t\t..Attempt# " << +cAttempt << "\t... RxPhase found  is... " << +cCurrPhase << RESET;
            cPhases.push_back(cCurrPhase);
            cUniquePhases.push_back(cCurrPhase);
        }

        cSuccess = cSuccess && (cAligned[cIndx] == cMaxAttempts);

        std::sort(cUniquePhases.begin(), cUniquePhases.end());
        cUniquePhases.erase(unique(cUniquePhases.begin(), cUniquePhases.end()), cUniquePhases.end());
        std::vector<uint8_t> cCount(0);
        size_t               cIndxBstPhase = 0;
        size_t               cCntBstPhase  = 0;
        for(size_t cIndx2 = 0; cIndx2 < cUniquePhases.size(); cIndx2++)
        {
            uint8_t cCountThisPhase = 0;
            for(auto cThisPhase: cPhases) { cCountThisPhase += (cThisPhase == cUniquePhases[cIndx2]); }
            if(cCountThisPhase >= cCntBstPhase)
            {
                cCntBstPhase  = cCountThisPhase;
                cIndxBstPhase = cIndx2;
            }
        }

        cSuccess = cSuccess && (cUniquePhases[cIndxBstPhase] != 15);
        if(cUniquePhases[cIndxBstPhase] != 15)
        {
            LOG(INFO) << BOLDGREEN << "Group#" << +cGroup << " Channel#" << +cChannel << "...\t\t..Most frequently found phase is " << +cUniquePhases[cIndxBstPhase] << RESET;
            SetPhaseTap(pChip, cGroup, cChannel, cUniquePhases[cIndxBstPhase]);
            cOptimalTaps.push_back(cUniquePhases[cIndxBstPhase]);
        }
        else
        {
            LOG(INFO) << BOLDRED << "Group#" << +cGroup << " Channel#" << +cChannel << "\t\t..Most frequently found phase is " << +cUniquePhases[cIndxBstPhase] << RESET;
            ConfigureRxPhase(pChip, cGroup, cChannel, 0);
            SetPhaseTap(pChip, cGroup, cChannel, cUniquePhases[cIndxBstPhase]);
        }
        ConfigureRxPhase(pChip, cGroup, cChannel, cUniquePhases[cIndxBstPhase]);
    }
    // Find mode
    std::vector<uint8_t> cTapsHist(15, 0);
    for(auto cItem: cOptimalTaps) cTapsHist[cItem]++;

    auto cTapMode = std::max_element(cTapsHist.begin(), cTapsHist.end()) - cTapsHist.begin();
    LOG(INFO) << BOLDMAGENTA << "Most frequent optimal tap is " << +cTapMode << RESET;
    uint8_t cMode = 0; // 2, continuous phase tracking : 0, fixed phase
    for(const auto& group: pGroups)
        for(const auto& channel: pChannels) lpGBTInterface::ConfigureRxGroup(pChip, group, channel, 2, cMode);

    return (cSuccess) ? cTapMode : 15;
}

void D19clpGBTInterface::ConfigurePSROH(Ph2_HwDescription::Chip* pChip)
{
    uint8_t cChipRate = GetChipRate(pChip);
    LOG(INFO) << BOLDGREEN << "Applying PS-ROH-" << +cChipRate << "G lpGBT configuration" << RESET;
    // Forcing driver attenuation to be 1
    uint8_t cEQAttenuation = 3;
    WriteChipReg(pChip, "EQConfig", cEQAttenuation << 3);

    // Configuring I2C Master pull-ups for VTRx+
    WriteChipReg(pChip, "I2CM1Config", 1 << 4 | 1 << 6);
    // Clocks
    std::vector<uint8_t> cClocks = {fClock_LHS_Hybrid, fClock_LHS_CIC, fClock_RHS_Hybrid, fClock_RHS_CIC};
    // clock frequency set to 0 to disable it at first and only later configure what is needed
    uint8_t cClkFreq = 0, cClkDriveStr = 0, cClkInvert = 0;
    uint8_t cClkPreEmphWidth = 0, cClkPreEmphMode = 0, cClkPreEmphStr = 0;
    ConfigureClocks(pChip, cClocks, cClkFreq, cClkDriveStr, cClkInvert, cClkPreEmphWidth, cClkPreEmphMode, cClkPreEmphStr);

    // Tx Groups and Channels
    uint8_t cTxDataRate = 3, cTxDriveStr = 7, cTxPreEmphMode = 1, cTxPreEmphStr = 4, cTxPreEmphWidth = 0;
    for(const auto& TxProperty: static_cast<lpGBT*>(pChip)->getTxProperties()) { lpGBTInterface::ConfigureTxGroup(pChip, TxProperty.Group, TxProperty.Channel, cTxDataRate); }

    for(const auto& TxProperty: static_cast<lpGBT*>(pChip)->getTxProperties())
    { ConfigureTxChannel(pChip, TxProperty.Group, TxProperty.Channel, cTxDriveStr, cTxPreEmphMode, cTxPreEmphStr, cTxPreEmphWidth, TxProperty.Polarity); }

    // Rx configuration and Phase Align
    // Configure Rx Groups
    uint8_t cRxDataRate = 2, cRxTrackMode = 0;
    for(const auto& RxProperty: static_cast<lpGBT*>(pChip)->getRxProperties()) { lpGBTInterface::ConfigureRxGroup(pChip, RxProperty.Group, RxProperty.Channel, cRxDataRate, cRxTrackMode); }

    uint8_t cRxEqual = 0, cRxTerm = 1, cRxAcBias = 0, cRxPhase = 9;
    for(const auto& RxProperty: static_cast<lpGBT*>(pChip)->getRxProperties())
    { ConfigureRxChannel(pChip, RxProperty.Group, RxProperty.Channel, cRxEqual, cRxTerm, cRxAcBias, RxProperty.Polarity, cRxPhase); }

    // Reset I2C Masters
    ResetI2C(pChip, {0, 1, 2});
    // Setting GPIO levels for PS ROH
    ConfigureGPIODirection(pChip, {fReset_LHS_CIC, fReset_LHS_MPA, fReset_LHS_SSA, fReset_RHS_CIC, fReset_RHS_MPA, fReset_RHS_SSA}, 1);
    ConfigureGPIOLevel(pChip, {fReset_LHS_CIC, fReset_LHS_MPA, fReset_LHS_SSA, fReset_RHS_CIC, fReset_RHS_MPA, fReset_RHS_SSA, fReset_VTRx}, 1);
    // hold resets
    for(uint8_t cSide = 0; cSide < 2; cSide++)
    {
        this->ssaReset(pChip, true, cSide);
        this->mpaReset(pChip, true, cSide);
        this->cicReset(pChip, true, cSide);
    }
#if defined(__TCUSB__)
    std::vector<uint8_t> cEportGroups = {4, 4, 5, 5, 6, 6, 0};
    std::vector<uint8_t> cEportChnls  = {0, 2, 0, 2, 0, 2, 0};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);
    cEportGroups = {0, 1, 1, 2, 2, 3, 3};
    cEportChnls  = {2, 0, 2, 0, 2, 0, 2};
    InitialPhaseAlignRx(pChip, cEportGroups, cEportChnls);

#endif
    LOG(INFO) << BOLDGREEN << "PS-ROH-" << +cChipRate << "G lpGBT configuration APPLIED" << RESET;
}
void D19clpGBTInterface::AddPSROHeLinkProperties(Ph2_HwDescription::Chip* pChip)
{
    std::vector<uint8_t> cTxGroups = {0, 1, 2, 3};
    uint8_t              cTxInvert = 0;
    for(const auto& cGroup: cTxGroups)
    {
        cTxInvert = (cGroup % 2 == 0) ? 1 : 0;
        static_cast<lpGBT*>(pChip)->addTxProperty(cGroup, 0, cTxInvert);
    }
    std::vector<uint8_t> cGrpsLeft{0, 1, 1, 2, 2, 3, 3};
    std::vector<uint8_t> cChnlsLeft{2, 0, 2, 0, 2, 0, 2};
    std::vector<uint8_t> cInvrtLeft{1, 1, 0, 1, 1, 1, 1};
    for(size_t cIndx = 0; cIndx < cInvrtLeft.size(); cIndx++)
    {
        uint8_t cGroup    = cGrpsLeft[cIndx];
        uint8_t cChannel  = cChnlsLeft[cIndx];
        uint8_t cRxInvert = cInvrtLeft[cIndx];
        static_cast<lpGBT*>(pChip)->addRxProperty(cGroup, cChannel, cRxInvert);
    }
    std::vector<uint8_t> cGrpsRight{4, 4, 5, 5, 6, 6, 0};
    std::vector<uint8_t> cChnlsRight{2, 0, 2, 0, 2, 0, 0};
    std::vector<uint8_t> cInvrtRight{0, 0, 0, 0, 0, 0, 1};
    for(size_t cIndx = 0; cIndx < cInvrtLeft.size(); cIndx++)
    {
        uint8_t cGroup    = cGrpsRight[cIndx];
        uint8_t cChannel  = cChnlsRight[cIndx];
        uint8_t cRxInvert = cInvrtRight[cIndx];
        static_cast<lpGBT*>(pChip)->addRxProperty(cGroup, cChannel, cRxInvert);
    }
}

void D19clpGBTInterface::updateCICinputClockToMatchPSrate(Ph2_HwDescription::Chip* pChip)
{
    auto        theChipRate               = this->GetChipRate(pChip);
    std::string cicClockRightRegisterName = "EPCLK" + std::to_string(fClock_RHS_CIC) + "ChnCntrH";
    std::string cicClockLeftRegisterName  = "EPCLK" + std::to_string(fClock_LHS_CIC) + "ChnCntrH";

    uint8_t expectedCicClockSetting;
    if(theChipRate == 5)
        expectedCicClockSetting = 0x4;
    else if(theChipRate == 10)
        expectedCicClockSetting = 0x5;
    else
    {
        std::string errorMessage =
            std::string(__PRETTY_FUNCTION__) + " LpGBT TX rate not identified on BeBoard " + std::to_string(pChip->getBeBoardId()) + " OpticalGroup " + std::to_string(pChip->getOpticalGroupId());
        throw std::runtime_error(errorMessage);
    }

    auto updateClockFunction = [this, theChipRate, pChip, expectedCicClockSetting](std::string registerName) {
        auto theCurrentRegisterValue = this->ReadChipReg(pChip, registerName);
        if((theCurrentRegisterValue & 0x7) != expectedCicClockSetting)
        {
            uint16_t theNewRegisterValue = (theCurrentRegisterValue & 0xF8) | (expectedCicClockSetting & 0x7);
            LOG(INFO) << BOLDYELLOW << "Attention! Updating " << registerName << " from 0x" << std::hex << +theCurrentRegisterValue << " to 0x" << +theNewRegisterValue << std::dec
                      << " to provide the CIC with the correct clock based on the LpGBT data rate (" << +theChipRate << "Gb) for on BeBoard " << +pChip->getBeBoardId() << " OpticalGroup "
                      << +pChip->getOpticalGroupId() << RESET;
            this->WriteChipReg(pChip, registerName, theNewRegisterValue);
        }
    };

    updateClockFunction(cicClockRightRegisterName);
    updateClockFunction(cicClockLeftRegisterName);
}

} // namespace Ph2_HwInterface
