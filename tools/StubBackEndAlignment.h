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

#ifndef StubBackEndAlignment_h__
#define StubBackEndAlignment_h__

#include "Tool.h"

#include <map>
const uint8_t FAILED_STUBBACKEND_ALIGNMENT = 100;
class StubBackEndAlignment : public Tool
{
  public:
    StubBackEndAlignment();
    ~StubBackEndAlignment();

    void Initialise();
    bool Align();

    bool FindStubLatency(Ph2_HwDescription::BeBoard* pBoard);
    bool FindPackageDelay(Ph2_HwDescription::BeBoard* pBoard);
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    void Reset();

    // get alignment results
    bool getStatus() const { return fSuccess; }

  protected:
    
  private:
    // Containers
    DetectorDataContainer fBoardRegContainer;
    bool fSuccess;
    bool fWithCIC;
};
#endif
