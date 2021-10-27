/*
        FileName :                     MPAInterface.cc
        Content :                      User Interface to the MPAs
        Programmer :                   K. nash, M. Haranko, D. Ceresa
        Version :                      1.0
        Date of creation :             5/01/18
 */

#include "MPAInterface.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/ChannelGroupHandler.h"
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
    std::vector<uint32_t> cVecReq;
    ChipRegItem           cRegItem;
    if(pRegNode.find("CounterStrip") != std::string::npos)
    {
        int cChannel = 0;
        sscanf(pRegNode.c_str(), "CounterStrip%d", &cChannel);
        cRegItem.fPage         = 0x00;
        cRegItem.fAddress      = 0x0901 + cChannel;
        cRegItem.fValue        = 0;
        uint8_t cRPLSB         = this->ReadReg(pMPA, cRegItem.fAddress) & 0xFF;
        cRegItem.fPage         = 0x00;
        cRegItem.fAddress      = 0x0801 + cChannel;
        uint8_t  cRPMSB        = this->ReadReg(pMPA, cRegItem.fAddress) & 0xFF;
        uint16_t cCounterValue = (cRPMSB << 8) | cRPLSB;
        LOG(DEBUG) << BOLDBLUE << "Counter MSB is 0x" << std::bitset<8>(cRPMSB) << " Counter LSB is 0x" << std::bitset<8>(cRPLSB) << " Counter value is " << std::hex << +cCounterValue << std::dec
                   << RESET;
        return cCounterValue;
    }
    else if(pRegNode.find("BendCode") != std::string::npos) // configure bend LUT
    {
        std::string cSubStr  = pRegNode.substr(pRegNode.find("BendCode") + std::string("BendCode").length(), pRegNode.length());
        std::string cPattern = "P";
        if(pRegNode.find("P") == std::string::npos) { cPattern = "M"; }
        int      cBendHalfStrips = std::stoi(cSubStr.substr(cSubStr.find(cPattern) + 1, cSubStr.length()));
        int      cIndex          = (cBendHalfStrips + 9) / 2;
        int      cNibble         = (cBendHalfStrips + 9) % 2;
        uint8_t  cBitShift       = 3 * (1 - cNibble);
        uint8_t  cRegMask        = (0x7 << cBitShift); //
        uint16_t cRegAddress     = this->regPeri(pMPA, 5 + cIndex);
        uint16_t cRegValue       = MPAInterface::ReadReg(pMPA, cRegAddress);
        uint8_t  cValue          = (cRegValue & cRegMask) >> cBitShift;
        return cValue;
    }
    else if(PERI_CONFIG_TABLE.find(pRegNode) != PERI_CONFIG_TABLE.end())
    // else if(pRegNode == "ErrorL1")
    {
        return this->readPeri(pMPA, pRegNode);
    }
    else if(pRegNode == "Threshold")
    {
        return this->ReadChipReg(pMPA, "ThDAC0");
    }

    else if(pRegNode == "TriggerLatency")
    {
        uint8_t cLatencyReg1 = pMPA->getRegItem("L1Offset_1").fValue;
        uint8_t cLatencyReg2 = pMPA->getRegItem("L1Offset_2").fValue;
        return (cLatencyReg2 << 8) | cLatencyReg1;
    }
    else
    {
        cRegItem = pMPA->getRegItem(pRegNode);
        return this->ReadReg(pMPA, cRegItem.fAddress) & 0xFF;
    }
}

