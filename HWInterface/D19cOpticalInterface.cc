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
// # Read/Write new Command Processor Block #
// #########################################
void D19cOpticalInterface::ResetCPB()
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    LOG(DEBUG) << BOLDBLUE << "Resetting CPB" << RESET;
    // Soft reset the GBT-SC worker
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    uint8_t cWorkerId = 0, cFunctionId = 2;
    // reset shoudl be 0x00020010
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | 16 << 0);
    WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", cCommandVector);
    ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", 10);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
}

void D19cOpticalInterface::WriteCommandCPB(const std::vector<uint32_t>& pCommandVector)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t cWordIndex = 0;
    for(auto cCommandWord : pCommandVector){
        LOG(DEBUG) << GREEN << "\t Write command word " << +cWordIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cCommandWord << std::dec << RESET;
        cWordIndex++;
    }
    WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", pCommandVector);
}

std::vector<uint32_t> D19cOpticalInterface::ReadReplyCPB(uint8_t pNWords)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    std::vector<uint32_t> cReplyVector = ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", pNWords);
    uint8_t               cWordIndex   = 0;
    for(auto cReplyWord : cReplyVector){
        LOG(DEBUG) << YELLOW << "\t Read reply word " << +cWordIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cReplyWord << std::dec << RESET;
        cWordIndex++;
    }
    LOG(DEBUG) << "\t lpgbtsc FSM state : 0b" << std::bitset<8>(ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state")) << RESET;
    return cReplyVector;
}

// ##########################################
// # ROC Register read/write #
// #########################################

std::vector<uint32_t> D19cOpticalInterface::EncodeCommand(uint8_t pFunctionId, Chip* pChip, ChipRegItem& pItem, bool pVerify)
{
    std::vector<uint32_t> cCommandVector;
    uint8_t cWorkerId = LpGBTSCWorker::BaseID + pChip->getOpticalId();
    uint8_t cChipId = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : (pChip->getId() % 8);
    uint8_t cChipCode = pChip->getChipCode();
    uint8_t cMasterId = pChip->getMasterId();
    switch(pFunctionId){
        case LpGBTSCWorker::SingleReadIC : 
            cCommandVector.push_back(cWorkerId << 24 | pFunctionId << 16 | pItem.fAddress << 0);
            break;
        
        case LpGBTSCWorker::SingleWriteIC : 
            cCommandVector.push_back(cWorkerId << 24 | pFunctionId << 16 | pItem.fAddress << 0);
            cCommandVector.push_back(pItem.fValue << 0);
            break;

        case LpGBTSCWorker::SingleReadFE :
            cCommandVector.push_back(cWorkerId << 24 | pFunctionId << 16 | cMasterId << 6 | cChipCode << 3 | cChipId << 0);
            cCommandVector.push_back(pItem.fAddress << 0);
            break;

        case LpGBTSCWorker::SingleWriteFE :
            cCommandVector.push_back(cWorkerId << 24 | pFunctionId << 16 | pVerify << 8 | cMasterId << 6 | cChipCode << 3 | cChipId << 0);
            cCommandVector.push_back(pItem.fValue << 16 | pItem.fAddress << 0);
            break;
        default :
            LOG(ERROR) << "D19cOpticalInterface::EncodeCommand : LpGBT-SC Worker fuction doesn't exist" << RESET;
            throw std::runtime_error("D19cOpticalInterface::EncodeCommand failure");
    }
    return cCommandVector;
}


