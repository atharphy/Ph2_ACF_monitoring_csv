/*!
 *
 * \file OTinjectionDelayOptimization.h
 * \brief OTinjectionDelayOptimization class
 * \author Fabio Ravera
 * \date 15/03/24
 *
 */

#ifndef OTinjectionDelayOptimization_h__
#define OTinjectionDelayOptimization_h__

#include "Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTinjectionDelayOptimization.h"
#endif

class OTinjectionDelayOptimization : public Tool
{
  public:
    OTinjectionDelayOptimization();
    ~OTinjectionDelayOptimization();

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
    void optimizeInjectionDelay();
    void injectionDelayScan2S();
    void injectionDelayScanPS();

    uint32_t fNumberOfEvents {100};
    uint8_t fCbcTestPulseValue {150};
    float fCbcNumberOfSigmaNoiseAwayFromPedestal {10.};
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTinjectionDelayOptimization fDQMHistogramOTinjectionDelayOptimization;
#endif
};

#endif
