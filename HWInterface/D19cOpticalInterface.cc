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

bool D19cOpticalInterface::SingleReadIC(Chip* pChip, ChipRegItem& pItem)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    if(pItem.fControlReg == 0x00)
    {
        auto cRegMap = pChip->getRegMap();
        auto cIterator    = find_if(cRegMap.begin(), cRegMap.end(), [&pItem](const ChipRegPair& obj) { return obj.second.fAddress == pItem.fAddress && obj.second.fPage == pItem.fPage; });
        if( cIterator != cRegMap.end() ){ 
            LOG(DEBUG) << BOLDYELLOW << "D19cOpticalInterface::SingleReadIC to " << cIterator->first << RESET;
        }
        uint8_t cLinkId = pChip->getOpticalId();
        WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
        uint8_t cWorkerId = fLpGBTSCWorkerInfo.BaseID + cLinkId;
        uint8_t cFunctionId = fLpGBTSCWorkerInfo.SingleReadIC;
        std::vector<uint32_t> cCommandVector;
        cCommandVector.clear();
        cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pItem.fAddress << 0);
        WriteCommandCPB(cCommandVector);
        uint8_t cWaitCounter = 10;
        while(!IsICToolDone() && (cWaitCounter != 0)){ 
            cWaitCounter--;
            continue;
        }
        if(cWaitCounter == 0){
            LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleReadIC : [ERROR] IC Tool stuck" << RESET;
            return false;
        }
        std::vector<uint32_t> cReplyVector     = ReadReplyCPB(1);
        uint8_t cErrorCode = (cReplyVector[0] & (0xFF << 8)) >> 8;
        if(cErrorCode != 0){ 
            LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleReadIC : [ERROR] Error Code is " << +cErrorCode << RESET;
            return false;
        }
        pItem.fValue = (cReplyVector[0] & (0xFF << 0)) >> 0;
    }
    return true;
}

bool D19cOpticalInterface::SingleWriteIC(Chip* pChip, ChipRegItem& pItem, bool pVerify)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    auto cRegMap = pChip->getRegMap();
    auto cIterator    = find_if(cRegMap.begin(), cRegMap.end(), [&pItem](const ChipRegPair& obj) { return obj.second.fAddress == pItem.fAddress && obj.second.fPage == pItem.fPage; });
    if( cIterator != cRegMap.end() ){ 
        LOG(DEBUG) << BOLDYELLOW << "D19cOpticalInterface::SingleWriteIC to " << cIterator->first << RESET;
    }
    uint8_t cLinkId = pChip->getOpticalId();
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
    uint8_t cWorkerId = fLpGBTSCWorkerInfo.BaseID + cLinkId;
    uint8_t cFunctionId = fLpGBTSCWorkerInfo.SingleWriteIC;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pItem.fAddress << 0);
    cCommandVector.push_back(pItem.fValue << 0);
    WriteCommandCPB(cCommandVector);
    uint8_t cWaitCounter = 10;
    while(!IsICToolDone() && (cWaitCounter != 0)){ 
        cWaitCounter--;
        continue; 
    }
    if(cWaitCounter == 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWriteIC : [ERROR] IC Tool stuck" << RESET;
        return false;
    }
    std::vector<uint32_t> cReplyVector     = ReadReplyCPB(1);
    uint8_t cErrorCode = (cReplyVector[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0){ 
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWriteIC : [ERROR] Error Code is " << +cErrorCode << RESET; 
        return false;
    }
    uint8_t cReadBack = (cReplyVector[0] & (0xFF << 0)) >> 0;
    if(pVerify){
        if(cReadBack != pItem.fValue){ 
            LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWriteIC : [ERROR] Wrong value read back" << RESET; 
            return false;
        }
    }
    pItem.fValue = cReadBack;
    return true;
}

bool D19cOpticalInterface::IsICToolDone()
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t cStatus = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    bool cWorkerDone = (cStatus & 0xFF) == 1;
    bool cICToolDone = ((cStatus & (0xFF << 8)) >> 8) == 1;
    return cWorkerDone && cICToolDone;
}