uint16_t MPAInterface::ReadReg(Chip* pChip, uint16_t pRegisterAddress, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    ChipRegItem cRegItem;
    cRegItem.fPage    = 0x00;
    cRegItem.fAddress = pRegisterAddress;
    cRegItem.fValue   = 0;

    if(!lpGBTFound())
    {
        bool                  cFailed = false;
        bool                  cRead;
        std::vector<uint32_t> cVecReq;
        fBoardFW->EncodeReg(cRegItem, pChip->getHybridId(), pChip->getId() % 8, cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        uint8_t cSSAId;
        fBoardFW->DecodeReg(cRegItem, cSSAId, cVecReq[0], cRead, cFailed);
    }
    else
    {
        // FIXME the FeId is hard coded for now, need to get the FeId info here
        // cRegItem.fValue = flpGBTInterface->mpaRead(flpGBT, pChip->getHybridId(), pChip->getId(), pRegisterAddress);
        cRegItem.fValue = fBoardFW->ReadFERegister(pChip, pRegisterAddress);
    }
    return cRegItem.fValue & 0xFF;
}
void MPAInterface::producePhaseAlignmentPattern(ReadoutChip* pChip, uint8_t pWait_ms )
{
    LOG(INFO) << GREEN << "Producing phase alignment pattern on MPA#" << +pChip->getId() <<  RESET;
    uint8_t                  cAlignmentPattern = 0xAA;
    std::vector<uint8_t>     cRegValues{0x2, cAlignmentPattern};
    std::vector<std::string> cRegNames{"ReadoutMode", "LFSR_data"};
    for(size_t cIndex = 0; cIndex < cRegValues.size(); cIndex++)
    {
        this->WriteChipReg(pChip, cRegNames[cIndex], cRegValues[cIndex]);
    }  // loop over registers
}
void MPAInterface::produceWordAlignmentPattern(ReadoutChip* pChip)
{
    LOG(INFO) << GREEN << "Producing word alignment pattern on MPA#" << +pChip->getId() <<  RESET;
    std::vector<uint8_t>     cRegValues{0x2, fWordAlignmentPatterns[0]};
    std::vector<std::string> cRegNames{"ReadoutMode", "LFSR_data"};
    for(size_t cIndex = 0; cIndex < cRegValues.size(); cIndex++)
    {
        this->WriteChipReg(pChip, cRegNames[cIndex], cRegValues[cIndex]);
    }  // loop over registers
}


void MPAInterface::digiInjection(ReadoutChip* pChip, std::vector<Injection> pInjections, uint8_t pPattern)
{
    // std::vector<uint32_t> cPixelIds(0);
    // for( auto pInjection : pInjections )
    // {
    //     cPixelIds.push_back( (uint32_t)(pInjection.fColumn)*120+(uint32_t)pInjection.fRow );
    // }
    // first make sure all pixels output 0x00
    this->WriteChipReg(pChip, "DigitalSync", 0x00);
    // then .. for pixels I want enable pattern on PixelN
    for(auto pInjection: pInjections)
    {
        uint32_t           cPixelIds = (uint32_t)(pInjection.fColumn) * NSSACHANNELS + (uint32_t)pInjection.fRow;
        std::ostringstream cRegName;
        cRegName << "DigitalSyncP" << std::to_string(cPixelIds);
        LOG(DEBUG) << BOLDMAGENTA << "\t... injecting digitally \t... " << cRegName.str() << " -- " << +pPattern << RESET;
        this->WriteChipReg(pChip, cRegName.str(), pPattern);
    } // injections
}
std::vector<int> MPAInterface::decodeBendCode(ReadoutChip* pChip, uint8_t pBendCode)
{
    std::vector<int>     cBends(0);
    std::vector<uint8_t> cBendLUT    = this->readLUT(pChip);
    int                  cStartValue = -9;
    for(auto cBendCode: cBendLUT)
    {
        if(cBendCode == pBendCode)
        {
            LOG(DEBUG) << BOLDMAGENTA << "BendCode " << std::bitset<3>(pBendCode) << " found in LUT at position "
                       << " which is " << cStartValue << " half strips. " << RESET;
            cBends.push_back(cStartValue);
        }
        cStartValue++;
    }
    return cBends;
}
std::vector<uint8_t> MPAInterface::readLUT(ReadoutChip* pChip)
{
    std::vector<uint8_t> cBendCodes(0); // bend registers are 0 -- 14. Each register encodes 2 codes

    int cStartValue = -9;
    for(size_t cIndex = 0; cIndex < 9; cIndex++) // 9 registers
    {
        uint16_t cRegAddress = 5 + cIndex; // each register controls two codes
        // code starts from dummy
        cRegAddress = this->regPeri(pChip, cRegAddress);
        std::vector<int> cTheseBends{cStartValue, cStartValue + 1};
        uint8_t          cCode = MPAInterface::ReadReg(pChip, cRegAddress);
        LOG(DEBUG) << BOLDMAGENTA << "Reading bend code register 0x" << std::hex << +cRegAddress << std::dec << " this contains the bends for  " << +cTheseBends[0] << " and " << +cTheseBends[1]
                   << " the register value is 0x" << std::hex << +cCode << std::dec << RESET;
        for(int cNibble = 0; cNibble < 2; cNibble++)
        {
            int     cBendHalfStrips = cStartValue + cNibble;
            int     cBitOffset      = (1 - cNibble) * 3;
            uint8_t cBendCode       = (cCode & (0x7 << cBitOffset)) >> cBitOffset;
            LOG(DEBUG) << BOLDMAGENTA << "BendCode for a bend of " << +cBendHalfStrips << " is " << std::bitset<3>(cBendCode) << RESET;
            cBendCodes.push_back(cBendCode);
        }
        cStartValue = cStartValue + 2;
    }
    return cBendCodes;
}
bool MPAInterface::configPixel(Chip* pChip, std::string cReg, int pPixelNum, uint8_t pValue, bool pVerifLoop)
{
    // auto cRowCol = static_cast<MPA*>(pChip)->PNlocal(pPixelNum);
    int      cPixNum     = pPixelNum - 1;
    uint32_t cRow        = (pPixelNum == 0) ? 0 : 1 + cPixNum / NSSACHANNELS;
    uint32_t cColumn     = (pPixelNum == 0) ? 0 : 1 + cPixNum % NSSACHANNELS;
    uint8_t  cRegAddress = PIXEL_CONFIG_TABLE.find(cReg)->second;
    uint16_t cAddress    = this->regPixel(pChip, cRegAddress, cRow, cColumn);
    LOG(DEBUG) << BOLDBLUE
               << "Configuring "
                  ""
               << cReg << " on PXL#" << +pPixelNum << " register is row " << +cRow << " column "
               << +cColumn
               //<< " [built-in MPA row " << +cRowCol.first << " col " << +cRowCol.second << " ]"
               << " register 0x" << std::hex << cAddress << std::dec << " value to write is 0x" << std::hex << +pValue << std::dec << RESET;

    // if global register don't readback
    // if(cRow == 0 || cColumn == 0) {
    //     LOG(INFO) << BOLDMAGENTA << "GLOBAL PXL REG - making sure that I set all registers in the map to the same value.. " << RESET;
    //     // also make sure that you've updated all the values of this register in memory 
    //     for( uint8_t cColumn=0 ; cColumn < 16; cColumn++)
    //     {
    //         for(uint8_t cRow=0; cRow < 120; cRow++)
    //         {
    //             uint32_t cPixelId = 1 + cColumn*120 + cRow; 
    //             std::stringstream cRegName; 
    //             cRegName << PIXEL_CONFIG_TABLE.find(cReg)->first << "_P" << cPixelId; 
    //             pChip->setReg(cRegName.str(), pValue);
    //         }
    //     }
    // }
    pVerifLoop = (cRow == 0 || cColumn == 0) ? false : pVerifLoop;
    return MPAInterface::WriteReg(pChip, cAddress, pValue, pVerifLoop);
}
uint16_t MPAInterface::readPixel(Chip* pChip, std::string cReg, int pPixelNum)
{
    int      cPixNum     = pPixelNum - 1;
    uint32_t cRow        = (pPixelNum == 0) ? 0 : 1 + cPixNum / NSSACHANNELS;
    uint32_t cColumn     = (pPixelNum == 0) ? 0 : 1 + cPixNum % NSSACHANNELS;
    uint8_t  cRegAddress = PIXEL_CONFIG_TABLE.find(cReg)->second;
    uint16_t cAddress    = this->regPixel(pChip, cRegAddress, cRow, cColumn);
    LOG(DEBUG) << BOLDBLUE << "PXL#" << +pPixelNum << " register is row " << +cRow << " column " << +cColumn << " register 0x" << std::hex << cAddress << std::dec << RESET;
    return MPAInterface::ReadReg(pChip, cAddress);
}
bool MPAInterface::maskPixel(Chip* pChip, int pPixelNum, uint8_t pMask, bool pVerifLoop)
{
    // pixel num starts from 1 [0 == global]
    auto    cRegValue = this->readPixel(pChip, "ENFLAGS", pPixelNum);
    uint8_t cNewValue = (cRegValue & 0xFE) | (1 - pMask);
    LOG(DEBUG) << BOLDBLUE << "Setting pixel mask to 0x" << std::hex << +cNewValue << std::dec << " on PixelNum#" << pPixelNum << RESET;
    return this->configPixel(pChip, "ENFLAGS", pPixelNum, cNewValue, pVerifLoop);
}
bool MPAInterface::maskRowCol(Chip* pChip, int pRow, int pColumn, uint8_t pMask, bool pVerifLoop)
{
    // row and col num starts from 1 [0 == global]
    int cPixNum = 1 + (pColumn - 1) * NSSACHANNELS + (pRow - 1); // starting from 1
    return this->maskPixel(pChip, cPixNum, pMask, pVerifLoop);
}
bool MPAInterface::configRow(Chip* pChip, std::string cReg, int pRow, uint8_t pValue, bool pVerifLoop)
{
    // LOG(INFO) << BOLDBLUE << "Configuring row register "
    //     << cReg
    //     << " on MPA#"<< +pChip->getId() << " : " << cReg << " writing " << +pValue << RESET;
    uint8_t cRegAddress = ROW_CONFIG_TABLE.find(cReg)->second;
    if(cReg.find("L1Offset") != std::string::npos)
    {
        bool cFound = pChip->getRegMap().find(cReg) != pChip->getRegMap().end();
        if(!cFound)
        {
            ChipRegItem cRegItem;
            cRegItem.fStatusReg = 1;
            cRegItem.fAddress   = cRegAddress;
            cRegItem.fValue     = pValue;
            pChip->appendToRegMap(cReg, cRegItem);
        }
        else
        {
            pChip->getRegMap().find(cReg)->second.fValue = pValue;
        }
    }

    // if global register don't readback
    pVerifLoop        = (pRow == 0) ? false : pVerifLoop;
    uint16_t cAddress = this->regRow(pChip, cRegAddress, pRow);
    LOG(DEBUG) << BOLDBLUE << "\t... register address 0x" << std::hex << +cAddress << std::dec << RESET;
    return MPAInterface::WriteReg(pChip, cAddress, pValue, pVerifLoop);
}
bool MPAInterface::configPeri(Chip* pChip, std::string cReg, uint8_t pValue, bool pVerifLoop)
{
    // LOG(INFO) << BOLDBLUE << "Configuring peri register "
    //     << cReg
    //     << " on MPA#"<< +pChip->getId() << " : " << cReg << " writing " << +pValue << RESET;
    // LOG (INFO) << BOLDRED << PERI_CONFIG_TABLE.size() << " items in peri map." << RESET;
    // for( auto cMapItem : PERI_CONFIG_TABLE )
    //     LOG (INFO) << cMapItem.first << " " << +cMapItem.second << RESET;
    uint8_t  cRegAddress = (PERI_CONFIG_TABLE.find(cReg))->second;
    uint16_t cAddress    = this->regPeri(pChip, cRegAddress);
    LOG(DEBUG) << BOLDBLUE << "\t... register address 0x" << std::hex << +cAddress << std::dec << RESET;
    return MPAInterface::WriteReg(pChip, cAddress, pValue, pVerifLoop);
}
uint16_t MPAInterface::readPeri(Chip* pChip, std::string cReg)
{
    uint8_t  cRegAddress = (PERI_CONFIG_TABLE.find(cReg))->second;
    uint16_t cAddress    = this->regPeri(pChip, cRegAddress);
    LOG(DEBUG) << BOLDBLUE << "Reading peri register 0x" << std::hex << cAddress << std::dec << RESET;
    return MPAInterface::ReadReg(pChip, cAddress);
}

uint16_t MPAInterface::regPixel(Chip* pChip, int pBaseRegister, int pRow, int pColumn)
{
    uint16_t cRegAddress = ((pRow << 11) | (pBaseRegister << 7) | pColumn);
    return cRegAddress;
}
uint16_t MPAInterface::regPeri(Chip* pChip, int pBaseRegister)
{
    uint16_t cRegAddress = ((0x11 << 11) | (0x0 << 8) | pBaseRegister);
    return cRegAddress;
}
uint16_t MPAInterface::regRow(Chip* pChip, int pBaseRegister, int pRow)
{
    uint16_t cRegAddress = ((pRow << 11) | (pBaseRegister << 7) | 0x79);
    return cRegAddress;
}
bool MPAInterface::setInjectionSchema(ReadoutChip* pCbc, const ChannelGroupBase* group, bool pVerifLoop)
{
    std::bitset<NSSACHANNELS*NMPACOLS> cBitset = std::bitset<NSSACHANNELS*NMPACOLS>(static_cast<const ChannelGroup<NSSACHANNELS*NMPACOLS>*>(group)->getBitset());
    if(cBitset.count() == 0) // no mask set... so do nothing
        return true;
    // figure this out 
    return true;
}
bool MPAInterface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const ChannelGroupBase* group, bool mask, bool inject, bool pVerifLoop)
{
    bool success = true;
    if(mask) success &= maskChannelsGroup(pChip, group, pVerifLoop);
    if(inject) success &= setInjectionSchema(pChip, group, pVerifLoop);
    return success;
}
bool MPAInterface::maskChannelsGroup(ReadoutChip* cChip, const ChannelGroupBase* group, bool pVerifLoop)
{
    // first make sure all pixels are enabled 
    // then mask those disabled 
    const ChannelGroupBase* cOriginalMask = cChip->getChipOriginalMask();
    const ChannelGroup<NSSACHANNELS*NMPACOLS>* groupToMask  = static_cast<const ChannelGroup<NSSACHANNELS*NMPACOLS>*>(group);
    // std::bitset<NCHANNELS> cBitset = std::bitset<NCHANNELS>(static_cast<const ChannelGroup<NCHANNELS>*>(group)->getBitset());
    // auto cBitset = std::bitset<NSSACHANNELS*NMPACOLS>(groupToMask->getBitset() & cOriginalMask->getBitset() );
    LOG(INFO) << BOLDBLUE << "\t... Applying mask to MPA" << +cChip->getId() << " with " << group->getNumberOfEnabledChannels()
               // << " desired mask \t... : " << cBitset
               << " original mask  \t... : " << cOriginalMask << " enabled channels "
               << " original bitset was be \t... " << groupToMask->getBitset()
               << RESET;
    return true;
}
bool MPAInterface::ConfigureChipOriginalMask(ReadoutChip* pCbc, bool pVerifLoop, uint32_t pBlockSize)
{
    ChannelGroup<NSSACHANNELS*NMPACOLS> allChannelEnabledGroup;
    return maskChannelsGroup(pCbc, &allChannelEnabledGroup, pVerifLoop);
}
void MPAInterface::readAllBias(Chip* pChip)
{
    std::vector<std::string> nameDAC{"A", "B", "C", "D", "E", "ThDAC", "CalDAC"};
    for(int ipoint = 0; ipoint < 5; ipoint++)
    {
        for(int iblock = 0; iblock < 7; iblock++)
        {
            std::string DAC = nameDAC[ipoint] + std::to_string(iblock);
            LOG(INFO) << BOLDBLUE << DAC << ": bias:" << +ReadChipReg(pChip, DAC) << " on MPA" << +pChip->getId() << RESET;
        }
    }
}

