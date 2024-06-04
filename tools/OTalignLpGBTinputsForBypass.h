/*!
 *
 * \file OTalignLpGBTinputsForBypass.h
 * \brief OTalignLpGBTinputsForBypass class
 * \author Fabio Ravera
 * \date 31/05/24
 *
 */

#ifndef OTalignLpGBTinputsForBypass_h__
#define OTalignLpGBTinputsForBypass_h__

#include "tools/Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTalignLpGBTinputsForBypass.h"
#endif
#include "Utils/PatternMatcher.h"

class OTalignLpGBTinputsForBypass : public Tool
{
  public:
    OTalignLpGBTinputsForBypass();
    ~OTalignLpGBTinputsForBypass();

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
    void    AlignLpGBTinputs();
    void    prepareForLpGBTalignmentPS();
    void    prepareForLpGBTalignment2Sstubs();
    void    prepareForLpGBTalignment2SL1();
    void    setCICBypass(uint8_t phyPort);
    uint8_t getBestPhase(const GenericDataArray<float, 15>& thePhaseEfficiencyList, Ph2_HwDescription::Hybrid* theHybrid, uint8_t line);
    float   getMatchingEfficiency2SL1(std::vector<uint32_t> inputDataVector);

    uint32_t fNumberOfIterations{1000};
    uint8_t  fShiftRegisterPatternMPA{0xAA};
    std::vector<uint8_t> fStubPattern2S{0x33, 0x55, 0xAA, 0xAA, 0xCC}; // last two bytes cannot be changed here

    PatternMatcher fPattern2SL1;

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTalignLpGBTinputsForBypass fDQMHistogramOTalignLpGBTinputsForBypass;
#endif
};

#endif