bool D19cOpticalInterface::SingleReadSlave(Chip* pChip, ChipRegItem& pItem)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t cLinkId = pChip->getOpticalId();
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
    uint8_t cWorkerId = fLpGBTSCWorkerInfo.BaseID + cLinkId;
    uint8_t cFunctionId = fLpGBTSCWorkerInfo.SingleReadFE;
    uint8_t cMasterId = pChip->getMasterId();
    uint8_t cChipCode = fChipCodeMap[pChip->getFrontEndType()];
    uint8_t cChipId = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : (pChip->getId() % 8); //use modulo 8 to accomodate for how MPAs are numbered
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | cMasterId << 6 | cChipCode << 3 | cChipId << 0);
    cCommandVector.push_back(pItem.fAddress << 0);
    WriteCommandCPB(cCommandVector);
    uint8_t cWaitCounter = 10;
    while(!IsFEToolDone() && (cWaitCounter != 0)){ 
        cWaitCounter--;
        continue; 
    }
    if(cWaitCounter == 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleReadSlave : [ERROR] IC Tool stuck" << RESET;
        return false;
    }
    std::vector<uint32_t> cReplyVector     = ReadReplyCPB(1);
    uint8_t cErrorCode = (cReplyVector[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0){ 
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleReadSlave : [ERROR] Error Code is " << +cErrorCode << RESET;
        return false;
    }
    pItem.fValue = (cReplyVector[0] & (0xFF << 0)) >> 0;
    return true;
}

bool D19cOpticalInterface::SingleWriteSlave(Chip* pChip, ChipRegItem& pItem, bool pVerify)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t cLinkId = pChip->getOpticalId();
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
    uint8_t cWorkerId = fLpGBTSCWorkerInfo.BaseID + cLinkId;
    uint8_t cFunctionId = fLpGBTSCWorkerInfo.SingleWriteFE;
    uint8_t cMasterId = pChip->getMasterId();
    uint8_t cChipCode = fChipCodeMap[pChip->getFrontEndType()];
    uint8_t cChipId = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : (pChip->getId() % 8); //use modulo 8 to accomodate for how MPAs are numbered
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pVerify << 8 | cMasterId << 6 | cChipCode << 3 | cChipId << 0);
    cCommandVector.push_back(pItem.fValue << 16 | pItem.fAddress << 0);
    WriteCommandCPB(cCommandVector);
    uint8_t cWaitCounter = 10;
    while(!IsFEToolDone() && (cWaitCounter != 0)){ 
        cWaitCounter--;
        continue; 
    }
    if(cWaitCounter == 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWriteSlave : [ERROR] IC Tool stuck" << RESET;
        return false;
    }
    std::vector<uint32_t> cReplyVector     = ReadReplyCPB(1);
    uint8_t cErrorCode = (cReplyVector[0] & (0xFF << 8)) >> 8;
    uint8_t cReadBack = (cReplyVector[0] & (0xFF << 0)) >> 0;
    if(cErrorCode != 0){ 
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWriteSlave : [ERROR] Error Code is " << +cErrorCode << RESET;
        LOG(ERROR) << BOLDRED << "register address 0x" << std::hex << +pItem.fAddress << " , write value 0x" << std::hex << pItem.fValue << " , read value 0x" << std::hex << +cReadBack << RESET;
        return false;
    }
    if(pVerify){
        pItem.fValue = cReadBack;
        if(cReadBack != pItem.fValue){ 
            LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWriteSlave : [ERROR] Wrong value read back" << RESET; 
            return false;
        }
    }
    return true;
}

bool D19cOpticalInterface::IsFEToolDone()
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t cStatus = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    bool cWorkerDone = (cStatus & 0xFF) == 1;
    bool cFEToolDone = ((cStatus & (0xFF << 24)) >> 24) == 1;
    return cWorkerDone && cFEToolDone;
}

