/*!
  \file                  RD53Monitor.h
  \brief                 Implementaion of monitoring process
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53Monitor_H
#define RD53Monitor_H

#include "../Utils/CharArray.h"
#include "../Utils/ContainerFactory.h"
#include "DetectorMonitor.h"

#ifdef __USE_ROOT__
#include "../MonitorDQM/MonitorDQMPlotRD53.h"
#endif

class RD53Monitor : public DetectorMonitor
{
  public:
    RD53Monitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig);

  private:
    void runMonitor() override;

    void runRD53RegisterMonitor(const std::string& registerName);
    void runLpGBTRegisterMonitor(const std::string& registerName);
    void sendData(DetectorDataContainer& theRegisterContainer, const std::string& registerName);

#ifdef __USE_ROOT__
    MonitorDQMPlotRD53* fMonitorDQM{nullptr};
#endif
};

#endif
