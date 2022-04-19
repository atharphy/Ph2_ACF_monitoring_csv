#ifndef SEH_MONITOR_H
#define SEH_MONITOR_H
#include "../Utils/EmptyContainer.h"
#include "../Utils/OpticalGroupContainerStream.h"
#include "DetectorMonitor.h"
#ifdef __USE_ROOT__
#include "MonitorDQM/MonitorDQMPlotCBC.h"
#endif
#include "../MonitorUtils/DetectorMonitor.h"

class SEHMonitor : public DetectorMonitor
{
  public:
    SEHMonitor(const Ph2_System::SystemController* theSystCntr, DetectorMonitorConfig theDetectorMonitorConfig);

  protected:
    void runMonitor() override;

  private:
    void runInputCurrentMonitor(std::string registerName);
// bool doMonitorInputCurrent{false};
#ifdef __USE_ROOT__
    MonitorDQMPlotCBC* fMonitorDQMPlotCBC;
#endif
};

#endif
