/*!

        \file                                            SSAInterface.cc
        \brief                                           User Interface to the SSAs
        \author                                          Marc Osherson
        \version                                         1.0
        \date                        31/07/19
        Support :                    mail to : oshersonmarc@gmail.com

 */

#include "SSAInterface.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Container.h"
#include <bitset>
#include <numeric>

using namespace Ph2_HwDescription;

#define DEV_FLAG 0
namespace Ph2_HwInterface
{ // start namespace
SSAInterface::SSAInterface(const BeBoardFWMap& pBoardMap) : ReadoutChipInterface(pBoardMap) {}
SSAInterface::~SSAInterface() {}

bool SSAInterface::ConfigureChip(Chip* pSSA, bool pVerify, uint32_t pBlockSize)
{
    bool cConfigLocalRegs = true;
    // for now ..
    std::stringstream cOutput;
    setBoard(pSSA->getBeBoardId());
    pSSA->printChipType(cOutput);
    LOG(INFO) << BOLDBLUE << cOutput.str() << "...Configuring chip with Id[" << +pSSA->getId() << "]" << RESET;
    pSSA->setRegisterTracking(0);

    std::vector<uint32_t> cVec;
    ChipRegMap            cSSARegMap = pSSA->getRegMap();
    // need to split between control and enable registers
    // don't read back enable registers
    std::vector<ChipRegItem> cCntrlRegItems;
    std::vector<ChipRegItem> cRegItems;
    std::vector<ChipRegItem> cLocalRegItems;
    cCntrlRegItems.clear();
    for(auto cMapItem: cSSARegMap)
    {
        if(cMapItem.second.fControlReg == 0x1) { cCntrlRegItems.push_back(cMapItem.second); }
        else if((cMapItem.first.find("_S") != std::string::npos) && (cMapItem.first.find("SampleEdge") == std::string::npos) && (cMapItem.first.find("AsyncRead") == std::string::npos))
            cLocalRegItems.push_back(cMapItem.second);
        else
            cRegItems.push_back(cMapItem.second);
    }
    // cntrl
    bool cSuccess = fBoardFW->MultiRegisterWrite(pSSA, cCntrlRegItems, false);
    if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cCntrlRegItems.size() << " control registers in SSA#" << +pSSA->getId() << RESET;
    // lcl
    if(cConfigLocalRegs)
    {
        cSuccess = fBoardFW->MultiRegisterWrite(pSSA, cLocalRegItems, pVerify);
        if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cLocalRegItems.size() << " local R/W registers in SSA#" << +pSSA->getId() << RESET;
    }
    // glbl
    cSuccess = fBoardFW->MultiRegisterWrite(pSSA, cRegItems, false);
    if(cSuccess) LOG(INFO) << BOLDGREEN << "Wrote " << cRegItems.size() << " R/W registers in SSA#" << +pSSA->getId() << RESET;

    pSSA->setRegisterTracking(1);
    return cSuccess;
}
void SSAInterface::producePhaseAlignmentPattern(ReadoutChip* pChip, uint8_t pWait_ms)
{
    LOG(INFO) << GREEN << "SSA Alignment" << RESET;
    this->WriteChipReg(pChip, "ReadoutMode", 2);
    uint8_t cAlignmentPattern = 0xAA;
    for(uint8_t cLineId = 0; cLineId < 8; cLineId++)
    {
        std::stringstream cRegName;
        if(cLineId < 7)
            cRegName << "OutPattern" << +cLineId;
        else
            cRegName << "OutPattern7/FIFOconfig";
        this->WriteChipReg(pChip, cRegName.str(), cAlignmentPattern);
    }
}
void SSAInterface::produceWordAlignmentPattern(ReadoutChip* pChip)
{
    LOG(INFO) << GREEN << "SSA Word Alignment" << RESET;
    this->WriteChipReg(pChip, "ReadoutMode", 2);
    uint8_t cAlignmentPattern = 0xEA;
    for(uint8_t cLineId = 0; cLineId < 8; cLineId++)
    {
        std::stringstream cRegName;
        if(cLineId < 7)
            cRegName << "OutPattern" << +cLineId;
        else
            cRegName << "OutPattern7/FIFOconfig";
        this->WriteChipReg(pChip, cRegName.str(), cAlignmentPattern);
    }
}
bool SSAInterface::enableInjection(ReadoutChip* pChip, bool inject, bool pVerify)
{
    // for now always with asynchronous mode
    return this->WriteChipReg(pChip, "AnalogueAsync", 1);
}
bool SSAInterface::setInjectionAmplitude(ReadoutChip* pChip, uint8_t injectionAmplitude, bool pVerify) { return this->WriteChipReg(pChip, "InjectedCharge", injectionAmplitude, pVerify); }

//

//
bool SSAInterface::setInjectionSchema(ReadoutChip* cChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerify)
{
    auto cOriginalMask = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS>>(cChip->getChipOriginalMask());
    auto groupToMask   = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS>>(group);

    auto cBitset = std::bitset<NSSACHANNELS>(groupToMask->getBitset() & cOriginalMask->getBitset());
    // cBitset = cBitset&std::bitset<NSSACHANNELS>(0x0000F0FF0);
    LOG(DEBUG) << BOLDBLUE << "\t... Applying mask to MPA" << +cChip->getId() << " with " << group->getNumberOfEnabledChannels() << " desired mask \t... : " << cBitset
               << " original mask  \t... : " << cOriginalMask << " enabled channels "
               << " original bitset was be \t... " << groupToMask->getBitset() << RESET;

    std::vector<std::pair<std::string, uint16_t>> pVecReq;
    pVecReq.clear();

    for(uint32_t ipix = 0; ipix < (NSSACHANNELS); ipix++)
    {
        auto shifted = std::bitset<NSSACHANNELS>(0x1) << ipix;
        bool bitval  = bool(((cBitset & shifted) >> ipix).to_ulong());

        std::pair<std::string, uint16_t> Req;
        uint32_t                         cPixelIds = ipix;
        std::ostringstream               cRegName;
        cRegName << "ENFLAGS_S" << std::to_string(cPixelIds + 1);
        uint16_t regval = this->ReadChipReg(cChip, cRegName.str());

        regval = (regval & 0xEF) | (bitval << 4);

        // LOG(INFO) << BOLDBLUE << cRegName.str() <<","<<ipix<<","<<bitval<<","<<regval<< RESET;
        Req.first  = cRegName.str();
        Req.second = regval;
        pVecReq.push_back(Req);
    }
    return this->WriteChipMultReg(cChip, pVecReq);
}
//

bool SSAInterface::maskChannelGroup(ReadoutChip* cChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerify)
{
    auto cOriginalMask = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS>>(cChip->getChipOriginalMask());
    auto groupToMask   = std::static_pointer_cast<const ChannelGroup<NSSACHANNELS>>(group);

    auto cBitset = std::bitset<NSSACHANNELS>(groupToMask->getBitset() & cOriginalMask->getBitset());
    // cBitset = cBitset&std::bitset<NSSACHANNELS>(0x0000F0FF0);
    LOG(DEBUG) << BOLDBLUE << "\t... Applying mask to MPA" << +cChip->getId() << " with " << group->getNumberOfEnabledChannels() << " desired mask \t... : " << cBitset
               << " original mask  \t... : " << cOriginalMask << " enabled channels "
               << " original bitset was be \t... " << groupToMask->getBitset() << RESET;

    std::vector<std::pair<std::string, uint16_t>> pVecReq;
    pVecReq.clear();

    for(uint32_t ipix = 0; ipix < (NSSACHANNELS); ipix++)
    {
        auto shifted = std::bitset<NSSACHANNELS>(0x1) << ipix;
        bool bitval  = bool(((cBitset & shifted) >> ipix).to_ulong());

        std::pair<std::string, uint16_t> Req;
        uint32_t                         cPixelIds = ipix;
        std::ostringstream               cRegName;
        cRegName << "ENFLAGS_S" << std::to_string(cPixelIds + 1);
        uint16_t regval = this->ReadChipReg(cChip, cRegName.str());

        regval = (regval & 0xFE) | (bitval);

        // LOG(INFO) << BOLDBLUE << cRegName.str() <<","<<ipix<<","<<bitval<<","<<regval<< RESET;

        Req.first  = cRegName.str();
        Req.second = regval;
        pVecReq.push_back(Req);
    }
    return this->WriteChipMultReg(cChip, pVecReq);
}
//
bool SSAInterface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerify)
{
    bool success = true;
    if(mask) success &= maskChannelGroup(pChip, group, pVerify);
    if(inject) success &= setInjectionSchema(pChip, group, pVerify);

    return success;
}

