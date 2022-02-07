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
    fTrackRegisters = false;
    this->WriteChipSingleReg(pSSA2, "mask_strip", 255, false);
    this->WriteChipSingleReg(pSSA2, "mask_peri_A", 255, false);
    this->WriteChipSingleReg(pSSA2, "mask_peri_D", 255, false);
    LOG(INFO) << BOLDBLUE << "Configuring SSA2 " << RESET;
    std::vector<uint32_t> cVec;
    ChipRegMap            cSSA2RegMap = pSSA2->getRegMap();
    bool                  cWrite      = true;
    bool                  cSuccess    = true;
    // do not overwrite these registers..
    std::vector<std::string> cRegsToSkip{"mask_strip", "mask_peri_A", "mask_peri_D"};
    for(auto& cRegItem: cSSA2RegMap)
    {
        if(std::find(cRegsToSkip.begin(), cRegsToSkip.end(), cRegItem.first) != cRegsToSkip.end()) continue;
        if(cRegItem.first.find("SEUcnt") != std::string::npos) continue;
        if(cRegItem.first.find("Ring_oscillator") != std::string::npos) continue;
        if(cRegItem.first.find("ADC_out") != std::string::npos) continue;
        if(cRegItem.first.find("bist_output") != std::string::npos) continue;

        LOG(DEBUG) << BOLDGREEN << "Encoding register " << cRegItem.first << " with address " << +cRegItem.second.fAddress << RESET;
        fBoardFW->EncodeReg(cRegItem.second, pSSA2, cVec, pVerifLoop, cWrite);
    }
    uint8_t cWriteAttempts = 0;
    cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
    if(pVerifLoop && cSuccess)
    {
        cWrite = false;
        cVec.clear();
        for(auto& cRegInMap: cSSA2RegMap) { fBoardFW->EncodeReg(cRegInMap.second, pSSA2, cVec, pVerifLoop, cWrite); }
        fBoardFW->ReadChipBlockReg(cVec);
        uint16_t                 cIndx = 0;
        std::vector<std::string> cReadOnlyRegs{"AC_ReadCounterLSB",
                                               "AC_ReadCounterMSB",
                                               "ENFLAGS",
                                               "DigCalibPattern_H",
                                               "DigCalibPattern_L",
                                               "THTRIMMING",
                                               "status_reg",
                                               "StripControl2",
                                               "mask_strip",
                                               "mask_peri_A",
                                               "mask_peri_D",
                                               "StripControl1",
                                               "StripControl2"};
        for(auto& cRegInMap: cSSA2RegMap)
        {
            uint8_t     cSSA2Id;
            bool        cFailed = false;
            bool        cRead;
            ChipRegItem cRegItem;
            fBoardFW->DecodeReg(cRegItem, cSSA2Id, cVec[cIndx], cRead, cFailed);
            if(std::find(cReadOnlyRegs.begin(), cReadOnlyRegs.end(), cRegInMap.first) == cReadOnlyRegs.end() && cRegInMap.first.find("SEUcnt") == std::string::npos &&
               cRegInMap.first.find("Ring_oscillator") == std::string::npos && cRegInMap.first.find("ADC_out") == std::string::npos && cRegInMap.first.find("bist_output") == std::string::npos)
            {
                if(cRegInMap.second.fValue != cRegItem.fValue)
                {
                    LOG(INFO) << RED << "!!!" << cRegInMap.first << "(0x" << std::hex << cRegInMap.second.fAddress << ")  should be 0x" << cRegInMap.second.fValue << ", is 0x" << cRegItem.fValue
                              << std::dec << "(SSA2:" << (int)pSSA2->getId() << ")" << RESET;
                }
                else
                    LOG(DEBUG) << BOLDGREEN << cRegInMap.first << "(0x" << std::hex << cRegInMap.second.fAddress << ")  should be 0x" << cRegInMap.second.fValue << ", is 0x" << cRegItem.fValue
                               << std::dec << "(SSA2:" << (int)pSSA2->getId() << ")" << RESET;
            }
            else
            {
                // check if _S is in the register name
                if(cRegInMap.first.find("_S") != std::string::npos && cRegInMap.first.find("SEUcnt") == std::string::npos && cRegInMap.first.find("mask_strip") == std::string::npos)
                {
                    if(cRegInMap.second.fValue != cRegItem.fValue)
                    {
                        LOG(INFO) << RED << "----" << cRegInMap.first << "(0x" << std::hex << cRegInMap.second.fAddress << ")  should be 0x" << cRegInMap.second.fValue << ", is 0x" << cRegItem.fValue
                                  << std::dec << "(SSA2:" << (int)pSSA2->getId() << ")" << RESET;
                    }
                    else
                        LOG(DEBUG) << BOLDGREEN << cRegInMap.first << "(0x" << std::hex << cRegInMap.second.fAddress << ")  should be 0x" << cRegInMap.second.fValue << ", is 0x" << cRegItem.fValue
                                   << std::dec << "(SSA2:" << (int)pSSA2->getId() << ")" << RESET;
                }
            }
            cIndx++;
        }
    }

#ifdef COUNT_FLAG
    LOG(INFO) << BOLDGREEN << "Wrote: " << +fRegisterCount << " resgisters in SSA2" << +pSSA2->getId() << " config." << RESET;
