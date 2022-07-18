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
//
bool D19cOpticalInterface::SingleRegisterRead(Chip* pChip, ChipRegItem& pRegisterItem)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    flpGBTSlowControlWorkerInterface->SelectLink(pChip->getOpticalGroupId());
    uint8_t                  cFunctionId = (pChip->getFrontEndType() == FrontEndType::LpGBT) ? LpGBTSlowControlWorker::READ_IC : LpGBTSlowControlWorker::READ_FE;
    bool                     cSuccess = true;
    auto cCommand = flpGBTSlowControlWorkerInterface->EncodeCommand(cFunctionId, pChip, {pRegisterItem});
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
    auto cReplies = flpGBTSlowControlWorkerInterface->ReadReply(1 + 1); // 1 header + 1 word
    uint8_t cErrorCode = (cReplies[1] & (0xFF << 8)) >> 8;
    uint8_t cReadBack  = (cReplies[1] & (0xFF << 0)) >> 0;
    if(cErrorCode != 0)
    {
        if(pChip->getFrontEndType() == FrontEndType::LpGBT)
            LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Read -- Error Code : " << +cErrorCode << RESET;
        else
            LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Read -- Error Code : " << +cErrorCode << " -- I2C Status : " << +cReadBack << RESET;
        LOG(ERROR) << BOLDRED << "Chip code : " << +pChip->getChipCode() << " -- Chip Id : " << +pChip->getId() << " -- Register address 0x" << std::hex
                   << +pRegisterItem.fAddress << std::dec << RESET;
        cSuccess &= false;
    }
    pRegisterItem.fValue = cReadBack;
    return cSuccess;
}

bool D19cOpticalInterface::MultiRegisterRead(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems)
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

bool D19cOpticalInterface::SingleRegisterWrite(Chip* pChip, ChipRegItem& pRegisterItem, bool pVerify)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    flpGBTSlowControlWorkerInterface->SelectLink(pChip->getOpticalGroupId());
    uint8_t                  cFunctionId = (pChip->getFrontEndType() == FrontEndType::LpGBT) ? LpGBTSlowControlWorker::WRITE_IC : LpGBTSlowControlWorker::WRITE_FE;
    bool                     cSuccess = true;
    auto cCommand = flpGBTSlowControlWorkerInterface->EncodeCommand(cFunctionId, pChip, {pRegisterItem}, pVerify);
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
    auto cReplies = flpGBTSlowControlWorkerInterface->ReadReply(1 + 1); // 1 header + 1 word
    uint8_t cErrorCode = (cReplies[1] & (0xFF << 8)) >> 8;
    uint8_t cReadBack  = (cReplies[1] & (0xFF << 0)) >> 0;
    if(cErrorCode != 0)
    {
        if(pChip->getFrontEndType() == FrontEndType::LpGBT)
            LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Write -- Error Code : " << +cErrorCode << RESET;
        else
            LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Write -- Error Code : " << +cErrorCode << " -- I2C Status : " << +cReadBack << RESET;
        LOG(ERROR) << BOLDRED << "Chip code : " << +pChip->getChipCode() << " -- Chip Id : " << +pChip->getId() << " -- Register address 0x" << std::hex
                   << +pRegisterItem.fAddress << std::dec << RESET;
        cSuccess &= false;
    }
    if(pVerify)
    {
        if(cReadBack != pRegisterItem.fValue)
        {
            LOG(ERROR) << BOLDRED << "D19cOpticalInterface::Write : Wrong value read back" << RESET;
            cSuccess &= false;
        }
        pRegisterItem.fValue = cReadBack;
    }
    return cSuccess;
}


bool D19cOpticalInterface::MultiRegisterWrite(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems, bool pVerify)
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

bool D19cOpticalInterface::SingleWrite(Chip* pChip, ChipRegItem& pRegisterItem)
{
    pChip->UpdateModifiedRegMap(pRegisterItem);
    bool                     cSuccess      = SingleRegisterWrite(pChip, pRegisterItem, false);
    return cSuccess;
}

bool D19cOpticalInterface::SingleRead(Chip* pChip, ChipRegItem& pRegisterItem)
{
    bool                     cSuccess      = SingleRegisterRead(pChip, pRegisterItem);
    return cSuccess;
}

bool D19cOpticalInterface::MultiRead(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems) { return MultiRegisterRead(pChip, pRegisterItems); }

bool D19cOpticalInterface::MultiWrite(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems) { return MultiRegisterWrite(pChip, pRegisterItems, false); }

bool D19cOpticalInterface::SingleWriteRead(Chip* pChip, ChipRegItem& pRegisterItem)
{
    bool                     cSuccess      = SingleRegisterWrite(pChip, pRegisterItem, true);
    return cSuccess;
}

bool D19cOpticalInterface::MultiWriteRead(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems) { return MultiRegisterWrite(pChip, pRegisterItems, true); }

bool D19cOpticalInterface::MultiByteWriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t                               cLinkId = pChip->getOpticalGroupId();
    flpGBTSlowControlWorkerInterface->SelectLink(cLinkId);
    uint8_t               cWorkerId   = LpGBTSlowControlWorker::BASE_ID + cLinkId;
    uint8_t               cFunctionId = LpGBTSlowControlWorker::MULTI_BYTE_WRITE_I2C;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | 1 << 0); //for now keep Nwords = 1
    cCommandVector.push_back(pMasterId << 30 | pMasterConfig << 22);
    cCommandVector.push_back(pSlaveAddress << 24 | pSlaveData << 0);
    flpGBTSlowControlWorkerInterface->WriteCommand(cCommandVector);
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
    if(cTryCntr > 0) { LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiByteWriteI2C : Tried " << +cTryCntr << "/" << +fConfiguration.fMaxRetryI2C << " before success" << RESET; }
    auto    cReply     = flpGBTSlowControlWorkerInterface->ReadReply(1 + 1); //1 header + 1 word
    uint8_t cErrorCode = (cReply[1] & (0xFF << 8)) >> 8;
    uint8_t cStatusI2C = (cReply[1] & (0xFF << 0)) >> 0;
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
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | 1 << 0); //for now keep Nwords = 1
    cCommandVector.push_back(pMasterId << 30 | pMasterConfig << 22);
    cCommandVector.push_back(pSlaveAddress << 24);   
    flpGBTSlowControlWorkerInterface->WriteCommand(cCommandVector);
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
    if(cTryCntr > 0) { LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleByteReadI2C : Tried " << +cTryCntr << "/" << +fConfiguration.fMaxRetryI2C << " before success" << RESET; }
    auto    cReply     = flpGBTSlowControlWorkerInterface->ReadReply(1 + 1); //1 header + 1 word
    uint8_t cErrorCode = (cReply[1] & (0xFF << 8)) >> 8;
    uint8_t cReadBack = (cReply[1] & (0xFF << 0)) >> 0;
    if(cErrorCode != 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleByteReadI2C -- Error Code : " << +cErrorCode << " -- I2C Status : " << +cReadBack << RESET;
        return 0;
    }
    return cReadBack;
}
} // namespace Ph2_HwInterface
