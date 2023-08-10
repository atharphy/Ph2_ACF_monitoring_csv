/*!

        \file                   MPA2.h
        \brief                  MPA2 Description class, config of the MPA2s
        \author                 Kevin Nash
        \version                1.0
        \date                   12/06/21
        Support :               mail to : knash201@gmail.com

 */

#include "HWInterface/MPA2Interface.h"
#include "Utils/ChannelGroupHandler.h"
#include "Utils/ConsoleColor.h"
#include <typeinfo>

#define DEV_FLAG 0

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
MPA2Interface::MPA2Interface(const BeBoardFWMap& pBoardMap) : ReadoutChipInterface(pBoardMap) {}
MPA2Interface::~MPA2Interface() {}

uint16_t MPA2Interface::ReadChipReg(Chip* pMPA2, const std::string& pRegNode)
{
    setBoard(pMPA2->getBeBoardId());
    std::vector<uint32_t> cVecReq;
    ChipRegItem           cRegItem;
    /*if(pRegNode.find("CounterStrip") != std::string::npos) is this from SSA?
    {
        int cChannel = 0;
        sscanf(pRegNode.c_str(), "CounterStrip%d", &cChannel);
        cRegItem.fPage         = 0x00;
        cRegItem.fAddress      = 0x0901 + cChannel;
        cRegItem.fValue        = 0;
        uint8_t cRPLSB         = this->ReadReg(pMPA2, cRegItem.fAddress) & 0xFF;
        cRegItem.fPage         = 0x00;
        cRegItem.fAddress      = 0x0801 + cChannel;
        uint8_t  cRPMSB        = this->ReadReg(pMPA2, cRegItem.fAddress) & 0xFF;
        uint16_t cCounterValue = (cRPMSB << 8) | cRPLSB;
        LOG(DEBUG) << BOLDBLUE << "Counter MSB is 0x" << std::bitset<8>(cRPMSB) << " Counter LSB is 0x" << std::bitset<8>(cRPLSB) << " Counter value is " << std::hex << +cCounterValue << std::dec
                   << RESET;
        return cCounterValue;
    }*/
    if(pRegNode == "StubMode") // should work with MPA2 address table
    {
        uint8_t cBitShift = ECM_TABLE.find("StubMode")->second;
        auto    cReg      = this->readPeri(pMPA2, "ECM");
        uint8_t cRegMask  = (0x3 << cBitShift); //
        uint8_t cValue    = (cReg & cRegMask) >> cBitShift;
        return cValue;
    }
    else if(pRegNode == "StubWindow")
    {
        uint8_t cBitShift = ECM_TABLE.find("StubWindow")->second;
        auto    cReg      = this->readPeri(pMPA2, "ECM");
        uint8_t cRegMask  = (0x3F << cBitShift);
        uint8_t cValue    = (cReg & cRegMask) >> cBitShift;
        return cValue;
    }
    else if(pRegNode == "vref")
    {
        cRegItem = pMPA2->getRegItem("ADCcontrol");
        return this->ReadReg(pMPA2, cRegItem.fAddress) & 0xFF;
    }
    else if(pRegNode == "ReadoutMode") // New decoding control reg for MPA2
    {
        uint8_t cBitShift = CONTROL_TABLE.find("ReadoutMode")->second;
        auto    cReg      = this->readPeri(pMPA2, "Control_1");
        uint8_t cRegMask  = (0x3 << cBitShift);
        uint8_t cValue    = (cReg & cRegMask) >> cBitShift;
        return cValue;
    }

    else if(pRegNode == "RetimePix") // New decoding control reg for MPA2
    {
        uint8_t cBitShift = CONTROL_TABLE.find("RetimePix")->second;
        auto    cReg      = this->readPeri(pMPA2, "Control_1");
        uint8_t cRegMask  = (0x7 << cBitShift); // to check
        uint8_t cValue    = (cReg & cRegMask) >> cBitShift;
        return cValue;
    }

    else if(pRegNode == "PhaseShift") // New decoding control reg for MPA2
    {
        uint8_t cBitShift = CONTROL_TABLE.find("PhaseShift")->second;
        auto    cReg      = this->readPeri(pMPA2, "Control_1");
        uint8_t cRegMask  = (0x7 << cBitShift);
        uint8_t cValue    = (cReg & cRegMask) >> cBitShift;
        return cValue;
    }

    else if(pRegNode == "LayerSwap") // should work with MPA2 address table
    {
        uint8_t cBitShift = ECM_TABLE.find("StubMode")->second;
        auto    cReg      = this->readPeri(pMPA2, "ECM");
        uint8_t cRegMask  = (0x3 << cBitShift); //
        uint8_t cValue    = (cReg & cRegMask) >> cBitShift;
        return cValue & 0x1; // 0 -- pixels as seed; 1 --> strip as seed
    }
    else if(pRegNode.find("BendCode") != std::string::npos) // should work with MPA2 address  table
    {
        std::string cSubStr  = pRegNode.substr(pRegNode.find("BendCode") + std::string("BendCode").length(), pRegNode.length());
        std::string cPattern = "P";
        if(pRegNode.find("P") == std::string::npos) { cPattern = "M"; }
        int      cBendHalfStrips = std::stoi(cSubStr.substr(cSubStr.find(cPattern) + 1, cSubStr.length()));
        int      cIndex          = (cBendHalfStrips + 9) / 2;
        int      cNibble         = (cBendHalfStrips + 9) % 2;
        uint8_t  cBitShift       = 3 * (1 - cNibble);
        uint8_t  cRegMask        = (0x7 << cBitShift); //
        uint16_t cRegAddress     = this->regPeri(pMPA2, 0x11, 5 + cIndex);
        uint16_t cRegValue       = MPA2Interface::ReadReg(pMPA2, cRegAddress);
        uint8_t  cValue          = (cRegValue & cRegMask) >> cBitShift;
        return cValue;
    }
    else if(PERI_CONFIG_TABLE.find(pRegNode) != PERI_CONFIG_TABLE.end())
    // else if(pRegNode == "ErrorL1")
    {
        return this->readPeri(pMPA2, pRegNode);
    }
    else if(pRegNode == "Threshold")
    {
        return this->ReadChipReg(pMPA2, "ThDAC0");
    }
    else if(pRegNode == "InjectedCharge")
    {
        return this->ReadChipReg(pMPA2, "CalDAC0");
    }
    else if(pRegNode == "ADC_output")
    {
        // std::cout<<(this->ReadChipReg(pMPA2, "ADC_output_LSB")&0xFF)<<","<<((this->ReadChipReg(pMPA2, "ADC_output_MSB")&0xF)<<8)<<std::endl;
        // std::cout<<(this->ReadChipReg(pMPA2, "ADC_output_LSB")&0xFF)+((this->ReadChipReg(pMPA2, "ADC_output_MSB")&0xF)<<8)<<std::endl;
        return (this->ReadChipReg(pMPA2, "ADC_output_LSB") & 0xFF) + ((this->ReadChipReg(pMPA2, "ADC_output_MSB") & 0xF) << 8);
    }
    else if(pRegNode == "TriggerLatency")
    {
        uint8_t cLatencyReg1 = pMPA2->getRegItem("MemoryControl_1_ALL").fValue;
        uint8_t cLatencyReg2 = (pMPA2->getRegItem("MemoryControl_2_ALL").fValue) & (0x1); // first bit is latency for MPA2
        return (cLatencyReg2 << 8) | cLatencyReg1;
    }
    else if(pRegNode == "PixelControl_ALL" || pRegNode == "PixelControl")
    {
        //LORE IRENE -- The pixel control register is written for all pixels (broacast) and must be read for the individual rows
        // in this case we are hardcoding row 1 and reading it back to prove that we actually set the register.
        //This read gives a warning saying that the register for that row is not in the list of registers MPA2.txt
        //We should add them (see DigPattern_P) with names like PixelControl_P.
        // The register is however read correctly because the address exist on the chip.
        cRegItem = pMPA2->getRegItem("PixelControl_ALL");
        uint16_t row = 1;
        cRegItem.fAddress = cRegItem.fAddress + ((row & 0x1F) << 11);
        return this->ReadReg(pMPA2, cRegItem.fAddress) & 0xFF;
    }
    else if(pRegNode == "ENFLAGS_ALL")
    {
        cRegItem = pMPA2->getRegItem("ENFLAGS_P1");
        uint16_t row = 1;
        cRegItem.fAddress = cRegItem.fAddress + ((row & 0x1F) << 11);
        return this->ReadReg(pMPA2, cRegItem.fAddress) & 0xFF;
    }
    else
    {
        LOG(DEBUG) << BOLDYELLOW << "Reading back register: " << pRegNode << RESET;
        cRegItem = pMPA2->getRegItem(pRegNode);
        return this->ReadReg(pMPA2, cRegItem.fAddress) & 0xFF;
    }
}

