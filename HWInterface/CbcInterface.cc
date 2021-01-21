/*

        FileName :                     CbcInterface.cc
        Content :                      User Interface to the Cbcs
        Programmer :                   Lorenzo BIDEGAIN, Nicolas PIERRE, Georg AUZINGER
        Version :                      1.0
        Date of creation :             10/07/14
        Support :                      mail to : lorenzo.bidegain@gmail.com, nico.pierre@icloud.com

 */

#include "CbcInterface.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Container.h"
#include <bitset>

#define DEV_FLAG 0

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
CbcInterface::CbcInterface(const BeBoardFWMap& pBoardMap) : ReadoutChipInterface(pBoardMap) { fActiveChannels.reset(); }

CbcInterface::~CbcInterface() {}

bool CbcInterface::ConfigureChip(Chip* pCbc, bool pVerifLoop, uint32_t pBlockSize)
{
    // std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "!!!!!!!!!!!!!!!!" << std::endl;
    // first, identify the correct BeBoardFWInterface
    setBoard(pCbc->getBeBoardId());

    // vector to encode all the registers into
    std::vector<uint32_t> cVec;

    // Deal with the ChipRegItems and encode them

    ChipRegMap cCbcRegMap = pCbc->getRegMap();

    for(auto& cRegItem: cCbcRegMap)
    {
        // this is to protect from readback errors during Configure as the BandgapFuse and ChipIDFuse registers should
        // be e-fused in the CBC3
        if(cRegItem.first != "BandgapFuse" || cRegItem.first.find("ChipIDFuse") == std::string::npos)
        {
            // if( cRegItem.second.fPage == 0 )
            // LOG (DEBUG) << BOLDBLUE << "Writing 0x" << std::hex << +cRegItem.second.fValue << std::dec << " to " <<
            // cRegItem.first <<  " : register address 0x" << std::hex << +cRegItem.second.fAddress << std::dec << " on
            // page " << +cRegItem.second.fPage <<  RESET;
            fBoardFW->EncodeReg(cRegItem.second, pCbc->getFeId(), pCbc->getChipId(), cVec, pVerifLoop, true);
#ifdef COUNT_FLAG
            fRegisterCount++;
#endif
        }
    }

    // write the registers, the answer will be in the same cVec
    // the number of times the write operation has been attempted is given by cWriteAttempts
    uint8_t cWriteAttempts = 0;
    bool    cSuccess       = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
#ifdef COUNT_FLAG
    fTransactionCount++;
#endif

    return cSuccess;
}

bool CbcInterface::enableInjection(ReadoutChip* pChip, bool inject, bool pVerifLoop) { return this->WriteChipReg(pChip, "TestPulse", (int)inject, pVerifLoop); }

bool CbcInterface::setInjectionAmplitude(ReadoutChip* pChip, uint8_t injectionAmplitude, bool pVerifLoop) { return this->WriteChipReg(pChip, "TestPulsePotNodeSel", injectionAmplitude, pVerifLoop); }

bool CbcInterface::setInjectionSchema(ReadoutChip* pCbc, const ChannelGroupBase* group, bool pVerifLoop)
{
    // std::bitset<NCHANNELS> baseInjectionChannel (std::string(CBC_CHANNEL_GROUP_BITSET));
    // uint8_t channelGroup = 0;
    // for(; channelGroup<=8; ++channelGroup)
    // {
    //     if(static_cast<const ChannelGroup<NCHANNELS>*>(group)->getBitset() == baseInjectionChannel)
    //     {
    //         break;
    //     }
    //     baseInjectionChannel = baseInjectionChannel<<2;
    // }
    // if(channelGroup == 8)
    //     throw Exception( "bool CbcInterface::setInjectionSchema (ReadoutChip* pCbc, const ChannelGroupBase *group,
    //     bool pVerifLoop): CBC is not able to inject the channel pattern" );

    // uint8_t groupLookupTable[8] = {0x0, 0x4, 0x2, 0x6, 0x1, 0x5, 0x3, 0x7};
    // return this->WriteChipReg ( pCbc, "TestPulseGroup", groupLookupTable[channelGroup], pVerifLoop );

    std::bitset<NCHANNELS> cBitset = std::bitset<NCHANNELS>(static_cast<const ChannelGroup<NCHANNELS>*>(group)->getBitset());
    if(cBitset.count() == 0) // no mask set... so do nothing
        return true;
    // LOG (DEBUG) << BOLDBLUE << "Setting injection scheme for " << std::bitset<NCHANNELS>(cBitset) << RESET;
    uint16_t cFirstHit;
    for(cFirstHit = 0; cFirstHit < NCHANNELS; cFirstHit++)
    {
        if(cBitset[cFirstHit] != 0) break;
    }
    uint8_t cGroupId = std::floor((cFirstHit % 16) / 2);
    // LOG (DEBUG) << BOLDBLUE << "First unmasked channel in position " << +cFirstHit << " --- i.e. in TP group " <<
    // +cGroupId << RESET;
    if(cGroupId > 7)
        throw Exception("bool CbcInterface::setInjectionSchema (ReadoutChip* pCbc, const ChannelGroupBase *group, bool "
                        "pVerifLoop): CBC is not able to inject the channel pattern");
    // write register which selects group
    return this->WriteChipReg(pCbc, "TestPulseGroup", cGroupId, pVerifLoop);
}

