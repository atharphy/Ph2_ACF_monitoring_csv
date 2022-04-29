#ifndef _D19cCommandProcessorInterface_H__
#define _D19cCommandProcessorInterface_H__

#include "CommandProcessorInterface.h"

namespace Ph2_HwInterface
{
class D19cCommandProcessorInterface : public CommandProcessorInterface
{
  public:
    D19cCommandProcessorInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19cCommandProcessorInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~D19cCommandProcessorInterface();

  public:
    // void                  Reset() override;
    void                  WriteCommand(const std::vector<uint32_t>& pCommand) override;
    std::vector<uint32_t> ReadReply(uint8_t pNWords) override;
};
} // namespace Ph2_HwInterface
#endif
