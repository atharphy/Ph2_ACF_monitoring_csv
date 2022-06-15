/*!

        \file                                            SSA2Interface.cc
        \brief                                           User Interface to the SSA2s
        \author                                          Marc Osherson
        \version                                         1.0
        \date                        Jan 2021
        Support :                    mail to : oshersonmarc@gmail.com

 */

#include "SSA2Interface.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Container.h"
#include <bitset>

using namespace Ph2_HwDescription;

#define DEV_FLAG 0
namespace Ph2_HwInterface
{ // start namespace
SSA2Interface::SSA2Interface(const BeBoardFWMap& pBoardMap) : ReadoutChipInterface(pBoardMap) {}
SSA2Interface::~SSA2Interface() {}
//	// CONFIGURE CHIP
//////////
void SSA2Interface::DumpConfiguration(Chip* pSSA2, std::string filename)
{
    std::ofstream myfile;
    myfile.open(filename);
    for(auto& cRegInMap: pSSA2->getRegMap())
    {
        uint16_t val = this->ReadChipReg(pSSA2, cRegInMap.first);
        LOG(INFO) << BLUE << cRegInMap.first << "    @0x" << std::hex << cRegInMap.second.fAddress << "    0x" << val << std::dec << RESET;
        myfile << cRegInMap.first << "    @0x" << std::hex << cRegInMap.second.fAddress << "    0x" << val << std::dec << "\n";
    }
    myfile.close();
}
bool SSA2Interface::ConfigureChip(Chip* pSSA2, bool pVerifLoop, uint32_t pBlockSize)
{
    bool cConfigLocalRegs = true;
    pSSA2->setRegisterTracking(0);
    ChipRegMap        cSSA2RegMap = pSSA2->getRegMap();
    std::stringstream cOutput;
    setBoard(pSSA2->getBeBoardId());
    pSSA2->printChipType(cOutput);
    LOG(INFO) << BOLDBLUE << cOutput.str() << "...Configuring chip with Id[" << +pSSA2->getId() << "] oh Hybrid" << +pSSA2->getHybridId() << RESET;

    // write mask registers
    std::vector<std::string> cMaskRegs{"peri_A", "peri_D", "strip"};
    std::vector<ChipRegItem> cRegItems;
    cRegItems.clear();
    for(auto cName: cMaskRegs)
    {
        auto cItem   = cSSA2RegMap["mask_" + cName];
        cItem.fValue = 0xFF;
        cRegItems.push_back(cItem);
    }
    fBoardFW->MultiRegisterWrite(pSSA2, cRegItems, false);

    // configure W/R registers
    // do not overwrite these registers..
    std::vector<std::string> cRegsToSkip{"mask_strip", "mask_peri_A", "mask_peri_D"};
    std::vector<std::string> cReadOnlyRegs{"SEUcnt", "Ring_oscillator", "ADC_out", "bist_output", "AC_ReadCounter", "status_reg"};

    cRegItems.clear();
    // need to split between control and enable registers
    // don't read back enable registers
    std::vector<ChipRegItem> cCntrlRegItems;
    std::vector<ChipRegItem> cLocalRegItems;
    cCntrlRegItems.clear();
    for(auto cMapItem: cSSA2RegMap)
    {
        if(std::find(cRegsToSkip.begin(), cRegsToSkip.end(), cMapItem.first) != cRegsToSkip.end()) continue;
        bool cReadOnly = false;
        for(auto cReadOnlyReg: cReadOnlyRegs) cReadOnly = cReadOnly || (cMapItem.first.find(cReadOnlyReg) != std::string::npos);
        if(cReadOnly) continue;

        if(cMapItem.second.fControlReg)
            cCntrlRegItems.push_back(cMapItem.second);
        else if((cMapItem.first.find("_S") != std::string::npos))
            cLocalRegItems.push_back(cMapItem.second);
        else
            cRegItems.push_back(cMapItem.second);
    }
    bool cSuccess = fBoardFW->MultiRegisterWrite(pSSA2, cCntrlRegItems, false);
    if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cCntrlRegItems.size() << " control registers in SSA#" << +pSSA2->getId() << RESET;

    cSuccess = fBoardFW->MultiRegisterWrite(pSSA2, cRegItems, pVerifLoop);
    if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cRegItems.size() << " global R/W registers in SSA#" << +pSSA2->getId() << RESET;

    if(cConfigLocalRegs)
    {
        cSuccess = fBoardFW->MultiRegisterWrite(pSSA2, cLocalRegItems, pVerifLoop);
        if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cLocalRegItems.size() << " local R/W registers in SSA#" << +pSSA2->getId() << RESET;
    }

    pSSA2->setRegisterTracking(1);
    return cSuccess;
}

uint16_t SSA2Interface::ReadADC(ReadoutChip* pChip, uint8_t pInput)
{
    bool cVerify = true;
    setBoard(pChip->getBeBoardId());
    auto cRegMap = pChip->getRegMap();
    auto cItem   = cRegMap["ADC_control"];
    cItem.fValue = 0xE0 | (pInput & 0x1F);
    fBoardFW->SingleRegisterWrite(pChip, cItem, cVerify);
    cItem.fValue = 0xC0 | (pInput & 0x1F);
    fBoardFW->SingleRegisterWrite(pChip, cItem, cVerify);
    // this->WriteChipReg(pChip, "ADC_control", 0xE0 | (pInput & 0x1F));
    // this->WriteChipReg(pChip, "ADC_control", 0xC0 | (pInput & 0x1F));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    cItem         = cRegMap["ADC_out_H"];
    uint16_t cMSB = fBoardFW->SingleRegisterRead(pChip, cItem);
    cItem         = cRegMap["ADC_out_L"];
    uint16_t cLSB = fBoardFW->SingleRegisterRead(pChip, cItem);
    // uint16_t cMSB = this->ReadChipReg(pChip, "ADC_out_H");
    // uint16_t cLSB = this->ReadChipReg(pChip, "ADC_out_L");
    return (cMSB << 8 | cLSB);
}

// READ REGISTER ON CHIP:
uint16_t SSA2Interface::ReadChipReg(Chip* pSSA2, const std::string& pRegNode)
{
    setBoard(pSSA2->getBeBoardId());
    auto                     cRegMap = pSSA2->getRegMap();
    std::vector<ChipRegItem> cRegItems;
    ChipRegItem              cRegItem;
    if(pRegNode.find("CounterStrip") != std::string::npos)
    {
        int cChannel = 0;
        sscanf(pRegNode.c_str(), "CounterStrip%d", &cChannel);
        cRegItem.fPage    = 0x00;
        cRegItem.fAddress = 0x500 + cChannel; // MSB
        cRegItem.fValue   = 0;
        cRegItems.push_back(cRegItem);
        cRegItem.fAddress = 0x600 + cChannel;
        cRegItems.push_back(cRegItem); // LSB
    }
    else if(pRegNode == "ChipId")
    {
        return this->ReadChipId(pSSA2);
    }
    else if(pRegNode == "Threshold")
    {
        LOG(INFO) << BOLDYELLOW << "Adding thrshld register to multi-reg read..." << RESET;
        cRegItem = cRegMap["Bias_THDAC"];
        cRegItems.push_back(cRegItem);
    }
    else
    {
        cRegItem = cRegMap[pRegNode];
        cRegItems.push_back(cRegItem);
    }

    auto cValues = fBoardFW->MultiRegisterRead(pSSA2, cRegItems);
    if(pRegNode.find("CounterStrip") != std::string::npos) { return (cValues[0] << 8) | cValues[1]; }
    else
        return cValues[0];
}
// READ CHIP ID:
// FIX-ME
uint8_t SSA2Interface::ReadChipId(Chip* pChip)
{
    // set board
    setBoard(pChip->getBeBoardId());
    auto cRegMap = pChip->getRegMap();
    // ask SSA team how to read chip id
    auto cItem   = cRegMap["Fuse_Mode"];
    cItem.fValue = 0x0F;
    return cItem.fValue;
}
// WRITE REGISTER (ALL LOCAL):
bool SSA2Interface::WriteChipAllLocalReg(ReadoutChip* pChip, const std::string& dacName, ChipContainer& localRegValues, bool pVerifLoop) // FIXME SSA2
{
    bool cSuccess = true;
    // set board
    setBoard(pChip->getBeBoardId());
    auto cRegMap = pChip->getRegMap();

    // check if all registers are the same
    std::vector<uint8_t> cVals(0);
    for(uint16_t iChannel = 0; iChannel < pChip->getNumberOfChannels(); ++iChannel)
    {
        cVals.push_back(localRegValues.getChannel<uint16_t>(iChannel));
        LOG(DEBUG) << BOLDMAGENTA << +cVals[cVals.size() - 1] << RESET;
    }
    auto cAllTheSame = (std::adjacent_find(cVals.begin(), cVals.end(), std::not_equal_to<uint16_t>()) == cVals.end());
    if(cAllTheSame)
    {
        std::string cRegName = (dacName == "GainTrim") ? "StripControl2" : "THTRIMMING";
        LOG(INFO) << BOLDGREEN << " All local registers are the same " << RESET;
        auto cRegItem   = cRegMap[cRegName];
        cRegItem.fValue = localRegValues.getChannel<uint8_t>(0);
        cSuccess        = fBoardFW->SingleRegisterWrite(pChip, cRegItem, false);
        cRegName        = (dacName == "GainTrim") ? "StripControl2_S32" : "THTRIMMING_S32";
        auto cRegValue  = fBoardFW->SingleRegisterRead(pChip, cRegMap[cRegName]);
        LOG(INFO) << BOLDBLUE << cRegName << " set to 0x" << std::hex << +cRegValue << std::dec << RESET;
        cSuccess = (cRegValue == localRegValues.getChannel<uint8_t>(0));
        return cSuccess;
    }

    // check that you are actually configuring all local registers
    assert(localRegValues.size() == pChip->getNumberOfChannels());
    // figure out a few items based on the template
    std::string dacTemplate = (dacName == "GainTrim") ? "GAINTRIMMING_S" : "THTRIMMING_S";
    uint8_t     cMaskValue  = (dacName == "GainTrim") ? 120 : 31;
    // write mask
    // for some reason I have to write to all the mask registers... why!?
    std::vector<std::string> cMaskRegs{"strip", "peri_A", "peri_D"};
    std::vector<ChipRegItem> cRegItems;
    for(auto cName: cMaskRegs)
    {
        auto cRegName = "mask_" + cName;
        auto cItem    = cRegMap[cRegName];
        cItem.fValue  = (cRegName == "strip") ? cMaskValue : 0xFF;
        cRegItems.push_back(cItem);
    }
    cSuccess = fBoardFW->MultiRegisterWrite(pChip, cRegItems, pVerifLoop);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Failed to write to one of these registers" << RESET;
        for(auto cName: cMaskRegs) LOG(INFO) << BOLDRED << "mask_" << cName << RESET;
        return cSuccess;
    }

