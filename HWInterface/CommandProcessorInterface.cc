#include "HWInterface/CommandProcessorInterface.h"

namespace Ph2_HwInterface
{
CommandProcessorInterface::CommandProcessorInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : RegManager(pId, pUri, pAddressTable) {}
CommandProcessorInterface::CommandProcessorInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : RegManager(puHalConfigFileName, pBoardId)
{
    LOG(INFO) << BOLDYELLOW << "CommandProcessorInterface::CommandProcessorInteface Constructor" << RESET;
}
} // namespace Ph2_HwInterface