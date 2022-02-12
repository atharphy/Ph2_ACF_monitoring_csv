#ifndef _FastCommandInterface_H__
#define _FastCommandInterface_H__

#include "RegManager.h"
#include "BeBoard.h"
#include "../Utils/Utilities.h"
#include "../Utils/easylogging++.h"
#include <string>

namespace Ph2_HwInterface
{
struct FastCommand
{
    uint32_t resync_en{0};
    uint32_t l1a_en{0};
    uint32_t cal_pulse_en{0}; 
    uint32_t bc0_en{0};
    uint32_t duration{0}; 
};

class FastCommandInterface : public RegManager
{
    public: // constructors 
  
    FastCommandInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    FastCommandInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~FastCommandInterface();

    protected : 
        FastCommand fFastCmd;

    public: // virtual functions 
        void setDuration(uint32_t pDuration){ fFastCmd.duration = pDuration; }
        
        virtual void SendGlobalReSync(uint8_t pDuration=0) // 1 clk cycle 
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::SendGlobalReSync is absent" << RESET;
        }
        virtual void SendGlobalCalPulse(uint8_t pDuration=0) // 1 clk cycle 
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::SendGlobalCalPulse is absent" << RESET;
        }
        virtual void SendGlobalL1A(uint8_t pDuration=0) // 1 clk cycle 
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::SendGlobalL1A is absent" << RESET;
        }
        virtual void SendGlobalCounterReset(uint8_t pDuration=0) // 1 clk cycle 
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::SendGlobalCounterReset is absent" << RESET;
        }
        virtual void SendGlobalCounterResetResync(uint8_t pDuration=0) // 1 clk cycle  // BC0 + ReScync 
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::SendGlobalCounterResetResync is absent" << RESET;
        }
         virtual void SendGlobalCounterResetL1A(uint8_t pDuration=0) // 1 clk cycle  // BC0 + ReScync 
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::SendGlobalCounterResetL1A is absent" << RESET;
        }
        virtual void SendGlobalCounterResetCalPulse(uint8_t pDuration=0) // 1 clk cycle  // BC0 + ReScync 
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::SendGlobalCounterResetCalPulse is absent" << RESET;
        }
        virtual void SendGlobalCustomFastCommands(std::vector<FastCommand> &pFastCmd)
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::SendGlobalCustomFastCommands is absent" << RESET;
        }
        virtual void ComposeFastCommand(const FastCommand& pFastCommand)
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function FastCommandInterface::ComposeFastCommand is absent" << RESET;
        }

    private : 
    
};
} // namespace Ph2_HwInterface
#endif