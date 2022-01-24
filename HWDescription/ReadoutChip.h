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
#include <memory>
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
    // C'tors which take Board ID, Frontend ID/Hybrid ID, FMC ID, Chip ID
    ReadoutChip(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pChipId, uint16_t pMaxRegValue = 255);

    // C'tors with object FE Description
    ReadoutChip(const FrontEndDescription& pFeDesc, uint8_t pChipId, uint16_t pMaxRegValue = 255);

    // Copy C'tor
    ReadoutChip(const ReadoutChip& chipObj);

    ~ReadoutChip();

    uint16_t getId() const override { return ChipContainer::getId(); }

    /*!
     * \brief acceptor method for HwDescriptionVisitor
     * \param pVisitor
     */
    virtual void accept(HwDescriptionVisitor& pVisitor) { pVisitor.visitChip(*this); }

    virtual uint32_t getNumberOfChannels() const = 0;

    virtual bool isDACLocal(const std::string& dacName) = 0;

    const std::shared_ptr<ChannelGroupBase> getChipOriginalMask() const override { return fChipOriginalMask; }
    // channels are encoded as columnNumber*Nrows + rowNumber
    void setChipOriginalMask(std::vector<uint16_t> pChannels)
    {
        fChipOriginalMask->enableAllChannels();
        for(auto cChnl: pChannels)
        {
            uint16_t cRow = cChnl;
            uint16_t cCol = 0;
            if(fChipOriginalMask->getNumberOfCols() > 0)
            {
                cRow = cChnl % fChipOriginalMask->getNumberOfRows();
                cCol = cChnl / fChipOriginalMask->getNumberOfRows();
                std::cout << cRow << " , " << cCol << "\n";
            }
            fChipOriginalMask->disableChannel(cRow, cCol);
        }
        std::cout << "Channel mask has " << fChipOriginalMask->getNumberOfEnabledChannels() << " enabled channels\n";
    }

  protected:
    std::shared_ptr<ChannelGroupBase> fChipOriginalMask{nullptr};
};
} // namespace Ph2_HwDescription

#endif
