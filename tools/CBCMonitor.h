#ifndef CBC_MONITOR_H
#define CBC_MONITOR_H

#include "../tools/DetectorMonitor.h"

class CBCMonitor : public DetectorMonitor
{
  public:
    CBCMonitor(DetectorMonitorConfig theDetectorMonitorConfig);

  private:
    virtual void runMonitor() override;
    void         runTemperatureMonitor();
    bool         doMonitorTemperature{false};
};

#endif