// Unchanged from MPA1 -- to check
uint16_t MPA2Interface::ReadReg(Chip* pChip, uint16_t pRegisterAddress, bool pVerify)
{
    setBoard(pChip->getBeBoardId());
    ChipRegItem cRegItem;
    cRegItem.fPage    = 0x00;
    cRegItem.fAddress = pRegisterAddress;
    cRegItem.fValue   = 0;
    return fBoardFW->SingleRegisterRead(pChip, cRegItem);
}

// Should work for MPA2 with control decoding
void MPA2Interface::producePhaseAlignmentPattern(ReadoutChip* pChip, uint8_t pWait_ms)
{
    LOG(INFO) << GREEN << "Producing phase alignment pattern on MPA#" << +pChip->getId() << RESET;
    uint8_t                  cAlignmentPattern = 0xAA;
    std::vector<uint8_t>     cRegValues{0x2, cAlignmentPattern};
    std::vector<std::string> cRegNames{"ReadoutMode", "LFSR_data"};
    for(size_t cIndex = 0; cIndex < cRegValues.size(); cIndex++) { this->WriteChipReg(pChip, cRegNames[cIndex], cRegValues[cIndex]); } // loop over registers
}
void MPA2Interface::produceWordAlignmentPattern(ReadoutChip* pChip)
{
    LOG(INFO) << GREEN << "Producing word alignment pattern on MPA#" << +pChip->getId() << RESET;
    std::vector<uint8_t>     cRegValues{0x2, fWordAlignmentPatterns[0]};
    std::vector<std::string> cRegNames{"ReadoutMode", "LFSR_data"};
    for(size_t cIndex = 0; cIndex < cRegValues.size(); cIndex++) { this->WriteChipReg(pChip, cRegNames[cIndex], cRegValues[cIndex]); } // loop over registers
}

void MPA2Interface::digiInjection(ReadoutChip* pChip, std::vector<Injection> pInjections, uint8_t pPattern)
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
    LOG(INFO) << BOLDMAGENTA << "DigitalSync DONE" << RESET;
}
std::vector<int> MPA2Interface::decodeBendCode(ReadoutChip* pChip, uint8_t pBendCode)
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
std::vector<uint8_t> MPA2Interface::readLUT(ReadoutChip* pChip, uint8_t pMode) // Only changed to CodeDM8 reg address
{
    std::vector<uint8_t> cBendCodes(0);

    float cStartValue = -7.0 / 2.;
    for(size_t cIndex = 0; cIndex < 8; cIndex++) // 9 registers
    {
        uint16_t cRegAddress = 3 + cIndex; // CodeDM8 at reg  3, but MPA1 is at 5 yet cRegAddress starts at 6 -- to check (ie this might be 4 + cIndex)
        // code starts from dummy
        cRegAddress = this->regPeri(pChip, 0x11, cRegAddress);
        std::vector<float> cTheseBends{cStartValue, (float)(cStartValue + 0.5)};
        uint8_t            cCode = MPA2Interface::ReadReg(pChip, cRegAddress);
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
                continue;
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
bool MPA2Interface::configPixel(Chip* pChip, std::string cReg, int pPixelNum, uint8_t pValue, bool pVerify)
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
    pVerify = (cRow == 0 || cColumn == 0) ? false : pVerify;
    return MPA2Interface::WriteReg(pChip, cAddress, pValue, pVerify);
}
uint16_t MPA2Interface::readPixel(Chip* pChip, std::string cReg, int pPixelNum)
{
    int      cPixNum     = pPixelNum - 1;
    uint32_t cRow        = (pPixelNum == 0) ? 0 : 1 + cPixNum / NSSACHANNELS;
    uint32_t cColumn     = (pPixelNum == 0) ? 0 : 1 + cPixNum % NSSACHANNELS;
    uint8_t  cRegAddress = PIXEL_CONFIG_TABLE.find(cReg)->second;
    uint16_t cAddress    = this->regPixel(pChip, cRegAddress, cRow, cColumn);
    LOG(DEBUG) << BOLDBLUE << "PXL#" << +pPixelNum << " register is row " << +cRow << " column " << +cColumn << " register 0x" << std::hex << cAddress << std::dec << RESET;
    return MPA2Interface::ReadReg(pChip, cAddress);
}
bool MPA2Interface::maskPixel(Chip* pChip, int pPixelNum, uint8_t pMask, bool pVerify)
{
    // pixel num starts from 1 [0 == global]
    auto    cRegValue = this->readPixel(pChip, "ENFLAGS", pPixelNum); // enflags content not changed in MPA2 --could use MPA2 register masking but no pixel mask
    uint8_t cNewValue = (cRegValue & 0xFE) | (1 - pMask);
    LOG(DEBUG) << BOLDBLUE << "Setting pixel mask to 0x" << std::hex << +cNewValue << std::dec << " on PixelNum#" << pPixelNum << RESET;
    return this->configPixel(pChip, "ENFLAGS", pPixelNum, cNewValue, pVerify);
}
bool MPA2Interface::maskRowCol(Chip* pChip, int pRow, int pColumn, uint8_t pMask, bool pVerify)
{
    // row and col num starts from 1 [0 == global]
    int cPixNum = 1 + (pColumn - 1) * NSSACHANNELS + (pRow - 1); // starting from 1
    return this->maskPixel(pChip, cPixNum, pMask, pVerify);
}
bool MPA2Interface::configRow(Chip* pChip, std::string cReg, int pRow, uint8_t pValue, bool pVerify)
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

    if(cReg.find("L1Offset_2") != std::string::npos) // Now L1Offset_2 is part of a larger register so broadcast would overwrite -- to improve, maybe an auto row loop?
    {
        if(pRow == 0) LOG(ERROR) << "Can not write L1Offset_2 as chip broadcast";
        uint8_t currval = MPA2Interface::ReadReg(pChip, cAddress, pVerify);
        pValue          = (currval & 0xFE) | (pValue & 1); // Flip last bit
    }

    LOG(DEBUG) << BOLDBLUE << "\t... register address 0x" << std::hex << +cAddress << std::dec << RESET;
    return MPA2Interface::WriteReg(pChip, cAddress, pValue, pVerify);
}
bool MPA2Interface::configPeri(Chip* pChip, std::string cReg, uint8_t pValue, bool pVerify) // MPA2 update to add block
{
    LOG(DEBUG) << BOLDBLUE << "Configuring peri register " << RESET;
    //     << cReg
    //     << " on MPA#"<< +pChip->getId() << " : " << cReg << " writing " << +pValue << RESET;
    // LOG (INFO) << BOLDRED << PERI_CONFIG_TABLE.size() << " items in peri map." << RESET;
    // for( auto cMapItem : PERI_CONFIG_TABLE )
    //     LOG (INFO) << cMapItem.first << " " << +cMapItem.second << RESET;

    if(cReg == "ReadoutMode")
    {
        uint8_t cBitShift = 0;
        return this->WriteChipRegBits(pChip, "Control_1", (pValue << cBitShift), "Mask", 0x3, pVerify);
    }

    uint8_t  cRegAddress = (PERI_CONFIG_TABLE.find(cReg))->second.second;
    uint8_t  cBlock      = (PERI_CONFIG_TABLE.find(cReg))->second.first;
    uint16_t cAddress    = this->regPeri(pChip, cBlock, cRegAddress);
    LOG(DEBUG) << BOLDBLUE << "\t... register address 0x" << std::hex << +cAddress << std::dec << RESET;
    return MPA2Interface::WriteReg(pChip, cAddress, pValue, pVerify);
}
uint16_t MPA2Interface::readPeri(Chip* pChip, std::string cReg) // MPA2 update to add block
{
    uint8_t  cRegAddress = (PERI_CONFIG_TABLE.find(cReg))->second.second;
    uint8_t  cBlock      = (PERI_CONFIG_TABLE.find(cReg))->second.first;
    uint16_t cAddress    = this->regPeri(pChip, cBlock, cRegAddress);
    LOG(DEBUG) << BOLDBLUE << "Reading peri register 0x" << std::hex << cAddress << std::dec << RESET;
    return MPA2Interface::ReadReg(pChip, cAddress);
}

