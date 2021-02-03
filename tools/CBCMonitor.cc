#include "CBCMonitor.h"
#include "../HWDescription/OuterTrackerHybrid.h"
#include "chrono"
#include "thread"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

CBCMonitor::CBCMonitor(DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theDetectorMonitorConfig)
{
    doMonitorTemperature = fDetectorMonitorConfig.isElementToMonitor("ModuleTemperature");
}

void CBCMonitor::runMonitor()
{
    if(doMonitorTemperature) runTemperatureMonitor();
}

void CBCMonitor::runTemperatureMonitor()
{
    for(const auto& board: *fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            for(const auto& hybrid: *opticalGroup)
            {
                uint16_t cbcOrMpa = fCicInterface->ReadChipReg(static_cast<const OuterTrackerHybrid*>(hybrid)->fCic, "CBCMPA_SEL"); // just to read something
                LOG(INFO) << BOLDMAGENTA << "Hybrid " << hybrid->getId() << " - CBCMPA_SEL = " << cbcOrMpa << RESET;
            }
        }
    }
}
