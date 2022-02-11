#ifndef _FEConfigurationInterface_H__
#define _FEConfigurationInterface_H__

#include "ChipRegItem.h"
#include "Chip.h"
#include "BeBoard.h"
#include "../Utils/Utilities.h"
#include "../Utils/easylogging++.h"
#include "RegManager.h"
#include <string>

namespace Ph2_HwInterface
{

struct Config
{
    uint8_t  fReTry       = 0;
    uint8_t  fVerbose     = 0;
    uint16_t fMaxAttempts = 500;
    uint8_t  fVerify      = 0; 
};

enum class ConfigurationType
{
    I2C    = 1,
    IC    = 2,
    EC   = 3
};


class FEConfigurationInterface : public RegManager
{
  public:
  
    FEConfigurationInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    FEConfigurationInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~FEConfigurationInterface();
  
  public:
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

    

     /*!
     * \brief Clear Register Map
     */

    // keeping track of register configuration 
    // these are shared functions between any implementation 
    void ClearModifiedRegisterMap();
    Ph2_HwDescription::ChipRegMap GetModifiedRegisterMap(Ph2_HwDescription::Chip* pChip);
    void OverwriteModifiedRegisterMap(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegMap pRegMap);
    void UpdateModifiedRegMap(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pPage = 0);
    
    void Configure(Config pConfig)
    {
        fConfig.fVerbose     = pConfig.fVerbose;
        fConfig.fReTry       = pConfig.fReTry;
        fConfig.fMaxAttempts = pConfig.fMaxAttempts;
        fConfig.fVerify = pConfig.fVerify;
    }
    void setVerify(uint8_t pVerify){ fConfig.fVerify = pVerify; }
    void setRetry(uint8_t pReTry){ fConfig.fReTry = pReTry; }
    void setRegisterTracking(uint8_t pTrack){ fTrackRegisters = pTrack; }

    void setConfigurationType(ConfigurationType pType){ fType = pType;}
    ConfigurationType getConfigurationType(){ return fType; }
  protected:
    uint8_t fTrackRegisters{true}; 
    std::map<uint32_t, Ph2_HwDescription::ChipRegMap> fModifiedRegisters;
    std::map<uint16_t, std::string>                   fMap;
    uint8_t fNReadoutChip{0};
    Config  fConfig; 
    ConfigurationType fType;
};
} // namespace Ph2_HwInterface
#endif
