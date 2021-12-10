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

#include "LinkAlignmentOT.h"

// add breakcodes here
const uint8_t FAILED_BACKEND_ALIGNMENT = 5;

class BackEndAlignment : public LinkAlignmentOT
{
  public:
    BackEndAlignment();
    ~BackEndAlignment();

    void Initialise();
    bool Align();

    void SetL1Debug(bool pDebug) { fL1Debug = pDebug; };
    void SetStubDebug(bool pDebug) { fStubDebug = pDebug; };

    bool Bx0Alignment(Ph2_HwDescription::BeBoard* pBoard);
    bool CICAlignment(Ph2_HwDescription::BeBoard* pBoard);
    bool CBCAlignment(Ph2_HwDescription::BeBoard* pBoard);
    bool PSAlignment(Ph2_HwDescription::BeBoard* pBoard, uint8_t pSSApair = 1);
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    void writeObjects();

    // get alignment results
    bool getStatus() const { return fSuccess; }

  protected:
    bool    fL1Debug   = false;
    bool    fStubDebug = false;
    uint8_t fPairSelect{0};

  private:
    //
    bool fAlignStub = true;
    // Containers
    DetectorDataContainer fEnabledFEs;
};
#endif