bool CbcInterface::maskChannelsGroup(ReadoutChip* pCbc, const ChannelGroupBase* group, bool pVerifLoop)
{
    const ChannelGroup<NCHANNELS>* originalMask = static_cast<const ChannelGroup<NCHANNELS>*>(pCbc->getChipOriginalMask());
    const ChannelGroup<NCHANNELS>* groupToMask  = static_cast<const ChannelGroup<NCHANNELS>*>(group);
    LOG(DEBUG) << BOLDBLUE << "\t... Applying mask to CBC" << +pCbc->getChipId() << " with " << group->getNumberOfEnabledChannels()
               << " enabled channels\t... mask : " << std::bitset<NCHANNELS>(groupToMask->getBitset()) << RESET;

    fActiveChannels = groupToMask->getBitset();
    // auto  a = static_cast<const ChannelGroup<NCHANNELS,1>*>(groupToMask);
    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    cRegVec.clear();
    std::bitset<NCHANNELS> tmpBit(255);
    for(uint8_t maskGroup = 0; maskGroup < 32; ++maskGroup)
    {
        uint16_t cValue = (uint16_t)((originalMask->getBitset() & fActiveChannels) >> (maskGroup << 3) & tmpBit).to_ulong();
        LOG(DEBUG) << BOLDBLUE << "\t...Group" << +maskGroup << " : " << std::bitset<8>(cValue) << RESET;
        cRegVec.push_back(make_pair(fChannelMaskMapCBC3[maskGroup], (uint16_t)((originalMask->getBitset() & fActiveChannels) >> (maskGroup << 3) & tmpBit).to_ulong()));
    }
    return WriteChipMultReg(pCbc, cRegVec, pVerifLoop);
}

bool CbcInterface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const ChannelGroupBase* group, bool mask, bool inject, bool pVerifLoop)
{
    bool success = true;
    if(mask) success &= maskChannelsGroup(pChip, group, pVerifLoop);
    if(inject) success &= setInjectionSchema(pChip, group, pVerifLoop);
    return success;
}

bool CbcInterface::ConfigureChipOriginalMask(ReadoutChip* pCbc, bool pVerifLoop, uint32_t pBlockSize)
{
    ChannelGroup<NCHANNELS> allChannelEnabledGroup;
    return CbcInterface::maskChannelsGroup(pCbc, &allChannelEnabledGroup, pVerifLoop);
}

