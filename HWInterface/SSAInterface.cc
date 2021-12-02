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
//

// //#FIXME temporary fix to use 1/2 PS skeleton
// void SSAInterface::LinkLpGBT(D19clpGBTInterface* pLpGBTInterface, lpGBT* pLpGBT)
// {
//     flpGBTInterface = pLpGBTInterface;
//     flpGBT          = pLpGBT;
// }

bool SSAInterface::ConfigureChip(Chip* pSSA, bool pVerifLoop, uint32_t pBlockSize)
{
    fTrackRegisters = false;
    // for now ..
    bool              cConfigLocalRegs = true;
    std::stringstream cOutput;
    setBoard(pSSA->getBeBoardId());
    pSSA->printChipType(cOutput);
    LOG(INFO) << BOLDBLUE << cOutput.str() << "...Configuring chip with Id[" << +pSSA->getId() << "]" << RESET;

    std::vector<uint32_t> cVec;
    ChipRegMap            cSSARegMap = pSSA->getRegMap();
    // get register map
    LOG(DEBUG) << BOLDMAGENTA << "Setting up SSA#" << +pSSA->getId() << " maps.." << RESET;
    fMap.clear();
    fWriteErrorMap.clear();
    for(auto& cRegItem: cSSARegMap)
    {
        fMap[cRegItem.second.fAddress] = cRegItem.first;
        // update map to indicate that there are not registers that
        // can be read back from
        if(cRegItem.first.find("_ALL") != std::string::npos) { pSSA->setReg(cRegItem.first, cRegItem.second.fValue, cRegItem.second.fPrmptCfg, 1); }
    }
    // update map
    cSSARegMap = pSSA->getRegMap();
    std::vector<std::pair<uint16_t, uint16_t>> cRegs;
    cRegs.clear();
    std::vector<std::string> cRegsToConfig;
    cRegsToConfig.push_back("THTRIMMING");
    cConfigLocalRegs = cConfigLocalRegs && (cRegsToConfig.size() > 0);
    for(auto& cMapItem: fMap)
    {
        // check for local register
        bool cIsLocal    = (cMapItem.second.find("_S") != std::string::npos);
        bool cIsAsyncDel = (cMapItem.second.find("AsyncRead") != std::string::npos);
        cIsLocal         = cIsLocal && !cIsAsyncDel;
        if(cIsLocal) LOG(DEBUG) << BOLDGREEN << " Local Register " << cMapItem.second << RESET;

        // if local and we are not configuring local then skip
        bool cSkip = (cIsLocal && !cConfigLocalRegs);
        if(cRegsToConfig.size() > 0 && cConfigLocalRegs && cIsLocal)
        {
            // check if this is one to not skip
            bool cRegFound = false;
            auto cIter     = cRegsToConfig.begin();
            do
            {
                cRegFound = cMapItem.second.find(*cIter) != std::string::npos;
                if(cRegFound) LOG(DEBUG) << BOLDMAGENTA << " Found " << cMapItem.second << RESET;

                cIter++;
            } while(cIter < cRegsToConfig.end() && !cRegFound);
            cSkip = (!cRegFound);
        }
        if(cSkip) continue;
        if(cIsLocal)
            LOG(DEBUG) << BOLDGREEN << "Configuring local register " << cMapItem.second << RESET;
        else
            LOG(DEBUG) << BOLDBLUE << "Configuring global register " << cMapItem.second << RESET;

        ChipRegItem& cItem = cSSARegMap[cMapItem.second];
        // create a register
        std::pair<uint16_t, uint16_t> cReg;
        cReg.first  = cMapItem.first;
        cReg.second = cItem.fValue;
        cRegs.push_back(cReg);
    }
    if(!cConfigLocalRegs)
        LOG(INFO) << BOLDBLUE << "Configuring SSA#" << +pSSA->getId() << " - write " << +cRegs.size() << " registers [skipping registers for individual strips]" << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Complete configuration of SSA#" << +pSSA->getId() << " - write " << +cRegs.size() << " registers [configuring registers for individual strips]" << RESET;
    bool cSuccess   = this->WriteRegs(pSSA, cRegs, pVerifLoop);
    fTrackRegisters = true;
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
    LOG(INFO) << GREEN << "SSA Alignment" << RESET;
    this->WriteChipReg(pChip, "ReadoutMode", 2);
    uint8_t cAlignmentPattern = 0xEA;
    for(uint8_t cLineId = 0; cLineId < 9; cLineId++)
    {
        std::stringstream cRegName;
        if(cLineId < 8)
            cRegName << "OutPattern" << +cLineId;
        else
            cRegName << "OutPattern7/FIFOconfig";
        this->WriteChipReg(pChip, cRegName.str(), cAlignmentPattern);
    }
}
bool SSAInterface::enableInjection(ReadoutChip* pChip, bool inject, bool pVerifLoop)
{
    // for now always with asynchronous mode
    return this->WriteChipReg(pChip, "AnalogueAsync", 1);
}
bool SSAInterface::setInjectionAmplitude(ReadoutChip* pChip, uint8_t injectionAmplitude, bool pVerifLoop) { return this->WriteChipReg(pChip, "InjectedCharge", injectionAmplitude, pVerifLoop); }

