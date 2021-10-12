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
lpGBT::lpGBT(uint8_t pBeId, uint8_t FMCId, uint8_t pOptGroupId, const std::string& fileName) : Chip(pBeId, FMCId, 0, pOptGroupId)
{
    configFileName = fileName;
    phaseRxAligned = false; // @TMP@
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
    {
        LOG(ERROR) << BOLDRED << "The lpGBT file settings " << BOLDYELLOW << fileName << BOLDRED << " does not exist" << RESET;
        exit(EXIT_FAILURE);
    }
}

void lpGBT::saveRegMap(const std::string& fileName)
{
    const int Nspaces = 26;

    std::ofstream file(fileName.c_str(), std::ios::out | std::ios::trunc);

    if(file)
    {
        std::set<ChipRegPair, RegItemComparer> fSetRegItem;
        for(const auto& it: fRegMap) fSetRegItem.insert({it.first, it.second});

        int cLineCounter = 0;
        for(const auto& v: fSetRegItem)
        {
            while(fCommentMap.find(cLineCounter) != std::end(fCommentMap))
            {
                auto cComment = fCommentMap.find(cLineCounter);

                file << cComment->second << std::endl;
                cLineCounter++;
            }

            file << v.first;
            for(auto j = 0; j < Nspaces; j++) file << " ";
            file.seekp(-v.first.size(), std::ios_base::cur);
            file << "0x" << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << int(v.second.fAddress) << "          0x" << std::setfill('0') << std::setw(4) << std::hex
                 << std::uppercase << int(v.second.fDefValue) << "                  0x" << std::setfill('0') << std::setw(4) << std::hex << std::uppercase << int(v.second.fValue)
                 << "                             " << std::setfill('0') << std::setw(2) << std::dec << std::uppercase << int(v.second.fBitSize) << std::endl;

            cLineCounter++;
        }

        file.close();
    }
    else
        LOG(ERROR) << BOLDRED << "Error opening file " << BOLDYELLOW << fileName << RESET;
}
} // namespace Ph2_HwDescription