bool MPAInterface::WriteChipReg(Chip* pMPA, const std::string& pRegName, uint16_t pValue, bool pVerifLoop)
{
    setBoard(pMPA->getBeBoardId());

    LOG (DEBUG) << BOLDMAGENTA << " MPAInterface::WriteChipReg writing to " << pRegName << RESET;

    // need to or success
    if(pRegName.find("ThDAC_ALL") != std::string::npos || pRegName.find("Threshold") != std::string::npos)
    {
        LOG(DEBUG) << BOLDMAGENTA << "Setting threshold on MPA#" << +pMPA->getId() << " to " << pValue << RESET;
        this->Set_threshold(pMPA, pValue);
        return true;
    }
    else if(pRegName == "EnablePhaseAlignmentPattern" ) 
    {
        this->producePhaseAlignmentPattern( static_cast<ReadoutChip*>(pMPA), pValue );
        return true;
    }
    else if(pRegName.find("MaskChannel") != std::string::npos )
    {
       std::string cToken       = "MaskChannel";
       auto        cPixelNum   = std::atoi(pRegName.substr(pRegName.find(cToken) + cToken.length(), 4).c_str());
       LOG (DEBUG) << BOLDMAGENTA << "Masking pixel number " << +cPixelNum << " register is " << pRegName << RESET;
       return maskPixel(pMPA, cPixelNum, pValue, pVerifLoop);
    }
    else if(pRegName.find("SelectEdgeT1") != std::string::npos)
    {
        std::string cRegName  = "EdgeSelT1Raw";
        uint8_t     cBitShift = 1;
        auto        cRegValue = this->ReadChipReg(pMPA, cRegName);
        uint8_t     cRegMask  = (0x1 << cBitShift); //
        cRegMask              = ~(cRegMask);
        uint8_t cValue        = (cRegValue & cRegMask) | (pValue << cBitShift);
        //LOG(INFO) << BOLDMAGENTA << "Setting EdgeSel register for T1 to 0x" << std::hex << +cValue << std::dec << RESET;
        return this->WriteChipSingleReg(pMPA, cRegName, cValue);
    }
    else if(pRegName.find("SelectEdgeL") != std::string::npos)
    {
        std::string cToken    = "SelectEdgeL";
        auto        cLineId   = std::atoi(pRegName.substr(pRegName.find(cToken) + cToken.length(), 1).c_str());
        std::string cRegName  = (cLineId == 0) ? "EdgeSelT1Raw" : "EdgeSelTrig";
        uint8_t     cBitShift = (cLineId == 0) ? cLineId : cLineId - 1;
        auto        cRegValue = this->ReadChipReg(pMPA, cRegName);
        uint8_t     cRegMask  = (0x1 << cBitShift); //
        cRegMask              = ~(cRegMask);
        uint8_t cValue        = (cRegValue & cRegMask) | (pValue << cBitShift);
        //LOG(INFO) << BOLDMAGENTA << "Setting EdgeSel register for Line" << +cLineId << " to 0x" << std::hex << +cValue << std::dec << RESET;
        return this->WriteChipSingleReg(pMPA, cRegName, cValue);
    }
    else if(pRegName.find("SLVSDrive") != std::string::npos)
    {
        uint8_t cBitShift = 0;
        uint8_t cRegMask  = (0x7 << cBitShift); //
        cRegMask          = ~(cRegMask);
        auto    cRegValue = this->ReadChipReg(pMPA, "ConfSLVS");
        uint8_t cValue    = (cRegValue & cRegMask) | (pValue << cBitShift);
        LOG(DEBUG) << BOLDMAGENTA << "Setting SLVS register to 0x" << std::hex << +cValue << std::dec << RESET;
        return this->WriteChipSingleReg(pMPA, "ConfSLVS", cValue);
    }
    else if(pRegName.find("BendCode") != std::string::npos) // configure bend LUT
    {
        std::string cSubStr = pRegName.substr(pRegName.find("BendCode") + std::string("BendCode").length(), pRegName.length());
        // int cSign  =  1;
        std::string cPattern = "P";
        if(pRegName.find("P") == std::string::npos)
        {
            // cSign = -1;
            cPattern = "M";
        }
        int cBendHalfStrips = std::stoi(cSubStr.substr(cSubStr.find(cPattern) + 1, cSubStr.length()));
        //    uint8_t cBendHalfStrips = -9 + cIndex*2 + cNibble;
        int     cIndex       = (cBendHalfStrips + 9) / 2;
        int     cNibble      = (cBendHalfStrips + 9) % 2;
        uint8_t cBitShift    = 3 * (1 - cNibble);
        uint8_t cRegMask     = (0x7 << cBitShift); //
        cRegMask             = ~(cRegMask);
        uint16_t cRegAddress = this->regPeri(pMPA, 5 + cIndex);
        uint16_t cRegValue   = MPAInterface::ReadReg(pMPA, cRegAddress);
        uint8_t  cValue      = (cRegValue & cRegMask) | (pValue << cBitShift);
        return MPAInterface::WriteReg(pMPA, cRegAddress, cValue, pVerifLoop);
    }
    else if(PERI_CONFIG_TABLE.find(pRegName) != PERI_CONFIG_TABLE.end())
    {
        return this->configPeri(pMPA, pRegName, pValue);
    }
    else if(pRegName == "TriggerLatency")
    {
        uint8_t cLatencyReg1 = (0x00FF & pValue);
        uint8_t cLatencyReg2 = (0x0100 & pValue) >> 8;
        bool    cConfigReg1  = this->configRow(pMPA, "L1Offset_1", 0, cLatencyReg1);
        bool    cConfigReg2  = this->configRow(pMPA, "L1Offset_2", 0, cLatencyReg2);
        LOG(DEBUG) << BOLDMAGENTA << "Setting TriggerLatency on MPA to " << pValue << RESET;

        return cConfigReg1 && cConfigReg2;
    }

    else if(pRegName == "StubInputPhase")
    {
        uint8_t cBitShift = 3;
        uint8_t cRegMask  = (0x7 << cBitShift); //
        cRegMask          = ~(cRegMask);
        auto    cReg      = this->ReadChipReg(pMPA, "LatencyRx320");
        uint8_t cValue    = (cReg & cRegMask) | (pValue << cBitShift);
        // LOG(INFO) << BOLDBLUE << "Writing " << std::hex << +cValue << " " << +(cReg & cRegMask) << " " << (pValue << cBitShift) << std::dec << RESET;
        return this->WriteChipSingleReg(pMPA, "LatencyRx320", cValue);
    }
    else if(pRegName == "L1InputPhase")
    {
        uint8_t cBitShift = 0;
        uint8_t cRegMask  = (0x7 << cBitShift); //
        cRegMask          = ~(cRegMask);
        auto    cReg      = this->ReadChipReg(pMPA, "LatencyRx320");
        uint8_t cValue    = (cReg & cRegMask) | (pValue << cBitShift);
        // LOG(INFO) << BOLDBLUE << "Writing " << std::bitset<8>(+cValue) << " mask is " << std::bitset<8>(cReg & cRegMask) << RESET; //"  "<<(pValue <<  cBitShift )<< std::dec << RESET;
        return this->WriteChipSingleReg(pMPA, "LatencyRx320", cValue);
    }
    else if(pRegName == "StubMode")
    {
        uint8_t cBitShift = ECM_TABLE.find("StubMode")->second;
        auto    cReg      = this->readPeri(pMPA, "ECM");
        uint8_t cRegMask  = (0x3 << cBitShift); //
        cRegMask          = ~(cRegMask);
        uint8_t cValue    = (cReg & cRegMask) | (pValue << cBitShift);
        return this->configPeri(pMPA, "ECM", cValue);
    }
    else if(pRegName == "StubWindow")
    {
        uint8_t cBitShift = ECM_TABLE.find("StubWindow")->second;
        auto    cReg      = this->readPeri(pMPA, "ECM");
        uint8_t cRegMask  = (0x3F << cBitShift); // FIX ME _ AUTOMATE THIS
        cRegMask          = ~(cRegMask);
        uint8_t cValue    = (cReg & cRegMask) | (pValue << cBitShift);
        return this->configPeri(pMPA, "ECM", cValue);
    }
    else if(pRegName == "DigitalPattern")
    {
        bool cReadoutMode   = configPeri(pMPA, "ReadoutMode", 0x02);
        bool cConfigPattern = WriteChipSingleReg(pMPA, "LFSR_data", pValue);
        return cReadoutMode && cConfigPattern;
    }
    else if(pRegName.find("DigitalSync") != std::string::npos)
    {
        // tracker mode
        bool cReadoutMode = this->configPeri(pMPA, "ReadoutMode", 0x00);
        // value to change
        // enable digital injection, disable analogue calibration and analogue count
        uint8_t cEnable = 1;
        uint8_t cValue  = (0 << PIXEL_ENABLE_TABLE.find("PixelMask")->second);                       // enable pixel;
        cValue          = (0 << PIXEL_ENABLE_TABLE.find("CounterEnable")->second);                   // disable counter
        cValue          = cValue | (cEnable << PIXEL_ENABLE_TABLE.find("DigitalInjection")->second); // enable digital injection
        cValue          = cValue | (0 << PIXEL_ENABLE_TABLE.find("AnalogueInjection")->second);      // disable analogue injection
        // register mask
        std::vector<std::string> cPixelRegs{"CounterEnable", "DigitalInjection", "AnalogueInjection"};
        uint8_t                  cRegMask = 0x00;
        for(auto cPixelReg: cPixelRegs) cRegMask = cRegMask | (1 << PIXEL_ENABLE_TABLE.find(cPixelReg)->second);
        cRegMask = ~(cRegMask);
        LOG(DEBUG) << BOLDBLUE << "Register mask is 0x" << std::hex << +cRegMask << std::dec << RESET;
        uint8_t cRegValue    = 0x00;
        int     cPixelNumber = 0;
        if(pRegName.find("P") != std::string::npos) // single pixel
        {
            cPixelNumber = std::stoi(pRegName.substr(pRegName.find("P") + 1, pRegName.length()));
            cRegValue    = this->readPixel(pMPA, "ENFLAGS", cPixelNumber);
            cRegValue    = (cRegValue & cRegMask) | cValue;
        }

        else
        {
            std::vector<uint8_t> cBits = {1, 1, 1, 0, 0, cEnable, 0, 0};
            for(auto cBit: cBits) cRegValue = cRegValue | (1 << cBit);
        }
        bool cEnableDigital = this->configPixel(pMPA, "ENFLAGS", cPixelNumber, cRegValue, pVerifLoop);
        // configure pattern
        bool cConfigPattern = this->configPixel(pMPA, "DigiPattern", cPixelNumber, pValue, pVerifLoop);
        return cReadoutMode && cEnableDigital && cConfigPattern;
    }
    else if(pRegName == "AnalogueAsync")
    {
        // readout mode 1 -- ASYNC counter
        ChipRegMask cMask; 
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("AnalogueInjection")->second; 
        cMask.fNbits = 1; 
        pMPA->setRegBits( "ENFLAGS_ALL", cMask , pValue );
        // hit counter- enable 
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("CounterEnable")->second; 
        cMask.fNbits = 1; 
        pMPA->setRegBits( "ENFLAGS_ALL", cMask , pValue );
        // edge BR - enable 
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("EnEdgeBR")->second; 
        cMask.fNbits = 1; 
        pMPA->setRegBits( "ENFLAGS_ALL", cMask , pValue );
        // un-mask all pixels  - pixel enable 
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("PixelMask")->second; 
        cMask.fNbits = 1; 
        pMPA->setRegBits( "ENFLAGS_ALL", cMask , pValue );
        // 
        if(pValue == 1)
            LOG(INFO) << BOLDBLUE << "Enabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        else
            LOG(INFO) << BOLDBLUE << "Disabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        bool cEnableAnalogue = this->configPixel(pMPA, "ENFLAGS", 0, pMPA->getRegItem("ENFLAGS_ALL").fValue, pVerifLoop);
        // enabling async readout for stubs 
        bool    cReadoutMode=true;
        if(pValue == 1)
        {   
            cReadoutMode = configPeri(pMPA, "ReadoutMode", 0x01);
            LOG(INFO) << BOLDBLUE << "Enabling readout of I2C counters on MPA by setting register ReadoutMode to 0x" << std::hex << +pValue << std::dec << RESET;
        }
        else
        {
            cReadoutMode = configPeri(pMPA, "ReadoutMode", 0x00);
            LOG(INFO) << BOLDBLUE << "Disabling readout of I2C counters on MPA by setting register ReadoutMode to 0x" << std::hex << +pValue << std::dec << RESET;
        }
        return cEnableAnalogue && cReadoutMode;
    }
    else if(pRegName == "AnalogueSync")
    {
        // readout mode 1 -- ASYNC counter
        ChipRegMask cMask; 
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("AnalogueInjection")->second; 
        cMask.fNbits = 1; 
        pMPA->setRegBits( "ENFLAGS_ALL", cMask , pValue );
        if(pValue == 1)
            LOG(INFO) << BOLDBLUE << "Enabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        else
            LOG(INFO) << BOLDBLUE << "Disabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        bool cEnableAnalogue = this->configPixel(pMPA, "ENFLAGS", 0, pMPA->getRegItem("ENFLAGS_ALL").fValue, pVerifLoop);
        // enabling async readout for stubs 
        bool    cReadoutMode=configPeri(pMPA, "ReadoutMode", 0x00);
        return cEnableAnalogue && cReadoutMode;
    }
    else if(pRegName == "Threshold" or pRegName == "Bias_THDAC")
    {
        LOG(DEBUG) << BOLDBLUE << "Setting "
                   << " bias thresh to " << +pValue << " on MPA" << +pMPA->getId() << RESET;
        Set_threshold(pMPA, pValue);
        return true;
    }

    else if(pRegName == "InjectedCharge")
    {
        LOG(INFO) << BOLDBLUE << "Setting "
                  << " bias calDac to " << +pValue << " on MPA" << +pMPA->getId() << RESET;

        Set_calibration(pMPA, pValue);

        return true;
    }
    else
    {
        LOG (DEBUG) << BOLDMAGENTA << "Writing " << +pValue << " to " << pRegName << RESET;
        return this->WriteChipSingleReg(pMPA, pRegName, pValue, pVerifLoop);
    }
}

