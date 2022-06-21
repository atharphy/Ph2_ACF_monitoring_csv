/*!
  \file                  RD53.cc
  \brief                 RD53 implementation class, config of the RD53
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinard@cern.ch
*/

#include "RD53.h"

namespace Ph2_HwDescription
{
RD53::RD53(uint8_t pBeId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pRD53Id, uint8_t pRD53Lane, const std::string& fileName, const std::string& cfgComment)
    : ReadoutChip(pBeId, pFMCId, pOpticalGroupId, pHybridId, pRD53Id)
{
    fMaxRegValue      = RD53Shared::setBits(RD53Constants::NBIT_MAXREG);
    fChipOriginalMask = std::make_shared<ChannelGroup<nRows, nCols>>();
    configFileName    = fileName;
    RD53::loadfRegMap(configFileName);
    this->setFrontEndType(FrontEndType::RD53);
    myComment  = cfgComment;
    myChipLane = pRD53Lane;
}

RD53::RD53(const RD53& chipObj) : ReadoutChip(chipObj) {}

void RD53::loadfRegMap(const std::string& fileName)
{
    std::ifstream      file(fileName.c_str(), std::ios::in);
    std::stringstream  myString;
    perColumnPixelData pixData(RD53::getNRows());

    if(file.good() == true)
    {
        std::string  line, fName, fAddress_str, fDefValue_str, fValue_str, fBitSize_str;
        bool         foundPixelConfig = false;
        int          cLineCounter     = 0;
        unsigned int col              = 0;
        ChipRegItem  fRegItem;

        while(getline(file, line))
        {
            if(line.find_first_not_of(" \t") == std::string::npos || line.at(0) == '#' || line.at(0) == '*' || line.empty())
                fCommentMap[cLineCounter] = line;
            else if((line.find("PIXELCONFIGURATION") != std::string::npos) || (foundPixelConfig == true))
            {
                foundPixelConfig = true;

                if(line.find("COL") != std::string::npos)
                {
                    std::fill(pixData.Enable.begin(), pixData.Enable.begin(), 0);
                    std::fill(pixData.HitBus.begin(), pixData.HitBus.end(), 0);
                    std::fill(pixData.InjEn.begin(), pixData.InjEn.end(), 0);
                    std::fill(pixData.TDAC.begin(), pixData.TDAC.end(), 0);
                }
                else if(line.find("ENABLE") != std::string::npos)
                {
                    line.erase(line.find("ENABLE"), 6);
                    myString.str("");
                    myString.clear();
                    myString << line;
                    unsigned int row = 0;
                    std::string  readWord;

                    while(getline(myString, readWord, ','))
                    {
                        readWord.erase(std::remove_if(readWord.begin(), readWord.end(), isspace), readWord.end());
                        if(std::all_of(readWord.begin(), readWord.end(), isdigit))
                        {
                            pixData.Enable[row] = atoi(readWord.c_str());
                            if(pixData.Enable[row] == 0) fChipOriginalMask->disableChannel(row, col);
                            row++;
                        }
                    }

                    if(row < nRows)
                    {
                        myString.str("");
                        myString.clear();
                        myString << "[RD53::loadfRegMap] Error, problem reading RD53 config file: too few rows (" << row << ") for column " << fPixelsMask.size();
                        throw Exception(myString.str().c_str());
                    }

                    col++;
                }
                else if(line.find("HITBUS") != std::string::npos)
                {
                    line.erase(line.find("HITBUS"), 6);
                    myString.str("");
                    myString.clear();
                    myString << line;
                    unsigned int row = 0;
                    std::string  readWord;

                    while(getline(myString, readWord, ','))
                    {
                        readWord.erase(std::remove_if(readWord.begin(), readWord.end(), isspace), readWord.end());
                        if(std::all_of(readWord.begin(), readWord.end(), isdigit))
                        {
                            pixData.HitBus[row] = atoi(readWord.c_str());
                            row++;
                        }
                    }

                    if(row < nRows)
                    {
                        myString.str("");
                        myString.clear();
                        myString << "[RD53::loadfRegMap] Error, problem reading RD53 config file: too few rows (" << row << ") for column " << fPixelsMask.size();
                        throw Exception(myString.str().c_str());
                    }
                }
                else if(line.find("INJEN") != std::string::npos)
                {
                    line.erase(line.find("INJEN"), 5);
                    myString.str("");
                    myString.clear();
                    myString << line;
                    unsigned int row = 0;
                    std::string  readWord;

                    while(getline(myString, readWord, ','))
                    {
                        readWord.erase(std::remove_if(readWord.begin(), readWord.end(), isspace), readWord.end());
                        if(std::all_of(readWord.begin(), readWord.end(), isdigit))
                        {
                            pixData.InjEn[row] = atoi(readWord.c_str());
                            row++;
                        }
                    }

                    if(row < nRows)
                    {
                        myString.str("");
                        myString.clear();
                        myString << "[RD53::loadfRegMap] Error, problem reading RD53 config file: too few rows (" << row << ") for column " << fPixelsMask.size();
                        throw Exception(myString.str().c_str());
                    }
                }
                else if(line.find("TDAC") != std::string::npos)
                {
                    line.erase(line.find("TDAC"), 4);
                    myString.str("");
                    myString.clear();
                    myString << line;
                    unsigned int row = 0;
                    std::string  readWord;

                    while(getline(myString, readWord, ','))
                    {
                        readWord.erase(std::remove_if(readWord.begin(), readWord.end(), isspace), readWord.end());
                        if(std::all_of(readWord.begin(), readWord.end(), isdigit))
                        {
                            pixData.TDAC[row] = atoi(readWord.c_str());
                            row++;
                        }
                    }

                    if(row < nRows)
                    {
                        myString.str("");
                        myString.clear();
                        myString << "[RD53::loadfRegMap] Error, problem reading RD53 config file: too few rows (" << row << ") for column " << fPixelsMask.size();
                        throw Exception(myString.str().c_str());
                    }

                    fPixelsMask.push_back(pixData);
                }
            }
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
                    throw Exception("[RD53::loadfRegMap] Error, unknown base");
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
                    throw Exception("[RD53::loadfRegMap] Error, unknown base");
                }

                fValue_str.erase(0, 2);
                fRegItem.fValue = strtoul(fValue_str.c_str(), 0, baseType);

                fRegItem.fPage    = 0;
                fRegItem.fBitSize = strtoul(fBitSize_str.c_str(), 0, 10);
                fRegMap[fName]    = fRegItem;
            }

            cLineCounter++;
        }

