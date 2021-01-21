/*
        FileName :                     MPAInterface.cc
        Content :                      User Interface to the MPAs
        Programmer :                   K. nash, M. Haranko, D. Ceresa
        Version :                      1.0
        Date of creation :             5/01/18
 */

#include "MPAInterface.h"
#include "../Utils/ConsoleColor.h"
#include <typeinfo>

#define DEV_FLAG 0

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
MPAInterface::MPAInterface(const BeBoardFWMap& pBoardMap) : ReadoutChipInterface(pBoardMap) {}
MPAInterface::~MPAInterface() {}

uint16_t MPAInterface::ReadChipReg(Chip* pMPA, const std::string& pRegNode)
{
    setBoard(pMPA->getBeBoardId());

    ChipRegItem cRegItem = pMPA->getRegItem(pRegNode);

    std::vector<uint32_t> cVecReq;

    fBoardFW->EncodeReg(cRegItem, pMPA->getFeId(), pMPA->getChipId(), cVecReq, true, false);
    fBoardFW->ReadChipBlockReg(cVecReq);

    // bools to find the values of failed and read
    bool    cFailed = false;
    bool    cRead;
    uint8_t cMPAId;
    fBoardFW->DecodeReg(cRegItem, cMPAId, cVecReq[0], cRead, cFailed);
    // std::cout<<"ritemread "<<cRegItem.fValue<<std::endl;
    if(!cFailed) pMPA->setReg(pRegNode, cRegItem.fValue);

    return cRegItem.fValue & 0xFF;
}

bool MPAInterface::WriteChipReg(Chip* pMPA, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop)
{
    setBoard(pMPA->getBeBoardId());
    ChipRegItem cRegItem = pMPA->getRegItem(pRegNode);
    cRegItem.fValue      = pValue & 0xFF;
    std::vector<uint32_t> cVec;
    // std::cout<<pMPA->getFeId()<<" , "<<pMPA->getChipId()<<std::endl;
    fBoardFW->EncodeReg(cRegItem, pMPA->getFeId(), pMPA->getChipId(), cVec, pVerifLoop, true);
    uint8_t cWriteAttempts = 0;
    bool    cSuccess       = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);

    if(cSuccess) pMPA->setReg(pRegNode, pValue);

#ifdef COUNT_FLAG
    fRegisterCount++;
    fTransactionCount++;
#endif
    return cSuccess;
}

bool MPAInterface::WriteChipMultReg(Chip* pMPA, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop)
{
    // first, identify the correct BeBoardFWInterface
    setBoard(pMPA->getBeBoardId());

    std::vector<uint32_t> cVec;

    // Deal with the ChipRegItems and encode them
    ChipRegItem cRegItem;

    for(const auto& cReg: pVecReq)
    {
        if(cReg.second > 0xFF)
        {
            LOG(ERROR) << "MPA register are 8 bits, impossible to write " << cReg.second << " on registed " << cReg.first;
            continue;
        }
        cRegItem        = pMPA->getRegItem(cReg.first);
        cRegItem.fValue = cReg.second;
        fBoardFW->EncodeReg(cRegItem, pMPA->getFeId(), pMPA->getChipId(), cVec, pVerifLoop, true);

        // HACK! take out
        this->WriteChipReg(pMPA, cReg.first, cReg.second, pVerifLoop);

#ifdef COUNT_FLAG
        fRegisterCount++;
#endif
    }

    // write the registers, the answer will be in the same cVec
    // the number of times the write operation has been attempted is given by cWriteAttempts
    // uint8_t cWriteAttempts = 0 ;

    // HACK! put back in
    // bool cSuccess = fBoardFW->WriteChipBlockReg (  cVec, cWriteAttempts, pVerifLoop );
    bool cSuccess = true;

#ifdef COUNT_FLAG
    fTransactionCount++;
#endif

    // if the transaction is successfull, update the HWDescription object
    if(cSuccess)
    {
        for(const auto& cReg: pVecReq)
        {
            cRegItem = pMPA->getRegItem(cReg.first);
            pMPA->setReg(cReg.first, cReg.second);
        }
    }

    return cSuccess;
}

