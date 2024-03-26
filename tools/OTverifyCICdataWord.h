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

#include "tools/Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTverifyCICdataWord.h"
#endif

namespace Ph2_HwDescription
{
class ReadoutChip;
class BeBoard;
} // namespace Ph2_HwDescription

namespace Ph2_HwInterface
{
class D19cFWInterface;
}

class PatternMatcher;

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

  protected:
    void         runIntegrityTest();
    void         runStubIntegrityTest(Ph2_HwDescription::BeBoard* theBoard, Ph2_HwInterface::D19cFWInterface* theFWInterface);
    void         runL1IntegrityTest(Ph2_HwDescription::BeBoard* theBoard, Ph2_HwInterface::D19cFWInterface* theFWInterface);
    void         injectStubs2S(Ph2_HwDescription::ReadoutChip* theChip, uint8_t chipIdForCIC, Ph2_HwInterface::D19cFWInterface* theFWInterface, uint8_t numberOfBytesInSinglePacket);
    virtual void injectStubsPS(Ph2_HwDescription::ReadoutChip* theMPA, uint8_t chipIdForCIC, Ph2_HwInterface::D19cFWInterface* theFWInterface, uint8_t numberOfBytesInSinglePacket);
    float        injectAndMatch2SstubPatterns(Ph2_HwDescription::ReadoutChip*      theChip,
                                              uint8_t                              chipIdForCIC,
                                              Ph2_HwInterface::D19cFWInterface*    theFWInterface,
                                              uint8_t                              numberOfBytesInSinglePacket,
                                              std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVector);
    void         injectL12S(Ph2_HwDescription::ReadoutChip* theChip, uint8_t chipIdForCIC, Ph2_HwInterface::D19cFWInterface* theFWInterface, uint8_t numberOfBytesInSinglePacket);
    virtual void injectL1PS(Ph2_HwDescription::ReadoutChip* theMPA, uint8_t chipIdForCIC, Ph2_HwInterface::D19cFWInterface* theFWInterface, uint8_t numberOfBytesInSinglePacket);
    bool         matchL1Pattern(std::vector<uint32_t> theWordVector, PatternMatcher thePatternMatcher, uint8_t numberOfBytesInSinglePacket);
    bool         matchStubPattern(std::vector<uint32_t> theWordVector, PatternMatcher thePatternMatcher, uint8_t numberOfBytesInSinglePacket, size_t numberOfLines);

    std::vector<uint32_t> mergeCICStubOuput(const std::vector<std::vector<uint32_t>>& stubLineDataList, uint8_t numberOfBytesInSinglePacket);

    size_t fNumberOfIterations{1000};
    // For simplicity, make sure bendind code is always greater than half value (0x7)
    std::map<uint8_t, uint8_t> fBendingAndCode{{0, 0x9}, {2, 0xB}, {4, 0xF}};

    DetectorDataContainer fPatternMatchingEfficiencyContainer;

  private:
    void fillHistograms();

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTverifyCICdataWord fDQMHistogramOTverifyCICdataWord;
#endif
};

#endif
