#include "HWInterface/FEConfigurationInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
FEConfigurationInterface::FEConfigurationInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : RegManager(pId, pUri, pAddressTable) {}
FEConfigurationInterface::FEConfigurationInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : RegManager(puHalConfigFileName, pBoardId)
{
    LOG(INFO) << BOLDYELLOW << "FEConfigurationInterface::FEConfigurationInterface Constructor" << RESET;
}
FEConfigurationInterface::~FEConfigurationInterface() {}

void FEConfigurationInterface::Configure(Configuration pConfiguration)
{
    fConfiguration.fRetryIC  = pConfiguration.fRetryIC;
    fConfiguration.fRetryI2C = pConfiguration.fRetryI2C;
    fConfiguration.fRetryFE  = pConfiguration.fRetryFE;

    fConfiguration.fMaxRetryIC  = pConfiguration.fMaxRetryIC;
    fConfiguration.fMaxRetryI2C = pConfiguration.fMaxRetryI2C;
    fConfiguration.fMaxRetryFE  = pConfiguration.fMaxRetryFE;

    fConfiguration.fRetry       = pConfiguration.fRetry;
    fConfiguration.fVerify      = pConfiguration.fVerify;
    fConfiguration.fMaxAttempts = pConfiguration.fMaxAttempts;
}

} // namespace Ph2_HwInterface