#endif
    // fTrackRegisters=false;
    return cSuccess;
}
uint16_t SSA2Interface::ReadADC(ReadoutChip* pChip, uint8_t pInput)
{
    this->WriteChipReg(pChip, "ADC_control", 0xE0 | (pInput & 0x1F));
    this->WriteChipReg(pChip, "ADC_control", 0xC0 | (pInput & 0x1F));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    uint16_t cMSB = this->ReadChipReg(pChip, "ADC_out_H");
    uint16_t cLSB = this->ReadChipReg(pChip, "ADC_out_L");
    return (cMSB << 8 | cLSB);
}
// bool SSA2Interface::ConfigureChip(Chip* pSSA2, bool pVerifLoop, uint32_t pBlockSize)
// {
// 	this->WriteChipSingleReg(pSSA2, "mask_strip", 255, pVerifLoop);
// 	this->WriteChipSingleReg(pSSA2, "mask_peri_A", 255, pVerifLoop);
// 	this->WriteChipSingleReg(pSSA2, "mask_peri_D", 255, pVerifLoop);
// 	LOG (INFO) << BOLDBLUE << "SSA2 --->" << RESET;
//     setBoard(pSSA2->getBeBoardId());
//     std::vector<uint32_t> cVec;
//     ChipRegMap            cSSA2RegMap = pSSA2->getRegMap();
//     bool                  cWrite     = true;
//     bool                  cSuccess   = true;
//     std::map<uint16_t, ChipRegItem> cMap;
//     cMap.clear();
//     for(auto& cRegInMap: cSSA2RegMap)
//     {
// 	cMap[cRegInMap.second.fAddress] = cRegInMap.second;
//     }
//     for(auto& cRegItem: cMap)
//     {
// 	fBoardFW->EncodeReg(cRegItem.second, pSSA2->getHybridId(), pSSA2->getId(), cVec, pVerifLoop, cWrite);
//     }
//     uint8_t cWriteAttempts = 0;
//     cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
//     if(pVerifLoop && cSuccess)
//     {
// 	cWrite = false;
// 	cVec.clear();
// 	for(auto& cRegInMap: cSSA2RegMap) { fBoardFW->EncodeReg(cRegInMap.second, pSSA2->getHybridId(), pSSA2->getId(), cVec, pVerifLoop, cWrite); }
// 	fBoardFW->ReadChipBlockReg(cVec);
// 	uint16_t cIndx = 0;
// 	for(auto& cRegInMap: cSSA2RegMap)
// 	{
// 	    uint8_t     cSSA2Id;
// 	    bool        cFailed = false;
// 	    bool        cRead;
// 	    ChipRegItem cRegItem;
// 	    fBoardFW->DecodeReg(cRegItem, cSSA2Id, cVec[cIndx], cRead, cFailed);
// 		if (cRegInMap.first != "ADC_out_H" and cRegInMap.first != "ADC_VREF" and cRegInMap.first != "THTRIMMING" and cRegInMap.first != "status_reg" and cRegInMap.first != "StripControl2"){
// 		    if(cRegInMap.second.fValue != cRegItem.fValue)
// 		    {	LOG (INFO) << RED << cRegInMap.first <<"(0x"<<std::hex << cRegInMap.second.fAddress <<")  should be 0x" << cRegInMap.second.fValue <<", is 0x"<<cRegItem.fValue<< std::dec << "(SSA2:"
// << (int)pSSA2->getId() <<")"<< RESET;
// 			//throw std::runtime_error(std::string("Failed to write to register ") + cRegInMap.first);
// 		    }
// 		}
// 	    //LOG (INFO) << BLUE << cRegInMap.first <<"(0x"<<std::hex << cRegInMap.second.fAddress <<")  should be 0x" << cRegInMap.second.fValue <<", is 0x"<<cRegItem.fValue<< std::dec << "(SSA2:" <<
// (int)pSSA2->getId() <<")"<< RESET; 	    cIndx++;
// 	}
//     }
//     #ifdef COUNT_FLAG
//     LOG(INFO) << BOLDGREEN << "Wrote: " << +fRegisterCount << " resgisters in SSA2" << +pSSA2->getId() << " config." << RESET;
//     #endif
//     return cSuccess;
// }
//	// READ REGISTER ON CHIP:
//////////
uint16_t SSA2Interface::ReadChipReg(Chip* pSSA2, const std::string& pRegNode)
{
    setBoard(pSSA2->getBeBoardId());
    std::vector<uint32_t> cVecReq;
    ChipRegItem           cRegItem;
    bool                  cFailed = false;
    bool                  cRead;
    uint8_t               cSSA2Id;
    if(pRegNode.find("CounterStrip") != std::string::npos)
    {
        int cChannel = 0;
        sscanf(pRegNode.c_str(), "CounterStrip%d", &cChannel);
        cRegItem.fPage    = 0x00;
        cRegItem.fAddress = 0x500 + cChannel;
        cRegItem.fValue   = 0;
        fBoardFW->EncodeReg(cRegItem, pSSA2, cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        fBoardFW->DecodeReg(cRegItem, cSSA2Id, cVecReq[0], cRead, cFailed);
        if(!cFailed)
        {
            uint8_t cRPLSB = cRegItem.fValue & 0xFF;
            cVecReq.clear();
            cRegItem.fPage    = 0x00;
            cRegItem.fAddress = 0x600 + cChannel;
            cRegItem.fValue   = 0;
            fBoardFW->EncodeReg(cRegItem, pSSA2, cVecReq, true, false);
            fBoardFW->ReadChipBlockReg(cVecReq);
            // bools to find the values of failed and read
            fBoardFW->DecodeReg(cRegItem, cSSA2Id, cVecReq[0], cRead, cFailed);
            uint8_t cRPMSB = cRegItem.fValue & 0xFF;
            if(!cFailed)
            {
                cVecReq.clear();
                uint16_t cCounterValue = (cRPMSB << 8) | cRPLSB;
                LOG(INFO) << BOLDBLUE << "Counter MSB is 0x" << std::bitset<8>(cRPMSB) << " Counter LSB is 0x" << std::bitset<8>(cRPLSB) << " Counter value is " << std::hex << +cCounterValue
                          << std::dec << RESET;
                return cCounterValue;
            }
            else
            {
                throw std::runtime_error(std::string("Failed to read strip counter register from SSA2 "));
                return 0;
            }
        }
        else
        {
            throw std::runtime_error(std::string("Failed to read strip counter register from SSA2 "));
            return 0;
        }
    }
    else if(pRegNode == "ChipId")
    {
        return this->ReadChipId(pSSA2);
    }
    else if(pRegNode == "Threshold")
    {
        return this->ReadChipReg(pSSA2, "Bias_THDAC");
    }
    else
    {
        cRegItem = pSSA2->getRegItem(pRegNode);
        fBoardFW->EncodeReg(cRegItem, pSSA2, cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        fBoardFW->DecodeReg(cRegItem, cSSA2Id, cVecReq[0], cRead, cFailed);
        if(!cFailed)
        {
            LOG(DEBUG) << BOLDRED << "Reading " << pRegNode << " from SSA#" << +pSSA2->getId() << " value is 0x" << std::hex << cRegItem.fValue << std::dec << RESET;
            pSSA2->setReg(pRegNode, cRegItem.fValue);
        }
        else
            LOG(ERROR) << BOLDRED << "\t.. Reading " << pRegNode << " from SSA#" << +pSSA2->getId() << " value is 0x" << std::hex << cRegItem.fValue << std::dec << RESET;
        return cRegItem.fValue & 0xFF;
    }
}
//	// READ ASYNC EVENT (SLOW):
//////////
void SSA2Interface::ReadASEvent(ReadoutChip* pSSA2, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange)
{
    if(pSRange == std::pair<uint32_t, uint32_t>{0, 0}) pSRange = std::pair<uint32_t, uint32_t>{1, pSSA2->getNumberOfChannels()};
    for(uint32_t i = pSRange.first; i <= pSRange.second; i++)
    {
        char cRegName[100];
        std::sprintf(cRegName, "CounterStrip%d", static_cast<int>(i));
        pData.push_back(this->ReadChipReg(pSSA2, cRegName));
    }
}
//	// READ CHIP ID:
//////////
uint8_t SSA2Interface::ReadChipId(Chip* pChip)
{
    bool cVerifLoop = true;
    if(!this->WriteChipSingleReg(pChip, "Fuse_Mode", 0x0F, cVerifLoop)) // FIXME What is this even for? Do we really need this?
    { return 0; }
    else
        throw std::runtime_error(std::string("Failed to start e-fuse read operation from SSA2 ") + std::to_string(pChip->getId()));
}
//	// WRITE REGISTER (ALL LOCAL):
//////////
bool SSA2Interface::WriteChipAllLocalReg(ReadoutChip* pChip, const std::string& dacName, ChipContainer& localRegValues, bool pVerifLoop) // FIXME SSA2
{
    assert(localRegValues.size() == pChip->getNumberOfChannels());
    std::string dacTemplate;
    // bool isMask = false;

    bool cSuccess = true;
    if(dacName == "GainTrim")
    {
        dacTemplate = "GAINTRIMMING_S%d";
        this->WriteChipSingleReg(pChip, "mask_strip", 120, pVerifLoop);
        std::vector<std::pair<std::string, uint16_t>> cRegVec;
        ChannelGroup<NCHANNELS, 1>                    channelToEnable;

        std::vector<uint32_t> cVec;
        cVec.clear();
        for(uint8_t iChannel = 0; iChannel < pChip->getNumberOfChannels(); ++iChannel)
        {
            char dacName1[20];
            sprintf(dacName1, dacTemplate.c_str(), 1 + iChannel);
            LOG(DEBUG) << BOLDBLUE << "Setting register " << dacName1 << " to " << (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F) << RESET;
            cSuccess = cSuccess && this->WriteChipSingleReg(pChip, dacName1, ((localRegValues.getChannel<uint16_t>(iChannel) << 3) & 0x1F), pVerifLoop);
        }
        this->WriteChipSingleReg(pChip, "mask_strip", 255, pVerifLoop);
    }
    else if(dacName == "ThresholdTrim")
    {
        dacTemplate = "THTRIMMING_S%d";
        std::vector<std::pair<std::string, uint16_t>> cRegVec;
        ChannelGroup<NCHANNELS, 1>                    channelToEnable;

        std::vector<uint32_t> cVec;
        cVec.clear();
        for(uint8_t iChannel = 0; iChannel < pChip->getNumberOfChannels(); ++iChannel)
        {
            char dacName1[20];
            sprintf(dacName1, dacTemplate.c_str(), 1 + iChannel);
            LOG(DEBUG) << BOLDBLUE << "Setting register " << dacName1 << " to " << (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F) << RESET;
            cSuccess = cSuccess && this->WriteChipSingleReg(pChip, dacName1, (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F), pVerifLoop);
        }
    }
    else
        LOG(ERROR) << "Error, DAC " << dacName << " is not a Local DAC";
    return cSuccess;
}
//	// WRITE REGISTER (SIMPLE):
//////////
bool SSA2Interface::WriteReg(Chip* pChip, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerifLoop)
{
    std::vector<uint32_t> cVec;
    ChipRegItem           cRegItem;
    cRegItem.fPage    = 0x00;
    cRegItem.fAddress = pRegisterAddress;
    cRegItem.fValue   = pRegisterValue & 0xFF;
    fBoardFW->EncodeReg(cRegItem, pChip, cVec, pVerifLoop, true);
    uint8_t cWriteAttempts = 0;
    return fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
}
//	// WRITE REGISTER (MULTI):
//////////
bool SSA2Interface::WriteChipMultReg(Chip* pSSA2, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop)
{
    setBoard(pSSA2->getBeBoardId());
    std::vector<uint32_t> cVec;
    ChipRegItem           cRegItem;
    for(const auto& cReg: pVecReq)
    {
        cRegItem        = pSSA2->getRegItem(cReg.first);
        cRegItem.fValue = cReg.second;
        fBoardFW->EncodeReg(cRegItem, pSSA2, cVec, pVerifLoop, true);
#ifdef COUNT_FLAG
        fRegisterCount++;
#endif
    }
    uint8_t cWriteAttempts = 0;
    bool    cSuccess       = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
#ifdef COUNT_FLAG
    fTransactionCount++;
#endif
    if(cSuccess)
    {
        for(const auto& cReg: pVecReq)
        {
            cRegItem = pSSA2->getRegItem(cReg.first);
            pSSA2->setReg(cReg.first, cReg.second);
        }
    }
    return cSuccess;
}
//	// WRITE REGISTER (SINGLE WITH VERIFY):
//////////
bool SSA2Interface::WriteChipSingleReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    std::vector<uint32_t> cVec;
    ChipRegItem           cRegItem = pChip->getRegItem(pRegNode);
    cRegItem.fValue                = pValue & 0xFF;
    fBoardFW->EncodeReg(cRegItem, pChip, cVec, pVerifLoop, true);
    uint8_t cWriteAttempts = 0;
    bool    cSuccess       = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
    if(cSuccess)
    {
        // bug for mask strip
        if(pRegNode == "mask_strip")
            pChip->setReg(pRegNode, pValue);
        else if(pVerifLoop)
        {
            auto cReadBack = ReadChipReg(pChip, pRegNode);
            if(cReadBack != pValue)
            {
                LOG(INFO) << BOLDRED << "Read back value (" << cReadBack << ") from " << pRegNode << BOLDBLUE << " at I2C address 0x" << std::hex << cRegItem.fAddress << std::dec
                          << " not equal to write value of " << std::hex << +cRegItem.fValue << std::dec << RESET;
                return false;
            }
            else
                pChip->setReg(pRegNode, pValue);
        }
        else
            pChip->setReg(pRegNode, pValue);
    }
#ifdef COUNT_FLAG
    fRegisterCount++;
    fTransactionCount++;
#endif
    return cSuccess;
}

bool SSA2Interface::WriteChipRegBits(Chip* pSSA2, const std::string& pRegNode, uint16_t pValue, const std::string& pMaskReg, uint8_t mask, bool pVerifLoop)
{
    this->WriteChipSingleReg(pSSA2, pMaskReg, mask, pVerifLoop);
    bool cReadoutMode = WriteChipSingleReg(pSSA2, pRegNode, pValue, pVerifLoop);
    this->WriteChipSingleReg(pSSA2, pMaskReg, 0xFF, pVerifLoop);
    return cReadoutMode;
}
//	// WRITE REGISTER (SINGLE CHIP REG <<main interface for writing>>):
//////////
bool SSA2Interface::WriteChipReg(Chip* pSSA2, const std::string& pRegName, uint16_t pValue, bool pVerifLoop)
{
    if(pRegName == "CountingMode")
    {
        uint8_t cRegValue = (pValue << 2) | (1 << 0);
        return WriteChipSingleReg(pSSA2, "ENFLAGS", cRegValue, pVerifLoop);
    }
    else if(pRegName == "AmuxHigh")
    {
        return this->ConfigureAmux(pSSA2, "HighZ");
    }
    // else if(pRegName == "inject")
    // {
    // 	uint8_t cRegValue       = (pValue << 4) | (0 << 2) | (1 << 0);
    // 	bool    cEnableAnalogue = WriteChipSingleReg(pSSA2, "ENFLAGS", cRegValue, pVerifLoop);
    // 	return cEnableAnalogue;
    // }
    else if(fAmuxMap.find(pRegName) != fAmuxMap.end())
    {
        return this->ConfigureAmux(pSSA2, pRegName);
    }
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
        bool cReadoutMode = this->WriteChipRegBits(pSSA2, "control_1", pValue, "mask_peri_D", 7, pVerifLoop);

        // this->WriteChipSingleReg(pSSA2, "mask_peri_D", 7, pVerifLoop);
        // bool cReadoutMode = WriteChipSingleReg(pSSA2, "control_1", pValue, pVerifLoop);
        // this->WriteChipSingleReg(pSSA2, "mask_peri_D", 255, pVerifLoop);
        return cReadoutMode;
    }
    else if(pRegName == "SamplingMode")
    {
        bool cReadoutMode = this->WriteChipRegBits(pSSA2, "control_1", pValue << 5, "mask_strip", 0x60, pVerifLoop);

        // this->WriteChipSingleReg(pSSA2, "mask_strip", 0x60, pVerifLoop);
        // bool cReadoutMode = WriteChipSingleReg(pSSA2, "control_1", pValue << 5, pVerifLoop);
        // this->WriteChipSingleReg(pSSA2, "mask_strip", 0xFF, pVerifLoop);
        return cReadoutMode;
    }
    else if(pRegName == "inject")
    {
        return true;
        /*
        bool cReadoutMode       = WriteChipSingleReg(pSSA2, "control_1", 0x0, pVerifLoop);
        bool cReadoutMode2 = WriteChipSingleReg(pSSA2, "ENFLAGS", 0x31, pVerifLoop);
        return cReadoutMode && cReadoutMode2;*/
    }
    else if(pRegName == "TriggerLatency")
    {
        bool cReadoutMode = this->WriteChipRegBits(pSSA2, "control_3", pValue & 0xFF, "mask_peri_D", 0xFF, pVerifLoop);
        cReadoutMode &= this->WriteChipRegBits(pSSA2, "control_1", (pValue & 0x100 >> 8), "mask_peri_D", 0x10, pVerifLoop);

        // this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, false);
        // bool cReadoutMode = WriteChipSingleReg(pSSA2, "control_3", pValue & 0xFF, pVerifLoop);
        // this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0x10, false);
        // cReadoutMode &= WriteChipSingleReg(pSSA2, "control_1", (pValue & 0x100 >> 8), pVerifLoop);

        // mask not reset??

        return cReadoutMode;
    }
    else if(pRegName == "DigitalSync")
    {
        uint8_t cReadoutMode = 0x0;
        uint8_t cEdgeSel_T1  = 0x0;
        // readout mode
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0x7, pVerifLoop);
        uint16_t cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write ReadoutMode] Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "control_1", cReadoutMode, false);
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);
        // edge select
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write Edge] Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", (0x1 << 3), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "control_1", (cEdgeSel_T1 << 3), false);
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);

        uint8_t cDuration = 0x1;
        cRegValue         = this->ReadChipReg(pSSA2, "control_2");
        LOG(DEBUG) << BOLDBLUE << "[pre-write Duration] Control_2 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", (0xF << 4), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "control_2", (cDuration << 4), false);
        cRegValue = this->ReadChipReg(pSSA2, "control_2");
        LOG(DEBUG) << BOLDBLUE << "Control_2 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);

        // sampling mode
        uint8_t cSamplingMode = 0;
        cRegValue             = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write] StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_strip", (0x3 << 5), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "ENFLAGS", (cSamplingMode << 5), false);
        this->WriteChipSingleReg(pSSA2, "mask_strip", 0xFF, pVerifLoop);
        cRegValue = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "[post-set sampling] StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        // configure for injection with the strip register
        uint8_t cMask         = 0;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 0;
        uint8_t cDigitalCalib = pValue;
        uint8_t cAnalogCalib  = 0;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        this->WriteChipSingleReg(pSSA2, "mask_strip", 0x1F, pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "ENFLAGS", cEnFlags, false);
        bool cSuccess = this->WriteChipSingleReg(pSSA2, "mask_strip", 0xFF, pVerifLoop);
        cRegValue     = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;

        return cSuccess;
    }
    else if(pRegName == "AsyncDelay")
    {
        uint8_t cLSB = pValue & 0xFF;
        uint8_t cMSB = (pValue << 8);
        WriteChipSingleReg(pSSA2, "AsyncRead_StartDel_LSB", cLSB, pVerifLoop);
        WriteChipSingleReg(pSSA2, "AsyncRead_StartDel_MSB", cMSB, pVerifLoop);
        return true;
    }
    else if(pRegName == "AnalogueSync")
    {
        uint8_t cReadoutMode = 0x0;
        uint8_t cEdgeSel_T1  = 0x0;
        // readout mode
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0x7, pVerifLoop);
        uint16_t cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write ReadoutMode] Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "control_1", cReadoutMode, false);
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);
        // edge select
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write Edge] Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", (0x1 << 3), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "control_1", (cEdgeSel_T1 << 3), false);
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);

        uint8_t cDuration = 0x8;
        cRegValue         = this->ReadChipReg(pSSA2, "control_2");
        LOG(DEBUG) << BOLDBLUE << "[pre-write Duration] Control_2 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", (0xF << 4), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "control_2", (cDuration << 4), false);
        cRegValue = this->ReadChipReg(pSSA2, "control_2");
        LOG(DEBUG) << BOLDBLUE << "Control_2 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);

        // sampling mode
        uint8_t cSamplingMode = 0;
        cRegValue             = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write] StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_strip", (0x3 << 5), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "ENFLAGS", (cSamplingMode << 5), false);
        this->WriteChipSingleReg(pSSA2, "mask_strip", 0xFF, pVerifLoop);
        cRegValue = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "[post-set sampling] StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        // configure for injection with the strip register
        uint8_t cMask         = 1;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 0;
        uint8_t cDigitalCalib = 0;
        uint8_t cAnalogCalib  = pValue;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        this->WriteChipSingleReg(pSSA2, "mask_strip", 0x1F, pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "ENFLAGS", cEnFlags, false);
        bool cSuccess = this->WriteChipSingleReg(pSSA2, "mask_strip", 0xFF, pVerifLoop);
        cRegValue     = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;

        return cSuccess;
    }
    else if(pRegName == "AnalogueAsync")
    // cTool.fReadoutChipInterface->WriteChipReg(theSSA, "control_1", 0x1);
    //  cTool.fReadoutChipInterface->WriteChipReg(theSSA, "mask_strip", 0xff);
    //	cTool.fReadoutChipInterface->WriteChipReg(theSSA, "ENFLAGS", 0x15);
    {
        // NOTE:assume mask always 0xFF?
        this->WriteChipSingleReg(pSSA2, "mask_strip", 0x1F, pVerifLoop);
        bool cEnableAnalogue = WriteChipSingleReg(pSSA2, "ENFLAGS", 0x15, false);
        this->WriteChipSingleReg(pSSA2, "mask_strip", 0x07, pVerifLoop);
        bool cReadoutMode = WriteChipSingleReg(pSSA2, "control_1", 0x1, pVerifLoop);
        /*
        uint8_t cReadoutMode = 0x1;
        uint8_t cEdgeSel_T1  = 0x0;
        // readout mode
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0x7, pVerifLoop);
        uint16_t cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write ReadoutMode] Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "control_1", cReadoutMode, false);
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);
        // edge select
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write Edge] Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", (0x1 << 3), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "control_1", (cEdgeSel_T1 << 3), false);
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);

        uint8_t cDuration = 0x8;
        cRegValue         = this->ReadChipReg(pSSA2, "control_2");
        LOG(DEBUG) << BOLDBLUE << "[pre-write Duration] Control_2 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", (0xF << 4), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "control_2", (cDuration << 4), false);
        cRegValue = this->ReadChipReg(pSSA2, "control_2");
        LOG(DEBUG) << BOLDBLUE << "Control_2 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);

        // sampling mode
        uint8_t cSamplingMode = 0;
        cRegValue             = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write] StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_strip", (0x3 << 5), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "ENFLAGS", (cSamplingMode << 5), false);
        this->WriteChipSingleReg(pSSA2, "mask_strip", 0xFF, pVerifLoop);
        cRegValue = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "[post-set sampling] StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        // configure for injection with the strip register
        uint8_t cMask         = 1;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 1;
        uint8_t cDigitalCalib = 0;
        uint8_t cAnalogCalib  = pValue;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        this->WriteChipSingleReg(pSSA2, "mask_strip", 0x1F, pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "ENFLAGS", cEnFlags, false);
        bool cSuccess = this->WriteChipSingleReg(pSSA2, "mask_strip", 0xFF, pVerifLoop);
        cRegValue     = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;

        // set readout delay
        cRegValue = this->ReadChipReg(pSSA2, "AsyncRead_StartDel_MSB");
        LOG(DEBUG) << BOLDBLUE << "[pre-write] AsyncRead_StartDel_MSB set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "AsyncRead_StartDel_MSB", 0, false);
        cRegValue = this->ReadChipReg(pSSA2, "AsyncRead_StartDel_LSB");
        LOG(DEBUG) << BOLDBLUE << "[pre-write] AsyncRead_StartDel_LSB set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "AsyncRead_StartDel_LSB", 0x8, false);
        */
        return cEnableAnalogue && cReadoutMode;
    }
    else if(pRegName == "Sync")
    {
        uint8_t pAnalogueCalib  = 1;
        uint8_t pDigitalCalib   = 1;
        uint8_t pHitCounter     = 0;
        uint8_t pSignalPolarity = 0;
        uint8_t pStripEnable    = 1;
        uint8_t cRegValue       = (pAnalogueCalib << 4) | (pDigitalCalib << 3) | (pHitCounter << 2) | (pSignalPolarity << 1);
        cRegValue               = cRegValue | (pStripEnable << 0);
        LOG(INFO) << BOLDRED << "Enable flag is 0x" << std::hex << +cRegValue << std::dec << RESET;
        bool cEnableAnalogue = WriteChipSingleReg(pSSA2, "ENFLAGS", cRegValue, pVerifLoop);

        bool cReadoutMode = this->WriteChipRegBits(pSSA2, "control_1", (1 - pValue), "mask_strip", 0x1, pVerifLoop);

        // this->WriteChipSingleReg(pSSA2, "mask_strip", 1, pVerifLoop);
        // bool cReadoutMode = WriteChipSingleReg(pSSA2, "control_1", (1 - pValue), pVerifLoop);
        // this->WriteChipSingleReg(pSSA2, "mask_strip", 255, pVerifLoop);

        return cEnableAnalogue && cReadoutMode;
    }
    else if(pRegName == "DigitalAsync")
    {
        // // digital injection, async , enable all strips
        // uint8_t cRegValue = (pValue << 3) | (1 << 2) | (1 << 0);
        // return WriteChipSingleReg(pSSA2, "ENFLAGS", cRegValue, pVerifLoop);

        uint8_t cReadoutMode = 0x1;
        uint8_t cEdgeSel_T1  = 0x0;
        // readout mode
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0x7, pVerifLoop);
        uint16_t cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write ReadoutMode] Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "control_1", cReadoutMode, false);
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);
        // edge select
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write Edge] Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", (0x1 << 3), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "control_1", (cEdgeSel_T1 << 3), false);
        cRegValue = this->ReadChipReg(pSSA2, "control_1");
        LOG(DEBUG) << BOLDBLUE << "Control_1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);

        uint8_t cDuration = 0x8;
        cRegValue         = this->ReadChipReg(pSSA2, "control_2");
        LOG(DEBUG) << BOLDBLUE << "[pre-write Duration] Control_2 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", (0xF << 4), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "control_2", (cDuration << 4), false);
        cRegValue = this->ReadChipReg(pSSA2, "control_2");
        LOG(DEBUG) << BOLDBLUE << "Control_2 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0xFF, pVerifLoop);

        // sampling mode
        uint8_t cSamplingMode = 0;
        cRegValue             = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "[pre-write] StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_strip", (0x3 << 5), pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "ENFLAGS", (cSamplingMode << 5), false);
        this->WriteChipSingleReg(pSSA2, "mask_strip", 0xFF, pVerifLoop);
        cRegValue = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "[post-set sampling] StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;
        // configure for injection with the strip register
        uint8_t cMask         = 1;
        uint8_t cPolarity     = 0;
        uint8_t cHitCounter   = 1;
        uint8_t cDigitalCalib = pValue;
        uint8_t cAnalogCalib  = 0;
        uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
        this->WriteChipSingleReg(pSSA2, "mask_strip", 0x1F, pVerifLoop);
        this->WriteChipSingleReg(pSSA2, "ENFLAGS", cEnFlags, false);
        bool cSuccess = this->WriteChipSingleReg(pSSA2, "mask_strip", 0xFF, pVerifLoop);
        cRegValue     = this->ReadChipReg(pSSA2, "ENFLAGS_S1");
        LOG(DEBUG) << BOLDBLUE << "StripControl1 set to 0x" << std::hex << cRegValue << std::dec << RESET;

        return cSuccess;
    }
    else if(pRegName == "DigitalSync")
    {
        uint8_t cRegValue = (pValue << 3) | (1 << 2) | (1 << 0);

        // NOTE:assume mask always 0xFF?
        // WriteChipReg(pSSA2, "mask_strip", 0xff, false);
        bool cEnFlags = WriteChipSingleReg(pSSA2, "ENFLAGS", cRegValue, pVerifLoop);
        bool cMode    = WriteChipReg(pSSA2, "ReadoutMode", 0x0, pVerifLoop);
        return cEnFlags && cMode;
    }
    else if(pRegName == "DigitalDuration")
    {
        bool cCalDuration = this->WriteChipRegBits(pSSA2, "control_2", (0xF << 4), "mask_peri_D", (0xF << 4), pVerifLoop);

        // this->WriteChipSingleReg(pSSA2, "mask_peri_D", (0xF << 4), false);
        // bool cCalDuration = WriteChipSingleReg(pSSA2, "control_2", (0xF << 4), false);
        // this->WriteChipSingleReg(pSSA2, "mask_peri_D", 255, false);
        return cCalDuration;
    }
    else if(pRegName == "EnableSLVSTestOutput")
    {
        if(pValue == 1)
            LOG(INFO) << BOLDBLUE << "Enabling SLVS test output on SSA2#" << +pSSA2->getId() << RESET;
        else if(pValue == 0)
            LOG(INFO) << BOLDBLUE << "Disabling SLVS test output on SSA2#" << +pSSA2->getId() << RESET;
        uint8_t cRegValue = ReadChipReg(pSSA2, "control_1");
        cRegValue         = (cRegValue & 0x4) | (pValue << 1); // What does this do?
        pVerifLoop        = false;
        bool cReadoutMode = this->WriteChipRegBits(pSSA2, "control_1", pValue << 1, "mask_peri_D", 2, pVerifLoop);
        return cReadoutMode;
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
        return this->WriteChipSingleReg(pSSA2, cRegNames[cLine], pValue, pVerifLoop);
    }
    else if(pRegName.find("OutPatternL1Line") != std::string::npos) // Stub Lines
    {
        return this->WriteChipSingleReg(pSSA2, "Shift_pattern_L1", pValue, pVerifLoop);
    }
    else if(pRegName == "CalibrationPattern")
    {
        uint8_t pAnalogueCalib  = 0;
        uint8_t pDigitalCalib   = 1;
        uint8_t pHitCounter     = 0;
        uint8_t pSignalPolarity = 0;
        uint8_t pStripEnable    = 1;
        uint8_t cRegValue       = (pAnalogueCalib << 4) | (pDigitalCalib << 3) | (pHitCounter << 2) | (pSignalPolarity << 1);
        cRegValue               = cRegValue | (pStripEnable << 0);
        bool cEnableAnalogue    = WriteChipSingleReg(pSSA2, "ENFLAGS", cRegValue, pVerifLoop);
        if(cEnableAnalogue)
            return WriteChipSingleReg(pSSA2, "DigCalibPattern_L", pValue, pVerifLoop);
        else
            return cEnableAnalogue;
    }
    else if(pRegName.find("CalibrationPattern") != std::string::npos)
    {
        int cChannel;
        std::sscanf(pRegName.c_str(), "CalibrationPatternS%d", &cChannel);
        uint16_t cAddress = 0x0300 + cChannel;
        LOG(DEBUG) << BOLDBLUE << "Configuring register 0x" << std::hex << cAddress << std::dec << " to 0x" << std::hex << pValue << std::dec << " for channel " << +cChannel << RESET;

        uint8_t pAnalogueCalib  = 0;
        uint8_t pDigitalCalib   = 1;
        uint8_t pHitCounter     = 0;
        uint8_t pSignalPolarity = 0;
        uint8_t pStripEnable    = 1;
        uint8_t cRegValue       = (pAnalogueCalib << 4) | (pDigitalCalib << 3) | (pHitCounter << 2) | (pSignalPolarity << 1);
        cRegValue               = cRegValue | (pStripEnable << 0);
        bool cEnableAnalogue    = WriteChipSingleReg(pSSA2, "ENFLAGS", cRegValue, pVerifLoop);
        if(cEnableAnalogue)
            return this->WriteReg(pSSA2, cAddress, pValue, pVerifLoop);
        else
            return cEnableAnalogue;
    }
    else if(pRegName == "InjectedCharge")
    {
        LOG(DEBUG) << BOLDBLUE << "Setting "
                   << " bias calDac to " << +pValue << " on SSA2#" << +pSSA2->getId() << RESET;
        return WriteChipSingleReg(pSSA2, "Bias_CALDAC", pValue, pVerifLoop);
    }
    else if(pRegName == "Threshold" || pRegName == "Bias_THDAC")
    {
        LOG(DEBUG) << BOLDBLUE << "Setting threshold to " << +pValue << RESET;
        this->WriteChipSingleReg(pSSA2, "mask_peri_A", 0xFF, false);
        return WriteChipSingleReg(pSSA2, "Bias_THDAC", (pValue), pVerifLoop);
    }
    else if(pRegName == "LateralRX_L_PhaseData")
    {
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0x07, false); // 0b00000111
        return this->WriteChipSingleReg(pSSA2, "LateralRX_sampling", (pValue), pVerifLoop);
    }
    else if(pRegName == "LateralRX_R_PhaseData")
    {
        this->WriteChipSingleReg(pSSA2, "mask_peri_D", 0x70, false); // 0b01110000
        return this->WriteChipSingleReg(pSSA2, "LateralRX_sampling", (pValue), pVerifLoop);
    }
    else
    {
        return this->WriteChipSingleReg(pSSA2, pRegName, pValue, pVerifLoop);
    }
}
//	// AMUX CONFIGURATION:
//////////
bool SSA2Interface::ConfigureAmux(Chip* pChip, const std::string& pRegister)
{
    // first make sure amux is set to 0 to avoid shorts
    // from SSA2 python methods
    uint8_t                  cHighZValue = 0x00;
    std::vector<std::string> cRegNames{"Bias_TEST_lsb", "Bias_TEST_msb"};
    for(auto cReg: cRegNames)
    {
        bool cSuccess = this->WriteChipSingleReg(pChip, cReg, cHighZValue);
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
                bool    cSuccess  = this->WriteChipSingleReg(pChip, cReg, cRegValue);
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
bool SSA2Interface::enableInjection(ReadoutChip* pChip, bool inject, bool pVerifLoop) { return this->WriteChipReg(pChip, "inject", 1); }
bool SSA2Interface::setInjectionAmplitude(ReadoutChip* pChip, uint8_t injectionAmplitude, bool pVerifLoop) { return this->WriteChipReg(pChip, "InjectedCharge", injectionAmplitude, pVerifLoop); }
/////////// SPOOFING OVERRIDES:
bool SSA2Interface::setInjectionSchema(ReadoutChip* pSSA2, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop) { return true; }
bool SSA2Interface::maskChannelGroup(ReadoutChip* pSSA2, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop) { return true; }
bool SSA2Interface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop) { return true; }
bool SSA2Interface::ConfigureChipOriginalMask(ReadoutChip* pSSA2, bool pVerifLoop, uint32_t pBlockSize) { return true; }
bool SSA2Interface::MaskAllChannels(ReadoutChip* pSSA2, bool mask, bool pVerifLoop) { return true; }
} // namespace Ph2_HwInterface
