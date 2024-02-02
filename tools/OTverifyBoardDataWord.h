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

#include "Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histrgrammer here
#include "DQMUtils/DQMHistogramOTverifyBoardDataWord.h"
#endif

namespace Ph2_HwDescription
{
class BeBoard;
}

namespace Ph2_HwInterface
{
class D19cDebugFWInterface;
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
    void runIntegrityTest();
    void runStubIntegrityTest(Ph2_HwDescription::BeBoard* theBoard, Ph2_HwInterface::D19cDebugFWInterface* theDebugInterface);
    void runL1IntegrityTest(Ph2_HwDescription::BeBoard* theBoard, Ph2_HwInterface::D19cDebugFWInterface* theDebugInterface);
    bool isStubPatternMatched(const std::vector<uint32_t>& theWordVector);
    bool isL1HeaderFound(const std::vector<uint32_t>&  theWordVector);

    DetectorDataContainer fPatternMatchingEfficiencyContainer;
    size_t                fNumberOfIterations{1000};

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTverifyBoardDataWord fDQMHistogramOTverifyBoardDataWord;
#endif
};

#endif
