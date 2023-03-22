#ifndef PS_MONITOR_H
#define PS_MONITOR_H

#include "MonitorUtils/DetectorMonitor.h"
#ifdef __USE_ROOT__
#include "MonitorDQM/MonitorDQMPlotPS.h"
#endif

class PSMonitor : public DetectorMonitor
{
  public:
    PSMonitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig);

  protected:
    void runMonitor() override;

  private:
    void runPSRegisterMonitor(std::string registerName);
    void runSSA2RegisterMonitor(std::string registerName);
    void runLpGBTRegisterMonitor(std::string registerName);

#ifdef __USE_ROOT__
    MonitorDQMPlotPS* fMonitorDQMPlotPS;
#endif
};

#endif
