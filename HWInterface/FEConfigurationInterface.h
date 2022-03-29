#ifndef _FEConfigurationInterface_H__
#define _FEConfigurationInterface_H__

#include "../Utils/Utilities.h"
#include "../Utils/easylogging++.h"
#include "BeBoard.h"
#include "Chip.h"
#include "ChipRegItem.h"
#include "RegManager.h"
#include <string>

namespace Ph2_HwInterface
{
struct Config
{
    uint8_t  fReTry       = 0;
    uint8_t  fVerbose     = 0;
    uint16_t fMaxAttempts = 5000;
    uint8_t  fVerify      = 0;
};

enum class ConfigurationType
{
    I2C   = 1, // electrical I2C via FW
    IC    = 2, // IC on optical link
    EC    = 3, // EC on optical link
    SLAVE = 4  // to an I2C slave on the optical link
};

class FEConfigurationInterface : public RegManager
{
  public:
    FEConfigurationInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    FEConfigurationInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~FEConfigurationInterface();

  public:

    virtual bool MultiWriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FEConfiguration::MultiI2CWrite is absent" << RESET;
        return false;
    }

    virtual uint8_t SingleReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FEConfiguration::SingleI2CRead is absent" << RESET;
        return false;
    }

    virtual bool MultiWrite(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FEConfiguration::MultiWrite is absent" << RESET;
        return false;
    }

    virtual bool MultiRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FEConfiguration::MultiRead is absent" << RESET;
        return 0;
    }

    virtual bool SingleWriteRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FEConfiguration::SingleWrite is absent" << RESET;
        return false;
    }

    virtual bool MultiWriteRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pItem)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FEConfiguration::MultiWriteRead is absent" << RESET;
        return false;
    }

    virtual bool SingleWrite(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FEConfiguration::SingleWrite is absent" << RESET;
        return false;
    }

    virtual bool SingleRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FEConfiguration::SingleRead is absent" << RESET;
        return 0;
    }

    virtual void PrintStatus()
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::PrintStatus is absent" << RESET;
        return;
    }
    
    void Configure(Config pConfig)
    {
        fConfig.fVerbose     = pConfig.fVerbose;
        fConfig.fReTry       = pConfig.fReTry;
        fConfig.fMaxAttempts = pConfig.fMaxAttempts;
        fConfig.fVerify      = pConfig.fVerify;
    }
    void setVerify(uint8_t pVerify) { fConfig.fVerify = pVerify; }
    void setRetry(uint8_t pReTry) { fConfig.fReTry = pReTry; }

    void              setConfigurationType(ConfigurationType pType) { fType = pType; }
    ConfigurationType getConfigurationType() { return fType; }
    void              setRegisterTracking(uint8_t pTrackRegisters) { fTrackRegisters = pTrackRegisters; }

  protected:
    uint8_t           fTrackRegisters{0};
    uint8_t           fNReadoutChip{0};
    Config            fConfig;
    ConfigurationType fType;

    std::map<FrontEndType, uint8_t> fChipCodeMap = {{FrontEndType::CBC3, 1}, 
                                                    {FrontEndType::MPA, 2}, 
                                                    {FrontEndType::SSA, 3},
                                                    {FrontEndType::CIC, 4},
                                                    {FrontEndType::CIC2, 5}};

    std::map<FrontEndType, uint8_t> fChipAddressMap = {{FrontEndType::CBC3, 0x40}, 
                                                       {FrontEndType::MPA, 0x40}, 
                                                       {FrontEndType::SSA, 0x20},
                                                       {FrontEndType::CIC, 0x60},
                                                       {FrontEndType::CIC2, 0x60}};

    std::map<FrontEndType, std::string> fChipTypeMap = {{FrontEndType::CBC3, "CBC3"}, 
                                                        {FrontEndType::MPA, "MPA"}, 
                                                        {FrontEndType::SSA, "SSA"},
                                                        {FrontEndType::CIC, "CIC1"},
                                                        {FrontEndType::CIC2, "CIC2"}};
};
} // namespace Ph2_HwInterface
#endif
