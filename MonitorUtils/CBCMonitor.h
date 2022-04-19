#ifndef CBC_MONITOR_H
#define CBC_MONITOR_H

#include "../Utils/EmptyContainer.h"
#include "../Utils/OpticalGroupContainerStream.h"
#include "DetectorMonitor.h"
#ifdef __USE_ROOT__
#include "MonitorDQM/MonitorDQMPlotCBC.h"
#endif

class CBCMonitor : public DetectorMonitor
{
  public:
    CBCMonitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig);

  protected:
    void runMonitor() override;

  private:
    void runCBCRegisterMonitor(std::string registerName);
    void runLpGBTRegisterMonitor(std::string registerName);

#ifdef __USE_ROOT__
    MonitorDQMPlotCBC* fMonitorDQMPlotCBC;
#endif
};

#endif
