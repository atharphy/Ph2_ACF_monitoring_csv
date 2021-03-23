#include "CBCMonitor.h"
#include "../HWDescription/OuterTrackerHybrid.h"

CBCMonitor::CBCMonitor(const Ph2_System::SystemController& theSystCntr, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystCntr, theDetectorMonitorConfig)
{
    doMonitorTemperature = fDetectorMonitorConfig.isElementToMonitor("ModuleTemperature");
}

void CBCMonitor::runMonitor()
{
    if(doMonitorTemperature) runTemperatureMonitor();
}

void CBCMonitor::runTemperatureMonitor()
{
    for(const auto& board: *theSystCntr.fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            for(const auto& hybrid: *opticalGroup)
            {
                uint16_t cbcOrMpa = theSystCntr.fCicInterface->ReadChipReg(static_cast<const Ph2_HwDescription::OuterTrackerHybrid*>(hybrid)->fCic, "CBCMPA_SEL"); // just to read something
                LOG(INFO) << BOLDMAGENTA << "Hybrid " << hybrid->getId() << " - CBCMPA_SEL = " << cbcOrMpa << RESET;
            }
        }
    }
}
