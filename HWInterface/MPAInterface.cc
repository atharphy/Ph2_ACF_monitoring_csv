/*
        FileName :                     MPAInterface.cc
        Content :                      User Interface to the MPAs
        Programmer :                   K. nash, M. Haranko, D. Ceresa
        Version :                      1.0
        Date of creation :             5/01/18
 */

#include "HWInterface/MPAInterface.h"
#include "Utils/ChannelGroupHandler.h"
#include "Utils/ConsoleColor.h"
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
        uint8_t cRPLSB         = fBoardFW->SingleRegisterRead(pMPA, cRegItem);
        cRegItem.fPage         = 0x00;
        cRegItem.fAddress      = 0x0801 + cChannel;
        uint8_t  cRPMSB        = fBoardFW->SingleRegisterRead(pMPA, cRegItem);
        uint16_t cCounterValue = (cRPMSB << 8) | cRPLSB;
        LOG(DEBUG) << BOLDBLUE << "Counter MSB is 0x" << std::bitset<8>(cRPMSB) << " Counter LSB is 0x" << std::bitset<8>(cRPLSB) << " Counter value is " << std::hex << +cCounterValue << std::dec
                   << RESET;
        return cCounterValue;
    }
    else if(pRegNode == "StubMode")
    {
        uint8_t cBitShift = ECM_TABLE.find("StubMode")->second;
        auto    cReg      = this->readPeri(pMPA, "ECM");
        uint8_t cRegMask  = (0x3 << cBitShift); //
        uint8_t cValue    = (cReg & cRegMask) >> cBitShift;
        return cValue;
    }
    else if(pRegNode == "LayerSwap")
    {
        uint8_t cBitShift = ECM_TABLE.find("StubMode")->second;
        auto    cReg      = this->readPeri(pMPA, "ECM");
        uint8_t cRegMask  = (0x3 << cBitShift); //
        uint8_t cValue    = (cReg & cRegMask) >> cBitShift;
        return cValue & 0x1; // 0 -- pixels as seed; 1 --> strip as seed
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

uint16_t MPAInterface::ReadReg(Chip* pChip, uint16_t pRegisterAddress, bool pVerify)
{
    setBoard(pChip->getBeBoardId());
    ChipRegItem cRegItem;
    cRegItem.fPage    = 0x00;
    cRegItem.fAddress = pRegisterAddress;
    cRegItem.fValue   = 0;
    return fBoardFW->SingleRegisterRead(pChip, cRegItem);
}
void MPAInterface::producePhaseAlignmentPattern(ReadoutChip* pChip, uint8_t pWait_ms)
{
    LOG(INFO) << GREEN << "Producing phase alignment pattern on MPA#" << +pChip->getId() << RESET;
    uint8_t                  cAlignmentPattern = 0xAA;
    std::vector<uint8_t>     cRegValues{0x2, cAlignmentPattern};
    std::vector<std::string> cRegNames{"ReadoutMode", "LFSR_data"};
    for(size_t cIndex = 0; cIndex < cRegValues.size(); cIndex++) { this->WriteChipReg(pChip, cRegNames[cIndex], cRegValues[cIndex]); } // loop over registers
}
void MPAInterface::produceWordAlignmentPattern(ReadoutChip* pChip)
{
    LOG(INFO) << GREEN << "Producing word alignment pattern on MPA#" << +pChip->getId() << RESET;
    std::vector<uint8_t>     cRegValues{0x2, fWordAlignmentPatterns[0]};
    std::vector<std::string> cRegNames{"ReadoutMode", "LFSR_data"};
    for(size_t cIndex = 0; cIndex < cRegValues.size(); cIndex++) { this->WriteChipReg(pChip, cRegNames[cIndex], cRegValues[cIndex]); } // loop over registers
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
    int                  cStartValue = -7;
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
std::vector<uint8_t> MPAInterface::readLUT(ReadoutChip* pChip, uint8_t pMode)
{
    std::vector<uint8_t> cBendCodes(0); // bend registers are 0 -- 14. Each register encodes 2 codes

    float                    cStartValue = -7.0 / 2.;
    std::vector<std::string> cRegNames{"CodeM76", "CodeM54", "CodeM32", "CodeM10", "CodeP12", "CodeP34", "CodeP56", "CodeP78"};
    for(size_t cIndex = 0; cIndex < 8; cIndex++) // 9 registers
    {
        uint16_t cRegAddress = 6 + cIndex; // each register controls two codes
        // code starts from dummy
        // cRegAddress = this->regPeri(pChip, cRegAddress);
        std::vector<float> cTheseBends{cStartValue, (float)(cStartValue + 0.5)};
        uint8_t            cCode = (pMode == 0) ? ReadChipReg(pChip, cRegNames[cIndex]) : pChip->getReg(cRegNames[cIndex]); // MPAInterface::ReadReg(pChip, cRegAddress);
        std::stringstream  cOut;
        cOut << "Reading bend code register 0x" << std::hex << +cRegAddress << std::dec << " this contains the bends for  ";
        if(cTheseBends[0] == -9)
            cOut << " dummy code and ";
        else
            cOut << +cTheseBends[0] << " and ";
        cOut << +cTheseBends[1] << " half-strips the register value is 0x" << std::hex << +cCode << std::dec << RESET;
        LOG(DEBUG) << BOLDMAGENTA << cOut.str() << RESET;
        for(int cNibble = 0; cNibble < 2; cNibble++)
        {
            float             cBendHalfStrips = cStartValue + cNibble * 0.5;
            int               cBitOffset      = (1 - cNibble) * 3;
            uint8_t           cBendCode       = (cCode & (0x7 << cBitOffset)) >> cBitOffset;
            std::stringstream cPrint;
            cPrint << "BendCode for a bend of ";
            if(cBendHalfStrips == -9)
                continue; // cPrint << " [dummy] is ";
            else
                cPrint << cBendHalfStrips << " is ";
            cPrint << std::bitset<3>(cBendCode);
            LOG(DEBUG) << BOLDBLUE << "\t\t" << cPrint.str() << RESET;
            cBendCodes.push_back(cBendCode);
        }
        cStartValue = cStartValue + 2 * 0.5;
    }
    return cBendCodes;
}
bool MPAInterface::configPixel(Chip* pChip, std::string cReg, int pPixelNum, uint8_t pValue, bool pVerify)
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

    pVerify = (cRow == 0 || cColumn == 0) ? false : pVerify;
    return MPAInterface::WriteReg(pChip, cAddress, pValue, pVerify);
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
bool MPAInterface::maskPixel(Chip* pChip, int pPixelNum, uint8_t pMask, bool pVerify)
{
    // pixel num starts from 1 [0 == global]
    auto    cRegValue = this->readPixel(pChip, "ENFLAGS", pPixelNum);
    uint8_t cNewValue = (cRegValue & 0xFE) | (1 - pMask);
    LOG(DEBUG) << BOLDBLUE << "Setting pixel mask to 0x" << std::hex << +cNewValue << std::dec << " on PixelNum#" << pPixelNum << RESET;
    return this->configPixel(pChip, "ENFLAGS", pPixelNum, cNewValue, pVerify);
}
bool MPAInterface::maskRowCol(Chip* pChip, int pRow, int pColumn, uint8_t pMask, bool pVerify)
{
    // row and col num starts from 1 [0 == global]
    int cPixNum = 1 + (pColumn - 1) * NSSACHANNELS + (pRow - 1); // starting from 1
    return this->maskPixel(pChip, cPixNum, pMask, pVerify);
}
bool MPAInterface::configRow(Chip* pChip, std::string cReg, int pRow, uint8_t pValue, bool pVerify)
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
    pVerify           = (pRow == 0) ? false : pVerify;
    uint16_t cAddress = this->regRow(pChip, cRegAddress, pRow);
    LOG(DEBUG) << BOLDBLUE << "\t... register address 0x" << std::hex << +cAddress << std::dec << RESET;
    return MPAInterface::WriteReg(pChip, cAddress, pValue, pVerify);
}
bool MPAInterface::configPeri(Chip* pChip, std::string cReg, uint8_t pValue, bool pVerify)
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
    return MPAInterface::WriteReg(pChip, cAddress, pValue, pVerify);
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

// two methods -- also kinda slow but cant really use broadcast commands with enflags without overwriting other bits
// Would want  to do a rowwise broadcast -- maybe using col 0  for bit mask?
bool MPAInterface::maskChannelGroup(ReadoutChip* cChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerify)
{
    auto cOriginalMask = std::static_pointer_cast<const ChannelGroup<NMPAROWS, NSSACHANNELS>>(cChip->getChipOriginalMask());
    auto groupToMask   = std::static_pointer_cast<const ChannelGroup<NMPAROWS, NSSACHANNELS>>(group);

    auto cBitset = std::bitset<NSSACHANNELS * NMPAROWS>(groupToMask->getBitset() & cOriginalMask->getBitset());
    // cBitset = cBitset&std::bitset<NSSACHANNELS*NMPAROWS>(0x0000F0FF0);
    LOG(DEBUG) << BOLDBLUE << "\t... Applying mask to MPA" << +cChip->getId() << " with " << group->getNumberOfEnabledChannels() << " desired mask \t... : " << cBitset
               << " original mask  \t... : " << cOriginalMask << " enabled channels "
               << " original bitset was be \t... " << groupToMask->getBitset() << RESET;

    // std::vector<std::pair<std::string, uint16_t>> pVecReq;
    // pVecReq.clear();
    bool returnval = true;
    for(uint32_t ipix = 0; ipix < (NSSACHANNELS * NMPAROWS); ipix++)
    {
        auto shifted = std::bitset<NSSACHANNELS * NMPAROWS>(0x1) << ipix;
        bool bitval  = bool(((cBitset & shifted) >> ipix).to_ulong());

        // if (not bitval)LOG(INFO) << BOLDBLUE << "MASKED " <<ipix<< RESET;
        // usually this function performs a complete reconfig but here it just masks.  See change to ConfigureChipOriginalMask below
        if(bitval) continue;

        returnval &= maskPixel(cChip, ipix + 1, (1 - bitval), pVerify); // I  think mask 0 is enable?
    }
    // return this->WriteChipMultReg(cChip, pVecReq);
    return returnval;
}

bool MPAInterface::setInjectionSchema(ReadoutChip* cChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerify)
{
    std::bitset<NSSACHANNELS* NMPAROWS> cBitset = std::bitset<NSSACHANNELS * NMPAROWS>(std::static_pointer_cast<const ChannelGroup<NMPAROWS, NSSACHANNELS>>(group)->getBitset());
    if(cBitset.count() == 0) // no mask set... so do nothing
        return true;

    auto cOriginalMask = std::static_pointer_cast<const ChannelGroup<NMPAROWS, NSSACHANNELS>>(cChip->getChipOriginalMask());
    auto groupToMask   = std::static_pointer_cast<const ChannelGroup<NMPAROWS, NSSACHANNELS>>(group);

    // cBitset=cBitset&std::bitset<NSSACHANNELS * NMPAROWS>(0xF0FF0);

    LOG(DEBUG) << BOLDBLUE << "\t... Applying injection to MPA" << +cChip->getId() << " with " << group->getNumberOfEnabledChannels() << " desired mask \t... : " << cBitset
               << " original mask  \t... : " << cOriginalMask << " enabled channels "
               << " original bitset was be \t... " << groupToMask->getBitset() << RESET;

    bool returnval = true;
    for(uint32_t ipix = 0; ipix < (NSSACHANNELS * NMPAROWS); ipix++)
    {
        auto shifted = std::bitset<NSSACHANNELS * NMPAROWS>(0x1) << ipix;
        bool bitval  = bool(((cBitset & shifted) >> ipix).to_ulong());

        returnval &= enablePixelInjection(cChip, ipix + 1, bitval, pVerify);
    }

    return returnval;
}
bool MPAInterface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerify)
{
    bool success = true;
    if(mask) success &= maskChannelGroup(pChip, group, pVerify);
    if(inject) success &= setInjectionSchema(pChip, group, pVerify);

    return success;
}

