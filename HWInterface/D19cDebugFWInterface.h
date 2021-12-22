#ifndef _D19cDebugFWInterface_H__
#define __D19cDebugFWInterface_H__

#include "BeBoardFWInterface.h"
#include <string>

namespace Ph2_HwInterface
{
class D19cDebugFWInterface : public BeBoardFWInterface
{
  public:
    D19cDebugFWInterface(const char* puHalConfigFileName, uint32_t pBoardId);
    D19cDebugFWInterface(const char* puHalConfigFileName, uint32_t pBoardId, FileHandler* pFileHandler);

    D19cDebugFWInterface(const char* pId, const char* pUri, const char* pAddressTable);
    D19cDebugFWInterface(const char* pId, const char* pUri, const char* pAddressTable, FileHandler* pFileHandler);
    ~D19cDebugFWInterface();

  public:
    std::vector<std::string> StubDebug(bool pWithTestPulse = true, uint8_t pNlines = 6);
    std::string              L1ADebug(uint8_t pWait_ms = 100, bool pPrint = true);
    std::vector<std::string> ScopeStubLines(bool pWithTestPulse = true);

  private:
    void     ResetReadout();
    uint32_t fWait_us{100};
};
} // namespace Ph2_HwInterface
#endif
