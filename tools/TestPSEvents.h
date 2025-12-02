/*!
 *
 * \file TestPSEvents.h
 * \brief TestPSEvents class
 * \author Fabio Ravera
 * \date 25/11/24
 *
 */

#ifndef TestPSEvents_h__
#define TestPSEvents_h__

#include "tools/OTinjectionDelayOptimization.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramTestPSEvents.h"
#endif

class TestPSEvents : public OTinjectionDelayOptimization
{
  public:
    TestPSEvents();
    ~TestPSEvents();

    void Initialise(void);

    // State machine
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;
    void Reset();

    static std::string fCalibrationDescription;

  private:
    void testEvents(uint8_t injectedRow, uint8_t injectedCol, const std::vector<uint8_t>& listOfInjectedChipId);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramTestPSEvents fDQMHistogramTestPSEvents;
#endif
};

#endif