bool MPAInterface::enablePixelInjection(Chip* pChip, int pPixelNum, uint8_t pInj, bool pVerify)
{
    auto    cRegValue = this->readPixel(pChip, "PixelEnable", pPixelNum);
    uint8_t cNewValue = (cRegValue & 0xBF) | (pInj << 6);

    LOG(DEBUG) << BOLDBLUE << "Setting Enable to 0x" << std::hex << +cNewValue << std::dec << RESET;
    return this->configPixel(pChip, "PixelEnable", pPixelNum, cNewValue, pVerify);
}

bool MPAInterface::ConfigureChipOriginalMask(ReadoutChip* pMPA, bool pVerify, uint32_t pBlockSize)
{
    // use pix 1 as a proxy. Better to save this as a constant

    auto pixval = readPixel(pMPA, "ENFLAGS", 1);
    // write broadcast then mask is much much faster than full config
    configPixel(pMPA, "ENFLAGS", 0, pixval);

    auto allChannelEnabledGroup = std::make_shared<ChannelGroup<NMPAROWS, NSSACHANNELS>>();
    return maskChannelGroup(pMPA, allChannelEnabledGroup, pVerify);
}
bool MPAInterface::WriteChipReg(Chip* pMPA, const std::string& pRegName, uint16_t pValue, bool pVerify)
{
    setBoard(pMPA->getBeBoardId());

    // LOG(INFO) << BOLDRED << "glorpMPA! " << RESET;

    LOG(DEBUG) << BOLDMAGENTA << " MPAInterface::WriteChipReg writing to " << pRegName << RESET;

    // need to or success
    if(pRegName.find("ThDAC_ALL") != std::string::npos || pRegName.find("Threshold") != std::string::npos)
    {
        LOG(DEBUG) << BOLDMAGENTA << "Setting threshold on MPA#" << +pMPA->getId() << " to " << pValue << RESET;
        this->Set_threshold(pMPA, pValue);
        return true;
    }
    else if(pRegName == "Offsets")
    {
        return this->WriteChipSingleReg(pMPA, "TrimDAC_ALL", pValue, false);
    }
    else if(pRegName == "EnablePhaseAlignmentPattern")
    {
        this->producePhaseAlignmentPattern(static_cast<ReadoutChip*>(pMPA), pValue);
        return true;
    }
    else if(pRegName.find("SelectEdgeT1") != std::string::npos)
    {
        std::string cRegName  = "EdgeSelT1Raw";
        uint8_t     cBitShift = 1;
        auto        cRegValue = this->ReadChipReg(pMPA, cRegName);
        uint8_t     cRegMask  = (0x1 << cBitShift); //
        cRegMask              = ~(cRegMask);
        uint8_t cValue        = (cRegValue & cRegMask) | (pValue << cBitShift);
        // LOG(INFO) << BOLDMAGENTA << "Setting EdgeSel register for T1 to 0x" << std::hex << +cValue << std::dec << RESET;
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
        // LOG(INFO) << BOLDMAGENTA << "Setting EdgeSel register for Line" << +cLineId << " to 0x" << std::hex << +cValue << std::dec << RESET;
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
        return MPAInterface::WriteReg(pMPA, cRegAddress, cValue, pVerify);
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
        // register mask
        std::vector<std::string> cPixelRegs{"PixelMask", "DigitalInjection"};
        uint8_t                  cFEEnable   = 0;
        uint8_t                  cEnableDigi = (pValue == 0) ? 0 : 1;
        std::vector<uint8_t>     cPixelVals{cFEEnable, cEnableDigi};
        uint8_t                  cRegMask = 0x00;
        for(auto cPixelReg: cPixelRegs) cRegMask = cRegMask | (1 << PIXEL_ENABLE_TABLE.find(cPixelReg)->second);
        cRegMask       = ~(cRegMask);
        uint8_t cValue = 0x00;
        size_t  cIndx  = 0;
        for(auto cVal: cPixelVals)
        {
            cValue = cValue | (cVal << PIXEL_ENABLE_TABLE.find(cPixelRegs[cIndx])->second);
            cIndx++;
        }
        int     cPixelNumber = 0;
        uint8_t cReadValue   = 0x00;
        if(pRegName.find("P") != std::string::npos) // single pixel
        {
            cPixelNumber = std::stoi(pRegName.substr(pRegName.find("P") + 1, pRegName.length()));
            cReadValue   = this->readPixel(pMPA, "ENFLAGS", cPixelNumber);
        }
        else
        {
            cReadValue = this->ReadChipReg(pMPA, "ENFLAGS_ALL");
        }
        auto cRegValue = (cReadValue & cRegMask) | cValue;
        LOG(INFO) << BOLDBLUE << "Register mask " << pRegName << " 0x" << std::hex << +cRegMask << std::dec << " readback value is 0x" << std::hex << +cReadValue << std::dec << " will write value 0x"
                  << std::hex << +cValue << std::dec << " register value is 0x" << std::hex << +cRegValue << std::dec << RESET;

        bool cEnableDigital = this->configPixel(pMPA, "ENFLAGS", cPixelNumber, cRegValue, pVerify);
        // configure pattern
        bool cConfigPattern = this->configPixel(pMPA, "DigiPattern", cPixelNumber, pValue, pVerify);
        return cReadoutMode && cEnableDigital && cConfigPattern;
    }
    else if(pRegName == "AnalogueAsync")
    {
        // readout mode 1 -- ASYNC counter
        ChipRegMask cMask;
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("AnalogueInjection")->second;
        cMask.fNbits    = 1;
        pMPA->setRegBits("ENFLAGS_ALL", cMask, pValue);
        // hit counter- enable
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("CounterEnable")->second;
        cMask.fNbits    = 1;
        pMPA->setRegBits("ENFLAGS_ALL", cMask, pValue);
        // edge BR - enable
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("EnEdgeBR")->second;
        cMask.fNbits    = 1;
        pMPA->setRegBits("ENFLAGS_ALL", cMask, pValue);
        // un-mask all pixels  - pixel enable
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("PixelMask")->second;
        cMask.fNbits    = 1;
        pMPA->setRegBits("ENFLAGS_ALL", cMask, pValue);
        //
        if(pValue == 1)
            LOG(DEBUG) << BOLDBLUE << "Enabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        else
            LOG(DEBUG) << BOLDBLUE << "Disabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        bool cEnableAnalogue = this->configPixel(pMPA, "ENFLAGS", 0, pMPA->getRegItem("ENFLAGS_ALL").fValue, pVerify);
        // enabling async readout for stubs
        bool cReadoutMode = true;
        if(pValue == 1)
        {
            cReadoutMode = configPeri(pMPA, "ReadoutMode", 0x01);
            LOG(DEBUG) << BOLDBLUE << "Enabling readout of I2C counters on MPA by setting register ReadoutMode to 0x" << std::hex << +pValue << std::dec << RESET;
        }
        else
        {
            cReadoutMode = configPeri(pMPA, "ReadoutMode", 0x00);
            LOG(DEBUG) << BOLDBLUE << "Disabling readout of I2C counters on MPA by setting register ReadoutMode to 0x" << std::hex << +pValue << std::dec << RESET;
        }
        return cEnableAnalogue && cReadoutMode;
    }
    else if(pRegName == "AnalogueSync")
    {
        // readout mode 1 -- ASYNC counter
        ChipRegMask cMask;
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("AnalogueInjection")->second;
        cMask.fNbits    = 1;
        pMPA->setRegBits("ENFLAGS_ALL", cMask, pValue);
        if(pValue == 1)
            LOG(DEBUG) << BOLDBLUE << "Enabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        else
            LOG(DEBUG) << BOLDBLUE << "Disabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        bool cEnableAnalogue = this->configPixel(pMPA, "ENFLAGS", 0, pMPA->getRegItem("ENFLAGS_ALL").fValue, pVerify);
        // enabling async readout for stubs
        bool cReadoutMode = configPeri(pMPA, "ReadoutMode", 0x00);
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
        LOG(DEBUG) << BOLDBLUE << "Setting "
                   << " bias calDac to " << +pValue << " on MPA" << +pMPA->getId() << RESET;

        Set_calibration(pMPA, pValue);

        return true;
    }
    else
    {
        LOG(DEBUG) << BOLDMAGENTA << "Writing " << +pValue << " to " << pRegName << RESET;
        return this->WriteChipSingleReg(pMPA, pRegName, pValue, pVerify);
    }
}

