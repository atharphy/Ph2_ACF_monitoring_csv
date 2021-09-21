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


#include "tools/BackEndAlignment.h"
#include "tools/StubBackEndAlignment.h"
#include "tools/CicFEAlignment.h"

class LinkAlignmentOT : public Tool
{
  public:
    LinkAlignmentOT();
    ~LinkAlignmentOT();

    void Initialise();
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
    bool fSuccess{false};
    bool fWithCIC{false};
    bool fStubDebug{true};
    bool fL1Debug{true};

    bool AlignLpGBTInputs(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool WordAlignBEdata(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool AlignStubPackage(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    void Align();
};
#endif
