/*!
  Filename :                      Chip.cc
  Content :                       Chip Description class, config of the Chips
  Programmer :                    Lorenzo BIDEGAIN
  Version :                       1.0
  Date of Creation :              25/06/14
  Support :                       mail to : lorenzo.bidegain@gmail.com
*/

#include "Chip.h"
#include "../Utils/ChannelGroupHandler.h"
#include "Definition.h"
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string.h>

namespace Ph2_HwDescription
{
// C'tors with object FE Description
Chip::Chip(const FrontEndDescription& pFeDesc, uint8_t pChipId, uint16_t pMaxRegValue) : FrontEndDescription(pFeDesc), fChipId(pChipId), fMaxRegValue(pMaxRegValue) {}

// C'tors which take Board ID, Frontend ID/Hybrid ID, FMC ID, Chip ID
Chip::Chip(uint8_t pBeBoardId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pChipId, uint16_t pMaxRegValue)
    : FrontEndDescription(pBeBoardId, pFMCId, pOpticalGroupId, pHybridId), fChipId(pChipId), fMaxRegValue(pMaxRegValue)
{
}

// Copy C'tor
Chip::Chip(const Chip& chipObj) : FrontEndDescription(chipObj), fChipId(chipObj.fChipId), fRegMap(chipObj.fRegMap), fModifiedRegs(chipObj.fModifiedRegs), fCommentMap(chipObj.fCommentMap) {}

// D'Tor
Chip::~Chip()
{
    fRegMap.clear();
    fCommentMap.clear();
    fModifiedRegs.clear();
}

ChipRegItem Chip::getRegItem(const std::string& pReg)
{
    ChipRegItem          cItem;
    ChipRegMap::iterator i = fRegMap.find(pReg);

    if(i != std::end(fRegMap)) return (i->second);

    LOG(ERROR) << BOLDRED << "Error, no register " << BOLDYELLOW << pReg << BOLDRED << " found in the RegisterMap of ChipID: " << BOLDYELLOW << +fChipId << RESET;
    throw Exception("Chip: no matching register found");
    return cItem;
}

uint16_t Chip::getReg(const std::string& pReg) const
{
    ChipRegMap::const_iterator i = fRegMap.find(pReg);

    if(i == fRegMap.end())
    {
        LOG(INFO) << "The Chip object: " << +fChipId << " doesn't have " << pReg;
        return 0;
    }
    else
        return i->second.fValue & fMaxRegValue;
}

void Chip::setReg(const std::string& pReg, uint16_t psetValue, bool pPrmptCfg, uint8_t pStatusReg)
{
    ChipRegMap::iterator i = fRegMap.find(pReg);

    if(i == fRegMap.end()) LOG(INFO) << "The Chip object: " << +fChipId << " doesn't have " << pReg;
    if(psetValue > fMaxRegValue)
        LOG(ERROR) << "Chip register are at most " << fMaxRegValue << " bits, impossible to write " << psetValue << " on registed " << pReg;
    else
    {
        i->second.fValue     = psetValue & fMaxRegValue;
        i->second.fStatusReg = pStatusReg;
        i->second.fPrmptCfg  = pPrmptCfg;
    }
}

void Chip::UpdateModifiedRegMap(ChipRegItem pItem)
{
    if(fTrackRegisters == 0) return;

    std::stringstream cOutput;
    this->printChipType(cOutput);
    auto cIterator = find_if(fRegMap.begin(), fRegMap.end(), [&pItem](const ChipRegPair& obj) { return obj.second.fAddress == pItem.fAddress && obj.second.fPage == pItem.fPage; });
    if(cIterator != fRegMap.end()) // is register in the original map
    {
        auto cName    = cIterator->first;
        auto cRegItem = cIterator->second;
        // only add the first time
        cIterator = fModifiedRegs.find(cName);
        if(cIterator == fModifiedRegs.end())
        {
            // auto cSize              = fModifiedRegs.size();
            fModifiedRegs[cName] = cRegItem;
            // LOG (INFO) << BOLDYELLOW << "ModMap for " << cOutput.str() << " contained " << cSize << " items.... will add " << cName << "\t Original Value " << fModifiedRegs[cName].fValue << RESET;
        }
    }
}
void Chip::UpdateModifiedRegMap(const std::string& pRegName)
{
    if(fTrackRegisters == 0) return;

    std::stringstream cOutput;
    this->printChipType(cOutput);

    auto cIterator = find_if(fRegMap.begin(), fRegMap.end(), [&pRegName](const ChipRegPair& obj) { return obj.first == pRegName; });
    if(cIterator != fRegMap.end())
    {
        auto cName    = cIterator->first;
        auto cRegItem = cIterator->second;
        // only add the first time
        cIterator = fModifiedRegs.find(cName);
        if(cIterator == fModifiedRegs.end())
        {
            // auto cSize              = fModifiedRegs.size();
            fModifiedRegs[cName] = cRegItem;
            // LOG (INFO) << BOLDYELLOW << "ModMap for " << cOutput.str() << " contained " << cSize << " items.... will add " << cName << "\t Original Value " << fModifiedRegs[cName].fValue << RESET;
        }
    }
}
void Chip::UpdateModifiedRegMap(uint16_t pRegisterAddress, uint8_t pPage)
{
    if(fTrackRegisters == 0) return;

    std::stringstream cOutput;
    this->printChipType(cOutput);

    auto cIterator = find_if(fRegMap.begin(), fRegMap.end(), [&pRegisterAddress, &pPage](const ChipRegPair& obj) { return obj.second.fAddress == pRegisterAddress && obj.second.fPage == pPage; });
    if(cIterator != fRegMap.end())
    {
        auto cName    = cIterator->first;
        auto cRegItem = cIterator->second;
        // only add the first time
        cIterator = fModifiedRegs.find(cName);
        if(cIterator == fModifiedRegs.end())
        {
            // auto cSize              = fModifiedRegs.size();
            fModifiedRegs[cName] = cRegItem;
            // LOG (INFO) << BOLDYELLOW << "ModMap for " << cOutput.str() << " contained " << cSize << " items.... will add " << cName << "\t Original Value " << fModifiedRegs[cName].fValue << RESET;
        }
    }
}

bool ChipComparer::operator()(const Chip& chip1, const Chip& chip2) const
{
    if(chip1.getBeBoardId() != chip2.getBeBoardId())
        return chip1.getBeBoardId() < chip2.getBeBoardId();
    else if(chip1.getFMCId() != chip2.getFMCId())
        return chip1.getFMCId() < chip2.getFMCId();
    else if(chip1.getHybridId() != chip2.getHybridId())
        return chip1.getHybridId() < chip2.getHybridId();
    else
        return chip1.getId() < chip2.getId();
}

bool RegItemComparer::operator()(const ChipRegPair& pRegItem1, const ChipRegPair& pRegItem2) const
{
    if(pRegItem1.second.fPage != pRegItem2.second.fPage)
        return pRegItem1.second.fPage < pRegItem2.second.fPage;
    else
        return pRegItem1.second.fAddress < pRegItem2.second.fAddress;
}

} // namespace Ph2_HwDescription
