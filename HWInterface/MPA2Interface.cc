/*!

        \file                   MPA2.h
        \brief                  MPA2 Description class, config of the MPA2s
        \author                 Kevin Nash
        \version                1.0
        \date                   12/06/21
        Support :               mail to : knash201@gmail.com

 */

#include "HWInterface/MPA2Interface.h"
#include "HWDescription/MPA2.h"
#include "Utils/ChannelGroupHandler.h"
#include "Utils/ConsoleColor.h"
#include "Utils/Utilities.h"
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
    if(pRegNode == "StubMode" || pRegNode == "LayerSwap") // should work with MPA2 address table
    {
        return (ReadChipReg(pMPA2, "ECM") >> 6) & 0x3;
    }
    else if(pRegNode == "StubWindow") { return ReadChipReg(pMPA2, "ECM") & 0x3F; }
    else if(pRegNode == "ADC_VREF") { return ReadChipReg(pMPA2, "ADCcontrol") & 0x1F; }
    else if(pRegNode == "ReadoutMode") // New decoding control reg for MPA2
    {
        return ReadChipReg(pMPA2, "Control_1") & 0x3;
    }
    else if(pRegNode == "RetimePix") // New decoding control reg for MPA2
    {
        return (ReadChipReg(pMPA2, "Control_1") >> 2) & 0x7;
    }

    else if(pRegNode == "PhaseShift") // New decoding control reg for MPA2
    {
        return (ReadChipReg(pMPA2, "Control_1") >> 5) & 0x7;
    }
    else if(pRegNode == "Threshold") { return this->ReadChipReg(pMPA2, "ThDAC0"); }
    else if(pRegNode == "InjectedCharge") { return this->ReadChipReg(pMPA2, "CalDAC0"); }
    else if(pRegNode == "ADC_output") { return (this->ReadChipReg(pMPA2, "ADC_output_LSB") & 0xFF) + ((this->ReadChipReg(pMPA2, "ADC_output_MSB") & 0xF) << 8); }
    else if(pRegNode == "TriggerLatency") { return ((ReadChipReg(pMPA2, "MemoryControl_2_R0") & (0x1)) << 8) | ReadChipReg(pMPA2, "MemoryControl_1_R0"); }
    else if(pRegNode == "PixelControl_ALL" || pRegNode == "PixelControl") { return ReadChipReg(pMPA2, "PixelControl_R0"); }
    else if(pRegNode == "ENFLAGS_ALL") { return ReadChipReg(pMPA2, "ENFLAGS_C0_R0"); }
    else { return ReadChipSingleReg(pMPA2, pRegNode); }
}

// Unchanged from MPA1 -- to check
uint16_t MPA2Interface::ReadChipSingleReg(Chip* pChip, const std::string& pRegisterAddress)
{
    setBoard(pChip->getBeBoardId());
    ChipRegItem cRegItem = pChip->getRegItem(pRegisterAddress);
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

void MPA2Interface::produceBX0AlignmentPattern(ReadoutChip* pChip)
{
    // use sync bit only
    // this->MaskAllChannels(pChip, true, false );
    // auto masked =     this->ReadChipReg(pChip, "ENFLAGS_ALL");
    // std::cout << " read back masking MPAs 0x" << std::hex <<  masked << std::dec << std::endl;

    // use stubs
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> theClusterList{std::make_tuple<uint8_t, uint8_t, uint8_t>(0xA, 0x55, 1)};
    this->injectNoiseClusters(pChip, theClusterList);
    this->WriteChipReg(pChip, "StubMode", 2); // Use pixel mode to exclude possible SSA communication issues
    this->WriteChipReg(pChip, "StubWindow", 31);
    this->WriteChipReg(pChip, "CodeM10", 0x0); // bendind = 0 will ouput 0
    

    LOG(INFO) << GREEN << "Producing BX0 alignment pattern on MPA#" << +pChip->getId() << RESET;
    std::vector<uint8_t>     cRegValues{0x0}; //, fBX0AlignmentPatterns[0]};
    std::vector<std::string> cRegNames{"ReadoutMode"}; //, "LFSR_data"};
    // std::vector<uint8_t>     cRegValues{0x2, fWordAlignmentPatterns[0]};
    // std::vector<std::string> cRegNames{"ReadoutMode", "LFSR_data"};}
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
        std::ostringstream cRegName;
        cRegName << "DigitalSync_C" << pInjection.fColumn << "_R" << pInjection.fRow;
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
    std::vector<uint8_t>     cBendCodes(0);
    std::vector<std::string> listOfRegisters{"CodeDM8", "CodeM76", "CodeM54", "CodeM32", "CodeM10", "CodeP12", "CodeP34", "CodeP56", "CodeP78"};
    auto                     registerReadList = ReadChipMultReg(pChip, listOfRegisters);

    size_t registerIndex = 0;
    bool   firstSkipped  = false;
    for(const auto& registerNameAndValue: registerReadList)
    {
        if(listOfRegisters[registerIndex] != registerNameAndValue.first)
        {
            std::cerr << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] This should never happen, read registers should be in the same order of the query" << std::endl;
            abort();
        }
        ++registerIndex;

        if(firstSkipped)
            cBendCodes.push_back((registerNameAndValue.second >> 3) & 0x7);
        else
            firstSkipped = true;
        cBendCodes.push_back(registerNameAndValue.second & 0x7);
    }
    return cBendCodes;
}