bool MPAInterface::WriteChipSingleReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop)
{
    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    auto cRegItem = pChip->getRegMap().find(pRegNode)->second; 
    if( fTrackRegisters ) UpdateModifiedRegMap(pChip,  cRegItem.fAddress); 
    cRegItem.fValue = pValue & 0xFF;
    bool cCheckReadback = (pRegNode.find("_ALL") != std::string::npos) ? false : pVerifLoop;
    cCheckReadback = cCheckReadback && (cRegItem.fStatusReg == 0);
    if(!lpGBTFound())
    {
        std::vector<uint32_t> cVec;
        fBoardFW->EncodeReg(cRegItem, pChip->getHybridId(), pChip->getId() % 8, cVec, pVerifLoop, true);
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
    }
    else
    {
        cSuccess     = fBoardFW->WriteFERegister(pChip, cRegItem.fAddress, cRegItem.fValue, cCheckReadback);
        if(cSuccess){ 
            LOG (DEBUG) << BOLDMAGENTA << "Updating value of " << pRegNode << " in memory to " << +cRegItem.fValue << RESET;
            pChip->setReg(pRegNode, cRegItem.fValue, cRegItem.fPrmptCfg, 1);
        }
    }
    if(cSuccess && !lpGBTFound()) // check is done in lpGBTInterface for opto
    {
        bool cVerify = pVerifLoop && (cRegItem.fStatusReg == 0);
        cSuccess     = (cVerify) ? (ReadChipReg(pChip, pRegNode) == pValue) : true;
    }
    if(cSuccess)
    {
        pChip->setReg(pRegNode, cRegItem.fValue, cRegItem.fPrmptCfg, 1);
        cRegItem = pChip->getRegItem(pRegNode);
        // LOG (INFO) << BOLDGREEN << "\t\t... MPAInterface::WriteChipSingleReg written " << pValue << " to " << pRegNode << " Status flag is " << +cRegItem.fStatusReg << RESET;
    }
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
            pMPA->setReg(cReg.first, cRegItem.fValue, cRegItem.fPrmptCfg, 1);
        }
    }

    return cSuccess;
}

