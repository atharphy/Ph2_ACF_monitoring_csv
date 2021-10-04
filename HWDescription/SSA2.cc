/*!

        \file                   SSA2.cc
        \brief                  SSA2 Description class, config of the SSAs
        \author                 Marc Osherson (copying from SSA.cc, SSA2 main difference is the implementation of the registers)
        \version                1.0
        \date                   28/12/2020
        Support :               mail to : oshersonmarc@gmail.com

 */

#include "SSA2.h"
#include "../Utils/ChannelGroupHandler.h"
#include "Definition.h"
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string.h>

namespace Ph2_HwDescription
{ // open namespace

SSA2::SSA2(const FrontEndDescription& pFeDesc, uint8_t pSSAId, uint8_t pPartnerId, uint8_t pSSASide, const std::string& filename) : ReadoutChip(pFeDesc, pSSAId)
{
    fMaxRegValue      = 255; // 8 bit registers in SSA2
    fChipOriginalMask = new ChannelGroup<120>;
    fPartnerId        = pPartnerId;
    loadfRegMap(filename);
    setFrontEndType(FrontEndType::SSA2);
}

SSA2::SSA2(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pSSAId, uint8_t pPartnerId, uint8_t pSSASide, const std::string& filename) : ReadoutChip(pBeId, pFMCId, pFeId, pSSAId)
{
    fMaxRegValue      = 255; // 8 bit registers in CBC
    fChipOriginalMask = new ChannelGroup<120>;
    fPartnerId        = pPartnerId;
    loadfRegMap(filename);
    setFrontEndType(FrontEndType::SSA2);
}

void SSA2::loadfRegMap(const std::string& filename)
{ // start loadfRegMap
    std::ifstream file(filename.c_str(), std::ios::in);

    if(file)
    {
        std::string line, fName, fPage_str, fAddress_str, fDefValue_str, fValue_str;
        int         cLineCounter = 0;
        ChipRegItem fRegItem;

        while(getline(file, line))
        {
            if(line.find_first_not_of(" \t") == std::string::npos)
            {
                fCommentMap[cLineCounter] = line;
                cLineCounter++;
            }

            else if(line.at(0) == '#' || line.at(0) == '*' || line.empty())
            {
                // if it is a comment, save the line mapped to the line number so I can later insert it in the same place
                fCommentMap[cLineCounter] = line;
                cLineCounter++;
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
        LOG(ERROR) << "The SSA2 Settings File " << filename << " does not exist!";
        exit(1);
    }
} // end loadfRegMap

void SSA2::saveRegMap(const std::string& filename)
{ // start saveRegMap

    std::ofstream file(filename.c_str(), std::ios::out | std::ios::trunc);

    if(file)
    {
        std::set<SSARegPair, RegItemComparer> fSetRegItem;

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
} // end saveRegMap

} // namespace Ph2_HwDescription