bool MPA2Interface::configPixel(Chip* pChip, std::string cReg, uint16_t row, uint16_t col, uint8_t pValue, bool pVerify)
{
    return WriteChipReg(pChip, MPA2::getPixelRegisterName(cReg, row, col), pValue, pVerify);
}

uint16_t MPA2Interface::readPixel(Chip* pChip, std::string cReg, uint16_t row, uint16_t col) { return ReadChipReg(pChip, MPA2::getPixelRegisterName(cReg, row, col)); }
bool     MPA2Interface::maskPixel(Chip* pChip, uint16_t row, uint16_t col, bool doMask, bool pVerify)
{
    auto    cRegValue = readPixel(pChip, "ENFLAGS", row, col);
    uint8_t cNewValue = (cRegValue & 0xFE) | (doMask ? 0 : 1);
    return configPixel(pChip, "ENFLAGS", row, col, cNewValue, pVerify);
}

bool MPA2Interface::maskChannelGroup(ReadoutChip* cChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerify)
{
    uint32_t numberOfEnabledChannels = group->getNumberOfEnabledChannels();
    uint32_t totalNumberOfChannels   = NSSACHANNELS * NMPAROWS;

    auto cOriginalMask = cChip->getChipOriginalMask();

    std::vector<std::pair<std::string, uint16_t>> theRegisterVector;
    theRegisterVector.push_back({"Mask_ALL", 0x01});
    if(numberOfEnabledChannels < totalNumberOfChannels / 2) // faster to write unmasked channels
    {
        theRegisterVector.push_back({"ENFLAGS_ALL", 0x00});
        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
        {
            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
            {
                if(cOriginalMask->isChannelEnabled(row, col) && group->isChannelEnabled(row, col)) theRegisterVector.push_back({MPA2::getPixelRegisterName("ENFLAGS", row, col), 0x01});
            }
        }
    }
    else // faster to write masked channels
    {
        theRegisterVector.push_back({"ENFLAGS_ALL", 0x01});
        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
        {
            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
            {
                if(cOriginalMask->isChannelEnabled(row, col) && group->isChannelEnabled(row, col)) continue;
                theRegisterVector.push_back({MPA2::getPixelRegisterName("ENFLAGS", row, col), 0x00});
            }
        }
    }
    theRegisterVector.push_back({"Mask_ALL", 0xFF});

    return WriteChipMultReg(cChip, theRegisterVector);
}

bool MPA2Interface::setInjectionSchema(ReadoutChip* cChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerify)
{
    uint32_t numberOfEnabledChannels = group->getNumberOfEnabledChannels();
    uint32_t totalNumberOfChannels   = NSSACHANNELS * NMPAROWS;

    std::vector<std::pair<std::string, uint16_t>> theRegisterVector;
    theRegisterVector.push_back({"Mask_ALL", 0x40});
    if(numberOfEnabledChannels < totalNumberOfChannels / 2) // faster to write injected channels
    {
        theRegisterVector.push_back({"ENFLAGS_ALL", 0x00});
        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
        {
            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
            {
                if(group->isChannelEnabled(row, col)) theRegisterVector.push_back({MPA2::getPixelRegisterName("ENFLAGS", row, col), 0x40});
            }
        }
    }
    else // faster to write not injected channels
    {
        theRegisterVector.push_back({"ENFLAGS_ALL", 0x40});
        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
        {
            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
            {
                if(group->isChannelEnabled(row, col)) continue;
                theRegisterVector.push_back({MPA2::getPixelRegisterName("ENFLAGS", row, col), 0x00});
            }
        }
    }
    theRegisterVector.push_back({"Mask_ALL", 0xFF});

    return WriteChipMultReg(cChip, theRegisterVector);
}
bool MPA2Interface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerify)
{
    bool success = true;
    if(mask) success &= maskChannelGroup(pChip, group, pVerify);
    if(inject) success &= setInjectionSchema(pChip, group, pVerify);
    return success;
}

bool MPA2Interface::enablePixelInjection(Chip* pChip, uint16_t row, uint16_t col, bool inject, bool pVerify)
{
    auto    cRegValue = this->readPixel(pChip, "ENFLAGS", row, col);
    uint8_t cNewValue = (cRegValue & 0xBF) | ((inject ? 1 : 0) << 6);

    LOG(DEBUG) << BOLDBLUE << "Setting Enable to 0x" << std::hex << +cNewValue << std::dec << RESET;
    return this->configPixel(pChip, "ENFLAGS", row, col, cNewValue, pVerify);
}

