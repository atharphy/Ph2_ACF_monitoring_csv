#ifndef _D19cOpticalInterface_H__
#define __D19cOpticalInterface_H__

#include "FEConfigurationInterface.h"

class CPBconfig;



namespace Ph2_HwInterface
{

struct LpGBTSCWorkerInfo {
  uint8_t BaseID = 16;
  uint8_t SingleReadIC = 2;
  uint8_t SingleWriteIC = 3;
  uint8_t SingleReadI2C = 4;
  uint8_t MultiWriteI2C = 5;
  uint8_t SingleReadFE = 6;
  uint8_t SingleWriteFE = 7;
};

class D19cOpticalInterface : public FEConfigurationInterface
{
  public:
    D19cOpticalInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19cOpticalInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~D19cOpticalInterface();

  public:

    bool SingleWrite(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem) override;
    bool SingleRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem) override;
    // Write+Read-back
    bool SingleWriteRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem) override;
    bool MultiWriteRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pItem) override;
    // Multi Write/Read
    bool MultiRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems) override;
    bool MultiWrite(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems) override;
    // Single Write/Read
    //lpGBT I2C Masters
    bool    MultiWriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData) override;
    uint8_t SingleReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress) override;
    bool IsDoneI2C();
    uint8_t GetTryCntrI2C();

    void setResetEnable(uint8_t pEnable) { fResetEn = pEnable; }
    void setWait(uint32_t pWait_us) { fWait_us = pWait_us; }

  private:

    // ##########################################
    // # Read/Write new Command Processor Block #
    // ##########################################
    // functions for new Command Processor Block
    void                  ResetCPB();
    void                  WriteCommandCPB(const std::vector<uint32_t>& pCommandVector);
    std::vector<uint32_t> ReadReplyCPB(uint8_t pNWords);
    LpGBTSCWorkerInfo fLpGBTSCWorkerInfo;

    
    // function for Write + Read to lpGBT I2C slave
    //lpGBT
    bool SingleReadIC(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem);
    bool SingleWriteIC(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem, bool pVerify = true);
    bool IsDoneIC();
    uint8_t GetTryCntrIC();
    //Front-End ASICs
    bool SingleWriteSlave(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem, bool pVerify = false);
    bool SingleReadSlave(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem);
    bool IsDoneFE();
    uint8_t GetTryCntrFE();

    // ############################
    // # Read/Write Optical Group #
    // ############################
    uint8_t  fI2Cstatus     = 0;
    uint32_t fI2CWriteCount = 0;
    uint32_t fI2CReadCount  = 0;
    uint8_t  fResetEn       = 1;
    uint32_t fWait_us       = 100;
};
} // namespace Ph2_HwInterface
#endif