#ifndef _D19cFastCommandInterface_H__
#define _D19cFastCommandInterface_H__

#include "FastCommandInterface.h"

namespace Ph2_HwInterface
{

class D19cFastCommandInterface : public FastCommandInterface
{
    public: // constructors 
  
    D19cFastCommandInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19cFastCommandInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~D19cFastCommandInterface();

    public: // functions 
        void SendGlobalReSync() override;
        void SendGlobalCalPulse() override;
        void SendGlobalL1A() override;
        void SendGlobalCounterReset() override;
        void SendGlobalCounterResetResync() override;
        void SendGlobalCounterResetL1A() override;
        void SendGlobalCounterResetCalPulse() override;
        void SendGlobalCustomFastCommands(std::vector<FastCommand> &pFastCmd) override;
        void ComposeFastCommand(const FastCommand& pFastCommand) override;
        
    private : 
        uint32_t fFastCommand{0};

};
} // namespace Ph2_HwInterface
#endif