bool MPA2Interface::ConfigureChipOriginalMask(ReadoutChip* pMPA, bool pVerify, uint32_t pBlockSize)
{
    auto allChannelEnabledGroup = std::make_shared<ChannelGroup<NMPAROWS, NSSACHANNELS>>();
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
    setBoard(pMPA2->getBeBoardId());
    uint16_t registerValue = pMPA2->getReg(pRegNode);

    // Preserve the original register values changing only the needed bits
    registerValue = (registerValue & ~mask) + pValue;

    std::vector<std::pair<std::string, uint16_t>> registerVector;
    registerVector.push_back({pMaskReg, mask});
    registerVector.push_back({pRegNode, pValue});
    registerVector.push_back({pMaskReg, 0xFF});
    bool cSuccess = WriteChipMultReg(pMPA2, registerVector, pVerify);

    pMPA2->setReg(pRegNode, registerValue);

    return cSuccess;
}

bool MPA2Interface::WriteChipReg(Chip* pMPA2, const std::string& pRegName, uint16_t pValue, bool pVerify)
{
    setBoard(pMPA2->getBeBoardId());

    // LOG(DEBUG) << BOLDMAGENTA << "MPA2Interface::WriteChipReg writing to " << pRegName << RESET;
    // LOG(DEBUG) << BOLDMAGENTA << "VALUE " << pValue << RESET;

    // need to or success
    if(pRegName.find("ThDAC_ALL") != std::string::npos || pRegName.find("Threshold") != std::string::npos) { return this->setThreshold(pMPA2, pValue); }
    else if(pRegName == "DL_ctrl")
        return this->setInjectionDelay(pMPA2, pValue);
    else if(pRegName == "ADC_VREF")
    {
        uint8_t cRegMask = 0x1F;
        // return this->WriteChipReg(pMPA2, "ADCcontrol", pValue , false);
        return this->WriteChipRegBits(pMPA2, "ADCcontrol", pValue, "Mask", cRegMask, false);
    }
    else if(pRegName == "Offsets") { return this->WriteChipReg(pMPA2, "TrimDAC_ALL", pValue, false); }
    else if(pRegName == "ReadoutMode") { return this->WriteChipRegBits(pMPA2, "Control_1", pValue, "Mask", 0x3, false); }
    else if(pRegName == "RetimePix")
    {
        uint8_t cBitShift = 2;
        uint8_t cRegMask  = (0x7 << cBitShift);
        return this->WriteChipRegBits(pMPA2, "Control_1", (pValue << cBitShift), "Mask", cRegMask, false);
    }
    else if(pRegName == "PhaseShift")
    {
        uint8_t cBitShift = 5;
        uint8_t cRegMask  = (0x7 << cBitShift);
        return this->WriteChipRegBits(pMPA2, "Control_1", (pValue << cBitShift), "Mask", cRegMask, false);
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
    else if(pRegName.find("SelectEdgeT1") != std::string::npos)
    {
        uint8_t cBitShift = 1; // Not sure about the bits here
        uint8_t cRegMask  = (0x1 << cBitShift);
        return this->WriteChipRegBits(pMPA2, "EdgeSelT1Raw", (pValue << cBitShift), "Mask", cRegMask, false);
    }
    else if(pRegName.find("SelectEdgeL") != std::string::npos)
    {
        std::string cToken  = "SelectEdgeL";
        auto        cLineId = std::atoi(pRegName.substr(pRegName.find(cToken) + cToken.length(), 1).c_str());
        if(cLineId == 8)
        {
            uint8_t cBitShift = 0; // Not sure about the bits here
            uint8_t cRegMask  = (0x1 << cBitShift);
            return this->WriteChipRegBits(pMPA2, "EdgeSelT1Raw", (pValue << cBitShift), "Mask", cRegMask, false);
        }
        else
        {
            uint8_t cBitShift = cLineId;
            uint8_t cRegMask  = (0x1 << cBitShift);
            return this->WriteChipRegBits(pMPA2, "EdgeSelTrig", (pValue << cBitShift), "Mask", cRegMask, false);
        }
    }
    else if(pRegName.find("OutSetting_0") != std::string::npos)
    {
        uint8_t cBitShift = 0;
        return this->WriteChipRegBits(pMPA2, "OutSetting_1_0", (pValue << cBitShift), "Mask", 0x7, false);
    }
    else if(pRegName.find("OutSetting_1") != std::string::npos)
    {
        uint8_t cBitShift = 3;
        return this->WriteChipRegBits(pMPA2, "OutSetting_1_0", (pValue << cBitShift), "Mask", 0x38, false);
    }
    else if(pRegName.find("OutSetting_2") != std::string::npos)
    {
        uint8_t cBitShift = 0;
        return this->WriteChipRegBits(pMPA2, "OutSetting_3_2", (pValue << cBitShift), "Mask", 0x7, false);
    }
    else if(pRegName.find("OutSetting_3") != std::string::npos)
    {
        uint8_t cBitShift = 3;
        return this->WriteChipRegBits(pMPA2, "OutSetting_3_2", (pValue << cBitShift), "Mask", 0x38, false);
    }
    else if(pRegName.find("OutSetting_4") != std::string::npos)
    {
        uint8_t cBitShift = 0;
        return this->WriteChipRegBits(pMPA2, "OutSetting_5_4", (pValue << cBitShift), "Mask", 0x7, false);
    }
    else if(pRegName.find("OutSetting_5") != std::string::npos)
    {
        uint8_t cBitShift = 3;
        return this->WriteChipRegBits(pMPA2, "OutSetting_5_4", (pValue << cBitShift), "Mask", 0x38, false);
    }
    else if(pRegName.find("SLVSDrive") != std::string::npos)
    {
        uint8_t cBitShift = 0;
        uint8_t cRegMask  = (0x7 << cBitShift);
        return this->WriteChipRegBits(pMPA2, "ConfSLVS", (pValue << cBitShift), "Mask", cRegMask, false);
    }
    else if(pRegName == "TriggerLatency")
    {
        uint8_t cBitShift = 0;
        return this->WriteChipReg(pMPA2, "MemoryControl_1_ALL", (0x00FF & pValue), pVerify) &
               this->WriteChipRegBits(pMPA2, "MemoryControl_2_ALL", (((0x0100 & pValue) >> 8) << cBitShift), "Mask_ALL", 0x1, false);
    }

    else if(pRegName == "StubInputPhase")
    {
        uint8_t cBitShift = 3;
        uint8_t cRegMask  = (0x7 << cBitShift);
        return this->WriteChipRegBits(pMPA2, "LatencyRx320", (pValue << cBitShift), "Mask", cRegMask, false);
    }
    else if(pRegName == "L1InputPhase")
    {
        uint8_t cBitShift = 0;
        uint8_t cRegMask  = (0x7 << cBitShift);
        return this->WriteChipRegBits(pMPA2, "LatencyRx320", (pValue << cBitShift), "Mask", cRegMask, false);
    }
    else if(pRegName == "SamplePhaseShiftFine")
    {
        uint8_t cBitShift = 0;
        uint8_t cRegMask  = (0xF << cBitShift);
        return this->WriteChipRegBits(pMPA2, "ConfDLL", (pValue << cBitShift), "Mask", cRegMask, false);
    }
    else if(pRegName == "StubMode")
    {
        uint8_t cBitShift = 6;
        uint8_t cRegMask  = (0x3 << cBitShift);
        return this->WriteChipRegBits(pMPA2, "ECM", (pValue << cBitShift), "Mask", cRegMask, false);
    }
    else if(pRegName == "StubWindow") { return this->WriteChipRegBits(pMPA2, "ECM", pValue, "Mask", 0x3F, false); }
    else if(pRegName == "DigitalPattern")
    {
        bool cReadoutMode   = WriteChipReg(pMPA2, "ReadoutMode", 0x02, pVerify);
        bool cConfigPattern = WriteChipReg(pMPA2, "LFSR_data", pValue, pVerify);
        return cReadoutMode && cConfigPattern;
    }
    else if(pRegName.find("DigitalSync") != std::string::npos) // Still need to convert to (pixel) masking
    {
        // tracker mode
        bool success = true;
        success &= WriteChipReg(pMPA2, "ReadoutMode", 0x00, pVerify);
        // register mask

        uint16_t    enableRegisterMask  = 0x21;
        uint16_t    enableRegisterValue = ((pValue == 0) ? 0 : 1) << 5; // enabling the channel and injecting digital
        std::string enableRegisterName;
        std::string patternRegisterName;
        if(pRegName.find("_P") != std::string::npos) // single pixel
        {
            auto pixelAddress   = extractMaskedPixelAddress(pRegName);
            enableRegisterName  = MPA2::getPixelRegisterName("ENFLAGS", pixelAddress.first, pixelAddress.second);
            patternRegisterName = MPA2::getPixelRegisterName("DigPattern", pixelAddress.first, pixelAddress.second);
        }
        else
        {
            enableRegisterName  = "ENFLAGS_ALL";
            patternRegisterName = "DigPattern_ALL";
        }

        success &= WriteChipRegBits(pMPA2, enableRegisterName, enableRegisterValue, "Mask_ALL", enableRegisterMask, false);
        success &= WriteChipReg(pMPA2, patternRegisterName, pValue, pVerify);
        return success;
    }
    else if(pRegName == "ModeSel_ALL")
    {
        uint8_t cBitShift = 0;
        bool    cSuccess  = this->WriteChipRegBits(pMPA2, "PixelControl_ALL", (pValue << cBitShift), "Mask_ALL", 0x03, false);
        return cSuccess;
    }
    else if(pRegName == "ClusterCut_ALL")
    {
        uint8_t cBitShift = 2;
        bool    cSuccess  = this->WriteChipRegBits(pMPA2, "PixelControl_ALL", (pValue << cBitShift), "Mask_ALL", 0x1C, false);
        return cSuccess;
    }
    else if(pRegName == "HipCut_ALL")
    {
        LOG(INFO) << BOLDMAGENTA << "HipCut_ALL" << RESET;
        uint8_t cBitShift = 5;
        bool    cSuccess  = this->WriteChipRegBits(pMPA2, "PixelControl_ALL", (pValue << cBitShift), "Mask_ALL", 0xE0, false);
        return cSuccess;
    }
    else if(pRegName == "AnalogueAsync")
    {
        if(pValue != 0 && pValue != 1)
        {
            std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] pRegName = " << pRegName << " -> pValue can be only 0 or 1" << std::endl;
            abort();
        }
        uint16_t enableRegisterValue;
        uint16_t enableRegisterMask = 0x55;
        uint16_t readoutMode;
        if(pValue == 1)
        {
            enableRegisterValue = 0x55;
            readoutMode         = 0x01;
        }
        else
        {
            enableRegisterValue = 0x00;
            readoutMode         = 0x00;
        }

        bool success = true;
        success &= WriteChipRegBits(pMPA2, "ENFLAGS_ALL", enableRegisterValue, "Mask_ALL", enableRegisterMask, false);

        success &= WriteChipReg(pMPA2, "ReadoutMode", readoutMode);
        return success;
    }
    else if(pRegName == "AnalogueSync")
    {
        bool success = true;
        success &= WriteChipReg(pMPA2, "ReadoutMode", 0x00, pVerify);
        success &= WriteChipRegBits(pMPA2, "ENFLAGS_ALL", pValue == 0 ? 0 : 1, "Mask_ALL", 0x1, false);
        return success;
    }
    else if(pRegName == "Threshold" or pRegName == "Bias_THDAC")
    {
        // LOG(DEBUG) << BOLDBLUE << "Setting "
        //            << " bias thresh to " << +pValue << " on MPA" << +pMPA2->getId() << RESET;
        return setThreshold(pMPA2, pValue);
    }
    else if(pRegName == "InjectedCharge")
    {
        // LOG(DEBUG) << BOLDBLUE << "Setting "
        //            << " bias calDac to " << +pValue << " on MPA" << +pMPA2->getId() << RESET;

        return Set_calibration(pMPA2, pValue);
    }
    else
    {
        // LOG(DEBUG) << BOLDMAGENTA << "Writing " << +pValue << " to " << pRegName << RESET;
        return this->WriteChipSingleReg(pMPA2, pRegName, pValue, pVerify);
    }
}