        fPixelsMaskDefault = fPixelsMask;
        file.close();
    }
    else
        throw Exception("[RD53::loadfRegMapd] The RD53 file settings does not exist");
}

void RD53::saveRegMap(const std::string& fName2Add)
{
    const int Nspaces = 26; // @CONST@

    std::string   output = RD53::getFileName(fName2Add);
    std::ofstream file(output.c_str(), std::ios::out | std::ios::trunc);

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

        file << std::dec << std::endl;
        file << "*-----------------------------------------------------------------------------------------------------"
                "--"
             << std::endl;
        file << "PIXELCONFIGURATION" << std::endl;
        file << "*-----------------------------------------------------------------------------------------------------"
                "--"
             << std::endl;
        for(auto col = 0u; col < fPixelsMask.size(); col++)
        {
            file << "COL                  " << std::setfill('0') << std::setw(3) << col << std::endl;

            file << "ENABLE " << +fPixelsMask[col].Enable[0];
            for(auto row = 1u; row < fPixelsMask[col].Enable.size(); row++) file << "," << +fPixelsMask[col].Enable[row];
            file << std::endl;

            file << "HITBUS " << +fPixelsMask[col].HitBus[0];
            for(auto row = 1u; row < fPixelsMask[col].HitBus.size(); row++) file << "," << +fPixelsMask[col].HitBus[row];
            file << std::endl;

            file << "INJEN  " << +fPixelsMask[col].InjEn[0];
            for(auto row = 1u; row < fPixelsMask[col].InjEn.size(); row++) file << "," << +fPixelsMask[col].InjEn[row];
            file << std::endl;

            file << "TDAC   " << +fPixelsMask[col].TDAC[0];
            for(auto row = 1u; row < fPixelsMask[col].TDAC.size(); row++) file << "," << +fPixelsMask[col].TDAC[row];
            file << std::endl;

            file << std::endl;
        }

        file.close();
    }
    else
        LOG(ERROR) << BOLDRED << "Error opening file " << BOLDYELLOW << output << RESET;
}

void RD53::copyMaskFromDefault()
{
    for(auto col = 0u; col < fPixelsMask.size(); col++)
    {
        fPixelsMask[col].Enable = fPixelsMaskDefault[col].Enable;
        fPixelsMask[col].HitBus = fPixelsMaskDefault[col].HitBus;
        fPixelsMask[col].InjEn  = fPixelsMaskDefault[col].InjEn;
        for(auto row = 0u; row < fPixelsMask[col].TDAC.size(); row++) fPixelsMask[col].TDAC[row] = fPixelsMaskDefault[col].TDAC[row];
    }
}

