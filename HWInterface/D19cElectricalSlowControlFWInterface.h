#ifndef _D19cElectricalSlowControlFWInterface_H__
#define __D19cElectricalSlowControlFWInterface_H__

#include "BeBoardFWInterface.h"
#include <string>

namespace Ph2_HwInterface
{
class D19cElectricalSlowControlFWInterface : public BeBoardFWInterface
{
  public:
    D19cElectricalSlowControlFWInterface(const char* puHalConfigFileName, uint32_t pBoardId);
    D19cElectricalSlowControlFWInterface(const char* puHalConfigFileName, uint32_t pBoardId, FileHandler* pFileHandler);

    D19cElectricalSlowControlFWInterface(const char* pId, const char* pUri, const char* pAddressTable);
    D19cElectricalSlowControlFWInterface(const char* pId, const char* pUri, const char* pAddressTable, FileHandler* pFileHandler);
    ~D19cElectricalSlowControlFWInterface();

  public:
    void EncodeReg(const Ph2_HwDescription::ChipRegItem& pRegItem, Ph2_HwDescription::Chip* pChip, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite) override;
    void BCEncodeReg(const Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t pNCbc, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite) override;
    void DecodeReg(Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t& pCbcId, uint32_t pWord, bool& pRead, bool& pFailed) override;

    bool WriteChipBlockReg(std::vector<uint32_t>& pVecReg, uint8_t& pWriteAttempts, bool pReadback) override;
    bool BCWriteChipBlockReg(std::vector<uint32_t>& pVecReg, bool pReadback) override;
    void ReadChipBlockReg(std::vector<uint32_t>& pVecReg);

    void ChipI2CRefresh();

  private:
    std::map<uint8_t, std::vector<uint32_t>> fI2CSlaveMap;
    void                                     ReadErrors();
    bool                                     WriteI2C(std::vector<uint32_t>& pVecSend, std::vector<uint32_t>& pReplies, bool pReadback, bool pBroadcast);
    bool                                     ReadI2C(uint32_t pNReplies, std::vector<uint32_t>& pReplies);
    // binary predicate for comparing sent I2C commands with replies using std::mismatch
    static bool    cmd_reply_comp(const uint32_t& cWord1, const uint32_t& cWord2);
    static bool    cmd_reply_ack(const uint32_t& cWord1, const uint32_t& cWord2);
    const uint32_t SINGLE_I2C_WAIT = 200; // used for 1MHz I2C
};
} // namespace Ph2_HwInterface
#endif