bool MPA2Interface::WriteChipSingleReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerify) // unchanged from MPA1 -- to check
{
    setBoard(pChip->getBeBoardId());
    ChipRegItem cRegItem = pChip->getRegItem(pRegNode);
    cRegItem.fValue      = pValue;
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
            LOG(ERROR) << BOLDRED << "MPA2Interface::WriteChipMultReg trtying to write to a register that doesn't exist in the map : " << cReq.first << RESET;
            continue;
        }

        ChipRegItem cItem = cIterator->second;
        cItem.fValue      = cReq.second;
        cRegItems.push_back(cItem);
    }
    return fBoardFW->MultiRegisterWrite(pMPA2, cRegItems, pVerify);
}

std::vector<std::pair<std::string, uint16_t>> MPA2Interface::ReadChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::string>& theRegisterList)
{
    setBoard(pChip->getBeBoardId());
    auto                     cRegMap = pChip->getRegMap();
    std::vector<ChipRegItem> cRegItems;
    for(auto cReq: theRegisterList)
    {
        auto cIterator = cRegMap.find(cReq);
        if(cIterator == cRegMap.end())
        {
            LOG(ERROR) << BOLDRED << "MPA2Interface::WriteChipMultReg trtying to write to a register that doesn't exist in the map : " << cReq << RESET;
            abort();
        }

        ChipRegItem cItem = cIterator->second;
        cRegItems.push_back(cItem);
    }

    fBoardFW->MultiRegisterRead(pChip, cRegItems);

    std::vector<std::pair<std::string, uint16_t>> theRegisterValues;
    for(size_t i = 0; i < theRegisterList.size(); ++i) theRegisterValues.push_back(std::make_pair(theRegisterList[i], cRegItems[i].fValue));
    return theRegisterValues;
}