    // write local registers
    cRegItems.clear();
    ChannelGroup<NCHANNELS, 1> channelToEnable;
    for(uint8_t iChannel = 0; iChannel < pChip->getNumberOfChannels(); ++iChannel)
    {
        std::stringstream dacName;
        dacName << dacTemplate.c_str() << 1 + iChannel;
        auto cIterator = cRegMap.find(dacName.str());
        if(cIterator == cRegMap.end())
        {
            LOG(ERROR) << BOLDRED << "SSA2Interaface::WriteChipAllLocalReg trtying to write to a register that doesn't exist in the map : " << dacName.str() << RESET;
            continue;
        }
        ChipRegItem cItem = cIterator->second;
        cItem.fValue      = localRegValues.getChannel<uint16_t>(iChannel) & 0x1F;
        // LOG(INFO) << BOLDBLUE << "Setting register " << dacName.str() << " to " << cItem.fValue << RESET;
        cRegItems.push_back(cItem);
    }
    cSuccess = cSuccess && fBoardFW->MultiRegisterWrite(pChip, cRegItems, pVerifLoop);
    // write mask
    cRegItems.clear();
    cMaskValue = 0xFF;
    for(auto cName: cMaskRegs)
    {
        auto cRegName = "mask_" + cName;
        auto cItem    = cRegMap[cRegName];
        cItem.fValue  = cMaskValue;
        cRegItems.push_back(cItem);
    }
    cSuccess = cSuccess && fBoardFW->MultiRegisterWrite(pChip, cRegItems, pVerifLoop);
    return cSuccess;
}

