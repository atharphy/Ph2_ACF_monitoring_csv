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
#include "Parser/ParserDefinitions.h"

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

void BeBoard::parseRegister(pugi::xml_node pRegisterNode, std::string& pAttributeString, double& pValue)
{
    if(std::string(pRegisterNode.name()) == "Register")
    {
        if(std::string(pRegisterNode.first_child().value()).empty())
        {
            if(!pAttributeString.empty()) pAttributeString += ".";

            pAttributeString += pRegisterNode.attribute(COMMON_NAME_ATTRIBUTE_NAME).value();

            for(pugi::xml_node cNode = pRegisterNode.child("Register"); cNode; cNode = cNode.next_sibling())
            {
                std::string cAttributeString = pAttributeString;
                parseRegister(cNode, cAttributeString, pValue);
            }
        }
        else
        {
            if(!pAttributeString.empty()) pAttributeString += ".";

            pAttributeString += pRegisterNode.attribute(COMMON_NAME_ATTRIBUTE_NAME).value();
            pValue = convertAnyDouble(pRegisterNode.first_child().value());
            std::cout << GREEN << "|\t|\t|"
               << "----" << pAttributeString << ": " << BOLDYELLOW << pValue << RESET << std::endl;
            this->setReg(pAttributeString, pValue);
        }
    }
}

// Private Members:

void BeBoard::loadConfigFile(const std::string& filename)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.c_str());
    if(!result) // Try if it is not a file, but a string containing the full xml
        result = doc.load_string(filename.c_str());
    if(!result)
    {
        LOG(ERROR) << BOLDRED << "ERROR : Unable to open the file : " << RESET << filename << std::endl;
        LOG(ERROR) << BOLDRED << "Error description : " << RED << result.description() << RESET << std::endl;
        throw Exception("Unable to parse BeBoard XML source!");
    }

    pugi::xml_node cBeBoardConfigurationNode = doc.child("BeBoardRegister");

    for(pugi::xml_node cBeBoardRegNode = cBeBoardConfigurationNode.child("Register"); cBeBoardRegNode; cBeBoardRegNode = cBeBoardRegNode.next_sibling())
    {
        if(std::string(cBeBoardRegNode.name()) == "Register")
        {
            std::string cNameString;
            double      cValue;
            parseRegister(cBeBoardRegNode, cNameString, cValue);
        }
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
