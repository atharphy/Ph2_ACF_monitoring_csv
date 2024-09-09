/*!
 *
 * \file OTBitErrorRateTest.h
 * \brief OTBitErrorRateTest class
 * \author Fabio Ravera
 * \date 02/07/24
 *
 */

#ifndef OTBitErrorRateTest_h__
#define OTBitErrorRateTest_h__

#include "tools/Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTBitErrorRateTest.h"
#endif

namespace Ph2_HwDescription
{
class OpticalGroup;
}

class OTBitErrorRateTest : public Tool
{
  public:
    OTBitErrorRateTest();
    ~OTBitErrorRateTest();

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
    void bitErrorRateTest();
    bool prepareLpGBTforBERT(Ph2_HwDescription::OpticalGroup* theOpticalGroup);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTBitErrorRateTest fDQMHistogramOTBitErrorRateTest;
#endif
};

#endif
