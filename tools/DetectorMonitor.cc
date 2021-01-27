#include "DetectorMonitor.h"
#include "chrono"
#include "thread"

DetectorMonitor::DetectorMonitor(DetectorMonitorConfig theDetectorMonitorConfig) : fDetectorMonitorConfig(theDetectorMonitorConfig) {}

void DetectorMonitor::operator()()
{
    while(fKeepRunning)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(fDetectorMonitorConfig.fSleepTimeMs));
        runMonitor();
        std::this_thread::sleep_for(std::chrono::milliseconds(fDetectorMonitorConfig.fSleepTimeMs));
    }
}
