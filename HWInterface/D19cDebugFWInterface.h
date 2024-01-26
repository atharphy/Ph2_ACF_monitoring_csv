#ifndef _D19cDebugFWInterface_H__
#define __D19cDebugFWInterface_H__

#include <string>
#include <vector>
namespace Ph2_HwInterface
{

class RegManager;
class D19cDebugFWInterface
{
  public:
    D19cDebugFWInterface(RegManager* theRegManager);

    ~D19cDebugFWInterface();

  public:
    std::vector<std::string> StubDebug(bool pWithTestPulse = true, uint8_t pNlines = 6, bool pPrint = true);
    std::string              L1ADebug(uint8_t pWait_ms = 1, bool pPrint = true);
    std::vector<std::string> ScopeStubLines(bool pWithTestPulse = true);
    RegManager* fTheRegManager {nullptr};
};
} // namespace Ph2_HwInterface
#endif
