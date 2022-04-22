#ifndef _D19cCommandProcessorInterface_H__
#define _D19cCommandProcessorInterface_H__

#include "CommandProcessorInterface.h"

namespace LpGBTSCWorker
{
const uint8_t BaseID            = 16;
const uint8_t SingleReadIC      = 2;
const uint8_t SingleWriteIC     = 3;
const uint8_t SingleByteReadI2C = 4;
const uint8_t MultiByteWriteI2C = 5;
const uint8_t SingleReadFE      = 6;
const uint8_t SingleWriteFE     = 7;
} // namespace LpGBTSCWorker

namespace Ph2_HwInterface
{
class D19cCommandProcessorInterface : public CommandProcessorInterface
{
  public:
    D19cCommandProcessorInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19cCommandProcessorInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~D19cCommandProcessorInterface();

  public:
    void                  Reset() override;
    void                  WriteCommand(const std::vector<uint32_t>& pCommand) override;
    std::vector<uint32_t> ReadReply(uint8_t pNWords) override;

    std::vector<uint32_t> EncodeCommand(uint8_t pFunctionId, Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem, bool pVerify = false);
    bool                  IsDone(uint8_t pFunctionId);
    uint8_t               GetTryCntr(uint8_t pFunctionId);
    uint16_t              GetStateFSM(uint8_t pFunctionId);
    void SelectLink(uint8_t pLinkId);
};
} // namespace Ph2_HwInterface
#endif
