/*!
  \file                  RD53Monitor.cc
  \brief                 Implementaion of monitoring process
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Monitor.h"

RD53Monitor::RD53Monitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig) 
: DetectorMonitor(theSystemController, theDetectorMonitorConfig) {}

void RD53Monitor::runMonitor()
{
    if(fDetectorMonitorConfig.fMonitorElementList.empty()) return;

    for(const auto cBoard: *fTheSystemController->fDetectorContainer) fTheSystemController->ReadSystemMonitor(cBoard, fDetectorMonitorConfig.fMonitorElementList);
}
