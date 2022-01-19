/*!
 *
 * \file LinkAlignment.h
 * \brief Link alignment class, automated alignment procedure for CIC-lpGBT-BE
 * connected to FEs
 * \author Sarah SEIF EL NASR-STOREY
 * \date 28 / 06 / 19
 *
 * \Support : sarah.storey@cern.ch
 *
 */

#ifndef LinkAlignmentOT_h__
#define LinkAlignmentOT_h__

#include "OTTool.h"

using namespace Ph2_HwDescription;

class LinkAlignmentOT : public OTTool
{
  public:
    LinkAlignmentOT();
    ~LinkAlignmentOT();

    void Initialise();
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;

    // get alignment results
    bool getStatus() const { return fSuccess; }

    bool    WordAlignBEdata(const Ph2_HwDescription::BeBoard* pBoard);
    bool    PhaseAlignBEdata(const Ph2_HwDescription::BeBoard* pBoard);
    uint8_t getBeSamplingDelay(uint8_t pBoardIndx, uint8_t pOGIndx, uint8_t pHybridIndx, uint8_t pLineIndx)
    {
        auto& cBeSamplingDelay      = fBeSamplingDelay.at(pBoardIndx);
        auto& cBeSamplingDelayOG    = cBeSamplingDelay->at(pOGIndx);
        auto& cBeSamplingDelayHybrd = cBeSamplingDelayOG->at(pHybridIndx);
        return cBeSamplingDelayHybrd->getSummary<std::vector<uint8_t>>()[pLineIndx];
    }
    uint8_t getBeBitSlip(uint8_t pBoardIndx, uint8_t pOGIndx, uint8_t pHybridIndx, uint8_t pLineIndx)
    {
        auto& cBeBitSlip      = fBeBitSlip.at(pBoardIndx);
        auto& cBeBitSlipOG    = cBeBitSlip->at(pOGIndx);
        auto& cBeBitSlipHybrd = cBeBitSlipOG->at(pHybridIndx);
        return cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>()[pLineIndx];
    }
    void    AlignStubPackage();
    uint8_t GetLpGBTDelay(const OpticalGroup* pOpticalGroup) const
    {
        auto cBoardId   = pOpticalGroup->getBeBoardId();
        auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
        return fLpGBTSamplingDelay.at((*cBoardIter)->getIndex())->at(pOpticalGroup->getIndex())->at(0)->getSummary<uint8_t>();
    }
    std::pair<bool, uint8_t> PhaseTuneLine(const Ph2_HwDescription::Chip* pChip, uint8_t pLineId);                                             // generic
    std::pair<bool, uint8_t> WordAlignLine(const Ph2_HwDescription::Chip* pChip, uint8_t pLineId, uint8_t pAlignmentPattern, uint8_t pPeriod); // generic
    bool                     L1WordAlignment(const Ph2_HwDescription::OpticalGroup* pOpticalGroup, bool pScope);
    void                     ManuallyConfigureLine(const Ph2_HwDescription::Chip* pChip, uint8_t pLineId, uint8_t pPhase, uint8_t pBitslip); // generic
    bool                     LineTuning(const Ph2_HwDescription::Chip* pChip, uint8_t pLineId, uint8_t pAlignmentPattern, uint8_t pPeriod);  // electrical
    void                     LegacyAlignmentMPA(const Ph2_HwDescription::Chip* pChip);
    void                     CheckLpgbtOutputs(uint8_t pPattern = 0xAA);

  protected:
  private:
    // Alignment parameters
    DetectorDataContainer fLpGBTSamplingDelay;
    DetectorDataContainer fBeSamplingDelay; // one per line per data line from hybrid
    DetectorDataContainer fBeBitSlip;       // one per line per data line from hybrid
    bool                  fStubDebug{false};
    bool                  fL1Debug{false};

    bool CheckLpgbtOutputs(const Ph2_HwDescription::OpticalGroup* pOpticalGroup, uint8_t pPattern = 0xAA);
    bool AlignLpGBTInputs(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool PhaseAlignBEdata(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool WordAlignBEdata(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool AlignStubPackage(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool AlignStubPackage(Ph2_HwDescription::BeBoard* pBoard);
    bool Align();
};
#endif
