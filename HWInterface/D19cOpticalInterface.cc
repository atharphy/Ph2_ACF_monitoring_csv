#include "D19cOpticalInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// ##########################################
// # Constructors #
// #########################################

D19cOpticalInterface::D19cOpticalInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : FEConfigurationInterface(pId, pUri, pAddressTable)
{
    fType = ConfigurationType::IC;
}

D19cOpticalInterface::D19cOpticalInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : FEConfigurationInterface(puHalConfigFileName, pBoardId) { fType = ConfigurationType::IC; }

D19cOpticalInterface::~D19cOpticalInterface() {}
// ##########################################
// # Chip Register read/write #
// #########################################

bool D19cOpticalInterface::Read(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    flpGBTSlowControlWorkerInterface->SelectLink(pChip->getOpticalGroupId());
    uint8_t                  cFunctionId = (pChip->getFrontEndType() == FrontEndType::LpGBT) ? LpGBTSlowControlWorker::READ_IC : LpGBTSlowControlWorker::READ_FE;
    std::vector<ChipRegItem> cRegisterBlock;
    bool                     cSuccess = true;
    int                      cBlockId = 0;
    for(std::vector<ChipRegItem>::iterator cRegItemIter = pRegisterItems.begin(); cRegItemIter < pRegisterItems.end(); cRegItemIter++)
    {
        cRegisterBlock.push_back(*cRegItemIter);
        if(cRegisterBlock.size() == LpGBTSlowControlWorker::BLOCK_SIZE || cRegItemIter == pRegisterItems.end() - 1)
        {
            auto cCommand = flpGBTSlowControlWorkerInterface->EncodeCommand(cFunctionId, pChip, cRegisterBlock);
            flpGBTSlowControlWorkerInterface->WriteCommand(cCommand);
            int cWaitCounter = 100000;
            while(!flpGBTSlowControlWorkerInterface->IsDone(cFunctionId) && (cWaitCounter != 0))
            {
                cWaitCounter--;
                continue;
            }
            if(cWaitCounter == 0)
            {
                flpGBTSlowControlWorkerInterface->PrintStateFSM();
                flpGBTSlowControlWorkerInterface->Reset();
                return false;
            }
            uint8_t cTryCntr = flpGBTSlowControlWorkerInterface->GetTryCntr(cFunctionId);
            if(cTryCntr > 0)
            {
                uint8_t cMaxRetry = (pChip->getFrontEndType() == FrontEndType::LpGBT) ? fConfiguration.fMaxRetryIC : fConfiguration.fMaxRetryFE;
                LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Read : Tried " << +cTryCntr << "/" << +cMaxRetry << " before success" << RESET;
            }
            auto cReplies = flpGBTSlowControlWorkerInterface->ReadReply(cRegisterBlock.size() + 1); // N words + 1 header
            for(size_t cReplyIdx = 0; cReplyIdx < cReplies.size(); cReplyIdx++)
            {
                if(cReplyIdx == 0) continue; // skip header
                uint8_t cErrorCode = (cReplies[cReplyIdx] & (0xFF << 8)) >> 8;
                uint8_t cReadBack  = (cReplies[cReplyIdx] & (0xFF << 0)) >> 0;
                if(cErrorCode != 0)
                {
                    if(pChip->getFrontEndType() == FrontEndType::LpGBT)
                        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Read -- Error Code : " << +cErrorCode << RESET;
                    else
                        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Read -- Error Code : " << +cErrorCode << " -- I2C Status : " << +cReadBack << RESET;
                    LOG(ERROR) << BOLDRED << "Chip code : " << +pChip->getChipCode() << " -- Chip Id : " << +pChip->getId() << " -- Register address 0x" << std::hex
                               << +cRegisterBlock.at(cReplyIdx - 1).fAddress << std::dec << RESET;
                    cSuccess &= false;
                }
                pRegisterItems.at(cBlockId * LpGBTSlowControlWorker::BLOCK_SIZE + cReplyIdx - 1).fValue = cReadBack;
            }
            cRegisterBlock.clear();
            cBlockId++;
        }
    }
    return cSuccess;
}

