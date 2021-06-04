/*!
  \file                  RD53lpGBTInterface.h
  \brief                 Interface to access and control the Low-power Gigabit Transceiver chip
  \author                Mauro DINARDO
  \version               1.0
  \date                  03/03/20
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53lpGBTInterface_H
#define RD53lpGBTInterface_H

#include "lpGBTInterface.h"

namespace Ph2_HwInterface
{
class RD53lpGBTInterface : public lpGBTInterface
{
  public:
    RD53lpGBTInterface(const BeBoardFWMap& pBoardMap) : lpGBTInterface(pBoardMap) {}

    bool ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    bool ExternalPhaseAlignRx(Ph2_HwDescription::Chip*               pChip,
                              const Ph2_HwDescription::BeBoard*      pBoard,
                              const Ph2_HwDescription::OpticalGroup* pOpticalGroup,
                              Ph2_HwInterface::BeBoardFWInterface*   pBeBoardFWInterface,
                              ReadoutChipInterface*                  pReadoutChipInterface) override;

  private:
};

} // namespace Ph2_HwInterface
#endif