bool MPAInterface::WriteChipSingleReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerify)
{
    setBoard(pChip->getBeBoardId());
    auto cRegItem   = pChip->getRegMap().find(pRegNode)->second;
    cRegItem.fValue = pValue & 0xFF;
    return fBoardFW->SingleRegisterWrite(pChip, cRegItem, pVerify);
}

bool MPAInterface::WriteChipMultReg(Chip* pMPA, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerify)
{
    // first, identify the correct BeBoardFWInterface
    setBoard(pMPA->getBeBoardId());
    auto                     cRegMap = pMPA->getRegMap();
    std::vector<ChipRegItem> cRegItems;
    for(auto cReq: pVecReq)
    {
        auto cIterator = cRegMap.find(cReq.first);
        if(cIterator == cRegMap.end())
        {
            LOG(ERROR) << BOLDRED << "MPAInterface::WriteChipMultReg trtying to write to a register that doesn't exist in the map : " << cReq.first << RESET;
            continue;
        }

        ChipRegItem cItem = cIterator->second;
        cItem.fValue      = cReq.second;
        cRegItems.push_back(cItem);
    }
    return fBoardFW->MultiRegisterWrite(pMPA, cRegItems, pVerify);
}

bool MPAInterface::WriteChipAllLocalReg(ReadoutChip* pMPA, const std::string& dacName, const ChipContainer& localRegValues, bool pVerify)
{
    setBoard(pMPA->getBeBoardId());
    assert(localRegValues.size() == pMPA->getNumberOfChannels());
    std::string dacTemplate;

    if(dacName == "TrimDAC_C" or dacName == "ThresholdTrim") { dacTemplate = "TrimDAC_C%d_R%d"; }

    else
        LOG(ERROR) << "Error, DAC " << dacName << " is not a Local DAC";

    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    ChannelGroup<NMPAROWS, NSSACHANNELS>          channelToEnable;
    std::vector<uint32_t>                         cVec;
    cVec.clear();
    bool cSuccess = true;

    // check if all registers are the same
    std::vector<uint8_t> cVals(0);
    for(uint16_t row = 0; row < pMPA->getNumberOfRows(); ++row)
    {
        for(uint16_t col = 0; col < pMPA->getNumberOfCols(); ++col)
        {
            cVals.push_back(localRegValues.getChannel<uint16_t>(row, col));
            LOG(DEBUG) << BOLDMAGENTA << +cVals[cVals.size() - 1] << RESET;
        }
    }

    if(std::adjacent_find(cVals.begin(), cVals.end(), std::not_equal_to<uint16_t>()) == cVals.end())
    {
        LOG(DEBUG) << BOLDBLUE << "All elements of " << dacName << " are equal to one  another .. will use global register" << RESET;
        if(dacName == "TrimDAC_C" or dacName == "ThresholdTrim")
        {
            bool cWrite = this->WriteChipReg(pMPA, "TrimDAC_ALL", cVals[0], false);
            if(pVerify)
            {
                auto cReadback = this->ReadChipReg(pMPA, "TrimDAC_C10_R10");
                LOG(DEBUG) << BOLDMAGENTA << "Read-back a value of " << +cReadback << " from trim-dac register" << RESET;
                return (cReadback == cVals[0]);
            }
            else
                return cWrite;
        }
        // to-add .. add the rest
    }

    LOG(DEBUG) << BOLDBLUE << "Different values for " << dacName << " ... will NOT use global register" << RESET;

    for(uint16_t row = 0; row < pMPA->getNumberOfRows(); ++row)
    {
        for(uint16_t col = 0; col < pMPA->getNumberOfCols(); ++col)
        {
            char dacName1[20];
            sprintf(dacName1, dacTemplate.c_str(), col, row);
            LOG(DEBUG) << BOLDBLUE << "Setting register " << dacName1 << " to " << (localRegValues.getChannel<uint16_t>(row, col) & 0x1F) << RESET;
            cSuccess = cSuccess && this->WriteChipReg(pMPA, dacName1, (localRegValues.getChannel<uint16_t>(row, col) & 0x1F), pVerify);
        }
    }
    return cSuccess;
}

