/*!
 *
 * \file OTverifyBoardDataWord.h
 * \brief OTverifyBoardDataWord class
 * \author Fabio Ravera
 * \date 01/02/24
 *
 */

#ifndef OTverifyBoardDataWord_h__
#define OTverifyBoardDataWord_h__

#include "tools/Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTverifyBoardDataWord.h"
#endif

namespace Ph2_HwDescription
{
class BeBoard;
class OpticalGroup;
} // namespace Ph2_HwDescription

namespace Ph2_HwInterface
{
class D19cFWInterface;
}

class OTverifyBoardDataWord : public Tool
{
  public:
    OTverifyBoardDataWord();
    ~OTverifyBoardDataWord();

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
    void    runIntegrityTest();
    void    runStubIntegrityTest(Ph2_HwDescription::BeBoard* theBoard, Ph2_HwInterface::D19cFWInterface* theFWInterface);
    void    runL1IntegrityTest(Ph2_HwDescription::BeBoard* theBoard, Ph2_HwInterface::D19cFWInterface* theFWInterface);
    bool    isStubPatternMatched(const std::vector<uint32_t>& theWordVector, uint8_t numberOfBytesInSinglePacket);
    bool    isL1HeaderFound(const std::vector<uint32_t>& theWordVector, uint8_t numberOfBytesInSinglePacket);
    uint8_t getNumberOfBytesInSinglePacket(Ph2_HwDescription::OpticalGroup* cOpticalGroup) const;

    DetectorDataContainer fPatternMatchingEfficiencyContainer;
    size_t                fNumberOfIterations{1000};

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTverifyBoardDataWord fDQMHistogramOTverifyBoardDataWord;
#endif
};

#endif
