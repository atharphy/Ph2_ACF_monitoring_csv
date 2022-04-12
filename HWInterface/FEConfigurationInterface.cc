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

void FEConfigurationInterface::Configure(Configuration pConfiguration)
{
    WriteStackReg({{"fc7_daq_cnfg.optical_block.lpgbt_sc_worker.ic_retry", pConfiguration.fRetryIC},
                   {"fc7_daq_cnfg.optical_block.lpgbt_sc_worker.max_ic_retry", pConfiguration.fMaxRetryIC},
                   {"fc7_daq_cnfg.optical_block.lpgbt_sc_worker.i2c_retry", pConfiguration.fRetryI2C},
                   {"fc7_daq_cnfg.optical_block.lpgbt_sc_worker.max_i2c_retry", pConfiguration.fMaxRetryI2C},
                   {"fc7_daq_cnfg.optical_block.lpgbt_sc_worker.fe_retry", pConfiguration.fRetryFE},
                   {"fc7_daq_cnfg.optical_block.lpgbt_sc_worker.max_fe_retry", pConfiguration.fMaxRetryFE}});
    //
    fConfiguration.fRetryIC = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.ic_retry");
    LOG(DEBUG) << BOLDYELLOW << "Configuring LpGBT-SC Worker ... IC Retry = " << +fConfiguration.fRetryIC << RESET;
    //
    fConfiguration.fMaxRetryIC = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.max_ic_retry");
    LOG(DEBUG) << BOLDYELLOW << "Configuring LpGBT-SC Worker ... IC Max Retry = " << +fConfiguration.fMaxRetryIC << RESET;
    //
    fConfiguration.fRetryI2C = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.i2c_retry");
    LOG(DEBUG) << BOLDYELLOW << "Configuring LpGBT-SC Worker ... I2C Retry = " << +fConfiguration.fRetryI2C << RESET;
    //
    fConfiguration.fMaxRetryI2C = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.max_i2c_retry");
    LOG(DEBUG) << BOLDYELLOW << "Configuring LpGBT-SC Worker ... I2C Max Retry = " << +fConfiguration.fMaxRetryI2C << RESET;
    //
    fConfiguration.fRetryFE = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.fe_retry");
    LOG(DEBUG) << BOLDYELLOW << "Configuring LpGBT-SC Worker ... FE Retry = " << +fConfiguration.fRetryFE << RESET;
    //
    fConfiguration.fMaxRetryFE = ReadReg("fc7_daq_cnfg.optical_block.lpgbt_sc_worker.max_fe_retry");
    LOG(DEBUG) << BOLDYELLOW << "Configuring LpGBT-SC Worker ... FE Max Retry = " << +fConfiguration.fMaxRetryFE << RESET;
    //
    fConfiguration.fVerify      = pConfiguration.fRetry;
    fConfiguration.fVerify      = pConfiguration.fVerify;
    fConfiguration.fMaxAttempts = pConfiguration.fMaxAttempts;
}

} // namespace Ph2_HwInterface