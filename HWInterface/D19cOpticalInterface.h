#ifndef _D19cOpticalInterface_H__
#define __D19cOpticalInterface_H__

#include "HWInterface/D19clpGBTSlowControlWorkerInterface.h"
#include "HWInterface/FEConfigurationInterface.h"

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
    // Single Write/Read
    bool SingleWrite(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pRegisterItem) override;
    bool SingleRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pRegisterItem) override;
    // Single Write+Read-back
    bool SingleWriteRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pRegisterItem) override;
    bool MultiWriteRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems) override;
    // Multi Write/Read
    bool MultiRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems) override;
    bool MultiWrite(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems) override;
    // Single lpGBT I2C Masters MultiByteWrite/SingleByteRead
    bool    SingleMultiByteWriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData, bool pWaitToBeDone = true);
    uint8_t SingleSingleByteReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress);
    // Multi lpGBT I2C Masters MultiByteWrite/SingleByteRead
    bool                  MultiMultiByteWriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, std::vector<uint32_t>& pSlaveData);
    std::vector<uint16_t> MultiSingleByteReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, std::vector<uint32_t>& pSlaveData);

  private:
    D19clpGBTSlowControlWorkerInterface* flpGBTSlowControlWorkerInterface;

  private:
    // IC and FE functions
    bool Write(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems, bool pVerify = false);
    bool Read(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems);
    // I2C Master functions
    bool                  WriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, std::vector<uint32_t>& pSlaveData, bool pWaitToBeDone);
    std::vector<uint16_t> ReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, std::vector<uint32_t>& pSlaveData);
};
} // namespace Ph2_HwInterface
#endif
