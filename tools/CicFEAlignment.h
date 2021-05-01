/*!
 *
 * \file CicFEAlignment.h
 * \brief CIC FE alignment class, automated alignment procedure for CICs
 * connected to FEs
 * \author Sarah SEIF EL NASR-STOREY
 * \date 28 / 06 / 19
 *
 * \Support : sarah.storey@cern.ch
 *
 */

#ifndef CicFEAlignment_h__
#define CicFEAlignment_h__

#include "Tool.h"
#include <map>
// #ifdef __USE_ROOT__

#ifndef PortAlignment
typedef std::vector<uint8_t> PortAlignment;
#endif
#ifndef PortAlignmentVals
typedef std::vector<PortAlignment> PortAlignmentVals;
#endif

// add break codes here
const uint8_t FAILED_PHASE_ALIGNMENT = 1;
const uint8_t FAILED_WORD_ALIGNMENT  = 2;
const uint8_t FAILED_BX_ALIGNMENT    = 3;

class CicFEAlignment : public Tool
{
    using RegisterVector      = std::vector<std::pair<std::string, uint8_t>>;
    using TestGroupChannelMap = std::map<int, std::vector<uint8_t>>;

  public:
    CicFEAlignment();
    ~CicFEAlignment();

    void                              Initialise();
    void                              SetStaticPhaseAlignment();
    bool                              PhaseAlignment(uint16_t pWait_ms = 10, uint32_t pNTriggers = 500);
    bool                              ManualPhaseAlignment(uint16_t pPhase = 10);
    bool                              WordAlignment(uint16_t pWait_ms = 100);
    bool                              Bx0Alignment(uint8_t pFe = 0, uint8_t pLine = 4, uint16_t pDelay = 1, uint16_t pWait_ms = 100, int cNrials = 3);
    bool                              SetBx0Delay(uint8_t pDelay = 8, uint8_t pStubPackageDelay = 3);
    bool                              BackEndAlignment();
    bool                              PhaseAlignmentMPA(uint16_t pWait_ms = 100);
    void                              Running() override;
    void                              Stop() override;
    void                              Pause() override;
    void                              Resume() override;
    void                              Reset();
    void                              InjectAlignmentPattern(uint8_t pChipId, uint8_t pPhyPort);
    void                              writeObjects();

    // injection
    void WordAlignmentPattern(Ph2_HwDescription::ReadoutChip* pChip, std::vector<uint8_t> pAlignmentPatterns);
    // get alignment results
    uint8_t getPhaseAlignmentValue(Ph2_HwDescription::BeBoard* pBoard, Ph2_HwDescription::OpticalGroup* pGroup, Ph2_HwDescription::Hybrid* pFe, Ph2_HwDescription::ReadoutChip* pChip, uint8_t pLine);
    uint8_t getWordAlignmentValue(Ph2_HwDescription::BeBoard* pBoard, Ph2_HwDescription::OpticalGroup* pGroup, Ph2_HwDescription::Hybrid* pFe, Ph2_HwDescription::ReadoutChip* pChip, uint8_t pLine);
    bool    getStatus() const { return fSuccess; }

  protected:
  private:
    // status
    bool fSuccess;
    // Containers
    DetectorDataContainer fThresholds, fLogic, fHIPs, fPtCuts;
    DetectorDataContainer fPhaseAlignmentValues;
    DetectorDataContainer fWordAlignmentValues;
    DetectorDataContainer fRegMapContainer;
    DetectorDataContainer fBoardRegContainer;
    // with MPA
    bool fWithMPA;

    // mapping of FEs for CIC
    std::vector<uint8_t> fFEMapping{3, 2, 1, 0, 4, 5, 6, 7}; // FE --> FE CIC [2S]
    void                 SetStubWindowOffsets(uint8_t pBendCode, int pBend);

    // expected number of bx first stub
    // appears after resync
    // different for CBC and MPA
    uint8_t fStubBxDelay2S = 8;
    uint8_t fStubBxDelayPS = 22;
};

#endif
// #endif