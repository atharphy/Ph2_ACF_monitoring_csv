/*!
 *
 * \file OTMeasureOccupancy.h
 * \brief OTMeasureOccupancy class
 * \author Fabio Ravera
 * \date 03/04/24
 *
 */

#ifndef OTMeasureOccupancy_h__
#define OTMeasureOccupancy_h__

#include "tools/Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTMeasureOccupancy.h"
#endif

class OTMeasureOccupancy : public Tool
{
  public:
    OTMeasureOccupancy();
    ~OTMeasureOccupancy();

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
    void measureChannelOccupancy();
    void prepareOccupancyMeasurement2S();
    void prepareOccupancyMeasurementPS();

    uint32_t fNumberOfEvents{10000};
    uint8_t  fCBCtestPulseValue{218};
    uint8_t  fSSAtestPulseValue{90};
    uint8_t  fMPAtestPulseValue{100};

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTMeasureOccupancy fDQMHistogramOTMeasureOccupancy;
#endif
};

#endif
