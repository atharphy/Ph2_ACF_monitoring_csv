/*!
  Filename :                      Chip.cc
  Content :                       Chip Description class, config of the Chips
  Programmer :                    Lorenzo BIDEGAIN
  Version :                       1.0
  Date of Creation :              25/06/14
  Support :                       mail to : lorenzo.bidegain@gmail.com
*/

#include "ReadoutChip.h"

namespace Ph2_HwDescription
{
// C'tors with object FE Description
ReadoutChip::ReadoutChip(const FrontEndDescription& pFeDesc, uint8_t pChipId, uint16_t pMaxRegValue) : Chip(pFeDesc, pChipId, pMaxRegValue), ChipContainer(pChipId), fChipOriginalMask(nullptr) {}

// C'tors which take Board ID, Frontend ID/Module ID, FMC ID, Chip ID
ReadoutChip::ReadoutChip(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pChipId, uint16_t pMaxRegValue)
    : Chip(pBeId, pFMCId, pFeId, pChipId, pMaxRegValue), ChipContainer(pChipId), fChipOriginalMask(nullptr)
{
}

// Copy C'tor
ReadoutChip::ReadoutChip(const ReadoutChip& chipObj) : Chip(chipObj), ChipContainer(chipObj.fChipId), fChipOriginalMask(nullptr) {}

ReadoutChip::~ReadoutChip()
{
    delete fChipOriginalMask;
    fChipOriginalMask = nullptr;
}
} // namespace Ph2_HwDescription
