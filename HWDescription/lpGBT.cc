/*!
  \file                  lpGBT.h
  \brief                 lpGBT implementation class, config of the lpGBT
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "lpGBT.h"

namespace Ph2_HwDescription
{
lpGBT::lpGBT(uint8_t pBeId, uint8_t FMCId, uint8_t pOpticalGroupId, uint8_t pChipId, const std::string& fileName) : Chip(pBeId, FMCId, pOpticalGroupId, 0, pChipId)
{
    fChipAddress   = 0x70 + pChipId;
    configFileName = fileName;
    phaseRxAligned = false; // @TMP@
    setFrontEndType(FrontEndType::LpGBT);
    lpGBT::loadfRegMap(configFileName);
}

void lpGBT::loadfRegMap(const std::string& fileName)
{
    std::ifstream     file(fileName.c_str(), std::ios::in);
    std::stringstream myString;

    if(file.good() == true)
    {
        std::string line, fName, fAddress_str, fDefValue_str, fValue_str, fBitSize_str;
        int         cLineCounter = 0;
        ChipRegItem fRegItem;

        while(getline(file, line))
        {
            if(line.find_first_not_of(" \t") == std::string::npos || line.at(0) == '#' || line.at(0) == '*' || line.empty())
                fCommentMap[cLineCounter] = line;
            else
            {
                myString.str("");
                myString.clear();
                myString << line;
                myString >> fName >> fAddress_str >> fDefValue_str >> fValue_str >> fBitSize_str;

                fRegItem.fAddress = strtoul(fAddress_str.c_str(), 0, 16);

                int baseType;
                if(fDefValue_str.compare(0, 2, "0x") == 0)
                    baseType = 16;
                else if(fDefValue_str.compare(0, 2, "0d") == 0)
                    baseType = 10;
                else if(fDefValue_str.compare(0, 2, "0b") == 0)
                    baseType = 2;
                else
                {
                    LOG(ERROR) << BOLDRED << "Unknown base " << BOLDYELLOW << fDefValue_str << RESET;
                    throw Exception("[lpGBT::loadfRegMap] Error, unknown base");
                }
                fDefValue_str.erase(0, 2);
                fRegItem.fDefValue = strtoul(fDefValue_str.c_str(), 0, baseType);

                if(fValue_str.compare(0, 2, "0x") == 0)
                    baseType = 16;
                else if(fValue_str.compare(0, 2, "0d") == 0)
                    baseType = 10;
                else if(fValue_str.compare(0, 2, "0b") == 0)
                    baseType = 2;
                else
                {
                    LOG(ERROR) << BOLDRED << "Unknown base " << BOLDYELLOW << fValue_str << RESET;
                    throw Exception("[lpGBT::loadfRegMap] Error, unknown base");
                }

                fValue_str.erase(0, 2);
                fRegItem.fValue = strtoul(fValue_str.c_str(), 0, baseType);

                fRegItem.fPage    = 0;
                fRegItem.fBitSize = strtoul(fBitSize_str.c_str(), 0, 10);
                fRegMap[fName]    = fRegItem;
            }

            cLineCounter++;
        }

        file.close();
    }
    else
        throw Exception("[lpGBT::loadfRegMapd] The LpGBT file settings does not exist");
}

std::stringstream lpGBT::saveRegMap(const std::string& fName2Add)
// #################################################################
// # If fName2Add != STREAMON --> then data are also saved on file #
// #################################################################
{
    const unsigned int Nspaces = 26; // @CONST@

    std::stringstream theStream;
    std::ofstream     file;
    std::string       fileName = this->getFileName(fName2Add);

    std::set<ChipRegPair, RegItemComparer> fSetRegItem;
    for(const auto& it: fRegMap) fSetRegItem.insert({it.first, it.second});

    int cLineCounter = 0;
    for(const auto& v: fSetRegItem)
    {
        while(fCommentMap.find(cLineCounter) != std::end(fCommentMap))
        {
            auto cComment = fCommentMap.find(cLineCounter);

            theStream << cComment->second << std::endl;
            cLineCounter++;
        }

        theStream << v.first;
        for(auto j = 0u; j < Nspaces; j++) theStream << " ";
        theStream.seekp(-(v.first.size() < Nspaces ? v.first.size() : Nspaces - 2), std::ios_base::cur);
        theStream << "0x" << std::setfill('0') << std::setw(3) << std::hex << std::uppercase << int(v.second.fAddress);
        for(auto j = 0u; j < 9 - (v.first.size() < Nspaces ? 0 : v.first.size() - Nspaces + 2); j++) theStream << " ";
        theStream << "0x" << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << int(v.second.fDefValue);
        for(auto j = 0u; j < 14; j++) theStream << " ";
        theStream << "0x" << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << int(v.second.fValue);
        for(auto j = 0u; j < 26; j++) theStream << " ";
        theStream << std::setfill('0') << std::setw(1) << std::dec << std::uppercase << int(v.second.fBitSize) << std::endl;

        cLineCounter++;
    }

    if(fName2Add != "STREAMON")
    {
        file.open(fileName.c_str(), std::ios::out | std::ios::trunc);

        if(file)
        {
            file << theStream.str();
            file.close();
        }
        else
            LOG(ERROR) << BOLDRED << "Error opening file " << BOLDYELLOW << fileName << RESET;
    }

    return theStream;
}

} // namespace Ph2_HwDescription
