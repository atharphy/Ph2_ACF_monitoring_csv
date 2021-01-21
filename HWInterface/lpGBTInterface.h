/*!
  \file                  lpGBTInterface.h
  \brief                 The implementation follows the skeleton of the register map section of the lpGBT Manual
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

    virtual void InitialiseLinks(std::vector<uint8_t>& pULGroups, std::vector<uint8_t>& pULChannels, std::vector<uint8_t>& pDLGroups, std::vector<uint8_t>& pDLChannels) = 0;
    virtual void SetTxRxPolarity(Ph2_HwDescription::Chip* pChip, uint8_t pTxPolarity, uint8_t pRxPolarity)                                                               = 0;
    virtual bool IslpGBTready(Ph2_HwDescription::Chip* pChip)                                                                                                            = 0;
    virtual void ConfigureDownLinks(Ph2_HwDescription::Chip* pChip, uint8_t pCurrent, uint8_t pPreEmphasis, bool pInvert = false)                                        = 0;
    virtual void DisableDownLinks(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups)                                                                   = 0;
    virtual void
                 ConfigureUpLinks(Ph2_HwDescription::Chip* pChip, uint8_t pDataRate, uint8_t pPhaseMode, uint8_t pEqual, uint8_t pPhase, bool pEnableTerm = true, bool pEnableBias = true, bool pInvert = false) = 0;
    virtual void DisableUpLinks(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups) = 0;
};
} // namespace Ph2_HwInterface

#endif