bool MPAInterface::WriteChipAllLocalReg(ReadoutChip* pMPA, const std::string& dacName, ChipContainer& localRegValues, bool pVerifLoop)
{
    setBoard(pMPA->getBeBoardId());
    assert(localRegValues.size() == pMPA->getNumberOfChannels());
    std::string dacTemplate;

    if(dacName == "TrimDAC_P" or dacName == "ThresholdTrim")
    {
        if(pMPA->getFrontEndType() == FrontEndType::MPA)
            dacTemplate = "TrimDAC_P%d";
        else if(pMPA->getFrontEndType() == FrontEndType::SSA)
            dacTemplate = "THTRIMMING_S%d";
    }

    else
        LOG(ERROR) << "Error, DAC " << dacName << " is not a Local DAC";

    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    ChannelGroup<NMPACHANNELS, 1>                 channelToEnable;
    std::vector<uint32_t>                         cVec;
    cVec.clear();
    bool cSuccess = true;

    for(uint16_t iChannel = 0; iChannel < pMPA->getNumberOfChannels(); ++iChannel)
    {
        char dacName1[20];
        sprintf(dacName1, dacTemplate.c_str(), 1 + iChannel);
        LOG(DEBUG) << BOLDBLUE << "Setting register " << dacName1 << " to " << (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F) << RESET;
        cSuccess = cSuccess && this->WriteChipReg(pMPA, dacName1, (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F), pVerifLoop);
    }
    return cSuccess;
}

