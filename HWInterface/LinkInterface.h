#ifndef _LinkInterface_H__
#define _LinkInterface_H__

#include "HWDescription/BeBoard.h"
#include "HWInterface/RegManager.h"
#include "Utils/Utilities.h"
#include "Utils/easylogging++.h"
#include <string>

namespace Ph2_HwInterface
{
struct LinkInterfaceConfiguration
{
    uint32_t fResetWait_ms = 200;
    uint8_t  fReTry        = 0;
    size_t   fMaxAttempts  = 10;
};

class LinkInterface : public RegManager
{
  public:
    LinkInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    LinkInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~LinkInterface();

  public:
    virtual void ResetLinks() { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function LinkInterface::ResetLinks is absent" << RESET; }
    virtual void ResetLink(uint8_t pLinkId = 0) { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function LinkInterface::ResetLink is absent" << RESET; }
    virtual bool GetLinkStatus(uint8_t pLinkId = 0)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function LinkInterface::GetLinkStatus is absent" << RESET;
        return false;
    }
    virtual void GeneralLinkReset(const Ph2_HwDescription::BeBoard* pBoard)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function LinkInterface::GeneralLinkReset is absent" << RESET;
    }

  protected:
    LinkInterfaceConfiguration fConfiguration;
    uint32_t                   getReset() { return fConfiguration.fResetWait_ms; }
    void                       setReset(uint32_t pReset_ms) { fConfiguration.fResetWait_ms = pReset_ms; }
};
} // namespace Ph2_HwInterface
#endif