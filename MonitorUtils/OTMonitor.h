#ifndef __MONITOR_OT_H__
#define __MONITOR_OT_H__

#include "MonitorUtils/DetectorMonitor.h"

class OTMonitor : public DetectorMonitor
{
  public:
    OTMonitor(const Ph2_System::SystemController* theSystemController, const DetectorMonitorConfig& theDetectorMonitorConfig);

  protected:
    void runMonitorLpGBT(const std::string& registerName);
    virtual void readChipMonitorValue(const std::string& monitorValueName, Ph2_HwDescription::ReadoutChip* theChip, DetectorDataContainer& theDataContainer) = 0;
    DetectorDataContainer getReadoutChipMonitorValues(const std::string& monitorValueName, FrontEndType theFrontEndType);
};

#endif