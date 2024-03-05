#include "HWInterface/D19cCommandProcessorInterface.h"
#include "HWInterface/RegManager.h"
#include "Utils/ConsoleColor.h"
#include "Utils/easylogging++.h"
#include <iomanip>

namespace Ph2_HwInterface
{
D19cCommandProcessorInterface::D19cCommandProcessorInterface(RegManager* theRegManager) : CommandProcessorInterface(theRegManager)
{
    LOG(INFO) << BOLDYELLOW << "D19cCommandProcessorInterface::D19cCommandProcessorInterface Constructor" << RESET;
}

D19cCommandProcessorInterface::~D19cCommandProcessorInterface() {}

void D19cCommandProcessorInterface::WriteCommand(const std::vector<uint32_t>& pCommand)
{
    std::lock_guard<std::recursive_mutex> theGuard(fTheRegManager->fMutex);
    // int                                   cWordIndex = 0;
    // for(auto cWord: pCommand)
    // {
    //     LOG(DEBUG) << GREEN << "\t Write command word " << +cWordIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cWord << RESET;
    //     cWordIndex++;
    // }
    fTheRegManager->WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", pCommand);
}

std::vector<uint32_t> D19cCommandProcessorInterface::ReadReply(int pNWords)
{
    std::lock_guard<std::recursive_mutex> theGuard(fTheRegManager->fMutex);
    std::vector<uint32_t>                 cReply     = fTheRegManager->ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", pNWords);
    // int                                   cWordIndex = 0;
    // for(auto cWord: cReply)
    // {
    //     LOG(DEBUG) << YELLOW << "\t Read reply word " << +cWordIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cWord << RESET;
    //     cWordIndex++;
    // }
    return cReply;
}
} // namespace Ph2_HwInterface