uint16_t MPA2Interface::regPixel(Chip* pChip, int pBaseRegister, int pRow, int pColumn)
{
    uint16_t cRegAddress = ((pRow << 11) | (pBaseRegister << 7) | pColumn);
    return cRegAddress;
}
uint16_t MPA2Interface::regPeri(Chip* pChip, int cBlock, int pBaseRegister) // MPA2 update to add block
{
    uint16_t cRegAddress = ((cBlock << 11) | (0x0 << 8) | pBaseRegister);
    return cRegAddress;
}
uint16_t MPA2Interface::regRow(Chip* pChip, int pBaseRegister, int pRow)
{
    uint16_t cRegAddress = ((pRow << 11) | (pBaseRegister << 7) | 0x79);
    return cRegAddress;
}

// two methods -- also kinda slow but cant really use broadcast commands with enflags without overwriting other bits
// Would want  to do a rowwise broadcast -- maybe using col 0  for bit mask?

bool MPA2Interface::maskChannelGroup(ReadoutChip* cChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerify)
{
    auto cOriginalMask = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS * NMPACOLS>>(cChip->getChipOriginalMask());
    auto groupToMask   = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS * NMPACOLS>>(group);

    auto cBitset = std::bitset<NSSACHANNELS * NMPACOLS>(groupToMask->getBitset() & cOriginalMask->getBitset());
    // cBitset = cBitset&std::bitset<NSSACHANNELS*NMPACOLS>(0x0000F0FF0);
    LOG(DEBUG) << BOLDBLUE << "\t... Applying mask to MPA" << +cChip->getId() << " with " << group->getNumberOfEnabledChannels() << " desired mask \t... : " << cBitset
               << " original mask  \t... : " << cOriginalMask << " enabled channels "
               << " original bitset was be \t... " << groupToMask->getBitset() << RESET;

    // std::vector<std::pair<std::string, uint16_t>> pVecReq;
    // pVecReq.clear();
    bool returnval = true;
    for(uint32_t ipix = 0; ipix < (NSSACHANNELS * NMPACOLS); ipix++)
    {
        auto shifted = std::bitset<NSSACHANNELS * NMPACOLS>(0x1) << ipix;
        bool bitval  = bool(((cBitset & shifted) >> ipix).to_ulong());

        if(bitval) continue;
        // std::cout<<"MASK "<<ipix<<std::endl;
        // uint32_t           cPixelIds = ipix;
        // std::ostringstream cRegName;
        // cRegName << "ENFLAGS_P" << std::to_string(cPixelIds+1);
        // uint16_t regval=this->ReadChipReg(cChip, cRegName.str());
        // regval=regval&(0xFF&bitval);
        // std::pair<std::string, uint16_t> Req;
        // Req.first=cRegName.str();
        // Req.second=regval;
        // pVecReq.push_back(Req);

        returnval &= maskPixel(cChip, ipix + 1, (1 - bitval), pVerify); // I  think mask 0 is enable?
    }
    // return this->WriteChipMultReg(cChip, pVecReq);
    return returnval;
}

bool MPA2Interface::setInjectionSchema(ReadoutChip* cChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerify)
{
    std::bitset<NSSACHANNELS* NMPACOLS> cBitset = std::bitset<NSSACHANNELS * NMPACOLS>(std::static_pointer_cast<const ChannelGroup<NSSACHANNELS * NMPACOLS>>(group)->getBitset());
    if(cBitset.count() == 0) // no mask set... so do nothing
        return true;

    auto cOriginalMask = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS * NMPACOLS>>(cChip->getChipOriginalMask());
    auto groupToMask   = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS * NMPACOLS>>(group);

    // cBitset=cBitset&std::bitset<NSSACHANNELS * NMPACOLS>(0xF0FF0);

    LOG(DEBUG) << BOLDBLUE << "\t... Applying injection to MPA" << +cChip->getId() << " with " << group->getNumberOfEnabledChannels() << " desired mask \t... : " << cBitset
               << " original mask  \t... : " << cOriginalMask << " enabled channels "
               << " original bitset was be \t... " << groupToMask->getBitset() << RESET;

    bool returnval = true;
    for(uint32_t ipix = 0; ipix < (NSSACHANNELS * NMPACOLS); ipix++)
    {
        auto shifted = std::bitset<NSSACHANNELS * NMPACOLS>(0x1) << ipix;
        bool bitval  = bool(((cBitset & shifted) >> ipix).to_ulong());

        returnval &= enablePixelInjection(cChip, ipix + 1, bitval, pVerify);
    }

    return returnval;
}
bool MPA2Interface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerify)
{
    bool success = true;
    if(mask) success &= maskChannelGroup(pChip, group, pVerify);
    if(inject) success &= setInjectionSchema(pChip, group, pVerify);

    return success;
}

bool MPA2Interface::enablePixelInjection(Chip* pChip, int pPixelNum, uint8_t pInj, bool pVerify)
{
    // auto    cRegValue = this->readPixel(pChip, "PixelEnable", pPixelNum);
    auto    cRegValue = this->readPixel(pChip, "ENFLAGS", pPixelNum);
    uint8_t cNewValue = (cRegValue & 0xBF) | (pInj << 6);

    LOG(DEBUG) << BOLDBLUE << "Setting Enable to 0x" << std::hex << +cNewValue << std::dec << RESET;
    return this->configPixel(pChip, "ENFLAGS", pPixelNum, cNewValue, pVerify);
}

bool MPA2Interface::ConfigureChipOriginalMask(ReadoutChip* pMPA, bool pVerify, uint32_t pBlockSize)
{
    // use pix 1 as a proxy. Better to save this as a constant
    LOG(DEBUG) << BOLDBLUE << "ConfigureChipOriginalMask" << RESET;
    auto pixval = readPixel(pMPA, "ENFLAGS", 1);
    // write broadcast then mask is much much faster than full config
    configPixel(pMPA, "ENFLAGS", 0, (pixval | 0x1));
    LOG(INFO) << BOLDBLUE << "Broadcasting " << pixval << " or " << (pixval | 0x1) << RESET;
    auto allChannelEnabledGroup = std::make_shared<ChannelGroup<NSSACHANNELS * NMPACOLS>>();
    return maskChannelGroup(pMPA, allChannelEnabledGroup, pVerify);
}

