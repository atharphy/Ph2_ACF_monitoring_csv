/*!
  \file                  lpGBTInterface.h
  \brief                 ImInterface to access and control the low-power Gigabit Transceiver chip
  \author                Younes Otarid
  \version               1.0
  \date                  03/03/20
  Support:               email to younes.otarid@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#include "HWInterface/lpGBTInterface.h"
#include "HWInterface/ExceptionHandler.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// ################################################
// # LpGBT chip register write and read functions #
// ################################################
bool lpGBTInterface::WriteChipReg(Chip* pChip, const std::string& pDacName, uint16_t pDacValue, bool pVerify)
{
    this->setBoard(pChip->getBeBoardId());
    auto     cBoardType       = fBoardFW->getBoardType();
    auto     cAddress         = pChip->getRegItem(pDacName).fAddress;
    uint16_t cMaxWriteAddress = (static_cast<lpGBT*>(pChip)->getVersion() == 0) ? 0x13C : 0x14F; // Setting highest write address possible (lpGBT version dependent)

    // Checking that written value isn't more than 8 bits
    if(pDacValue > 0xFF)
    {
        LOG(ERROR) << BOLDRED << "LpGBT registers are 8 bits, impossible to write " << BOLDYELLOW << pDacValue << BOLDRED << " to address " << BOLDYELLOW << cAddress << RESET;
        return false;
    }

    // Checking that register address isn't higher than highest write address
    if(cAddress > cMaxWriteAddress)
    {
        LOG(ERROR) << "LpGBT read-write registers end at 0x13C ... impossible to write to address " << BOLDYELLOW << cAddress << RESET;
        return false;
    }
    bool cSuccess = false;

    if(cBoardType != BoardType::RD53 && pChip->isOptical())
    {
        auto cRegisterMap             = pChip->getRegMap();
        cRegisterMap[pDacName].fValue = pDacValue;
        cSuccess                      = fBoardFW->SingleRegisterWrite(pChip, cRegisterMap[pDacName], pVerify);
    }
    else if(pChip->isOptical())
        cSuccess = fBoardFW->WriteOptoLinkRegister(pChip, cAddress, pDacValue, pVerify);
    // TO-DO .. figure out what to do if piGBT is used

    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "LpGBT register writing issue on Board id " << +pChip->getBeBoardId() << " OpticalGroup id" << +pChip->getOpticalGroupId() << " --- OpticalGroup will be disabled"
                  << RESET;
        ExceptionHandler::getInstance()->disableOpticalGroup(pChip->getBeBoardId(), pChip->getOpticalGroupId());
        return false;
    }

    pChip->setReg(pDacName, pDacValue);
    return cSuccess;
}

uint16_t lpGBTInterface::ReadChipReg(Chip* pChip, const std::string& pDacName)
{
    this->setBoard(pChip->getBeBoardId());
    auto cBoardType = fBoardFW->getBoardType();

    auto     cAddress = pChip->getRegItem(pDacName).fAddress;
    uint16_t cValue   = 0x00;

    if(cBoardType != BoardType::RD53 && pChip->isOptical())
    {
        auto cRegisterMap = pChip->getRegMap();
        cValue            = fBoardFW->SingleRegisterRead(pChip, cRegisterMap[pDacName]);
    }
    else if(pChip->isOptical())
    {
        cValue = fBoardFW->ReadOptoLinkRegister(pChip, cAddress);
    }

    pChip->setReg(pDacName, cValue);
    return cValue;
}

uint32_t lpGBTInterface::ReadChipID(Ph2_HwDescription::Chip* pChip, uint8_t version)
{
    if(version == 1)
    {
        uint32_t cChipID   = 0;
        uint32_t cChipID_0 = ReadChipFusedBlock(pChip, 0, 0);
        // cChipID_0          = 0x10000067;
        uint32_t cChipID_1 = ReadChipFusedBlock(pChip, 0, 4);
        // cChipID_1          = 0b10000000000000000001100111000100;
        cChipID_1          = ((cChipID_1 & 0xFFFFFFC0) >> 6) | ((cChipID_1 & 0x3f) << 26);
        uint32_t cChipID_2 = ReadChipFusedBlock(pChip, 0, 8);
        // cChipID_2          = 0b00000000000001100000000100100011;
        cChipID_2          = ((cChipID_2 & 0xFFFFF000) >> 12) | ((cChipID_2 & 0xfff) << 20);
        uint32_t cChipID_3 = ReadChipFusedBlock(pChip, 0, 12);
        // cChipID_3          = 0b00000001100000000000100011010000;
        cChipID_3          = ((cChipID_3 & 0xFFFC0000) >> 18) | ((cChipID_3 & 0x3ffff) << 14);
        uint32_t cChipID_4 = ReadChipFusedBlock(pChip, 0, 16);
        // cChipID_4          = 0b01100111000000000011010001010000;
        cChipID_4 = ((cChipID_4 & 0xFF000000) >> 24) | ((cChipID_4 & 0xffffff) << 8);
        // Majority vote for lpgbt ID
        for(int i = 0; i < 32; i++)
        {
            uint8_t cTemp = 0;
            cTemp         = ((cChipID_0 >> i) & 1) | ((cChipID_1 >> i & 1) << 1) | ((cChipID_2 >> i & 1) << 2) | ((cChipID_3 >> i & 1) << 3) | ((cChipID_4 >> i & 1) << 4);
            // LOG(INFO) << BOLDYELLOW << "cTemp 0x"<< std::hex << +cTemp << std::dec << RESET;
            // LOG(INFO) << BOLDYELLOW << "popcount 0x"<< std::hex << +__builtin_popcountll(cTemp) << std::dec << RESET;
            if(__builtin_popcountll(cTemp) > 2) { cChipID = cChipID | (1 << i); }
            else
            {
                cChipID = cChipID | (0 << i);
            }
        }
        if(cChipID == 0)
        {
            LOG(INFO) << BOLDYELLOW << "No redundant lpgbt ID, only use first register" << RESET;
            cChipID = cChipID_0;
        }
        return cChipID;
    }
    else
    {
        return 0;
    }
}

uint32_t lpGBTInterface::ReadChipFusedBlock(Ph2_HwDescription::Chip* pChip, uint8_t cFuseH, uint8_t cFuseL)
{
    WriteChipReg(pChip, "FUSEControl", 2);
    int      cReadBack = 0;
    uint32_t cResult   = 0;

    while(cReadBack != 4)
    {
        cReadBack = ReadChipReg(pChip, "FUSEStatus");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        LOG(DEBUG) << BOLDGREEN << "lpgbt FUSEStatus = " << +cReadBack << RESET;
    }
    WriteChipReg(pChip, "FUSEBlowAddH", cFuseH);
    WriteChipReg(pChip, "FUSEBlowAddL", cFuseL);

    LOG(DEBUG) << BOLDGREEN << "lpgbt FUSEBlowAddH = " << +cFuseH << RESET;
    LOG(DEBUG) << BOLDGREEN << "lpgbt FUSEBlowAddL = " << +cFuseL << RESET;

    cReadBack = ReadChipReg(pChip, "FUSEValuesA");
    cResult   = cResult | (cReadBack << 24);
    LOG(DEBUG) << BOLDGREEN << "lpgbt FUSEValuesA = " << +cReadBack << RESET;

    cReadBack = ReadChipReg(pChip, "FUSEValuesB");
    cResult   = cResult | (cReadBack << 16);
    LOG(DEBUG) << BOLDGREEN << "lpgbt FUSEValuesB = " << +cReadBack << RESET;

    cReadBack = ReadChipReg(pChip, "FUSEValuesC");
    cResult   = cResult | (cReadBack << 8);
    LOG(DEBUG) << BOLDGREEN << "lpgbt FUSEValuesC = " << +cReadBack << RESET;

    cReadBack = ReadChipReg(pChip, "FUSEValuesD");
    cResult   = cResult | (cReadBack);
    LOG(DEBUG) << BOLDGREEN << "lpgbt FUSEValuesD = " << +cReadBack << RESET;

    WriteChipReg(pChip, "FUSEControl", 0);

    return cResult;
}

bool lpGBTInterface::WriteChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& pRegVec, bool pVerify)
{
    bool writeGood = true;
    for(const auto& cReg: pRegVec) writeGood = WriteChipReg(pChip, cReg.first, cReg.second);
    return writeGood;
}

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

void lpGBTInterface::ConfigureRxAlignmentMode(Chip* pChip, const std::vector<uint8_t>& pGroups, uint8_t pTrackMode)
{
    for(const auto& cGroup: pGroups)
    {
        std::string cRXCntrlReg = "EPRX" + std::to_string(cGroup) + "Control";
        auto        cRegValue   = ReadChipReg(pChip, cRXCntrlReg);
        WriteChipReg(pChip, cRXCntrlReg, (cRegValue & 0xFC) | pTrackMode);
    }
}

uint16_t lpGBTInterface::GetRxDataRate(Chip* pChip, uint8_t pGroup)
{
    uint16_t    cChipRate   = GetChipRate(pChip);
    std::string cRXCntrlReg = "EPRX" + std::to_string(pGroup) + "Control";
    auto        cRegValue   = ReadChipReg(pChip, cRXCntrlReg);
    uint16_t    cValue      = (cRegValue & 0xC);
    return (cChipRate / 5.) * (int)cValue * (float)lpGBTconstants::ACCELERATOR_CLK / 1e6;
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
            LOG(INFO) << GREEN << "Configuring Tx Group " << BOLDYELLOW << +cGroup << RESET << GREEN << " Source to NORMAL " << RESET;
        else if(pSource == 1)
            LOG(INFO) << GREEN << "Configuring Tx Group " << BOLDYELLOW << +cGroup << RESET << GREEN << " Source to PRBS7 " << RESET;
        else if(pSource == 2)
            LOG(INFO) << GREEN << "Configuring Tx Group " << BOLDYELLOW << +cGroup << RESET << GREEN << " Source to Binary counter " << RESET;
        else if(pSource == 3)
            LOG(INFO) << GREEN << "Configuring Tx Group " << BOLDYELLOW << +cGroup << RESET << GREEN << " Source to Constant Pattern" << RESET;

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
    LOG(DEBUG) << BOLDMAGENTA << "lpGBT#" << +pChip->getId() << "Grp#" << +pGroup << " Chnl#" << +pChannel << " - phase " << +pPhase << RESET;
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

void lpGBTInterface::PhaseTrainRx(Chip* pChip, const std::vector<uint8_t>& pGroups)
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

void lpGBTInterface::ResetRxDll(Chip* pChip, const std::vector<uint8_t>& pGroups)
{
    std::string cRegName = "RST1";
    uint8_t     cValue   = 0x00;
    for(auto cGroup: pGroups) { cValue = cValue | (1 << cGroup); }
    this->WriteChipReg(pChip, "RST1", cValue);
    std::this_thread::sleep_for(std::chrono::microseconds(lpGBTconstants::DEEPSLEEP));
    this->WriteChipReg(pChip, "RST1", 0x00);
}

uint8_t lpGBTInterface::GetPhaseTap(Chip* pChip, uint8_t pGroup, uint8_t pChannel)
{
    std::string cKey = "Group" + std::to_string(pGroup) + "Channel" + std::to_string(pChannel);
    auto        cIt  = fPhaseTapMap.find(cKey);
    if(cIt != fPhaseTapMap.end()) { return cIt->second; }
    else
    {
        throw std::runtime_error(std::string("Unused Channel or Group!"));
        return 15;
    }
}

void lpGBTInterface::SetPhaseTap(Chip* pChip, uint8_t pGroup, uint8_t pChannel, uint8_t pPhase)
{
    std::string cKey = "Group" + std::to_string(pGroup) + "Channel" + std::to_string(pChannel);
    auto        cIt  = fPhaseTapMap.find(cKey);
    if(cIt != fPhaseTapMap.end()) { cIt->second = pPhase; }
    else
    {
        throw std::runtime_error(std::string("Unused Channel or Group!"));
    }
}

// ################################
// # LpGBT Block Status functions #
// ################################

bool lpGBTInterface::IsPUSMDone(Chip* pChip) { return lpGBTInterface::GetPUSMStatus(pChip) == revertedPUSMStatusMap["READY"]; }

void lpGBTInterface::PrintChipMode(Chip* pChip)
{
    uint8_t cChipMode = (ReadChipReg(pChip, "ConfigPins") & 0xF0) >> 4;
    switch(cChipMode)
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

void lpGBTInterface::ConfigureInternalMonitoring(Chip* pChip, uint8_t pEnable)
{
    WriteChipReg(pChip, "ADCMon", (pEnable == 1) ? 0x1F : 0x00);
    std::this_thread::sleep_for(std::chrono::microseconds(lpGBTconstants::DEEPSLEEP));
}

float lpGBTInterface::GetInternalTemperature(Chip* pChip)
{
    auto cVal = ReadChipReg(pChip, "ADCMon");
    // Enable reset on temperature sensor
    WriteChipReg(pChip, "ADCMon", (1 << 4 | cVal));
    std::this_thread::sleep_for(std::chrono::microseconds(lpGBTconstants::DEEPSLEEP));
    // Disable reset on temperature sensor
    WriteChipReg(pChip, "ADCMon", (0 << 4 | cVal));

    std::vector<float> cMeasurements(0);
    for(uint8_t cIndx = 0; cIndx < 10; cIndx++) { cMeasurements.push_back(ReadADC(pChip, "TEMP", "VREF/2", 0)); }
    return std::accumulate(cMeasurements.begin(), cMeasurements.end(), 0.) / cMeasurements.size();
}

float lpGBTInterface::ReadResistance(Chip* pChip, const std::string& pADC, const std::vector<uint8_t>& pCurrents, uint8_t pGain)
{
    std::vector<float> cTempVoltageReadings;
    std::vector<float> cTempCurrentValues;
    for(auto cCurrentDAC: pCurrents)
    {
        ConfigureCurrentDAC(pChip, {pADC}, cCurrentDAC);
        float              cCurrent = (0.9e-3) * cCurrentDAC / 256;
        std::vector<float> cMeasurements(0);
        for(uint8_t cIndx = 0; cIndx < 10; cIndx++)
        {
            auto cMeasurement = ReadADC(pChip, pADC, "VREF/2", pGain);
            if(cMeasurement != 1023)
            {
                LOG(DEBUG) << BOLDBLUE << "Current DAC " << cCurrentDAC << " \t... " << cMeasurement << RESET;
                cMeasurements.push_back(cMeasurement);
            }
        }
        if(cMeasurements.size() > 0)
        {
            float cMean = std::accumulate(cMeasurements.begin(), cMeasurements.end(), 0.) / cMeasurements.size();
            cTempCurrentValues.push_back(cCurrent);
            cTempVoltageReadings.push_back(cMean);
            LOG(DEBUG) << "Current of " << cCurrent << " mean voltage reading is " << cMean << " ADC units" << RESET;
        }
        else
            LOG(DEBUG) << BOLDBLUE << "\t\t Current DAC " << +cCurrentDAC << " no valid ADC readings" << RESET;
    }
    ConfigureCurrentDAC(pChip, {pADC}, 0x00);
    float cLSQResistance = (cTempVoltageReadings.size() != 0) ? getLeastSquareSlope<float>(cTempCurrentValues, cTempVoltageReadings) : -1;
    LOG(DEBUG) << BOLDBLUE << "Resistance \t... " << cLSQResistance << RESET;
    return cLSQResistance;
}

uint16_t lpGBTInterface::GetADCOffset(Chip* pChip, bool pVerbose)
{
    uint16_t cMeasurement = ReadADC(pChip, "VREF/2", "VREF/2");
    if(pVerbose) LOG(INFO) << BOLDBLUE << "Reading ADC Offset " << BOLDYELLOW << +cMeasurement << RESET;
    return cMeasurement;
}

// #################################################################
// # Implements the ADC master formula for the a basic measurement #
// # Assumes a calibrated Vref                                     #
// #################################################################
float lpGBTInterface::GetADCVoltage(Chip* pChip, const std::string& pADCInputP, uint16_t cOffset, float cGain, bool pVerbose)
{
    uint16_t cMeasurement = ReadADC(pChip, pADCInputP, "VREF/2");
    return (cMeasurement - cOffset * (1. - cGain / 2.)) / cGain / 512.;
}

float lpGBTInterface::GetADCVoltage(Chip* pChip, const std::string& pADCInputP, bool pVerbose)
{
    uint16_t cOffset = GetADCOffset(pChip, pVerbose);
    float    cGain   = GetADCGain(pChip, pVerbose);
    return GetADCVoltage(pChip, pADCInputP, cOffset, cGain, pVerbose);
}

float lpGBTInterface::GetRssiPower(Chip* pChip, const std::string& pADCInputP, float cResponsivity, bool pVerbose)
{
    uint16_t cOffset = GetADCOffset(pChip, pVerbose);
    float    cGain   = GetADCGain(pChip, pVerbose);
    return GetRssiPower(pChip, pADCInputP, cResponsivity, cOffset, cGain, pVerbose);
}

// Calculation vaild for 2S SEH v3.2 prototypes in W
// Resistor values also valid for PS ROH v2
// R1=1k; Voltage divider 680k and 1000k
// Typical responsivity 0.45-0.55 A/W
float lpGBTInterface::GetRssiPower(Chip* pChip, const std::string& pADCInputP, float cResponsivity, uint16_t cOffset, float cGain, bool pVerbose)
{
    float cAdcMeasurement = GetADCVoltage(pChip, pADCInputP, cOffset, cGain, pVerbose);
    float cCurrent        = 1. / 1000. * (2.5 - cAdcMeasurement * 1680. / 680.);
    float cResult         = cCurrent / cResponsivity;
    if(pVerbose) LOG(INFO) << BOLDBLUE << "Measured RSSI Power " << BOLDYELLOW << +cResult << BOLDBLUE << " W" << RESET;
    return cResult;
}

float lpGBTInterface::GetADCGain(Chip* pChip, bool pVerbose)
{
    float cResult;
    WriteChipReg(pChip, "ADCMon", 0);
    // Disable resistive divider, so "VDD" is actually GND
    std::this_thread::sleep_for(std::chrono::microseconds(1000));
    uint16_t cMeasurement = ReadADC(pChip, "VDD", "VREF/2");
    cResult               = ((cMeasurement * 1.) - (GetADCOffset(pChip, pVerbose) * 1.)) / 512. * 2. * -1.;
    if(pVerbose) LOG(INFO) << BOLDBLUE << "Reading ADC value " << BOLDYELLOW << +cMeasurement << RESET;
    if(pVerbose) LOG(INFO) << BOLDBLUE << "Reading ADC Gain via GND-Vref/2 " << BOLDYELLOW << +cResult << RESET;
    cMeasurement = ReadADC(pChip, "VREF/2", "VDD");
    cResult      = ((cMeasurement * 1.) - (GetADCOffset(pChip, pVerbose) * 1.)) / 512. * 2.;
    if(pVerbose) LOG(INFO) << BOLDBLUE << "Reading ADC value " << BOLDYELLOW << +cMeasurement << RESET;
    if(pVerbose) LOG(INFO) << BOLDBLUE << "Reading ADC Gain via Vref/2-GND " << BOLDYELLOW << +cResult << RESET;

    return cResult;
}

uint16_t lpGBTInterface::ReadADC(Chip* pChip, const std::string& pADCInputP, const std::string& pADCInputN, uint8_t pGain)
{
    // Read differential (converted) data on two ADC inputs
    uint8_t cADCInputP = lpGBTInterface::fADCInputMap[pADCInputP];
    uint8_t cADCInputN = lpGBTInterface::fADCInputMap[pADCInputN];

    LOG(DEBUG) << GREEN << "Reading ADC value from " << BOLDYELLOW << pADCInputP << RESET;

    // Select ADC Input
    WriteChipReg(pChip, "ADCSelect", cADCInputP << 4 | cADCInputN << 0);

    // Enable ADC Input without starting conversion
    lpGBTInterface::ConfigureADC(pChip, pGain, true, false);

    // Enable Internal VREF
    uint8_t cVrefcntrContent = ReadChipReg(pChip, "VREFCNTR");
    WriteChipReg(pChip, "VREFCNTR", 1 << 7 | (0x3f & cVrefcntrContent));
    std::this_thread::sleep_for(std::chrono::microseconds(lpGBTconstants::DEEPSLEEP));

    // Start ADC conversion
    lpGBTInterface::ConfigureADC(pChip, pGain, true, true);

    // Check conversion status
    uint8_t cIter    = 0;
    bool    cSuccess = false;
    do
    {
        LOG(DEBUG) << GREEN << "Waiting for ADC conversion to end" << RESET;

        cSuccess = lpGBTInterface::IsReadADCDone(pChip);
        cIter++;
    } while((cIter < lpGBTconstants::MAXATTEMPTS) && (cSuccess == false));
    if(cIter == lpGBTconstants::MAXATTEMPTS)
    {
        LOG(INFO) << BOLDRED << "LpGBT ADC conversion timed out on Board id " << +pChip->getBeBoardId() << " OpticalGroup id" << +pChip->getOpticalGroupId() << " --- OpticalGroup will be disabled"
                  << RESET;
        ExceptionHandler::getInstance()->disableOpticalGroup(pChip->getBeBoardId(), pChip->getOpticalGroupId());
        return 0;
    }

    // Read ADC value
    uint8_t cADCvalue1 = ReadChipReg(pChip, "ADCStatusH") & 0x3;
    uint8_t cADCvalue2 = ReadChipReg(pChip, "ADCStatusL");

    // Clear ADC conversion bit and disable ADC
    lpGBTInterface::ConfigureADC(pChip, pGain, false, false);

    // disable Internal VREF
    WriteChipReg(pChip, "VREFCNTR", 0 << 7 | (0x3f & cVrefcntrContent));

    return (cADCvalue1 << 8 | cADCvalue2);
}

bool lpGBTInterface::IsReadADCDone(Chip* pChip) { return (((ReadChipReg(pChip, "ADCStatusH") & 0x40) >> 6) == 1); }

// ########################
// # LpGBT Vref functions #
// ########################

bool lpGBTInterface::ConfigureVref(Ph2_HwDescription::Chip* pChip, uint8_t pEnable, uint8_t pCorrection)
{
    uint8_t cVal     = pEnable << 7 | (pCorrection & 0x3F);
    bool    cSuccess = WriteChipReg(pChip, "VREFCNTR", cVal);
    LOG(DEBUG) << BOLDBLUE << "VREFCNTR : 0x" << std::hex << +cVal << std::dec << RESET;
    std::this_thread::sleep_for(std::chrono::milliseconds(lpGBTconstants::SUPERDEEPSLEEP));
    return cSuccess;
}


bool lpGBTInterface::EnableInternalVref(Ph2_HwDescription::Chip* pChip, bool pEnable)
{
    return true;
}
bool lpGBTInterface::SetVrefTune(Ph2_HwDescription::Chip* pChip, uint8_t pVrefTune)
{
    uint8_t cNbits = (static_cast<lpGBT*>(pChip)->getVersion() == 0 ) ? 5 : 8 ;
    std::string cRegName = (static_cast<lpGBT*>(pChip)->getVersion() == 0 ) ? "VREFCNTR" : "VREFTUNE";
    ChipRegMask cMask;
    cMask.fBitShift = 0;
    cMask.fNbits    = cNbits;
    pChip->setRegBits(cRegName, cMask, pVrefTune);
    this->WriteChipReg(pChip,cRegName, pVrefTune);
    std::this_thread::sleep_for(std::chrono::milliseconds(lpGBTconstants::SUPERDEEPSLEEP));
    auto cVrefTune = pChip->getRegItem(cRegName).fValue; 
    return cVrefTune == pVrefTune;
}

uint8_t lpGBTInterface::GetVrefTune(Ph2_HwDescription::Chip* pChip)
{
    uint8_t cNbits = (static_cast<lpGBT*>(pChip)->getVersion() == 0 ) ? 5 : 8 ;
    std::string cRegName = (static_cast<lpGBT*>(pChip)->getVersion() == 0 ) ? "VREFCNTR" : "VREFTUNE";
    auto cVrefTune = pChip->getRegItem(cRegName).fValue;
    uint8_t mask = (0xFF >> (8 - cNbits));
    return (mask & cVrefTune);
}

float lpGBTInterface::GetVref(Ph2_HwDescription::Chip* pChip, const std::string& pADC, float pVinput)
{
    auto cGain = GetADCGain(pChip,false);
    auto cOffset = GetADCOffset(pChip,false);
    auto cADC = ReadADC(pChip, pADC); 
    return (pVinput*cGain*512)/(cADC - cOffset*(1-cGain/2.));
}

uint8_t  lpGBTInterface::TuneVref(Ph2_HwDescription::Chip* pChip)
{
    const std::string pADC = static_cast<lpGBT*>(pChip)->getTuneVrefADC();
    float pVinput = static_cast<lpGBT*>(pChip)->getTuneVrefVoltage();
    LOG (INFO) << BOLDYELLOW << "Tune Vref of lpGBT using input of " << pADC << " and " << pVinput << "V" <<RESET;
    uint8_t cNbits = (static_cast<lpGBT*>(pChip)->getVersion() == 0 ) ? 5 : 8 ;
    uint8_t cCurrentStep = (0xFF >> (8 - cNbits));  //Start value
    SetVrefTune(pChip, cCurrentStep);
    auto cVrefTune = GetVrefTune(pChip);
    float cCurrentVref = GetVref(pChip,pADC, pVinput);  //Returns a voltage. Goal: 1V
    uint16_t cPreviousStep = cCurrentStep;
    for(int iBit = cNbits -1 ; iBit >= 0; --iBit)
    {
        // flip bit 
        cCurrentStep =cPreviousStep + (1 << iBit);
        SetVrefTune(pChip, cCurrentStep);
        cVrefTune = GetVrefTune(pChip);
        cCurrentVref = GetVref(pChip,pADC, pVinput);

        // Determine if it is better or not
        if( cCurrentVref < 1. ) cPreviousStep = cCurrentStep;
        SetVrefTune(pChip, cCurrentStep);
        cVrefTune = GetVrefTune(pChip);
        cCurrentVref = GetVref(pChip,pADC, pVinput);

        if (static_cast<lpGBT*>(pChip)->getVersion() == 0 )
            LOG (INFO) << BOLDYELLOW << " Flip Bit#" << +iBit << " Tune =  " << std::bitset<5>(cVrefTune) << " Vref = " << cCurrentVref << RESET;
        else
            LOG (INFO) << BOLDYELLOW << " Flip Bit#" << +iBit << " Tune =  " << std::bitset<8>(cVrefTune) << " Vref = " << cCurrentVref << RESET;

    }
    LOG (INFO) << BOLDYELLOW << "Vref tune set to " << +cVrefTune << " - Vref = " << cCurrentVref << RESET;
    return cVrefTune;
}

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
    std::this_thread::sleep_for(std::chrono::microseconds(lpGBTconstants::DEEPSLEEP));

    // Wait for BERT to end
    while(lpGBTInterface::IsBERTDone(pChip) == false)
    {
        LOG(INFO) << BOLDBLUE << "\t--> BERT still running ... " << RESET;
        std::this_thread::sleep_for(std::chrono::microseconds(lpGBTconstants::DEEPSLEEP));
    }

    // Throw error if empty data
    if(lpGBTInterface::IsBERTEmptyData(pChip) == true)
    {
        // Stop BERT
        lpGBTInterface::StartBERT(pChip, false);
        LOG(INFO) << BOLDRED << "LpGBT BERT : All zeros at input on Board id " << +pChip->getBeBoardId() << " OpticalGroup id" << +pChip->getOpticalGroupId() << " --- OpticalGroup will be disabled"
                  << RESET;
        ExceptionHandler::getInstance()->disableOpticalGroup(pChip->getBeBoardId(), pChip->getOpticalGroupId());
        return 0.;
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
    const uint32_t nBitInClkPeriod = 32. * std::pow(2, frontendSpeed); // Number of bits in the 40 MHz clock period
    const double   fps             = 1.28e9 / nBitInClkPeriod;         // Frames per second
    const int      nPrints         = 10;                               // Only an indication, the real number of printouts will be driven by the length of the time steps @CONST@
    double         frames2run;
    double         time2run;

    if(given_time == true)
        time2run = frames_or_time;
    else
        time2run = frames_or_time / fps;
    size_t BERTMeasTime = (log2(time2run * lpGBTconstants::ACCELERATOR_CLK) - 5) / 2.;
    frames2run          = fBERTMeasTimeMap[BERTMeasTime];

    // Configure number of printouts and calculate the frequency of printouts
    double time_per_step = std::min(std::max(time2run / nPrints, 1.), 3600.); // The runtime of the PRBS test will have a precision of one step (at most 1h and at least 1s)

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
    std::this_thread::sleep_for(std::chrono::microseconds(lpGBTconstants::DEEPSLEEP));

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
        LOG(INFO) << BOLDRED << "LpGBT BERT : All zeros at input on Board id " << +pChip->getBeBoardId() << " OpticalGroup id" << +pChip->getOpticalGroupId() << " --- OpticalGroup will be disabled"
                  << RESET;
        ExceptionHandler::getInstance()->disableOpticalGroup(pChip->getBeBoardId(), pChip->getOpticalGroupId());
        return 0.;
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
    std::this_thread::sleep_for(std::chrono::microseconds(lpGBTconstants::DEEPSLEEP));
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
    LOG(DEBUG) << GREEN << "Eye Opening Monitor status : " << BOLDYELLOW << lpGBTInterface::fEOMStatusMap[(cEOMStatus & (0x3 << 2)) >> 2] << RESET;
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
    // Write configuration data into the I2C Master Data register
    std::string cI2CDataReg = "I2CM" + std::to_string(pMaster) + "Data0";
    uint8_t     cValueData  = (pFreq << 0) | (pNBytes << 2) | (pSCLDriveMode << 7);
    WriteChipReg(pChip, cI2CDataReg, cValueData);

    // Write Command (0x00) to the Command register to tranfer Configuration to the I2C Master Control register
    std::string cI2CCmdReg = "I2CM" + std::to_string(pMaster) + "Cmd";
    WriteChipReg(pChip, cI2CCmdReg, 0x00);
}

uint8_t lpGBTInterface::GetI2CConfiguration(Ph2_HwDescription::Chip* pChip, uint8_t pMaster)
{
    std::string cI2CCntrlReg = "I2CM" + std::to_string(pMaster) + "Ctrl";
    return ReadChipReg(pChip, cI2CCntrlReg);
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
    } while(cIter < lpGBTconstants::MAXATTEMPTS && !IsI2CSuccess(pChip, pMaster));

    if(cIter == lpGBTconstants::MAXATTEMPTS)
    {
        LOG(INFO) << BOLDRED << "I2C Write transaction FAILED" << RESET;
#if defined(__TCUSB__)
        // In the test system a run time error is undesired
        return false;
#else
        LOG(INFO) << BOLDRED << "LpGBT I2C write transaction FAILED on Board id " << +pChip->getBeBoardId() << " OpticalGroup id" << +pChip->getOpticalGroupId() << " --- OpticalGroup will be disabled"
                  << RESET;
        ExceptionHandler::getInstance()->disableOpticalGroup(pChip->getBeBoardId(), pChip->getOpticalGroupId());
        return false;
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
    } while(cIter < lpGBTconstants::MAXATTEMPTS && !lpGBTInterface::IsI2CSuccess(pChip, pMaster));
    if(cIter == lpGBTconstants::MAXATTEMPTS)
    {
        LOG(INFO) << BOLDRED << "I2C Read Transaction FAILED" << RESET;
#if defined(__TCUSB__)
        // In the test system a run time error is undesired
        return false;
#else
        LOG(INFO) << BOLDRED << "LpGBT I2C read transaction FAILED on Board id " << +pChip->getBeBoardId() << " OpticalGroup id" << +pChip->getOpticalGroupId() << " --- OpticalGroup will be disabled"
                  << RESET;
        ExceptionHandler::getInstance()->disableOpticalGroup(pChip->getBeBoardId(), pChip->getOpticalGroupId());
        return false;
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

std::string lpGBTInterface::GetI2CState(Ph2_HwDescription::Chip* pChip, uint8_t pStatus) { return fI2CStatusMap[pStatus]; }

bool lpGBTInterface::IsI2CSuccess(Ph2_HwDescription::Chip* pChip, uint8_t pMaster) { return (lpGBTInterface::GetI2CStatus(pChip, pMaster) == 4); }

} // namespace Ph2_HwInterface