bool MPAInterface::WriteChipAllLocalReg(ReadoutChip* pMPA, const std::string& dacName, ChipContainer& localRegValues, bool pVerifLoop)
{
    setBoard(pMPA->getBeBoardId());
    assert(localRegValues.size() == pMPA->getNumberOfChannels());
    std::string dacTemplate;
    bool        isMask = false;

    if(dacName == "TrimDAC_P")
        dacTemplate = "TrimDAC_P%d";
    else if(dacName == "Mask")
        isMask = true;
    else
        LOG(ERROR) << "Error, DAC " << dacName << " is not a Local DAC";

    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    // std::vector<uint32_t> listOfChannelToUnMask;
    ChannelGroup<NMPACHANNELS, 1> channelToEnable;

    std::vector<uint32_t> cVec;
    cVec.clear();
    for(uint8_t iChannel = 0; iChannel < pMPA->getNumberOfChannels(); ++iChannel)
    {
        if(isMask)
        {
            if(localRegValues.getChannel<uint16_t>(iChannel))
            {
                channelToEnable.enableChannel(iChannel);
                // listOfChannelToUnMask.emplace_back(iChannel);
            }
        }
        else
        {
            char dacName1[20];

            sprintf(dacName1, dacTemplate.c_str(), iChannel + 1);
            // fBoardFW->EncodeReg ( cRegItem, pMPA->getFeId(), pMPA->getChipId(), cVec, pVerifLoop, true );
            // #ifdef COUNT_FLAG
            //     fRegisterCount++;
            // #endif
            cRegVec.emplace_back(dacName1, localRegValues.getChannel<uint16_t>(iChannel));
        }
    }

    if(isMask) { return maskChannelsGroup(pMPA, &channelToEnable, pVerifLoop); }
    else
    {
        // uint8_t cWriteAttempts = 0 ;
        // bool cSuccess = fBoardFW->WriteChipBlockReg ( cVec, cWriteAttempts, pVerifLoop);
        // #ifdef COUNT_FLAG
        //     fTransactionCount++;
        // #endif
        // return cSuccess;
        // fReadoutChipInterface->WriteChipReg(theChip, "THTRIMMING_S" + std::to_string(istrip), THtowrite);

        return WriteChipMultReg(pMPA, cRegVec, pVerifLoop);
    }
}

bool MPAInterface::ConfigureChip(Chip* pMPA, bool pVerifLoop, uint32_t pBlockSize)
{
    uint8_t cWriteAttempts = 0;
    // first, identify the correct BeBoardFWInterface
    setBoard(pMPA->getBeBoardId());
    std::vector<uint32_t> cVec;
    ChipRegMap            cMPARegMap = pMPA->getRegMap();
    int                   NumReg     = 0;
    for(auto& cRegItem: cMPARegMap)
    {
        NumReg++;
#ifdef COUNT_FLAG
        fRegisterCount++;
#endif
        // LOG (INFO) << BOLDRED << "Write "<<cRegItem.first<< RESET;
        fBoardFW->EncodeReg(cRegItem.second, pMPA->getFeId(), pMPA->getChipId(), cVec, pVerifLoop, true);
        bool cSuccess = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
        if(cSuccess)
        {
            auto cReadBack = ReadChipReg(pMPA, cRegItem.first);
            if(cReadBack != cRegItem.second.fValue)
            {
                std::size_t found = (cRegItem.first).find("ReadCounter");
                if(found == std::string::npos)
                {
                    LOG(INFO) << BOLDRED << "Read back value from " << cRegItem.first << BOLDBLUE << " at I2C address " << std::hex << pMPA->getRegItem(cRegItem.first).fAddress << std::dec
                              << " not equal to write value of " << std::hex << +cRegItem.second.fValue << std::dec << RESET;
                    // return false;
                }
            }
        }
        // LOG (INFO) << BOLDRED << "READ "<<ReadChipReg( pMPA, cRegItem.first )<< RESET;
        // LOG (INFO) << BOLDBLUE << cRegItem.first << "  <   " << BOLDRED << cSuccess << RESET;
        if(not cSuccess) return false;
        cVec.clear();
    }

    LOG(INFO) << BOLDGREEN << "Wrote: " << NumReg << RESET;
#ifdef COUNT_FLAG
    fTransactionCount++;
#endif
    return true;
}

