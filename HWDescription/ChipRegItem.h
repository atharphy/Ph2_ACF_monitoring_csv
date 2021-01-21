/*!
        \file            ChipRegItem.h
        \brief                   ChipRegItem description, contents of the structure ChipRegItem with is the value of the
   ChipRegMap \author                  Lorenzo BIDEGAIN \version                 1.0 \date                    25/06/14
        Support :                mail to : lorenzo.bidegain@cern.ch
 */

#ifndef ChipRegItem_H
#define ChipRegItem_H

#include <stdint.h>

namespace Ph2_HwDescription
{
struct ChipRegItem
{
    ChipRegItem(){};
    ChipRegItem(uint8_t pPage, uint16_t pAddress, uint16_t pDefValue, uint16_t pValue) : fPage(pPage), fAddress(pAddress), fDefValue(pDefValue), fValue(pValue) {}

    uint8_t  fPage;
    uint16_t fAddress;
    uint16_t fDefValue;
    uint16_t fValue;
    bool     fPrmptCfg = false;
    uint8_t  fBitSize  = 0;
};
} // namespace Ph2_HwDescription

#endif
