#ifndef _L1ReadoutInterface_H__
#define _L1ReadoutInterface_H__

#include "../Utils/Utilities.h"
#include "../Utils/easylogging++.h"
#include "RegManager.h"
#include <string>

#include "FastCommandInterface.h"
#include "TriggerInterface.h"

namespace Ph2_HwInterface
{
class L1ReadoutInterface : public RegManager
{
  public: // constructors
    L1ReadoutInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    L1ReadoutInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~L1ReadoutInterface();

  protected:
    FastCommandInterface* fFastCommandInterface;
    TriggerInterface*     fTriggerInterface;
    uint8_t               fHandshake;
    std::vector<uint32_t> fData;
    uint32_t              fNEvents;
    uint32_t              fNReadoutEvents;
    uint32_t              fMaxAttempts{10};
    uint32_t              fReadoutAttempt{0};
    uint32_t              fTimeout_us{5000000}; // time-out after 5s

  public: // virtual functions
    virtual void FillData() { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function L1ReadoutInterface::ReadData is absent" << RESET; }
    virtual bool WaitForReadout()
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function L1ReadoutInterface::WaitForData is absent" << RESET;
        return false;
    }
    virtual bool WaitForNTriggers()
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function L1ReadoutInterface::WaitForNTriggers is absent" << RESET;
        return false;
    }
    virtual bool ReadEvents(const Ph2_HwDescription::BeBoard* pBoard)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function L1ReadoutInterface::ReadEvents is absent" << RESET;
        return false;
    }
    virtual bool PollReadoutData(const Ph2_HwDescription::BeBoard* pBoard, bool pWait = false)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function L1ReadoutInterface::ReadEvents is absent" << RESET;
        return false;
    }
    virtual bool ResetReadout()
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function L1ReadoutInterface::ResetReadout is absent" << RESET;
        return false;
    }

    virtual bool CheckBuffers()
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function L1ReadoutInterface::CheckBuffers is absent" << RESET;
        return false;
    }

    std::vector<uint32_t> getData() { return fData; }
    void                  clearData() { fData.clear(); }
    // function to link FEConfigurationInterface
    void LinkFastCommandInterface(FastCommandInterface* pInterface) { fFastCommandInterface = pInterface; }
    void LinkTriggerInterface(TriggerInterface* pInterface) { fTriggerInterface = pInterface; }

    void     setNEvents(uint32_t pNEvents) { fNEvents = pNEvents; }
    uint32_t getNEvents() { return fNEvents; }
    uint32_t getNReadoutEvents() { return fNReadoutEvents; }
    void     setHandshake(uint8_t pHandshake) { fHandshake = pHandshake; }
};
} // namespace Ph2_HwInterface
#endif