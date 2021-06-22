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
    std::stringstream cOutput;
    setBoard(pCbc->getBeBoardId());
    pCbc->printChipType(cOutput);
    LOG(INFO) << BOLDBLUE << cOutput.str() << "...Configuring chip with Id[" << +pCbc->getId() << "]" << RESET;

    bool cSkipLocalRegs = false;
    // Deal with the ChipRegItems and encode them
    bool                                             cSuccess   = false;
    ChipRegMap                                       cCbcRegMap = pCbc->getRegMap();
    std::vector<std::pair<std::string, ChipRegItem>> cRegList;
    cRegList.clear();
    for(auto cMapItem: cCbcRegMap)
    {
        std::pair<std::string, ChipRegItem> cItem;
        cItem.first  = cMapItem.first;
        cItem.second = cMapItem.second;
        cRegList.push_back(cItem);
    }
    struct
    {
        bool operator()(std::pair<std::string, ChipRegItem> a, std::pair<std::string, ChipRegItem> b) const { return a.second.fPage < b.second.fPage; }
    } customPageInc;

    struct
    {
        bool operator()(std::pair<std::string, ChipRegItem> a, std::pair<std::string, ChipRegItem> b) const { return a.second.fAddress < b.second.fAddress; }
    } customAddressInc;
    std::sort(cRegList.begin(), cRegList.end(), customPageInc);    // all to page0 then page 1
    std::sort(cRegList.begin(), cRegList.end(), customAddressInc); // sort by address

    if(!lpGBTFound())
    {
        // vector to encode all the registers into
        std::vector<uint32_t> cVec;
        for(auto& cRegItem: cRegList)
        {
            // this is to protect from readback errors during Configure as the BandgapFuse and ChipIDFuse registers should
            // be e-fused in the CBC3
            if(cRegItem.first.find("Channel") != std::string::npos && cSkipLocalRegs) continue;
            if(cRegItem.first.find("BandgapFuse") != std::string::npos) continue;
            if(cRegItem.first.find("ChipIDFuse") != std::string::npos) continue;

            // if( cRegItem.second.fPage == 0 )
            //     LOG (DEBUG) << BOLDBLUE << "Writing 0x" << std::hex << +cRegItem.second.fValue << std::dec << " to " <<
            //         cRegItem.first <<  " : register address 0x" << std::hex << +cRegItem.second.fAddress << std::dec
            //         << " on page " << +cRegItem.second.fPage <<  RESET;
            fBoardFW->EncodeReg(cRegItem.second, pCbc, cVec, pVerifLoop, true);
#ifdef COUNT_FLAG
            fRegisterCount++;
#endif
        }

        // write the registers, the answer will be in the same cVec
        // the number of times the write operation has been attempted is given by cWriteAttempts
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
#ifdef COUNT_FLAG
        fTransactionCount++;
#endif
    }
    else
    {
        std::vector<std::pair<std::string, uint16_t>> cRegsToWrite;
        cRegsToWrite.clear();
        for(auto& cRegItem: cRegList)
        {
            // this is to protect from readback errors during Configure as the BandgapFuse and ChipIDFuse registers should
            // be e-fused in the CBC3
            if(cRegItem.first.find("Channel") != std::string::npos && cSkipLocalRegs) continue;
            if(cRegItem.first.find("BandgapFuse") != std::string::npos) continue;
            if(cRegItem.first.find("ChipIDFuse") != std::string::npos) continue;

            std::pair<std::string, uint16_t> cRegToWrite;
            cRegToWrite.first  = cRegItem.first;
            cRegToWrite.second = cRegItem.second.fValue;
            cRegsToWrite.push_back(cRegToWrite);
        }
        cSuccess = this->WriteChipMultReg(pCbc, cRegsToWrite, pVerifLoop);
    }

    return cSuccess;
}

