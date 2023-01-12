#ifndef SEH_MONITOR_H
#define SEH_MONITOR_H
#include "NetworkUtils/TCPClient.h"
#include "Utils/EmptyContainer.h"
#include "Utils/OpticalGroupContainerStream.h"
#include "MonitorUtils/DetectorMonitor.h"
#ifdef __USE_ROOT__
#include "MonitorDQM/MonitorDQMPlotCBC.h"
#include "MonitorDQM/MonitorDQMPlotSEH.h"
#endif
class SEHMonitor : public DetectorMonitor
{
  public:
    SEHMonitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig);
    virtual ~SEHMonitor();
    TCPClient* fPowerSupplyClient{nullptr};

  protected:
    void runMonitor() override;

  private:
    void runInputCurrentMonitor(std::string registerName);
    void runLpGBTRegisterMonitor(std::string registerName);
    void runPowerSupplyMonitor(std::string registerName);
    void runTestCardMonitor(std::string registerName);
// bool doMonitorInputCurrent{false};
#ifdef __USE_ROOT__
    MonitorDQMPlotSEH* fMonitorPlotDQMSEH;
    MonitorDQMPlotCBC* fMonitorDQMPlotCBC;
#endif

    std::string getVariableValue(std::string variable, std::string buffer);
};

#endif
