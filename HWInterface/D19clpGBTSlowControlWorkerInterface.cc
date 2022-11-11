#include "D19clpGBTSlowControlWorkerInterface.h"

namespace Ph2_HwInterface
{
D19clpGBTSlowControlWorkerInterface::D19clpGBTSlowControlWorkerInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable)
    : D19cCommandProcessorInterface(pId, pUri, pAddressTable)
{
    LOG(INFO) << BOLDYELLOW << "D19clpGBTSlowControlWorkerInterface::D19clpGBTSlowControlWorkerInterface Constructor" << RESET;
}

D19clpGBTSlowControlWorkerInterface::D19clpGBTSlowControlWorkerInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : D19cCommandProcessorInterface(puHalConfigFileName, pBoardId)
{
    LOG(INFO) << BOLDYELLOW << "D19clpGBTSlowControlWorkerInterface::D19clpGBTSlowControlWorkerInterface Constructor" << RESET;
}

D19clpGBTSlowControlWorkerInterface::~D19clpGBTSlowControlWorkerInterface() {}

void D19clpGBTSlowControlWorkerInterface::Reset()
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    LOG(DEBUG) << BOLDBLUE << "Resetting Command Processor" << RESET;
    // Soft reset the GBT-SC worker
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    uint8_t cWorkerId = 0, cFunctionId = 2;
    // reset should be 0x00020010
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | 16 << 0);
    WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", cCommandVector);
    ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", 10);
}
void D19clpGBTSlowControlWorkerInterface::SelectLink(uint8_t pLinkId)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
}

std::vector<uint32_t> D19clpGBTSlowControlWorkerInterface::EncodeCommand(uint8_t pFunctionId, Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem, bool pVerify)
{
    std::vector<uint32_t> cCommand;
    uint8_t               cWorkerId = LpGBTSlowControlWorker::BASE_ID + pChip->getOpticalGroupId();
    uint8_t               cChipId   = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : (pChip->getId() % 8);
    uint8_t               cChipCode = pChip->getChipCode();
    uint8_t               cMasterId = pChip->getMasterId();
    switch(pFunctionId)
    {
    case LpGBTSlowControlWorker::SINGLE_READ_IC: cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | pItem.fAddress << 0); break;

    case LpGBTSlowControlWorker::SINGLE_WRITE_IC:
        cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | pItem.fAddress << 0);
        cCommand.push_back(pItem.fValue << 0);
        break;

    case LpGBTSlowControlWorker::SINGLE_READ_FE:
        cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | cMasterId << 6 | cChipCode << 3 | cChipId << 0);
        cCommand.push_back(pItem.fAddress << 0);
        break;

    case LpGBTSlowControlWorker::SINGLE_WRITE_FE:
        cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | pVerify << 8 | cMasterId << 6 | cChipCode << 3 | cChipId << 0);
        cCommand.push_back(pItem.fValue << 16 | pItem.fAddress << 0);
        break;
    default:
        LOG(ERROR) << "D19clpGBTSlowControlWorkerInterface::EncodeCommand : LpGBT-SC Worker fuction doesn't exist" << RESET;
        throw std::runtime_error("D19clpGBTSlowControlWorkerInterface::EncodeCommand failure");
    }
    return cCommand;
}

void D19clpGBTSlowControlWorkerInterface::PrintStateFSM()
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t                              cState                = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    uint8_t                               cWorkerState          = (cState & 0xFF);
    uint8_t                               cFunctionStateIC      = (cState & (0xFF << 8)) >> 8;
    uint8_t                               cFunctionStateI2C     = (cState & (0xFF << 16)) >> 16;
    uint8_t                               cFunctionStateFE      = (cState & (0xFF << 24)) >> 24;
    std::string                           cWorkerStateDesc      = LpGBTSlowControlWorker::WORKER_FSM_STATE_MAP.at(cWorkerState);
    std::string                           cFunctionStateDescIC  = LpGBTSlowControlWorker::IC_FSM_STATE_MAP.at(cFunctionStateIC);
    std::string                           cFunctionStateDescI2C = LpGBTSlowControlWorker::I2C_FSM_STATE_MAP.at(cFunctionStateI2C);
    std::string                           cFunctionStateDescFE  = LpGBTSlowControlWorker::FE_FSM_STATE_MAP.at(cFunctionStateFE);
    LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWrite : Tool stuck - Worker state = " << cWorkerStateDesc << " - Function State IC = " << cFunctionStateDescIC
               << " - Function State I2C = " << cFunctionStateDescI2C << " - Function State FE = " << cFunctionStateDescFE << RESET;
}

bool D19clpGBTSlowControlWorkerInterface::IsDone(uint8_t pFunctionId)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t                              cStatus       = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    bool                                  cWorkerDone   = (cStatus & 0xFF) == 1;
    bool                                  cFunctionDone = false;
    if((pFunctionId == LpGBTSlowControlWorker::SINGLE_READ_IC) || (pFunctionId == LpGBTSlowControlWorker::SINGLE_WRITE_IC)) { cFunctionDone = ((cStatus & (0xFF << 8)) >> 8) == 1; }
    else if((pFunctionId == LpGBTSlowControlWorker::SINGLE_BYTE_READ_I2C) || (pFunctionId == LpGBTSlowControlWorker::MULTI_BYTE_WRITE_I2C))
    {
        cFunctionDone = ((cStatus & (0xFF << 16)) >> 16) == 1;
    }
    else if((pFunctionId == LpGBTSlowControlWorker::SINGLE_READ_FE) || (pFunctionId == LpGBTSlowControlWorker::SINGLE_WRITE_FE))
    {
        cFunctionDone = ((cStatus & (0xFF << 24)) >> 24) == 1;
    }
    else
    {
        LOG(ERROR) << "D19clpGBTSlowControlWorkerInterface::IsDone : LpGBT-SC Worker fuction doesn't exist" << RESET;
        throw std::runtime_error("D19clpGBTSlowControlWorkerInterface::IsDone failure");
    }
    return cWorkerDone && cFunctionDone;
}

uint8_t D19clpGBTSlowControlWorkerInterface::GetTryCntr(uint8_t pFunctionId)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t                              cAllCntr = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_try_counters");
    uint8_t                               cCntr    = 255;
    if((pFunctionId == LpGBTSlowControlWorker::SINGLE_READ_IC) || (pFunctionId == LpGBTSlowControlWorker::SINGLE_WRITE_IC)) { cCntr = (cAllCntr & (0xFF << 0)) >> 0; }
    else if((pFunctionId == LpGBTSlowControlWorker::SINGLE_BYTE_READ_I2C) || (pFunctionId == LpGBTSlowControlWorker::MULTI_BYTE_WRITE_I2C))
    {
        cCntr = (cAllCntr & (0xFF << 8)) >> 8;
    }
    else if((pFunctionId == LpGBTSlowControlWorker::SINGLE_READ_FE) || (pFunctionId == LpGBTSlowControlWorker::SINGLE_WRITE_FE))
    {
        cCntr = (cAllCntr & (0xFF << 16)) >> 16;
    }
    else
    {
        LOG(ERROR) << "D19clpGBTSlowControlWorkerInterface::GetTryCntr : LpGBT-SC Worker fuction doesn't exist" << RESET;
        throw std::runtime_error("D19clpGBTSlowControlWorkerInterface::GetTryCntr failure");
    }
    return cCntr;
}

} // namespace Ph2_HwInterface
