#include "HWInterface/VTRxInterface.h"
#include "HWInterface/BeBoardFWInterface.h"
#include "HWInterface/D19cFWInterface.h"

#include <sstream>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
VTRxInterface::VTRxInterface(const BeBoardFWMap& pBoardMap) : ChipInterface(pBoardMap)
{
}


VTRxInterface::~VTRxInterface() {}

bool VTRxInterface::ConfigureChip(Chip* theVTRx, bool pVerify, uint32_t pBlockSize)
{
    std::stringstream cOutput;
    setBoard(theVTRx->getBeBoardId());
    theVTRx->printChipType(cOutput);
    LOG(INFO) << BOLDBLUE << cOutput.str() << "...Configuring chip with Id[" << +theVTRx->getId() << "]" << RESET;
    ChipRegMap cCicRegMap = theVTRx->getRegMap();
    // get register map
    std::vector<std::pair<std::string, uint16_t>> cRegItems;
    auto                                          theListOfFreeRegisters = theVTRx->getFreeRegisters();
    for(auto cItem: cCicRegMap)
    {
        bool isFreeRegister = false;
        for(const auto& freeRegister: theListOfFreeRegisters)
        {
            isFreeRegister = std::regex_match(cItem.first, freeRegister.first);
            if(isFreeRegister) break;
        }
        if(isFreeRegister) continue; // skipping readonly registers

        cRegItems.push_back(std::make_pair(cItem.first, cItem.second.fValue));
    }
    bool cSuccess = WriteChipMultReg(theVTRx, cRegItems, pVerify);
    if(cSuccess) LOG(INFO) << BOLDGREEN << "Succesful write to " << cRegItems.size() << " registers on CIC" << RESET;
    return cSuccess;
}


bool VTRxInterface::WriteChipReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerify)
{
    setBoard(pChip->getBeBoardId());
    // LOG(DEBUG) << BOLDMAGENTA << "VTRxInterface::WriteChipReg trying to write to register 0x" << pRegNode << RESET;
    ChipRegMap cRegMap       = pChip->getRegMap();
    cRegMap[pRegNode].fValue = pValue;
    return fBoardFW->SingleRegisterWrite(pChip, cRegMap[pRegNode], pVerify);
}

bool VTRxInterface::WriteChipMultReg(Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerify)
{
    // first, identify the correct BeBoardFWInterface
    setBoard(pChip->getBeBoardId());
    auto                     cRegMap = pChip->getRegMap();
    std::vector<ChipRegItem> cRegItems;
    for(auto cReq: pVecReq)
    {
        auto cIterator = cRegMap.find(cReq.first);
        if(cIterator == cRegMap.end())
        {
            LOG(ERROR) << BOLDRED << "VTRxInterface::WriteChipMultReg trying to write to a register that doesn't exist in the map : " << cReq.first << RESET;
            continue;
        }

        ChipRegItem cItem = cIterator->second;
        cItem.fValue      = cReq.second;
        cRegItems.push_back(cItem);
    }
    return fBoardFW->MultiRegisterWrite(pChip, cRegItems, pVerify);
}

int32_t VTRxInterface::ReadChipReg(Chip* pChip, const std::string& pRegNode)
{
    setBoard(pChip->getBeBoardId());
    // LOG(DEBUG) << BOLDMAGENTA << "VTRxInterface::ReadChipReg(string) Register " << pRegNode << RESET;

    ChipRegMap cRegMap = pChip->getRegMap();
    if(cRegMap.find(pRegNode) == cRegMap.end()) { LOG(INFO) << BOLDRED << "Could not find CIC register " << pRegNode << RESET; }

    ChipRegItem cRegItem = pChip->getRegItem(pRegNode);
    return fBoardFW->SingleRegisterRead(pChip, cRegItem);
}

std::vector<std::pair<std::string, uint16_t>> VTRxInterface::ReadChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::string>& theRegisterList)
{
    setBoard(pChip->getBeBoardId());
    auto                     cRegMap = pChip->getRegMap();
    std::vector<ChipRegItem> cRegItems;
    for(auto cReq: theRegisterList)
    {
        auto cIterator = cRegMap.find(cReq);
        if(cIterator == cRegMap.end())
        {
            LOG(ERROR) << BOLDRED << "VTRxInterface::WriteChipMultReg trying to write to a register that doesn't exist in the map : " << cReq << RESET;
            abort();
        }

        ChipRegItem cItem = cIterator->second;
        cRegItems.push_back(cItem);
    }

    fBoardFW->MultiRegisterRead(pChip, cRegItems);

    std::vector<std::pair<std::string, uint16_t>> theRegisterValues;
    for(size_t i = 0; i < theRegisterList.size(); ++i) theRegisterValues.push_back(std::make_pair(theRegisterList[i], cRegItems[i].fValue));
    return theRegisterValues;
}


}