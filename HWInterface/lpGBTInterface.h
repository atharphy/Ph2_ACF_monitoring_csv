/*!
  \file                  lpGBTInterface.h
  \brief                 Interface to access and control the low-power Gigabit Transceiver chip
  \author                Younes Otarid
  \version               1.0
  \date                  03/03/20
  Support:               email to younes.otarid@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef lpGBTInterface_H
#define lpGBTInterface_H

#include "../HWDescription/lpGBT.h"
#include "ChipInterface.h"

namespace Ph2_HwInterface
{
class lpGBTInterface : public ChipInterface
{
  public:
    lpGBTInterface(const BeBoardFWMap& pBoardMap) : ChipInterface(pBoardMap) {}
    virtual ~lpGBTInterface() {}

    virtual bool RunBERtest(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel, bool given_time, double frames_or_time, uint8_t frontendSpeed = 0) = 0;
    virtual void StartPRBSpattern(Ph2_HwDescription::Chip* pChip)                                                                                                = 0;
    virtual void StopPRBSpattern(Ph2_HwDescription::Chip* pChip)                                                                                                 = 0;
};
} // namespace Ph2_HwInterface

#endif