bool MPAInterface::ConfigureChip(Chip* pMPA, bool pVerifLoop, uint32_t pBlockSize)
{
    fTrackRegisters=false;
    // for now ...
    bool              cSkipLocalRegs = true;
    std::stringstream cOutput;
    setBoard(pMPA->getBeBoardId());
    pMPA->printChipType(cOutput);
    LOG(INFO) << BOLDBLUE << cOutput.str() << "...Configuring chip with Id[" << +pMPA->getId() << "]" << RESET;

    std::vector<uint32_t> cVec;
    ChipRegMap            cMPARegMap = pMPA->getRegMap();
    // for some reason this makes block write work
    // otherwise need to configure one by one which
    // takes forever
    for(auto& cRegItem: cMPARegMap)
    {
        fMap[cRegItem.second.fAddress] = cRegItem.first;
        // update map to indicate that there are not registers that
        // can be read back from
        if(cRegItem.first.find("_ALL") != std::string::npos) { pMPA->setReg(cRegItem.first, cRegItem.second.fValue, cRegItem.second.fPrmptCfg, 1); }
    }
    // update map
    cMPARegMap = pMPA->getRegMap();
    std::vector<std::pair<uint16_t, uint16_t>> cRegs;
    cRegs.clear();
    std::vector<std::string> cRegsToConfig;
    //cRegsToConfig.push_back("TrimDAC");
    for(auto& cMapItem: fMap)
    {
        bool cSkip=(cSkipLocalRegs && (cMapItem.second.find("_P") != std::string::npos));
        if(cSkip && cRegsToConfig.size() > 0 ) 
        {
            // check if this is one to skip 
            bool cRegFound=false;
            auto cIter = cRegsToConfig.begin(); 
            do
            {
                cRegFound = cMapItem.second.find(*cIter) != std::string::npos ;
                cIter++;
            }while( cIter < cRegsToConfig.end() && !cRegFound);
            cSkip = (!cRegFound);
        }
        if( cSkip ) continue;
        // LOG (INFO) << BOLDMAGENTA << "Configuring MPA#" << +pMPA->getId()%8 << " : " << cMapItem.second << RESET;
        // create a register
        ChipRegItem&                  cItem = cMPARegMap[cMapItem.second];
        std::pair<uint16_t, uint16_t> cReg;
        cReg.first  = cMapItem.first;
        cReg.second = cItem.fValue;
        cRegs.push_back(cReg);
    }
    if(cSkipLocalRegs)
        LOG(INFO) << BOLDBLUE << "Configuring MPA#" << +pMPA->getId() << " - write " << +cRegs.size() << " registers [skipping registers for individual pixels]" << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Complete configuration of MPA#" << +pMPA->getId() << " - write " << +cRegs.size() << " registers [configuring registers for individual pixels]" << RESET;
    bool cSuccess = this->WriteRegs(pMPA, cRegs, pVerifLoop);
    fTrackRegisters = true;
    return cSuccess;
}

