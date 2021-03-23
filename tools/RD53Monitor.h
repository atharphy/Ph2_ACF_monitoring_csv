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

#include "DetectorMonitor.h"

class RD53Monitor : public DetectorMonitor
{
  public:
    RD53Monitor(const Ph2_System::SystemController& theSystCntr, DetectorMonitorConfig theDetectorMonitorConfig);

  protected:
    void runMonitor() override;

  private:
    bool allVariables;
};

#endif
