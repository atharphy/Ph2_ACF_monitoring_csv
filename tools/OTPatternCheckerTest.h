/*!
 *
 * \file OTPatternCheckerTest.h
 * \brief OTPatternCheckerTest class
 * \author Fabio Ravera
 * \date 24/01/25
 *
 */

#ifndef OTPatternCheckerTest_h__
#define OTPatternCheckerTest_h__

#include "tools/OTalignBoardDataWord.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTPatternCheckerTest.h"
#endif

class OTPatternCheckerTest : public OTalignBoardDataWord
{
  public:
    OTPatternCheckerTest();
    ~OTPatternCheckerTest();

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
    void PatternCheckerTest(Ph2_HwDescription::BeBoard* theBoard, BoardDataContainer* thePatternTestContainer, float numberOfBits, float lineNumber);
    void PatternCheckerTest();
    void PatternCheckerTest(uint8_t line);

    float fNumberOfBits;

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTPatternCheckerTest fDQMHistogramOTPatternCheckerTest;
#endif
};

#endif