bool SSAInterface::ConfigureChipOriginalMask(ReadoutChip* pSSA, bool pVerify, uint32_t pBlockSize) { return true; }
//

bool SSAInterface::MaskAllChannels(ReadoutChip* pSSA, bool mask, bool pVerify) { return true; }
// I actually want this one!
bool SSAInterface::WriteChipReg(Chip* pSSA, const std::string& pRegName, uint16_t pValue, bool pVerify)
{
    setBoard(pSSA->getBeBoardId());

    // LOG(INFO) << BOLDRED << "SSA! " << RESET;
    if(pRegName == "CountingMode")
    {
        auto cRegItem   = pSSA->getRegItem("ENFLAGS_ALL");
        cRegItem.fValue = (pValue << 2) | (1 << 0);
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
    else if(pRegName == "EnablePhaseAlignmentPattern")
    {
        this->producePhaseAlignmentPattern(static_cast<ReadoutChip*>(pSSA), pValue);
        return true;
    }
    else if(pRegName == "AmuxHigh")
    {
        return this->ConfigureAmux(pSSA, "HighZ");
    }
    else if(pRegName == "MonitorBandgap")
    {
        return this->ConfigureAmux(pSSA, "Bandgap");
    }
    else if(pRegName == "MonitorGround")
    {
        return this->ConfigureAmux(pSSA, "GND");
    }
    else if(pRegName.find("MaskChannel") != std::string::npos)
    {
        std::string cToken    = "MaskChannel";
        auto        cStripNum = std::atoi(pRegName.substr(pRegName.find(cToken) + cToken.length(), 4).c_str());
        LOG(DEBUG) << BOLDMAGENTA << "Masking strip number " << +cStripNum << " register is " << pRegName << RESET;
        std::stringstream cRegName;
        if(cStripNum == 0)
            cRegName << "ENFLAGS_ALL";
        else
            cRegName << "ENFLAGS_S" << +cStripNum;
        auto    cRegValue = pSSA->getReg(cRegName.str());
        uint8_t cRegMask  = (0x1 << 0); //
        cRegMask          = ~(cRegMask);
        uint8_t cValue    = (cRegValue & cRegMask) | (1 - pValue);
        auto    cRegItem  = pSSA->getRegItem(cRegName.str());
        cRegItem.fValue   = pValue;
        LOG(DEBUG) << BOLDBLUE << "Setting strip mask to 0x" << std::hex << +cValue << std::dec << " on StripNum#" << cStripNum << RESET;
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
    else if(pRegName == "Offsets")
    {
        auto cRegItem   = pSSA->getRegItem("THTRIMMING_ALL");
        cRegItem.fValue = pValue;
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
    else if(pRegName == "AsyncDelay")
    {
        std::vector<ChipRegItem> cRegItems;
        uint8_t                  cLSB     = pValue & 0xFF;
        auto                     cRegItem = pSSA->getRegItem("AsyncRead_StartDel_LSB");
        cRegItem.fValue                   = cLSB;
        cRegItems.push_back(cRegItem);
        uint8_t cMSB    = (pValue << 8);
        cRegItem        = pSSA->getRegItem("AsyncRead_StartDel_MSB");
        cRegItem.fValue = cMSB;
        cRegItems.push_back(cRegItem);

        return fBoardFW->MultiRegisterWrite(pSSA, cRegItems, pVerify);
    }
    else if(pRegName == "AnalogueAsync")
    {
        uint8_t cRegValue = (pValue << 4) | (pValue << 2) | (1 << 0);
        auto    cRegItem  = pSSA->getRegItem("ENFLAGS_ALL");
        cRegItem.fValue   = cRegValue;

        bool cEnableAnalogue = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        cRegValue            = ReadChipReg(pSSA, "ReadoutMode");
        cRegValue            = (cRegValue & 0x4) | (1);
        cRegItem             = pSSA->getRegItem("ReadoutMode");
        cRegItem.fValue      = cRegValue;
        bool cReadoutMode    = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        LOG(DEBUG) << BOLDBLUE << "Setting register ReadoutMode on SSA to 0x" << std::hex << +pValue << std::dec << RESET;

        cRegItem          = pSSA->getRegItem("FE_Calibration");
        cRegItem.fValue   = 1;
        bool cEnableFECal = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);

        return cEnableAnalogue && cReadoutMode && cEnableFECal;
    }
    else if(pRegName == "AnalogueSync")
    {
        uint8_t cRegValue = (pValue << 4) | (pValue << 2) | (1 << 0);
        auto    cRegItem  = pSSA->getRegItem("ENFLAGS_ALL");
        cRegItem.fValue   = cRegValue;

        bool cEnableAnalogue = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        cRegValue            = ReadChipReg(pSSA, "ReadoutMode");
        cRegValue            = (cRegValue & 0x4) | (0);
        cRegItem             = pSSA->getRegItem("ReadoutMode");
        cRegItem.fValue      = cRegValue;
        bool cReadoutMode    = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        return cEnableAnalogue && cReadoutMode;
    }
    else if(pRegName == "TriggerLatency")
    {
        //   LOG(INFO) << " pValue " << +pValue;
        uint8_t cLatencyReg1 = (0x00FF & pValue);
        auto    cRegItem     = pSSA->getRegItem("L1-Latency_LSB");
        cRegItem.fValue      = cLatencyReg1;
        bool cConfigReg1     = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);

        uint8_t cLatencyReg2 = (0x0100 & pValue) >> 8;
        cRegItem             = pSSA->getRegItem("L1-Latency_MSB");
        cRegItem.fValue      = cLatencyReg2;
        bool cConfigReg2     = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);

        LOG(DEBUG) << BOLDMAGENTA << "Setting TriggerLatency on SSA to " << pValue << RESET;
        return cConfigReg1 && cConfigReg2;
    }
    else if(pRegName == "Sync")
    {
        uint8_t pAnalogueCalib  = 1;
        uint8_t pDigitalCalib   = 1;
        uint8_t pHitCounter     = 0;
        uint8_t pSignalPolarity = 0;
        uint8_t pStripEnable    = 1;
        uint8_t cRegValue       = (pAnalogueCalib << 4) | (pDigitalCalib << 3) | (pHitCounter << 2) | (pSignalPolarity << 1);
        auto    cRegItem        = pSSA->getRegItem("ENFLAGS_ALL");
        cRegItem.fValue         = cRegValue | (pStripEnable << 0);
        LOG(INFO) << BOLDRED << "Enable flag is 0x" << std::hex << +cRegValue << std::dec << RESET;
        bool cEnableAnalogue = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        //
        cRegValue         = ReadChipReg(pSSA, "ReadoutMode");
        cRegItem          = pSSA->getRegItem("ReadoutMode");
        cRegValue         = (cRegValue & 0x4) | ((1 - pValue));
        bool cReadoutMode = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        LOG(INFO) << BOLDRED << "Readout mode is 0x" << std::hex << +cRegValue << std::dec << RESET;
        return cEnableAnalogue && cReadoutMode;
    }
    else if(pRegName.find("DigitalSync") != std::string::npos)
    {
        auto cRegItem                  = pSSA->getRegItem("ReadoutMode");
        cRegItem.fValue                = 0x00;
        bool              cReadoutMode = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        std::stringstream cRegName;
        if(pRegName.find("S") != std::string::npos) // global
        { cRegName << "ENFLAGS_ALL"; }
        else // single row
        {
            int cStripNumber = std::stoi(pRegName.substr(pRegName.find("R") + 1, pRegName.length()));
            cRegName << "ENFLAGS_S" << cStripNumber;
        }
        uint8_t pAnalogueCalib  = 0;
        uint8_t pDigitalCalib   = 1;
        uint8_t pHitCounter     = 0;
        uint8_t pSignalPolarity = 0;
        uint8_t pStripEnable    = (pValue != 0x00); // 1 == enable , 0 == disable
        uint8_t cRegValue       = (pAnalogueCalib << 4) | (pDigitalCalib << 3) | (pHitCounter << 2) | (pSignalPolarity << 1);
        cRegItem                = pSSA->getRegItem(cRegName.str());
        cRegItem.fValue         = cRegValue | (pStripEnable << 0);
        LOG(DEBUG) << BOLDRED << "Enable flag is 0x" << std::hex << +cRegValue << std::dec << RESET;
        bool cEnableReg = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        return cEnableReg && cReadoutMode;
    }
    else if(pRegName.find("DigitalSync") != std::string::npos)
    {
        int               cStripId = 0;
        std::stringstream cRegName;
        std::sscanf(pRegName.c_str(), "DigitalSync_S%d", &cStripId);
        cRegName << "ENFLAGS_S" << (1 + cStripId);
        // LOG (INFO) << BOLDYELLOW << "Digital injection on Strip#" << +cStripId << "\t" << cRegName.str() << RESET;

        auto cRegItem           = pSSA->getRegItem("ReadoutMode");
        cRegItem.fValue         = 0x00;
        bool    cReadoutMode    = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        uint8_t pAnalogueCalib  = 0;
        uint8_t pDigitalCalib   = 1;
        uint8_t pHitCounter     = 0;
        uint8_t pSignalPolarity = 0;
        uint8_t pStripEnable    = (pValue != 0x00); // 1 == enable , 0 == disable
        uint8_t cRegValue       = (pAnalogueCalib << 4) | (pDigitalCalib << 3) | (pHitCounter << 2) | (pSignalPolarity << 1);
        cRegItem                = pSSA->getRegItem(cRegName.str());
        cRegItem.fValue         = cRegValue | (pStripEnable << 0);
        LOG(DEBUG) << BOLDRED << "Enable flag is 0x" << std::hex << +cRegValue << std::dec << RESET;
        bool cEnableReg = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        return cEnableReg && cReadoutMode;
    }
    else if(pRegName == "DigitalAsync")
    {
        // digital injection, async , enable all strips
        auto cRegItem   = pSSA->getRegItem("ENFLAGS_ALL");
        cRegItem.fValue = (pValue << 3) | (1 << 2) | (1 << 0);
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
    else if(pRegName == "EnableSLVSTestOutput")
    {
        uint8_t cReadoutMode = (pValue == 0x1) ? 0x2 : 0x0;
        LOG(INFO) << BOLDBLUE << "Enabling SLVS test output on SSA#" << +pSSA->getId() << RESET;
        uint8_t cRegValue = ReadChipReg(pSSA, "ReadoutMode");
        auto    cRegItem  = pSSA->getRegItem("ReadoutMode");
        cRegItem.fValue   = (cRegValue & 0x4) | (cReadoutMode);
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
    else if(pRegName.find("OutPatternStubLine") != std::string::npos) // Stub Lines
    {
        int cLine;
        std::sscanf(pRegName.c_str(), "OutPatternStubLine%d", &cLine);
        std::stringstream cRegName;
        if(cLine < 7)
            cRegName << "OutPattern" << +cLine;
        else
            cRegName << "OutPattern7/FIFOconfig";

        auto cRegItem   = pSSA->getRegItem(cRegName.str());
        cRegItem.fValue = pValue;
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
    else if(pRegName.find("OutPatternL1Line") != std::string::npos) // Stub Lines
    {
        LOG(INFO) << BOLDRED << "SSA1 - cannot send pattern on L1 line" << RESET;
        return true;
    }
    else if(pRegName == "CalibrationPattern")
    {
        uint8_t pAnalogueCalib  = 0;
        uint8_t pDigitalCalib   = 1;
        uint8_t pHitCounter     = 0;
        uint8_t pSignalPolarity = 0;
        uint8_t pStripEnable    = 1;
        uint8_t cRegValue       = (pAnalogueCalib << 4) | (pDigitalCalib << 3) | (pHitCounter << 2) | (pSignalPolarity << 1);
        auto    cRegItem        = pSSA->getRegItem("ENFLAGS_ALL");
        cRegItem.fValue         = cRegValue | (pStripEnable << 0);
        bool cEnableAnalogue    = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        if(cEnableAnalogue)
        {
            auto cRegItem   = pSSA->getRegItem("DigCalibPattern_L");
            cRegItem.fValue = pValue;
            return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        }
        else
            return cEnableAnalogue;
    }
    else if(pRegName.find("SLVS_pad_current") != std::string::npos)
    {
        auto cRegItem   = pSSA->getRegItem("SLVS_pad_current");
        cRegItem.fValue = pValue;
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
    else if(pRegName.find("OutPatternStubLine") != std::string::npos) // Stub Lines
    {
        int cLine;
        std::sscanf(pRegName.c_str(), "OutPatternStubLine%d", &cLine);
        std::stringstream cRegName;
        if(cLine < 7)
            cRegName << "OutPattern" << +cLine;
        else
            cRegName << "OutPattern7/FIFOconfig";

        auto cRegItem   = pSSA->getRegItem(cRegName.str());
        cRegItem.fValue = pValue;
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
    else if(pRegName.find("OutPatternL1Line") != std::string::npos) // Stub Lines
    {
        LOG(INFO) << BOLDRED << "SSA1 - cannot send pattern on L1 line" << RESET;
        return true;
    }
    else if(pRegName.find("CalibrationPattern") != std::string::npos)
    {
        int cChannel;
        std::sscanf(pRegName.c_str(), "CalibrationPatternS%d", &cChannel);
        uint16_t cAddress = 0x0600 + cChannel + 1;
        LOG(INFO) << BOLDBLUE << "Configuring register 0x" << std::hex << cAddress << std::dec << " to 0x" << std::hex << pValue << std::dec << " for channel " << +cChannel << RESET;
        std::vector<ChipRegItem> cRegItems;
        std::vector<std::string> cBase{"DigCalibPattern_L", "DigCalibPattern_H"};
        for(auto cStr: cBase)
        {
            std::stringstream cRegName;
            cRegName << cStr << "_" << cChannel;
            auto cRegItem   = pSSA->getRegItem(cRegName.str());
            cRegItem.fValue = pValue;
            cRegItems.push_back(cRegItem);
        }
        bool cCnfgPattern = fBoardFW->MultiRegisterWrite(pSSA, cRegItems, pVerify);

        uint8_t pAnalogueCalib  = 0;
        uint8_t pDigitalCalib   = 1;
        uint8_t pHitCounter     = 0;
        uint8_t pSignalPolarity = 0;
        uint8_t pStripEnable    = 1;
        auto    cRegItem        = pSSA->getRegItem("ENFLAGS_ALL");
        uint8_t cRegValue       = (pAnalogueCalib << 4) | (pDigitalCalib << 3) | (pHitCounter << 2) | (pSignalPolarity << 1);
        cRegItem.fValue         = cRegValue | (pStripEnable << 0);
        bool cEnableAnalogue    = fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
        return cCnfgPattern && cEnableAnalogue;
    }
    else if(pRegName == "InjectedCharge")
    {
        auto cRegItem   = pSSA->getRegItem("Bias_CALDAC");
        cRegItem.fValue = pValue;
        LOG(DEBUG) << BOLDYELLOW << "Setting "
                   << " bias calDac to " << +cRegItem.fValue << " on SSA" << +pSSA->getId() << RESET;
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
    else if(pRegName == "Threshold")
    {
        LOG(DEBUG) << BOLDMAGENTA << "Setting threshold on SSA#" << +pSSA->getId() << " to " << pValue << RESET;
        auto cRegItem   = pSSA->getRegItem("Bias_THDAC");
        cRegItem.fValue = pValue;
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
    else if(pRegName == "EnableClockOut")
    {
        uint8_t cDefaultValue = 4;
        uint8_t cValue        = (pValue == 1) ? cDefaultValue : 0x00;
        LOG(DEBUG) << BOLDRED << "Setting SLVS_pad_current to " << +cValue << RESET;
        auto cRegItem   = pSSA->getRegItem("SLVS_pad_current");
        cRegItem.fValue = pValue;
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
    else if(fAmuxMap.find(pRegName) != fAmuxMap.end())
    {
        LOG(INFO) << BOLDYELLOW << pRegName << RESET;
        return ConfigureAmux(pSSA, pRegName);
    }
    else if(pRegName.substr(0, pRegName.find("__")) == "AMUX")
    {
        return this->ConfigureAmux(pSSA, pRegName.substr(1, pRegName.find("__")));
    }
    else
    {
        auto cRegItem   = pSSA->getRegItem(pRegName);
        cRegItem.fValue = pValue;
        return fBoardFW->SingleRegisterWrite(pSSA, cRegItem, pVerify);
    }
}
bool SSAInterface::ConfigureAmux(Chip* pChip, const std::string& pRegister)
{
    setBoard(pChip->getBeBoardId());
    // first make sure amux is set to 0 to avoid shorts
    // from SSA python methods
    uint8_t                  cHighZValue = 0x00;
    std::vector<std::string> cRegNames{"Bias_TEST_LSB", "Bias_TEST_MSB"};
    for(auto cReg: cRegNames)
    {
        auto cItem    = pChip->getRegItem(cReg);
        cItem.fValue  = cHighZValue;
        bool cSuccess = fBoardFW->SingleRegisterWrite(pChip, cItem, true);
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
            LOG(INFO) << BOLDBLUE << "Select test_Bias 0x" << std::hex << cValue << std::dec << RESET;
            uint8_t cIndex = 0;
            for(auto cReg: cRegNames)
            {
                auto    cItem     = pChip->getRegItem(cReg);
                uint8_t cRegValue = (cValue & (0xFF << 8 * cIndex)) >> 8 * cIndex;
                cItem.fValue      = cRegValue;
                bool cSuccess     = fBoardFW->SingleRegisterWrite(pChip, cItem, true);
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
uint8_t SSAInterface::ReadChipId(Chip* pChip)
{
    bool cVerifLoop = true;
    // start chip id read operation
    auto cItem   = pChip->getRegItem("Fuse_Mode");
    cItem.fValue = 0x0F;
    if(!fBoardFW->SingleRegisterWrite(pChip, cItem, cVerifLoop))
    {
        // chip id - 8 LSBs of e-fuse register
        return 0;
    }
    else
        throw std::runtime_error(std::string("Failed to start e-fuse read operation from SSA ") + std::to_string(pChip->getId()));
}

std::pair<int, float> SSAInterface::getWRattempts()
{
    float cReWR = 0;
    float cN    = 0;
    for(auto cIter: fReWrMap)
    {
        if(cIter.second != 0)
        {
            cN++;
            cReWR += cIter.second;
        }
    }
    float cMean = (cN == 0) ? 0 : cReWR / cN;
    return std::make_pair(cN, cMean);
}
std::pair<float, float> SSAInterface::getMinMaxWRattempts()
{
    float cReWRmin = 0;
    float cReWRmax = 0;
    for(auto cIter: fReWrMap)
    {
        if(cIter.second != 0 && cReWRmin == 0)
            cReWRmin = cIter.second;
        else
        {
            if(cIter.second < cReWRmin) cReWRmin = cIter.second;
        }
        if(cIter.second > cReWRmax) cReWRmax = cIter.second;
    }
    return std::make_pair(cReWRmin, cReWRmax);
}
void SSAInterface::printErrorSummary()
{
    LOG(INFO) << BOLDRED << "SSA total write error count : " << +fWriteErrors << " out of a total of " << +fRegisterWrites << " writes" << RESET;

    LOG(INFO) << BOLDRED << "SSA total read-back error count : " << +fReadBackErrors << " out of a total of " << +fRegisterWrites << " writes" << RESET;
}
bool SSAInterface::runVerification(Ph2_HwDescription::Chip* pChip, uint16_t pValue, std::string pRegName)
{
    auto     cRegItem = pChip->getRegItem(pRegName);
    uint32_t cValue   = pValue;
    // only check against map if this is
    // a register than *can* be read back
    // from
    bool cSuccess = ((cRegItem.fStatusReg == 0x01) ? true : (cRegItem.fValue == cValue));
    if(cSuccess)
        LOG(DEBUG) << BOLDGREEN << "\t...[DEBUG] Have written 0x" << std::hex << +cRegItem.fValue << std::dec << " SSA register with address 0x" << std::hex << +cRegItem.fAddress << std::dec
                   << " value read back is 0x" << std::hex << +cValue << std::dec << RESET;
    else if(!cSuccess)
        LOG(INFO) << BOLDRED << "Have written 0x" << std::hex << +cRegItem.fValue << std::dec << " SSA register with address 0x" << std::hex << +cRegItem.fAddress << std::dec
                  << " value read back is 0x" << std::hex << +cValue << std::dec << RESET;
    return cSuccess;
}
bool SSAInterface::WriteChipMultReg(Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerify)
{
    setBoard(pChip->getBeBoardId());
    auto                     cRegMap = pChip->getRegMap();
    std::vector<ChipRegItem> cRegItems;
    for(auto cReq: pVecReq)
    {
        auto cIterator = cRegMap.find(cReq.first);
        if(cIterator == cRegMap.end())
        {
            LOG(ERROR) << BOLDRED << "SSAInterface::WriteChipMultReg trtying to write to a register that doesn't exist in the map : " << cReq.first << RESET;
            continue;
        }

        ChipRegItem cItem = cIterator->second;
        cItem.fValue      = cReq.second;
        cRegItems.push_back(cItem);
    }
    return fBoardFW->MultiRegisterWrite(pChip, cRegItems, pVerify);
}

void SSAInterface::Set_calibration(Chip* pSSA, uint32_t cal) { this->WriteChipReg(pSSA, "Bias_CALDAC", cal); }

void SSAInterface::Set_threshold(Chip* pSSA, uint32_t th)
{
    setBoard(pSSA->getBeBoardId());

    this->WriteChipReg(pSSA, "Bias_THDAC", th);
}

bool SSAInterface::WriteChipAllLocalReg(ReadoutChip* pChip, const std::string& dacName, ChipContainer& localRegValues, bool pVerify)
{
    setBoard(pChip->getBeBoardId());
    assert(localRegValues.size() == pChip->getNumberOfChannels());
    std::string dacTemplate;
    // bool isMask = false;

    if(dacName == "GainTrim")
        dacTemplate = "GAINTRIMMING_S";
    else if(dacName == "ThresholdTrim")
        dacTemplate = "THTRIMMING_S";
    // else if(dacName == "Mask") isMask = true;
    else
        LOG(ERROR) << "Error, DAC " << dacName << " is not a Local DAC";

    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    ChannelGroup<NCHANNELS, 1>                    channelToEnable;

    // check if all registers are the same
    std::vector<uint8_t> cVals(0);
    for(uint16_t iChannel = 0; iChannel < pChip->getNumberOfChannels(); ++iChannel)
    {
        cVals.push_back(localRegValues.getChannel<uint16_t>(iChannel));
        LOG(DEBUG) << BOLDMAGENTA << +cVals[cVals.size() - 1] << RESET;
    }

    if(std::adjacent_find(cVals.begin(), cVals.end(), std::not_equal_to<uint16_t>()) == cVals.end())
    {
        LOG(INFO) << BOLDBLUE << "All elements of " << dacName << " are equal to one  another .. will use global register" << RESET;
        if(dacName == "TrimDAC_S" or dacName == "ThresholdTrim")
        {
            bool cWrite = this->WriteChipReg(pChip, "THTRIMMING_ALL", cVals[0], false);
            if(pVerify)
            {
                // THTRIMMING_S5
                auto cReadback = this->ReadChipReg(pChip, "THTRIMMING_S100");
                LOG(DEBUG) << BOLDMAGENTA << "Read-back a value of " << +cReadback << " from trim-dac register" << RESET;
                return (cReadback == cVals[0]);
            }
            else
                return cWrite;
        }
        // to-add .. add the rest
    }

    LOG(DEBUG) << BOLDBLUE << "Different values for " << dacName << " ... will NOT use global register" << RESET;

    std::vector<uint32_t> cVec;
    cVec.clear();
    bool                     cSuccess = true;
    std::vector<ChipRegItem> cRegItems;
    auto                     cRegMap = pChip->getRegMap();
    cRegItems.clear();
    for(uint8_t iChannel = 0; iChannel < pChip->getNumberOfChannels(); ++iChannel)
    {
        std::stringstream dacName1;
        dacName1 << dacTemplate.c_str() << 1 + iChannel;
        LOG(DEBUG) << BOLDBLUE << "Setting register " << dacName1.str() << " to " << (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F) << RESET;

        auto cIterator = cRegMap.find(dacName1.str());
        if(cIterator == cRegMap.end())
        {
            LOG(ERROR) << BOLDRED << "SSA2Interaface::WriteChipAllLocalReg trtying to write to a register that doesn't exist in the map : " << dacName1.str() << RESET;
            continue;
        }
        ChipRegItem cItem = cIterator->second;
        cItem.fValue      = localRegValues.getChannel<uint16_t>(iChannel) & 0x1F;
        // LOG(INFO) << BOLDBLUE << "Setting register " << dacName.str() << " to " << cItem.fValue << RESET;
        cRegItems.push_back(cItem);
    }
    cSuccess = cSuccess && fBoardFW->MultiRegisterWrite(pChip, cRegItems, pVerify);

    return cSuccess;
}

uint16_t SSAInterface::ReadChipReg(Chip* pSSA, const std::string& pRegNode)
{
    setBoard(pSSA->getBeBoardId());
    std::vector<uint32_t> cVecReq;
    ChipRegItem           cRegItem;
    if(pRegNode.find("CounterStrip") != std::string::npos)
    {
        int cChannel = 0;
        sscanf(pRegNode.c_str(), "CounterStrip%d", &cChannel);
        cRegItem.fPage         = 0x00;
        cRegItem.fAddress      = 0x0901 + cChannel;
        cRegItem.fValue        = 0;
        uint8_t cRPLSB         = fBoardFW->SingleRegisterRead(pSSA, cRegItem);
        cRegItem.fPage         = 0x00;
        cRegItem.fAddress      = 0x0801 + cChannel;
        uint8_t  cRPMSB        = fBoardFW->SingleRegisterRead(pSSA, cRegItem);
        uint16_t cCounterValue = (cRPMSB << 8) | cRPLSB;
        LOG(DEBUG) << BOLDBLUE << "Counter MSB is 0x" << std::bitset<8>(cRPMSB) << " Counter LSB is 0x" << std::bitset<8>(cRPLSB) << " Counter value is " << std::hex << +cCounterValue << std::dec
                   << RESET;
        return cCounterValue;
    }
    else if(pRegNode == "ChipId")
    {
        return this->ReadChipId(pSSA);
    }
    else if(pRegNode == "Threshold")
    {
        return this->ReadChipReg(pSSA, "Bias_THDAC");
    }
    else if(pRegNode.find("SLVS_pad_current") != std::string::npos)
    {
        return this->ReadChipReg(pSSA, "SLVS_pad_current");
    }
    else if(pRegNode == "TriggerLatency")
    {
        auto    cRegItem     = pSSA->getRegItem("L1-Latency_LSB");
        uint8_t cLatencyReg1 = fBoardFW->SingleRegisterRead(pSSA, cRegItem);
        cRegItem             = pSSA->getRegItem("L1-Latency_MSB");
        uint8_t cLatencyReg2 = fBoardFW->SingleRegisterRead(pSSA, cRegItem);
        return (cLatencyReg2 << 8) | cLatencyReg1;
    }
    else
    {
        cRegItem = pSSA->getRegItem(pRegNode);
        // LOG (INFO) << BOLDMAGENTA << "Reading " << pRegNode << " - value in memory is " << cRegItem.fValue << RESET;
        return fBoardFW->SingleRegisterRead(pSSA, cRegItem);
    }
}

} // namespace Ph2_HwInterface
