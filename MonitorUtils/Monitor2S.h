#ifndef CBC_MONITOR_H
#define CBC_MONITOR_H

#include "MonitorUtils/DetectorMonitor.h"
#ifdef __USE_ROOT__
#include "MonitorDQM/MonitorDQMPlot2S.h"
#endif

class Monitor2S : public DetectorMonitor
{
  public:
    Monitor2S(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig);

  protected:
    void runMonitor() override;

  private:
    void runCBCRegisterMonitor(std::string registerName);
    void runLpGBTRegisterMonitor(std::string registerName);

#ifdef __USE_ROOT__
    MonitorDQMPlot2S* fMonitorDQMPlot2S;
#endif
};

#endif