//

//
bool SSAInterface::setInjectionSchema(ReadoutChip* cChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop)
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

bool SSAInterface::maskChannelGroup(ReadoutChip* cChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop)
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
bool SSAInterface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop)
{
    bool success = true;
    if(mask) success &= maskChannelGroup(pChip, group, pVerifLoop);
    if(inject) success &= setInjectionSchema(pChip, group, pVerifLoop);

    return success;
}

bool SSAInterface::ConfigureChipOriginalMask(ReadoutChip* pSSA, bool pVerifLoop, uint32_t pBlockSize) { return true; }
//

bool SSAInterface::MaskAllChannels(ReadoutChip* pSSA, bool mask, bool pVerifLoop) { return true; }
// I actually want this one!
bool SSAInterface::WriteChipReg(Chip* pSSA, const std::string& pRegName, uint16_t pValue, bool pVerifLoop)
{
    if(pRegName == "CountingMode")
    {
        uint8_t cRegValue = (pValue << 2) | (1 << 0);
        return WriteChipSingleReg(pSSA, "ENFLAGS_ALL", cRegValue, false);
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
        LOG(DEBUG) << BOLDBLUE << "Setting strip mask to 0x" << std::hex << +cValue << std::dec << " on StripNum#" << cStripNum << RESET;
        return WriteChipSingleReg(pSSA, cRegName.str(), cValue, pVerifLoop);
    }
    else if(pRegName == "Offsets")
    {
        return this->WriteChipSingleReg(pSSA, "THTRIMMING_ALL", pValue, false);
    }
    // else if(pRegName == "AnalogueAsync")
    // {
    //     uint8_t cRegValue       = (pValue << 4) | (pValue << 2) | (1 << 0);
    //     bool    cEnableAnalogue = WriteChipSingleReg(pSSA, "ENFLAGS_ALL", cRegValue, pVerifLoop);
    //     bool    cEnableFECal    = WriteChipSingleReg(pSSA, "FE_Calibration", 1, pVerifLoop);
    //     cRegValue               = ReadChipReg(pSSA, "ReadoutMode");
    //     cRegValue               = (cRegValue & 0x4) | (1);
    //     bool cReadoutMode       = WriteChipSingleReg(pSSA, "ReadoutMode", cRegValue, pVerifLoop);
    //     return cEnableAnalogue && cEnableFECal && cReadoutMode;

    //     // auto cRegValue               = ReadChipReg(pSSA, "ReadoutMode");
    //     // cRegValue = (cRegValue & 0x4) | pValue;
    //     // bool cReadoutMode       = WriteChipSingleReg(pSSA, "ReadoutMode", cRegValue, pVerifLoop);
    //     // // enable calibration pulse
    //     // bool    cEnableFECal    = WriteChipSingleReg(pSSA, "FE_Calibration", pValue, pVerifLoop);

    //     // // configure strip mode
    //     // // readout mode 1 -- ASYNC counter
    //     // ChipRegMask cMask;
    //     // cMask.fBitShift = STRIP_ENABLE_TABLE.find("AnalogueInjection")->second;
    //     // cMask.fNbits = 1;
    //     // pSSA->setRegBits( "ENFLAGS_ALL", cMask , pValue );
    //     // // hit counter- enable
    //     // cMask.fBitShift = STRIP_ENABLE_TABLE.find("CounterEnable")->second;
    //     // cMask.fNbits = 1;
    //     // pSSA->setRegBits( "ENFLAGS_ALL", cMask , pValue );
    //     // // un-mask all strips  - strip enable
    //     // cMask.fBitShift = STRIP_ENABLE_TABLE.find("StripMask")->second;
    //     // cMask.fNbits = 1;
    //     // pSSA->setRegBits( "ENFLAGS_ALL", cMask , pValue );

    //     // if(pValue == 1)
    //     //     LOG(INFO) << BOLDBLUE << "Enabling analogue injection on SSA by setting register ENFLAGS_ALL to 0x" << std::hex << +pSSA->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;
    //     // else
    //     //     LOG(INFO) << BOLDBLUE << "Disabling analogue injection on SSA by setting register ENFLAGS_ALL to 0x" << std::hex << +pSSA->getRegItem("ENFLAGS_ALL").fValue << std::dec << RESET;

    //     // /*uint8_t cRegValue       = (pValue << 4) | (pValue << 2) | (1 << 0);
    //     // bool    cEnableAnalogue = WriteChipSingleReg(pSSA, "ENFLAGS_ALL", cRegValue, false);
    //     // cRegValue               = ReadChipReg(pSSA, "ReadoutMode");*/
    //     // return cReadoutMode && WriteChipSingleReg(pSSA, "ENFLAGS_ALL", pSSA->getRegItem("ENFLAGS_ALL").fValue, false) && cEnableFECal;
    // }
    else if(pRegName == "AsyncDelay")
    {
        uint8_t cLSB = pValue & 0xFF;
        uint8_t cMSB = (pValue >> 8);
        LOG(DEBUG) << BOLDBLUE << "Delay value is 0x" << std::hex << pValue << std::dec << " LSB should be 0x" << std::hex << +cLSB << std::dec << " MSB should be 0x" << std::hex << +cMSB << std::dec
                   << RESET;
        WriteChipSingleReg(pSSA, "AsyncRead_StartDel_LSB", cLSB, pVerifLoop);
        WriteChipSingleReg(pSSA, "AsyncRead_StartDel_MSB", cMSB, pVerifLoop);
        return true;
    }
    else if(pRegName == "AnalogueAsync")
    {
        uint8_t cRegValue       = (pValue << 4) | (pValue << 2) | (1 << 0);
        bool    cEnableAnalogue = WriteChipSingleReg(pSSA, "ENFLAGS_ALL", cRegValue, pVerifLoop);
        bool    cEnableFECal    = WriteChipSingleReg(pSSA, "FE_Calibration", 1, pVerifLoop);
        cRegValue               = ReadChipReg(pSSA, "ReadoutMode");
        cRegValue               = (cRegValue & 0x4) | (1);
        bool cReadoutMode       = WriteChipSingleReg(pSSA, "ReadoutMode", cRegValue, pVerifLoop);
        return cEnableAnalogue && cEnableFECal && cReadoutMode;
    }
    else if(pRegName == "AnalogueSync")
    {
        uint8_t cRegValue       = (pValue << 4) | (pValue << 2) | (1 << 0);
        bool    cEnableAnalogue = WriteChipSingleReg(pSSA, "ENFLAGS_ALL", cRegValue, false);
        bool    cEnableFECal    = WriteChipSingleReg(pSSA, "FE_Calibration", 1, pVerifLoop);
        cRegValue               = ReadChipReg(pSSA, "ReadoutMode");
        cRegValue               = (cRegValue & 0x4) | (0);
        bool cReadoutMode       = WriteChipSingleReg(pSSA, "ReadoutMode", cRegValue, pVerifLoop);
        return cEnableAnalogue && cEnableFECal && cReadoutMode;
    }
    else if(pRegName == "TriggerLatency")
    {
        //   LOG(INFO) << " pValue " << +pValue;
        uint8_t cLatencyReg1 = (0x00FF & pValue);
        uint8_t cLatencyReg2 = (0x0100 & pValue) >> 8;
        bool    cConfigReg1  = this->WriteChipSingleReg(pSSA, "L1-Latency_LSB", cLatencyReg1);
        bool    cConfigReg2  = this->WriteChipSingleReg(pSSA, "L1-Latency_MSB", cLatencyReg2);

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
        cRegValue               = cRegValue | (pStripEnable << 0);
        LOG(INFO) << BOLDRED << "Enable flag is 0x" << std::hex << +cRegValue << std::dec << RESET;
        bool cEnableAnalogue = WriteChipSingleReg(pSSA, "ENFLAGS_ALL", cRegValue, false);
        bool cEnableFECal    = WriteChipSingleReg(pSSA, "FE_Calibration", 1, pVerifLoop);
        cRegValue            = ReadChipReg(pSSA, "ReadoutMode");
        cRegValue            = (cRegValue & 0x4) | ((1 - pValue));
        bool cReadoutMode    = WriteChipSingleReg(pSSA, "ReadoutMode", cRegValue, pVerifLoop);
        LOG(INFO) << BOLDRED << "Readout mode is 0x" << std::hex << +cRegValue << std::dec << RESET;
        return cEnableAnalogue && cEnableFECal && cReadoutMode;
    }
    else if(pRegName.find("DigitalSync") != std::string::npos)
    {
        bool        cReadoutMode = WriteChipSingleReg(pSSA, "ReadoutMode", 0x00, pVerifLoop);
        std::string cRegName;
        if(pRegName.find("S") != std::string::npos) // global
        { cRegName = "ENFLAGS_ALL"; }
        else // single row
        {
            std::ostringstream cRegName;
            int                cStripNumber = std::stoi(pRegName.substr(pRegName.find("R") + 1, pRegName.length()));
            cRegName << "ENFLAGS_S" << std::to_string(cStripNumber);
        }
        uint8_t pAnalogueCalib  = 0;
        uint8_t pDigitalCalib   = 1;
        uint8_t pHitCounter     = 0;
        uint8_t pSignalPolarity = 0;
        uint8_t pStripEnable    = (pValue != 0x00); // 1 == enable , 0 == disable
        uint8_t cRegValue       = (pAnalogueCalib << 4) | (pDigitalCalib << 3) | (pHitCounter << 2) | (pSignalPolarity << 1);
        cRegValue               = cRegValue | (pStripEnable << 0);
        LOG(DEBUG) << BOLDRED << "Enable flag is 0x" << std::hex << +cRegValue << std::dec << RESET;
        bool cEnableReg = WriteChipSingleReg(pSSA, cRegName, 1, pVerifLoop);
        return cEnableReg && cReadoutMode;
    }
    else if(pRegName == "DigitalAsync")
    {
        // digital injection, async , enable all strips
        uint8_t cRegValue = (pValue << 3) | (1 << 2) | (1 << 0);
        return WriteChipSingleReg(pSSA, "ENFLAGS_ALL", cRegValue, false);
    }
    else if(pRegName == "EnableSLVSTestOutput")
    {
        LOG(DEBUG) << BOLDBLUE << "Enabling SLVS test output on SSA#" << +pSSA->getId() << RESET;
        uint8_t cRegValue = ReadChipReg(pSSA, "ReadoutMode");
        cRegValue         = (cRegValue & 0x4) | (pValue << 1);
        return WriteChipSingleReg(pSSA, "ReadoutMode", cRegValue, pVerifLoop);
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
        return this->WriteChipSingleReg(pSSA, cRegName.str(), pValue, pVerifLoop);
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
        cRegValue               = cRegValue | (pStripEnable << 0);
        bool cEnableAnalogue    = WriteChipSingleReg(pSSA, "ENFLAGS_ALL", cRegValue, false);
        if(cEnableAnalogue)
            return WriteChipSingleReg(pSSA, "DigCalibPattern_L", pValue, pVerifLoop);
        else
            return cEnableAnalogue;
    }
    else if(pRegName.find("CalibrationPattern") != std::string::npos)
    {
        int cChannel;
        std::sscanf(pRegName.c_str(), "CalibrationPatternS%d", &cChannel);
        uint16_t cAddress = 0x0600 + cChannel + 1;
        LOG(INFO) << BOLDBLUE << "Configuring register 0x" << std::hex << cAddress << std::dec << " to 0x" << std::hex << pValue << std::dec << " for channel " << +cChannel << RESET;

        uint8_t pAnalogueCalib  = 0;
        uint8_t pDigitalCalib   = 1;
        uint8_t pHitCounter     = 0;
        uint8_t pSignalPolarity = 0;
        uint8_t pStripEnable    = 1;
        uint8_t cRegValue       = (pAnalogueCalib << 4) | (pDigitalCalib << 3) | (pHitCounter << 2) | (pSignalPolarity << 1);
        cRegValue               = cRegValue | (pStripEnable << 0);
        bool cEnableAnalogue    = WriteChipSingleReg(pSSA, "ENFLAGS_ALL", cRegValue, false);
        if(cEnableAnalogue)
            return this->WriteReg(pSSA, cAddress, pValue, pVerifLoop);
        else
            return cEnableAnalogue;
    }
    else if(pRegName == "InjectedCharge")
    {
        LOG(DEBUG) << BOLDBLUE << "Setting "
                   << " bias calDac to " << +pValue << " on SSA" << +pSSA->getId() << RESET;
        return WriteChipSingleReg(pSSA, "Bias_CALDAC", pValue, pVerifLoop);
    }
    else if(pRegName == "Threshold")
    {
        LOG(DEBUG) << BOLDMAGENTA << "Setting threshold on SSA#" << +pSSA->getId() << " to " << pValue << RESET;
        return WriteChipSingleReg(pSSA, "Bias_THDAC", (pValue), pVerifLoop);
    }
    else if(pRegName == "EnableClockOut")
    {
        uint8_t cDefaultValue = 4;
        uint8_t cValue        = (pValue == 1) ? cDefaultValue : 0x00;
        LOG(DEBUG) << BOLDRED << "Setting SLVS_pad_current to " << +cValue << RESET;
        return WriteChipSingleReg(pSSA, "SLVS_pad_current", cValue, pVerifLoop);
    }
    else
    {
        return this->WriteChipSingleReg(pSSA, pRegName, pValue, pVerifLoop);
    }
}
bool SSAInterface::ConfigureAmux(Chip* pChip, const std::string& pRegister)
{
    // first make sure amux is set to 0 to avoid shorts
    // from SSA python methods
    uint8_t                  cHighZValue = 0x00;
    std::vector<std::string> cRegNames{"Bias_TEST_LSB", "Bias_TEST_MSB"};
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
uint8_t SSAInterface::ReadChipId(Chip* pChip)
{
    bool cVerifLoop = true;
    // start chip id read operation
    if(!this->WriteChipSingleReg(pChip, "Fuse_Mode", 0x0F, cVerifLoop))
    {
        // chip id - 8 LSBs of e-fuse register
        return 0;
    }
    else
        throw std::runtime_error(std::string("Failed to start e-fuse read operation from SSA ") + std::to_string(pChip->getId()));
}

bool SSAInterface::WriteReg(Chip* pChip, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerifLoop)
{
    // LOG (INFO) << BOLDMAGENTA << "SSAInterface::WriteReg writing " << +pRegisterAddress << RESET;
    // bool cRetry   = false;
    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    ChipRegItem cRegItem;
    cRegItem.fAddress = pRegisterAddress;
    if(fTrackRegisters) UpdateModifiedRegMap(pChip, pRegisterAddress);
    bool cFound = pChip->getRegMap().find(fMap[pRegisterAddress]) != pChip->getRegMap().end();
    if(cFound) cRegItem = pChip->getRegItem(fMap[pRegisterAddress]);
    cRegItem.fValue = pRegisterValue & 0xFF;
    // write
    if(!lpGBTFound())
    {
        std::vector<uint32_t> cVec;
        fBoardFW->EncodeReg(cRegItem, pChip->getId(), pChip->getId(), cVec, pVerifLoop, true);
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
    }
    else
    {
        LOG(DEBUG) << BOLDBLUE << "Writing address 0x" << std::hex << +pRegisterAddress << std::dec << RESET;
        bool cVerify = pVerifLoop && (cRegItem.fStatusReg == 0);
        cSuccess     = fBoardFW->WriteFERegister(pChip, pRegisterAddress, pRegisterValue, cVerify);
        if(cSuccess && cFound) { pChip->setReg(fMap[pRegisterAddress], pRegisterValue, cRegItem.fPrmptCfg, cRegItem.fStatusReg); }
        fRegisterWrites++;
    }
    return cSuccess;
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
bool SSAInterface::WriteRegs(Chip* pChip, const std::vector<std::pair<uint16_t, uint16_t>> pRegs, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    bool cSuccess = true;
    if(!lpGBTFound())
    // if(flpGBTInterface == nullptr)
    {
        std::vector<uint32_t> cVec;
        cVec.clear();
        for(const auto& cReg: pRegs)
        {
            ChipRegItem cRegItem;
            cRegItem.fPage    = 0x00;
            cRegItem.fAddress = cReg.first;
            cRegItem.fValue   = cReg.second & 0xFF;
            fBoardFW->EncodeReg(cRegItem, pChip->getId(), pChip->getId(), cVec, pVerifLoop, true);
#ifdef COUNT_FLAG
            fRegisterCount++;
#endif
        }
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
#ifdef COUNT_FLAG
        fTransactionCount++;
#endif
    }
    else
    {
        cSuccess = true;
        for(const auto& cReg: pRegs) { cSuccess = cSuccess && this->WriteReg(pChip, cReg.first, cReg.second, pVerifLoop); }
    }
    return cSuccess;
}

uint16_t SSAInterface::ReadReg(Chip* pChip, uint16_t pRegisterAddress, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    if(fMap.size() == 0)
    {
        ChipRegMap cSSARegMap = pChip->getRegMap();
        for(auto& cRegItem: cSSARegMap)
        {
            fMap[cRegItem.second.fAddress] = cRegItem.first;
            // update map to indicate that there are not registers that
            // can be read back from
            if(cRegItem.first.find("_ALL") != std::string::npos)
            {
                LOG(INFO) << BOLDMAGENTA << "\t.. found a status register : " << cRegItem.first << RESET;
                pChip->setReg(cRegItem.first, cRegItem.second.fValue, cRegItem.second.fPrmptCfg, 1);
            }
        }
    }

    ChipRegItem cRegItem;
    cRegItem.fPage    = 0x00;
    cRegItem.fAddress = pRegisterAddress;
    cRegItem.fValue   = 0;
    if(!lpGBTFound())
    {
        bool                  cFailed = false;
        bool                  cRead;
        std::vector<uint32_t> cVecReq;
        fBoardFW->EncodeReg(cRegItem, pChip->getId(), pChip->getId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        uint8_t cSSAId;
        fBoardFW->DecodeReg(cRegItem, cSSAId, cVecReq[0], cRead, cFailed);
    }
    else
    {
        // auto cValue = flpGBTInterface->ssaRead(flpGBT, pChip->getHybridId(), pChip->getId(), pRegisterAddress);
        auto cValue = fBoardFW->ReadFERegister(pChip, pRegisterAddress);
        // LOG (INFO) << "SSAInterface::ReadReg reading from 0x" << std::hex << pRegisterAddress << std::dec << " -- " << +cValue << RESET;
        pChip->setReg(fMap[pRegisterAddress], cValue, cRegItem.fPrmptCfg, cRegItem.fStatusReg);
        cRegItem.fValue = cValue;
    }
    return cRegItem.fValue & 0xFF;
}

bool SSAInterface::WriteChipSingleReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop)
{
    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    // LOG (INFO) << BOLDMAGENTA << "SSAInterface::WriteChipSingleReg writing " << pRegNode << RESET;
    if(fMap.size() == 0)
    {
        ChipRegMap cSSARegMap = pChip->getRegMap();
        for(auto& cRegItem: cSSARegMap)
        {
            fMap[cRegItem.second.fAddress] = cRegItem.first;
            // update map to indicate that there are not registers that
            // can be read back from
            if(cRegItem.first.find("_ALL") != std::string::npos)
            {
                LOG(INFO) << BOLDMAGENTA << "\t.. found a status register : " << cRegItem.first << RESET;
                pChip->setReg(cRegItem.first, cRegItem.second.fValue, cRegItem.second.fPrmptCfg, 1);
            }
        }
    }
    auto cRegItem = pChip->getRegMap().find(pRegNode)->second;
    if(fTrackRegisters) UpdateModifiedRegMap(pChip, cRegItem.fAddress);
    cRegItem.fValue     = pValue & 0xFF;
    bool cCheckReadback = (pRegNode.find("_ALL") != std::string::npos) ? false : pVerifLoop;
    cCheckReadback      = cCheckReadback && (cRegItem.fStatusReg == 0);
    if(!lpGBTFound())
    {
        std::vector<uint32_t> cVec;
        fBoardFW->EncodeReg(cRegItem, pChip->getHybridId(), pChip->getId() % 8, cVec, cCheckReadback, true);
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, cCheckReadback);
    }
    else
    {
        // bool cVerify = cCheckReadback && (cRegItem.fStatusReg == 0);
        cSuccess = fBoardFW->WriteFERegister(pChip, cRegItem.fAddress, cRegItem.fValue, cCheckReadback);
        if(cSuccess) pChip->setReg(pRegNode, cRegItem.fValue, cRegItem.fPrmptCfg, 1);
    }
    if(cSuccess && !lpGBTFound()) // check is done in lpGBTInterface for opto
    {
        // bool cVerify = cCheckReadback && (cRegItem.fStatusReg == 0);
        cSuccess = (cCheckReadback) ? (ReadChipReg(pChip, pRegNode) == pValue) : true;
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
bool SSAInterface::WriteChipMultReg(Chip* pSSA, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop)
{
    setBoard(pSSA->getBeBoardId());
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
        this->WriteChipReg(pSSA, cReg.first, cReg.second, pVerifLoop);

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
            cRegItem = pSSA->getRegItem(cReg.first);
            pSSA->setReg(cReg.first, cRegItem.fValue, cRegItem.fPrmptCfg, 1);
        }
    }

    return cSuccess;
}

void SSAInterface::Set_calibration(Chip* pSSA, uint32_t cal) { this->WriteChipReg(pSSA, "Bias_CALDAC", cal); }

void SSAInterface::Set_threshold(Chip* pSSA, uint32_t th)
{
    setBoard(pSSA->getBeBoardId());

    this->WriteChipReg(pSSA, "Bias_THDAC", th);
}

bool SSAInterface::WriteChipAllLocalReg(ReadoutChip* pChip, const std::string& dacName, ChipContainer& localRegValues, bool pVerifLoop)
{
    assert(localRegValues.size() == pChip->getNumberOfChannels());
    std::string dacTemplate;
    // bool isMask = false;

    if(dacName == "GainTrim")
        dacTemplate = "GAINTRIMMING_S%d";
    else if(dacName == "ThresholdTrim")
        dacTemplate = "THTRIMMING_S%d";
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
        LOG(DEBUG) << BOLDBLUE << "All elements of " << dacName << " are equal to one  another .. will use global register" << RESET;
        if(dacName == "TrimDAC_S" or dacName == "ThresholdTrim")
        {
            bool cWrite = this->WriteChipReg(pChip, "THTRIMMING_ALL", cVals[0], false);
            if(pVerifLoop)
            {
                auto cReadback = this->ReadChipReg(pChip, "TrimDAC_S100");
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
    bool cSuccess = true;
    for(uint8_t iChannel = 0; iChannel < pChip->getNumberOfChannels(); ++iChannel)
    {
        char dacName1[20];
        sprintf(dacName1, dacTemplate.c_str(), 1 + iChannel);
        LOG(DEBUG) << BOLDBLUE << "Setting register " << dacName1 << " to " << (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F) << RESET;
        cSuccess = cSuccess && this->WriteChipSingleReg(pChip, dacName1, (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F), pVerifLoop);
    }
    return cSuccess;
}
// Definitely needed:

void SSAInterface::ReadASEvent(ReadoutChip* pSSA, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange)
{
    if(pSRange == std::pair<uint32_t, uint32_t>{0, 0}) pSRange = std::pair<uint32_t, uint32_t>{1, pSSA->getNumberOfChannels() - 1};
    for(uint32_t i = pSRange.first; i <= pSRange.second; i++)
    {
        char cRegName[100];
        std::sprintf(cRegName, "CounterStrip%d", static_cast<int>(i));
        pData.push_back(this->ReadChipReg(pSSA, cRegName));
        // uint8_t cRP1 = this->ReadChipReg(pSSA, "ReadCounter_LSB_S" + std::to_string(i));
        // uint8_t cRP2 = this->ReadChipReg(pSSA, "ReadCounter_MSB_S" + std::to_string(i));
        // LOG(INFO) << BOLDBLUE << "cRP1 "<<+cRP1 <<  " cRP2 " << +cRP2<< RESET;
        // pData.push_back((cRP2*256) + cRP1);
    }
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
        uint8_t cRPLSB         = this->ReadReg(pSSA, cRegItem.fAddress) & 0xFF;
        cRegItem.fPage         = 0x00;
        cRegItem.fAddress      = 0x0801 + cChannel;
        uint8_t  cRPMSB        = this->ReadReg(pSSA, cRegItem.fAddress) & 0xFF;
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
    else if(pRegNode == "TriggerLatency")
    {
        uint8_t cLatencyReg1 = this->ReadReg(pSSA, pSSA->getRegItem("L1-Latency_LSB").fAddress);
        uint8_t cLatencyReg2 = this->ReadReg(pSSA, pSSA->getRegItem("L1-Latency_MSB").fAddress);
        return (cLatencyReg2 << 8) | cLatencyReg1;
    }
    else
    {
        cRegItem = pSSA->getRegItem(pRegNode);
        // LOG (INFO) << BOLDMAGENTA << "Reading " << pRegNode << " - value in memory is " << cRegItem.fValue << RESET;
        return this->ReadReg(pSSA, cRegItem.fAddress) & 0xFF;
    }
}

} // namespace Ph2_HwInterface