void RD53::copyMaskToDefault(const std::string& which)
// ########################
// # which = all          #
// # which =  en : Enable #
// # which =  hb : HitBus #
// # which =  in : InjEn  #
// # which =  td : TDAC   #
// ########################
{
    for(auto col = 0u; col < fPixelsMaskDefault.size(); col++)
    {
        if((which == "all") || (which == "en")) fPixelsMaskDefault[col].Enable = fPixelsMask[col].Enable;
        if((which == "all") || (which == "hb")) fPixelsMaskDefault[col].HitBus = fPixelsMask[col].HitBus;
        if((which == "all") || (which == "in")) fPixelsMaskDefault[col].InjEn = fPixelsMask[col].InjEn;
        if((which == "all") || (which == "td"))
            for(auto row = 0u; row < fPixelsMaskDefault[col].TDAC.size(); row++) fPixelsMaskDefault[col].TDAC[row] = fPixelsMask[col].TDAC[row];
    }
}

void RD53::resetMask()
{
    for(auto col = 0u; col < fPixelsMask.size(); col++)
    {
        fPixelsMask[col].Enable.fill(0);
        fPixelsMask[col].HitBus.fill(0);
        fPixelsMask[col].InjEn.fill(0);
        fPixelsMask[col].TDAC.fill(RD53Shared::setBits(RD53Constants::NBIT_TDAC) / 2);
    }
}

void RD53::enableAllPixels()
{
    for(auto col = 0u; col < fPixelsMask.size(); col++)
    {
        fPixelsMask[col].Enable.fill(1);
        fPixelsMask[col].HitBus.fill(1);
    }
}

void RD53::disableAllPixels()
{
    for(auto col = 0u; col < fPixelsMask.size(); col++)
    {
        fPixelsMask[col].Enable.fill(0);
        fPixelsMask[col].HitBus.fill(0);
    }
}

size_t RD53::getNbMaskedPixels()
{
    size_t cnt = 0;

    for(auto col = 0u; col < fPixelsMask.size(); col++)
        for(auto row = 0u; row < fPixelsMask[col].Enable.size(); row++)
            if(fPixelsMask[col].Enable[row] == 0) cnt++;

    return cnt;
}

void RD53::enablePixel(unsigned int row, unsigned int col, bool enable)
{
    fPixelsMask[col].Enable[row] = enable;
    fPixelsMask[col].HitBus[row] = enable;
}

void RD53::injectPixel(unsigned int row, unsigned int col, bool inject) { fPixelsMask[col].InjEn[row] = inject; }

void RD53::setTDAC(unsigned int row, unsigned int col, uint8_t TDAC) { fPixelsMask[col].TDAC[row] = TDAC; }

void RD53::resetTDAC()
{
    for(auto col = 0u; col < fPixelsMask.size(); col++) fPixelsMask[col].TDAC.fill(RD53Shared::setBits(RD53Constants::NBIT_TDAC) / 2);
}

uint8_t RD53::getTDAC(unsigned int row, unsigned int col) { return fPixelsMask[col].TDAC[row]; }

uint32_t RD53::getNumberOfChannels() const { return RD53::getNRows() * RD53::getNCols(); }

bool RD53::isDACLocal(const std::string& regName)
{
    if(regName != "PIX_PORTAL") return false;
    return true;
}

uint8_t RD53::getNumberOfBits(const std::string& regName)
{
    auto it = fRegMap.find(regName);
    if(it == fRegMap.end()) return 0;
    return it->second.fBitSize;
}

RD53::CalCmd::CalCmd(const uint8_t& cal_edge_mode, const uint8_t& cal_edge_delay, const uint8_t& cal_edge_width, const uint8_t& cal_aux_mode, const uint8_t& cal_aux_delay)
    : cal_edge_mode(cal_edge_mode), cal_edge_delay(cal_edge_delay), cal_edge_width(cal_edge_width), cal_aux_mode(cal_aux_mode), cal_aux_delay(cal_aux_delay)
{
}

void RD53::CalCmd::setCalCmd(const uint8_t& _cal_edge_mode, const uint8_t& _cal_edge_delay, const uint8_t& _cal_edge_width, const uint8_t& _cal_aux_mode, const uint8_t& _cal_aux_delay)
{
    cal_edge_mode  = _cal_edge_mode;
    cal_edge_delay = _cal_edge_delay;
    cal_edge_width = _cal_edge_width;
    cal_aux_mode   = _cal_aux_mode;
    cal_aux_delay  = _cal_aux_delay;
}

uint32_t RD53::CalCmd::getCalCmd(const uint8_t& chipId) { return bits::pack<4, 1, 3, 6, 1, 5>(chipId, cal_edge_mode, cal_edge_delay, cal_edge_width, cal_aux_mode, cal_aux_delay); }

} // namespace Ph2_HwDescription
