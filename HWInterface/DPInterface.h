
#ifndef __ROHInterface_H__
#define __ROHInterface_H__

#include "BeBoardFWInterface.h"

namespace Ph2_HwInterface
{
// Class to interface with ROH hybrid on TestCard
class DPInterface
{
  public:
    DPInterface();
    ~DPInterface();
    void Configure(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint32_t pPattern);
    // default config is for ps-feh data player
    void Start(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pType = 0);
    void Stop(Ph2_HwInterface::BeBoardFWInterface* pInterface);
    // default config is for ps-feh data player
    bool IsRunning(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pType = 0);
    void CheckNPatterns(Ph2_HwInterface::BeBoardFWInterface* pInterface);

  protected:
  private:
    bool     fEmulatorRunning;
    bool     fEmulatorConfigured;
    uint32_t fWait_us = 10;
};

} // namespace Ph2_HwInterface
#endif