bool MPA2Interface::WriteChipAllLocalReg(ReadoutChip* pMPA2, const std::string& dacName, const ChipContainer& localRegValues, bool pVerify) // unchanged from MPA1 -- to check
{
    setBoard(pMPA2->getBeBoardId());
    assert(localRegValues.size() == pMPA2->getNumberOfChannels());

    if(dacName != "TrimDAC_C" && dacName != "ThresholdTrim")
    {
        LOG(ERROR) << "Error, DAC " << dacName << " is not a Local DAC";
        abort();
    }

    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    ChannelGroup<NMPAROWS, NSSACHANNELS>          channelToEnable;
    std::vector<uint32_t>                         cVec;
    cVec.clear();
    bool cSuccess = true;

    // check if all registers are the same
    std::vector<uint8_t> cVals(0);
    for(uint16_t row = 0; row < pMPA2->getNumberOfRows(); ++row)
    {
        for(uint16_t col = 0; col < pMPA2->getNumberOfCols(); ++col)
        {
            cVals.push_back(localRegValues.getChannel<uint16_t>(row, col));
            // LOG(DEBUG) << BOLDMAGENTA << +cVals[cVals.size() - 1] << RESET;
        }
    }

    if(std::adjacent_find(cVals.begin(), cVals.end(), std::not_equal_to<uint16_t>()) == cVals.end())
    {
        // LOG(DEBUG) << BOLDBLUE << "All elements of " << dacName << " are equal to one  another .. will use global register" << RESET;
        if(dacName == "TrimDAC_C" or dacName == "ThresholdTrim")
        {
            bool cWrite = this->WriteChipReg(pMPA2, "TrimDAC_ALL", cVals[0], false);
            if(pVerify)
            {
                auto cReadback = this->ReadChipReg(pMPA2, "TrimDAC_C10_R10");
                // LOG(DEBUG) << BOLDMAGENTA << "Read-back a value of " << +cReadback << " from trim-dac register" << RESET;
                return (cReadback == cVals[0]);
            }
            else
                return cWrite;
        }
        // to-add .. add the rest
    }

    // LOG(DEBUG) << BOLDBLUE << "Different values for " << dacName << " ... will NOT use global register" << RESET;
    std::vector<std::pair<std::string, uint16_t>> registerList;

    for(uint16_t row = 0; row < pMPA2->getNumberOfRows(); ++row)
    {
        for(uint16_t col = 0; col < pMPA2->getNumberOfCols(); ++col)
        {
            registerList.push_back({MPA2::getPixelRegisterName("TrimDAC", row, col), localRegValues.getChannel<uint16_t>(row, col) & 0x1F});
        }
    }
    cSuccess &= WriteChipMultReg(pMPA2, registerList, pVerify);
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
    auto theListOfFreeRegisters = pMPA2->getFreeRegisters();

    uint8_t maskValue    = 0xFF;
    uint8_t maskAllValue = 0xFF;

    for(auto cMapItem: cRegMap)
    {
        if(cMapItem.first == "Mask") maskValue = cMapItem.second.fValue;
        if(cMapItem.first == "Mask_ALL") maskAllValue = cMapItem.second.fValue;
        bool isFreeRegister = false;
        for(const auto& freeRegister: theListOfFreeRegisters)
        {
            isFreeRegister = std::regex_match(cMapItem.first, freeRegister.first);
            if(isFreeRegister) break;
        }
        if(isFreeRegister) continue; // skipping readonly registers

        if(cMapItem.second.fControlReg)
            cCntrlRegItems.push_back(cMapItem.second);
        else if((cMapItem.first.find("_C") != std::string::npos))
        {
            cLocalRegItems.push_back(cMapItem.second);
            if(cMapItem.first.find("ENFLAGS") != std::string::npos)
            {
                if((cMapItem.second.fValue & 0x1) == 0)
                {
                    auto maskedPixelAddress = extractMaskedPixelAddress(cMapItem.first);
                    cOriginalMask->disableChannel(maskedPixelAddress.first, maskedPixelAddress.second);
                }
            }
        }
        else
            cRegItems.push_back(cMapItem.second);
    }

    // Mask need to be written first, default value is 0
    this->WriteChipReg(pMPA2, "Mask", maskValue, false);
    this->WriteChipReg(pMPA2, "Mask_ALL", maskAllValue, false);

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
    return cSuccess;
}

