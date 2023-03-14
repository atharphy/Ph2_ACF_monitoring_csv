#ifndef DETECTOR_MONITOR_H
#define DETECTOR_MONITOR_H

#include "Parser/DetectorMonitorConfig.h"
#include "System/SystemController.h"

#include "chrono"
#include "thread"

#ifdef __USE_ROOT__
class TFile;
#include "MonitorDQM/MonitorDQMPlotBase.h"
#endif

class DetectorMonitor
{
  public:
    DetectorMonitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig);
    virtual ~DetectorMonitor();
    void        forkMonitor();
    void        operator()();
    void        startMonitoring() { startMonitor = true; }
    void        stopMonitoring() { startMonitor = false; }
    void        stopRunning() { fKeepRunning = false; }
    std::string getMonitorFileName();
#if defined(__TCUSB__)
    void setTestCardPointer(TC_2SSEH* cTC_2SSEH) { pTC_2SSEH = cTC_2SSEH; };
#endif
#if defined(__TCUSB__)
    TC_2SSEH* pTC_2SSEH{nullptr};
#endif

  protected:
    virtual void                        runMonitor() = 0;
    const Ph2_System::SystemController* fTheSystemController{nullptr};
    DetectorMonitorConfig               fDetectorMonitorConfig;
#ifdef __USE_ROOT__
    TFile*              fOutputFile{nullptr};
    MonitorDQMPlotBase* fMonitorPlotDQM{nullptr};
    std::string         fMonitorFileName = "";
#endif
    time_t      getTimeStamp();
    std::string getMonitorName();

  private:
    std::atomic<bool> fKeepRunning;
    std::atomic<bool> startMonitor;
    std::future<void> fMonitorFuture;
};

#endif
