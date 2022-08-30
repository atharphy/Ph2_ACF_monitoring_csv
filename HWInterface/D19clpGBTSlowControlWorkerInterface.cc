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
    // reset shoudl be 0x00020010
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | 16 << 0);
    WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", cCommandVector);
    ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", 10);
}
void D19clpGBTSlowControlWorkerInterface::SelectLink(uint8_t pLinkId)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
}
std::vector<uint32_t>
D19clpGBTSlowControlWorkerInterface::EncodeCommand(uint8_t pFunctionId, Ph2_HwDescription::Chip* pChip, const std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems, bool pVerify)
{
    std::vector<uint32_t> cCommand;
    uint8_t               cWorkerId = LpGBTSlowControlWorker::BASE_ID + pChip->getOpticalGroupId();
    uint8_t               cChipId   = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : (pChip->getId() % 8);
    uint8_t               cChipCode = pChip->getChipCode();
    uint8_t               cMasterId = pChip->getMasterId();
    uint16_t              cNWords   = pRegisterItems.size();

    // fill command header (if needed)
    switch(pFunctionId)
    {
    case LpGBTSlowControlWorker::READ_IC: cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | cNWords << 0); break;

    case LpGBTSlowControlWorker::WRITE_IC: cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | cNWords << 0); break;

    case LpGBTSlowControlWorker::READ_FE:
        cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | (cNWords + 1) << 0);
        cCommand.push_back(cChipCode << 29 | cChipId << 26 | cMasterId << 24 | pVerify << 23);
        break;

    case LpGBTSlowControlWorker::WRITE_FE:
        cCommand.push_back(cWorkerId << 24 | pFunctionId << 16 | (cNWords + 1) << 0);
        cCommand.push_back(cChipCode << 29 | cChipId << 26 | cMasterId << 24 | pVerify << 23);
        break;

    default:
        LOG(ERROR) << "D19clpGBTSlowControlWorkerInterface::EncodeCommand Header : LpGBT-SC Worker fuction doesn't exist" << RESET;
        throw std::runtime_error("D19clpGBTSlowControlWorkerInterface::EncodeCommand failure");
    }

    // fill command payload
    for(auto& cRegItem: pRegisterItems)
    {
        switch(pFunctionId)
        {
        case LpGBTSlowControlWorker::READ_IC: cCommand.push_back(cRegItem.fAddress << 16); break;

        case LpGBTSlowControlWorker::WRITE_IC: cCommand.push_back(cRegItem.fAddress << 16 | cRegItem.fValue << 8); break;

        case LpGBTSlowControlWorker::READ_FE: cCommand.push_back(cRegItem.fAddress << 16); break;

        case LpGBTSlowControlWorker::WRITE_FE: cCommand.push_back(cRegItem.fAddress << 16 | cRegItem.fValue << 8); break;

        default:
            LOG(ERROR) << "D19clpGBTSlowControlWorkerInterface::EncodeCommand Payload : LpGBT-SC Worker fuction doesn't exist" << RESET;
            throw std::runtime_error("D19clpGBTSlowControlWorkerInterface::EncodeCommand failure");
        }
    }
    return cCommand;
}
void D19clpGBTSlowControlWorkerInterface::PrintStateFSM()
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t                               cCommandArbitratorState = ReadReg("fc7_daq_stat.command_processor_block.command_arbitrator_fsm_state");
    uint8_t                               cReplyArbitratorState   = ReadReg("fc7_daq_stat.command_processor_block.reply_arbitrator_fsm_state");
    std::string                           cWorkerState      = LpGBTSlowControlWorker::WORKER_FSM_STATE_MAP.at(ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state.worker_state"));
    std::string                           cFunctionStateIC  = LpGBTSlowControlWorker::IC_FSM_STATE_MAP.at(ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state.ic_state"));
    std::string                           cFunctionStateI2C = LpGBTSlowControlWorker::I2C_FSM_STATE_MAP.at(ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state.i2c_state"));
    std::string                           cFunctionStateFE  = LpGBTSlowControlWorker::FE_FSM_STATE_MAP.at(ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state.fe_state"));
    LOG(ERROR) << BOLDRED << "D19cOpticalInterface::SingleWrite : Tool stuck - Command Arbitrator State = " << +cCommandArbitratorState << " - Reply Arbitrator State = " << +cReplyArbitratorState
               << " - Worker state = " << cWorkerState << " - Function State IC = " << cFunctionStateIC << " - Function State I2C = " << cFunctionStateI2C
               << " - Function State FE = " << cFunctionStateFE << RESET;
}

bool D19clpGBTSlowControlWorkerInterface::IsDone(uint8_t pFunctionId)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint8_t                               cState        = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state.worker_state");
    bool                                  cWorkerDone   = (cState == 1);
    bool                                  cFunctionDone = false;
    if((pFunctionId == LpGBTSlowControlWorker::READ_IC) || (pFunctionId == LpGBTSlowControlWorker::WRITE_IC))
    {
        uint8_t cState = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state.ic_state");
        cFunctionDone  = (cState == 1);
    }
    else if((pFunctionId == LpGBTSlowControlWorker::SINGLE_BYTE_READ_I2C) || (pFunctionId == LpGBTSlowControlWorker::MULTI_BYTE_WRITE_I2C))
    {
        uint8_t cState = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state.i2c_state");
        cFunctionDone  = (cState == 1);
    }
    else if((pFunctionId == LpGBTSlowControlWorker::READ_FE) || (pFunctionId == LpGBTSlowControlWorker::WRITE_FE))
    {
        uint8_t cState = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state.fe_state");
        cFunctionDone  = (cState == 1);
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
    uint8_t                               cCntr = 255;
    if((pFunctionId == LpGBTSlowControlWorker::READ_IC) || (pFunctionId == LpGBTSlowControlWorker::WRITE_IC))
    { cCntr = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_try_counters.ic_tool"); }
    else if((pFunctionId == LpGBTSlowControlWorker::SINGLE_BYTE_READ_I2C) || (pFunctionId == LpGBTSlowControlWorker::MULTI_BYTE_WRITE_I2C))
    {
        cCntr = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_try_counters.i2c_tool");
    }
    else if((pFunctionId == LpGBTSlowControlWorker::READ_FE) || (pFunctionId == LpGBTSlowControlWorker::WRITE_FE))
    {
        cCntr = ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_try_counters.fe_tool");
    }
    else
    {
        LOG(ERROR) << "D19clpGBTSlowControlWorkerInterface::GetTryCntr : LpGBT-SC Worker fuction doesn't exist" << RESET;
        throw std::runtime_error("D19clpGBTSlowControlWorkerInterface::GetTryCntr failure");
    }
    return cCntr;
}

} // namespace Ph2_HwInterface