void MPA2Interface::setFileHandler(FileHandler* pHandler)
{
    setBoard(0);
    fBoardFW->setFileHandler(pHandler);
}

// void MPA2Interface::Activate_async(Chip* pMPA2) { this->WriteChipReg(pMPA2, "ReadoutMode", 0x1); }

// void MPA2Interface::Activate_sync(Chip* pMPA2) { this->WriteChipReg(pMPA2, "ReadoutMode", 0x0); }

void MPA2Interface::Activate_pp(Chip* pMPA2, uint8_t win) { this->WriteChipReg(pMPA2, "ECM", (0x2 << 6) | win); }

void MPA2Interface::Activate_ss(Chip* pMPA2, uint8_t win) { this->WriteChipReg(pMPA2, "ECM", (0x1 << 6) | win); }

void MPA2Interface::Activate_ps(Chip* pMPA2, uint8_t win) { this->WriteChipReg(pMPA2, "ECM", win); }

bool MPA2Interface::Set_calibration(Chip* pMPA2, uint32_t cal)
{
    std::vector<std::pair<std::string, uint16_t>> theVector;
    for(int index = 0; index <= 6; ++index)
    {
        std::string registerName = "CalDAC" + convertToString(index);
        theVector.push_back(std::make_pair(registerName, cal));
    }
    bool success = this->WriteChipMultReg(pMPA2, theVector);
    return success;
}

bool MPA2Interface::setThreshold(Chip* pMPA2, uint8_t threshold) { return setAllBiasBlockRegisters(pMPA2, "ThDAC", threshold); }

bool MPA2Interface::setInjectionDelay(Chip* pMPA2, uint8_t injectionDelay) { return setAllBiasBlockRegisters(pMPA2, "DL_ctrl", injectionDelay); }

