/*!

        Filename :                      MPA.cc
        Content :                       MPA Description class, config of the MPAs
        Programmer :                    Lorenzo BIDEGAIN
        Version :                       1.0
        Date of Creation :              25/06/14
        Support :                       mail to : lorenzo.bidegain@gmail.com

 */

#include "MPA.h"
#include "Definition.h"
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string.h>

namespace Ph2_HwDescription
{
// C'tors which take BeId, FMCId, FeID, MPAId

MPA::MPA(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pMPAId, uint8_t pPartnerId, const std::string& filename) : ReadoutChip(pBeId, pFMCId, pFeId, pMPAId)
{
    fMaxRegValue      = 255;
    fChipOriginalMask = new ChannelGroup<1920>;
    fPartnerId        = pPartnerId;
    loadfRegMap(filename);
    setFrontEndType(FrontEndType::MPA);
}

MPA::MPA(const FrontEndDescription& pFeDesc, uint8_t pMPAId, uint8_t pPartnerId, const std::string& filename) : ReadoutChip(pFeDesc, pMPAId)
{
    fMaxRegValue      = 255; // 8 bit registers in MPA
    fChipOriginalMask = new ChannelGroup<1920>;
    fPartnerId        = pPartnerId;
    loadfRegMap(filename);
    setFrontEndType(FrontEndType::MPA);
}

void MPA::loadfRegMap(const std::string& filename)
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
                // FIXME this channel masking part is currently using the MPA values. Need to check what the SSA format
                // is
                if(fRegItem.fPage == 0x00 && fRegItem.fAddress >= 0x20 && fRegItem.fAddress <= 0x3F)
                { // Register is a Mask
                    if(fRegItem.fValue != 0xFF)
                    {
                        for(uint8_t channel = 0; channel < 8; ++channel)
                        {
                            if((fRegItem.fValue & (0x1 << channel)) == 0) { fChipOriginalMask->disableChannel((fRegItem.fAddress - 0x20) * 8 + channel); }
                        }
                    }
                }
                fRegMap[fName] = fRegItem;
                // std::cout << __PRETTY_FUNCTION__ <<fName<<"," <<fRegItem.fValue << std::endl;
                cLineCounter++;
            }
        }

        file.close();
    }
    else
    {
        LOG(ERROR) << "The MPA Settings File " << filename << " does not exist!";
        exit(1);
    }

} // end loadfRegMap

void MPA::saveRegMap(const std::string& filename)
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
