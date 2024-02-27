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
#include <vector>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTalignBoardDataWord.h"
#endif

namespace Ph2_HwInterface
{
class AlignerObject;
class LineConfiguration;
class D19cBackendAlignmentFWInterface;
class D19cDebugFWInterface;
} // namespace Ph2_HwInterface
namespace Ph2_HwDescription
{
class BeBoard;
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
    DetectorDataContainer fBitSlipContainer;
    DetectorDataContainer fAlignmentRetryContainer;

    void wordAlignBEdata();
    void stubAndL1WordAlignment(Ph2_HwDescription::BeBoard* theBoard);
    bool stubWordAlignment(const Ph2_HwDescription::OpticalGroup*            theOpticalGroup,
                           Ph2_HwInterface::D19cBackendAlignmentFWInterface* theAlignerInterface,
                           Ph2_HwInterface::D19cDebugFWInterface*            theDebugInterface);
    bool L1WordAlignment(const Ph2_HwDescription::OpticalGroup*            pOpticalGroup,
                         Ph2_HwInterface::D19cBackendAlignmentFWInterface* theAlignerInterface,
                         Ph2_HwInterface::D19cDebugFWInterface*            theDebugInterface);
    void manuallyConfigureLine(const Ph2_HwDescription::Chip* pChip, uint8_t pLineId, uint8_t pPhase, uint8_t pBitslip);
    bool tryLineAlignment(Ph2_HwInterface::D19cBackendAlignmentFWInterface* theAlignerInterface,
                          uint8_t                                           lineId,
                          Ph2_HwInterface::AlignerObject&                   theAlignerObject,
                          Ph2_HwInterface::LineConfiguration&               theLineConfiguration,
                          std::vector<uint8_t>&                             theHybridBitSlipVector,
                          std::vector<uint8_t>&                             theHybridAlignmentRetryVector);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTalignBoardDataWord fDQMHistogramOTalignBoardDataWord;
#endif
};

#endif
