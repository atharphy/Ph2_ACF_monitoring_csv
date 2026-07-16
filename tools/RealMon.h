#ifndef __REAL_MON__
#define __REAL_MON__

#include "tools/Tool.h"
#include <string>

class RealMon : public Tool
{
  public:
    RealMon();
    ~RealMon();

    void ConfigureCalibration() override;
    void Running() override;
    void Stop() override;

    static std::string fCalibrationDescription;

  private:
    bool hasEnabledMonitorElement() const;
};

#endif
