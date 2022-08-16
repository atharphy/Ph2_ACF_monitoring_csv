/*!
  \file                  RD53BInterface.h
  \brief                 User interface to the RD53B readout chip
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#include "RD53BInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
bool RD53BInterface::ConfigureChip(Chip* pChip, bool pVerifLoop, uint32_t pBlockSize)
{
    this->setBoard(pChip->getBeBoardId());

    auto pRD53 = static_cast<RD53*>(pChip);
    ChipRegMap& pRD53RegMap = pChip->getRegMap();

    // @TMP@ : what is this?
    RD53Interface::WriteChipReg(pChip, "PIX_DEFAULT_CONFIG", 0x9CE2, pVerifLoop);
    RD53Interface::WriteChipReg(pChip, "PIX_DEFAULT_CONFIG_B", 0x631D, pVerifLoop);

    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    // ######################
    // # Reset Core Columns #
    // ######################
    RD53BInterface::ResetCoreColumns(pRD53);

    // @TMP@ : what is this?
    RD53Interface::WriteChipReg(pChip, "TriggerConfig", 0, pVerifLoop);
    RD53Interface::WriteChipReg(pChip, "DataConcentratorConf", 0, pVerifLoop);
    RD53Interface::WriteChipReg(pChip, "CoreColEncoderConf", 0, pVerifLoop);

    // ################################################
    // # Programming global registers from white list #
    // ################################################
    static const char* registerWhileList[] = {"DAC_PREAMP_L_DIFF", "DAC_PREAMP_R_DIFF", "DAC_PREAMP_TL_DIFF", "DAC_PREAMP_TR_DIFF", "DAC_PREAMP_T_DIFF", "DAC_PREAMP_M_DIFF",
                                              "DAC_PRECOMP_DIFF",  "DAC_COMP_DIFF",     "DAC_VFF_DIFF",       "DAC_TH1_L_DIFF",     "DAC_TH1_R_DIFF",    "DAC_TH1_M_DIFF",
                                              "DAC_TH2_DIFF",      "DAC_LCC_DIFF",      "DAC_PREAMP_L_LIN",   "DAC_PREAMP_R_LIN",   "DAC_PREAMP_TL_LIN", "DAC_PREAMP_TR_LIN",
                                              "DAC_PREAMP_T_LIN",  "DAC_PREAMP_M_LIN",  "DAC_FC_LIN",         "DAC_KRUM_CURR_LIN",  "DAC_REF_KRUM_LIN",  "DAC_COMP_LIN",
                                              "DAC_COMP_TA_LIN",   "DAC_GDAC_L_LIN",    "DAC_GDAC_R_LIN",     "DAC_GDAC_M_LIN",     "DAC_LDAC_LIN"}; // @CONST@

    for(auto i = 0u; i < ArraySize(registerWhileList); i++)
    {
        auto it = pRD53RegMap.find(registerWhileList[i]);
        if(it != pRD53RegMap.end()) RD53Interface::WriteChipReg(pChip, it->first, it->second.fValue, pVerifLoop);
    }

    // #######################################
    // # Programming CLK_DATA_DELAY register #
    // #######################################
    static const char*               registerClkDataDelayList[] = {"CLK_DATA_DELAY", "CLK_DATA_DELAY_DATA", "CLK_DATA_DELAY_CLK"}; // @CONST@
    std::pair<std::string, uint16_t> nameAndValue("CLK_DATA_DELAY", pRD53RegMap["CLK_DATA_DELAY"].fValue);
    bool                             doWriteClkDataDelay = false;

    for(auto i = 0u; i < ArraySize(registerClkDataDelayList); i++)
    {
        auto cRegItem = pRD53RegMap.find(registerClkDataDelayList[i]);
        if((cRegItem != pRD53RegMap.end()) && (cRegItem->second.fPrmptCfg == true))
        {
            doWriteClkDataDelay = true;

            nameAndValue.second |= SplitSpecialRegisters(std::string(cRegItem->first), cRegItem->second.fValue, pRD53RegMap).second;

            if(cRegItem->first == "CLK_DATA_DELAY") break;
        }
    }

    if(doWriteClkDataDelay == true)
    {
        RD53Interface::WriteChipReg(pChip, nameAndValue.first, nameAndValue.second, false);
        static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(std::vector<uint16_t>(RD53Constants::NSYNC_WORS, RD53BCmd::RD53BCmdEncoder::SYNC), -1);
        RD53Interface::WriteChipReg(pChip, nameAndValue.first, nameAndValue.second, pVerifLoop);
    }

    // ###############################
    // # Programmig global registers #
    // ###############################
    static const std::set<std::string> registerBlackList = {"ADC_OFFSET_VOLT", "ADC_MAXIMUM_VOLT", "TEMPSENS_IDEAL_FACTOR", "CLK_DATA_DELAY", "CLK_DATA_DELAY_DATA", "CLK_DATA_DELAY_CLK"};

    for(auto& cRegItem: pRD53RegMap)
        if(cRegItem.second.fPrmptCfg == true)
        {
            if(registerBlackList.find(cRegItem.first) != registerBlackList.end()) continue;

            if(cRegItem.first == "CDR_CONFIG")
            {
                RD53Interface::SendCommand(pRD53, RD53BCmd::Clear{});
                RD53Interface::SendCommand(pRD53, RD53BCmd::Clear{});
                std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
            }

            RD53Interface::WriteChipReg(pChip, cRegItem.first, cRegItem.second.fValue, pVerifLoop);
        }

    // ###################################
    // # Programmig pixel cell registers #
    // ###################################
    RD53BInterface::WriteRD53Mask(pRD53, false, true);

    return true;
}

void RD53BInterface::SendGlobalPulse(Chip* pChip, uint16_t route, uint16_t pulseDuration)
{
    std::vector<uint16_t> cmdStream;
    PackWriteCommand(pChip, "GlobalPulseConf", route, cmdStream);
    PackWriteCommand(pChip, "GlobalPulseWidth", pulseDuration, cmdStream);
    serialize(RD53BCmd::GlobalPulse{pChip->getId()}, cmdStream);
    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(cmdStream, pChip->getHybridId());
}

void RD53BInterface::SendGlobalPulseBroadcast(const BeBoard* pBoard, uint16_t route, uint16_t pulseDuration)
{
    std::vector<uint16_t> cmdStream;
    serialize(RD53BCmd::WrReg{RD53BConstants::BROADCAST_CHIPID, 61, route}, cmdStream);
    serialize(RD53BCmd::WrReg{RD53BConstants::BROADCAST_CHIPID, 62, pulseDuration}, cmdStream);
    serialize(RD53BCmd::GlobalPulse{RD53BConstants::BROADCAST_CHIPID}, cmdStream);
    SendChipCommands(pBoard, cmdStream, -1);
}

void RD53BInterface::InitRD53Downlink(const BeBoard* pBoard)
{
    this->setBoard(pBoard->getId());

    LOG(INFO) << GREEN << "Down-link phase initialization..." << RESET;

    // @TMP@ : what is this?
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "GCR_DEFAULT_CONFIG", 0xAC75);
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "GCR_DEFAULT_CONFIG_B", 0x538A);
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "CMDERR_CNT", 0);

    // ##############
    // # Link speed #
    // ##############
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "CDR_CONFIG_SEL_SER_CLK", static_cast<RD53FWInterface*>(fBoardFW)->ReadoutSpeed() == RD53FWconstants::ReadoutSpeed::x1280 ? 0 : 1);

    // ##########
    // # Resets #
    // ##########
    RD53BInterface::SendGlobalPulseBroadcast(pBoard, 7, 0xFF); // ResetChannelSynchronizer, ResetCommandDecoder, ResetGlobalConfiguration
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "RingOscConfig", 0x7FFF);
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "RingOscConfig", 0x5EFF);
    RD53BInterface::SendGlobalPulseBroadcast(pBoard, 1 << 8, 0xFF); // ResetEfuses

    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
}

void RD53BInterface::InitRD53Uplinks(ReadoutChip* pChip, int nActiveLanes)
{
    LOG(INFO) << GREEN << "Configuring up-link lanes and monitoring..." << RESET;

    RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", 0x0055, false);
    size_t hybridId = pChip->getHybridId();
    // @TMP@ : what is this?
    if(hybridId >= 2)
        RD53Interface::WriteChipReg(pChip, "CML_CONFIG", 15, false);
    else
        RD53Interface::WriteChipReg(pChip, "CML_CONFIG", 1, false);
    RD53Interface::WriteChipReg(pChip, "AuroraConfig", bits::pack<4, 6, 2>(1, 25, 3), false);
    uint16_t val;
    // @TMP@ : what is this?
    if(hybridId >= 2)
        val = bits::pack<2, 2, 2, 2, 2, 2, 2, 2>(0, 1, 2, 3, 0, 1, 2, 3);
    else
        val = bits::pack<2, 2, 2, 2, 2, 2, 2, 2>(3, 2, 1, 0, 3, 2, 1, 0);
    RD53Interface::WriteChipReg(pChip, "DataMergingMux", val, false);
    RD53Interface::WriteChipReg(pChip, "ServiceDataConf", (1 << 8) | 50, false);
    RD53Interface::WriteChipReg(pChip, "AURORA_CB_CONFIG0", 0x0FF1, false);
    RD53Interface::WriteChipReg(pChip, "AURORA_CB_CONFIG1", 0x0000, false);

    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    // RD53BInterface::Reset(pChip, 4, 0xFF);
    // RD53BInterface::Reset(pChip, 5, 0xFF);
    RD53BInterface::SendGlobalPulse(pChip, 0b110000, 0xFF);
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    RD53Interface::SendCommand(pChip, RD53BCmd::Clear{pChip->getId()});

    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
}

std::vector<std::pair<uint16_t, uint16_t>> RD53BInterface::ReadRD53Reg(ReadoutChip* pChip, const std::string& regName)
{
    this->setBoard(pChip->getBeBoardId());

    RD53Interface::SendCommand(pChip, RD53BCmd::RdReg{pChip->getId(), pChip->getRegItem(regName).fAddress});
    auto regReadback = static_cast<RD53FWInterface*>(fBoardFW)->ReadChipRegisters(pChip);

    for(auto i = 0u; i < regReadback.size(); i++)
        // Removing bit related to PIX_PORTAL register identification
        regReadback[i].first = regReadback[i].first & static_cast<uint16_t>(RD53Shared::setBits(RD53Constants::NBIT_ADDR));

    return regReadback;
}

std::pair<std::string, uint16_t> RD53BInterface::SplitSpecialRegisters(std::string regName, uint16_t value, ChipRegMap& pRD53RegMap)
{
    static const std::map<std::string, RD53Interface::SpecialRegInfo> specialRegMap = {{"CDR_CONFIG_SEL_SER_CLK", {"CDR_CONFIG", 0}},

                                                                                       {"CLK_DATA_DELAY_DATA", {"CLK_DATA_DELAY", 0}},
                                                                                       {"CLK_DATA_DELAY_CLK", {"CLK_DATA_DELAY", 7}},

                                                                                       {"MON_ADC_TRIM", {"MON_ADC", 0}},

                                                                                       {"VOLTAGE_TRIM_DIG", {"VOLTAGE_TRIM", 0}},
                                                                                       {"VOLTAGE_TRIM_ANA", {"VOLTAGE_TRIM", 4}},

                                                                                       {"CalibrationConfig_DELAY", {"CalibrationConfig", 0}},

                                                                                       {"CML_CONFIG_SER_EN_TAP", {"CML_CONFIG", 4}},
                                                                                       {"CML_CONFIG_SER_INV_TAP", {"CML_CONFIG", 6}},

                                                                                       {"SER_SEL_OUT_0", {"SER_SEL_OUT", 0}},
                                                                                       {"SER_SEL_OUT_1", {"SER_SEL_OUT", 2}},
                                                                                       {"SER_SEL_OUT_2", {"SER_SEL_OUT", 4}},
                                                                                       {"SER_SEL_OUT_3", {"SER_SEL_OUT", 6}}};

    auto it = specialRegMap.find(regName);
    if(it == specialRegMap.end())
        return {regName, value};
    else
    {
        ChipRegItem& specialReg = pRD53RegMap.at(regName);
        ChipRegItem& Reg        = pRD53RegMap.at(it->second.regName);
        return {it->second.regName, RD53Interface::SetFieldValue(Reg.fValue, value, it->second.start, specialReg.fBitSize)};
    }
}

uint16_t RD53BInterface::GetPixelConfig(const pixelMask& mask, uint16_t row, uint16_t col)
{
    return bits::pack<8, 8>(
        bits::pack<5, 1, 1, 1>(
            mask.TDAC[row + RD53B::NROWS * (col + 1)], mask.HitBus[row + RD53B::NROWS * (col + 1)], mask.InjEn[row + RD53B::NROWS * (col + 1)], mask.Enable[row + RD53B::NROWS * (col + 1)]),
        bits::pack<5, 1, 1, 1>(
            mask.TDAC[row + RD53B::NROWS * (col + 0)], mask.HitBus[row + RD53B::NROWS * (col + 0)], mask.InjEn[row + RD53B::NROWS * (col + 0)], mask.Enable[row + RD53B::NROWS * (col + 0)]));
}

uint16_t RD53BInterface::GetPixelConfigMask(const pixelMask& mask, uint16_t row, uint16_t col)
{
    return bits::pack<5, 5>(bits::pack<2, 1, 1, 1>(0, mask.HitBus[row + RD53B::NROWS * (col + 1)], mask.InjEn[row + RD53B::NROWS * (col + 1)], mask.Enable[row + RD53B::NROWS * (col + 1)]),
                            bits::pack<2, 1, 1, 1>(0, mask.HitBus[row + RD53B::NROWS * (col + 0)], mask.InjEn[row + RD53B::NROWS * (col + 0)], mask.Enable[row + RD53B::NROWS * (col + 0)]));
}

uint16_t RD53BInterface::GetPixelConfigTDAC(const pixelMask& mask, uint16_t row, uint16_t col)
{
    return bits::pack<5, 5>(mask.TDAC[row + RD53B::NROWS * (col + 1)], mask.TDAC[row + RD53B::NROWS * (col + 0)]);
}

void RD53BInterface::ResetCoreColumns(RD53* pRD53)
{
    for(auto suffix : {"_0", "_1", "_2"}) {
        for (int i = 0; i < 2; i++) {
            uint16_t value = 0x55 << i;
            RD53Interface::WriteChipReg(pRD53, std::string("EN_CORE_COL") + suffix, value, false);
            RD53Interface::WriteChipReg(pRD53, std::string("EN_CORE_COL_RESET") + suffix, value, false);
            RD53Interface::SendCommand(pRD53, RD53BCmd::Clear{});
        }

        RD53Interface::WriteChipReg(pRD53, std::string("EN_CORE_COL") + suffix, 0, false);
        RD53Interface::WriteChipReg(pRD53, std::string("EN_CORE_COL_RESET") + suffix, 0, false);
    }

    RD53Interface::WriteChipReg(pRD53, "EN_CORE_COL_3", 0x3F, false);
    RD53Interface::WriteChipReg(pRD53, "EN_CORE_COL_RESET_3", 0x3F, false);
    RD53Interface::SendCommand(pRD53, RD53BCmd::Clear{});

    RD53Interface::WriteChipReg(pRD53, "EN_CORE_COL_3", 0, false);
    RD53Interface::WriteChipReg(pRD53, "EN_CORE_COL_RESET_3", 0, false);
}

void RD53BInterface::WriteRD53Mask(RD53* pRD53, bool doSparse, bool doDefault)
{
    this->setBoard(pRD53->getBeBoardId());

    std::vector<uint16_t> commandList;
    const uint16_t        REGION_COL_ADDR = pRD53->getRegItem("REGION_COL").fAddress;
    const uint16_t        REGION_ROW_ADDR = pRD53->getRegItem("REGION_ROW").fAddress;
    const uint16_t        PIX_MODE_ADDR   = pRD53->getRegItem("PIX_MODE").fAddress;
    const uint8_t         chipID          = pRD53->getId();
    auto&                 mask            = doDefault == true ? pRD53->getPixelsMaskDefault() : pRD53->getPixelsMask();

    // ############
    // # PIX_MODE #
    // ############
    // bit[0]: enable auto-row
    // bit[1]: select mask(0) or TDAC(1)

    auto pixMode = RD53Interface::ReadChipReg(pRD53, "PIX_MODE");

    for(auto col = 0u; col < RD53B::NCOLS; col += 2)
    {
        RD53BCmd::serialize(RD53BCmd::WrReg{chipID, REGION_COL_ADDR, col / 2}, commandList);

        // ####################
        // # Send pixels mask #
        // ####################
        std::vector<uint16_t> dColConfigMask;
        RD53BCmd::serialize(RD53BCmd::WrReg{chipID, REGION_ROW_ADDR, 0x0}, commandList);
        for(auto row = 0u; row < RD53B::NROWS; row++) dColConfigMask.push_back(RD53BInterface::GetPixelConfigMask(mask, row, col));

        RD53BCmd::serialize(RD53BCmd::WrReg{chipID, PIX_MODE_ADDR, 0x1}, commandList);
        RD53BCmd::serialize(RD53BCmd::WrRegLong{chipID, std::move(dColConfigMask)}, commandList);

        // ####################
        // # Send pixels TDAC #
        // ####################
        std::vector<uint16_t> dColConfigTDAC;
        RD53BCmd::serialize(RD53BCmd::WrReg{chipID, REGION_ROW_ADDR, 0x0}, commandList);
        for(auto row = 0u; row < RD53B::NROWS; row++) dColConfigTDAC.push_back(RD53BInterface::GetPixelConfigTDAC(mask, row, col));

        RD53BCmd::serialize(RD53BCmd::WrReg{chipID, PIX_MODE_ADDR, 0x3}, commandList);
        RD53BCmd::serialize(RD53BCmd::WrRegLong{chipID, std::move(dColConfigTDAC)}, commandList);
    }
    RD53BInterface::SendChipCommandsWithSync(pRD53, commandList);

    RD53Interface::WriteChipReg(pRD53, "PIX_MODE", pixMode);
}

void RD53BInterface::SendChipCommandsWithSync(RD53* pRD53, std::vector<uint16_t>& cmdStream)
{
    // Compute number of 16-bit words to which we add 2 sync words every 30:
    // nWordsPerPacketExclSync + 2 * nWordsPerPacketExclSync / 30 = totaNumb16bitWords ( = 2 * (1 << RD53FWconstants::NBIT_SLOWCMD_FIFO))
    constexpr size_t nWordsPerPacketExclSync = 2 * ((1 << RD53FWconstants::NBIT_SLOWCMD_FIFO) - 1) / (1 + 2. / 30);
    auto             begin                   = cmdStream.begin();

    while(begin != cmdStream.end())
    {
        std::vector<uint16_t> cmdPacket;
        size_t                nWordsThisPacketExclSync = std::min(nWordsPerPacketExclSync, size_t(cmdStream.end() - begin));
        cmdPacket.reserve(std::ceil(nWordsThisPacketExclSync + 2 * nWordsThisPacketExclSync / 30.));
        auto it = begin;
        while(it != begin + nWordsThisPacketExclSync)
        {
            auto next = std::min(cmdStream.end(), std::min(it + 30, begin + nWordsThisPacketExclSync));
            std::copy(it, next, std::back_inserter(cmdPacket));
            it = next;
            serialize(RD53BCmd::Sync{}, cmdPacket);
            serialize(RD53BCmd::Sync{}, cmdPacket);
        }
        static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(cmdPacket, pRD53->getHybridId());
        begin = it;
    }
}

void RD53BInterface::Reset(ReadoutChip* pChip, const size_t resetType, const size_t duration)
// #################################################
// # resetType =  0 --> Reset Channel Synchronizer #
// # resetType =  1 --> Reset Command Decoder      #
// # resetType =  2 --> Reset Global Configuration #
// # resetType =  3 --> Reset Service Data         #
// # resetType =  4 --> Reset Aurora               #
// # resetType =  5 --> Reset Serializer           #
// # resetType =  6 --> Reset ADC                  #
// # resetType =  7 --> Reset Data Merging         #
// # resetType =  8 --> Reset Efuses               #
// # resetType =  9 --> Reset Trigger Table        #
// # resetType = 10 --> Reset BCID Counter         #
// # resetType = 11 --> Reset Aurora pattern       #
// #################################################
{
    this->setBoard(pChip->getBeBoardId());

    if(resetType > 10)
        RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_AURORA, false);
    else
        RD53BInterface::SendGlobalPulse(pChip, (size_t)(1 << resetType), duration);
}

void RD53BInterface::ChipErrorReport(ReadoutChip* pChip)
{
    RD53Interface::ChipErrorReport(pChip);

    LOG(INFO) << BOLDBLUE << "READTRIG_CNT        = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "READTRIG_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "RDWRFIFOERROR_CNT   = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "RDWRFIFOERROR_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "PIXELSEU_CNT        = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "PIXELSEU_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "GLOBALCONFIGSEU_CNT = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "GLOBALCONFIGSEU_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
}

void RD53BInterface::PackWriteCommand(Chip* pChip, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg)
{
    RD53BCmd::serialize(RD53BCmd::WrReg{pChip->getId(), pChip->getRegItem(regName).fAddress, data}, chipCommandList);
    if(updateReg == true) pChip->setReg(regName, data);
}

void RD53BInterface::PackWriteBroadcastCommand(const BeBoard* pBoard, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg)
{
    RD53BCmd::serialize(RD53BCmd::WrReg{RD53BConstants::BROADCAST_CHIPID, RD53Shared::firstChip->getRegItem(regName).fAddress, data}, chipCommandList);

    if(updateReg == true)
        for(auto cOpticalGroup: *pBoard)
            for(auto cHybrid: *cOpticalGroup)
                for(auto cChip: *cHybrid) cChip->setReg(regName, data);
}

uint32_t RD53BInterface::ReadChipFuseID(Chip* pChip)
{
    uint16_t low  = RD53Interface::ReadChipReg(pChip, "EfusesReadData0");
    uint16_t high = RD53Interface::ReadChipReg(pChip, "EfusesReadData1");
    return low | (high << pChip->getNumberOfBits("EfusesReadData0"));
}

} // namespace Ph2_HwInterface
