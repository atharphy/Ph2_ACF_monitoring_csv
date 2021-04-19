#ifndef DETECTOR_MONITOR_H
#define DETECTOR_MONITOR_H

#include "../System/SystemController.h"
#include "../Utils/DetectorMonitorConfig.h"

#include "chrono"
#include "thread"

class DetectorMonitor
{
  public:
    DetectorMonitor(const Ph2_System::SystemController& theSystCntr, DetectorMonitorConfig theDetectorMonitorConfig);
    virtual ~DetectorMonitor();
    void forkMonitor();
    void operator()();
    void startMonitoring() { startMonitor = true; }
    void stopMonitoring() { startMonitor = false; }
    void stopRunning() { fKeepRunning = false; }

  protected:
    virtual void                        runMonitor() = 0;
    const Ph2_System::SystemController& theSystCntr;
    DetectorMonitorConfig               fDetectorMonitorConfig;

  private:
    std::atomic<bool> fKeepRunning;
    std::atomic<bool> startMonitor;
    std::future<void> fMonitorFuture;
};

#endif