bool MPAInterface::WriteRegs(Chip* pChip, const std::vector<std::pair<uint16_t, uint16_t>> pRegs, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    // checking if register map is empty 
    if( fMap.size() == 0 )
    {
        ChipRegMap cRegMap = pChip->getRegMap();
        // get register map
        LOG(DEBUG) << BOLDMAGENTA << "Setting up MPA maps.." << RESET;
        fMap.clear();
        for(auto& cRegItem: cRegMap) { fMap[cRegItem.second.fAddress] = cRegItem.first; }
    }


    bool cSuccess = true;
    bool cVerify  = false;
    if(!lpGBTFound())
    {
        std::vector<uint32_t> cVec;
        cVec.clear();
        for(const auto& cReg: pRegs)
        {
            auto cRegItem   = pChip->getRegItem(fMap[cReg.first]);
            cRegItem.fValue = cReg.second & 0xFF;
            fBoardFW->EncodeReg(cRegItem, pChip->getHybridId(), pChip->getId() % 8, cVec, pVerifLoop, true);
#ifdef COUNT_FLAG
            fRegisterCount++;
#endif
        }
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, cVerify);
        if(cSuccess)
        {
            for(const auto& cReg: pRegs)
            {
                auto cRegItem   = pChip->getRegItem(fMap[cReg.first]);
                cRegItem.fValue = cReg.second & 0xFF;
                pChip->setReg(fMap[cReg.first], cRegItem.fValue, cRegItem.fPrmptCfg, cRegItem.fStatusReg);
            }
        }
#ifdef COUNT_FLAG
        fTransactionCount++;
#endif
    }
    else
    {
        int cCount = 0;
        for(const auto& cReg: pRegs)
        {
            auto cRegItem   = pChip->getRegItem(fMap[cReg.first]);
            cRegItem.fValue = cReg.second & 0xFF;
            cVerify         = pVerifLoop && (cRegItem.fStatusReg == 0);
            // if(cCount % 1000 == 0) LOG(INFO) << BOLDBLUE << "Writing MPA register with address 0x" << std::hex << +cReg.first << std::dec << RESET;
            // cSuccess = flpGBTInterface->mpaWrite(flpGBT, pChip->getHybridId(), pChip->getId(), cReg.first, cReg.second, pVerifLoop);
            if(fBoardFW->WriteFERegister(pChip, cReg.first, cReg.second, cVerify)) pChip->setReg(fMap[cReg.first], cRegItem.fValue, cRegItem.fPrmptCfg, cRegItem.fStatusReg);

            if(!cSuccess) continue;
#ifdef COUNT_FLAG
            fRegisterCount++;
#endif
            cCount++;
        }
    }
    return cSuccess;
}

