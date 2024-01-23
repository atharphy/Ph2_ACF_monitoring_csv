/*!
 *
 * \file OTalignBoardDataWord.h
 * \brief OTalignBoardDataWord class
 * \author Fabio Ravera
 * \date 19/01/24
 *
 */

#ifndef OTalignBoardDataWord_h__
#define OTalignBoardDataWord_h__

#include "Tool.h"
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histrgrammer here
#include "DQMUtils/DQMHistogramOTalignBoardDataWord.h"
#endif

namespace Ph2_HwDescription
{
    class BeBoard;
    class OpticalGroup;
}

class OTalignBoardDataWord : public Tool
{
  public:
    OTalignBoardDataWord();
    ~OTalignBoardDataWord();

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
    DetectorDataContainer fBeBitSlip;

    void WordAlignBEdata();
    bool WordAlignBEdata(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool L1WordAlignment(const Ph2_HwDescription::OpticalGroup* pOpticalGroup, bool pScope);
    std::pair<bool, uint8_t> PhaseTuneLine(const Ph2_HwDescription::Chip* pChip, uint8_t pLineId);
    void ManuallyConfigureLine(const Ph2_HwDescription::Chip* pChip, uint8_t pLineId, uint8_t pPhase, uint8_t pBitslip);
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTalignBoardDataWord fDQMHistogramOTalignBoardDataWord;
#endif
};

#endif
