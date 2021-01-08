/*!
 *
 * \file BackEndAlignment.h
 * \brief CIC FE alignment class, automated alignment procedure for CICs
 * connected to FEs
 * \author Sarah SEIF EL NASR-STOREY
 * \date 28 / 06 / 19
 *
 * \Support : sarah.storey@cern.ch
 *
 */

#ifndef BackEndAlignment_h__
#define BackEndAlignment_h__

#include "Tool.h"

#include <map>

namespace Ph2_HwInterface
{
class BackendAlignmentInterface;
}

// add breakcodes here
const uint8_t FAILED_BACKEND_ALIGNMENT = 5;

class BackEndAlignment : public Tool
{
  public:
    BackEndAlignment();
    ~BackEndAlignment();

    void Initialise();
    bool Align();

    void SetL1Debug(bool pDebug) { fL1Debug = pDebug; };
    void SetStubDebug(bool pDebug) { fStubDebug = pDebug; };

    bool CICAlignment(Ph2_HwDescription::BeBoard* pBoard);
    bool CBCAlignment(Ph2_HwDescription::BeBoard* pBoard);
    bool PSAlignment(Ph2_HwDescription::BeBoard* pBoard);
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    void writeObjects();
    void Reset();

    // get alignment results
    bool getStatus() const { return fSuccess; }

  protected:
    bool fL1Debug   = false;
    bool fStubDebug = false;

  private:
    // status
    bool fSuccess;
    // Containers
    DetectorDataContainer fRegMapContainer;
    DetectorDataContainer fBoardRegContainer;

// booking histograms
#ifdef __USE_ROOT__
//  DQMHistogramCic fDQMHistogram;
#endif
};
#endif
