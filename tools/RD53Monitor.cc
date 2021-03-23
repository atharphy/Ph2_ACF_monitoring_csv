/*!
  \file                  RD53Monitor.cc
  \brief                 Implementaion of monitoring process
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Monitor.h"

RD53Monitor::RD53Monitor(const Ph2_System::SystemController& theSystCntr, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystCntr, theDetectorMonitorConfig)
{
    allVariables = fDetectorMonitorConfig.isElementToMonitor("AllVariables");
}

void RD53Monitor::runMonitor()
{
    if(allVariables == true)
        for(const auto cBoard: *theSystCntr.fDetectorContainer) theSystCntr.ReadSystemMonitor(cBoard, {"VOUT_ana_ShuLDO", "VOUT_dig_ShuLDO", "ADCbandgap", "Iref", "TEMPSENS_1", "TEMPSENS_4"});
}
