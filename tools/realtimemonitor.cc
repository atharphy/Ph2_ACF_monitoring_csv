#include "tools/realtimemonitor.h"
#include "MonitorUtils/DetectorMonitor.h"
#include "MonitorUtils/PrometheusExporter.h"
#include "Parser/DetectorMonitorConfig.h"
#include "Utils/ConsoleColor.h"
#include "Utils/easylogging++.h"

#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>

std::string RealtimeMonitor::fCalibrationDescription = "Realtime monitoring of XML-enabled detector registers";

RealtimeMonitor::RealtimeMonitor() : Tool() {}

RealtimeMonitor::~RealtimeMonitor() {}

void RealtimeMonitor::ConfigureCalibration()
{
    if(fDetectorMonitorConfig == nullptr) throw std::runtime_error("[RealtimeMonitor::ConfigureCalibration] Monitoring settings were not parsed");

    if(fDetectorMonitorConfig->fEnable == false) throw std::runtime_error("[RealtimeMonitor::ConfigureCalibration] Monitoring is disabled in the XML configuration");

    if(hasEnabledMonitorElement() == false) throw std::runtime_error("[RealtimeMonitor::ConfigureCalibration] No enabled MonitoringElement entries found in the XML configuration");

    if(fDetectorMonitor == nullptr) throw std::runtime_error("[RealtimeMonitor::ConfigureCalibration] Detector monitor was not created from the XML configuration");

    const double configuredPort = findValueInSettings<double>("PrometheusPort", 9101);
    if(configuredPort < 1 || configuredPort > 65535) throw std::runtime_error("[RealtimeMonitor::ConfigureCalibration] PrometheusPort must be between 1 and 65535");
    fPrometheusPort = static_cast<uint16_t>(configuredPort);
}

void RealtimeMonitor::Running()
{
    ConfigureCalibration();

    LOG(INFO) << BOLDMAGENTA << "[RealtimeMonitor::Running] Starting realtime monitoring" << RESET;

    PrometheusExporter::getInstance().start(fPrometheusPort);
    LOG(INFO) << BOLDMAGENTA << "[RealtimeMonitor::Running] Prometheus metrics available at http://0.0.0.0:" << fPrometheusPort << "/metrics" << RESET;

    fDetectorMonitor->startMonitoring();

    while(Tool::fKeepRunning == true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }

    fDetectorMonitor->stopMonitoring();
    PrometheusExporter::getInstance().stop();

    LOG(INFO) << BOLDMAGENTA << "[RealtimeMonitor::Running] Realtime monitoring stopped" << RESET;
}

void RealtimeMonitor::Stop()
{
    if(Tool::fKeepRunning == true)
    {
        Tool::fKeepRunning = false;
        Tool::waitForRunToBeCompleted();

        try
        {
            if(fRunningFuture.valid()) fRunningFuture.get();
        }
        catch(const std::future_error& e)
        {
            LOG(INFO) << "Ignoring future exception, future already retrieved";
        }
        catch(const std::exception& e)
        {
            throw std::runtime_error(e.what());
        }

        if(fDetectorMonitor != nullptr) fDetectorMonitor->stopMonitoring();

        Tool::dumpConfigFiles();
        Tool::SaveResults();
        Tool::WriteRootFile();
    }
}

bool RealtimeMonitor::hasEnabledMonitorElement() const
{
    for(const auto& monitorElementList: fDetectorMonitorConfig->fMonitorElementList)
        for(const auto& monitorElement: monitorElementList.second)
            if(monitorElement.second == true) return true;

    return false;
}