void MPA2Interface::readAllBias(Chip* pChip)
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

bool MPA2Interface::WriteChipRegBits(Chip* pMPA2, const std::string& pRegNode, uint16_t pValue, const std::string& pMaskReg, uint8_t mask, bool pVerify)
{
    this->WriteChipSingleReg(pMPA2, pMaskReg, mask, false);
    bool cReadoutMode = WriteChipSingleReg(pMPA2, pRegNode, pValue, false);
    this->WriteChipSingleReg(pMPA2, pMaskReg, 0xFF, false);
    return cReadoutMode;
}

bool MPA2Interface::WriteChipReg(Chip* pMPA2, const std::string& pRegName, uint16_t pValue, bool pVerify)
{
    setBoard(pMPA2->getBeBoardId());

    LOG(DEBUG) << BOLDMAGENTA << "MPA2Interface::WriteChipReg writing to " << pRegName << RESET;
    LOG(DEBUG) << BOLDMAGENTA << "VALUE " << pValue << RESET;

    // need to or success
    if(pRegName.find("ThDAC_ALL") != std::string::npos || pRegName.find("Threshold") != std::string::npos)
    {
        LOG(DEBUG) << BOLDMAGENTA << "Setting threshold on MPA#" << +pMPA2->getId() << " to " << pValue << RESET;
        this->Set_threshold(pMPA2, pValue);
        return true;
    }
    else if(pRegName == "vref")
    {
        return this->WriteChipSingleReg(pMPA2, "ADCcontrol", pValue, false);
    }
    else if(pRegName == "Offsets")
    {
        return this->WriteChipSingleReg(pMPA2, "TrimDAC_ALL", pValue, false);
    }
    else if(pRegName == "ReadoutMode")
    {
        return this->configPeri(pMPA2, "ReadoutMode", pValue);
    }

    else if(pRegName == "RetimePix")
    {
        uint8_t cBitShift = CONTROL_TABLE.find("RetimePix")->second;
        uint8_t cRegMask  = (0x7 << cBitShift);
        return this->WriteChipRegBits(pMPA2, "Control_1", (pValue << cBitShift), "Mask", cRegMask, pVerify);
    }

    else if(pRegName == "PhaseShift")
    {
        uint8_t cBitShift = CONTROL_TABLE.find("PhaseShift")->second;
        uint8_t cRegMask  = (0x7 << cBitShift);
        return this->WriteChipRegBits(pMPA2, "Control_1", (pValue << cBitShift), "Mask", cRegMask, pVerify);
    }

    else if(pRegName == "SSAOffset") // should work with MPA2 address table
    {
        uint8_t cBitShift = 0;
        return this->WriteChipReg(pMPA2, "SSAOffset_1", (0x00FF & pValue), false) & this->WriteChipRegBits(pMPA2, "SSAOffset_2", (((0x0100 & pValue) >> 8) << cBitShift), "Mask", 0x1, false);
    }

    else if(pRegName == "EnablePhaseAlignmentPattern")
    {
        this->producePhaseAlignmentPattern(static_cast<ReadoutChip*>(pMPA2), pValue);
        return true;
    }
    else if(pRegName.find("MaskChannel") != std::string::npos)
    {
        std::string cToken    = "MaskChannel";
        auto        cPixelNum = std::atoi(pRegName.substr(pRegName.find(cToken) + cToken.length(), 4).c_str());
        LOG(DEBUG) << BOLDMAGENTA << "Masking pixel number " << +cPixelNum << " register is " << pRegName << RESET;
        return maskPixel(pMPA2, cPixelNum, pValue, pVerify);
    }

    else if(pRegName.find("SelectEdgeT1") != std::string::npos)
    {
        std::string cRegName  = "EdgeSelT1Raw";
        uint8_t     cBitShift = 1; // Not sure about the bits here

        uint8_t cRegMask = (0x1 << cBitShift);

        return this->WriteChipRegBits(pMPA2, cRegName, (pValue << cBitShift), "Mask", cRegMask, pVerify);
    }
    else if(pRegName.find("SelectEdgeL") != std::string::npos)
    {
        std::string cToken  = "SelectEdgeL";
        auto        cLineId = std::atoi(pRegName.substr(pRegName.find(cToken) + cToken.length(), 1).c_str());
        if(cLineId == 8)
        {
            std::string cRegName  = "EdgeSelT1Raw";
            uint8_t     cBitShift = 0; // Not sure about the bits here

            uint8_t cRegMask = (0x1 << cBitShift);

            return this->WriteChipRegBits(pMPA2, cRegName, (pValue << cBitShift), "Mask", cRegMask, pVerify);
        }
        else
        {
            std::string cRegName  = "EdgeSelTrig";
            uint8_t     cBitShift = cLineId;

            uint8_t cRegMask = (0x1 << cBitShift); //

            return this->WriteChipRegBits(pMPA2, cRegName, (pValue << cBitShift), "Mask", cRegMask, pVerify);
        }

        return false;
    }

    else if(pRegName.find("OutSetting_0") != std::string::npos)
    {
        uint8_t cBitShift = 0;
        return this->WriteChipRegBits(pMPA2, "OutSetting_1_0", (pValue << cBitShift), "Mask", 0x7, pVerify);
    }
    else if(pRegName.find("OutSetting_1") != std::string::npos)
    {
        uint8_t cBitShift = 3;
        return this->WriteChipRegBits(pMPA2, "OutSetting_1_0", (pValue << cBitShift), "Mask", 0x38, pVerify);
    }
    else if(pRegName.find("OutSetting_2") != std::string::npos)
    {
        uint8_t cBitShift = 0;
        return this->WriteChipRegBits(pMPA2, "OutSetting_3_2", (pValue << cBitShift), "Mask", 0x7, pVerify);
    }
    else if(pRegName.find("OutSetting_3") != std::string::npos)
    {
        uint8_t cBitShift = 3;
        return this->WriteChipRegBits(pMPA2, "OutSetting_3_2", (pValue << cBitShift), "Mask", 0x38, pVerify);
    }
    else if(pRegName.find("OutSetting_4") != std::string::npos)
    {
        uint8_t cBitShift = 0;
        return this->WriteChipRegBits(pMPA2, "OutSetting_5_4", (pValue << cBitShift), "Mask", 0x7, pVerify);
    }
    else if(pRegName.find("OutSetting_5") != std::string::npos)
    {
        uint8_t cBitShift = 3;
        return this->WriteChipRegBits(pMPA2, "OutSetting_5_4", (pValue << cBitShift), "Mask", 0x38, pVerify);
    }
    else if(pRegName.find("SLVSDrive") != std::string::npos)
    {
        uint8_t cBitShift = 0;
        uint8_t cRegMask  = (0x7 << cBitShift); //
        // cRegMask          = ~(cRegMask);

        return this->WriteChipRegBits(pMPA2, "ConfSLVS", (pValue << cBitShift), "Mask", cRegMask, pVerify);

        // auto    cRegValue = this->ReadChipReg(pMPA2, "ConfSLVS");
        // uint8_t cValue    = (cRegValue & cRegMask) | (pValue << cBitShift);
        // LOG(DEBUG) << BOLDMAGENTA << "Setting SLVS register to 0x" << std::hex << +cValue << std::dec << RESET;
        // return this->WriteChipSingleReg(pMPA2, "ConfSLVS", cValue);
    }
    else if(pRegName.find("BendCode") != std::string::npos) // configure bend LUT -- Still need to convert to masking
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
        int     cIndex    = (cBendHalfStrips + 9) / 2;
        int     cNibble   = (cBendHalfStrips + 9) % 2;
        uint8_t cBitShift = 3 * (1 - cNibble);
        uint8_t cRegMask  = (0x7 << cBitShift); //
        // cRegMask             = ~(cRegMask);
        uint16_t cRegAddress = this->regPeri(pMPA2, 0x11, 5 + cIndex);
        uint16_t cRegValue   = MPA2Interface::ReadReg(pMPA2, cRegAddress);
        uint8_t  cValue      = (cRegValue & cRegMask) | (pValue << cBitShift);

        return MPA2Interface::WriteReg(pMPA2, cRegAddress, cValue, pVerify);
    }
    else if(PERI_CONFIG_TABLE.find(pRegName) != PERI_CONFIG_TABLE.end())
    {
        return this->configPeri(pMPA2, pRegName, pValue);
    }
    else if(pRegName == "TriggerLatency")
    {
        uint8_t cBitShift = 0;
        return this->WriteChipReg(pMPA2, "MemoryControl_1_ALL", (0x00FF & pValue), pVerify) &
               this->WriteChipRegBits(pMPA2, "MemoryControl_2_ALL", (((0x0100 & pValue) >> 8) << cBitShift), "Mask_ALL", 0x1, pVerify);
    }

    else if(pRegName == "StubInputPhase")
    {
        uint8_t cBitShift = 3;
        uint8_t cRegMask  = (0x7 << cBitShift); //
        // cRegMask          = ~(cRegMask);

        return this->WriteChipRegBits(pMPA2, "LatencyRx320", (pValue << cBitShift), "Mask", cRegMask, pVerify);

        // auto    cReg      = this->ReadChipReg(pMPA2, "LatencyRx320");
        // uint8_t cValue    = (cReg & cRegMask) | (pValue << cBitShift);
        // LOG(INFO) << BOLDBLUE << "Writing " << std::hex << +cValue << " " << +(cReg & cRegMask) << " " << (pValue << cBitShift) << std::dec << RESET;
        // return this->WriteChipSingleReg(pMPA2, "LatencyRx320", cValue);
    }
    else if(pRegName == "L1InputPhase")
    {
        uint8_t cBitShift = 0;
        uint8_t cRegMask  = (0x7 << cBitShift); //
        // cRegMask          = ~(cRegMask);

        return this->WriteChipRegBits(pMPA2, "LatencyRx320", (pValue << cBitShift), "Mask", cRegMask, pVerify);

        // auto    cReg      = this->ReadChipReg(pMPA2, "LatencyRx320");
        // uint8_t cValue    = (cReg & cRegMask) | (pValue << cBitShift);
        // LOG(INFO) << BOLDBLUE << "Writing " << std::bitset<8>(+cValue) << " mask is " << std::bitset<8>(cReg & cRegMask) << RESET; //"  "<<(pValue <<  cBitShift )<< std::dec << RESET;
        // return this->WriteChipSingleReg(pMPA2, "LatencyRx320", cValue);
    }
    else if(pRegName == "SamplePhaseShiftFine")
    {
        uint8_t cBitShift = 0;
        uint8_t cRegMask  = (0xF << cBitShift); //

        return this->WriteChipRegBits(pMPA2, "ConfDLL", (pValue << cBitShift), "Mask", cRegMask, pVerify);
    }

    else if(pRegName == "StubMode")
    {
        uint8_t cBitShift = ECM_TABLE.find("StubMode")->second;
        uint8_t cRegMask  = (0x3 << cBitShift); //
        return this->WriteChipRegBits(pMPA2, "ECM", (pValue << cBitShift), "Mask", cRegMask, pVerify);
    }
    else if(pRegName == "StubWindow")
    {
        uint8_t cBitShift = ECM_TABLE.find("StubWindow")->second;
        uint8_t cRegMask  = (0x3F << cBitShift);
        return this->WriteChipRegBits(pMPA2, "ECM", (pValue << cBitShift), "Mask", cRegMask, pVerify);
    }
    else if(pRegName == "DigitalPattern")
    {
        bool cReadoutMode   = configPeri(pMPA2, "ReadoutMode", 0x02);
        bool cConfigPattern = WriteChipSingleReg(pMPA2, "LFSR_data", pValue);
        return cReadoutMode && cConfigPattern;
    }
    else if(pRegName.find("DigitalSync") != std::string::npos) // Still need to convert to (pixel) masking
    {
        // tracker mode
        bool cReadoutMode = this->configPeri(pMPA2, "ReadoutMode", 0x00);
        // register mask
        std::vector<std::string> cPixelRegs{"PixelMask", "DigitalInjection"};
        uint8_t                  cFEEnable   = 0;
        uint8_t                  cEnableDigi = (pValue == 0) ? 0 : 1;
        std::vector<uint8_t>     cPixelVals{cFEEnable, cEnableDigi};
        uint8_t                  cRegMask = 0x00;
        for(auto cPixelReg: cPixelRegs) cRegMask = cRegMask | (1 << PIXEL_ENABLE_TABLE.find(cPixelReg)->second);
        // cRegMask       = ~(cRegMask);
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
            cReadValue   = this->readPixel(pMPA2, "ENFLAGS", cPixelNumber);
        }
        else
        {
            cReadValue = this->ReadChipReg(pMPA2, "ENFLAGS_ALL");
        }
        auto cRegValue = (cReadValue & cRegMask) | cValue;
        LOG(DEBUG) << BOLDBLUE << "Register mask " << pRegName << " 0x" << std::hex << +cRegMask << std::dec << " readback value is 0x" << std::hex << +cReadValue << std::dec << " will write value 0x"
                   << std::hex << +cValue << std::dec << " register value is 0x" << std::hex << +cRegValue << std::dec << RESET;

        bool cEnableDigital = this->configPixel(pMPA2, "ENFLAGS", cPixelNumber, cRegValue, pVerify);
        // configure pattern
        bool cConfigPattern = this->configPixel(pMPA2, "DigiPattern", cPixelNumber, pValue, pVerify);
        return cReadoutMode && cEnableDigital && cConfigPattern;
    }

    else if(pRegName == "ModeSel_ALL")
    {
        uint8_t cBitShift = 0;
        bool cSuccess = this->WriteChipRegBits(pMPA2, "PixelControl_ALL", (pValue << cBitShift), "Mask_ALL", 0x03, pVerify);
        return cSuccess;
    }
    else if(pRegName == "ClusterCut_ALL")
    {
        uint8_t cBitShift = 2;
        bool cSuccess = this->WriteChipRegBits(pMPA2, "PixelControl_ALL", (pValue << cBitShift), "Mask_ALL", 0x1C, pVerify);
        return cSuccess;
    }
    else if(pRegName == "HipCut_ALL")
    {
        LOG(INFO) << BOLDMAGENTA << "HipCut_ALL" << RESET;
        uint8_t cBitShift = 5;
        bool cSuccess = this->WriteChipRegBits(pMPA2, "PixelControl_ALL", (pValue << cBitShift), "Mask_ALL", 0xE0, pVerify);
        return cSuccess;
    }

    else if(pRegName == "AnalogueAsync")
    {
        // readout mode 1 -- ASYNC counter
        ChipRegMask cMask;
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("AnalogueInjection")->second;
        cMask.fNbits    = 1;
        pMPA2->setRegBits("ENFLAGS_ALL", cMask, pValue);
        // hit counter- enable
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("CounterEnable")->second;
        cMask.fNbits    = 1;
        pMPA2->setRegBits("ENFLAGS_ALL", cMask, pValue);
        // edge BR - enable
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("EnEdgeBR")->second;
        cMask.fNbits    = 1;
        pMPA2->setRegBits("ENFLAGS_ALL", cMask, pValue);
        // un-mask all pixels  - pixel enable
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("PixelMask")->second;
        cMask.fNbits    = 1;
        pMPA2->setRegBits("ENFLAGS_ALL", cMask, pValue);
        //
        if(pValue == 1)
            LOG(DEBUG) << BOLDBLUE << "Enabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA2->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        else
            LOG(DEBUG) << BOLDBLUE << "Disabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA2->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        bool cEnableAnalogue = this->configPixel(pMPA2, "ENFLAGS", 0, pMPA2->getRegItem("ENFLAGS_ALL").fValue, pVerify);
        // enabling async readout for stubs
        bool cReadoutMode = true;
        if(pValue == 1)
        {
            cReadoutMode = configPeri(pMPA2, "ReadoutMode", 0x01);
            LOG(DEBUG) << BOLDBLUE << "Enabling readout of I2C counters on MPA by setting register ReadoutMode to 0x" << std::hex << +pValue << std::dec << RESET;
        }
        else
        {
            cReadoutMode = configPeri(pMPA2, "ReadoutMode", 0x00);
            LOG(DEBUG) << BOLDBLUE << "Disabling readout of I2C counters on MPA by setting register ReadoutMode to 0x" << std::hex << +pValue << std::dec << RESET;
        }

        maskChannelGroup(static_cast<ReadoutChip*>(pMPA2), static_cast<ReadoutChip*>(pMPA2)->getChipOriginalMask());
        return cEnableAnalogue && cReadoutMode;
    }
    else if(pRegName == "AnalogueSync")
    {
        // readout mode 1 -- ASYNC counter
        ChipRegMask cMask;
        cMask.fBitShift = PIXEL_ENABLE_TABLE.find("AnalogueInjection")->second;
        cMask.fNbits    = 1;
        pMPA2->setRegBits("ENFLAGS_ALL", cMask, pValue);
        if(pValue == 1)
            LOG(DEBUG) << BOLDBLUE << "Enabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA2->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        else
            LOG(DEBUG) << BOLDBLUE << "Disabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" << std::hex << +pMPA2->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
        bool cEnableAnalogue = this->configPixel(pMPA2, "ENFLAGS", 0, pMPA2->getRegItem("ENFLAGS_ALL").fValue, pVerify);
        // enabling async readout for stubs
        bool cReadoutMode = configPeri(pMPA2, "ReadoutMode", 0x00);
        return cEnableAnalogue && cReadoutMode;
    }
    else if(pRegName == "Threshold" or pRegName == "Bias_THDAC")
    {
        LOG(DEBUG) << BOLDBLUE << "Setting "
                   << " bias thresh to " << +pValue << " on MPA" << +pMPA2->getId() << RESET;
        return Set_threshold(pMPA2, pValue);
    }

    else if(pRegName == "InjectedCharge")
    {
        LOG(DEBUG) << BOLDBLUE << "Setting "
                   << " bias calDac to " << +pValue << " on MPA" << +pMPA2->getId() << RESET;

        return Set_calibration(pMPA2, pValue);
    }
    else
    {
        LOG(DEBUG) << BOLDMAGENTA << "Writing " << +pValue << " to " << pRegName << RESET;
        return this->WriteChipSingleReg(pMPA2, pRegName, pValue, pVerify);
    }
}

