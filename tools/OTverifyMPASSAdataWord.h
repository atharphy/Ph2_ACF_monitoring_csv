/*!
 *
 * \file OTverifyMPASSAdataWord.h
 * \brief OTverifyMPASSAdataWord class
 * \author Fabio Ravera
 * \date 07/03/24
 *
 */

#ifndef OTverifyMPASSAdataWord_h__
#define OTverifyMPASSAdataWord_h__

#include "tools/Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTverifyMPASSAdataWord.h"
#endif
#include "tools/OTverifyCICdataWord.h"

class OTverifyMPASSAdataWord : public OTverifyCICdataWord
{
  public:
    OTverifyMPASSAdataWord();
    ~OTverifyMPASSAdataWord();

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
    void                  fillHistograms();
    void                  injectStubsPS(Ph2_HwDescription::ReadoutChip* theMPA, uint8_t chipIdForCIC, Ph2_HwInterface::D19cFWInterface* theFWInterface, uint8_t numberOfBytesInSinglePacket) override;
    void                  injectL1PS(Ph2_HwDescription::ReadoutChip* theMPA, uint8_t chipIdForCIC, Ph2_HwInterface::D19cFWInterface* theFWInterface, uint8_t numberOfBytesInSinglePacket) override;
    DetectorDataContainer fPatternMatchingEfficiencyContainer;

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTverifyMPASSAdataWord fDQMHistogramOTverifyMPASSAdataWord;
#endif
};

#endif
