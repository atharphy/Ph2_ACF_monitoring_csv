/*!
  \file                   Chip.h
  \brief                  Chip Description class, config of the Chips
  \author                 Lorenzo BIDEGAIN
  \version                1.0
  \date                   25/06/14
  Support :               mail to : lorenzo.bidegain@gmail.com
*/

#ifndef ReadoutChip_H
#define ReadoutChip_H

#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/Container.h"
#include "../Utils/Exception.h"
#include "../Utils/Visitor.h"
#include "../Utils/easylogging++.h"
#include "Chip.h"
#include "Definition.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdint.h>
#include <string>
#include <utility>

/*!
 * \namespace Ph2_HwDescription
 * \brief Namespace regrouping all the hardware description
 */
namespace Ph2_HwDescription
{
/*!
 * \class ReadoutChip
 * \brief Read/Write Chip's registers on a file, contains a register map
 */
class ReadoutChip
    : public Chip
    , public ChipContainer
{
  public:
    // C'tors which take Board ID, Frontend ID/Module ID, FMC ID, Chip ID
    ReadoutChip(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pChipId, uint16_t pMaxRegValue = 255);

    // C'tors with object FE Description
    ReadoutChip(const FrontEndDescription& pFeDesc, uint8_t pChipId, uint16_t pMaxRegValue = 255);

    // Copy C'tor
    ReadoutChip(const ReadoutChip& chipObj);

    ~ReadoutChip();

    /*!
     * \brief acceptor method for HwDescriptionVisitor
     * \param pVisitor
     */
    virtual void accept(HwDescriptionVisitor& pVisitor) { pVisitor.visitChip(*this); }

    virtual uint32_t getNumberOfChannels() const = 0;

    virtual bool isDACLocal(const std::string& dacName) = 0;

    const ChannelGroupBase* getChipOriginalMask() const override { return fChipOriginalMask; }

  protected:
    ChannelGroupBase* fChipOriginalMask;
};
} // namespace Ph2_HwDescription

#endif