std::vector<uint8_t> CbcInterface::createHitListFromStubs(uint8_t pSeed, bool pSeedLayer)
{
    std::vector<uint8_t> cChannelList(0);
    uint32_t             cSeedStrip = std::floor(pSeed / 2.0); // counting from 1
    LOG(DEBUG) << BOLDMAGENTA << "Seed of " << +pSeed << " means first hit is in strip " << +cSeedStrip << RESET;
    size_t cNumberOfChannels = 1 + (pSeed % 2 != 0);
    for(size_t cIndex = 0; cIndex < cNumberOfChannels; cIndex++)
    {
        uint32_t cSeedChannel = 2 * (cSeedStrip - 1) + !pSeedLayer + 2 * cIndex;
        LOG(DEBUG) << BOLDMAGENTA << ".. need to unmask channel " << +cSeedChannel << RESET;
        cChannelList.push_back(static_cast<uint32_t>(cSeedChannel));
    }
    return cChannelList;
}
std::vector<uint8_t> CbcInterface::stubInjectionPattern(uint8_t pStubAddress, int pStubBend, bool pLayerSwap)
{
    LOG(DEBUG) << BOLDBLUE << "Injecting... stub in position " << +pStubAddress << " [half strips] with a bend of " << pStubBend << " [half strips]." << RESET;
    std::vector<uint8_t> cSeedHits = createHitListFromStubs(pStubAddress, !pLayerSwap);
    // correlation layer
    uint8_t              cCorrelated     = pStubAddress + pStubBend; // start counting strips from 0
    std::vector<uint8_t> cCorrelatedHits = createHitListFromStubs(cCorrelated, pLayerSwap);
    // merge two lists and unmask
    cSeedHits.insert(cSeedHits.end(), cCorrelatedHits.begin(), cCorrelatedHits.end());
    return cSeedHits;
}
std::vector<uint8_t> CbcInterface::stubInjectionPattern(ReadoutChip* pChip, uint8_t pStubAddress, int pStubBend)
{
    bool cLayerSwap = (this->ReadChipReg(pChip, "LayerSwap") == 1);
    return stubInjectionPattern(pStubAddress, pStubBend, cLayerSwap);
}
bool CbcInterface::injectStubs(ReadoutChip* pCbc, std::vector<uint8_t> pStubAddresses, std::vector<int> pStubBends, bool pUseNoise)
{
    setBoard(pCbc->getBeBoardId());
    if(pUseNoise)
    {
        uint16_t cVcth = 1023;
        // LOG (DEBUG) << BOLDBLUE << "Injecting stubs in CBC" << +pCbc->getChipId() << RESET;
        // LOG (DEBUG) << BOLDBLUE << "Starting by lowering threshold on this CBC and masking all channels." << RESET;
        this->WriteChipReg(pCbc, "VCth", cVcth);
    }
    ChannelGroup<NCHANNELS, 1> cChannelMask;
    cChannelMask.disableAllChannels();
    for(size_t cIndex = 0; cIndex < pStubAddresses.size(); cIndex += 1)
    {
        std::vector<uint8_t> cPattern = this->stubInjectionPattern(pCbc, pStubAddresses[cIndex], pStubBends[cIndex]);
        for(auto cChannel: cPattern) cChannelMask.enableChannel(cChannel);
    }
    if(!pUseNoise)
    {
        uint16_t               cFirstHit = 0;
        std::bitset<NCHANNELS> cBitset   = std::bitset<NCHANNELS>(cChannelMask.getBitset());
        LOG(DEBUG) << BOLDMAGENTA << "Bitset for this mask is " << cBitset << RESET;
        for(cFirstHit = 0; cFirstHit < NCHANNELS; cFirstHit++)
        {
            if(cBitset[cFirstHit] != 0) break;
        }
        uint8_t cGroupId = std::floor((cFirstHit % 16) / 2);
        LOG(INFO) << BOLDBLUE << "First unmasked channel in position " << +cFirstHit << " --- i.e. in TP group " << +cGroupId << RESET;
        if(cGroupId > 7)
            throw Exception("bool CbcInterface::setInjectionSchema (ReadoutChip* pCbc, const ChannelGroupBase *group, "
                            "bool pVerifLoop): CBC is not able to inject the channel pattern");
        // write register which selects group
        this->WriteChipReg(pCbc, "TestPulseGroup", cGroupId);
    }
    return this->maskChannelsGroup(pCbc, &cChannelMask);
}
std::vector<uint8_t> CbcInterface::readLUT(ReadoutChip* pCbc)
{
    setBoard(pCbc->getBeBoardId());
    std::vector<uint8_t> cBendCodes(30, 0); // bend registers are 0 -- 14. Each register encodes 2 codes
    for(size_t cIndex = 0; cIndex < 15; cIndex += 1)
    {
        const size_t cLength = (cIndex < 10) ? 5 : 6;
        char         cBuffer[20];
        sprintf(cBuffer, "Bend%d", static_cast<int>(cIndex));
        std::string cRegName(cBuffer, cLength);
        LOG(DEBUG) << BOLDBLUE << "Reading bend register " << cRegName << RESET;
        uint16_t cValue            = this->ReadChipReg(pCbc, cRegName);
        cBendCodes[cIndex * 2]     = (cValue & 0x0F);
        cBendCodes[cIndex * 2 + 1] = (cValue & 0xF0) >> 4;
    }
    return cBendCodes;
}

uint16_t CbcInterface::readErrorRegister(ReadoutChip* pCbc)
{
    // read I2c register with error flags
    ChipRegItem cRegItem;
    cRegItem.fPage    = 0x01;
    cRegItem.fAddress = 0x1D;

    setBoard(pCbc->getBeBoardId());
    std::vector<uint32_t> cVecReq;
    fBoardFW->EncodeReg(cRegItem, pCbc->getFeId(), pCbc->getChipId(), cVecReq, true, false);
    fBoardFW->ReadChipBlockReg(cVecReq);
    // bools to find the values of failed and read
    bool    cFailed = false;
    bool    cRead;
    uint8_t cChipId;
    fBoardFW->DecodeReg(cRegItem, cChipId, cVecReq[0], cRead, cFailed);
    std::pair<bool, uint16_t> cReadBack = std::make_pair(!cFailed, cRegItem.fValue);
    if(cReadBack.first)
        return cReadBack.second;
    else
        return 0xFF;
}

