#include "D19cCommandProcessorInterface.h"

namespace Ph2_HwInterface
{
D19cCommandProcessorInterface::D19cCommandProcessorInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : CommandProcessorInterface(pId, pUri, pAddressTable)
{
    LOG(INFO) << BOLDYELLOW << "D19cCommandProcessorInterface::D19cCommandProcessorInterface Constructor" << RESET;
}

D19cCommandProcessorInterface::D19cCommandProcessorInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : CommandProcessorInterface(puHalConfigFileName, pBoardId)
{
    LOG(INFO) << BOLDYELLOW << "D19cCommandProcessorInterface::D19cCommandProcessorInterface Constructor" << RESET;
}

D19cCommandProcessorInterface::~D19cCommandProcessorInterface() {}

void D19cCommandProcessorInterface::Reset()
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    LOG(DEBUG) << BOLDBLUE << "Resetting Command Processor" << RESET;
    // Soft reset the GBT-SC worker
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    uint8_t cWorkerId = 0, cFunctionId = 2;
    // reset shoudl be 0x00020010
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | 16 << 0);
    WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", cCommandVector);
    ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", 10);
}

void D19cCommandProcessorInterface::WriteCommand(const std::vector<uint32_t>& pCommand)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t                               cWordIndex = 0;
    for(auto cWord: pCommand)
    {
        LOG(DEBUG) << GREEN << "\t Write command word " << +cWordIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cWord << RESET;
        cWordIndex++;
    }
    WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", pCommand);
}

std::vector<uint32_t> D19cCommandProcessorInterface::ReadReply(uint8_t pNWords)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    std::vector<uint32_t>                 cReply     = ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", pNWords);
    uint8_t                               cWordIndex = 0;
    for(auto cWord: cReply)
    {
        LOG(DEBUG) << YELLOW << "\t Read reply word " << +cWordIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cWord << RESET;
        cWordIndex++;
    }
    LOG(DEBUG) << "\t lpgbtsc FSM state : 0b" << std::bitset<8>(ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state")) << RESET;
    return cReply;
}

std::vector<uint32_t> D19cCommandProcessorInterface::EncodeCommand(uint8_t pFunctionId, Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem, bool pVerify)
{
    std::vector<uint32_t> cCommand;
    uint8_t               cWorkerId = LpGBTSCWorker::BaseID + pChip->getOpticalGroupId();
    uint8_t               cChipId   = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : (pChip->getId() % 8);
    uint8_t               cChipCode = pChip->getChipCode();
    uint8_t               cMasterId = pChip->getMasterId();
    switch(pFunctionId)
    {
    case LpGBTSCWorker::SingleReadIC: cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | pItem.fAddress << 0); break;

    case LpGBTSCWorker::SingleWriteIC:
        cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | pItem.fAddress << 0);
        cCommand.push_back(pItem.fValue << 0);
        break;

    case LpGBTSCWorker::SingleReadFE:
        cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | cMasterId << 6 | cChipCode << 3 | cChipId << 0);
        cCommand.push_back(pItem.fAddress << 0);
        break;

    case LpGBTSCWorker::SingleWriteFE:
        cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | pVerify << 8 | cMasterId << 6 | cChipCode << 3 | cChipId << 0);
        cCommand.push_back(pItem.fValue << 16 | pItem.fAddress << 0);
        break;
    default:
        LOG(ERROR) << "D19cCommandProcessorInterface::EncodeCommand : LpGBT-SC Worker fuction doesn't exist" << RESET;
        throw std::runtime_error("D19cCommandProcessorInterface::EncodeCommand failure");
    }
    return cCommand;
}

uint16_t D19cCommandProcessorInterface::GetStateFSM(uint8_t pFunctionId)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t                              cStatus        = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    uint16_t                              cWorkerState   = cStatus & 0xFF;
    uint16_t                              cFunctionState = 0;
    if((pFunctionId == LpGBTSCWorker::SingleReadIC) || (pFunctionId == LpGBTSCWorker::SingleWriteIC)) { cFunctionState = (cStatus & (0xFF << 8)) >> 8; }
    else if((pFunctionId == LpGBTSCWorker::SingleByteReadI2C) || (pFunctionId == LpGBTSCWorker::MultiByteWriteI2C))
    {
        cFunctionState = (cStatus & (0xFF << 16)) >> 16;
    }
    else if((pFunctionId == LpGBTSCWorker::SingleReadFE) || (pFunctionId == LpGBTSCWorker::SingleWriteFE))
    {
        cFunctionState = (cStatus & (0xFF << 24)) >> 24;
    }
    else
    {
        LOG(ERROR) << "D19cCommandProcessorInterface::GetStateFSM : LpGBT-SC Worker fuction doesn't exist" << RESET;
        throw std::runtime_error("D19cCommandProcessorInterface::GetStateFSM failure");
    }
    return ((cFunctionState << 8) | (cWorkerState << 0));
}

bool D19cCommandProcessorInterface::IsDone(uint8_t pFunctionId)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t                              cStatus       = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    bool                                  cWorkerDone   = false;
    bool                                  cFunctionDone = false;
    if((pFunctionId == LpGBTSCWorker::SingleReadIC) || (pFunctionId == LpGBTSCWorker::SingleWriteIC))
    {
        cWorkerDone   = (cStatus & 0xFF) == 1;
        cFunctionDone = ((cStatus & (0xFF << 8)) >> 8) == 1;
    }
    else if((pFunctionId == LpGBTSCWorker::SingleByteReadI2C) || (pFunctionId == LpGBTSCWorker::MultiByteWriteI2C))
    {
        cWorkerDone   = (cStatus & 0xFF) == 1;
        cFunctionDone = ((cStatus & (0xFF << 16)) >> 16) == 1;
    }
    else if((pFunctionId == LpGBTSCWorker::SingleReadFE) || (pFunctionId == LpGBTSCWorker::SingleWriteFE))
    {
        cWorkerDone   = (cStatus & 0xFF) == 1;
        cFunctionDone = ((cStatus & (0xFF << 24)) >> 24) == 1;
    }
    else
    {
        LOG(ERROR) << "D19cCommandProcessorInterface::IsDone : LpGBT-SC Worker fuction doesn't exist" << RESET;
        throw std::runtime_error("D19cCommandProcessorInterface::IsDone failure");
    }
    return cWorkerDone && cFunctionDone;
}

uint8_t D19cCommandProcessorInterface::GetTryCntr(uint8_t pFunctionId)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t                              cAllCntr = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_try_counters");
    uint8_t                               cCntr    = 255;
    if((pFunctionId == LpGBTSCWorker::SingleReadIC) || (pFunctionId == LpGBTSCWorker::SingleWriteIC)) { cCntr = (cAllCntr & (0xFF << 0)) >> 0; }
    else if((pFunctionId == LpGBTSCWorker::SingleByteReadI2C) || (pFunctionId == LpGBTSCWorker::MultiByteWriteI2C))
    {
        cCntr = (cAllCntr & (0xFF << 8)) >> 8;
    }
    else if((pFunctionId == LpGBTSCWorker::SingleReadFE) || (pFunctionId == LpGBTSCWorker::SingleWriteFE))
    {
        cCntr = (cAllCntr & (0xFF << 16)) >> 16;
    }
    else
    {
        LOG(ERROR) << "D19cCommandProcessorInterface::GetTryCntr : LpGBT-SC Worker fuction doesn't exist" << RESET;
        throw std::runtime_error("D19cCommandProcessorInterface::GetTryCntr failure");
    }
    return cCntr;
}
} // namespace Ph2_HwInterface
