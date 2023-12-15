/*!
 *
 * \file ECVLinkAlignment.h
 * \brief Link alignment class, automated alignment procedure for CIC-lpGBT-BE
 * connected to FEs
 * \author Stefan Maier based on Sarah SEIF EL NASR-STOREY work in LinkAlignment
 * \date 21 / 11 / 23
 *
 * \Support : s.maier@kit.edu
 *
 */

#ifndef ECVLinkAlignmentOT_h__
#define ECVLinkAlignmentOT_h__

#include "tools/LinkAlignmentOT.h"

#ifdef __USE_ROOT__
#include "DQMUtils/DQMHistogramECV.h"
#endif

using namespace Ph2_HwDescription;

class ECVLinkAlignmentOT : public LinkAlignmentOT
{
  public:
    ECVLinkAlignmentOT();
    ~ECVLinkAlignmentOT();

    void Initialise();
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;

    void writeObjects();
    bool Scan();

  protected:
  private:
#ifdef __USE_ROOT__
    DQMHistogramECV fDQMHistogrammer;
#endif
    void                  SetHybridClockPolarityAndStrength(const Ph2_HwDescription::OpticalGroup* pOpticalGroup, bool pInverted, uint8_t pStrength);
    std::vector<uint8_t>  getGroupsAndChannels(const Ph2_HwDescription::OpticalGroup* pOpticalGroup, bool pGroups);
    void                  ECV(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    void                  ECV2(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);

    std::vector<float>    BitErrorTest(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    std::vector<float>    StubBitErrorTest(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    std::vector<float>    L1BitErrorTest(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    void                  SetlpGBTRxPhase(const Ph2_HwDescription::OpticalGroup* pOpticalGroup, uint8_t pPhase);
    std::vector<bool>     CheckWordAlignBEdata(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    void                  InitWordAlignStubs(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    std::vector<bool>     CheckWordAlignBEdataStubs(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    void                  StopWordAlignStubs(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    std::vector<bool>     CheckWordAlignBEdataL1(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    std::stringstream     PrintECVResultTable(const Ph2_HwDescription::OpticalGroup* pOpticalGroup, std::vector<std::vector<bool>> pWordAlignment, std::vector<std::vector<float>> pBitErrors);
    void                  StoreValuesInHistogram(uint8_t pClockPolarity, uint8_t pClockStrength, uint8_t pCicStrength, uint8_t pPhase, std::vector<float> pBers);
    void                  StoreWordAlignInHistogram(uint8_t pClockPolarity, uint8_t pClockStrength, uint8_t pCicStrength, uint8_t pPhase, uint8_t pLine, bool pAligned);


};
#endif
