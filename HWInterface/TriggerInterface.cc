#include "TriggerInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
TriggerInterface::TriggerInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : RegManager(pId, pUri, pAddressTable) {}
TriggerInterface::TriggerInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : RegManager(puHalConfigFileName, pBoardId)
{
    LOG(INFO) << BOLDYELLOW << "TriggerInterface::TriggerInterface Constructor" << RESET;
}
TriggerInterface::~TriggerInterface() {}

} // namespace Ph2_HwInterface