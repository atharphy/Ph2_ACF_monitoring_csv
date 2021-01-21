/*!

        \file                   MPA.h
        \brief                  MPA Description class, config of the MPAs
        \author                 Lorenzo BIDEGAIN
        \version                1.0
        \date                   25/06/14
        Support :               mail to : lorenzo.bidegain@gmail.com

 */

#ifndef MPA_h__
#define MPA_h__

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

/*!
 * \namespace Ph2_HwDescription
 * \brief Namespace regrouping all the hardware description
 */
namespace Ph2_HwDescription
{
using MPARegPair = std::pair<std::string, ChipRegItem>;
using CommentMap = std::map<int, std::string>;
class MPA : public ReadoutChip
{
  public:
    MPA(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pMPAId, const std::string& filename);
    // C'tors with object FE Description
    MPA(const FrontEndDescription& pFeDesc, uint8_t pMPAId, const std::string& filename);

    using MPARegPair = std::pair<std::string, ChipRegItem>;

    void loadfRegMap(const std::string& filename) override;
    void saveRegMap(const std::string& filename) override;

    bool isDACLocal(const std::string& dacName) override
    {
        if(dacName.find("TrimDAC_P", 0, 9) != std::string::npos)
            return true;
        else
            return false;
    }
    uint8_t getNumberOfBits(const std::string& dacName) override
    {
        if(dacName.find("TrimDAC_P", 0, 9) != std::string::npos)
            return 5;
        else
            return 8;
    }

    std::pair<uint32_t, uint32_t> PNlocal(const uint32_t PN) { return std::pair<uint32_t, uint32_t>(PN / 120, PN % 120); }

    uint32_t getNumberOfChannels() const override { return NMPACHANNELS; }

    uint32_t PNglobal(std::pair<uint32_t, uint32_t> PC) { return PC.first * 120 + PC.second; }
};

struct MPARegItemComparer
{
    bool operator()(const MPARegPair& pRegItem1, const MPARegPair& pRegItem2) const;
};
} // namespace Ph2_HwDescription

#endif
