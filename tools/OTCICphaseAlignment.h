/*!
 *
 * \file OTCICphaseAlignment.h
 * \brief OTCICphaseAlignment class
 * \author Fabio Ravera
 * \date 07/02/24
 *
 */

#ifndef OTCICphaseAlignment_h__
#define OTCICphaseAlignment_h__

#include "Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histrgrammer here
#include "DQMUtils/DQMHistogramOTCICphaseAlignment.h"
#endif

class OTCICphaseAlignment : public Tool
{
  public:
    OTCICphaseAlignment();
    ~OTCICphaseAlignment();

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
    void phaseAlignment();

    size_t fNumberOfLockCheckIterations {100};
    float fMinLockingSuccessRate {1.};

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTCICphaseAlignment fDQMHistogramOTCICphaseAlignment;
#endif
};

#endif
