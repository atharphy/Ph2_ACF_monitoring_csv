/*!
  \file                  RD53Monitor.cc
  \brief                 Implementaion of monitoring process
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Monitor.h"

RD53Monitor::RD53Monitor(const Ph2_System::SystemController& theSystCntr, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystCntr, theDetectorMonitorConfig) {}

void RD53Monitor::runMonitor()
{
    if(fDetectorMonitorConfig.fMonitorElementList.empty()) return;

    for(const auto cBoard: *theSystCntr.fDetectorContainer) theSystCntr.ReadSystemMonitor(cBoard, fDetectorMonitorConfig.fMonitorElementList);
}
