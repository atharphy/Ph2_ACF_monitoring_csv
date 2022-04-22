#include "LinkInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
LinkInterface::LinkInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : RegManager(pId, pUri, pAddressTable) {}
LinkInterface::LinkInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : RegManager(puHalConfigFileName, pBoardId)
{
    LOG(INFO) << BOLDYELLOW << "LinkInterface::LinkInterface Constructor" << RESET;
}
LinkInterface::~LinkInterface() {}


} // namespace Ph2_HwInterface