bool CbcInterface::selectLogicMode(ReadoutChip* pCbc, std::string pModeSelect, bool pForHits, bool pForStubs, bool pVerifLoop)
{
    uint8_t pMode;
    if(pModeSelect == "Sampled")
        pMode = 0;
    else if(pModeSelect == "OR")
        pMode = 1;
    else if(pModeSelect == "HIP")
        pMode = 2;
    else if(pModeSelect == "Latched")
        pMode = 3;
    else
    {
        LOG(INFO) << BOLDYELLOW << "Invalid mode selected! Valid modes are Sampled/OR/Latched/HIP" << RESET;
        return false;
    }
    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    setBoard(pCbc->getBeBoardId());
    std::string cRegName       = "Pipe&StubInpSel&Ptwidth";
    uint16_t    cOriginalValue = this->ReadChipReg(pCbc, cRegName);
    uint16_t    cMask          = 0xFF - ((3 * pForHits << 6) | (3 * pForStubs << 4));
    uint16_t    cRegValue      = (cOriginalValue & cMask) | (pMode * pForHits << 6) | (pMode * pForStubs << 4);
    // LOG (DEBUG) << BOLDBLUE << "Original register value : 0x" << std::hex << cOriginalValue << std::dec << "\t logic
    // register set to 0x" << std::hex << +cRegValue << std::dec << " to select " << pModeSelect << " mode on CBC" <<
    // +pCbc->getChipId() << RESET;
    return WriteChipSingleReg(pCbc, cRegName, cRegValue, pVerifLoop);
    // WriteChipSingleReg
    // uint8_t cMask = (uint8_t)(~(((3*pForHits) << 6) | ((3*pForStubs) << 4)));
    // uint8_t cValue = (((pMode*pForHits) << 6) | ((pMode*pForStubs) << 4)) | (cOriginalValue & cMask);
    // LOG (DEBUG) << BOLDBLUE << "Settinng logic selection register to 0x" << std::hex << +cValue << std::dec << " to
    // select " << pModeSelect << " mode on CBC" << +pCbc->getChipId() << RESET; cRegVec.emplace_back (cRegName, cValue);
    // return WriteChipMultReg (pCbc, cRegVec, pVerifLoop);
}

bool CbcInterface::enableHipSuppression(ReadoutChip* pCbc, bool pForHits, bool pForStubs, uint8_t pClocks, bool pVerifLoop)
{
    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    setBoard(pCbc->getBeBoardId());
    // configure logic mode
    if(!this->selectLogicMode(pCbc, "HIP", pForHits, pForStubs, pVerifLoop))
    {
        LOG(INFO) << BOLDYELLOW << "Could not select HIP mode..." << RESET;
        return false;
    }
    // configure hip logic
    std::string cRegName       = "HIP&TestMode";
    uint8_t     cOriginalValue = this->ReadChipReg(pCbc, cRegName);
    uint8_t     cMask          = 0x0F;
    bool        cEnableHips    = true;
    uint8_t     cMaxClocks     = 7;
    uint8_t     cValue         = (((pClocks << 5) & (cMaxClocks << 5)) & (!cEnableHips << 4)) | (cOriginalValue & cMask);
    // LOG (DEBUG) << BOLDBLUE << "Settinng HIP configuration register to 0x" << std::hex << +cValue << std::dec << " to
    // enable max. " << +pClocks << " clocks on CBC" << +pCbc->getChipId() << RESET;
    cRegVec.emplace_back(cRegName, cValue);
    return WriteChipMultReg(pCbc, cRegVec, pVerifLoop);
}

bool CbcInterface::MaskAllChannels(ReadoutChip* pCbc, bool mask, bool pVerifLoop)
{
    ChannelGroup<NCHANNELS, 1> cChannelMask;
    if(mask)
        cChannelMask.disableAllChannels();
    else
        cChannelMask.enableAllChannels();
    // LOG (DEBUG)  << BOLDBLUE << "Mask to be set is " << std::bitset<254>( cChannelMask.getBitset() ) << RESET;
    return this->maskChannelsGroup(pCbc, &cChannelMask, pVerifLoop);
}