bool MPA2Interface::WriteChipSingleReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerify) // unchanged from MPA1 -- to check
{
    setBoard(pChip->getBeBoardId());
    auto cRegItem   = pChip->getRegMap().find(pRegNode)->second;
    cRegItem.fValue = pValue;
    return fBoardFW->SingleRegisterWrite(pChip, cRegItem, pVerify);
}

bool MPA2Interface::WriteChipMultReg(Chip* pMPA2, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerify) // unchanged from MPA1 -- to check
{
    // first, identify the correct BeBoardFWInterface
    setBoard(pMPA2->getBeBoardId());
    auto                     cRegMap = pMPA2->getRegMap();
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
    return fBoardFW->MultiRegisterWrite(pMPA2, cRegItems, pVerify);
}

bool MPA2Interface::WriteChipAllLocalReg(ReadoutChip* pMPA2, const std::string& dacName, const ChipContainer& localRegValues, bool pVerify) // unchanged from MPA1 -- to check

{
    setBoard(pMPA2->getBeBoardId());
    assert(localRegValues.size() == pMPA2->getNumberOfChannels());
    std::string dacTemplate;

    if(dacName == "TrimDAC_P" or dacName == "ThresholdTrim") { dacTemplate = "TrimDAC_P%d"; }

    else
        LOG(ERROR) << "Error, DAC " << dacName << " is not a Local DAC";

    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    ChannelGroup<NMPACHANNELS, 1>                 channelToEnable;
    std::vector<uint32_t>                         cVec;
    cVec.clear();
    bool cSuccess = true;

    // check if all registers are the same
    std::vector<uint8_t> cVals(0);
    for(uint16_t iChannel = 0; iChannel < pMPA2->getNumberOfChannels(); ++iChannel)
    {
        cVals.push_back(localRegValues.getChannel<uint16_t>(iChannel));
        LOG(DEBUG) << BOLDMAGENTA << +cVals[cVals.size() - 1] << RESET;
    }

    if(std::adjacent_find(cVals.begin(), cVals.end(), std::not_equal_to<uint16_t>()) == cVals.end())
    {
        LOG(DEBUG) << BOLDBLUE << "All elements of " << dacName << " are equal to one  another .. will use global register" << RESET;
        if(dacName == "TrimDAC_P" or dacName == "ThresholdTrim")
        {
            bool cWrite = this->WriteChipReg(pMPA2, "TrimDAC_ALL", cVals[0], false);
            if(pVerify)
            {
                auto cReadback = this->ReadChipReg(pMPA2, "TrimDAC_P100");
                LOG(DEBUG) << BOLDMAGENTA << "Read-back a value of " << +cReadback << " from trim-dac register" << RESET;
                return (cReadback == cVals[0]);
            }
            else
                return cWrite;
        }
        // to-add .. add the rest
    }

    LOG(DEBUG) << BOLDBLUE << "Different values for " << dacName << " ... will NOT use global register" << RESET;

    for(uint16_t iChannel = 0; iChannel < pMPA2->getNumberOfChannels(); ++iChannel)
    {
        char dacName1[20];
        sprintf(dacName1, dacTemplate.c_str(), 1 + iChannel);
        LOG(DEBUG) << BOLDBLUE << "Setting register " << dacName1 << " to " << (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F) << RESET;
        cSuccess = cSuccess && this->WriteChipReg(pMPA2, dacName1, (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F), pVerify);
    }
    return cSuccess;
}

