#include "FEConfigurationInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
FEConfigurationInterface::FEConfigurationInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : RegManager(pId, pUri, pAddressTable) {}
FEConfigurationInterface::FEConfigurationInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : RegManager(puHalConfigFileName, pBoardId)
{
    LOG(INFO) << BOLDYELLOW << "FEConfigurationInterface::FEConfigurationInterface Constructor" << RESET;
}
FEConfigurationInterface::~FEConfigurationInterface() {}

} // namespace Ph2_HwInterface