/*!

  \file                   Cbc.h
  \brief                  Cbc Description class, config of the Cbcs
  \author                 Lorenzo BIDEGAIN
  \version                1.0
  \date                   25/06/14
  Support :               mail to : lorenzo.bidegain@gmail.com

*/

#ifndef Cbc_h__
#define Cbc_h__

#include "../Utils/Exception.h"
#include "../Utils/Visitor.h"
#include "FrontEndDescription.h"
#include "ReadoutChip.h"
#include <iostream>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>

/*!
 * \namespace Ph2_HwDescription
 * \brief Namespace regrouping all the hardware description
 */
namespace Ph2_HwDescription
{
using CbcRegPair = std::pair<std::string, ChipRegItem>;

/*!
 * \class Cbc
 * \brief Read/Write Cbc's registers on a file, contains a register map
 */
class Cbc : public ReadoutChip
{
  public:
    // C'tors which take BeId, FMCId, FeID, CbcId
    Cbc(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pCbcId, const std::string& filename);

    // C'tors with object FE Description
    Cbc(const FrontEndDescription& pFeDesc, uint8_t pCbcId, const std::string& filename);

    /*!
     * \brief acceptor method for HwDescriptionVisitor
     * \param pVisitor
     */
    virtual void accept(HwDescriptionVisitor& pVisitor) { pVisitor.visitChip(*this); }

    /*!
     * \brief Load RegMap from a file
     * \param filename
     */
    void loadfRegMap(const std::string& filename) override;

    // uint16_t getReg ( const std::string& pReg ) const override;
    // void setReg ( const std::string& pReg, uint16_t psetValue, bool pPrmptCfg = false) override;

    /*!
     * \brief Write the registers of the Map in a file
     * \param filename
     */
    void saveRegMap(const std::string& filename) override;

    uint32_t getNumberOfChannels() const override { return NCHANNELS; }

    bool isDACLocal(const std::string& dacName) override
    {
        if(dacName.find("MaskChannel-", 0, 12) != std::string::npos || dacName.find("Channel", 0, 7) != std::string::npos)
            return true;
        else
            return false;
    }

    uint8_t getNumberOfBits(const std::string& dacName) override
    {
        if(dacName.find("MaskChannel-", 0, 12) != std::string::npos)
            return 1;
        else if(dacName == "VCth")
            return 10;
        else if(dacName == "VCth2")
            return 2;
        else if(dacName == "TriggerLatency")
            return 9;
        else
            return 8;
    }
};
} // namespace Ph2_HwDescription

#endif