void MPAInterface::setFileHandler(FileHandler* pHandler)
{
    setBoard(0);
    fBoardFW->setFileHandler(pHandler);
}

// These are not currently used but can encode pix registers
void MPAInterface::Pix_write(ReadoutChip* cMPA, ChipRegItem cRegItem, uint32_t row, uint32_t pixel, uint32_t data)
{
    uint8_t cWriteAttempts = 0;

    ChipRegItem rowreg = cRegItem;
    rowreg.fAddress    = ((row & 0x0001f) << 11) | ((cRegItem.fAddress & 0x000f) << 7) | (pixel & 0xfffffff);
    rowreg.fValue      = data;
    std::vector<uint32_t> cVecReq;
    cVecReq.clear();
    fBoardFW->EncodeReg(rowreg, cMPA->getFeId(), cMPA->getChipId(), cVecReq, false, true);
    fBoardFW->WriteChipBlockReg(cVecReq, cWriteAttempts, false);
}

uint32_t MPAInterface::Pix_read(ReadoutChip* cMPA, ChipRegItem cRegItem, uint32_t row, uint32_t pixel)
{
    uint8_t  cWriteAttempts = 0;
    uint32_t rep;

    std::vector<uint32_t> cVecReq;
    cVecReq.clear();
    fBoardFW->EncodeReg(cRegItem, cMPA->getFeId(), cMPA->getChipId(), cVecReq, false, false);
    fBoardFW->WriteChipBlockReg(cVecReq, cWriteAttempts, false);
    std::chrono::milliseconds cShort(1);

    rep = this->ReadChipReg(cMPA, "fc7_daq_ctrl.command_processor_block.i2c.mpa_MPA_i2c_reply.data");

    return rep;
}

