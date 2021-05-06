#ifndef CBC_MONITOR_H
#define CBC_MONITOR_H

#include "DetectorMonitor.h"
#include "Utils/EmptyContainer.h"
#ifdef __USE_ROOT__
#include "MonitorDQM/MonitorDQMPlotCBC.h"
#endif

class CBCMonitor : public DetectorMonitor
{
  public:
    CBCMonitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig);

  protected:
    void runMonitor() override;

  private:
    void runThresholdMonitor();
    bool fDoMonitorThreshold{false};
    #ifdef __USE_ROOT__
    MonitorDQMPlotCBC fMonitorPlotDQM;
    #endif

};

#endif
