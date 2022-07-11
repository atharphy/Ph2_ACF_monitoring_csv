#ifndef _D19cOpticalInterface_H__
#define __D19cOpticalInterface_H__

#include "D19clpGBTSlowControlWorkerInterface.h"
#include "FEConfigurationInterface.h"

namespace Ph2_HwInterface
{
class D19cOpticalInterface : public FEConfigurationInterface
{
  public:
    D19cOpticalInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19cOpticalInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~D19cOpticalInterface();

    void LinkLpGBTSlowControlWorkerInterface(D19clpGBTSlowControlWorkerInterface* pInterface) { flpGBTSlowControlWorkerInterface = pInterface; }

  public:
    bool SingleWrite(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem) override;
    bool SingleRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem) override;
    // Write+Read-back
    bool SingleWriteRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem) override;
    bool MultiWriteRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems) override;
    // Multi Write/Read
    bool MultiRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems) override;
    bool MultiWrite(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems) override;
    // Single Write/Read
    // lpGBT I2C Masters
    bool    MultiByteWriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData);
    uint8_t SingleByteReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress);

  private:
    D19clpGBTSlowControlWorkerInterface* flpGBTSlowControlWorkerInterface;

  private:
    // write function
    bool Write(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pItem, bool pVerify = false);
    bool Read(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pItem);
};
} // namespace Ph2_HwInterface
#endif
