/*!
  \file                  lpGBTInterface.h
  \brief                 ImInterface to access and control the low-power Gigabit Transceiver chip
  \author                Younes Otarid
  \version               1.0
  \date                  03/03/20
  Support:               email to younes.otarid@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#include "lpGBTInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// #######################################
// # LpGBT block configuration functions #
// #######################################

void lpGBTInterface::SetPUSMDone(Chip* pChip, bool pPllConfigDone, bool pDllConfigDone) { WriteChipReg(pChip, "POWERUP2", pDllConfigDone << 2 | pPllConfigDone << 1); }

void lpGBTInterface::ConfigureRxGroups(Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, uint8_t pDataRate, uint8_t pTrackMode)
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

void lpGBTInterface::ConfigureRxChannels(Chip*                       pChip,
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

void lpGBTInterface::ConfigureTxGroups(Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, uint8_t pDataRate)
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

void lpGBTInterface::ConfigureTxChannels(Chip*                       pChip,
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
            std::string cTXChnCntr;
            if(cChannel == 0 || cChannel == 1)
                cTXChnCntr = "EPTX" + std::to_string(cGroup) + "1_" + std::to_string(cGroup) + "0ChnCntr";
            else if(cChannel == 2 || cChannel == 3)
                cTXChnCntr = "EPTX" + std::to_string(cGroup) + "3_" + std::to_string(cGroup) + "2ChnCntr";

            uint8_t cValueChnCntr = ReadChipReg(pChip, cTXChnCntr);
            WriteChipReg(pChip, cTXChnCntr, (cValueChnCntr & ~(0x0F << 4 * (cChannel % 2))) | ((pInvert << 3 | pPreEmphWidth << 0) << 4 * (cChannel % 2)));
        }
    }
}

void lpGBTInterface::ConfigureClocks(Chip*                       pChip,
                                     const std::vector<uint8_t>& pClocks,
                                     uint8_t                     pFreq,
                                     uint8_t                     pDriveStr,
                                     uint8_t                     pInvert,
                                     uint8_t                     pPreEmphWidth,
                                     uint8_t                     pPreEmphMode,
                                     uint8_t                     pPreEmphStr)
{
    for(const auto& cClock: pClocks)
    {
        // Configure Clocks Frequency, Drive Strength, Inversion, Pre-Emphasis Width, Pre-Emphasis Mode, Pre-Emphasis Strength
        std::string cClkHReg = "EPCLK" + std::to_string(cClock) + "ChnCntrH";
        std::string cClkLReg = "EPCLK" + std::to_string(cClock) + "ChnCntrL";
        WriteChipReg(pChip, cClkHReg, pInvert << 6 | pDriveStr << 3 | pFreq);
        WriteChipReg(pChip, cClkLReg, pPreEmphStr << 5 | pPreEmphMode << 3 | pPreEmphWidth);
    }
}

void lpGBTInterface::ConfigureHighSpeedPolarity(Chip* pChip, uint8_t pOutPolarity, uint8_t pInPolarity)
{
    uint8_t cPolarity = (pOutPolarity << 7 | pInPolarity << 6);
    WriteChipReg(pChip, "ChipConfig", cPolarity);
}

void lpGBTInterface::ConfigureDPPattern(Chip* pChip, uint32_t pPattern)
{
    WriteChipReg(pChip, "DPDataPattern0", (pPattern & 0xFF));
    WriteChipReg(pChip, "DPDataPattern1", ((pPattern & 0xFF00) >> 8));
    WriteChipReg(pChip, "DPDataPattern2", ((pPattern & 0xFF0000) >> 16));
    WriteChipReg(pChip, "DPDataPattern3", ((pPattern & 0xFF000000) >> 24));
}

void lpGBTInterface::ConfigureRxPRBS(Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, bool pEnable)
{
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

        uint8_t cEnabledCh       = 0;
        uint8_t cValueEnablePRBS = ReadChipReg(pChip, cPRBSReg);
        for(const auto cChannel: pChannels) cEnabledCh |= pEnable << cChannel;
        WriteChipReg(pChip, cPRBSReg, (cValueEnablePRBS & ~(0xF << 4 * (cGroup % 2))) | (cEnabledCh << (4 * (cGroup % 2))));
    }
}

void lpGBTInterface::ConfigureRxSource(Chip* pChip, const std::vector<uint8_t>& pGroups, uint8_t pSource)
{
    for(const auto& cGroup: pGroups)
    {
        if(pSource == 0)
            LOG(INFO) << GREEN << "Configuring Rx group " << BOLDYELLOW << +cGroup << RESET << GREEN << " source to " << BOLDYELLOW << "NORMAL " << RESET;
        else if(pSource == 1)
            LOG(INFO) << GREEN << "Configuring Rx group " << BOLDYELLOW << +cGroup << RESET << GREEN << " source to " << BOLDYELLOW << "PRBS7 " << RESET;
        else if(pSource == 4 || pSource == 5)
            LOG(INFO) << GREEN << "Configuring Rx group " << BOLDYELLOW << +cGroup << RESET << GREEN << " source to " << BOLDYELLOW << "Constant Pattern" << RESET;

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

void lpGBTInterface::ConfigureTxSource(Chip* pChip, const std::vector<uint8_t>& pGroups, uint8_t pSource)
{
    for(const auto& cGroup: pGroups)
    {
        if(pSource == 0)
            LOG(INFO) << GREEN << "Configuring Rx Group " << BOLDYELLOW << +cGroup << RESET << GREEN << " Source to NORMAL " << RESET;
        else if(pSource == 1)
            LOG(INFO) << GREEN << "Configuring Rx Group " << BOLDYELLOW << +cGroup << RESET << GREEN << " Source to PRBS7 " << RESET;
        else if(pSource == 2)
            LOG(INFO) << GREEN << "Configuring Rx Group " << BOLDYELLOW << +cGroup << RESET << GREEN << " Source to Binary counter " << RESET;
        else if(pSource == 3)
            LOG(INFO) << GREEN << "Configuring Rx Group " << BOLDYELLOW << +cGroup << RESET << GREEN << " Source to Constant Pattern" << RESET;

        uint8_t cULDataSrcValue = ReadChipReg(pChip, "ULDataSource5");
        cULDataSrcValue         = (cULDataSrcValue & ~(0x3 << (2 * cGroup))) | (pSource << (2 * cGroup));
        WriteChipReg(pChip, "ULDataSource5", cULDataSrcValue);
    }
}

void lpGBTInterface::ConfigureRxPhase(Chip* pChip, uint8_t pGroup, uint8_t pChannel, uint8_t pPhase)
{
    std::string cRegName      = "EPRX" + std::to_string(pGroup) + std::to_string(pChannel) + "ChnCntr";
    uint8_t     cValueChnCntr = ReadChipReg(pChip, cRegName);
    cValueChnCntr             = (cValueChnCntr & ~(0xF << 4)) | (pPhase << 4);
    WriteChipReg(pChip, cRegName, cValueChnCntr);
}

void lpGBTInterface::ConfigurePhShifter(Chip* pChip, const std::vector<uint8_t>& pClocks, uint8_t pFreq, uint8_t pDriveStr, uint8_t pEnFTune, uint16_t pDelay)
{
    for(const auto& cClock: pClocks)
    {
        std::string cDelayReg  = "PS" + std::to_string(cClock) + "Delay";
        std::string cConfigReg = "PS" + std::to_string(cClock) + "Config";
        WriteChipReg(pChip, cConfigReg, (((pDelay & 0x100) >> 8) << 7) | pEnFTune << 6 | pDriveStr << 3 | pFreq);
        WriteChipReg(pChip, cDelayReg, pDelay);
    }
}

// ####################################
// # LpGBT specific routine functions #
// ####################################

void lpGBTInterface::PhaseTrainRx(Chip* pChip, const std::vector<uint8_t>& pGroups, bool pTrain)
{
    for(const auto& cGroup: pGroups)
    {
        std::string cTrainRxReg;
        if(cGroup == 0 || cGroup == 1)
            cTrainRxReg = "EPRXTrain10";
        else if(cGroup == 2 || cGroup == 3)
            cTrainRxReg = "EPRXTrain32";
        else if(cGroup == 4 || cGroup == 5)
            cTrainRxReg = "EPRXTrain54";
        else if(cGroup == 6)
            cTrainRxReg = "EPRXTrainEc6";

        WriteChipReg(pChip, cTrainRxReg, 0x0F << 4 * (cGroup % 2));
        WriteChipReg(pChip, cTrainRxReg, 0x00 << 4 * (cGroup % 2));
    }
}

void lpGBTInterface::InternalPhaseAlignRx(Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels)
{
    const uint8_t cChipRate = lpGBTInterface::GetChipRate(pChip);

    // Set data source to PRBS
    lpGBTInterface::ConfigureRxSource(pChip, pGroups, lpGBTconstants::PATTERN_PRBS);

    // Turn ON PRBS
    lpGBTInterface::ConfigureRxPRBS(pChip, pGroups, pChannels, true);

    // Configure Rx Phase Shifter
    uint16_t cDelay = 0x0;
    uint8_t  cFreq = (cChipRate == 5) ? 4 : 5, cEnFTune = 0, cDriveStr = 0; // 4 --> 320 MHz || 5 --> 640 MHz
    lpGBTInterface::ConfigurePhShifter(pChip, {0, 1, 2, 3}, cFreq, cDriveStr, cEnFTune, cDelay);

    lpGBTInterface::PhaseTrainRx(pChip, pGroups, true);
    for(const auto& cGroup: pGroups)
    {
        // Wait until channels lock
        LOG(INFO) << GREEN << "Phase aligning Rx Group " << BOLDYELLOW << +cGroup << RESET;
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
        } while(lpGBTInterface::IsRxLocked(pChip, cGroup, pChannels) == false);
        LOG(INFO) << BOLDBLUE << "\t--> Group " << BOLDYELLOW << +cGroup << BOLDBLUE << " LOCKED" << RESET;

        // Set new phase
        for(const auto& cChannel: pChannels)
        {
            uint8_t cCurrPhase = lpGBTInterface::GetRxPhase(pChip, cGroup, cChannel);
            LOG(INFO) << BOLDBLUE << "\t\t--> Channel " << BOLDYELLOW << +cChannel << BOLDBLUE << " has phase " << BOLDYELLOW << +cCurrPhase << RESET;
            lpGBTInterface::ConfigureRxPhase(pChip, cGroup, cChannel, cCurrPhase);
        }
    }
    lpGBTInterface::PhaseTrainRx(pChip, pGroups, false);

    // Set back Rx groups to fixed phase
    lpGBTInterface::ConfigureRxGroups(pChip, pGroups, pChannels, f10GRxDataRateMap[static_cast<lpGBT*>(pChip)->getRxDataRate()], lpGBTconstants::rxPhaseTracking);

    // Turn off PRBS
    lpGBTInterface::ConfigureRxPRBS(pChip, pGroups, pChannels, false);

    // Set back Rx source to normal data
    lpGBTInterface::ConfigureRxSource(pChip, pGroups, lpGBTconstants::PATTERN_NORMAL);
}

void lpGBTInterface::ResetRxDll(Chip* pChip, const std::vector<uint8_t>& pGroups)
{
    std::string cRegName = "RST1";
    uint8_t     cValue   = 0x00;

    for(auto cGroup: pGroups) { cValue = cValue | (1 << cGroup); }
    this->WriteChipReg(pChip, "RST1", cValue);
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    this->WriteChipReg(pChip, "RST1", 0x00);
}

// ################################
// # LpGBT Block Status functions #
// ################################

bool lpGBTInterface::IsPUSMDone(Chip* pChip) { return lpGBTInterface::GetPUSMStatus(pChip) == 18; }

void lpGBTInterface::PrintChipMode(Chip* pChip)
{
    switch((ReadChipReg(pChip, "ConfigPins") & 0xF0) >> 4)
    {
    case 0:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "5 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC5" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Off" << RESET;
        break;
    case 1:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "5 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC5" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Simplex TX" << RESET;
        break;
    case 2:
        LOG(INFO) << GREEN << "LpGBT chip info; Tx Data Rate = " << BOLDYELLOW << "5 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC5" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Simplex RX" << RESET;
        break;
    case 3:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "5 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC5" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Transceiver" << RESET;
        break;
    case 4:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "5 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC12" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Off" << RESET;
        break;
    case 5:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "5 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC12" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Simplex TX" << RESET;
        break;
    case 6:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "5 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC12" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Simplex RX" << RESET;
        break;
    case 7:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "5 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC12" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Transceiver" << RESET;
        break;
    case 8:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "10 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC5" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Off" << RESET;
        break;
    case 9:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "10 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC5" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Simplex TX" << RESET;
        break;
    case 10:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "10 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC5" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Simplex RX" << RESET;
        break;
    case 11:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "10 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC5" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Transceiver" << RESET;
        break;
    case 12:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "10 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC12" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Off" << RESET;
        break;
    case 13:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "10 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC12" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Simplex TX" << RESET;
        break;
    case 14:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "10 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC12" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Simplex RX" << RESET;
        break;
    case 15:
        LOG(INFO) << GREEN << "LpGBT chip info: Tx Data Rate = " << BOLDYELLOW << "10 Gbit/s" << RESET << GREEN << "; TxEncoding = " << BOLDYELLOW << "FEC12" << RESET << GREEN
                  << "; LpGBT Mode = " << BOLDYELLOW << "Transceiver" << RESET;
        break;
    }
}

uint8_t lpGBTInterface::GetChipRate(Chip* pChip)
{
    uint8_t cValueConfigPins = ((ReadChipReg(pChip, "ConfigPins") & 0xF0) >> 4);
    if(cValueConfigPins <= 7)
        return 5;
    else if(cValueConfigPins <= 15)
        return 10;
    else
        throw std::runtime_error(std::string("lpGBT hard wired configuration doesn't exist"));
}

uint8_t lpGBTInterface::GetPUSMStatus(Chip* pChip) { return ReadChipReg(pChip, "PUSMStatus"); }

uint8_t lpGBTInterface::GetRxPhase(Chip* pChip, uint8_t pGroup, uint8_t pChannel)
{
    std::string cRxPhaseReg;
    if(pChannel == 0 || pChannel == 1)
        cRxPhaseReg = "EPRX" + std::to_string(pGroup) + "CurrentPhase10";
    else if(pChannel == 3 || pChannel == 2)
        cRxPhaseReg = "EPRX" + std::to_string(pGroup) + "CurrentPhase32";

    uint8_t cRxPhaseRegValue = ReadChipReg(pChip, cRxPhaseReg);
    return ((cRxPhaseRegValue & (0x0F << 4 * (pChannel % 2))) >> 4 * (pChannel % 2));
}

bool lpGBTInterface::IsRxLocked(Chip* pChip, uint8_t pGroup, const std::vector<uint8_t>& pChannels)
{
    std::string cRXLockedReg = "EPRX" + std::to_string(pGroup) + "Locked";
    uint8_t     cChannelMask = 0;
    for(auto cChannel: pChannels) cChannelMask |= (1 << cChannel);
    return (((ReadChipReg(pChip, cRXLockedReg) & (cChannelMask << 4)) >> 4) == cChannelMask);
}

uint8_t lpGBTInterface::GetRxDllStatus(Chip* pChip, uint8_t pGroup)
{
    std::string cRXDllStatReg = "EPRX" + std::to_string(pGroup) + "DllStatus";
    return ReadChipReg(pChip, cRXDllStatReg);
}

// ########################
// # LpGBT GPIO functions #
// ########################

void lpGBTInterface::ConfigureGPIODirection(Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pDir)
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

void lpGBTInterface::ConfigureGPIOLevel(Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pOut)
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

void lpGBTInterface::ConfigureGPIODriverStrength(Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pDriveStr)
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

void lpGBTInterface::ConfigureGPIOPull(Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pEnable, uint8_t pUpDown)
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

bool lpGBTInterface::ReadGPIO(Ph2_HwDescription::Chip* pChip, uint8_t pGPIO)
{
    LOG(INFO) << GREEN << "Reading GPIO value from " << BOLDYELLOW << std::to_string(pGPIO) << RESET;
    uint8_t cPIOInH = ReadChipReg(pChip, "PIOInH");
    uint8_t cPIOInL = ReadChipReg(pChip, "PIOInL");
    return ((cPIOInH << 8 | cPIOInL) >> pGPIO) & 1;
}

// ###########################
// # LpGBT ADC-DAC functions #
// ###########################

void lpGBTInterface::ConfigureADC(Chip* pChip, uint8_t pGainSelect, bool pADCEnable, bool pStartConversion) { WriteChipReg(pChip, "ADCConfig", pStartConversion << 7 | pADCEnable << 2 | pGainSelect); }

void lpGBTInterface::ConfigureCurrentDAC(Chip* pChip, const std::vector<std::string>& pCurrentDACChannels, uint8_t pCurrentDACOutput)
{
    // Enables current DAC without changing the voltage DAC
    uint8_t cDACConfigH = ReadChipReg(pChip, "DACConfigH");
    WriteChipReg(pChip, "DACConfigH", cDACConfigH | 0x40);

    // Sets output current for the current DAC. Current = CURDACSelect * 3.5 uA
    WriteChipReg(pChip, "CURDACValue", pCurrentDACOutput);

    // Setting Nth bit in this register attaches current DAC to ADCN pin. Current source can be attached to any number of channels
    uint8_t cCURDACCHN = 0;
    uint8_t cADCInput;

    for(auto cCurrentDACChannel: pCurrentDACChannels)
    {
        cADCInput = lpGBTInterface::fADCInputMap[cCurrentDACChannel];
        cCURDACCHN += 1 << cADCInput;
        WriteChipReg(pChip, "CURDACCHN", cCURDACCHN);
    }
}

uint16_t lpGBTInterface::ReadADC(Chip* pChip, const std::string& pADCInputP, const std::string& pADCInputN, uint8_t pGain)
{
    // Read differential (converted) data on two ADC inputs
    uint8_t cADCInputP = lpGBTInterface::fADCInputMap[pADCInputP];
    uint8_t cADCInputN = lpGBTInterface::fADCInputMap[pADCInputN];

    LOG(INFO) << GREEN << "Reading ADC value from " << BOLDYELLOW << pADCInputP << RESET;

    // Select ADC Input
    WriteChipReg(pChip, "ADCSelect", cADCInputP << 4 | cADCInputN << 0);

    // Enable ADC Input without starting conversion
    lpGBTInterface::ConfigureADC(pChip, pGain, true, false);

    // Enable Internal VREF
    WriteChipReg(pChip, "VREFCNTR", 1 << 7);

    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    // Start ADC conversion
    lpGBTInterface::ConfigureADC(pChip, pGain, true, true);

    // Check conversion status
    uint8_t cIter    = 0;
    bool    cSuccess = false;
    do
    {
        LOG(INFO) << GREEN << "Waiting for ADC conversion to end" << RESET;

        cSuccess = lpGBTInterface::IsReadADCDone(pChip);
        cIter++;
    } while((cIter < RD53Shared::MAXATTEMPTS) && (cSuccess == false));
    if(cIter == RD53Shared::MAXATTEMPTS) throw std::runtime_error(std::string("ADC conversion timed out"));

    // Read ADC value
    uint8_t cADCvalue1 = ReadChipReg(pChip, "ADCStatusH") & 0x3;
    uint8_t cADCvalue2 = ReadChipReg(pChip, "ADCStatusL");

    // Clear ADC conversion bit and disable ADC
    lpGBTInterface::ConfigureADC(pChip, pGain, false, false);

    return (cADCvalue1 << 8 | cADCvalue2);
}

bool lpGBTInterface::IsReadADCDone(Chip* pChip) { return (((ReadChipReg(pChip, "ADCStatusH") & 0x40) >> 6) == 1); }

// #######################
// # Bit Error Rate test #
// #######################

void lpGBTInterface::ConfigureBERT(Chip* pChip, uint8_t pCoarseSource, uint8_t pFineSource, uint8_t pMeasTime, bool pSkipDisable)
{
    WriteChipReg(pChip, "BERTSource", (pCoarseSource << 4) | pFineSource);
    WriteChipReg(pChip, "BERTConfig", (pMeasTime << 4) | (pSkipDisable << 1));
}

void lpGBTInterface::StartBERT(Chip* pChip, bool pStartBERT)
{
    uint8_t cRegisterValue = ReadChipReg(pChip, "BERTConfig");
    WriteChipReg(pChip, "BERTConfig", (cRegisterValue & ~(0x1 << 0)) | (pStartBERT << 0));
}

void lpGBTInterface::ConfigureBERTPattern(Chip* pChip, uint32_t pPattern)
{
    LOG(INFO) << GREEN << "Setting BERT pattern to " << BOLDYELLOW << std::bitset<32>(pPattern) << RESET;

    WriteChipReg(pChip, "BERTDataPattern0", (pPattern & (0xFF << 0)) >> 0);
    WriteChipReg(pChip, "BERTDataPattern1", (pPattern & (0xFF << 8)) >> 8);
    WriteChipReg(pChip, "BERTDataPattern2", (pPattern & (0xFF << 16)) >> 16);
    WriteChipReg(pChip, "BERTDataPattern3", (pPattern & (0xFF << 24)) >> 24);
}

uint8_t lpGBTInterface::GetBERTStatus(Chip* pChip) { return ReadChipReg(pChip, "BERTStatus"); }

bool lpGBTInterface::IsBERTDone(Chip* pChip) { return (lpGBTInterface::GetBERTStatus(pChip) & 0x1) == 1; }

bool lpGBTInterface::IsBERTEmptyData(Chip* pChip) { return ((lpGBTInterface::GetBERTStatus(pChip) & (0x1 << 2)) >> 2) == 1; }

uint64_t lpGBTInterface::GetBERTErrors(Chip* pChip)
{
    uint64_t cResult0 = ReadChipReg(pChip, "BERTResult0");
    uint64_t cResult1 = ReadChipReg(pChip, "BERTResult1");
    uint64_t cResult2 = ReadChipReg(pChip, "BERTResult2");
    uint64_t cResult3 = ReadChipReg(pChip, "BERTResult3");
    uint64_t cResult4 = ReadChipReg(pChip, "BERTResult4");

    return ((cResult4 << 32) | (cResult3 << 24) | (cResult2 << 16) | (cResult1 << 8) | cResult0);
}

double lpGBTInterface::GetBERTResult(Chip* pChip)
{
    lpGBTInterface::StartBERT(pChip, false); // Stop
    lpGBTInterface::StartBERT(pChip, true);  // Start
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    // Wait for BERT to end
    while(lpGBTInterface::IsBERTDone(pChip) == false)
    {
        LOG(INFO) << BOLDBLUE << "\t--> BERT still running ... " << RESET;
        std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    }

    // Throw error if empty data
    if(lpGBTInterface::IsBERTEmptyData(pChip) == true)
    {
        // Stop BERT
        lpGBTInterface::StartBERT(pChip, false);
        LOG(INFO) << BOLDRED << "BERT : All zeros at input ... exiting" << RESET;
        throw std::runtime_error(std::string("BERT : All zeros at input"));
    }

    // Compute number of bits checked
    uint64_t cErrors           = lpGBTInterface::GetBERTErrors(pChip);
    uint8_t  cMeasTime         = (ReadChipReg(pChip, "BERTConfig") & (0xF << 4)) >> 4;
    uint64_t cNClkCycles       = std::pow(2, 5 + cMeasTime * 2);
    uint8_t  cNBitsPerClkCycle = (lpGBTInterface::GetChipRate(pChip) == 5) ? 8 : 16; // 5G(320MHz) == 8 bits/clk, 10G(640MHz) == 16 bits/clk
    uint64_t cBitsChecked      = cNClkCycles * cNBitsPerClkCycle;

    // Stop BERT
    lpGBTInterface::StartBERT(pChip, false);
    LOG(INFO) << BOLDBLUE << "\t--> Bits checked  : " << BOLDYELLOW << +cBitsChecked << RESET;
    LOG(INFO) << BOLDBLUE << "\t--> Bits in error : " << BOLDYELLOW << +cErrors << RESET;

    // Return fraction of errors
    return (double)cErrors / cBitsChecked;
}

double lpGBTInterface::RunBERtest(Chip* pChip, uint8_t pGroup, uint8_t pChannel, bool given_time, double frames_or_time, uint8_t frontendSpeed)
// ####################
// # frontendSpeed    #
// # 1.28 Gbit/s  = 0 #
// # 640 Mbit/s   = 1 #
// # 320 Mbit/s   = 2 #
// ####################
{
    const double   mainClock       = 40e6;                             // @CONST@
    const uint32_t nBitInClkPeriod = 32. * std::pow(2, frontendSpeed); // Number of bits in the 40 MHz clock period
    const double   fps             = 1.28e9 / nBitInClkPeriod;         // Frames per second
    const int      n_prints        = 10;                               // Only an indication, the real number of printouts will be driven by the length of the time steps @CONST@
    double         frames2run;
    double         time2run;

    if(given_time == true)
        time2run = frames_or_time;
    else
        time2run = frames_or_time / fps;
    uint32_t BERTMeasTime = (log2(time2run * mainClock) - 5) / 2.;
    frames2run            = fBERTMeasTimeMap[BERTMeasTime];

    // Configure number of printouts and calculate the frequency of printouts
    double time_per_step = std::min(std::max(time2run / n_prints, 1.), 3600.); // The runtime of the PRBS test will have a precision of one step (at most 1h and at least 1s)

    // ###############
    // # Configuring #
    // ###############
    lpGBTInterface::ConfigureRxSource(pChip, {pGroup}, lpGBTconstants::PATTERN_NORMAL);
    lpGBTInterface::ConfigureBERT(pChip, fGroup2BERTsourceCourse[pGroup], fChannelSpeed2BERTsourceFine[pChannel + 4 * (2 - frontendSpeed)], BERTMeasTime);

    // #########
    // # Start #
    // #########
    lpGBTInterface::StartBERT(pChip, false); // Stop
    lpGBTInterface::StartBERT(pChip, true);  // Start
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    LOG(INFO) << BOLDGREEN << "===== BER run starting =====" << std::fixed << std::setprecision(0) << RESET;
    int      idx = 1;
    uint64_t nErrors;
    while(lpGBTInterface::IsBERTDone(pChip) == false)
    {
        std::this_thread::sleep_for(std::chrono::seconds(static_cast<unsigned int>(time_per_step)));

        nErrors = lpGBTInterface::GetBERTErrors(pChip);

        LOG(INFO) << GREEN << "I've been running for " << BOLDYELLOW << time_per_step * idx << RESET << GREEN << "s" << RESET;
        LOG(INFO) << GREEN << "Current counter: " << BOLDYELLOW << nErrors / nBitInClkPeriod << RESET << GREEN << " frames with error(s), i.e. " << BOLDYELLOW << nErrors << RESET << GREEN
                  << " bits with errors" << RESET;
        idx++;
    }
    LOG(INFO) << BOLDGREEN << "========= Finished =========" << RESET;

    if(lpGBTInterface::IsBERTEmptyData(pChip) == true)
    {
        lpGBTInterface::StartBERT(pChip, false); // Stop
        throw Exception("[lpGBTInterface::RunBERtest] All zeros at input");
    }

    // ########
    // # Stop #
    // ########
    nErrors = lpGBTInterface::GetBERTErrors(pChip);
    lpGBTInterface::StartBERT(pChip, false); // Stop

    // Read PRBS frame counter
    LOG(INFO) << BOLDGREEN << "===== BER test summary =====" << RESET;
    LOG(INFO) << GREEN << "Final number of PRBS frames sent: " << BOLDYELLOW << frames2run << RESET;
    LOG(INFO) << GREEN << "Final counter: " << BOLDYELLOW << nErrors / nBitInClkPeriod << RESET << GREEN << " frames with error(s), i.e. " << BOLDYELLOW << nErrors << RESET << GREEN
              << " bits with errors" << RESET;
    LOG(INFO) << GREEN << "Final BER: " << BOLDYELLOW << nErrors / frames2run << RESET << GREEN << " bits/clk (" << BOLDYELLOW << nErrors / nBitInClkPeriod / frames2run * 100 << RESET << GREEN << "%)"
              << RESET;
    LOG(INFO) << BOLDGREEN << "====== End of summary ======" << RESET;

    return nErrors / frames2run;
}

void lpGBTInterface::StartPRBSpattern(Chip* pChip)
{
    lpGBTInterface::ConfigureRxPRBS(pChip, {lpGBTconstants::fictitiousGroup}, {lpGBTconstants::fictitiousChannel}, true);
    lpGBTInterface::ConfigureRxSource(pChip, {lpGBTconstants::fictitiousGroup}, lpGBTconstants::PATTERN_PRBS);
}

void lpGBTInterface::StopPRBSpattern(Chip* pChip)
{
    lpGBTInterface::ConfigureRxPRBS(pChip, {lpGBTconstants::fictitiousGroup}, {lpGBTconstants::fictitiousChannel}, false);
    lpGBTInterface::ConfigureRxSource(pChip, {lpGBTconstants::fictitiousGroup}, lpGBTconstants::PATTERN_NORMAL);
}

// ####################################
// # LpGBT eye opening monitor tester #
// ####################################

void lpGBTInterface::ConfigureEOM(Chip* pChip, uint8_t pEndOfCountSelect, bool pByPassPhaseInterpolator, bool pEnableEOM)
{
    WriteChipReg(pChip, "EOMConfigH", pEndOfCountSelect << 4 | pByPassPhaseInterpolator << 2 | pEnableEOM << 0);
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
}

void lpGBTInterface::StartEOM(Chip* pChip, bool pStartEOM)
{
    uint8_t cRegisterValue = ReadChipReg(pChip, "EOMConfigH");
    WriteChipReg(pChip, "EOMConfigH", (cRegisterValue & ~(0x1 << 1)) | (pStartEOM << 1));
}

void lpGBTInterface::SelectEOMPhase(Chip* pChip, uint8_t pPhase) { WriteChipReg(pChip, "EOMConfigL", pPhase); }

void lpGBTInterface::SelectEOMVof(Chip* pChip, uint8_t pVof) { WriteChipReg(pChip, "EOMvofSel", pVof); }

uint8_t lpGBTInterface::GetEOMStatus(Chip* pChip)
{
    uint8_t cEOMStatus = ReadChipReg(pChip, "EOMStatus");
    LOG(INFO) << GREEN << "Eye Opening Monitor status : " << BOLDYELLOW << lpGBTInterface::fEOMStatusMap[(cEOMStatus & (0x3 << 2)) >> 2] << RESET;
    return cEOMStatus;
}

uint16_t lpGBTInterface::GetEOMCounter(Chip* pChip) { return (ReadChipReg(pChip, "EOMCounterValueH") << 8 | ReadChipReg(pChip, "EOMCounterValueL") << 0); }

// ##############################################
// # LpGBT I2C Masters functions (Slow Control) #
// ##############################################

void lpGBTInterface::ResetI2C(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pMasters)
{
    LOG(INFO) << GREEN << "Reseting I2C Masters" << RESET;
    std::vector<uint8_t> cBitPosition = {2, 1, 0};
    uint8_t              cResetMask   = 0;

    for(const auto& cMaster: pMasters) cResetMask |= (1 << cBitPosition[cMaster]);

    WriteChipReg(pChip, "RST0", 0);
    WriteChipReg(pChip, "RST0", cResetMask);
    WriteChipReg(pChip, "RST0", 0);
}

void lpGBTInterface::ConfigureI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMaster, uint8_t pFreq, uint8_t pNBytes, uint8_t pSCLDriveMode)
{
    // First let's write configuration data into the I2C Master Data register
    std::string cI2CCntrlReg = "I2CM" + std::to_string(pMaster) + "Data0";
    uint8_t     cValueCntrl  = (pFreq << 0) | (pNBytes << 2) | (pSCLDriveMode << 7);
    WriteChipReg(pChip, cI2CCntrlReg, cValueCntrl);

    // Now let's write Command (0x00) to the Command register to tranfer Configuration to the I2C Master Control register
    std::string cI2CCmdReg = "I2CM" + std::to_string(pMaster) + "Cmd";
    WriteChipReg(pChip, cI2CCmdReg, 0x00);
}

bool lpGBTInterface::WriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMaster, uint8_t pSlaveAddress, uint32_t pData, uint8_t pNBytes, uint8_t pFreq)
{
    // Write Data to Slave Address using I2C Master
    lpGBTInterface::ConfigureI2C(pChip, pMaster, pFreq, (pNBytes > 1) ? pNBytes : 0, 0);

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

    // Wait until the transaction is done
    uint8_t cIter = 0;
    do
    {
        LOG(DEBUG) << GREEN << "Waiting for I2C Write transaction to finisih" << RESET;
        cIter++;
    } while(cIter < RD53Shared::MAXATTEMPTS && !IsI2CSuccess(pChip, pMaster));

    if(cIter == RD53Shared::MAXATTEMPTS)
    {
        LOG(INFO) << BOLDRED << "I2C Write transaction FAILED" << RESET;
#ifdef __TCUSB__
        // In the test system a run time error is undesired
        return false;
#else
        throw std::runtime_error(std::string("in D19clpGBTInterface::WriteI2C : I2C write transaction FAILED"));
#endif
    }

    return true;
}

uint32_t lpGBTInterface::ReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMaster, uint8_t pSlaveAddress, uint8_t pNBytes, uint8_t pFreq)
{
    // Read Data from Slave Address using I2C Master
    lpGBTInterface::ConfigureI2C(pChip, pMaster, pFreq, pNBytes, 0);
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
        WriteChipReg(pChip, cI2CCmdReg, 0xD);

    // Wait until the transaction is done
    uint8_t cIter = 0;
    do
    {
        LOG(DEBUG) << GREEN << "Waiting for I2C Read transaction to finisih" << RESET;
        cIter++;
    } while(cIter < RD53Shared::MAXATTEMPTS && !lpGBTInterface::IsI2CSuccess(pChip, pMaster));
    if(cIter == RD53Shared::MAXATTEMPTS)
    {
        LOG(INFO) << BOLDRED << "I2C Read Transaction FAILED" << RESET;
#ifdef __TCUSB__
        // In the test system a run time error is undesired
        return false;
#else
        throw std::runtime_error(std::string("in D19clpGBTInterface::ReadI2C : I2C Transaction failed"));
#endif
    }

    // Return read back value
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
            cReadData |= ((uint32_t)ReadChipReg(pChip, cI2CDataReg) << 8 * cByte);
        }
        return cReadData;
    }
}

uint8_t lpGBTInterface::GetI2CStatus(Ph2_HwDescription::Chip* pChip, uint8_t pMaster)
{
    std::string cI2CStatReg = "I2CM" + std::to_string(pMaster) + "Status";
    uint8_t     cStatus     = ReadChipReg(pChip, cI2CStatReg);
    LOG(DEBUG) << GREEN << "I2C Master " << +pMaster << " -- Status : " << lpGBTInterface::fI2CStatusMap[cStatus] << RESET;
    return cStatus;
}

bool lpGBTInterface::IsI2CSuccess(Ph2_HwDescription::Chip* pChip, uint8_t pMaster) { return (lpGBTInterface::GetI2CStatus(pChip, pMaster) == 4); }

} // namespace Ph2_HwInterface
