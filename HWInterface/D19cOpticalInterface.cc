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

void D19cOpticalInterface::SelectLink(uint8_t pLinkId)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
}

// ##########################################
// # ROC Register read/write #
// #########################################

bool D19cOpticalInterface::SingleRead(Chip* pChip, ChipRegItem& pItem)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    SelectLink(pChip->getOpticalId());
    uint8_t cFunctionId = (pChip->getFrontEndType() == FrontEndType::LpGBT) ? LpGBTSCWorker::SingleReadIC : LpGBTSCWorker::SingleReadFE;
    auto    cCommand    = fCommandProcessorInterface->EncodeCommand(cFunctionId, pChip, pItem);
    fCommandProcessorInterface->WriteCommand(cCommand);
    uint8_t cWaitCounter = 10;
    while(!fCommandProcessorInterface->IsDone(cFunctionId) && (cWaitCounter != 0))
    {
        cWaitCounter--;
        continue;
    }
    uint8_t cTryCntr = fCommandProcessorInterface->GetTryCntr(cFunctionId);
    if(cTryCntr > 0)
    {
        uint8_t cMaxRetry = (pChip->getFrontEndType() == FrontEndType::LpGBT) ? fConfiguration.fMaxRetryIC : fConfiguration.fMaxRetryFE;
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleRead : Tried " << +cTryCntr << "/" << +cMaxRetry << " before success" << RESET;
    }
    if(cWaitCounter == 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleRead : Tool stuck" << RESET;
        return false;
    }
    auto    cReply     = fCommandProcessorInterface->ReadReply(1);
    uint8_t cErrorCode = (cReply[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleRead : Error Code is " << +cErrorCode << RESET;
        return false;
    }
    pItem.fValue = (cReply[0] & (0xFF << 0)) >> 0;
    return true;
}

bool D19cOpticalInterface::WriteChipRegister(Chip* pChip, ChipRegItem& pItem, bool pVerify)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    SelectLink(pChip->getOpticalId());
    uint8_t cFunctionId = (pChip->getFrontEndType() == FrontEndType::LpGBT) ? LpGBTSCWorker::SingleWriteIC : LpGBTSCWorker::SingleWriteFE;
    auto    cCommand    = fCommandProcessorInterface->EncodeCommand(cFunctionId, pChip, pItem, pVerify);
    fCommandProcessorInterface->WriteCommand(cCommand);
    uint8_t cWaitCounter = 10;
    while(!fCommandProcessorInterface->IsDone(cFunctionId) && (cWaitCounter != 0))
    {
        cWaitCounter--;
        continue;
    }
    if(cWaitCounter == 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWrite : Tool stuck" << RESET;
        return false;
    }
    uint8_t cTryCntr = fCommandProcessorInterface->GetTryCntr(cFunctionId);
    if(cTryCntr > 0)
    {
        uint8_t cMaxRetry = (pChip->getFrontEndType() == FrontEndType::LpGBT) ? fConfiguration.fMaxRetryIC : fConfiguration.fMaxRetryFE;
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWrite : Tried " << +cTryCntr << "/" << +cMaxRetry << " before success" << RESET;
    }
    auto    cReply     = fCommandProcessorInterface->ReadReply(1);
    uint8_t cErrorCode = (cReply[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWrite : Error Code is " << +cErrorCode << RESET;
        LOG(ERROR) << BOLDRED << "Chip code : " << +pChip->getChipCode() << " , register address 0x" << std::hex << +pItem.fAddress << RESET;
        return false;
    }
    uint8_t cReadBack = (cReply[0] & (0xFF << 0)) >> 0;
    if(pVerify)
    {
        if(cReadBack != pItem.fValue)
        {
            LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWrite : Wrong value read back" << RESET;
            return false;
        }
        pItem.fValue = cReadBack;
    }
    return true;
}

bool D19cOpticalInterface::SingleWrite(Chip* pChip, ChipRegItem& pItem)
{
    pChip->UpdateModifiedRegMap(pItem);
    return WriteChipRegister(pChip, pItem, false);
}