bool MPAInterface::ConfigureChip(Chip* pMPA, bool pVerify, uint32_t pBlockSize)
{
    // for now ..
    bool              cConfigLocalRegs = true;
    std::stringstream cOutput;
    setBoard(pMPA->getBeBoardId());
    pMPA->printChipType(cOutput);
    LOG(INFO) << BOLDBLUE << cOutput.str() << "...Configuring chip with Id[" << +pMPA->getId() << "]" << RESET;
    pMPA->setRegisterTracking(0);

    std::vector<uint32_t> cVec;
    ChipRegMap            cRegMap = pMPA->getRegMap();
    // need to split between control and enable registers
    // don't read back enable registers
    std::vector<ChipRegItem> cCntrlRegItems;
    std::vector<ChipRegItem> cRegItems;
    std::vector<ChipRegItem> cLocalRegItems;
    cCntrlRegItems.clear();
    for(auto cMapItem: cRegMap)
    {
        if(cMapItem.second.fControlReg)
            cCntrlRegItems.push_back(cMapItem.second);
        else if((cMapItem.first.find("_P") != std::string::npos))
            cLocalRegItems.push_back(cMapItem.second);
        else
            cRegItems.push_back(cMapItem.second);
    }
    // cntrl
    bool cSuccess = fBoardFW->MultiRegisterWrite(pMPA, cCntrlRegItems, false);
    if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cCntrlRegItems.size() << " control registers in" << cOutput.str() << "#" << +pMPA->getId() << RESET;
    // glbl
    cSuccess = fBoardFW->MultiRegisterWrite(pMPA, cRegItems, pVerify);
    if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cRegItems.size() << " R/W registers in" << cOutput.str() << "#" << +pMPA->getId() << RESET;
    // lcl
    if(cConfigLocalRegs)
    {
        cSuccess = fBoardFW->MultiRegisterWrite(pMPA, cLocalRegItems, pVerify);
        if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cLocalRegItems.size() << " local R/W registers in" << cOutput.str() << "#" << +pMPA->getId() << RESET;
    }
    pMPA->setRegisterTracking(1);
    return cSuccess;
}

