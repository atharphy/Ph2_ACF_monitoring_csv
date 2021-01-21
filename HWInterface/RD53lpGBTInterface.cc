/*!
  \file                  RD53lpGBTInterface.cc
  \brief                 The implementation follows the skeleton of the register map section of the lpGBT Manual
  \author                Mauro DINARDO
  \version               1.0
  \date                  03/03/20
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53lpGBTInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
bool RD53lpGBTInterface::ConfigureChip(Chip* pChip, bool pVerifLoop, uint32_t pBlockSize)
{
    this->setBoard(pChip->getBeBoardId());

    bool       writeGood   = true;
    ChipRegMap lpGBTRegMap = pChip->getRegMap();

    for(auto& cRegItem: lpGBTRegMap) writeGood &= RD53lpGBTInterface::WriteChipReg(pChip, cRegItem.first, cRegItem.second.fValue, true);

    return writeGood;
}

/*-------------------------------------------------------------------------*/
/* Read/Write lpGBT chip registers                                         */
/*-------------------------------------------------------------------------*/
bool RD53lpGBTInterface::WriteChipReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop)
{
    if(pValue > 0xFF)
    {
        LOG(ERROR) << BOLDRED << "lpGBT registers are 8 bits, impossible to write " << BOLDYELLOW << pValue << BOLDRED << " on registed " << BOLDYELLOW << pRegNode << RESET;
        return false;
    }

    this->setBoard(pChip->getBeBoardId());

    if(pChip->getRegItem(pRegNode).fAddress > 316)
    {
        LOG(ERROR) << "[RD53lpGBTInterface::WriteChipReg] Writing to a read-only register " << BOLDYELLOW << pRegNode << RESET;
        return false;
    }

    return fBoardFW->WriteOptoLinkRegister(pChip, pChip->getRegItem(pRegNode).fAddress, pValue, pVerifLoop);
}

bool RD53lpGBTInterface::WriteChipMultReg(Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& pRegVec, bool pVerifLoop)
{
    bool writeGood = true;

    for(const auto& cReg: pRegVec) writeGood = RD53lpGBTInterface::WriteChipReg(pChip, cReg.first, cReg.second);

    return writeGood;
}

uint16_t RD53lpGBTInterface::ReadChipReg(Chip* pChip, const std::string& pRegNode)
{
    this->setBoard(pChip->getBeBoardId());
    return fBoardFW->ReadOptoLinkRegister(pChip, pChip->getRegItem(pRegNode).fAddress);
}

/*-------------------------------------------------------------------------*/
/* lpGBT configuration functions                                           */
/*-------------------------------------------------------------------------*/
void RD53lpGBTInterface::InitialiseLinks(std::vector<uint8_t>& pULGroups, std::vector<uint8_t>& pULChannels, std::vector<uint8_t>& pDLGroups, std::vector<uint8_t>& pDLChannels)
{
    fULGroups   = std::move(pULGroups);
    fULChannels = std::move(pULChannels);

    fDLGroups   = std::move(pDLGroups);
    fDLChannels = std::move(pDLChannels);
}

void RD53lpGBTInterface::SetTxRxPolarity(Ph2_HwDescription::Chip* pChip, uint8_t pTxPolarity, uint8_t pRxPolarity)
{
    uint8_t cPolarity = (pTxPolarity << 7 | pRxPolarity << 6);
    RD53lpGBTInterface::WriteChipReg(pChip, "ChipConfig", cPolarity);
}

bool RD53lpGBTInterface::IslpGBTready(Chip* pChip) { return (RD53lpGBTInterface::ReadChipReg(pChip, "PUSMStatus") == 0x12); }

