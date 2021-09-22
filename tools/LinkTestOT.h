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

#ifndef LinkTestOT_h__
#define LinkTestOT_h__

#include "LinkAlignmentOT.h"

using namespace Ph2_HwDescription;

class LinkTestOT : public LinkAlignmentOT
{
  public:
    LinkTestOT();
    ~LinkTestOT();

    //void Initialise();
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    //void Reset();

    void TestL1ALines(bool pAlign=false);
  protected:
    
  private:
    bool TestL1ALine(Ph2_HwDescription::BeBoard* pBoard, bool pAlign=false, size_t pAttempts = 20 ); 

};
#endif
