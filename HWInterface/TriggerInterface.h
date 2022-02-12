#ifndef _TriggerInterface_H__
#define _TriggerInterface_H__

#include "RegManager.h"
#include "BeBoard.h"
#include "../Utils/Utilities.h"
#include "../Utils/easylogging++.h"
#include <string>

namespace Ph2_HwInterface
{
struct TriggerConfiguration
{
    uint8_t     fTriggerSource;
    uint16_t    fTriggerRate; 
    uint32_t    fNtriggersToAccept; 
};

class TriggerInterface : public RegManager
{   public: // constructors 
  
    TriggerInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    TriggerInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~TriggerInterface();

    public: // virtual functions 
        virtual bool SetNTriggersToAccept(uint32_t pNTriggersToAccept) 
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::SetNTriggersToAccept is absent" << RESET;
            return false;
        }
        virtual void ResetTriggerFSM()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::ResetTriggerFSM is absent" << RESET;
        }
        virtual void ReconfigureTriggerFSM(std::vector<std::pair<std::string, uint32_t>> pTriggerConfig)
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::ReconfigureTriggerFSM is absent" << RESET;
        }
        
        virtual bool Start()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::Start is absent" << RESET;
            return false;
        }
        virtual bool Stop()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::Stop is absent" << RESET;
            return false;
        }
        virtual void Pause()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::Start is absent" << RESET;
        }
        virtual void Resume()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::Stop is absent" << RESET;
        }
        virtual uint32_t GetTriggerState() 
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::GetTriggerState is absent" << RESET;
            return 0;
        }
        virtual bool RunTriggerFSM()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::RunTriggerFSM is absent" << RESET;
            return false;
        }
        virtual bool WaitForNTriggers(uint32_t pNTriggers)
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::WaitForNTriggers is absent" << RESET;
            return false;
        }
        virtual bool SendNTriggers(uint32_t pNTriggers)
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function TriggerInterface::SendNTriggers is absent" << RESET;
            return false;
        }

        uint8_t getTriggerSource(){ return fTriggerConfiguration.fTriggerSource; }
        uint8_t getTriggerRate(){ return fTriggerConfiguration.fTriggerRate; }
        
    protected : 
        uint32_t fWait_us{10};
        TriggerConfiguration fTriggerConfiguration;
        uint32_t fTimeout_us{5000000}; //time-out after 5s
};
} // namespace Ph2_HwInterface
#endif