bool CbcInterface::WriteChipReg(Chip* pCbc, const std::string& dacName, uint16_t dacValue, bool pVerifLoop)
{
    if(dacName == "VCth")
    {
        if(pCbc->getFrontEndType() == FrontEndType::CBC3)
        {
            if(dacValue > 1023)
                LOG(ERROR) << "Error, Threshold for CBC3 can only be 10 bit max (1023)!";
            else
            {
                std::vector<std::pair<std::string, uint16_t>> cRegVec;
                // VCth1 holds bits 0-7 and VCth2 holds 8-9
                uint16_t cVCth1 = dacValue & 0x00FF;
                uint16_t cVCth2 = (dacValue & 0x0300) >> 8;
                cRegVec.emplace_back("VCth1", cVCth1);
                cRegVec.emplace_back("VCth2", cVCth2);
                return WriteChipMultReg(pCbc, cRegVec, pVerifLoop);
            }
        }
        else
            LOG(ERROR) << "Not a valid chip type!";
    }
    else if(dacName == "TriggerLatency")
    {
        if(pCbc->getFrontEndType() == FrontEndType::CBC3)
        {
            if(dacValue > 511)
                LOG(ERROR) << "Error, Threshold for CBC3 can only be 10 bit max (1023)!";
            else
            {
                std::vector<std::pair<std::string, uint16_t>> cRegVec;
                // TriggerLatency1 holds bits 0-7 and FeCtrl&TrgLate2 holds 8
                uint16_t cLat1 = dacValue & 0x00FF;
                uint16_t cLat2 = (pCbc->getReg("FeCtrl&TrgLat2") & 0xFE) | ((dacValue & 0x0100) >> 8);
                cRegVec.emplace_back("TriggerLatency1", cLat1);
                cRegVec.emplace_back("FeCtrl&TrgLat2", cLat2);
                LOG(DEBUG) << BOLDBLUE << "Setting latency on " << +pCbc->getChipId() << " to " << +dacValue << " 0x" << std::hex << +cLat1 << std::dec << " --- 0x" << std::hex << +cLat2 << std::dec
                           << RESET;
                return WriteChipMultReg(pCbc, cRegVec, pVerifLoop);
            }
        }
        else
            LOG(ERROR) << "Not a valid chip type!";
    }
    else if(dacName == "TestPulseDelay")
    {
        uint8_t cValue = pCbc->getReg("TestPulseDel&ChanGroup");
        uint8_t cGroup = cValue & 0x07;
        // groupId is reversed in this register
        std::bitset<5> cDelay  = dacValue;
        std::string    cSelect = cDelay.to_string();
        std::reverse(cSelect.begin(), cSelect.end());
        std::bitset<5> cTestPulseDelay(cSelect);
        uint8_t        cRegValue = (cGroup | (static_cast<uint8_t>(cTestPulseDelay.to_ulong()) << 3));
        LOG(DEBUG) << BOLDBLUE << "Setting test pulse delay for goup [rev.] " << std::bitset<3>(cGroup) << " to " << +dacValue << " --  register to  0x" << std::bitset<8>(+cRegValue) << std::dec
                   << RESET;
        return WriteChipSingleReg(pCbc, "TestPulseDel&ChanGroup", cRegValue, pVerifLoop);
    }
    else if(dacName == "TestPulseGroup")
    {
        uint8_t cValue = pCbc->getReg("TestPulseDel&ChanGroup");
        uint8_t cDelay = cValue & 0xF8;
        // groupId is reversed in this register
        std::bitset<3> cGroup  = dacValue;
        std::string    cSelect = cGroup.to_string();
        std::reverse(cSelect.begin(), cSelect.end());
        std::bitset<3> cTestPulseGroup(cSelect);
        uint8_t        cRegValue = (cDelay | (static_cast<uint8_t>(cTestPulseGroup.to_ulong())));
        LOG(DEBUG) << BOLDBLUE << "Setting test pulse register on CBC" << +pCbc->getChipId() << " to select group " << +dacValue << " --  register to  0x" << std::bitset<8>(+cRegValue) << std::dec
                   << RESET;
        return WriteChipSingleReg(pCbc, "TestPulseDel&ChanGroup", cRegValue, pVerifLoop);
    }
    else if(dacName == "AmuxOutput")
    {
        uint8_t cValue    = pCbc->getReg("MiscTestPulseCtrl&AnalogMux");
        uint8_t cRegValue = (cValue & 0xE0) | dacValue;
        LOG(DEBUG) << BOLDBLUE << "Setting AmuxOutput on Chip" << +pCbc->getChipId() << " to " << +dacValue << " - register set to : 0x" << std::hex << +cRegValue << std::dec << RESET;
        return WriteChipSingleReg(pCbc, "MiscTestPulseCtrl&AnalogMux", cRegValue, pVerifLoop);
    }
    else if(dacName == "TestPulse")
    {
        uint8_t cValue    = pCbc->getReg("MiscTestPulseCtrl&AnalogMux");
        uint8_t cRegValue = (cValue & 0xBF) | (dacValue << 6);
        return WriteChipSingleReg(pCbc, "MiscTestPulseCtrl&AnalogMux", cRegValue, pVerifLoop);
    }
    else if(dacName == "HitOr")
    {
        uint8_t cValue    = pCbc->getReg("40MhzClk&Or254");
        uint8_t cRegValue = (cValue & 0xBf) | (dacValue << 6);
        LOG(DEBUG) << BOLDBLUE << "Setting HITOr on Chip" << +pCbc->getChipId() << " from 0x" << std::hex << +cValue << std::dec << " to " << +dacValue << " - register set to : 0x" << std::hex
                   << +cRegValue << std::dec << RESET;
        return WriteChipSingleReg(pCbc, "40MhzClk&Or254", cRegValue, pVerifLoop);
    }
    else if(dacName == "DLL")
    {
        uint8_t cOriginalValue = pCbc->getReg("40MhzClk&Or254");
        // dll is reversed in this register
        std::bitset<5> cDelay  = dacValue;
        std::string    cSelect = cDelay.to_string();
        std::reverse(cSelect.begin(), cSelect.end());
        std::bitset<5> cClockDelay(cSelect);
        uint8_t        cNewRegValue = ((cOriginalValue & 0xE0) | static_cast<uint8_t>(cClockDelay.to_ulong()));
        LOG(DEBUG) << BOLDBLUE << "Setting clock delay on Chip" << +pCbc->getChipId() << " to " << std::bitset<5>(+dacValue) << " - register set to : 0x" << std::hex << +cNewRegValue << std::dec
                   << RESET;
        return WriteChipSingleReg(pCbc, "40MhzClk&Or254", cNewRegValue, pVerifLoop);
    }
    else if(dacName == "PtCut")
    {
        uint8_t cValue    = pCbc->getReg("Pipe&StubInpSel&Ptwidth");
        uint8_t cRegValue = (cValue & 0xF0) | dacValue;
        return WriteChipSingleReg(pCbc, "Pipe&StubInpSel&Ptwidth", cRegValue, pVerifLoop);
    }
    else if(dacName == "EnableSLVS")
    {
        uint8_t cValue    = pCbc->getReg("HIP&TestMode");
        uint8_t cRegValue = (cValue & 0xFE) | !dacValue;
        if(dacValue == 1)
            LOG(DEBUG) << BOLDBLUE << "Enabling SLVS output on CBCs by setting register to " << std::bitset<8>(cRegValue) << RESET;
        else
            LOG(DEBUG) << BOLDBLUE << "Disabling SLVS output on CBCs by setting register to " << std::bitset<8>(cRegValue) << RESET;
        return WriteChipSingleReg(pCbc, "HIP&TestMode", cRegValue, pVerifLoop);
    }
    else
    {
        if(dacValue > 255)
            LOG(ERROR) << "Error, DAC " << dacName << " for CBC3 can only be 8 bit max (255)!";
        else
            return WriteChipSingleReg(pCbc, dacName, dacValue, pVerifLoop);
    }
    return false;
}

