/*!

        \file                   MPA2.h
        \brief                  MPA2 Description class, config of the MPA2s
        \author                 Kevin Nash
        \version                1.0
        \date                   12/06/21
        Support :               mail to : knash201@gmail.com

 */
// pretty much a copy of MPA.cc, does not seem to be any relevant changes...

#include "MPA2.h"
#include "Definition.h"
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string.h>

namespace Ph2_HwDescription
{
// C'tors which take BeBoardId, FMCId, FeID, ChipId

MPA2::MPA2(uint8_t pBeBoardId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pChipId, uint8_t pPartnerId, const std::string& filename) : ReadoutChip(pBeBoardId, pFMCId, pOpticalGroupId, pHybridId, pChipId)
{
    fChipCode = 2;
    fMaxRegValue      = 255;
    fChipOriginalMask = std::make_shared<ChannelGroup<NSSACHANNELS * NMPACOLS>>();
    fChipOriginalMask->enableAllChannels();
    fPartnerId = pPartnerId;
    loadfRegMap(filename);
    setFrontEndType(FrontEndType::MPA);
    for(auto& cMapItem: fRegMap)
    {
        if(cMapItem.first.find("_ALL") == std::string::npos) continue;
        cMapItem.second.fControlReg = 1;
    }
}

MPA2::MPA2(const FrontEndDescription& pFeDesc, uint8_t pChipId, uint8_t pPartnerId, const std::string& filename) : ReadoutChip(pFeDesc, pChipId)
{
    fChipCode = 2;
    fMaxRegValue      = 255; // 8 bit registers in MPA
    fChipOriginalMask = std::make_shared<ChannelGroup<NSSACHANNELS, NMPACOLS>>();
    fChipOriginalMask->enableAllChannels();
    fPartnerId = pPartnerId;
    loadfRegMap(filename);
    setFrontEndType(FrontEndType::MPA);
    for(auto& cMapItem: fRegMap)
    {
        if(cMapItem.first.find("_ALL") == std::string::npos) continue;
        cMapItem.second.fControlReg = 1;
    }
}

void MPA2::loadfRegMap(const std::string& filename)
{ // start loadfRegMap
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

                fRegMap[fName] = fRegItem;
                // std::cout << __PRETTY_FUNCTION__ <<fName<<"," <<fRegItem.fValue << std::endl;
                cLineCounter++;
            }
        }

        file.close();
    }
    else
    {
        LOG(ERROR) << "The MPA2 Settings File " << filename << " does not exist!";
        exit(1);
    }

} // end loadfRegMap

void MPA2::saveRegMap(const std::string& filename)
{ // start saveRegMap

    std::ofstream file(filename.c_str(), std::ios::out | std::ios::trunc);

    if(file)
    {
        std::set<MPARegPair, RegItemComparer> fSetRegItem;

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

bool MPARegItemComparer::operator()(const MPARegPair& pRegItem1, const MPARegPair& pRegItem2) const
{
    if(pRegItem1.second.fPage != pRegItem2.second.fPage)
        return pRegItem1.second.fPage < pRegItem2.second.fPage;
    else
        return pRegItem1.second.fAddress < pRegItem2.second.fAddress;
}

} // namespace Ph2_HwDescription