void RD53lpGBTInterface::ConfigureDownLinks(Chip* pChip, uint8_t pCurrent, uint8_t pPreEmphasis, bool pInvert)
{
    this->setBoard(pChip->getBeBoardId());

    // Configure EPTXDataRate
    uint32_t cValueDataRate = 0;
    for(const auto& cGroup: fDLGroups) cValueDataRate = (cValueDataRate & ~(0x03 << 2 * cGroup)) | (2 << 2 * cGroup);
    fBoardFW->WriteOptoLinkRegister(pChip, 0x0A7, cValueDataRate);

    // Configure EPTXEnable
    for(const auto& cGroup: fDLGroups)
    {
        uint32_t cValueEnableTx = fBoardFW->ReadOptoLinkRegister(pChip, 0x0A9 + (cGroup / 2));
        LOG(INFO) << BOLDBLUE << "Enabling ePort Tx channels for group " << +cGroup << RESET;
        for(const auto& cChannel: fDLChannels)
        {
            LOG(INFO) << BOLDBLUE << "Enabling ePort Tx channel " << +cChannel << RESET;
            cValueEnableTx += (1 << (cChannel + 4 * (cGroup % 2)));
        }
        fBoardFW->WriteOptoLinkRegister(pChip, 0x0A9 + (cGroup / 2), cValueEnableTx);
    }

    // Configure EPTXChnCntr
    for(const auto& cGroup: fDLGroups)
        for(const auto& cChannel: fDLChannels)
        {
            uint32_t cValueChnCntr = 0;
            cValueChnCntr |= (pPreEmphasis << 5) | (3 << 3) | (pCurrent << 0);
            fBoardFW->WriteOptoLinkRegister(pChip, 0x0AC + (4 * cGroup) + cChannel, cValueChnCntr);
        }

    // Configure EPTXChn_Cntr
    for(const auto& cGroup: fDLGroups)
        for(const auto& cChannel: fDLChannels)
        {
            uint32_t cValue_ChnCntr = fBoardFW->ReadOptoLinkRegister(pChip, 0x0BC + (2 * cGroup) + (cChannel / 2));
            cValue_ChnCntr |= (pInvert << (3 + 4 * (cChannel % 2))) | (0 << (0 + 4 * (cChannel % 2)));
            fBoardFW->WriteOptoLinkRegister(pChip, 0x0BC + (2 * cGroup) + (cChannel / 2), cValue_ChnCntr);
        }
}

void RD53lpGBTInterface::DisableDownLinks(Chip* pChip, const std::vector<uint8_t>& pGroups)
{
    this->setBoard(pChip->getBeBoardId());

    // Configure EPTXDataRate
    uint32_t cValueDataRate = 0;
    for(const auto& cGroup: pGroups) cValueDataRate = (cValueDataRate & ~(0x03 << 2 * cGroup)) | (0 << 2 * cGroup);
    fBoardFW->WriteOptoLinkRegister(pChip, 0x0A7, cValueDataRate);
}

void RD53lpGBTInterface::ConfigureUpLinks(Chip* pChip, uint8_t pDataRate, uint8_t pPhaseMode, uint8_t pEqual, uint8_t pPhase, bool pEnableTerm, bool pEnableBias, bool pInvert)
{
    // Configure EPRXControl
    for(const auto& cGroup: fULGroups)
    {
        uint32_t cValueEnableRx = 0;
        for(const auto& cChannel: fULChannels) cValueEnableRx += (1 << (cChannel + 4));
        uint32_t cValueEPRxControl = (cValueEnableRx << 4) | (pDataRate << 2) | (pPhaseMode << 0);
        char     cBuffer[12];
        sprintf(cBuffer, "EPRX%iControl", cGroup);
        std::string cRegName(cBuffer, sizeof(cBuffer));
        RD53lpGBTInterface::WriteChipReg(pChip, cRegName, cValueEPRxControl);
    }

    // Configure EPRXChnCntr
    for(const auto& cGroup: fULGroups)
        for(const auto& cChannel: fULChannels)
        {
            uint32_t cValueChnCntr = (pPhase << 4) | (pInvert << 3) | (pEnableBias << 2) | (pEnableTerm << 1) | (pEqual << 0);
            char     cBuffer[12];
            sprintf(cBuffer, "EPRX%i%iChnCntr", cGroup, cChannel);
            std::string cRegName(cBuffer, sizeof(cBuffer));
            RD53lpGBTInterface::WriteChipReg(pChip, cRegName, cValueChnCntr);
        }
}

void RD53lpGBTInterface::DisableUpLinks(Chip* pChip, const std::vector<uint8_t>& pGroups)
{
    // Configure EPRXControl
    for(const auto& cGroup: fULGroups)
    {
        uint32_t cValueEPRxControl = (0 << 2) | (0 << 0);
        char     cBuffer[12];
        sprintf(cBuffer, "EPRX%iControl", cGroup);
        std::string cRegName(cBuffer, sizeof(cBuffer));
        RD53lpGBTInterface::WriteChipReg(pChip, cRegName, cValueEPRxControl);
    }
}
} // namespace Ph2_HwInterface
