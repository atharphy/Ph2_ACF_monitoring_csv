/*!
 *
 * \file OTCommonNoise.h
 * \brief OTCommonNoise class using Gamma function and correlation matrix techniques as described in https://inspirehep.net/literature/600273
 * \author Fabio Ravera
 * \date 17/09/21
 *
 */

#ifndef OTCommonNoise_h__
#define OTCommonNoise_h__

#include "Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histrgrammer here
#include "../DQMUtils/DQMHistogramOTCommonNoise.h"
#endif

class OTCommonNoise : public Tool
{
  public:
    OTCommonNoise();
    ~OTCommonNoise();

    void Initialise(void);

    // State machine
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;
    void Reset();

  private:
  
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTCommonNoise fDQMHistogramOTCommonNoise;
#endif
};

#endif