bool MPA2Interface::setAllBiasBlockRegisters(Chip* pMPA2, std::string registerName, uint8_t value)
{
    std::vector<std::pair<std::string, uint16_t>> theRegisterVector;
    for(int index = 0; index <= 6; ++index)
    {
        std::string registerFullName = registerName + convertToString(index);
        theRegisterVector.push_back(std::make_pair(registerFullName, value));
    }
    bool success = this->WriteChipMultReg(pMPA2, theRegisterVector);
    return success;
}

uint32_t MPA2Interface::readADC(Ph2_HwDescription::ReadoutChip* pChip, std::string pRegName)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    auto                                  theRegister = ADC_CONTROL_TABLE.find(pRegName);
    if(theRegister == ADC_CONTROL_TABLE.end())
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " " << pRegName << "not found for this chip type - aborting." << RESET;
        std::runtime_error(std::string("MPA2Interface::ReadADC: Error, register not found for this chip type. Abort."));
    }
    LOG(DEBUG) << BOLDMAGENTA << "ReadADC for MPA2  register " << pRegName << " block " << +theRegister->second.first << " shift " << +theRegister->second.second << RESET;
    this->selectBlock(static_cast<ReadoutChip*>(pChip), theRegister->second.first, theRegister->second.second);
    uint16_t ADC = this->ADCMeasure(static_cast<ReadoutChip*>(pChip));
    LOG(DEBUG) << BOLDMAGENTA << " ADC " << ADC << RESET;
    return ADC;
}

uint32_t MPA2Interface::readADCGround(Ph2_HwDescription::ReadoutChip* pChip)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    // It seems to be more precise for the ground...
    this->WriteChipReg(pChip, "ADC_TEST_selection", 0, true);
    uint32_t sumData = 0;
    for(uint32_t iBlock = 0; iBlock < 7; iBlock++)
    {
        this->selectBlock(pChip, iBlock + 1, 7, 1);
        sumData += this->ADCMeasure(pChip); // maybe??
    }
    this->WriteChipReg(pChip, "ADC_TEST_selection", 0, true);
    return uint32_t(float(sumData) / 7.0);

    // return readADC(pChip,"GND");
}

uint32_t MPA2Interface::readADCBandGap(Ph2_HwDescription::ReadoutChip* pChip)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    return readADC(pChip, "VBG");
}

uint32_t MPA2Interface::readADCVref(Ph2_HwDescription::ReadoutChip* pChip)
{
    uint32_t theVrefADC = readADC(pChip, "ADC_VREF");
    return theVrefADC;
}

uint32_t MPA2Interface::readVrefRegister(Ph2_HwDescription::ReadoutChip* pChip)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    uint32_t                              theVrefADC = ReadChipReg(pChip, "ADC_VREF");
    return theVrefADC;
}

float MPA2Interface::ADCMeasure(Chip* pMPA2, uint32_t nreads)
{
    uint32_t ADCReadsAve = 0;
    for(uint32_t i = 0; i < nreads; i++)
    {
        // this->WriteChipRegBits(pMPA2, "ADCcontrol", pValue, "Mask", cRegMask, false);
        this->WriteChipRegBits(pMPA2, "ADCcontrol", (0x7 << 5), "Mask", 0xE0);
        this->WriteChipRegBits(pMPA2, "ADCcontrol", (0x6 << 5), "Mask", 0xE0);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        uint16_t ADCRead = this->ReadChipReg(pMPA2, "ADC_output");
        ADCReadsAve += ADCRead;
        // std::cout<<"ADCRead "<<+ADCRead<<std::endl;
    }
    // disabling the ADC output after the mesurement
    this->WriteChipRegBits(pMPA2, "ADCcontrol", (0x0 << 5), "Mask", 0xE0);

    // std::cout<<"ADCReadAVE "<<float(ADCReadsAve)/float(nreads)<<std::endl;
    return float(ADCReadsAve) / float(nreads);
}

float MPA2Interface::calculateADCLSB(ReadoutChip* pMPA2, float theVrefValue)
{
    float offset = this->measureGround(pMPA2);
    // LOG(INFO) << BOLDMAGENTA << "ADCLSB "<<theVrefValue/(4095.0 - offset) << RESET;
    return theVrefValue / (4095.0 - offset);
}

bool MPA2Interface::selectBlock(Chip* pMPA2, uint8_t block, uint8_t testPoint, uint8_t swEn) { return this->WriteChipReg(pMPA2, "ADC_TEST_selection", ((swEn << 7) + (testPoint << 4) + block), true); }

uint32_t MPA2Interface::measureGround(ReadoutChip* pMPA2)
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

uint32_t MPA2Interface::ReadChipFuseID(Chip* pMPA2)
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
    return val;
}

bool MPA2Interface::setVrefFromFuseID(ReadoutChip* pMPA2)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);

    // Set the Vref from the fuse
    // this->ReadChipFuseID(pMPA2);
    LOG(DEBUG) << BOLDMAGENTA << " loading VREF from fuse ID " << +pMPA2->pChipFuseID.ADCRef() << RESET;
    return this->WriteChipRegBits(pMPA2, "ADCcontrol", pMPA2->pChipFuseID.ADCRef(), "Mask", (0x1F));
}

