/*!
 *
 * \file OTverifyCICdataWord.h
 * \brief OTverifyCICdataWord class
 * \author Fabio Ravera
 * \date 14/02/24
 *
 */

#ifndef OTverifyCICdataWord_h__
#define OTverifyCICdataWord_h__

#include "Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTverifyCICdataWord.h"
#endif

namespace Ph2_HwDescription
{
    class ReadoutChip;
    class BeBoard;
}

namespace Ph2_HwInterface
{
    class D19cDebugFWInterface;
}

class OTverifyCICdataWord : public Tool
{
  public:
    OTverifyCICdataWord();
    ~OTverifyCICdataWord();

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
    void injectStubs2S(Ph2_HwDescription::ReadoutChip* theChip, Ph2_HwInterface::D19cDebugFWInterface* theDebugInterface, uint8_t numberOfBytesInSinglePacket);
    void injectStubsPS(Ph2_HwDescription::ReadoutChip* theChip, Ph2_HwInterface::D19cDebugFWInterface* theDebugInterface, uint8_t numberOfBytesInSinglePacket);
    void    runIntegrityTest();
    void    runStubIntegrityTest(Ph2_HwDescription::BeBoard* theBoard, Ph2_HwInterface::D19cDebugFWInterface* theDebugInterface);
    void    runL1IntegrityTest(Ph2_HwDescription::BeBoard* theBoard, Ph2_HwInterface::D19cDebugFWInterface* theDebugInterface);

    size_t fNumberOfIterations {1};
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTverifyCICdataWord fDQMHistogramOTverifyCICdataWord;
#endif
};

#endif
