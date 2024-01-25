#ifndef _CommandProcessorInterface_H__
#define _CommandProcessorInterface_H__

#include "Utils/Utilities.h"
#include "Utils/easylogging++.h"
#include <string>

namespace Ph2_HwInterface
{
class RegManager;
class CommandProcessorInterface
{
  public: // constructors
    CommandProcessorInterface(RegManager* theRegManager);
    virtual ~CommandProcessorInterface(){};

  public: // Virtual functions
    virtual void Reset();
    virtual void WriteCommand(const std::vector<uint32_t>& pCommand);
    virtual std::vector<uint32_t> ReadReply(int pNWords);

  protected:
    RegManager* fTheRegManager {nullptr};
};
} // namespace Ph2_HwInterface

#endif