bool D19cOpticalInterface::SingleRead(Chip* pChip, ChipRegItem& pItem)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    SelectLink(pChip->getOpticalId());
    uint8_t cFunctionId = (pChip->getFrontEndType() == FrontEndType::LpGBT) ? LpGBTSCWorker::SingleReadIC : LpGBTSCWorker::SingleReadFE;
    auto cCommand = EncodeCommand(cFunctionId, pChip, pItem);
    WriteCommandCPB(cCommand);
    uint8_t cWaitCounter = 10;
    while(!IsDone(cFunctionId) && (cWaitCounter != 0)){ 
        cWaitCounter--;
        continue;
    }
    uint8_t cTryCntr = GetTryCntr(cFunctionId);
    if(cTryCntr > 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleRead : Tried " << +cTryCntr << "/100 before success" << RESET;
    }
    if(cWaitCounter == 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleRead : Tool stuck" << RESET;
        return false;
    }
    auto cReply = ReadReplyCPB(1);
    uint8_t cErrorCode = (cReply[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0){ 
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
    auto cCommand = EncodeCommand(cFunctionId, pChip, pItem, pVerify);
    WriteCommandCPB(cCommand);
    uint8_t cWaitCounter = 10;
    while(!IsDone(cFunctionId) && (cWaitCounter != 0)){ 
        cWaitCounter--;
        continue;
    }
    uint8_t cTryCntr = GetTryCntr(cFunctionId);
    if(cTryCntr > 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWrite : Tried " << +cTryCntr << "/100 before success" << RESET;
    }
    if(cWaitCounter == 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWrite : Tool stuck" << RESET;
        return false;
    }
    auto cReply = ReadReplyCPB(1);
    uint8_t cErrorCode = (cReply[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0){ 
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWrite : Error Code is " << +cErrorCode << RESET;
        LOG(ERROR) << BOLDRED << "Chip code : " << +pChip->getChipCode() << " , register address 0x" << std::hex << +pItem.fAddress << RESET;
        return false;
    }
    uint8_t cReadBack = (cReply[0] & (0xFF << 0)) >> 0;
    if(pVerify){
        if(cReadBack != pItem.fValue){ 
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
    uint8_t cLinkId = pChip->getOpticalId();
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
    uint8_t cWorkerId = LpGBTSCWorker::BaseID + cLinkId;
    uint8_t cFunctionId = LpGBTSCWorker::MultiByteWriteI2C;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | pMasterConfig << 6);
    cCommandVector.push_back(pSlaveData << 8 | pSlaveAddress << 0);
    WriteCommandCPB(cCommandVector);
    uint8_t cWaitCounter = 10;
    while(!IsDone(cFunctionId) && (cWaitCounter != 0))
    { 
        cWaitCounter--;
        continue; 
    }
    if(cWaitCounter == 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiByteWriteI2C : I2C Tool stuck" << RESET;
        return false;
    }
    uint8_t cTryCntr = GetTryCntr(cFunctionId);
    if(cTryCntr > 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiByteWriteI2C : Tried " << +cTryCntr << "/100 before success" << RESET;
    }
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(1);
    uint8_t cErrorCode = (cReplyVector[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiByteWriteI2C : Error Code is " << +cErrorCode << RESET;
        return false;
    }
    uint8_t cStatus = (cReplyVector[0] & (0xFF << 0)) >> 0;
    if(cStatus != 4){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiByteWriteI2C : I2C Status is " << +cStatus << RESET;
    }
    return true;
}

uint8_t D19cOpticalInterface::SingleByteReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t cLinkId = pChip->getOpticalId();
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
    uint8_t cWorkerId = LpGBTSCWorker::BaseID + cLinkId;
    uint8_t cFunctionId = LpGBTSCWorker::SingleByteReadI2C;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | pMasterConfig << 6);
    cCommandVector.push_back(pSlaveAddress << 0);
    WriteCommandCPB(cCommandVector);
    uint8_t cWaitCounter = 10;
    while(!IsDone(cFunctionId) && (cWaitCounter != 0)){ 
        cWaitCounter--;
        continue; 
    }
    if(cWaitCounter == 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleByteReadI2C : I2C Tool stuck" << RESET;
        return 0;
    }
    uint8_t cTryCntr = GetTryCntr(cFunctionId);
    if(cTryCntr > 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleByteReadI2C : Tried " << +cTryCntr << "/100 before success" << RESET;
    }
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(1);
    uint8_t cErrorCode = (cReplyVector[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleByteReadI2C : Error Code is " << +cErrorCode << RESET;
        return 0;
    }
    uint8_t cReadBack = (cReplyVector[0] & (0xFF << 0)) >> 0;
    return cReadBack;
}

bool D19cOpticalInterface::IsDone(uint8_t pFunctionId)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t cStatus = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    bool cWorkerDone = false;
    bool cFunctionDone = false;
    if((pFunctionId == LpGBTSCWorker::SingleReadIC) || (pFunctionId == LpGBTSCWorker::SingleWriteIC)){
        cWorkerDone = (cStatus & 0xFF) == 1;
        cFunctionDone = ((cStatus & (0xFF << 8)) >> 8) == 1;
    }
    else if((pFunctionId == LpGBTSCWorker::SingleByteReadI2C) || (pFunctionId == LpGBTSCWorker::MultiByteWriteI2C)){
        cWorkerDone = (cStatus & 0xFF) == 1;
        cFunctionDone = ((cStatus & (0xFF << 16)) >> 16) == 1;
    }
    else if((pFunctionId == LpGBTSCWorker::SingleReadFE) || (pFunctionId == LpGBTSCWorker::SingleWriteFE)){
        cWorkerDone = (cStatus & 0xFF) == 1;
        cFunctionDone = ((cStatus & (0xFF << 24)) >> 24) == 1;
    }
    else{
        LOG(ERROR) << "D19cOpticalInterface::IsDone : LpGBT-SC Worker fuction doesn't exist" << RESET;
        throw std::runtime_error("D19cOpticalInterface::IsDone failure");
    }
    return cWorkerDone && cFunctionDone;
}

uint8_t D19cOpticalInterface::GetTryCntr(uint8_t pFunctionId)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t cAllCntr = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_try_counters");
    uint8_t cCntr = 255;
        if((pFunctionId == LpGBTSCWorker::SingleReadIC) || (pFunctionId == LpGBTSCWorker::SingleWriteIC)){
            cCntr = (cAllCntr & (0xFF << 0)) >> 0;
        }
        else if((pFunctionId == LpGBTSCWorker::SingleByteReadI2C) || (pFunctionId == LpGBTSCWorker::MultiByteWriteI2C)){
            cCntr = (cAllCntr & (0xFF << 8)) >> 8;
        }
        else if((pFunctionId == LpGBTSCWorker::SingleReadFE) || (pFunctionId == LpGBTSCWorker::SingleWriteFE)){
            cCntr = (cAllCntr & (0xFF << 16)) >> 16;
        }
        else{
            LOG(ERROR) << "D19cOpticalInterface::GetTryCntr : LpGBT-SC Worker fuction doesn't exist" << RESET;
            throw std::runtime_error("D19cOpticalInterface::GetTryCntr failure");
        }
    return cCntr;
}
} // namespace Ph2_HwInterface