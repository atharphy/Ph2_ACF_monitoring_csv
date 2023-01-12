#ifndef _CommandProcessorInterface_H__
#define _CommandProcessorInterface_H__

#include "Utils/Utilities.h"
#include "Utils/easylogging++.h"
#include "HWDescription/BeBoard.h"
#include "HWInterface/RegManager.h"
#include <string>

namespace Ph2_HwInterface
{
class CommandProcessorInterface : public RegManager
{
  public: // constructors
    CommandProcessorInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    CommandProcessorInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    virtual ~CommandProcessorInterface(){};

  public: // Virtual functions
    virtual void Reset() { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function CommandProcessorInterface::Reset is absent" << RESET; }

    virtual void WriteCommand(const std::vector<uint32_t>& pCommand)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function CommandProcessorInterface::WriteCommand is absent" << RESET;
    }

    virtual std::vector<uint32_t> ReadReply(int pNWords)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function CommandProcessorInterface::ReadReply is absent" << RESET;
        return {};
    }
};
} // namespace Ph2_HwInterface

#endif
