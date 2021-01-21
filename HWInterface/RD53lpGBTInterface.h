/*!
  \file                  RD53lpGBTInterface.h
  \brief                 The implementation follows the skeleton of the register map section of the lpGBT Manual
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

    bool     ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    bool     WriteChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true) override;
    bool     WriteChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& RegVec, bool pVerifLoop = true) override;
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode) override;

    void InitialiseLinks(std::vector<uint8_t>& pULGroups, std::vector<uint8_t>& pULChannels, std::vector<uint8_t>& pDLGroups, std::vector<uint8_t>& pDLChannels) override;
    void SetTxRxPolarity(Ph2_HwDescription::Chip* pChip, uint8_t pTxPolarity, uint8_t pRxPolarity) override;
    bool IslpGBTready(Ph2_HwDescription::Chip* pChip) override;

    void ConfigureDownLinks(Ph2_HwDescription::Chip* pChip, uint8_t pCurrent, uint8_t pPreEmphasis, bool pInvert = false) override;
    void DisableDownLinks(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups) override;
    void ConfigureUpLinks(Ph2_HwDescription::Chip* pChip, uint8_t pDataRate, uint8_t pPhaseMode, uint8_t pEqual, uint8_t pPhase, bool pEnableTerm = true, bool pEnableBias = true, bool pInvert = false)
        override;
    void DisableUpLinks(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups) override;

  private:
    std::vector<uint8_t> fULGroups, fDLGroups;
    std::vector<uint8_t> fULChannels, fDLChannels;
};
} // namespace Ph2_HwInterface

#endif