bool D19cOpticalInterface::SingleRead(Chip* pChip, ChipRegItem& pItem)
{
    if(pChip->getFrontEndType() == FrontEndType::LpGBT)
        return SingleReadIC(pChip, pItem);
    else
        return SingleReadSlave(pChip, pItem);
}

bool D19cOpticalInterface::SingleWrite(Chip* pChip, ChipRegItem& pItem)
{
    pChip->UpdateModifiedRegMap(pItem);
    if(pChip->getFrontEndType() == FrontEndType::LpGBT)
        return SingleWriteIC(pChip, pItem, false);
    else
        return SingleWriteSlave(pChip, pItem, false);
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
    if(pChip->getFrontEndType() == FrontEndType::LpGBT)
        return SingleWriteIC(pChip, pItem, true);
    else
        return SingleWriteSlave(pChip, pItem, true);

}

// for now this is just a loop of WriteRead
bool D19cOpticalInterface::MultiWriteRead(Chip* pChip, std::vector<ChipRegItem>& pWriteRegs)
{
    bool cSuccess = true;
    for(auto cWriteReg: pWriteRegs) cSuccess = cSuccess && SingleWriteRead(pChip, cWriteReg);
    return cSuccess;
}

bool D19cOpticalInterface::MultiWriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t cLinkId = pChip->getOpticalId();
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
    uint8_t cWorkerId = fLpGBTSCWorkerInfo.BaseID + cLinkId;
    uint8_t cFunctionId = fLpGBTSCWorkerInfo.MultiWriteI2C;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | pMasterConfig << 6);
    cCommandVector.push_back(pSlaveData << 8 | pSlaveAddress << 0);
    WriteCommandCPB(cCommandVector);
    uint8_t cWaitCounter = 10;
    while(!IsI2CToolDone() && (cWaitCounter != 0))
    { 
        cWaitCounter--;
        continue; 
    }
    if(cWaitCounter == 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiI2CWrite : [ERROR] I2C Tool stuck" << RESET;
        return false;
    }
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(1);
    uint8_t cErrorCode = (cReplyVector[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiI2CWrite : [ERROR] Error Code is " << +cErrorCode << RESET;
        return false;
    }
    uint8_t cStatus = (cReplyVector[0] & (0xFF << 0)) >> 0;
    if(cStatus != 4){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::MultiI2CWrite : [ERROR] I2C Status is " << +cStatus << RESET;
    }
    return true;
}

uint8_t D19cOpticalInterface::SingleReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t cLinkId = pChip->getOpticalId();
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
    uint8_t cWorkerId = fLpGBTSCWorkerInfo.BaseID + cLinkId;
    uint8_t cFunctionId = fLpGBTSCWorkerInfo.SingleReadI2C;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | pMasterConfig << 6);
    cCommandVector.push_back(pSlaveAddress << 0);
    WriteCommandCPB(cCommandVector);
    uint8_t cWaitCounter = 10;
    while(!IsI2CToolDone() && (cWaitCounter != 0))
    { 
        cWaitCounter--;
        continue; 
    }
    if(cWaitCounter == 0){
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleI2CRead : [ERROR] I2C Tool stuck" << RESET;
        return 0;
    }
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(1);
    uint8_t cErrorCode = (cReplyVector[0] & (0xFF << 8)) >> 8;
    if(cErrorCode != 0)
    {
        LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleI2CRead : [ERROR] Error Code is " << +cErrorCode << RESET;
        return 0;
    }
    uint8_t cReadBack = (cReplyVector[0] & (0xFF << 0)) >> 0;
    return cReadBack;
}

bool D19cOpticalInterface::IsI2CToolDone()
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t cStatus = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    bool cWorkerDone = (cStatus & 0xFF) == 1;
    bool cI2CToolDone = ((cStatus & (0xFF << 16)) >> 16) == 1;
    return cWorkerDone && cI2CToolDone;
}


} // namespace Ph2_HwInterface