// for now this is just looping over single read
bool D19cOpticalInterface::MultiRead(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems)
{
    bool cSuccess = true;
    for(auto& cRegItem: pRegisterItems) { cSuccess = cSuccess && SingleRead(pChip, cRegItem); }
    return cSuccess;
}

// for now this is just looping over single write
bool D19cOpticalInterface::MultiWrite(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems)
{
    bool cSuccess = true;
    for(auto cRegItem: pRegisterItems) { cSuccess = cSuccess && SingleWrite(pChip, cRegItem); }
    return cSuccess;
}

// for now this is just a write followed by a read
bool D19cOpticalInterface::SingleWriteRead(Chip* pChip, ChipRegItem& pItem)
{
    pChip->UpdateModifiedRegMap(pItem);
    return WriteChipRegister(pChip, pItem, true);
}

// for now this is just a loop of WriteRead
bool D19cOpticalInterface::MultiWriteRead(Chip* pChip, std::vector<ChipRegItem>& pWriteRegs)
{
    bool cSuccess = true;
    for(auto cWriteReg: pWriteRegs) cSuccess = cSuccess && SingleWriteRead(pChip, cWriteReg);
    return cSuccess;
}

bool D19cOpticalInterface::MultiByteWriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t                               cLinkId = pChip->getOpticalId();
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
    uint8_t               cWorkerId   = LpGBTSCWorker::BaseID + cLinkId;
    uint8_t               cFunctionId = LpGBTSCWorker::MultiByteWriteI2C;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | pMasterConfig << 6);
    cCommandVector.push_back(pSlaveData << 8 | pSlaveAddress << 0);
    fCommandProcessorInterface->WriteCommand(cCommandVector);
    uint8_t cWaitCounter = 10;
    while(!fCommandProcessorInterface->IsDone(cFunctionId) && (cWaitCounter != 0))
    {
        cWaitCounter--;
        continue;
    }
    if(cWaitCounter == 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiByteWriteI2C : I2C Tool stuck" << RESET;
        return false;
    }
    uint8_t cTryCntr = fCommandProcessorInterface->GetTryCntr(cFunctionId);
    if(cTryCntr > 0) { LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiByteWriteI2C : Tried " << +cTryCntr << "/" << +fConfiguration.fMaxRetryI2C << " before success" << RESET; }
    auto    cReply     = fCommandProcessorInterface->ReadReply(1);
    uint8_t cErrorCode = (cReply[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiByteWriteI2C : Error Code is " << +cErrorCode << RESET;
        return false;
    }
    uint8_t cStatus = (cReply[0] & (0xFF << 0)) >> 0;
    if(cStatus != 4) { LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiByteWriteI2C : I2C Status is " << +cStatus << RESET; }
    return true;
}

uint8_t D19cOpticalInterface::SingleByteReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t                               cLinkId = pChip->getOpticalId();
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
    uint8_t               cWorkerId   = LpGBTSCWorker::BaseID + cLinkId;
    uint8_t               cFunctionId = LpGBTSCWorker::SingleByteReadI2C;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | pMasterConfig << 6);
    cCommandVector.push_back(pSlaveAddress << 0);
    fCommandProcessorInterface->WriteCommand(cCommandVector);
    uint8_t cWaitCounter = 10;
    while(!fCommandProcessorInterface->IsDone(cFunctionId) && (cWaitCounter != 0))
    {
        cWaitCounter--;
        continue;
    }
    if(cWaitCounter == 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleByteReadI2C : I2C Tool stuck" << RESET;
        return 0;
    }
    uint8_t cTryCntr = fCommandProcessorInterface->GetTryCntr(cFunctionId);
    if(cTryCntr > 0) { LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleByteReadI2C : Tried " << +cTryCntr << "/" << +fConfiguration.fMaxRetryI2C << " before success" << RESET; }
    auto    cReply     = fCommandProcessorInterface->ReadReply(1);
    uint8_t cErrorCode = (cReply[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleByteReadI2C : Error Code is " << +cErrorCode << RESET;
        return 0;
    }
    uint8_t cReadBack = (cReply[0] & (0xFF << 0)) >> 0;
    return cReadBack;
}

} // namespace Ph2_HwInterface