bool MPAInterface::WriteReg(Chip* pChip, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerifLoop)
{
    // checking if register map is empty 
    if( fMap.size() == 0 )
    {
        ChipRegMap cRegMap = pChip->getRegMap();
        // get register map
        LOG(INFO) << BOLDMAGENTA << "Setting up MPA maps.." << RESET;
        fMap.clear();
        for(auto& cRegItem: cRegMap) { fMap[cRegItem.second.fAddress] = cRegItem.first; }
    }

    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    ChipRegItem cRegItem;
    cRegItem.fAddress = pRegisterAddress; 
    if( fTrackRegisters ) UpdateModifiedRegMap(pChip,  pRegisterAddress); 
    bool cFound = pChip->getRegMap().find(fMap[pRegisterAddress]) != pChip->getRegMap().end();
    if(cFound) cRegItem = pChip->getRegItem(fMap[pRegisterAddress]);
    else cRegItem.fValue=0x66;

    LOG (DEBUG) << BOLDBLUE << "MPAInterface::WriteReg writing register with address 0x" << std::hex << pRegisterAddress << std::dec << " register value in map 0x" << std::hex << +cRegItem.fValue << std::dec << RESET;
    cRegItem.fValue = pRegisterValue & 0xFF;
    // write
    if(!lpGBTFound())
    {
        std::vector<uint32_t> cVec;
        ChipRegItem           cRegItem;
        cRegItem.fPage    = 0x00;
        cRegItem.fAddress = pRegisterAddress;
        cRegItem.fValue   = pRegisterValue & 0xFF;
        fBoardFW->EncodeReg(cRegItem, pChip->getHybridId(), pChip->getId() % 8, cVec, pVerifLoop, true);
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
        if(cSuccess ) // check is done in lpGBTInterface for opto
        {
            bool     cVerify = pVerifLoop && (cRegItem.fStatusReg == 0); 
            uint16_t cValue  = pRegisterValue; // if not verifying .. set this to what I have written to  
            if(cVerify) cValue = ReadReg(pChip, pRegisterAddress);
            cSuccess = (cVerify) ? (cValue == pRegisterValue) : true;
        }
    }
    else
    {
        bool cVerify = pVerifLoop && (cRegItem.fStatusReg == 0);
        cSuccess     = fBoardFW->WriteFERegister(pChip, pRegisterAddress, pRegisterValue, cVerify);
    }
    
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Read back value from " << fMap[pRegisterAddress] << BOLDBLUE << " at I2C address " << std::hex << cRegItem.fAddress << std::dec << " not equal to write value of "
                  << std::hex << +cRegItem.fValue << std::dec << RESET;
    }
    if(cSuccess && cFound) { 
        LOG (DEBUG) << BOLDBLUE << "\t..MPAInterface::WriteReg updating value in memory of register with address 0x" << std::hex << pRegisterAddress << std::dec << " to 0x" << std::hex << +cRegItem.fValue << std::dec << RESET;
        pChip->setReg(fMap[pRegisterAddress], cRegItem.fValue, cRegItem.fPrmptCfg, cRegItem.fStatusReg); 
    }
    return cSuccess;
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
    fBoardFW->EncodeReg(rowreg, cMPA->getHybridId(), cMPA->getId() % 8, cVecReq, false, true);
    fBoardFW->WriteChipBlockReg(cVecReq, cWriteAttempts, false);
}

uint32_t MPAInterface::Pix_read(ReadoutChip* cMPA, ChipRegItem cRegItem, uint32_t row, uint32_t pixel)
{
    uint8_t  cWriteAttempts = 0;
    uint32_t rep;

    std::vector<uint32_t> cVecReq;
    cVecReq.clear();
    fBoardFW->EncodeReg(cRegItem, cMPA->getHybridId(), cMPA->getId() % 8, cVecReq, false, false);
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
    uint16_t              curdata = 0;

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

void MPAInterface::Activate_pp(Chip* pMPA, uint8_t win) { this->WriteChipReg(pMPA, "ECM", (0x2 << 6) | win); }

void MPAInterface::Activate_ss(Chip* pMPA, uint8_t win) { this->WriteChipReg(pMPA, "ECM", (0x1 << 6) | win); }

void MPAInterface::Activate_ps(Chip* pMPA, uint8_t win) { this->WriteChipReg(pMPA, "ECM", win); }

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

    return this->WriteChipReg(pChip,"AnalogueAsync",1);
    /*
    uint32_t enwrite = 0x17;
    if(inject) enwrite = 0x53;
    this->WriteChipReg(pChip, "ENFLAGS_ALL", enwrite);
    return true;*/
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