bool D19cOpticalInterface::Write(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems, bool pVerify)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    flpGBTSlowControlWorkerInterface->SelectLink(pChip->getOpticalGroupId());
    uint8_t                  cFunctionId = (pChip->getFrontEndType() == FrontEndType::LpGBT) ? LpGBTSlowControlWorker::WRITE_IC : LpGBTSlowControlWorker::WRITE_FE;
    std::vector<ChipRegItem> cRegisterBlock;
    bool                     cSuccess = true;
    int                      cBlockId = 0;
    for(std::vector<ChipRegItem>::iterator cRegItemIter = pRegisterItems.begin(); cRegItemIter < pRegisterItems.end(); cRegItemIter++)
    {
        cRegisterBlock.push_back(*cRegItemIter);
        if(cRegisterBlock.size() == LpGBTSlowControlWorker::BLOCK_SIZE || cRegItemIter == pRegisterItems.end() - 1)
        {
            auto cCommand = flpGBTSlowControlWorkerInterface->EncodeCommand(cFunctionId, pChip, cRegisterBlock, pVerify);
            flpGBTSlowControlWorkerInterface->WriteCommand(cCommand);
            int cWaitCounter = 100000;
            while(!flpGBTSlowControlWorkerInterface->IsDone(cFunctionId) && (cWaitCounter != 0))
            {
                cWaitCounter--;
                continue;
            }
            if(cWaitCounter == 0)
            {
                flpGBTSlowControlWorkerInterface->PrintStateFSM();
                flpGBTSlowControlWorkerInterface->Reset();
                return false;
            }
            uint8_t cTryCntr = flpGBTSlowControlWorkerInterface->GetTryCntr(cFunctionId);
            if(cTryCntr > 0)
            {
                uint8_t cMaxRetry = (pChip->getFrontEndType() == FrontEndType::LpGBT) ? fConfiguration.fMaxRetryIC : fConfiguration.fMaxRetryFE;
                LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Write : Tried " << +cTryCntr << "/" << +cMaxRetry << " before success" << RESET;
            }
            auto cReplies = flpGBTSlowControlWorkerInterface->ReadReply(cRegisterBlock.size() + 1); // N words + 1 header
            for(size_t cReplyIdx = 0; cReplyIdx < cReplies.size(); cReplyIdx++)
            {
                if(cReplyIdx == 0) continue; // skip header
                uint8_t cErrorCode = (cReplies[cReplyIdx] & (0xFF << 8)) >> 8;
                uint8_t cReadBack  = (cReplies[cReplyIdx] & (0xFF << 0)) >> 0;
                if(cErrorCode != 0)
                {
                    if(pChip->getFrontEndType() == FrontEndType::LpGBT)
                        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Write -- Error Code : " << +cErrorCode << RESET;
                    else
                        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Write -- Error Code : " << +cErrorCode << " -- I2C Status : " << +cReadBack << RESET;
                    LOG(ERROR) << BOLDRED << "Chip code : " << +pChip->getChipCode() << " -- Chip Id : " << +pChip->getId() << " -- Register address 0x" << std::hex
                               << +cRegisterBlock.at(cReplyIdx - 1).fAddress << std::dec << RESET;
                    cSuccess &= false;
                }
                if(pVerify)
                {
                    if(cReadBack != pRegisterItems.at(cBlockId * LpGBTSlowControlWorker::BLOCK_SIZE + cReplyIdx - 1).fValue)
                    {
                        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Write : Wrong value read back" << RESET;
                        cSuccess &= false;
                    }
                    pRegisterItems.at(cBlockId * LpGBTSlowControlWorker::BLOCK_SIZE + cReplyIdx - 1).fValue = cRegisterBlock.at(cReplyIdx - 1).fValue;
                }
            }
            cRegisterBlock.clear();
            cBlockId++;
        }
    }
    return cSuccess;
}

bool D19cOpticalInterface::SingleWrite(Chip* pChip, ChipRegItem& pItem)
{
    pChip->UpdateModifiedRegMap(pItem);
    std::vector<ChipRegItem> cRegisterItem = {pItem};
    bool                     cSuccess      = Write(pChip, cRegisterItem, false);
    pItem.fValue                           = cRegisterItem.at(0).fValue;
    return cSuccess;
}

bool D19cOpticalInterface::SingleRead(Chip* pChip, ChipRegItem& pItem)
{
    std::vector<ChipRegItem> cRegisterItem = {pItem};
    bool                     cSuccess      = Read(pChip, cRegisterItem);
    pItem.fValue                           = cRegisterItem.at(0).fValue;
    return cSuccess;
}

// for now this is just looping over single read
bool D19cOpticalInterface::MultiRead(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems) { return Read(pChip, pRegisterItems); }