bool MPA2Interface::setVref(ReadoutChip* pMPA2, uint16_t VREFvalue)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);

    // Set the Vref to a desired value
    LOG(DEBUG) << BOLDMAGENTA << " loading VREF " << VREFvalue << RESET;
    return this->WriteChipReg(pMPA2, "ADC_VREF", VREFvalue); // , "Mask", (0x1F));

    // return this->WriteChipRegBits(pMPA2, "ADCcontrol", VREFvalue, "Mask", (0x1F));
}

const std::map<std::string, std::pair<uint8_t, float>> MPA2Interface::getBiasStructureDefaultTable(Ph2_HwDescription::ReadoutChip* pMPA2) { return MPA2_BIAS_STRUCTURE_DEFAULT; }

// FIXME At the moment we are setting the exepected values
//  of bandgap and ADC_VREF to the default nominal value.
//  This will be updated once we have the real values for each chip
float MPA2Interface::getBandGapExpectedValue(Ph2_HwDescription::ReadoutChip* pMPA2) { return MPA2_VBG_EXPECTED; }
// FIXME At the moment we are setting the exepected values
//  of bandgap and ADC_VREF to the default nominal value.
//  This will be updated once we have the real values for each chip
float MPA2Interface::getVrefExpectedValue(Ph2_HwDescription::ReadoutChip* pMPA2) { return MPA2_VREF_EXPECTED; }
// FIXME At the moment we are setting the exepected values
//  of bandgap and ADC_VREF to the default nominal value.
//  This will be updated once we have the real values for each chip
float MPA2Interface::getVrefPrecision(Ph2_HwDescription::ReadoutChip* pMPA2) { return MPA2_ADC_PRECISION; }
// FIXME At the moment we are setting the exepected values
//  of bandgap and ADC_VREF to the default nominal value.
//  This will be updated once we have the real values for each chip
float MPA2Interface::getVrefMinValue(Ph2_HwDescription::ReadoutChip* pMPA2) { return MPA2_VREF_MIN; }
// FIXME At the moment we are setting the exepected values
//  of bandgap and ADC_VREF to the default nominal value.
//  This will be updated once we have the real values for each chip
float MPA2Interface::getVrefMaxValue(Ph2_HwDescription::ReadoutChip* pMPA2) { return MPA2_VREF_MAX; }

bool MPA2Interface::disableTestPadsOutput(ReadoutChip* pMPA2)
{
    LOG(DEBUG) << BOLDMAGENTA << "Disable all MPA test pads outputs... " << RESET;
    return this->selectBlock(pMPA2, 0);
}

// bool MPA2Interface::selectTestPadsOutput(ReadoutChip* pMPA2, std::string theRegisterName)
// {
//     LOG(INFO) << BOLDMAGENTA << "Select MPA test pad output for "<< theRegisterName << RESET;
//     auto theBlock = ADC_CONTROL_TABLE
//     return this->selectBlock(pMPA2, 0);
// }

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

bool MPA2Interface::MaskAllChannels(ReadoutChip* pMPA, bool mask, bool pVerify) { return WriteChipRegBits(pMPA, "ENFLAGS_ALL", mask ? 0 : 1, "Mask_ALL", 0x01, pVerify); }

bool MPA2Interface::injectNoiseClusters(ReadoutChip* pMPA, std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> theClusterList)
{
    std::vector<std::pair<std::string, uint16_t>> listOfRegisters;

    listOfRegisters.push_back({"ENFLAGS_ALL", 0xa}); // masking all MPA and make sure polarity is 1
    listOfRegisters.push_back({"Mask", 0x03});
    listOfRegisters.push_back({"Control_1", 0x0}); // set Readout mode to normal
    listOfRegisters.push_back({"Mask", 0xFF});
    listOfRegisters.push_back({"PixelControl_ALL", 0x1E}); // disable Hip cut, cluster cut to the maximum, mode select to or
    listOfRegisters.push_back({"Mask_ALL", 0x02});

    for(const auto& theCluster: theClusterList)
    {
        for(uint8_t colIndex = 0; colIndex < std::get<2>(theCluster); ++colIndex)
        {
            listOfRegisters.push_back({MPA2::getPixelRegisterName("ENFLAGS", std::get<0>(theCluster), std::get<1>(theCluster) + colIndex), 0x0}); // inverting polarity for the pixels to inject
        }
    }

    listOfRegisters.push_back({"Mask_ALL", 0xFF});

    return WriteChipMultReg(pMPA, listOfRegisters);
}

std::pair<int, int> MPA2Interface::extractMaskedPixelAddress(const std::string& registerName) const
{
    std::pair<int, int> pixelAddress;

    // Find the position of "_C" and "_R"
    size_t posC = registerName.find("_C");
    size_t posR = registerName.find("_R");

    // Extract the number after "_C"
    std::string columnAddress = registerName.substr(posC + 2, posR - posC - 2);
    pixelAddress.second       = std::stoi(columnAddress);

    // Extract the number after "_R"
    std::string rowAddress = registerName.substr(posR + 2);
    pixelAddress.first     = std::stoi(rowAddress);

    return pixelAddress;
}

} // namespace Ph2_HwInterface
