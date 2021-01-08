/*!

        \file                   SSA.h
        \brief                  SSA Description class, config of the SSAs
        \author                 Marc Osherson (copying from Cbc.h)
        \version                1.0
        \date                   31/07/19
        Support :               mail to : oshersonmarc@gmail.com

 */

#ifndef SSA_h__
#define SSA_h__

#include "../Utils/Exception.h"
#include "../Utils/Visitor.h"
#include "../Utils/easylogging++.h"
#include "ChipRegItem.h"
#include "FrontEndDescription.h"
#include "ReadoutChip.h"
#include <iostream>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>

namespace Ph2_HwDescription
{ // open namespace

using SSARegPair = std::pair<std::string, ChipRegItem>;
using CommentMap = std::map<int, std::string>;

class SSA : public ReadoutChip
{ // open class def
  public:
    // C'tors which take BeId, FMCId, FeID, SSAId
    SSA(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pSSAId, uint8_t pPartnerId, uint8_t pSSASide, const std::string& filename);
    // C'tors with object FE Description
    SSA(const FrontEndDescription& pFeDesc, uint8_t pSSAId, uint8_t pPartnerId, uint8_t pSSASide, const std::string& filename);
    uint8_t fPartnerId;
    uint8_t getPartid() {return fPartnerId;}
    virtual void accept(HwDescriptionVisitor& pVisitor) { pVisitor.visitChip(*this); }
    void         loadfRegMap(const std::string& filename) override;
    void         saveRegMap(const std::string& filename) override;
    uint32_t     getNumberOfChannels() const override { return NSSACHANNELS; }
    bool         isDACLocal(const std::string& dacName) override
    {
        if(dacName.find("THTRIMMING_S", 0, 12) != std::string::npos)
            return true;
        else if(dacName.find("ThresholdTrim") != std::string::npos)
            return true;
        else
            return false;
    }
    uint8_t getNumberOfBits(const std::string& dacName) override
    {
        if(dacName.find("THTRIMMING_S", 0, 12) != std::string::npos)
            return 5;
        else if(dacName.find("ThresholdTrim") != std::string::npos)
            return 5;
        else
            return 8;
    }

  protected:
}; // close class def

} // namespace Ph2_HwDescription

#endif
