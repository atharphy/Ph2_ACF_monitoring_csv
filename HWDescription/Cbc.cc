/*!

        Filename :                      Cbc.cc
        Content :                       Cbc Description class, config of the Cbcs
        Programmer :                    Lorenzo BIDEGAIN
        Version :                       1.0
        Date of Creation :              25/06/14
        Support :                       mail to : lorenzo.bidegain@gmail.com

 */

#include "Cbc.h"
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

Cbc::Cbc(const FrontEndDescription& pFeDesc, uint8_t pCbcId, const std::string& filename) : ReadoutChip(pFeDesc, pCbcId)
{
    fMaxRegValue      = 255; // 8 bit registers in CBC
    fChipOriginalMask = new ChannelGroup<NCHANNELS, 1>;
    loadfRegMap(filename);
    setFrontEndType(FrontEndType::CBC3);
}

// C'tors which take BeId, FMCId, FeID, CbcId
Cbc::Cbc(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pCbcId, const std::string& filename) : ReadoutChip(pBeId, pFMCId, pFeId, pCbcId)
{
    fMaxRegValue      = 255; // 8 bit registers in CBC
    fChipOriginalMask = new ChannelGroup<NCHANNELS, 1>;
    loadfRegMap(filename);
    setFrontEndType(FrontEndType::CBC3);
}

// load fRegMap from file

void Cbc::loadfRegMap(const std::string& filename)
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
            // std::cout<< __PRETTY_FUNCTION__ << " " << line << std::endl;
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

                if(fRegItem.fPage == 0x00 && fRegItem.fAddress >= 0x20 && fRegItem.fAddress <= 0x3F)
                { // Register is a Mask
                    // if(!fhasMaskedChannels && fRegItem.fValue!=0xFF) fhasMaskedChannels=true;
                    // disable masked channels only
                    if(fRegItem.fValue != 0xFF)
                    {
                        for(uint8_t channel = 0; channel < 8; ++channel)
                        {
                            uint8_t chn = 1 << channel;
                            if((fRegItem.fValue && chn) == 0) { fChipOriginalMask->disableChannel((fRegItem.fAddress - 0x20) * 8 + channel); }
                        }
                    }
                }

                fRegMap[fName] = fRegItem;
                // std::cout << __PRETTY_FUNCTION__ << +fRegItem.fValue << std::endl;
                cLineCounter++;
            }
        }

        file.close();
    }
    else
    {
        LOG(ERROR) << "The CBC Settings File " << filename << " does not exist!";
        exit(1);
    }
}
// Write RegValues in a file
void Cbc::saveRegMap(const std::string& filename)
{
    std::ofstream file(filename.c_str(), std::ios::out | std::ios::trunc);

    if(file)
    {
        std::set<CbcRegPair, RegItemComparer> fSetRegItem;

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
