#ifndef DETECTOR_MONITOR_H
#define DETECTOR_MONITOR_H

#include "../System/SystemController.h"
#include "../Utils/ChannelContainerStream.h"
#include "../Utils/ChipContainerStream.h"
#include "../Utils/HybridContainerStream.h"
#include "../Utils/OpticalGroupContainerStream.h"
#include "../Utils/BoardContainerStream.h"
#include "DetectorMonitorConfig.h"

#include "chrono"
#include "thread"

#ifdef __USE_ROOT__
class TFile;
#include "../MonitorDQM/MonitorDQMPlotBase.h"
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
    TFile*              fOutputFile;
    MonitorDQMPlotBase* fMonitorPlotDQM;
#endif
    time_t      getTimeStamp();
    std::string getMonitorName();

    template <typename T, typename... H>
    ChannelContainerStream<T, H...> prepareChannelContainerStreamer(std::string appendName = "")
    {
        auto theContainerStreamer = std::unique_ptr<ChannelContainerStream<T, H...>>(new ChannelContainerStream<T, H...>(getMonitorName() + appendName));
        return theContainerStreamer;
    }

    template <typename T, typename C, typename... I>
    std::unique_ptr<ChipContainerStream<T, C, I...>> prepareChipContainerStreamer(std::string appendName = "")
    {
        auto theContainerStreamer = std::unique_ptr<ChipContainerStream<T, C, I...>>(new ChipContainerStream<T, C, I...>(getMonitorName() + appendName));
        return theContainerStreamer;
    }

    template <typename T, typename C, typename H, typename... I>
    HybridContainerStream<T, C, H, I...> prepareHybridContainerStreamer(std::string appendName = "")
    {
        auto theContainerStreamer = std::unique_ptr<HybridContainerStream<T, C, H, I...>>(new HybridContainerStream<T, C, H, I...>(getMonitorName() + appendName));
        return theContainerStreamer;
    }

    template <typename T, typename C, typename H, typename O, typename... I>
    std::unique_ptr<OpticalGroupContainerStream<T, C, H, O, I...>> prepareOpticalGroupContainerStreamer(std::string appendName = "")
    {
        auto theContainerStreamer = std::unique_ptr<OpticalGroupContainerStream<T, C, H, O, I...>>(new OpticalGroupContainerStream<T, C, H, O, I...>(getMonitorName() + appendName));
        return theContainerStreamer;
    }

    template <typename T, typename C, typename H, typename O, typename B, typename... I>
    std::unique_ptr<BoardContainerStream<T, C, H, O, B, I...>> prepareBoardContainerStreamer(std::string appendName = "")
    {
        auto theContainerStreamer = std::unique_ptr<BoardContainerStream<T, C, H, O, B, I...>>(new BoardContainerStream<T, C, H, O, B, I...>(getMonitorName() + appendName));
        return theContainerStreamer;
    }

  private:
    std::atomic<bool> fKeepRunning;
    std::atomic<bool> startMonitor;
    std::future<void> fMonitorFuture;
};

#endif