Stubs MPAInterface::Format_stubs(std::vector<std::vector<uint8_t>> rawstubs)
{
    int   j     = 0;
    int   cycle = 0;
    Stubs formstubs;
    for(int i = 0; i < 39; i++)
    {
        if((rawstubs[0][i] & 0x80) == 128)
        {
            j = i + 1;
            formstubs.pos.push_back(std::vector<uint8_t>(5, 0));
            formstubs.row.push_back(std::vector<uint8_t>(5, 0));
            formstubs.cur.push_back(std::vector<uint8_t>(5, 0));

            formstubs.nst.push_back(((rawstubs[1][i] & 0x80) >> 5) | ((rawstubs[2][i] & 0x80) >> 6) | ((rawstubs[3][i] & 0x80) >> 7));
            formstubs.pos[cycle][0] = ((rawstubs[4][i] & 0x80) << 0) | ((rawstubs[0][i] & 0x40) << 0) | ((rawstubs[1][i] & 0x40) >> 1) | ((rawstubs[2][i] & 0x40) >> 2) |
                                      ((rawstubs[3][i] & 0x40) >> 3) | ((rawstubs[4][i] & 0x40) >> 4) | ((rawstubs[0][i] & 0x20) >> 4) | ((rawstubs[1][i] & 0x20) >> 5);
            formstubs.pos[cycle][1] = ((rawstubs[4][i] & 0x10) << 3) | ((rawstubs[0][i] & 0x8) << 3) | ((rawstubs[1][i] & 0x8) << 2) | ((rawstubs[2][i] & 0x8) << 1) | ((rawstubs[3][i] & 0x8) << 0) |
                                      ((rawstubs[4][i] & 0x8) >> 1) | ((rawstubs[0][i] & 0x4) >> 1) | ((rawstubs[1][i] & 0x4) >> 2);
            formstubs.pos[cycle][2] = ((rawstubs[4][i] & 0x2) << 6) | ((rawstubs[0][i] & 0x1) << 6) | ((rawstubs[1][i] & 0x1) << 5) | ((rawstubs[2][i] & 0x1) << 4) | ((rawstubs[3][i] & 0x1) << 3) |
                                      ((rawstubs[4][i] & 0x1) << 3) | ((rawstubs[1][j] & 0x80) >> 6) | ((rawstubs[2][j] & 0x80) >> 7);
            formstubs.pos[cycle][3] = ((rawstubs[0][j] & 0x20) << 2) | ((rawstubs[1][j] & 0x20) << 1) | ((rawstubs[2][j] & 0x20) << 0) | ((rawstubs[3][j] & 0x20) >> 1) |
                                      ((rawstubs[4][j] & 0x20) >> 2) | ((rawstubs[0][j] & 0x10) >> 2) | ((rawstubs[1][j] & 0x10) >> 3) | ((rawstubs[2][j] & 0x10) >> 4);
            formstubs.pos[cycle][4] = ((rawstubs[0][j] & 0x4) << 5) | ((rawstubs[1][j] & 0x4) << 4) | ((rawstubs[2][j] & 0x4) << 3) | ((rawstubs[3][j] & 0x4) << 2) | ((rawstubs[4][j] & 0x4) << 1) |
                                      ((rawstubs[0][j] & 0x2) << 1) | ((rawstubs[1][j] & 0x2) << 0) | ((rawstubs[2][j] & 0x2) >> 1);
            formstubs.row[cycle][0] = ((rawstubs[0][i] & 0x10) >> 1) | ((rawstubs[1][i] & 0x10) >> 2) | ((rawstubs[2][i] & 0x10) >> 3) | ((rawstubs[3][i] & 0x10) >> 4);
            formstubs.row[cycle][1] = ((rawstubs[0][i] & 0x2) << 2) | ((rawstubs[1][i] & 0x2) << 1) | ((rawstubs[2][i] & 0x2) << 0) | ((rawstubs[3][i] & 0x2) >> 1);
            formstubs.row[cycle][2] = ((rawstubs[1][j] & 0x40) >> 3) | ((rawstubs[2][j] & 0x40) >> 4) | ((rawstubs[3][j] & 0x40) >> 5) | ((rawstubs[4][j] & 0x40) >> 6);
            formstubs.row[cycle][3] = ((rawstubs[1][j] & 0x8) >> 0) | ((rawstubs[2][j] & 0x8) >> 1) | ((rawstubs[3][j] & 0x8) >> 2) | ((rawstubs[4][j] & 0x8) >> 3);
            formstubs.row[cycle][4] = ((rawstubs[1][j] & 0x1) << 3) | ((rawstubs[2][j] & 0x1) << 2) | ((rawstubs[3][j] & 0x1) << 1) | ((rawstubs[4][j] & 0x1) << 0);
            formstubs.cur[cycle][0] = ((rawstubs[2][i] & 0x20) >> 3) | ((rawstubs[3][i] & 0x20) >> 4) | ((rawstubs[4][i] & 0x20) >> 5);
            formstubs.cur[cycle][1] = ((rawstubs[2][i] & 0x4) >> 0) | ((rawstubs[3][i] & 0x4) >> 1) | ((rawstubs[4][i] & 0x4) >> 2);
            formstubs.cur[cycle][2] = ((rawstubs[3][j] & 0x80) >> 5) | ((rawstubs[4][j] & 0x80) >> 6) | ((rawstubs[0][j] & 0x40) >> 6);
            formstubs.cur[cycle][3] = ((rawstubs[3][j] & 0x10) >> 2) | ((rawstubs[4][j] & 0x10) >> 3) | ((rawstubs[0][j] & 0x8) >> 3);
            formstubs.cur[cycle][4] = ((rawstubs[3][j] & 0x2) << 1) | ((rawstubs[4][j] & 0x2) >> 0) | ((rawstubs[0][j] & 0x1) >> 0);
            // std::cout<<"RS1 "<<+formstubs.pos[cycle][0]<<std::endl;
            // std::cout<<"RS2 "<<+formstubs.pos[cycle][1]<<std::endl;
            // std::cout<<"RS3 "<<+formstubs.pos[cycle][2]<<std::endl;
            // std::cout<<"RS01"<<+rawstubs[1][i]<<std::endl; std::cout<<"RS4 "<<+formstubs.pos[cycle][3]<<std::endl;
            cycle += 1;
        }
    }
    return formstubs;
}

