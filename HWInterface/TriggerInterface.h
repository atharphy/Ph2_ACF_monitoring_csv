#ifndef _TriggerInterface_H__
#define _TriggerInterface_H__

#include "RegManager.h"
#include "BeBoard.h"
#include "../Utils/Utilities.h"
#include "../Utils/easylogging++.h"
#include <string>

namespace Ph2_HwInterface
{
class TriggerInterface : public RegManager
{
    public: // constructors 
  
    TriggerInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    TriggerInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~TriggerInterface();

    public: // virtual functions 
    
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

    protected : 
        uint32_t fWait_us{10};
};
} // namespace Ph2_HwInterface
#endif