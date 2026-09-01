#ifndef REALTIME_MONITOR_H
#define REALTIME_MONITOR_H

#include "tools/Tool.h"

#include <string>

class RealtimeMonitor : public Tool
{
  public:
    void ConfigureCalibration() override;
    void Running() override;
    void Stop() override;

    static std::string fCalibrationDescription;

  private:
    bool hasEnabledMonitorElement() const;
};

#endif