L1data MPAInterface::Format_l1(std::vector<uint8_t> rawl1, bool verbose)
{
    bool    found = false;
    uint8_t header, error(0), L1_ID, strip_counter, pixel_counter;
    L1data  formL1data;

    std::vector<uint16_t> strip_data, pixel_data;
    uint16_t              curdata;

    for(int i = 1; i < 200; i++)
    {
        if((rawl1[i] == 255) & (rawl1[i - 1] == 255) & (!found))
        {
            header        = rawl1[i - 1] << 11 | rawl1[i - 1] << 3 | ((rawl1[i + 1] & 0xE0) >> 5);
            error         = ((rawl1[i + 1] & 0x18) >> 3);
            L1_ID         = ((rawl1[i + 1] & 0x7) << 6) | ((rawl1[i + 2] & 0xFC) >> 2);
            strip_counter = ((rawl1[i + 2] & 0x1) << 4) | ((rawl1[i + 3] & 0xF0) >> 4);
            pixel_counter = ((rawl1[i + 3] & 0xF) << 1) | ((rawl1[i + 4] & 0x80) >> 7);

            uint8_t wordl = 11, counter = 0;
            bool    curbit;
            uint8_t bitmask = 0x80;
            for(int j = 4; j < 50; j++)
            {
                for(int k = 0; k < 8; k++)
                {
                    curbit = (rawl1[i + j] & (bitmask >> k));
                    counter += 1;
                    curdata += (curbit << (wordl - counter));
                    if(counter == wordl)
                    {
                        if(wordl == 11)
                            strip_data.push_back(curdata);
                        else
                            pixel_data.push_back(curdata);
                        if(strip_counter == strip_data.size()) wordl = 14;
                        curdata = 0;
                        counter = 0;
                    }
                }
            }
            found = true;
        }
    }
    if(found)
    {
        formL1data.strip_counter = strip_counter;
        formL1data.pixel_counter = pixel_counter;
        if(verbose)
        {
            std::cout << "Header: " << std::bitset<8>(header) << std::endl;
            std::cout << "Error: " << std::bitset<8>(error) << std::endl;
            std::cout << "L1 ID: " << L1_ID << std::endl;
            std::cout << "Strip counter: " << strip_counter << std::endl;
            std::cout << "Pixel counter: " << pixel_counter << std::endl;
            std::cout << "Strip data:" << std::endl;
        }

        for(auto& sdata: strip_data)
        {
            formL1data.pos_strip.push_back((sdata & 0x7F0) >> 4);
            formL1data.width_strip.push_back((sdata & 0xE) >> 1);
            formL1data.MIP.push_back((sdata & 0x1));

            if(verbose) std::cout << "\tPosition: " << formL1data.pos_strip.back() << "\n\tWidth: " << formL1data.width_strip.back() << "\n\tMIP: " << formL1data.MIP.back() << std::endl;
        }
        if(verbose) std::cout << "Pixel data:" << std::endl;

        for(auto& pdata: pixel_data)
        {
            formL1data.pos_pixel.push_back((pdata & 0x3F80) >> 7);
            formL1data.width_pixel.push_back((pdata & 0x70) >> 4);
            formL1data.Z.push_back((pdata & 0xF) + 1);

            if(verbose) std::cout << "\tPosition: " << formL1data.pos_pixel.back() << "\n\tWidth: " << formL1data.width_pixel.back() << "\n\tRow Number: " << formL1data.Z.back() << std::endl;
        }

        return formL1data;
    }
    else
        std::cout << "Header not found!" << std::endl;

    return formL1data;
}

