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
    void AlignStubPackage();

  protected:
  private:
    // Alignment parameters
    DetectorDataContainer fBeSamplingDelay; // one per line per data line from hybrid
    DetectorDataContainer fBeBitSlip;       // one per line per data line from hybrid
    bool                  fStubDebug{false};
    bool                  fL1Debug{false};

    bool AlignLpGBTInputs(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool PhaseAlignBEdata(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool WordAlignBEdata(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool AlignStubPackage(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool AlignStubPackage(Ph2_HwDescription::BeBoard* pBoard);
    bool Align();
};
#endif
