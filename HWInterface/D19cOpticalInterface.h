#ifndef _D19cOpticalInterface_H__
#define __D19cOpticalInterface_H__

#include "FEConfigurationInterface.h"

class CPBconfig;

namespace Ph2_HwInterface
{

class D19cOpticalInterface : public FEConfigurationInterface
{
  public:
  
    D19cOpticalInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19cOpticalInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~D19cOpticalInterface();

  public:
    // Multi Write/Read
    bool MultiWrite(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems) override ;
    bool MultiRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems) override;
    // Single Write/Read
    bool SingleWrite(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem) override;
    bool SingleRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem ) override; 
    // Write+Read-back 
    bool SingleWriteRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem) override;
    bool MultiWriteRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pItem) override;
    
    
    void setResetEnable(uint8_t pEnable){ fResetEn = pEnable; }
    void setWait(uint32_t pWait_us){ fWait_us = pWait_us; }
    
  private:
  
    // ##########################################
    // # Read/Write new Command Processor Block #
    // ##########################################
    // functions for new Command Processor Block
    void                  ResetCPB() ;
    void                  WriteCommandCPB(const std::vector<uint32_t>& pCommandVector) ;
    std::vector<uint32_t> ReadReplyCPB(uint8_t pNWords) ;
    std::vector<uint32_t> WriteCommandCPBandReadReply(const std::vector<uint32_t>& pCommandVector, uint8_t pNWords);

    // function for I2C transactions using lpGBT I2C Masters
    bool    I2CWrite(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint32_t pSlaveData, uint8_t pNBytes, uint8_t pFrequency, uint32_t& pNWrites) ;
    uint8_t I2CRead(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint8_t pNBytes, uint8_t pFrequency, uint32_t& pNReads) ;
    
    // ############################
    // # Read/Write Optical Group #
    // ############################
    uint8_t       fI2Cstatus    = 0;
    uint32_t      fI2CWriteCount = 0; 
    uint32_t      fI2CReadCount = 0; 
    uint8_t       fResetEn     = 1;
    uint32_t      fWait_us     = 50;


};
} // namespace Ph2_HwInterface
#endif