bool MPA2Interface::ConfigureChip(Chip* pMPA2, bool pVerify, uint32_t pBlockSize)
{
    // for now ..
    bool              cConfigLocalRegs = true;
    std::stringstream cOutput;
    setBoard(pMPA2->getBeBoardId());
    pMPA2->printChipType(cOutput);
    LOG(INFO) << BOLDBLUE << cOutput.str() << "...Configuring chip with Id[" << +pMPA2->getId() << "]" << RESET;
    pMPA2->setRegisterTracking(0);

    std::vector<uint32_t> cVec;
    ChipRegMap            cRegMap = pMPA2->getRegMap();
    // need to split between control and enable registers
    // don't read back enable registers
    std::vector<ChipRegItem> cCntrlRegItems;
    std::vector<ChipRegItem> cRegItems;
    std::vector<ChipRegItem> cLocalRegItems;
    cCntrlRegItems.clear();

    auto cOriginalMask = static_cast<ReadoutChip*>(pMPA2)->getChipOriginalMask();
    // std::vector<std::string>
    for(auto cMapItem: cRegMap)
    {
        if(cMapItem.second.fControlReg)
            cCntrlRegItems.push_back(cMapItem.second);
        else if((cMapItem.first.find("_P") != std::string::npos))
        {
            cLocalRegItems.push_back(cMapItem.second);
            if(cMapItem.first.find("ENFLAGS") != std::string::npos)
            {
                if((cMapItem.second.fValue & 0x1) == 0)
                {
                    // std::cout<<cMapItem.first<<","<<cMapItem.second.fValue<<" MASK"<<std::endl;
                    cOriginalMask->disableChannel(std::stoi(cMapItem.first.substr(9, cMapItem.first.size())) - 1);
                }
            }
        }
        else
            cRegItems.push_back(cMapItem.second);
    }
    this->WriteChipReg(pMPA2, "Mask", 0xFF, false);
    this->WriteChipReg(pMPA2, "Mask_ALL", 0xFF, false);

    // cntrl
    bool cSuccess = fBoardFW->MultiRegisterWrite(pMPA2, cCntrlRegItems, false);
    if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cCntrlRegItems.size() << " control registers in" << cOutput.str() << "#" << +pMPA2->getId() << RESET;
    // glbl
    cSuccess = fBoardFW->MultiRegisterWrite(pMPA2, cRegItems, false);
    if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cRegItems.size() << " R/W registers in" << cOutput.str() << "#" << +pMPA2->getId() << RESET;

    // lcl
    if(cConfigLocalRegs)
    {
        cSuccess = fBoardFW->MultiRegisterWrite(pMPA2, cLocalRegItems, true);
        if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cLocalRegItems.size() << " local R/W registers in" << cOutput.str() << "#" << +pMPA2->getId() << RESET;
    }
    pMPA2->setRegisterTracking(1);
    this->readFuseID(pMPA2);
    return cSuccess;
}

