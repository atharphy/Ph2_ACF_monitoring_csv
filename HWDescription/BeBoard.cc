/*!

        Filename :                              BeBoard.cc
        Content :                               BeBoard Description class, configs of the BeBoard
        Programmer :                    Lorenzo BIDEGAIN
        Version :               1.0
        Date of Creation :              14/07/14
        Support :                               mail to : lorenzo.bidegain@gmail.com

 */

#include "BeBoard.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Ph2_HwDescription
{
// Constructors

BeBoard::BeBoard() : BoardContainer(0), fEventType(EventType::VR), fCondDataSet(nullptr) {}

BeBoard::BeBoard(uint8_t pBeId) : BoardContainer(pBeId), fEventType(EventType::VR), fCondDataSet(nullptr) {}

BeBoard::BeBoard(uint8_t pBeId, const std::string& filename) : BoardContainer(pBeId), fEventType(EventType::VR), fCondDataSet(nullptr) { loadConfigFile(filename); }

// Public Members:

uint32_t BeBoard::getReg(const std::string& pReg) const
{
    BeBoardRegMap::const_iterator i = fRegMap.find(pReg);

    if(i == fRegMap.end())
    {
        LOG(INFO) << "The Board object: " << +getId() << " doesn't have " << pReg;
        return 0;
    }
    else
        return i->second;
}

void BeBoard::setReg(const std::string& pReg, uint32_t psetValue)
{
    auto oldRegister = fRegMap[pReg];
    fRegMap[pReg]    = psetValue;
    if(fTrackModifiedRegistersEnabled)
    {
        bool isFreeRegister = false;
        for(const auto& freeRegister: fListOfFreeRegisters)
        {
            isFreeRegister = std::regex_match(pReg, freeRegister);
            if(isFreeRegister) break;
        }
        if(!isFreeRegister && oldRegister != psetValue) fModifiedRegisters[pReg] = oldRegister;
    }
}

void BeBoard::updateCondData(uint32_t& pTDCVal)
{
    if(fCondDataSet == nullptr)
        return;
    else if(fCondDataSet->fCondDataVector.size() == 0)
        return;
    else if(!fCondDataSet->testEffort())
        return;
    else
    {
        for(auto& cCondItem: this->fCondDataSet->fCondDataVector)
        {
            // if it is the TDC item, save it in fValue
            if(cCondItem.fUID == 3)
                cCondItem.fValue = pTDCVal;
            else if(cCondItem.fUID == 1)
            {
                for(auto cOpticalGroup: *this)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        if(cCondItem.fHybridId != cHybrid->getId()) continue;

                        for(auto cCbc: *cHybrid)
                        {
                            if(cCondItem.fCbcId != cCbc->getId())
                                continue;
                            else if(cHybrid->getId() == cCondItem.fHybridId && cCbc->getId() == cCondItem.fCbcId)
                            {
                                ChipRegItem cRegItem = static_cast<ReadoutChip*>(cCbc)->getRegItem(cCondItem.fRegName);
                                cCondItem.fValue     = cRegItem.fValue;
                            }
                        }
                    }
                }
            }
        }
    }
}

// Private Members:

void BeBoard::loadConfigFile(const std::string& filename)

{
    std::ifstream cFile(filename.c_str(), std::ios::in);

    if(!cFile)
        LOG(ERROR) << "The BeBoard Settings File " << filename << " could not be opened!";
    else
    {
        fRegMap.clear();
        std::string cLine, cName, cValue, cFound;

        while(!(getline(cFile, cLine).eof()))
        {
            if(cLine.find_first_not_of(" \t") == std::string::npos) continue;

            if(cLine.at(0) == '#' || cLine.at(0) == '*') continue;

            if(cLine.find(":") == std::string::npos) continue;

            std::istringstream input(cLine);
            input >> cName >> cFound >> cValue;

            // Here the Reg name sits in cName and the Reg value sits in cValue
            if(cValue.find("0x") != std::string::npos)
                fRegMap[cName] = strtol(cValue.c_str(), 0, 16);
            else
                fRegMap[cName] = strtol(cValue.c_str(), 0, 10);
        }

        cFile.close();
    }
}

std::vector<FrontEndType> BeBoard::connectedFrontEndTypes() const
{
    std::vector<FrontEndType> cFrontEndTypes;
    for(auto cOpticalGroup: *this)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                auto cIter = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), cChip->getFrontEndType());
                if(cIter == cFrontEndTypes.end()) cFrontEndTypes.push_back(cChip->getFrontEndType());
            } // chips
        }     // hybrids
    }         // opticalGroup
    return cFrontEndTypes;
}

void BeBoard::takeSnapshot()
{
    clearSnapshot();
    fTrackModifiedRegistersEnabled = true;
}

void BeBoard::clearSnapshot()
{
    fTrackModifiedRegistersEnabled = false;
    fModifiedRegisters.clear();
}

std::vector<std::pair<std::string, uint32_t>> BeBoard::getSnapshot() const
{
    std::vector<std::pair<std::string, uint32_t>> theModifiedRegisterVector(fModifiedRegisters.begin(), fModifiedRegisters.end());
    return theModifiedRegisterVector;
}

void BeBoard::clearFreeRegisters() { fListOfFreeRegisters.clear(); }

void BeBoard::addFreeRegister(const std::regex& theRegisterName) { fListOfFreeRegisters.push_back(theRegisterName); }

} // namespace Ph2_HwDescription