// for now this is just looping over single write
bool D19cOpticalInterface::MultiWrite(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems) { return Write(pChip, pRegisterItems, false); }

// for now this is just a write followed by a read
bool D19cOpticalInterface::SingleWriteRead(Chip* pChip, ChipRegItem& pItem)
{
    pChip->UpdateModifiedRegMap(pItem);
    std::vector<ChipRegItem> cRegisterItem = {pItem};
    bool                     cSuccess      = Write(pChip, cRegisterItem, true);
    pItem.fValue                           = cRegisterItem.at(0).fValue;
    return cSuccess;
}

// for now this is just a loop of WriteRead
bool D19cOpticalInterface::MultiWriteRead(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems) { return Write(pChip, pRegisterItems, true); }
bool D19cOpticalInterface::MultiByteWriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t                               cLinkId = pChip->getOpticalGroupId();
    flpGBTSlowControlWorkerInterface->SelectLink(cLinkId);
    uint8_t               cWorkerId   = LpGBTSlowControlWorker::BASE_ID + cLinkId;
    uint8_t               cFunctionId = LpGBTSlowControlWorker::MULTI_BYTE_WRITE_I2C;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | pMasterConfig << 6);
    cCommandVector.push_back(pSlaveData << 8 | pSlaveAddress << 0);
    flpGBTSlowControlWorkerInterface->WriteCommand(cCommandVector);
    int cWaitCounter = 1000;
    while(!flpGBTSlowControlWorkerInterface->IsDone(cFunctionId) && (cWaitCounter != 0))
    {
        cWaitCounter--;
        continue;
    }
    if(cWaitCounter == 0)
    {
        flpGBTSlowControlWorkerInterface->PrintStateFSM();
        flpGBTSlowControlWorkerInterface->Reset();
        return false;
    }
    uint8_t cTryCntr = flpGBTSlowControlWorkerInterface->GetTryCntr(cFunctionId);
    if(cTryCntr > 0) { LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiByteWriteI2C : Tried " << +cTryCntr << "/" << +fConfiguration.fMaxRetryI2C << " before success" << RESET; }
    auto    cReply     = flpGBTSlowControlWorkerInterface->ReadReply(1);
    uint8_t cErrorCode = (cReply[0] & (0xFF << 8)) >> 8;
    uint8_t cStatusI2C = (cReply[0] & (0xFF << 0)) >> 0;
    if(cErrorCode != 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiByteWriteI2C -- Error Code : " << +cErrorCode << " -- I2C Status : " << +cStatusI2C << RESET;
        return false;
    }
    return true;
}

uint8_t D19cOpticalInterface::SingleByteReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t                               cLinkId = pChip->getOpticalGroupId();
    flpGBTSlowControlWorkerInterface->SelectLink(cLinkId);
    uint8_t               cWorkerId   = LpGBTSlowControlWorker::BASE_ID + cLinkId;
    uint8_t               cFunctionId = LpGBTSlowControlWorker::SINGLE_BYTE_READ_I2C;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | pMasterConfig << 6);
    cCommandVector.push_back(pSlaveAddress << 0);
    flpGBTSlowControlWorkerInterface->WriteCommand(cCommandVector);
    int cWaitCounter = 1000;
    while(!flpGBTSlowControlWorkerInterface->IsDone(cFunctionId) && (cWaitCounter != 0))
    {
        cWaitCounter--;
        continue;
    }
    if(cWaitCounter == 0)
    {
        flpGBTSlowControlWorkerInterface->PrintStateFSM();
        flpGBTSlowControlWorkerInterface->Reset();
        return false;
    }
    uint8_t cTryCntr = flpGBTSlowControlWorkerInterface->GetTryCntr(cFunctionId);
    if(cTryCntr > 0) { LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleByteReadI2C : Tried " << +cTryCntr << "/" << +fConfiguration.fMaxRetryI2C << " before success" << RESET; }
    auto    cReply     = flpGBTSlowControlWorkerInterface->ReadReply(1);
    uint8_t cErrorCode = (cReply[0] & (0xFF << 8)) >> 8;
    uint8_t cStatusI2C = (cReply[0] & (0xFF << 0)) >> 0;
    if(cErrorCode != 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleByteReadI2C -- Error Code : " << +cErrorCode << " -- I2C Status : " << +cStatusI2C << RESET;
        return 0;
    }
    uint8_t cReadBack = (cReply[0] & (0xFF << 0)) >> 0;
    return cReadBack;
}
} // namespace Ph2_HwInterface
