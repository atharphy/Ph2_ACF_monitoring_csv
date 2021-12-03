#ifndef DETECTOR_MONITOR_H
#define DETECTOR_MONITOR_H

#include "../System/SystemController.h"
#include "../Utils/ContainerStream.h"
#include "DetectorMonitorConfig.h"

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
    void forkMonitor();
    void operator()();
    void startMonitoring() { startMonitor = true; }
    void stopMonitoring() { startMonitor = false; }
    void stopRunning() { fKeepRunning = false; }

  protected:
    virtual void                        runMonitor() = 0;
    const Ph2_System::SystemController* fTheSystemController{nullptr};
    DetectorMonitorConfig               fDetectorMonitorConfig;
#ifdef __USE_ROOT__
    TFile* fOutputFile;
    MonitorDQMPlotBase *fMonitorPlotDQM;
#endif
    time_t      getTimeStamp();
    std::string getMonitorName();

    template <typename T, typename... H>
    ChannelContainerStream<T, H...> prepareChannelContainerStreamer(std::string appendName = "")
    {
        ChannelContainerStream<T, H...> theContainerStreamer(getMonitorName() + appendName);
        return theContainerStreamer;
    }

    template <typename T, typename C, typename... H>
    ChipContainerStream<T, C, H...> prepareChipContainerStreamer(std::string appendName = "")
    {
        ChipContainerStream<T, C, H...> theContainerStreamer(getMonitorName() + appendName);
        return theContainerStreamer;
    }

    template <typename T, typename C, typename M, typename... H>
    HybridContainerStream<T, C, M, H...> prepareHybridContainerStreamer(std::string appendName = "")
    {
        HybridContainerStream<T, C, M, H...> theContainerStreamer(getMonitorName() + appendName);
        return theContainerStreamer;
    }

    // not yet available...
    // template <typename T, typename C, typename M, typename O, typename... H>
    // OpticalGroupContainerStream<T, C, M, O, H...> prepareOpticalGroupContainerStreamer(std::string appendName = "")
    // {
    //     OpticalGroupContainerStream<T, C, M, O, H...> theContainerStreamer(getMonitorName() + appendName);
    //     return theContainerStreamer;
    // }

  private:
    std::atomic<bool> fKeepRunning;
    std::atomic<bool> startMonitor;
    std::future<void> fMonitorFuture;
};

#endif
