#ifndef _D19cTriggerInterface_H__
#define _D19cTriggerInterface_H__

#include "TriggerInterface.h"

namespace Ph2_HwInterface
{
class D19cTriggerInterface : public TriggerInterface
{
    public: // constructors 
  
    D19cTriggerInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19cTriggerInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~D19cTriggerInterface();

    public: // virtual functions 
        bool SetNTriggersToAccept(uint32_t pNTriggersToAccept) override;
        void ResetTriggerFSM() override;
        void ReconfigureTriggerFSM(std::vector<std::pair<std::string, uint32_t>> pTriggerConfig) override;
        bool Start() override;
        void Pause() override;
        void Resume() override;
        bool Stop() override;
        bool RunTriggerFSM() override;
        uint32_t GetTriggerState()  override;

    private : 
        void TriggerConfiguration();
    
};
} // namespace Ph2_HwInterface
#endif