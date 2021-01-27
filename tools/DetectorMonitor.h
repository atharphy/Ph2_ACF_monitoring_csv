#ifndef DETECTOR_MONITOR_H
#define DETECTOR_MONITOR_H

#include "../System/SystemController.h"
#include "../Utils/DetectorMonitorConfig.h"

class DetectorMonitor : public Ph2_System::SystemController
{
  public:
    DetectorMonitor(DetectorMonitorConfig theDetectorMonitorConfig);
    void operator()();
    void stopMonitoring() { fKeepRunning = false; }

  protected:
    virtual void          runMonitor() = 0;
    std::atomic<bool>     fKeepRunning{true};
    DetectorMonitorConfig fDetectorMonitorConfig;
};

#endif