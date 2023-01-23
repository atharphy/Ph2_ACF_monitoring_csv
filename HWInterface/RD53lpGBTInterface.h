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

#include "HWInterface/lpGBTInterface.h"

namespace Ph2_HwInterface
{
class RD53lpGBTInterface : public lpGBTInterface
{
  public:
    RD53lpGBTInterface(const BeBoardFWMap& pBoardMap) : lpGBTInterface(pBoardMap) {}

    bool     WriteChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true);
    bool     WriteChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& RegVec, bool pVerifLoop = true);
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode);

    bool    ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    uint8_t PhaseAlignRx(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels) override { return 0; };
    void
    PhaseAlignRx(Ph2_HwDescription::Chip* pChip, const Ph2_HwDescription::BeBoard* pBoard, const Ph2_HwDescription::OpticalGroup* pOpticalGroup, ReadoutChipInterface* pReadoutChipInterface) override;

    // ###################################
    // # RD53 specific routine functions #
    // ###################################
    void SetDownLinkMapping(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    void SetUpLinkMapping(const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    bool ExternalPhaseAlignRx(Ph2_HwDescription::Chip*               pChip,
                              const Ph2_HwDescription::BeBoard*      pBoard,
                              const Ph2_HwDescription::OpticalGroup* pOpticalGroup,
                              Ph2_HwInterface::BeBoardFWInterface*   pBeBoardFWInterface,
                              ReadoutChipInterface*                  pReadoutChipInterface);

  private:
    bool     WriteReg(Ph2_HwDescription::Chip* pChip, uint16_t pAddress, uint16_t pValue, bool pVerifLoop = true);
    uint16_t ReadReg(Ph2_HwDescription::Chip* pChip, uint16_t pAddress);

    std::map<uint8_t, uint8_t> mapLpGBTGrCh2fwGr = {{00, 0}, {01, 1}, {10, 2}, {11, 3}, {20, 4}, {21, 5}, {30, 6}, {31, 7}};
};

} // namespace Ph2_HwInterface
#endif