bool CbcInterface::enableInjection(ReadoutChip* pChip, bool inject, bool pVerifLoop) { return this->WriteChipReg(pChip, "TestPulse", (int)inject, pVerifLoop); }

bool CbcInterface::setInjectionAmplitude(ReadoutChip* pChip, uint8_t injectionAmplitude, bool pVerifLoop) { return this->WriteChipReg(pChip, "TestPulsePotNodeSel", injectionAmplitude, pVerifLoop); }

bool CbcInterface::setInjectionSchema(ReadoutChip* pCbc, const ChannelGroupBase* group, bool pVerifLoop)
{
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
    LOG(DEBUG) << BOLDBLUE << "\t... Applying mask to CBC" << +pCbc->getId() << " with " << group->getNumberOfEnabledChannels()
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
bool CbcInterface::injectStubs(ReadoutChip* pCbc, std::vector<uint8_t> pStubAddresses, std::vector<int> pStubBends, bool pUseNoise, bool pUseOffsets, uint8_t pAllOff)
{
    setBoard(pCbc->getBeBoardId());

    ChannelGroup<NCHANNELS, 1> cChannelMask;
    cChannelMask.disableAllChannels();
    std::vector<uint8_t> cActiveChannels(0);
    std::vector<uint8_t> cDisabledChannels(0);
    for(size_t cIndex = 0; cIndex < pStubAddresses.size(); cIndex += 1)
    {
        std::vector<uint8_t> cPattern = this->stubInjectionPattern(pCbc, pStubAddresses[cIndex], pStubBends[cIndex]);
        // for(auto cChannel: cPattern) cChannelMask.enableChannel(cChannel);
        for(size_t cChnl = 0; cChnl < pCbc->size(); cChnl++)
        {
            if(std::find(cPattern.begin(), cPattern.end(), cChnl) != cPattern.end())
            {
                cActiveChannels.push_back(cChnl);
                cChannelMask.enableChannel(cChnl);
            }
            else
                cDisabledChannels.push_back(cChnl);
        }
    }
    if(!pUseNoise) // with TP
    {
        uint16_t               cFirstHit = 0;
        std::bitset<NCHANNELS> cBitset   = std::bitset<NCHANNELS>(cChannelMask.getBitset());
        LOG(DEBUG) << BOLDMAGENTA << "Bitset for this mask is " << cBitset << RESET;
        for(cFirstHit = 0; cFirstHit < NCHANNELS; cFirstHit++)
        {
            if(cBitset[cFirstHit] != 0) break;
        }
        uint8_t cGroupId = std::floor((cFirstHit % 16) / 2);
        LOG(DEBUG) << BOLDBLUE << "First unmasked channel in position " << +cFirstHit << " --- i.e. in TP group " << +cGroupId << RESET;
        if(cGroupId > 7)
            throw Exception("bool CbcInterface::setInjectionSchema (ReadoutChip* pCbc, const ChannelGroupBase *group, "
                            "bool pVerifLoop): CBC is not able to inject the channel pattern");
        // write register which selects group
        this->WriteChipReg(pCbc, "TestPulseGroup", cGroupId);
        // write registers which enable injection
        this->enableInjection(pCbc, true); // enable injection
        // write register which sets TP amplitude
        this->setInjectionAmplitude(pCbc, 0xFF - 100); // fix injection amplitude
        return this->maskChannelsGroup(pCbc, &cChannelMask);
    }
    else // with noise
    {
        // assuming global chip threshold
        // is already at the pedestal
        if(pUseOffsets)
        {
            uint8_t cAllOff = pAllOff;
            uint8_t cAllOn  = 0x00;
            // use offsets on individual disc. to shift output to all 1 or all 0
            // always off for disabled channels
            std::vector<std::pair<std::string, uint16_t>> cVecReq;
            for(auto cDisabledChannel: cDisabledChannels)
            {
                char cDacName[20];
                sprintf(cDacName, "Channel%03d", cDisabledChannel + 1);
                cVecReq.push_back({cDacName, cAllOff});
            }
            // always on for active channels
            for(auto cActiveChannel: cActiveChannels)
            {
                char cDacName[20];
                sprintf(cDacName, "Channel%03d", cActiveChannel + 1);
                cVecReq.push_back({cDacName, cAllOn});
            }
            return this->WriteChipMultReg(pCbc, cVecReq, true);
        }
        else
        {
            uint16_t cVcth = 1023;
            this->WriteChipReg(pCbc, "VCth", cVcth);
            return this->maskChannelsGroup(pCbc, &cChannelMask);
        }
    }
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
    uint8_t cErrorReg = 0xFF;
    if(!lpGBTFound())
    {
        setBoard(pCbc->getBeBoardId());
        std::vector<uint32_t> cVecReq;
        // fBoardFW->EncodeReg(cRegItem, pCbc->getHybridId(), pCbc->getId(), cVecReq, true, false);
        fBoardFW->EncodeReg(cRegItem, pCbc, cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        // bools to find the values of failed and read
        bool    cFailed = false;
        bool    cRead;
        uint8_t cChipId;
        fBoardFW->DecodeReg(cRegItem, cChipId, cVecReq[0], cRead, cFailed);
        std::pair<bool, uint16_t> cReadBack = std::make_pair(!cFailed, cRegItem.fValue);
        if(cReadBack.first) cErrorReg = cReadBack.second;
    }
    else
    {
        bool cVerifLoop = true;
        bool cSuccess   = ConfigurePage(pCbc, cRegItem.fPage, cVerifLoop);
        if(cSuccess) cErrorReg = fBoardFW->ReadFERegister(pCbc, cRegItem.fAddress);
    }
    return cErrorReg;
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
    // +pCbc->getId() << RESET;
    return WriteChipSingleReg(pCbc, cRegName, cRegValue, pVerifLoop);
    // WriteChipSingleReg
    // uint8_t cMask = (uint8_t)(~(((3*pForHits) << 6) | ((3*pForStubs) << 4)));
    // uint8_t cValue = (((pMode*pForHits) << 6) | ((pMode*pForStubs) << 4)) | (cOriginalValue & cMask);
    // LOG (DEBUG) << BOLDBLUE << "Settinng logic selection register to 0x" << std::hex << +cValue << std::dec << " to
    // select " << pModeSelect << " mode on CBC" << +pCbc->getId() << RESET; cRegVec.emplace_back (cRegName, cValue);
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
    // enable max. " << +pClocks << " clocks on CBC" << +pCbc->getId() << RESET;
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
    std::lock_guard<std::mutex> theGuard(fMutex);
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
    else if(dacName == "ClusterCut")
    {
        uint8_t cRegValue = ReadChipSingleReg(pCbc, "LayerSwap&CluWidth");
        uint8_t cValue    = (cRegValue & 0xF8) | dacValue;
        return WriteChipSingleReg(pCbc, "LayerSwap&CluWidth", cValue, pVerifLoop);
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
                // LOG(INFO) << BOLDBLUE << "Setting latency on " << +pCbc->getId() << " to " << +dacValue << " 0x" << std::hex << +cLat1 << std::dec << " --- 0x" << std::hex << +cLat2 << std::dec
                //            << " for a latency vale of " << dacValue << RESET;
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
        LOG(DEBUG) << BOLDBLUE << "Setting test pulse register on CBC" << +pCbc->getId() << " to select group " << +dacValue << " --  register to  0x" << std::bitset<8>(+cRegValue) << std::dec
                   << RESET;
        return WriteChipSingleReg(pCbc, "TestPulseDel&ChanGroup", cRegValue, pVerifLoop);
    }
    else if(dacName == "AmuxOutput")
    {
        uint8_t cValue    = pCbc->getReg("MiscTestPulseCtrl&AnalogMux");
        uint8_t cRegValue = (cValue & 0xE0) | dacValue;
        LOG(DEBUG) << BOLDBLUE << "Setting AmuxOutput on Chip" << +pCbc->getId() << " to " << +dacValue << " - register set to : 0x" << std::hex << +cRegValue << std::dec << RESET;
        bool cSuccess = WriteChipSingleReg(pCbc, "MiscTestPulseCtrl&AnalogMux", cRegValue, pVerifLoop);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return cSuccess;
    }
    else if(dacName == "TestPulse" || dacName == "InjectedCharge")
    {
        uint8_t cValue    = pCbc->getReg("MiscTestPulseCtrl&AnalogMux");
        uint8_t cRegValue = (cValue & 0xBF) | (dacValue << 6);
        return WriteChipSingleReg(pCbc, "MiscTestPulseCtrl&AnalogMux", cRegValue, pVerifLoop);
    }
    else if(dacName == "HitOr")
    {
        uint8_t cValue    = pCbc->getReg("40MhzClk&Or254");
        uint8_t cRegValue = (cValue & 0xBf) | (dacValue << 6);
        LOG(DEBUG) << BOLDBLUE << "Setting HITOr on Chip" << +pCbc->getId() << " from 0x" << std::hex << +cValue << std::dec << " to " << +dacValue << " - register set to : 0x" << std::hex
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
        LOG(DEBUG) << BOLDBLUE << "Setting clock delay on Chip" << +pCbc->getId() << " to " << std::bitset<5>(+dacValue) << " - register set to : 0x" << std::hex << +cNewRegValue << std::dec << RESET;
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
bool CbcInterface::ConfigurePage(Chip* pCbc, uint8_t pPage, bool pVerifLoop)
{
    // only written for optical .. electrical readout the fw takes care of this
    bool cSuccess = !lpGBTFound();
    if(cSuccess) return cSuccess;

    // address in map depends on board id, hybrid id, chip id
    uint32_t cAddress = (pCbc->getBeBoardId() << 16) | (pCbc->getHybridId() << 8) | pCbc->getId();
    auto     cIter    = fPageMap.find(cAddress);
    if(cIter == fPageMap.end())
    {
        uint8_t cDefaultPage = 0;
        fPageMap.insert(std::make_pair(cAddress, cDefaultPage));
    }
    cIter         = fPageMap.find(cAddress);
    uint8_t cPage = cIter->second;
    cSuccess      = (cPage == pPage);
    // don't need to change page
    if(cSuccess) return true;

    // switch page
    // LOG (INFO) << BOLDMAGENTA << "Switching page on CBC#" << +pCbc->getId() << " on hybrid " << +pCbc->getHybridId()
    //     << " from page " <<+cPage
    //     << " to page " << +pPage
    //     << RESET;
    ChipRegItem cPageReg  = pCbc->getRegItem("FeCtrl&TrgLat2");
    uint8_t     cRegValue = (cPageReg.fValue & 0x7F) | (pPage << 7);
    // LOG (INFO) << BOLDBLUE << "\t...Current page is " << cPage << " want to write to page " << +pPage << " need to update page register on the CBC" << RESET;
    // update page in map
    cIter->second = pPage;
    // write to page register in the CBC
    cSuccess = fBoardFW->WriteFERegister(pCbc, cPageReg.fAddress, cRegValue, pVerifLoop);
    // return false if this didn't work
    if(!cSuccess) return cSuccess;
    // update register value in map
    pCbc->setReg("FeCtrl&TrgLat2", cRegValue);
    return cSuccess;
}
bool CbcInterface::WriteChipSingleReg(Chip* pCbc, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop)
{
    // first, identify the correct BeBoardFWInterface
    setBoard(pCbc->getBeBoardId());
    bool cSuccess = false;
    bool cFound   = pCbc->getRegMap().find(pRegNode) != pCbc->getRegMap().end();
    if(pRegNode.find("BandgapFuse") != std::string::npos)
    {
        LOG(ERROR) << "Cbc register  " << pRegNode << " is READ ONLY." << RESET;
        return cSuccess;
    }
    if(pValue > 0xFF)
    {
        LOG(ERROR) << "Cbc register are 8 bits, impossible to write " << pValue << " on registed " << pRegNode;
        return cSuccess;
    }

    ChipRegItem cRegItem;
    // modified registers map
    uint32_t cChipId = (uint8_t)(pCbc->getFrontEndType() == FrontEndType::MPA || pCbc->getFrontEndType() == FrontEndType::RD53) << 12;
    cChipId          = cChipId | pCbc->getOpticalId() << 8 | pCbc->getHybridId() << 4 | pCbc->getId();
    auto cMapIter    = fModifiedRegisters.find(cChipId);
    if(cMapIter == fModifiedRegisters.end())
    {
        ChipRegMap cRegMap;
        fModifiedRegisters[cChipId] = cRegMap;
    }
    cMapIter      = fModifiedRegisters.find(cChipId);
    auto& cModMap = cMapIter->second;
    if(cFound)
    {
        cRegItem = pCbc->getRegItem(pRegNode);
        // update map with value before it has been modified
        if(cModMap.find(pRegNode) == cModMap.end()) cModMap[pRegNode] = cRegItem;
    }
    else
    {
        return cFound;
    }
    cRegItem.fValue = pValue & 0xFF;

    if(!lpGBTFound())
    {
        // vector for transaction
        std::vector<uint32_t> cVec;

        // encode the reg specific to the FW, pVerifLoop decides if it should be read back, true means to write it
        fBoardFW->EncodeReg(cRegItem, pCbc, cVec, pVerifLoop, true);
        // fBoardFW->EncodeReg(cRegItem, pCbc->getHybridId(), pCbc->getId(), cVec, pVerifLoop, true);
        // write the registers, the answer will be in the same cVec
        // the number of times the write operation has been attempted is given by cWriteAttempts
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
    }
    else
    {
        cSuccess = ConfigurePage(pCbc, cRegItem.fPage, pVerifLoop);
        if(!cSuccess) return cSuccess;
        // read only  register
        if(pRegNode.find("ChipIDFuse") != std::string::npos) { pVerifLoop = false; }
        cSuccess = fBoardFW->WriteFERegister(pCbc, cRegItem.fAddress, pValue, pVerifLoop);
    }
    // update the HWDescription object
    if(cSuccess)
    {
        // LOG (INFO) << BOLDMAGENTA << "CbcInterface::WriteChipSingleReg written 0x"
        //     << std::hex << +pValue << std::dec
        //     << " to register " << pRegNode
        //     << RESET;
        pCbc->setReg(pRegNode, pValue);
    }
#ifdef COUNT_FLAG
    fRegisterCount++;
    fTransactionCount++;
#endif

    return cSuccess;
}
uint8_t CbcInterface::GetLastPage(Chip* pCbc)
{
    uint32_t cAddress = (pCbc->getBeBoardId() << 16) | (pCbc->getHybridId() << 8) | pCbc->getId();
    auto     cIter    = fPageMap.find(cAddress);
    if(cIter == fPageMap.end())
        return 6;
    else
        return cIter->second;
}
bool CbcInterface::WriteChipMultReg(Chip* pCbc, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop)
{
    // remember to sort by page . that is helpful for speeding things up later
    std::vector<std::pair<std::string, ChipRegItem>> cRegItems;
    struct
    {
        bool operator()(std::pair<std::string, ChipRegItem> a, std::pair<std::string, ChipRegItem> b) const { return a.second.fPage < b.second.fPage; }
    } customLessForPage;
    struct
    {
        bool operator()(std::pair<std::string, ChipRegItem> a, std::pair<std::string, ChipRegItem> b) const { return a.second.fPage > b.second.fPage; }
    } customGreaterForPage;
    for(auto& cReg: pVecReq)
    {
        if(cReg.first.find("BandgapFuse") != std::string::npos) continue;
        if(cReg.first.find("ChipIDFuse") != std::string::npos) continue;

        ChipRegItem cRegister = pCbc->getRegItem(cReg.first);
        cRegister.fValue      = cReg.second;
        cRegItems.push_back(std::make_pair(cReg.first, cRegister));
    }
    // address in map depends on board id, hybrid id, chip id
    uint32_t cAddress = (pCbc->getBeBoardId() << 16) | (pCbc->getHybridId() << 8) | pCbc->getId();
    auto     cIter    = fPageMap.find(cAddress);
    if(cIter == fPageMap.end())
    {
        uint8_t cDefaultPage = 0;
        fPageMap.insert(std::make_pair(cAddress, cDefaultPage));
    }
    cIter         = fPageMap.find(cAddress);
    uint8_t cPage = cIter->second;
    if(cPage == 1) std::sort(cRegItems.begin(), cRegItems.end(), customGreaterForPage); // all to page1 then page 0
    if(cPage == 0) std::sort(cRegItems.begin(), cRegItems.end(), customLessForPage);    // all to page0 then page 1
    // first, identify the correct BeBoardFWInterface
    setBoard(pCbc->getBeBoardId());
    bool cSuccess = false;
    if(!lpGBTFound())
    {
        // Deal with the ChipRegItems and encode them
        std::vector<uint32_t> cVec;
        for(const auto& cRegItem: cRegItems)
        {
            if(cRegItem.second.fValue > 0xFF)
            {
                LOG(ERROR) << "Cbc register are 8 bits, impossible to write " << cRegItem.second.fValue << " on register " << cRegItem.first;
                continue;
            }
            fBoardFW->EncodeReg(cRegItem.second, pCbc, cVec, pVerifLoop, true);
// fBoardFW->EncodeReg(cRegItem.second, pCbc->getHybridId(), pCbc->getId(), cVec, pVerifLoop, true);
#ifdef COUNT_FLAG
            fRegisterCount++;
#endif
        }

        // write the registers, the answer will be in the same cVec
        // the number of times the write operation has been attempted is given by cWriteAttempts
        uint8_t cWriteAttempts = 0;
        if(cVec.size() == 0) return true;

        cSuccess = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
#ifdef COUNT_FLAG
        fTransactionCount++;
#endif
        // if the transaction is successfull, update the HWDescription object
        if(cSuccess)
        {
            for(const auto& cRegItem: cRegItems) { pCbc->setReg(cRegItem.first, cRegItem.second.fValue); }
        }
    }
    else
    {
        for(const auto& cRegItem: cRegItems)
        {
            bool cRegWriteSuccess = this->WriteChipSingleReg(pCbc, cRegItem.first, cRegItem.second.fValue, pVerifLoop);
            if(!cRegWriteSuccess) LOG(INFO) << BOLDRED << "Failed to write to " << cRegItem.first << RESET;
            cSuccess = cSuccess && cRegWriteSuccess;
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
            // fBoardFW->EncodeReg ( cRegItem, pCbc->getHybridId(), pCbc->getId(), cVec, pVerifLoop, true );
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
uint8_t CbcInterface::ReadChipSingleReg(Chip* pCbc, const std::string& pRegNode)
{
    setBoard(pCbc->getBeBoardId());
    ChipRegItem cRegItem = pCbc->getRegItem(pRegNode);
    uint8_t     cValue   = 0x00;
    if(!lpGBTFound())
    {
        std::vector<uint32_t> cVecReq;
        fBoardFW->EncodeReg(cRegItem, pCbc, cVecReq, true, false);
        // fBoardFW->EncodeReg( cRegItem,  pCbc->getHybridId(), pCbc->getId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        bool    cRead;
        uint8_t cCbcId;
        bool    cFailed = false;
        fBoardFW->DecodeReg(cRegItem, cCbcId, cVecReq[0], cRead, cFailed);
        cValue = cRegItem.fValue;
    }
    else
    {
        bool cVerifLoop = true;
        bool cSuccess   = ConfigurePage(pCbc, cRegItem.fPage, cVerifLoop);
        if(cSuccess) cValue = fBoardFW->ReadFERegister(pCbc, cRegItem.fAddress);
    }
    pCbc->setReg(pRegNode, cRegItem.fValue);

    return cValue;
}
uint16_t CbcInterface::ReadChipReg(Chip* pCbc, const std::string& pRegNode)
{
    std::lock_guard<std::mutex> theGuard(fMutex);
    ChipRegItem                 cRegItem;
    setBoard(pCbc->getBeBoardId());
    std::vector<uint32_t> cVecReq;
    if(pRegNode == "VCth")
    {
        uint8_t  cReg0      = ReadChipSingleReg(pCbc, "VCth1");
        uint8_t  cReg1      = ReadChipSingleReg(pCbc, "VCth2");
        uint16_t cThreshold = ((cReg1 & 0x3) << 8) | cReg0;
        return cThreshold;
    }
    else if(pRegNode == "Threshold")
    {
        return ReadChipReg(pCbc, "VCth");
    }
    else if(pRegNode == "HitLogic")
    {
        uint8_t cRegValue = ReadChipSingleReg(pCbc, "Pipe&StubInpSel&Ptwidth");
        return (cRegValue & 0xC0) >> 6;
    }
    else if(pRegNode == "StubLogic")
    {
        uint8_t cRegValue = ReadChipSingleReg(pCbc, "Pipe&StubInpSel&Ptwidth");
        return (cRegValue & 0x30) >> 4;
    }
    else if(pRegNode == "HitOr")
    {
        uint8_t cRegValue = ReadChipSingleReg(pCbc, "Pipe&StubInpSel&Ptwidth");
        return (cRegValue & 0x40) >> 6;
    }
    else if(pRegNode == "LayerSwap")
    {
        uint8_t cRegValue = ReadChipSingleReg(pCbc, "LayerSwap&CluWidth");
        return (cRegValue & 0x08) >> 3;
    }
    else if(pRegNode == "PtCut")
    {
        uint8_t cRegValue = ReadChipSingleReg(pCbc, "Pipe&StubInpSel&Ptwidth");
        return (cRegValue & 0x0F);
    }
    else if(pRegNode == "ChipId")
    {
        return ReadCbcIDeFuse(pCbc);
    }
    else if(pRegNode == "TriggerLatency")
    {
        auto cRegValueFirst = ReadChipSingleReg(pCbc,"FeCtrl&TrgLat2"); 
        auto cRegValueSecond = ReadChipSingleReg(pCbc,"TriggerLatency1"); 
        uint16_t cLatency = ((cRegValueFirst& 0x1) << 8) | cRegValueSecond; 
        return cLatency; 
    }
    else
    {
        return ReadChipSingleReg(pCbc, pRegNode) & 0xFF;
    }
}

void CbcInterface::WriteHybridBroadcastChipReg(const Hybrid* pHybrid, const std::string& pRegNode, uint16_t pValue)
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

void CbcInterface::WriteBroadcastCbcMultiReg(const Hybrid* pHybrid, const std::vector<std::pair<std::string, uint8_t>> pVecReg)
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
    // make fuse read-able
    WriteChipReg(pCbc, "ChipIDFuse3", 8);
    uint8_t  IDa     = ReadChipReg(pCbc, "ChipIDFuse1");
    uint8_t  IDb     = ReadChipReg(pCbc, "ChipIDFuse2");
    uint8_t  IDc     = ReadChipReg(pCbc, "ChipIDFuse3");
    uint32_t IDeFuse = ((IDa)&0x000000FF) + (((IDb) << 8) & 0x0000FF00) + (((IDc) << 16) & 0x000F0000);
    return IDeFuse;
}

} // namespace Ph2_HwInterface
