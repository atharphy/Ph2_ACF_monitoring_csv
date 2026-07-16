#ifndef __REALTIME_MONITOR__
#define __REALTIME_MONITOR__

#include "tools/Tool.h"

#include <cstdint>
#include <string>

class RealtimeMonitor : public Tool
{
  public:
    RealtimeMonitor();
    ~RealtimeMonitor();

    void ConfigureCalibration() override;
    void Running() override;
    void Stop() override;

    static std::string fCalibrationDescription;

  private:
    bool hasEnabledMonitorElement() const;

    uint16_t fPrometheusPort{9101};
};

#endif
