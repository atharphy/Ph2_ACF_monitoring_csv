#ifndef SEH_MONITOR_H
#define SEH_MONITOR_H

#include "DetectorMonitor.h"

class SEHMonitor : public DetectorMonitor
{
  public:
    SEHMonitor(const Ph2_System::SystemController& theSystCntr, DetectorMonitorConfig theDetectorMonitorConfig);

  protected:
    void runMonitor() override;

  private:
    void runInputCurrentMonitor();
    bool doMonitorInputCurrent{false};
};

#endif
