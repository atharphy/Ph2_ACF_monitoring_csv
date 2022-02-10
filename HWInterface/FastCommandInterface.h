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
        
        virtual void ComposeFastCommand(uint32_t resync_en, uint32_t l1a_en, uint32_t cal_pulse_en, uint32_t bc0_en)
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::ComposeFastCommand is absent" << RESET;
        }

    private : 
    
};
} // namespace Ph2_HwInterface
#endif