bool MPA2Interface::WriteRegs(Chip* pChip, const std::vector<std::pair<uint16_t, uint16_t>> pRegs, bool pVerify)
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

bool MPA2Interface::WriteReg(Chip* pChip, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerify)
{
    setBoard(pChip->getBeBoardId());
    auto        cRegMap   = pChip->getRegMap();
    auto        cIterator = find_if(cRegMap.begin(), cRegMap.end(), [&pRegisterAddress](const ChipRegPair& obj) { return obj.second.fAddress == pRegisterAddress; });
    ChipRegItem cItem     = cIterator->second;
    cItem.fValue          = pRegisterValue;
    return fBoardFW->SingleRegisterWrite(pChip, cItem, pVerify);
}

void MPA2Interface::setFileHandler(FileHandler* pHandler)
{
    setBoard(0);
    fBoardFW->setFileHandler(pHandler);
}

// These are not currently used but can encode pix registers
void MPA2Interface::Pix_write(ReadoutChip* cMPA, ChipRegItem cRegItem, uint32_t row, uint32_t pixel, uint32_t data)
{
    ChipRegItem rowreg = cRegItem;
    rowreg.fAddress    = ((row & 0x0001f) << 11) | ((cRegItem.fAddress & 0x000f) << 7) | (pixel & 0xfffffff);
    rowreg.fValue      = data;
    fBoardFW->SingleRegisterWrite(cMPA, rowreg, true);
}

uint32_t MPA2Interface::Pix_read(ReadoutChip* cMPA, ChipRegItem cRegItem, uint32_t row, uint32_t pixel)
{
    return fBoardFW->SingleRegisterRead(cMPA, cRegItem);
    // what is this for!
    // std::chrono::milliseconds cShort(1);
    // rep = this->ReadChipReg(cMPA, "fc7_daq_ctrl.command_processor_block.i2c.mpa_MPA_i2c_reply.data");
    // return rep;
}

// void MPA2Interface::Activate_async(Chip* pMPA2) { this->WriteChipReg(pMPA2, "ReadoutMode", 0x1); }

// void MPA2Interface::Activate_sync(Chip* pMPA2) { this->WriteChipReg(pMPA2, "ReadoutMode", 0x0); }

void MPA2Interface::Activate_pp(Chip* pMPA2, uint8_t win) { this->WriteChipReg(pMPA2, "ECM", (0x2 << 6) | win); }

void MPA2Interface::Activate_ss(Chip* pMPA2, uint8_t win) { this->WriteChipReg(pMPA2, "ECM", (0x1 << 6) | win); }

void MPA2Interface::Activate_ps(Chip* pMPA2, uint8_t win) { this->WriteChipReg(pMPA2, "ECM", win); }

bool MPA2Interface::Set_calibration(Chip* pMPA2, uint32_t cal)
{
    bool success = true;
    success      = success & (this->WriteChipReg(pMPA2, "CalDAC0", cal));
    success      = success & (this->WriteChipReg(pMPA2, "CalDAC1", cal));
    success      = success & (this->WriteChipReg(pMPA2, "CalDAC2", cal));
    success      = success & (this->WriteChipReg(pMPA2, "CalDAC3", cal));
    success      = success & (this->WriteChipReg(pMPA2, "CalDAC4", cal));
    success      = success & (this->WriteChipReg(pMPA2, "CalDAC5", cal));
    success      = success & (this->WriteChipReg(pMPA2, "CalDAC6", cal));
    return success;
}

bool MPA2Interface::Set_threshold(Chip* pMPA2, uint32_t th)
{
    bool success = true;
    success      = success & (this->WriteChipReg(pMPA2, "ThDAC0", th));
    success      = success & (this->WriteChipReg(pMPA2, "ThDAC1", th));
    success      = success & (this->WriteChipReg(pMPA2, "ThDAC2", th));
    success      = success & (this->WriteChipReg(pMPA2, "ThDAC3", th));
    success      = success & (this->WriteChipReg(pMPA2, "ThDAC4", th));
    success      = success & (this->WriteChipReg(pMPA2, "ThDAC5", th));
    success      = success & (this->WriteChipReg(pMPA2, "ThDAC6", th));
    return success;
}

uint16_t MPA2Interface::ReadADC(Ph2_HwDescription::ReadoutChip* pChip, std::string pRegName)
{
    auto theRegister = ADC_CONTROL_TABLE.find(pRegName);
    if(theRegister == ADC_CONTROL_TABLE.end())
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " " << pRegName << "not found for this chip type - aborting." << RESET;
        std::runtime_error(std::string("MPA2Interface::ReadADC: Error, register not found for this chip type. Abort."));
    }
    LOG(DEBUG) << BOLDMAGENTA << "ReadADC for MPA2  register " << pRegName << " block " << +theRegister->second.first << " shift " << +theRegister->second.second << RESET;
    this->selectBlock(static_cast<ReadoutChip*>(pChip), theRegister->second.first, theRegister->second.second);
    uint32_t ADC = this->ADCMeasure(static_cast<ReadoutChip*>(pChip));
    LOG(DEBUG) << BOLDMAGENTA << " ADC " << ADC << RESET;
    return ADC;
}

float MPA2Interface::ADCMeasure(Chip* pMPA2, uint32_t nreads)
{
    uint32_t ADCReadsAve = 0;
    for(uint32_t i = 0; i < nreads; i++)
    {
        this->WriteChipRegBits(pMPA2, "ADCcontrol", (0x7 << 5), "Mask", (0x7 << 5));
        this->WriteChipRegBits(pMPA2, "ADCcontrol", (0x6 << 5), "Mask", (0x7 << 5));
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
        uint16_t ADCRead = this->ReadChipReg(pMPA2, "ADC_output");
        ADCReadsAve += ADCRead;
        // std::cout<<"ADCRead "<<+ADCRead<<std::endl;
    }
    // std::cout<<"ADCReadAVE "<<float(ADCReadsAve)/float(nreads)<<std::endl;
    return float(ADCReadsAve) / float(nreads);
}