bool SSA2Interface::WriteChipMultReg(Chip* pSSA2, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop)
{
    setBoard(pSSA2->getBeBoardId());
    auto                     cRegMap = pSSA2->getRegMap();
    std::vector<ChipRegItem> cRegItems;
    for(auto cReq: pVecReq)
    {
        auto cIterator = cRegMap.find(cReq.first);
        if(cIterator == cRegMap.end())
        {
            LOG(ERROR) << BOLDRED << "SSA2Interaface::WriteChipMultReg trtying to write to a register that doesn't exist in the map : " << cReq.first << RESET;
            continue;
        }

        ChipRegItem cItem = cIterator->second;
        cItem.fValue      = cReq.second;
        cRegItems.push_back(cItem);
    }
    return fBoardFW->MultiRegisterWrite(pSSA2, cRegItems, pVerifLoop);
}

bool SSA2Interface::WriteChipRegBits(Chip* pSSA2, const std::string& pRegNode, uint16_t pValue, const std::string& pMaskReg, uint8_t mask, bool pVerifLoop)
{
    bool cSuccess = true;
    setBoard(pSSA2->getBeBoardId());
    auto cRegMap = pSSA2->getRegMap();

    // write mask registers
    std::vector<std::string> cMaskRegs{"strip", "peri_A", "peri_D"};
    std::vector<ChipRegItem> cRegItems;
    LOG(DEBUG) << BOLDYELLOW << "Testing writing 0xFF to mask registers" << RESET;
    for(auto cName: cMaskRegs)
    {
        auto cRegName = "mask_" + cName;
        auto cItem    = cRegMap[cRegName];
        cItem.fValue  = (cRegName == pMaskReg) ? mask : 0xFF;
        cRegItems.push_back(cItem);
    }
    LOG(DEBUG) << BOLDYELLOW << "SSA2Interface::WriteChipRegBits Writing mask ... writing 0x" << std::hex << +mask << " to " << pMaskReg << std::dec << RESET;
    if(fBoardFW->MultiRegisterWrite(pSSA2, cRegItems, false))
    {
        LOG(DEBUG) << BOLDYELLOW << "\t..SSA2Interface::WriteChipRegBits Writing 0x" << std::hex << +pValue << " to " << pRegNode << std::dec << RESET;
        auto cRegItem   = cRegMap[pRegNode];
        cRegItem.fValue = pValue;
        cSuccess        = fBoardFW->SingleRegisterWrite(pSSA2, cRegItem, pVerifLoop);
    }
    // ensure mask is always reset
    cRegItems.clear();
    for(auto cName: cMaskRegs)
    {
        auto cRegName = "mask_" + cName;
        auto cItem    = cRegMap["mask_" + cName];
        cItem.fValue  = 0xFF;
        cRegItems.push_back(cItem);
    }
    LOG(DEBUG) << BOLDYELLOW << "SSA2Interface::WriteChipRegBits Resetting mask ... writing 0x" << std::hex << +mask << " to " << pMaskReg << std::dec << RESET;
    return cSuccess && fBoardFW->MultiRegisterWrite(pSSA2, cRegItems, false);

    // this->WriteChipSingleReg(pSSA2, pMaskReg, mask, pVerifLoop);
    // bool cReadoutMode = WriteChipSingleReg(pSSA2, pRegNode, pValue, pVerifLoop);
    // this->WriteChipSingleReg(pSSA2, pMaskReg, 0xFF, pVerifLoop);
    // return cReadoutMode;
}
//	// WRITE REGISTER (SINGLE CHIP REG <<main interface for writing>>):
//////////
bool SSA2Interface::WriteChipReg(Chip* pSSA2, const std::string& pRegName, uint16_t pValue, bool pVerifLoop)
{
    setBoard(pSSA2->getBeBoardId());
    // LOG (DEBUG) << BOLDYELLOW << "SSA2Interface::WriteChipReg  writing to " << pRegName << RESET;
    auto        cRegMap = pSSA2->getRegMap();
    ChipRegItem cRegItem;
    if(pRegName == "CountingMode")
    {
        cRegItem        = cRegMap["ENFLAGS"];
        cRegItem.fValue = (pValue << 2) | (1 << 0);
        return fBoardFW->SingleRegisterWrite(pSSA2, cRegItem, pVerifLoop);
    }
    else if(pRegName == "AmuxHigh")
    {
        return this->ConfigureAmux(pSSA2, "HighZ");
    }
    // need to re-name threshold here..
    // else if(fAmuxMap.find(pRegName) != fAmuxMap.end())
    // {
    //     return this->ConfigureAmux(pSSA2, pRegName);
    // }
    else if(pRegName == "MonitorBandgap")
    {
        return this->ConfigureAmux(pSSA2, "Bandgap");
    }
    else if(pRegName == "MonitorGround")
    {
        return this->ConfigureAmux(pSSA2, "GND");
    }
    else if(pRegName == "ReadoutMode")
    {
        return this->WriteChipRegBits(pSSA2, "control_1", pValue, "mask_peri_D", 7);
    }
    else if(pRegName == "SamplingMode")
    {
        return this->WriteChipRegBits(pSSA2, "control_1", pValue << 5, "mask_strip", 0x60);
    }
    else if(pRegName == "TriggerLatency")
    {
        bool cSuccess = this->WriteChipRegBits(pSSA2, "control_3", pValue & 0xFF, "mask_peri_D", 0xFF);
        cSuccess &= this->WriteChipRegBits(pSSA2, "control_1", (pValue & 0x100 >> 8), "mask_peri_D", 0x10);
        return cSuccess;
    }
    else if(pRegName == "AsyncDelay")
    {
        uint8_t                  cLSB = pValue & 0xFF;
        uint8_t                  cMSB = (pValue << 8);
        std::vector<ChipRegItem> cRegItems;
        std::vector<std::string> cRegNames{"AsyncRead_StartDel_LSB", "AsyncRead_StartDel_MSB"};
        for(auto cRegName: cRegNames)
        {
            cRegItem        = cRegMap[cRegName];
            cRegItem.fValue = (cRegName == "AsyncRead_StartDel_LSB") ? cLSB : cMSB;
            cRegItems.push_back(cRegItem);
        }
        return fBoardFW->MultiRegisterWrite(pSSA2, cRegItems, pVerifLoop);
    }
    else if(pRegName == "AnalogueSync")
    {
        uint8_t cReadoutMode = 0x0;
        uint8_t cEdgeSel_T1  = 0x0;
        // readout mode
        bool cSuccess = this->WriteChipRegBits(pSSA2, "control_1", cReadoutMode, "mask_peri_D", 0x7, pVerifLoop);
        // edge select
        cSuccess = cSuccess && this->WriteChipRegBits(pSSA2, "control_1", (cEdgeSel_T1 << 3), "mask_peri_D", (0x1 << 3));
        // duration
        uint8_t cDuration = 0x8;
        cSuccess          = cSuccess && this->WriteChipRegBits(pSSA2, "control_2", (cDuration << 4), "mask_peri_D", (0xF << 4));

        // sampling mode
        // sampling mode
        uint8_t cSamplingMode = 0;
        cSuccess              = cSuccess && this->WriteChipRegBits(pSSA2, "ENFLAGS", (cSamplingMode << 5), "mask_strip", (0x3 << 5));
        cRegItem              = cRegMap["ENFLAGS_S1"];
        auto cRegValue        = fBoardFW->SingleRegisterRead(pSSA2, cRegItem);
        LOG(DEBUG) << BOLDBLUE << "[post-set sampling] StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        // configure for injection with the strip register
        uint8_t cMask         = 1;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 0;
        uint8_t cDigitalCalib = 0;
        uint8_t cAnalogCalib  = pValue;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        cSuccess              = cSuccess && this->WriteChipRegBits(pSSA2, "ENFLAGS", cEnFlags, "mask_strip", 0x1F);
        cRegValue             = fBoardFW->SingleRegisterRead(pSSA2, cRegItem);
        LOG(DEBUG) << BOLDBLUE << "StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        return cSuccess;
    }
    else if(pRegName == "AnalogueAsync")
    {
        uint8_t cReadoutMode = 0x1;
        uint8_t cEdgeSel_T1  = 0x0;
        // readout mode
        bool cSuccess = this->WriteChipRegBits(pSSA2, "control_1", cReadoutMode, "mask_peri_D", 0x7);
        // edge select
        cSuccess = cSuccess && this->WriteChipRegBits(pSSA2, "control_1", (cEdgeSel_T1 << 3), "mask_peri_D", (0x1 << 3));
        // duration
        uint8_t cDuration = 0x8;
        cSuccess          = cSuccess && this->WriteChipRegBits(pSSA2, "control_2", (cDuration << 4), "mask_peri_D", (0xF << 4));

        // configure for injection with the strip register
        uint8_t cMask         = 1;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 1;
        uint8_t cDigitalCalib = 0;
        uint8_t cAnalogCalib  = pValue;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        cSuccess              = cSuccess && this->WriteChipRegBits(pSSA2, "ENFLAGS", cEnFlags, "mask_strip", 0x1F);
        // cRegItem = cRegMap["ENFLAGS_S1"];
        // auto cRegValue = fBoardFW->SingleRegisterRead(pSSA2, cRegItem);
        // LOG(INFO) << BOLDYELLOW  << "ENFLAGS_S1 set to 0x" << std::hex << +cRegValue << std::dec << RESET;
        return cSuccess;
    }
    else if(pRegName == "Sync")
    {
        uint8_t cMask         = 1;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 0;
        uint8_t cDigitalCalib = 1;
        uint8_t cAnalogCalib  = 1;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        bool    cSuccess      = this->WriteChipRegBits(pSSA2, "ENFLAGS", cEnFlags, "mask_strip", 0x1F);
        // cRegItem = cRegMap["ENFLAGS_S1"];
        // cRegValue = fBoardFW->SingleRegisterRead(pSSA2, cRegItem);
        // LOG(INFO) << BOLDYELLOW  << "ENFLAGS_S1 set to 0x" << std::hex << +cRegValue << std::dec << RESET;
        return cSuccess && this->WriteChipRegBits(pSSA2, "control_1", (1 - pValue), "mask_peri_D", 0x7);
    }
    else if(pRegName == "DigitalAsync")
    {
        uint8_t cReadoutMode = 0x1;
        uint8_t cEdgeSel_T1  = 0x0;
        // readout mode
        bool cSuccess = this->WriteChipRegBits(pSSA2, "control_1", cReadoutMode, "mask_peri_D", 0x7);
        // edge select
        cSuccess = cSuccess && this->WriteChipRegBits(pSSA2, "control_1", (cEdgeSel_T1 << 3), "mask_peri_D", (0x1 << 3));
        // duration
        uint8_t cDuration = 0x8;
        cSuccess          = cSuccess && this->WriteChipRegBits(pSSA2, "control_2", (cDuration << 4), "mask_peri_D", (0xF << 4));

        // sampling mode
        // sampling mode
        uint8_t cSamplingMode = 0;
        cSuccess              = cSuccess && this->WriteChipRegBits(pSSA2, "ENFLAGS", (cSamplingMode << 5), "mask_strip", (0x3 << 5));
        cRegItem              = cRegMap["ENFLAGS_S1"];
        auto cRegValue        = fBoardFW->SingleRegisterRead(pSSA2, cRegItem);
        LOG(DEBUG) << BOLDBLUE << "[post-set sampling] StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        // configure for injection with the strip register
        uint8_t cMask         = 0;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 0;
        uint8_t cDigitalCalib = 1;
        uint8_t cAnalogCalib  = pValue;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        cSuccess              = cSuccess && this->WriteChipRegBits(pSSA2, "ENFLAGS", cEnFlags, "mask_strip", 0x1F);
        // cRegItem = cRegMap["ENFLAGS_S1"];
        // cRegValue = fBoardFW->SingleRegisterRead(pSSA2, cRegItem);
        // LOG(INFO) << BOLDYELLOW  << "ENFLAGS_S1 set to 0x" << std::hex << +cRegValue << std::dec << RESET;
        LOG(DEBUG) << BOLDBLUE << "StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;

        return cSuccess;
    }
    else if(pRegName == "DigitalSync")
    {
        this->WriteChipReg(pSSA2, "ReadoutMode", 0);
        // configure for injection with the strip register
        uint8_t cMask         = 0;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 0;
        uint8_t cDigitalCalib = pValue;
        uint8_t cAnalogCalib  = 0;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        bool    cSuccess      = this->WriteChipRegBits(pSSA2, "ENFLAGS", cEnFlags, "mask_strip", 0x1F);
        auto    cRegItem      = cRegMap["ENFLAGS_S1"];
        auto    cRegValue     = fBoardFW->SingleRegisterRead(pSSA2, cRegItem);
        LOG(INFO) << BOLDYELLOW << "ENFLAGS_S1 set to 0x" << std::hex << +cRegValue << std::dec << RESET;
        return cSuccess;
    }
    else if(pRegName == "PulseDuration")
    {
        // edge select
        return this->WriteChipRegBits(pSSA2, "control_2", (pValue << 4), "mask_peri_D", (0xF << 4));
    }
    else if(pRegName == "EdgeSel")
    {
        // edge select
        return this->WriteChipRegBits(pSSA2, "control_1", (pValue << 3), "mask_peri_D", (0x1 << 3));
    }
    else if(pRegName.find("DigitalSync") != std::string::npos)
    {
        this->WriteChipReg(pSSA2, "ReadoutMode", 0);

        int               cStripId = 0;
        std::stringstream cRegName;
        std::sscanf(pRegName.c_str(), "DigitalSync_S%d", &cStripId);
        cRegName << "ENFLAGS_S" << (1 + cStripId);
        // LOG (INFO) << BOLDYELLOW << "Digital injection on Strip#" << +cStripId << "\t" << cRegName.str() << RESET;

        // configure for injection with the strip register
        uint8_t cMask         = 0;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 0;
        uint8_t cDigitalCalib = pValue;
        uint8_t cAnalogCalib  = 0;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        bool    cSuccess      = this->WriteChipRegBits(pSSA2, cRegName.str(), cEnFlags, "mask_strip", 0x1F);

        auto cRegItem  = pSSA2->getRegItem(cRegName.str());
        auto cRegValue = fBoardFW->SingleRegisterRead(pSSA2, cRegItem);
        LOG(INFO) << BOLDYELLOW << cRegName.str() << " set to 0x" << std::hex << +cRegValue << std::dec << RESET;
        return cSuccess;
    }
    else if(pRegName == "DigitalDuration")
    {
        bool cCalDuration = this->WriteChipRegBits(pSSA2, "control_2", (0xF << 4), "mask_peri_D", (0xF << 4));
        return cCalDuration;
    }
    else if(pRegName == "EnableSLVSTestOutput")
    {
        LOG(INFO) << BOLDBLUE << "Enabling SLVS test output on SSA2#" << +pSSA2->getId() << RESET;
        return this->WriteChipRegBits(pSSA2, "control_1", pValue << 1, "mask_peri_D", 2);
    }
    else if(pRegName.find("OutPatternStubLine") != std::string::npos) // Stub Lines
    {
        int cLine;
        std::sscanf(pRegName.c_str(), "OutPatternStubLine%d", &cLine);
        std::vector<std::string> cRegNames{"Shift_pattern_st_0",
                                           "Shift_pattern_st_1",
                                           "Shift_pattern_st_2",
                                           "Shift_pattern_st_3",
                                           "Shift_pattern_st_4_st_5",
                                           "Shift_pattern_st_4_st_5",
                                           "Shift_pattern_st_6_st_7",
                                           "Shift_pattern_st_6_st_7"};
        cRegItem        = cRegMap[cRegNames[cLine]];
        cRegItem.fValue = pValue;
        return fBoardFW->SingleRegisterWrite(pSSA2, cRegItem, pVerifLoop);
    }
    else if(pRegName.find("OutPatternL1Line") != std::string::npos) // Stub Lines
    {
        cRegItem        = cRegMap["Shift_pattern_L1"];
        cRegItem.fValue = pValue;
        return fBoardFW->SingleRegisterWrite(pSSA2, cRegItem, pVerifLoop);
        // return this->WriteChipSingleReg(pSSA2, "Shift_pattern_L1", pValue, pVerifLoop);
    }
    else if(pRegName == "CalibrationPattern")
    {
        // configure for injection with the strip register
        uint8_t cMask         = 0;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 0;
        uint8_t cDigitalCalib = 1;
        uint8_t cAnalogCalib  = pValue;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        bool    cSuccess      = this->WriteChipRegBits(pSSA2, "ENFLAGS", cEnFlags, "mask_strip", 0x1F);
        uint8_t cRegValue     = fBoardFW->SingleRegisterRead(pSSA2, cRegItem);
        LOG(DEBUG) << BOLDYELLOW << "Strip register set to 0x" << std::hex << +cRegValue << std::dec << RESET;

        cRegItem        = cRegMap["DigCalibPattern_L"];
        cRegItem.fValue = pValue;
        return cSuccess && fBoardFW->SingleRegisterWrite(pSSA2, cRegItem, pVerifLoop);
    }
    else if(pRegName.find("CalibrationPattern") != std::string::npos)
    {
        int cChannel;
        std::sscanf(pRegName.c_str(), "CalibrationPatternS%d", &cChannel);
        uint16_t cAddress = 0x0300 + cChannel;
        LOG(DEBUG) << BOLDBLUE << "Configuring register 0x" << std::hex << cAddress << std::dec << " to 0x" << std::hex << pValue << std::dec << " for channel " << +cChannel << RESET;

        // configure for injection with the strip register
        uint8_t cMask         = 0;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 0;
        uint8_t cDigitalCalib = 1;
        uint8_t cAnalogCalib  = pValue;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        bool    cSuccess      = this->WriteChipRegBits(pSSA2, "ENFLAGS", cEnFlags, "mask_strip", 0x1F);
        auto    cRegValue     = fBoardFW->SingleRegisterRead(pSSA2, cRegItem);
        LOG(DEBUG) << BOLDYELLOW << "Strip register set to 0x" << std::hex << +cRegValue << std::dec << RESET;

        cRegItem.fAddress = 0x0300 + cChannel;
        cRegItem.fValue   = pValue;
        return cSuccess && fBoardFW->SingleRegisterWrite(pSSA2, cRegItem, pVerifLoop);
    }
    else if(pRegName == "InjectedCharge")
    {
        LOG(DEBUG) << BOLDBLUE << "Setting "
                   << " bias calDac to " << +pValue << " on SSA2#" << +pSSA2->getId() << RESET;
        cRegItem        = cRegMap["Bias_CALDAC"];
        cRegItem.fValue = pValue;
        return fBoardFW->SingleRegisterWrite(pSSA2, cRegItem, pVerifLoop);
    }
    else if(pRegName == "Threshold" || pRegName == "Bias_THDAC")
    {
        // LOG (INFO) << BOLDYELLOW << "!!! Writing to " << pRegName << RESET;
        return this->WriteChipRegBits(pSSA2, "Bias_THDAC", pValue, "mask_peri_A", 0xFF);
    }
    else if(pRegName == "LateralRX_L_PhaseData")
    {
        return this->WriteChipRegBits(pSSA2, "LateralRX_sampling", pValue, "mask_peri_A", 0x07);
    }
    else if(pRegName == "LateralRX_R_PhaseData")
    {
        return this->WriteChipRegBits(pSSA2, "LateralRX_sampling", (pValue << 4), "mask_peri_A", (0x7 << 4));
    }
    else
    {
        cRegItem        = cRegMap[pRegName];
        cRegItem.fValue = pValue;
        return fBoardFW->SingleRegisterWrite(pSSA2, cRegItem, pVerifLoop);
    }
    return true;
}
//	// AMUX CONFIGURATION:
bool SSA2Interface::ConfigureAmux(Chip* pChip, const std::string& pRegister, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    auto        cRegMap = pChip->getRegMap();
    ChipRegItem cRegItem;
    // first make sure amux is set to 0 to avoid shorts
    // from SSA2 python methods
    uint8_t                  cHighZValue = 0x00;
    std::vector<std::string> cRegNames{"Bias_TEST_lsb", "Bias_TEST_msb"};
    for(auto cReg: cRegNames)
    {
        cRegItem        = cRegMap[cReg];
        cRegItem.fValue = cHighZValue;
        bool cSuccess   = fBoardFW->SingleRegisterWrite(pChip, cRegItem, pVerifLoop);
        if(!cSuccess)
            return cSuccess;
        else
            LOG(DEBUG) << BOLDBLUE << "Set " << cReg << " to 0x" << std::hex << +cHighZValue << std::dec << RESET;
    }
    if(pRegister != "HighZ")
    {
        auto cMapIterator = fAmuxMap.find(pRegister);
        if(cMapIterator != fAmuxMap.end())
        {
            uint16_t cValue = (1 << cMapIterator->second);
            LOG(DEBUG) << BOLDBLUE << "Select test_Bias 0x" << std::hex << cValue << std::dec << RESET;
            uint8_t cIndex = 0;
            for(auto cReg: cRegNames)
            {
                uint8_t cRegValue = (cValue & (0xFF << 8 * cIndex)) >> 8 * cIndex;
                cRegItem          = cRegMap[cReg];
                cRegItem.fValue   = cRegValue;

                bool cSuccess = fBoardFW->SingleRegisterWrite(pChip, cRegItem, pVerifLoop);
                if(!cSuccess)
                    return cSuccess;
                else
                    LOG(DEBUG) << BOLDBLUE << "Set " << cReg << " to 0x" << std::hex << +cRegValue << std::dec << RESET;
                cIndex++;
            }
            return true;
        }
        else
            return false;
    }
    else
        return true;
}
/////////// ALIAS CALLS:
bool SSA2Interface::enableInjection(ReadoutChip* pChip, bool inject, bool pVerifLoop) { return this->WriteChipReg(pChip, "AnalogueAsync", 1); }
bool SSA2Interface::setInjectionAmplitude(ReadoutChip* pChip, uint8_t injectionAmplitude, bool pVerifLoop) { return this->WriteChipReg(pChip, "InjectedCharge", injectionAmplitude, pVerifLoop); }
bool SSA2Interface::setInjectionSchema(ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop)
{
    auto cOriginalMask = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS>>(pChip->getChipOriginalMask());
    auto groupToMask   = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS>>(group);
    auto cBitset       = std::bitset<NSSACHANNELS>(groupToMask->getBitset() & cOriginalMask->getBitset());
    LOG(INFO) << BOLDYELLOW << "\t... Applying mask to SSA" << +pChip->getId() << " with " << group->getNumberOfEnabledChannels() << " desired mask \t... : " << cBitset
              << " original mask  \t... : " << cOriginalMask << " enabled channels "
              << " original bitset was be \t... " << groupToMask->getBitset() << RESET;

    auto                     cRegMap = pChip->getRegMap();
    std::vector<ChipRegItem> cRegItems;
    for(uint32_t cIndx = 0; cIndx < pChip->size(); cIndx++)
    {
        std::stringstream cRegName;
        cRegName << "ENFLAGS_S" << (cIndx + 1);
        cRegItems.push_back(cRegMap[cRegName.str()]);
    }
    auto   cRegValues = fBoardFW->MultiRegisterRead(pChip, cRegItems);
    size_t cIndx      = 0;
    for(auto& cReg: cRegItems)
    {
        uint16_t regval  = cReg.fValue;
        auto     shifted = std::bitset<NSSACHANNELS>(0x1) << cIndx;
        bool     bitval  = bool(((cBitset & shifted) >> cIndx).to_ulong());
        regval           = (regval & 0xEF) | (bitval << 4); // enable injection bit
        cReg.fValue      = regval;
        LOG(INFO) << BOLDYELLOW << "SSA2 - setting mask on channel#"
                  << "," << cIndx << "," << bitval << "," << regval << RESET;
        cIndx++;
    }
    return fBoardFW->MultiRegisterWrite(pChip, cRegItems, true);
}
bool SSA2Interface::maskChannelGroup(ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop)
{
    auto cOriginalMask = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS>>(pChip->getChipOriginalMask());
    auto groupToMask   = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS>>(group);
    auto cBitset       = std::bitset<NSSACHANNELS>(groupToMask->getBitset() & cOriginalMask->getBitset());
    LOG(INFO) << BOLDYELLOW << "\t... Applying mask to SSA" << +pChip->getId() << " with " << group->getNumberOfEnabledChannels() << " desired mask \t... : " << cBitset
              << " original mask  \t... : " << cOriginalMask << " enabled channels "
              << " original bitset was be \t... " << groupToMask->getBitset() << RESET;

    std::vector<std::pair<std::string, uint16_t>> pVecReq;
    pVecReq.clear();

    auto                     cRegMap = pChip->getRegMap();
    std::vector<ChipRegItem> cRegItems;
    for(uint32_t cIndx = 0; cIndx < pChip->size(); cIndx++)
    {
        std::stringstream cRegName;
        cRegName << "ENFLAGS_S" << (cIndx + 1);
        cRegItems.push_back(cRegMap[cRegName.str()]);
    }
    auto   cRegValues = fBoardFW->MultiRegisterRead(pChip, cRegItems);
    size_t cIndx      = 0;
    for(auto& cReg: cRegItems)
    {
        uint16_t regval  = cReg.fValue;
        auto     shifted = std::bitset<NSSACHANNELS>(0x1) << cIndx;
        bool     bitval  = bool(((cBitset & shifted) >> cIndx).to_ulong());
        regval           = (regval & 0xFE) | (bitval);
        cReg.fValue      = regval;
        LOG(INFO) << BOLDYELLOW << "SSA2 - setting mask on channel#"
                  << "," << cIndx << "," << bitval << "," << regval << RESET;
        cIndx++;
    }
    return fBoardFW->MultiRegisterWrite(pChip, cRegItems, true);
}
bool SSA2Interface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop)
{
    bool success = true;
    if(mask) success &= maskChannelGroup(pChip, group, pVerifLoop);
    if(inject) success &= setInjectionSchema(pChip, group, pVerifLoop);

    return success;
}
bool SSA2Interface::ConfigureChipOriginalMask(ReadoutChip* pSSA2, bool pVerifLoop, uint32_t pBlockSize) { return true; }
bool SSA2Interface::MaskAllChannels(ReadoutChip* pSSA2, bool mask, bool pVerifLoop) { return true; }
} // namespace Ph2_HwInterface
