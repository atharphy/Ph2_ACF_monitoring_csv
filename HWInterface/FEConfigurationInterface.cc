#include "FEConfigurationInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
    
FEConfigurationInterface::FEConfigurationInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : RegManager(pId, pUri, pAddressTable) {
    fModifiedRegisters.clear();
    fMap.clear();
}
FEConfigurationInterface::FEConfigurationInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : RegManager(puHalConfigFileName, pBoardId) {
    
    LOG (INFO) << BOLDYELLOW << "FEConfigurationInterface::FEConfigurationInterface Constructor" << RESET;
    // fModifiedRegisters.clear();
    // fMap.clear();
}
FEConfigurationInterface::~FEConfigurationInterface() {}
Ph2_HwDescription::ChipRegMap FEConfigurationInterface::GetModifiedRegisterMap(Ph2_HwDescription::Chip* pChip)
{
    Ph2_HwDescription::ChipRegMap cMap;
    uint32_t                      cChipId = (uint8_t)(pChip->getFrontEndType() == FrontEndType::MPA || pChip->getFrontEndType() == FrontEndType::RD53) << 12;
    cChipId                               = cChipId | pChip->getOpticalId() << 8 | pChip->getHybridId() << 4 | pChip->getId();
    auto cIter                            = fModifiedRegisters.find(cChipId);
    if(cIter != fModifiedRegisters.end())
    {
        // std::cout << "GetModifiedRegisterMap interface --- " << +cChipId << " contains " << cIter->second.size() << " items.\n";
        return cIter->second;
    }
    else
    {
        // std::cout << "GetModifiedRegisterMap interface --- " << +cChipId << " contains " << 0 << " items.\n";
        return cMap;
    }
}
void FEConfigurationInterface::OverwriteModifiedRegisterMap(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegMap pRegMap)
{
    uint32_t cChipId = (uint8_t)(pChip->getFrontEndType() == FrontEndType::MPA || pChip->getFrontEndType() == FrontEndType::RD53) << 12;
    cChipId          = cChipId | pChip->getOpticalId() << 8 | pChip->getHybridId() << 4 | pChip->getId();
    auto cIter       = fModifiedRegisters.find(cChipId);
    if(cIter != fModifiedRegisters.end()) fModifiedRegisters.erase(cChipId);
    // std::cout << "Overwriting map with an item that has " << pRegMap.size() << " entries.\n";
    fModifiedRegisters[cChipId] = pRegMap;
    // std::cout << "OverwriteModifiedRegisterMap interface --- " << +cChipId << " contains " << fModifiedRegisters[cChipId].size() << " items.\n";
}
void FEConfigurationInterface::UpdateModifiedRegMap(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pPage )
{
    // modified registers map
    uint32_t cChipId = (uint8_t)(pChip->getFrontEndType() == FrontEndType::MPA || pChip->getFrontEndType() == FrontEndType::RD53) << 12;
    cChipId          = cChipId | pChip->getOpticalId() << 8 | pChip->getHybridId() << 4 | pChip->getId();
    auto cMapIter    = fModifiedRegisters.find(cChipId);
    if(cMapIter == fModifiedRegisters.end())
    {
        Ph2_HwDescription::ChipRegMap cRegMap;
        fModifiedRegisters[cChipId] = cRegMap;
    }
    cMapIter      = fModifiedRegisters.find(cChipId);
    auto& cModMap = cMapIter->second;
    bool  cFound  = false;
    for(auto cMapItem: pChip->getRegMap())
    {
        if(cFound) break;
        if(cMapItem.second.fAddress == pRegisterAddress && cMapItem.second.fPage == pPage)
        {
            cFound = true;
            if(cModMap.find(cMapItem.first) == cModMap.end())
            {
                auto cSize              = cModMap.size();
                cModMap[cMapItem.first] = cMapItem.second;
                LOG(DEBUG) << BOLDMAGENTA << "ReadoutChipInterface - ModMap contained " << cSize << " items....now has " << cModMap.size() << " items that " << cMapItem.first
                            << " register will be  modified "
                            << " original value is " << +cMapItem.second.fValue << RESET;
            }
        }
    }
}

void FEConfigurationInterface::ClearModifiedRegisterMap()
{
    fModifiedRegisters.clear();
    LOG(DEBUG) << BOLDMAGENTA << "After clearing register map have " << +fModifiedRegisters.size() << " regs." << RESET;
}
    
}