bool CbcInterface::WriteChipSingleReg(Chip* pCbc, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop)
{
    if(pValue > 0xFF)
    {
        LOG(ERROR) << "Cbc register are 8 bits, impossible to write " << pValue << " on registed " << pRegNode;
        return false;
    }

    // first, identify the correct BeBoardFWInterface
    setBoard(pCbc->getBeBoardId());

    // next, get the reg item
    ChipRegItem cRegItem = pCbc->getRegItem(pRegNode);
    cRegItem.fValue      = pValue & 0xFF;

    // vector for transaction
    std::vector<uint32_t> cVec;

    // encode the reg specific to the FW, pVerifLoop decides if it should be read back, true means to write it
    fBoardFW->EncodeReg(cRegItem, pCbc->getFeId(), pCbc->getChipId(), cVec, pVerifLoop, true);
    // write the registers, the answer will be in the same cVec
    // the number of times the write operation has been attempted is given by cWriteAttempts
    uint8_t cWriteAttempts = 0;
    bool    cSuccess       = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);

    // update the HWDescription object
    if(cSuccess) pCbc->setReg(pRegNode, pValue);

#ifdef COUNT_FLAG
    fRegisterCount++;
    fTransactionCount++;
#endif

    return cSuccess;
}

bool CbcInterface::WriteChipMultReg(Chip* pCbc, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop)
{
    // first, identify the correct BeBoardFWInterface
    setBoard(pCbc->getBeBoardId());

    std::vector<uint32_t> cVec;

    // Deal with the ChipRegItems and encode them
    ChipRegItem cRegItem;

    for(const auto& cReg: pVecReq)
    {
        if(cReg.second > 0xFF)
        {
            LOG(ERROR) << "Cbc register are 8 bits, impossible to write " << cReg.second << " on registed " << cReg.first;
            continue;
        }
        cRegItem        = pCbc->getRegItem(cReg.first);
        cRegItem.fValue = cReg.second;
        fBoardFW->EncodeReg(cRegItem, pCbc->getFeId(), pCbc->getChipId(), cVec, pVerifLoop, true);
#ifdef COUNT_FLAG
        fRegisterCount++;
#endif
    }

    // write the registers, the answer will be in the same cVec
    // the number of times the write operation has been attempted is given by cWriteAttempts
    uint8_t cWriteAttempts = 0;
    bool    cSuccess       = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
#ifdef COUNT_FLAG
    fTransactionCount++;
#endif

    // if the transaction is successfull, update the HWDescription object
    if(cSuccess)
    {
        for(const auto& cReg: pVecReq)
        {
            cRegItem = pCbc->getRegItem(cReg.first);
            pCbc->setReg(cReg.first, cReg.second);
        }
    }

    return cSuccess;
}
bool CbcInterface::WriteChipAllLocalReg(ReadoutChip* pCbc, const std::string& dacName, ChipContainer& localRegValues, bool pVerifLoop)
{
    setBoard(pCbc->getBeBoardId());
    assert(localRegValues.size() == pCbc->getNumberOfChannels());
    std::string dacTemplate;
    bool        isMask = false;

    if(dacName == "ChannelOffset")
        dacTemplate = "Channel%03d";
    else if(dacName == "Mask")
        isMask = true;
    else
        LOG(ERROR) << "Error, DAC " << dacName << " is not a Local DAC";

    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    // std::vector<uint32_t> listOfChannelToUnMask;
    ChannelGroup<NCHANNELS, 1> channelToEnable;

    std::vector<uint32_t> cVec;
    cVec.clear();
    for(uint8_t iChannel = 0; iChannel < pCbc->getNumberOfChannels(); ++iChannel)
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
            // fBoardFW->EncodeReg ( cRegItem, pCbc->getFeId(), pCbc->getChipId(), cVec, pVerifLoop, true );
            // #ifdef COUNT_FLAG
            //     fRegisterCount++;
            // #endif
            cRegVec.emplace_back(dacName1, localRegValues.getChannel<uint16_t>(iChannel));
        }
    }

    if(isMask) { return maskChannelsGroup(pCbc, &channelToEnable, pVerifLoop); }
    else
    {
        // uint8_t cWriteAttempts = 0 ;
        // bool cSuccess = fBoardFW->WriteChipBlockReg ( cVec, cWriteAttempts, pVerifLoop);
        // #ifdef COUNT_FLAG
        //     fTransactionCount++;
        // #endif
        // return cSuccess;
        return WriteChipMultReg(pCbc, cRegVec, pVerifLoop);
    }
}