void MPAInterface::Activate_async(Chip* pMPA) { this->WriteChipReg(pMPA, "ReadoutMode", 0x1); }

void MPAInterface::Activate_sync(Chip* pMPA) { this->WriteChipReg(pMPA, "ReadoutMode", 0x0); }

void MPAInterface::Activate_pp(Chip* pMPA) { this->WriteChipReg(pMPA, "ECM", 0x81); }

void MPAInterface::Activate_ss(Chip* pMPA) { this->WriteChipReg(pMPA, "ECM", 0x41); }

void MPAInterface::Activate_ps(Chip* pMPA) { this->WriteChipReg(pMPA, "ECM", 0x8); }

void MPAInterface::Pix_Smode(ReadoutChip* pMPA, uint32_t p, std::string smode = "edge")
{
    uint32_t smodewrite = 0x0;
    if(smode == "edge") smodewrite = 0x0;
    if(smode == "level") smodewrite = 0x1;
    if(smode == "or") smodewrite = 0x2;
    if(smode == "xor") smodewrite = 0x3;
    this->WriteChipReg(pMPA, "ModeSel_P" + std::to_string(p + 1), smodewrite);
}

void MPAInterface::Enable_pix_BRcal(ReadoutChip* pMPA, uint32_t p, std::string polarity, std::string smode)
{
    uint32_t PixelMask = 1, Polarity = 1, EnEdgeBR = 1, EnLevelBR = 0, Encount = 0, DigCal = 0, AnCal = 0, BRclk = 0;

    if(polarity == "rise")
        Polarity = 1;
    else if(polarity == "fall")
        Polarity = 0;
    else
    {
        std::cout << "bad pol option" << std::endl;
        return;
    }
    if(smode == "level")
    {
        Pix_Smode(pMPA, p, "level");
        EnEdgeBR = 0, EnLevelBR = 1, Encount = 1, AnCal = 1;
    }
    else if(smode == "edge")
    {
        Pix_Smode(pMPA, p, "edge");
        EnEdgeBR = 1, EnLevelBR = 0, Encount = 1, AnCal = 1;
    }
    else
    {
        std::cout << "bad edge option" << std::endl;
        return;
    }
    Pix_Set_enable(pMPA, p, PixelMask, Polarity, EnEdgeBR, EnLevelBR, Encount, DigCal, AnCal, BRclk);
}

void MPAInterface::Enable_pix_counter(ReadoutChip* pMPA, uint32_t p)
{
    uint32_t PixelMask = 1, Polarity = 1, EnEdgeBR = 0, EnLevelBR = 0, Encount = 1, DigCal = 0, AnCal = 1, BRclk = 0;
    Pix_Set_enable(pMPA, p, PixelMask, Polarity, EnEdgeBR, EnLevelBR, Encount, DigCal, AnCal, BRclk);
}

void MPAInterface::Enable_pix_sync(ReadoutChip* pMPA, uint32_t p)
{
    uint32_t PixelMask = 1, Polarity = 1, EnEdgeBR = 0, EnLevelBR = 0, Encount = 1, DigCal = 0, AnCal = 1, BRclk = 0;
    Pix_Set_enable(pMPA, p, PixelMask, Polarity, EnEdgeBR, EnLevelBR, Encount, DigCal, AnCal, BRclk);
}

void MPAInterface::Disable_pixel(ReadoutChip* pMPA, uint32_t p)
{
    uint32_t PixelMask = 0, Polarity = 0, EnEdgeBR = 0, EnLevelBR = 0, Encount = 0, DigCal = 0, AnCal = 0, BRclk = 0;
    Pix_Set_enable(pMPA, p, PixelMask, Polarity, EnEdgeBR, EnLevelBR, Encount, DigCal, AnCal, BRclk);
}

