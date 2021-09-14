#include "DetectorMonitor.h"

DetectorMonitor::DetectorMonitor(const Ph2_System::SystemController& theSystCntr, DetectorMonitorConfig theDetectorMonitorConfig)
    : theSystCntr(theSystCntr), fDetectorMonitorConfig(theDetectorMonitorConfig)
{
    fKeepRunning = true;
    startMonitor = false;
}

DetectorMonitor::~DetectorMonitor()
{
    LOG(INFO) << BOLDRED << "\t--> Destroying monitoring" << RESET;
    DetectorMonitor::stopRunning();
    while(fMonitorFuture.wait_for(std::chrono::milliseconds(fDetectorMonitorConfig.fSleepTimeMs)) != std::future_status::ready)
    { LOG(INFO) << GREEN << "\t-->Waiting for monitoring to be completed..." << RESET; }
}

void DetectorMonitor::operator()()
{
    while(fKeepRunning == true)
    {
        if(startMonitor == true) runMonitor();
        // LOG(INFO) << BOLDMAGENTA << "Running in the loop" << RESET;
        std::this_thread::sleep_for(std::chrono::milliseconds(fDetectorMonitorConfig.fSleepTimeMs));
    }
}

void DetectorMonitor::forkMonitor() { fMonitorFuture = std::async(std::launch::async, std::ref(*this)); }
