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
    void printCICStrengthAndPhase();
    void itrOverCICStrength();
    void writeCBCReg();

    static std::string fCalibrationDescription;

  private:
    void                               setCBCshiftRegister();
    void                               runElectricChainValidation();
    std::vector<std::vector<uint32_t>> readCICbypassOutput(Ph2_HwDescription::Hybrid* theHybrid, Ph2_HwInterface::D19cFWInterface* theFWinterface, uint8_t phyPort);

    uint8_t            fShiftRegisterPattern{0xAA};
    uint32_t           fNumberOfIterations{100};
    std::vector<float> fListOfCBCslvsCurrents{1, 4, 7};

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTCBCtoCICecv fDQMHistogramOTCBCtoCICecv;
#endif
};

#endif