void MPAInterface::Enable_pix_digi(ReadoutChip* pMPA, uint32_t p)
{
    uint32_t PixelMask = 0, Polarity = 0, EnEdgeBR = 0, EnLevelBR = 0, Encount = 0, DigCal = 1, AnCal = 0, BRclk = 0;
    Pix_Set_enable(pMPA, p, PixelMask, Polarity, EnEdgeBR, EnLevelBR, Encount, DigCal, AnCal, BRclk);
}

void MPAInterface::Pix_Set_enable(ReadoutChip* pMPA,
                                  uint32_t     p,
                                  uint32_t     PixelMask = 1,
                                  uint32_t     Polarity  = 1,
                                  uint32_t     EnEdgeBR  = 1,
                                  uint32_t     EnLevelBR = 0,
                                  uint32_t     Encount   = 0,
                                  uint32_t     DigCal    = 0,
                                  uint32_t     AnCal     = 0,
                                  uint32_t     BRclk     = 0)
{
    uint32_t comboword = (PixelMask) + (Polarity << 1) + (EnEdgeBR << 2) + (EnLevelBR << 3) + (Encount << 4) + (DigCal << 5) + (AnCal << 6) + (BRclk << 7);
    this->WriteChipReg(pMPA, "ENFLAGS_P" + std::to_string(p + 1), comboword);
}

void MPAInterface::Set_calibration(Chip* pMPA, uint32_t cal)
{
    this->WriteChipReg(pMPA, "CalDAC0", cal);
    this->WriteChipReg(pMPA, "CalDAC1", cal);
    this->WriteChipReg(pMPA, "CalDAC2", cal);
    this->WriteChipReg(pMPA, "CalDAC3", cal);
    this->WriteChipReg(pMPA, "CalDAC4", cal);
    this->WriteChipReg(pMPA, "CalDAC5", cal);
    this->WriteChipReg(pMPA, "CalDAC6", cal);
}

void MPAInterface::Set_threshold(Chip* pMPA, uint32_t th)
{
    setBoard(pMPA->getBeBoardId());
    this->WriteChipReg(pMPA, "ThDAC0", th);
    this->WriteChipReg(pMPA, "ThDAC1", th);
    this->WriteChipReg(pMPA, "ThDAC2", th);
    this->WriteChipReg(pMPA, "ThDAC3", th);
    this->WriteChipReg(pMPA, "ThDAC4", th);
    this->WriteChipReg(pMPA, "ThDAC5", th);
    this->WriteChipReg(pMPA, "ThDAC6", th);
}

void MPAInterface::ReadASEvent(ReadoutChip* pMPA, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange)
{
    if(pSRange == std::pair<uint32_t, uint32_t>{0, 0}) pSRange = std::pair<uint32_t, uint32_t>{1, pMPA->getNumberOfChannels()};
    for(uint32_t i = pSRange.first; i <= pSRange.second; i++)
    {
        uint8_t cRP1 = this->ReadChipReg(pMPA, "ReadCounter_LSB_P" + std::to_string(i));
        uint8_t cRP2 = this->ReadChipReg(pMPA, "ReadCounter_MSB_P" + std::to_string(i));

        pData.push_back((cRP2 * 256) + cRP1);
        // std::cout<<i<<" "<<(cRP2*256) + cRP1<<std::endl;
    }
}

bool MPAInterface::enableInjection(ReadoutChip* pChip, bool inject, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    // if sync

    // uint32_t enwrite=1;
    // if(inject) enwrite=17;

    uint32_t enwrite = 0x17;
    if(inject) enwrite = 0x47;

    // std::cout<<"enwrite "<<enwrite<<std::endl;
    for(uint32_t i = 1; i <= pChip->getNumberOfChannels(); i++) this->WriteChipReg(pChip, "ENFLAGS_P" + std::to_string(i), enwrite);
    return true;
}

uint32_t MPAInterface::ReadData(BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait)
{
    setBoard(0);
    return fBoardFW->ReadData(pBoard, pBreakTrigger, pData, pWait);
}

void MPAInterface::Cleardata()
{
    setBoard(0);
    // fBoardFW->Cleardata( );
}

} // namespace Ph2_HwInterface
