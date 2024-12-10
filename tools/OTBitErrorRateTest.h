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

#include "tools/OTalignBoardDataWord.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTBitErrorRateTest.h"
#endif

namespace Ph2_HwDescription
{
class OpticalGroup;
class BeBoard;
}

class OTBitErrorRateTest : public OTalignBoardDataWord
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
    void bitErrorRateTestOld();
    bool prepareLpGBTforBERT(Ph2_HwDescription::OpticalGroup* theOpticalGroup);
    void writeWithComment(Ph2_HwDescription::BeBoard* theBoard, const std::string& registerName, uint32_t registerValue, const std::string& comment);
    void readForAllLines(Ph2_HwDescription::BeBoard* theBoard, const std::string& controlRegisterName, uint32_t controlRegisterValue, const std::string& controlComment, const std::string& statusRegisterName, const std::string& statusComment);
    void runBitErrorRateTest(Ph2_HwDescription::BeBoard* theBoard);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTBitErrorRateTest fDQMHistogramOTBitErrorRateTest;
#endif
};

#endif
