/*!
 *
 * \file OTCBCtoCICecv.h
 * \brief OTCBCtoCICecv class
 * \author Fabio Ravera
 * \date 28/05/24
 *
 */

#ifndef OTCBCtoCICecv_h__
#define OTCBCtoCICecv_h__

#include "tools/Tool.h"
#include "tools/OTCicBypassTest.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTCBCtoCICecv.h"
#endif
#include "Utils/PatternMatcher.h"

namespace Ph2_HwDescription
{
class Hybrid;
}

namespace Ph2_HwInterface
{
class D19cFWInterface;
}

class OTCBCtoCICecv : public OTCicBypassTest
{
  public:
    OTCBCtoCICecv();
    ~OTCBCtoCICecv();

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
    void printCICStrengthAndPhase();
    void itrOverCICStrength();
    void itrOverCBCStrength();
    void                               setCBCshiftRegister();
    void                               runElectricChainValidation();
    void                               runOTCBCtoCICecv();
    std::vector<std::vector<uint32_t>> readCICbypassOutput(Ph2_HwDescription::Hybrid* theHybrid, Ph2_HwInterface::D19cFWInterface* theFWinterface, uint8_t phyPort);

    uint8_t            fShiftRegisterPattern{0xAA};
    uint32_t           fNumberOfIterations{100};
    std::vector<float> fListOfCBCslvsCurrents{1, 4, 7};

    //////////////////////// From OTalignLpGBTinputsForBypass.cc //////////////////////////
    void    prepareForLpGBTalignment2Sstubs();
    void    prepareForLpGBTalignment2SL1();
    float   getMatchingEfficiency2SL1(std::vector<uint32_t> inputDataVector);
    void    setCICBypass(uint8_t phyPort);

    uint8_t              fShiftRegisterPatternMPA{0xAA};
    std::vector<uint8_t> fStubPattern2S{0x33, 0x55, 0xAA, 0xAA, 0xAA}; // last two bytes cannot be changed here

    PatternMatcher fPattern2SL1;
    ///////////////////////////////////////////////////////////////////////////////////////

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTCBCtoCICecv fDQMHistogramOTCBCtoCICecv;
#endif
};

#endif
