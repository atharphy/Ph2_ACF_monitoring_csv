#ifndef _FastCommandInterface_H__
#define _FastCommandInterface_H__

#include "RegManager.h"
#include "BeBoard.h"
#include "../Utils/Utilities.h"
#include "../Utils/easylogging++.h"
#include <string>

namespace Ph2_HwInterface
{
class FastCommandInterface : public RegManager
{
    public: // constructors 
  
    FastCommandInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    FastCommandInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~FastCommandInterface();

    public: // virtual functions 
    
        virtual void ResetTriggerFSM()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::ResetTriggerFSM is absent" << RESET;
        }
        virtual void ReconfigureTriggerFSM(std::vector<std::pair<std::string, uint32_t>> pTriggerConfig)
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::ReconfigureTriggerFSM is absent" << RESET;
        }
        
        virtual bool Start()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::Start is absent" << RESET;
            return false;
        }
        virtual bool Stop()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::Stop is absent" << RESET;
            return false;
        }

    private : 
    
};
} // namespace Ph2_HwInterface
#endif