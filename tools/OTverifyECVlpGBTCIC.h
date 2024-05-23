/*!
 *
 * \file OTverifyECVlpGBTCIC.h
 * \brief OTverifyECVlpGBTCIC class
 * \author Irene Zoi
 * \date 23/05/24
 *
 */

#ifndef OTverifyECVlpGBTCIC_h__
#define OTverifyECVlpGBTCIC_h__

#include "tools/Tool.h"
#include "tools/OTverifyBoardDataWord.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTverifyECVlpGBTCIC.h"
#endif

class OTverifyECVlpGBTCIC : public OTverifyBoardDataWord
{
  public:
    OTverifyECVlpGBTCIC();
    ~OTverifyECVlpGBTCIC();

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
    void runECV();
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTverifyECVlpGBTCIC fDQMHistogramOTverifyECVlpGBTCIC;
#endif
};

#endif
