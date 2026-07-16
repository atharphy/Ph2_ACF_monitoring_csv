#include "tools/RealMon.h"
#include "MonitorUtils/DetectorMonitor.h"
#include "Parser/DetectorMonitorConfig.h"
#include "Utils/ConsoleColor.h"
#include "Utils/easylogging++.h"

#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>

std::string RealMon::fCalibrationDescription = "Realtime monitoring of XML-enabled detector registers";

RealMon::RealMon() : Tool() {}

RealMon::~RealMon() {}

void RealMon::ConfigureCalibration()
{
    if(fDetectorMonitorConfig == nullptr) throw std::runtime_error("[RealMon::ConfigureCalibration] Monitoring settings were not parsed");

    if(fDetectorMonitorConfig->fEnable == false) throw std::runtime_error("[RealMon::ConfigureCalibration] Monitoring is disabled in the XML configuration");

    if(hasEnabledMonitorElement() == false) throw std::runtime_error("[RealMon::ConfigureCalibration] No enabled MonitoringElement entries found in the XML configuration");

    if(fDetectorMonitor == nullptr) throw std::runtime_error("[RealMon::ConfigureCalibration] Detector monitor was not created from the XML configuration");
}

void RealMon::Running()
{
    ConfigureCalibration();

    LOG(INFO) << BOLDMAGENTA << "[RealMon::Running] Starting realtime monitoring" << RESET;

    fDetectorMonitor->startMonitoring();

    while(Tool::fKeepRunning == true) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }

    fDetectorMonitor->stopMonitoring();

    LOG(INFO) << BOLDMAGENTA << "[RealMon::Running] Realtime monitoring stopped" << RESET;
}

void RealMon::Stop()
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

bool RealMon::hasEnabledMonitorElement() const
{
    for(const auto& monitorElementList: fDetectorMonitorConfig->fMonitorElementList)
        for(const auto& monitorElement: monitorElementList.second)
            if(monitorElement.second == true) return true;

    return false;
}