float MPA2Interface::calculateADCLSB(Chip* pMPA2, float vrefExp)
{
    this->selectBlock(pMPA2, 1, 7, 0);
    float offset = this->ADCMeasure(pMPA2);

    // LOG(INFO) << BOLDMAGENTA << "ADCLSB "<<vrefExp/(4095.0 - offset) << RESET;
    return vrefExp / (4095.0 - offset);
}

bool MPA2Interface::selectBlock(Chip* pMPA2, uint8_t block, uint8_t testPoint, uint8_t swEn) { return this->WriteChipReg(pMPA2, "ADC_TEST_selection", ((swEn << 7) + (testPoint << 4) + block), true); }

float MPA2Interface::measureGnd(Chip* pMPA2)
{
    this->WriteChipReg(pMPA2, "ADC_TEST_selection", 0, true);
    uint32_t sumData = 0;
    for(uint32_t iBlock = 0; iBlock < 7; iBlock++)
    {
        this->selectBlock(pMPA2, iBlock + 1, 7, 1);
        sumData += this->ADCMeasure(pMPA2); // maybe??
    }
    this->WriteChipReg(pMPA2, "ADC_TEST_selection", 0, true);
    return float(sumData) / 7.0;
}
float MPA2Interface::measureBg(Chip* pMPA2)
{
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    this->WriteChipReg(pMPA2, "ADC_TEST_selection", 0, true);
    this->selectBlock(pMPA2, 8, 7, 1);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    float data = this->ADCMeasure(pMPA2); // maybe??
    this->WriteChipReg(pMPA2, "ADC_TEST_selection", 0, true);
    return data;
}

void MPA2Interface::readFuseID(Chip* pMPA2)
{
    this->WriteChipReg(pMPA2, "EfuseMode", 0x0);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    this->WriteChipReg(pMPA2, "EfuseMode", 0xF);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    this->WriteChipReg(pMPA2, "EfuseMode", 0x0);

    uint32_t val = (this->ReadChipReg(pMPA2, "EfuseValue3") << 24) | (this->ReadChipReg(pMPA2, "EfuseValue2") << 16) | (this->ReadChipReg(pMPA2, "EfuseValue1") << 8) |
                   (this->ReadChipReg(pMPA2, "EfuseValue0") << 0);
    pMPA2->pChipFuseID.SetId(val);

    LOG(INFO) << GREEN << "FuseID from MPA2#" << +pMPA2->getId() << " Pos " << +pMPA2->pChipFuseID.Pos() << " Wafer " << +pMPA2->pChipFuseID.Wafer() << " Lot " << +pMPA2->pChipFuseID.Lot()
              << " Status " << +pMPA2->pChipFuseID.Status() << " Process " << +pMPA2->pChipFuseID.Process() << " ADCRef " << +pMPA2->pChipFuseID.ADCRef() << RESET;
}

void MPA2Interface::loadVref(Chip* pMPA2)
{
    // Set the Vref from the fuse
    this->readFuseID(pMPA2);
    this->WriteChipRegBits(pMPA2, "ADCcontrol", pMPA2->pChipFuseID.ADCRef(), "Mask", (0x1F));
    LOG(DEBUG) << BOLDMAGENTA << " loading VREF from fuse ID " << +pMPA2->pChipFuseID.ADCRef() << RESET;
}

void MPA2Interface::loadVref(Chip* pMPA2, uint8_t VREFvalue)
{
    // Set the Vref to a desired value
    this->WriteChipRegBits(pMPA2, "ADCcontrol", VREFvalue, "Mask", (0x1F));
    LOG(DEBUG) << BOLDMAGENTA << " loading VREF " << +VREFvalue << RESET;
}

// This is done at probe?
/*def calibrate_vref(self, vref_exp, lin_pts, plot = 0, verbose = 0):

    self.mpa.ctrl_base.disable_test()
    self.mpa.ctrl_base.set_peri_mask()
    self.i2c.peri_write("ADCtrimming", 0b01000000) # Bit 7 "trim_sel" to 1 selects I2C ctrl of VREF DAC
    self.select_block(10, 0 ,1)
    self.mpa.ctrl_base.set_peri_mask(0b00011111)
    if lin_pts < 1:
        lin_pts = 1
    elif lin_pts > 32:
        lin_pts = 32
    vref_dac_vals = np.zeros((2, lin_pts))
    iterable = np.ndenumerate(np.around(np.linspace(0, 31, lin_pts),0))
    # step through all DAC values, measure and capture VREF
    for index, dac_val in iterable:
        self.i2c.peri_write("ADCcontrol", int(dac_val))
        vref_val = self.multimeter.measure()
        vref_dac_vals[0:,index[0]] = (dac_val, vref_val)
        if verbose:
            print(f"VREF DAC val {int(dac_val)} : {round(vref_val, 4)}V")

    # linear regression, commented out because because not always the most accurate :/
    #slope = round(stats.linregress(vref_dac_vals)[0], 5) # return first value of linregress, which is the calculated slope
    # slope from two points
    #slope = (vref_val - vref_dac_vals[1].flat[0]) / 31
    # calculate new DAC value
    #offset = vref_dac_vals[1].flat[0]
    #vref_dac_new = int(round((vref_exp - offset) / slope,0))

    # choose DAC value based on closest to expected value
    diff = [(vref_exp - i)**2 for i in vref_dac_vals[1]]
    index_min = np.argmin(diff)
    vref_dac_new = vref_dac_vals[0][index_min]
    if (vref_dac_new > 31): vref_dac_new = 31
    # measure corrected VREF
    self.i2c.peri_write("ADCcontrol", int(vref_dac_new))
    vref_act = self.multimeter.measure()
    if (vref_act < (vref_exp + vref_exp*0.01)) & (vref_act > (vref_exp - vref_exp*0.01)):
        utils.print_good(f"Calibration of VREF DAC --> Done ({vref_act} V for {vref_dac_new} DAC)")
        ret = 1
    else:
        utils.print_error(f"Calibration of VREF DAC --> Failed ({vref_act} V for {vref_dac_new} DAC)")
        ret =0
    if verbose:
        print(f"vref_dac_new = {vref_dac_new}, VREF = {round(vref_act, 4)}V")
    self.mpa.ctrl_base.set_peri_mask()
    if plot:
        plt.plot(vref_dac_vals[0], vref_dac_vals[1])
        plt.xlabel('VREF code [LSB]'); plt.ylabel('VREF value [mV]'); plt.show()
    return ret, vref_dac_new, vref_dac_vals, vref_act*/

void MPA2Interface::ReadASEvent(ReadoutChip* pMPA2, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange)
{
    if(pSRange == std::pair<uint32_t, uint32_t>{0, 0}) pSRange = std::pair<uint32_t, uint32_t>{1, pMPA2->getNumberOfChannels()};
    for(uint32_t i = pSRange.first; i <= pSRange.second; i++)
    {
        uint8_t cRP1 = this->ReadChipReg(pMPA2, "ReadCounter_LSB_P" + std::to_string(i));
        uint8_t cRP2 = this->ReadChipReg(pMPA2, "ReadCounter_MSB_P" + std::to_string(i));

        pData.push_back((cRP2 * 256) + cRP1);
    }
}

bool MPA2Interface::enableInjection(ReadoutChip* pChip, bool inject, bool pVerify)
{
    setBoard(pChip->getBeBoardId());
    return this->WriteChipReg(pChip, "AnalogueAsync", 1);
}

uint32_t MPA2Interface::ReadData(BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait)
{
    setBoard(0);
    return fBoardFW->ReadData(pBoard, pBreakTrigger, pData, pWait);
}

void MPA2Interface::Cleardata()
{
    setBoard(0);
    // fBoardFW->Cleardata( );
}

} // namespace Ph2_HwInterface
