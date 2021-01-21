/*!

        Filename :                      Cic.cc
        Content :                       Cic Description class, config of the Cbcs
        Programmer :                    Davide Di Croce, Fabio Ravera, Sarah Seif El Nasr-Storey
        Version :                       1.0
        Date of Creation :              25/06/19
        Support :                       mail to : sarah.storey@cern.ch

 */

#include "Cic.h"
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

Cic::Cic(const FrontEndDescription& pFeDesc, uint8_t pCicId, const std::string& filename) : Chip(pFeDesc, pCicId)
{
    fMaxRegValue = 255; // 8 bit registers in CIC
    loadfRegMap(filename);
    setFrontEndType(FrontEndType::CIC);
}

// C'tors which take BeId, FMCId, FeID, CbcId
Cic::Cic(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pCicId, const std::string& filename) : Chip(pBeId, pFMCId, pFeId, pCicId)
{
    fMaxRegValue = 255; // 8 bit registers in CIC
    loadfRegMap(filename);
    setFrontEndType(FrontEndType::CIC);
}

// load fRegMap from file
void Cic::loadfRegMap(const std::string& filename)
{
    std::ifstream file(filename.c_str(), std::ios::in);

    if(file)
    {
        std::string line, fName, fPage_str, fAddress_str, fDefValue_str, fValue_str;
        int         cLineCounter = 0;
        ChipRegItem fRegItem;

        // fhasMaskedChannels = false;
        while(getline(file, line))
        {
            if(line.find_first_not_of(" \t") == std::string::npos)
            {
                fCommentMap[cLineCounter] = line;
                cLineCounter++;
                // continue;
            }

            else if(line.at(0) == '#' || line.at(0) == '*' || line.empty())
            {
                // if it is a comment, save the line mapped to the line number so I can later insert it in the same
                // place
                fCommentMap[cLineCounter] = line;
                cLineCounter++;
                // continue;
            }
            else
            {
                std::istringstream input(line);
                input >> fName >> fPage_str >> fAddress_str >> fDefValue_str >> fValue_str;

                fRegItem.fPage     = strtoul(fPage_str.c_str(), 0, 16);
                fRegItem.fAddress  = strtoul(fAddress_str.c_str(), 0, 16);
                fRegItem.fDefValue = strtoul(fDefValue_str.c_str(), 0, 16);
                fRegItem.fValue    = strtoul(fValue_str.c_str(), 0, 16);
                fRegMap[fName]     = fRegItem;
                cLineCounter++;
            }
        }

        file.close();
    }
    else
    {
        LOG(ERROR) << "The CIC Settings File " << filename << " does not exist!";
        exit(1);
    }

    for(auto& cRegItem: fRegMap) { LOG(DEBUG) << BOLDBLUE << "CIC register : " << cRegItem.first << " --- " << +cRegItem.second.fValue << RESET; }
}
// Write RegValues in a file
void Cic::saveRegMap(const std::string& filename)
{
    std::ofstream file(filename.c_str(), std::ios::out | std::ios::trunc);
    if(file)
    {
        std::set<CicRegPair, RegItemComparer> fSetRegItem;
        for(auto& it: fRegMap) fSetRegItem.insert({it.first, it.second});

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

            for(int j = 0; j < 48; j++) file << " ";

            file.seekp(-v.first.size(), std::ios_base::cur);

            file << "0x" << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << int(v.second.fPage) << "\t0x" << std::setfill('0') << std::setw(2) << std::hex << std::uppercase
                 << int(v.second.fAddress) << "\t0x" << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << int(v.second.fDefValue) << "\t0x" << std::setfill('0') << std::setw(2)
                 << std::hex << std::uppercase << int(v.second.fValue) << std::endl;

            cLineCounter++;
        }

        file.close();
    }
    else
        LOG(ERROR) << "Error opening file";
}

} // namespace Ph2_HwDescription
