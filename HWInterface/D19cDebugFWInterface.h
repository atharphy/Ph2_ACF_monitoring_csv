#ifndef _D19cDebugFWInterface_H__
#define __D19cDebugFWInterface_H__

#include "HWDescription/BeBoard.h"
#include "Utils/Utilities.h"
#include "Utils/easylogging++.h"
#include "HWInterface/RegManager.h"
#include <string>

namespace Ph2_HwInterface
{
class D19cDebugFWInterface : public RegManager
{
  public:
    D19cDebugFWInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19cDebugFWInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);

    ~D19cDebugFWInterface();

  public:
    std::vector<std::string> StubDebug(bool pWithTestPulse = true, uint8_t pNlines = 6, bool pPrint = true);
    std::string              L1ADebug(uint8_t pWait_ms = 1, bool pPrint = true);
    std::vector<std::string> ScopeStubLines(bool pWithTestPulse = true);
};
} // namespace Ph2_HwInterface
#endif