bool MPAInterface::WriteRegs(Chip* pChip, const std::vector<std::pair<uint16_t, uint16_t>> pRegs, bool pVerify)
{
    setBoard(pChip->getBeBoardId());
    std::vector<ChipRegItem> cRegItems;
    auto                     cRegMap = pChip->getRegMap();
    for(auto cReq: pRegs)
    {
        auto cIterator = find_if(cRegMap.begin(), cRegMap.end(), [&cReq](const ChipRegPair& obj) { return obj.second.fAddress == cReq.first; });
        if(cIterator == cRegMap.end())
        {
            LOG(ERROR) << BOLDRED << "MPAInterface::WriteChipMultReg trtying to write to a register that doesn't exist in the map : " << cReq.first << RESET;
            continue;
        }

        ChipRegItem cItem = cIterator->second;
        cItem.fValue      = cReq.second;
        cRegItems.push_back(cItem);
    }
    return fBoardFW->MultiRegisterWrite(pChip, cRegItems, pVerify);
}

bool MPAInterface::WriteReg(Chip* pChip, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerify)
{
    setBoard(pChip->getBeBoardId());
    auto        cRegMap   = pChip->getRegMap();
    auto        cIterator = find_if(cRegMap.begin(), cRegMap.end(), [&pRegisterAddress](const ChipRegPair& obj) { return obj.second.fAddress == pRegisterAddress; });
    ChipRegItem cItem     = cIterator->second;
    cItem.fValue          = pRegisterValue;
    return fBoardFW->SingleRegisterWrite(pChip, cItem, pVerify);
}
void MPAInterface::readAllBias(Chip* pChip)
{
    std::vector<std::string> nameDAC{"A", "B", "C", "D", "E", "ThDAC", "CalDAC"};
    for(int ipoint = 0; ipoint < 5; ipoint++)
    {
        for(int iblock = 0; iblock < 7; iblock++)
        {
            std::string DAC    = nameDAC[ipoint] + std::to_string(iblock);
            auto        cValue = ReadChipReg(pChip, DAC);
            LOG(INFO) << BOLDBLUE << DAC << ": bias:" << cValue << " on MPA" << +pChip->getId() << RESET;
        }
    }
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

bool MPAInterface::enableInjection(ReadoutChip* pChip, bool inject, bool pVerify)
{
    setBoard(pChip->getBeBoardId());
    // if sync

    // uint32_t enwrite=1;
    // if(inject) enwrite=17;

    // return this->WriteChipReg(pChip, "AnalogueAsync", 1);
    uint32_t enwrite = 0x17;
    if(inject) enwrite = 0x53;
    return this->WriteChipReg(pChip, "ENFLAGS_ALL", enwrite);
}

} // namespace Ph2_HwInterface
