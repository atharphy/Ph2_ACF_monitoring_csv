#include "tools/realtimemonitor.h"
#include "MonitorUtils/DetectorMonitor.h"
#include "Parser/DetectorMonitorConfig.h"
#include "Utils/ConsoleColor.h"
#include "Utils/easylogging++.h"

#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>

std::string RealtimeMonitor::fCalibrationDescription = "Realtime monitoring of XML-enabled detector registers";

void RealtimeMonitor::ConfigureCalibration()
{
    if(fDetectorMonitorConfig == nullptr) throw std::runtime_error("[RealtimeMonitor::ConfigureCalibration] Monitoring settings were not parsed");

    if(!fDetectorMonitorConfig->fEnable) throw std::runtime_error("[RealtimeMonitor::ConfigureCalibration] Monitoring is disabled in the XML configuration");

    if(!hasEnabledMonitorElement()) throw std::runtime_error("[RealtimeMonitor::ConfigureCalibration] No enabled MonitoringElement entries found in the XML configuration");

    if(fDetectorMonitor == nullptr) throw std::runtime_error("[RealtimeMonitor::ConfigureCalibration] Detector monitor was not created from the XML configuration");
}

void RealtimeMonitor::Running()
{
    ConfigureCalibration();

    LOG(INFO) << BOLDMAGENTA << "[RealtimeMonitor::Running] Starting realtime monitoring" << RESET;

    while(fKeepRunning) std::this_thread::sleep_for(std::chrono::milliseconds(100));

    LOG(INFO) << BOLDMAGENTA << "[RealtimeMonitor::Running] Realtime monitoring stopped" << RESET;
}

void RealtimeMonitor::Stop()
{
    if(fKeepRunning)
    {
        fKeepRunning = false;
        waitForRunToBeCompleted();

        try
        {
            if(fRunningFuture.valid()) fRunningFuture.get();
        }
        catch(const std::future_error&)
        {
            LOG(INFO) << "Ignoring future exception, future already retrieved";
        }
        catch(const std::exception& e)
        {
            throw std::runtime_error(e.what());
        }

        if(fDetectorMonitor != nullptr) fDetectorMonitor->stopMonitoring();

        dumpConfigFiles();
        SaveResults();
        WriteRootFile();
    }
}

bool RealtimeMonitor::hasEnabledMonitorElement() const
{
    for(const auto& monitorElementsForDevice: fDetectorMonitorConfig->fMonitorElementList)
        for(const auto& monitorElement: monitorElementsForDevice.second)
            if(monitorElement.second) return true;

    return false;
}
