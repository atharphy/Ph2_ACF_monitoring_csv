/*!
 *
 * \file OTPSADCCalibration.h
 * \brief OTPSADCCalibration class
 * \author Irene Zoi
 * \date 22/03/24
 *
 */

#ifndef OTPSADCCalibration_h__
#define OTPSADCCalibration_h__

#include "Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTPSADCCalibration.h"
#endif

class OTPSADCCalibration : public Tool
{
  public:
    OTPSADCCalibration();
    ~OTPSADCCalibration();

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
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTPSADCCalibration fDQMHistogramOTPSADCCalibration;
#endif
};

#endif