uint16_t CbcInterface::ReadChipReg(Chip* pCbc, const std::string& pRegNode)
{
    ChipRegItem cRegItem;
    bool        cFailed = false;
    bool        cRead;
    uint8_t     cCbcId;
    setBoard(pCbc->getBeBoardId());
    std::vector<uint32_t> cVecReq;
    if(pRegNode == "VCth")
    {
        fBoardFW->EncodeReg(pCbc->getRegItem("VCth1"), pCbc->getFeId(), pCbc->getChipId(), cVecReq, true, false);
        fBoardFW->EncodeReg(pCbc->getRegItem("VCth2"), pCbc->getFeId(), pCbc->getChipId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        fBoardFW->DecodeReg(cRegItem, cCbcId, cVecReq[0], cRead, cFailed);

        uint16_t cReg0 = cRegItem.fValue;
        pCbc->setReg("VCth1", cRegItem.fValue);
        fBoardFW->DecodeReg(cRegItem, cCbcId, cVecReq[1], cRead, cFailed);
        if(cFailed) return 0;

        pCbc->setReg("VCth2", cRegItem.fValue);
        uint16_t cReg1      = cRegItem.fValue;
        uint16_t cThreshold = ((cReg1 & 0x3) << 8) | cReg0;
        return cThreshold;
    }
    else if(pRegNode == "Threshold")
    {
        fBoardFW->EncodeReg(pCbc->getRegItem("VCth1"), pCbc->getFeId(), pCbc->getChipId(), cVecReq, true, false);
        fBoardFW->EncodeReg(pCbc->getRegItem("VCth2"), pCbc->getFeId(), pCbc->getChipId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        fBoardFW->DecodeReg(cRegItem, cCbcId, cVecReq[0], cRead, cFailed);

        uint16_t cReg0 = cRegItem.fValue;
        pCbc->setReg("VCth1", cRegItem.fValue);
        fBoardFW->DecodeReg(cRegItem, cCbcId, cVecReq[1], cRead, cFailed);
        if(cFailed) return 0;

        pCbc->setReg("VCth2", cRegItem.fValue);
        uint16_t cReg1      = cRegItem.fValue;
        uint16_t cThreshold = ((cReg1 & 0x3) << 8) | cReg0;
        return cThreshold;
    }
    else if(pRegNode == "HitLogic")
    {
        cRegItem = pCbc->getRegItem("Pipe&StubInpSel&Ptwidth");
        fBoardFW->EncodeReg(cRegItem, pCbc->getFeId(), pCbc->getChipId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        // bools to find the values of failed and read
        fBoardFW->DecodeReg(cRegItem, cCbcId, cVecReq[0], cRead, cFailed);
        if(!cFailed) pCbc->setReg("Pipe&StubInpSel&Ptwidth", cRegItem.fValue);

        return (cRegItem.fValue & 0xC0) >> 6;
    }
    else if(pRegNode == "StubLogic")
    {
        cRegItem = pCbc->getRegItem("Pipe&StubInpSel&Ptwidth");
        fBoardFW->EncodeReg(cRegItem, pCbc->getFeId(), pCbc->getChipId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        // bools to find the values of failed and read
        fBoardFW->DecodeReg(cRegItem, cCbcId, cVecReq[0], cRead, cFailed);
        if(!cFailed) pCbc->setReg("Pipe&StubInpSel&Ptwidth", cRegItem.fValue);

        return (cRegItem.fValue & 0x30) >> 4;
    }
    else if(pRegNode == "HitOr")
    {
        cRegItem = pCbc->getRegItem("40MhzClk&Or254");
        fBoardFW->EncodeReg(cRegItem, pCbc->getFeId(), pCbc->getChipId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        fBoardFW->DecodeReg(cRegItem, cCbcId, cVecReq[0], cRead, cFailed);
        if(!cFailed) pCbc->setReg("40MhzClk&Or254", cRegItem.fValue);
        return (cRegItem.fValue & 0x40) >> 6;
    }
    else if(pRegNode == "LayerSwap")
    {
        cRegItem = pCbc->getRegItem("LayerSwap&CluWidth");
        fBoardFW->EncodeReg(cRegItem, pCbc->getFeId(), pCbc->getChipId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        fBoardFW->DecodeReg(cRegItem, cCbcId, cVecReq[0], cRead, cFailed);
        if(!cFailed) pCbc->setReg("LayerSwap&CluWidth", cRegItem.fValue);
        return (cRegItem.fValue & 0x08) >> 3;
    }
    else if(pRegNode == "PtCut")
    {
        cRegItem = pCbc->getRegItem("Pipe&StubInpSel&Ptwidth");
        fBoardFW->EncodeReg(cRegItem, pCbc->getFeId(), pCbc->getChipId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        fBoardFW->DecodeReg(cRegItem, cCbcId, cVecReq[0], cRead, cFailed);
        if(!cFailed) pCbc->setReg("Pipe&StubInpSel&Ptwidth", cRegItem.fValue);
        return (cRegItem.fValue & 0x0F);
    }
    else
    {
        cRegItem = pCbc->getRegItem(pRegNode);
        fBoardFW->EncodeReg(cRegItem, pCbc->getFeId(), pCbc->getChipId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        // bools to find the values of failed and read
        fBoardFW->DecodeReg(cRegItem, cCbcId, cVecReq[0], cRead, cFailed);
        if(!cFailed) pCbc->setReg(pRegNode, cRegItem.fValue);

        return cRegItem.fValue & 0xFF;
    }
}

void CbcInterface::WriteModuleBroadcastChipReg(const Module* pHybrid, const std::string& pRegNode, uint16_t pValue)
{
    // first set the correct BeBoard
    setBoard(pHybrid->getBeBoardId());

    ChipRegItem cRegItem = static_cast<ReadoutChip*>(pHybrid->at(0))->getRegItem(pRegNode);
    cRegItem.fValue      = pValue;

    // vector for transaction
    std::vector<uint32_t> cVec;

    // encode the reg specific to the FW, pVerifLoop decides if it should be read back, true means to write it
    // the 1st boolean could be true if I acually wanted to read back from each CBC but this somehow does not make
    // sense!
    fBoardFW->BCEncodeReg(cRegItem, pHybrid->size(), cVec, false, true);

    // true is the readback bit - the IC FW just checks that the transaction was successful and the
    // Strasbourg FW does nothing
    bool cSuccess = fBoardFW->BCWriteChipBlockReg(cVec, true);

#ifdef COUNT_FLAG
    fRegisterCount++;
    fTransactionCount++;
#endif

    // update the HWDescription object -- not sure if the transaction was successfull
    if(cSuccess)
        for(auto cCbc: *pHybrid) static_cast<ReadoutChip*>(cCbc)->setReg(pRegNode, pValue);
}

void CbcInterface::WriteBroadcastCbcMultiReg(const Module* pHybrid, const std::vector<std::pair<std::string, uint8_t>> pVecReg)
{
    // first set the correct BeBoard
    setBoard(pHybrid->getBeBoardId());

    std::vector<uint32_t> cVec;

    // Deal with the ChipRegItems and encode them
    ChipRegItem cRegItem;

    for(const auto& cReg: pVecReg)
    {
        cRegItem        = static_cast<ReadoutChip*>(pHybrid->at(0))->getRegItem(cReg.first);
        cRegItem.fValue = cReg.second;

        fBoardFW->BCEncodeReg(cRegItem, pHybrid->size(), cVec, false, true);
#ifdef COUNT_FLAG
        fRegisterCount++;
#endif
    }

    // write the registerss, the answer will be in the same cVec
    bool cSuccess = fBoardFW->BCWriteChipBlockReg(cVec, true);

#ifdef COUNT_FLAG
    fTransactionCount++;
#endif

    if(cSuccess)
        for(auto cCbc: *pHybrid)
            for(auto& cReg: pVecReg)
            {
                cRegItem = static_cast<ReadoutChip*>(cCbc)->getRegItem(cReg.first);
                static_cast<ReadoutChip*>(cCbc)->setReg(cReg.first, cReg.second);
            }
}

uint32_t CbcInterface::ReadCbcIDeFuse(Chip* pCbc)
{
    WriteChipReg(pCbc, "ChipIDFuse3", 8);
    uint8_t  IDa     = ReadChipReg(pCbc, "ChipIDFuse1");
    uint8_t  IDb     = ReadChipReg(pCbc, "ChipIDFuse2");
    uint8_t  IDc     = ReadChipReg(pCbc, "ChipIDFuse3");
    uint32_t IDeFuse = ((IDa)&0x000000FF) + (((IDb) << 8) & 0x0000FF00) + (((IDc) << 16) & 0x000F0000);
    LOG(INFO) << BOLDBLUE << " CHIP ID FUSE " << +IDeFuse << RESET;
    return IDeFuse;
}

} // namespace Ph2_HwInterface
