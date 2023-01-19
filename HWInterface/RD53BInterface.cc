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

    auto        pRD53       = static_cast<RD53*>(pChip);
    ChipRegMap& pRD53RegMap = pChip->getRegMap();

    // ########################################################################
    // # Switching to pixel-register configuration, instead of the hard-wired #
    // ########################################################################
    RD53Interface::WriteChipReg(pChip, "PIX_DEFAULT_CONFIG", 0x9CE2, pVerifLoop);
    RD53Interface::WriteChipReg(pChip, "PIX_DEFAULT_CONFIG_B", 0x631D, pVerifLoop);

    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    // ######################
    // # Reset Core Columns #
    // ######################
    RD53BInterface::ResetCoreColumns(pRD53);

    // ##############
    // # Field data #
    // ##############
    RD53Interface::WriteChipReg(pChip, "DataConcentratorConf", 0, false); // To be consistent with RD53B event decoder
    // # bit 12:   EnCRC         --> Map in FormatOptions: enableCRC
    // # bit 11:   EnBCId        --> Map in FormatOptions: enableBCID
    // # bit 10:   EnLv1Id       --> Map in FormatOptions: enableTriggerId
    // # bit 9:    EnEoS         --> Map in FormatOptions: enableEOSmarker
    // # bits 1-8: NumOfEventsInStream[7:0]
    RD53Interface::WriteChipReg(pChip, "CoreColEncoderConf", 0, false);
    // # bit9:     BinaryReadOut --> Map in FormatOptions: enableToT
    // # bit8:     RawData       --> Map in FormatOptions: enableRawMap
    // # bits 4-7: MaxHits[3:0]
    // # bits 1-3: MaxToT[2:0]

    // #######################################
    // # Programming CLK_DATA_DELAY register #
    // #######################################
    static const char* registerClkDataDelayList[] = {"CLK_DATA_DELAY", "CLK_DATA_DELAY_DATA", "CLK_DATA_DELAY_CLK"}; // @CONST@
    bool               doWriteClkDataDelay        = false;

    for(auto i = 0u; i < ArraySize(registerClkDataDelayList); i++)
    {
        auto cRegItem = pRD53RegMap.find(registerClkDataDelayList[i]);
        if((cRegItem != pRD53RegMap.end()) && (cRegItem->second.fPrmptCfg == true))
        {
            doWriteClkDataDelay = true;

            pChip->getRegItem("CLK_DATA_DELAY").fValue = SetSpecialRegister(std::string(cRegItem->first), cRegItem->second.fDefValue, pRD53RegMap).second;

            if(cRegItem->first == "CLK_DATA_DELAY") break;
        }
    }
    if(doWriteClkDataDelay == true) RD53BInterface::WriteClockDataDelay(pChip, pChip->getRegItem("CLK_DATA_DELAY").fValue);

    // ###############################
    // # Programmig global registers #
    // ###############################
    static const std::set<std::string> registerBlackList = {"ADC_OFFSET_VOLT", "ADC_MAXIMUM_VOLT", "TEMPSENS_IDEAL_FACTOR", "VREF_ADC", "CLK_DATA_DELAY", "CLK_DATA_DELAY_DATA", "CLK_DATA_DELAY_CLK"};

    // ################################################
    // # Programming global registers from white list #
    // ################################################
    static const std::set<std::string> registerWhileList = {"DAC_PREAMP_L_LIN",
                                                            "DAC_PREAMP_R_LIN",
                                                            "DAC_PREAMP_TL_LIN",
                                                            "DAC_PREAMP_TR_LIN",
                                                            "DAC_PREAMP_T_LIN",
                                                            "DAC_PREAMP_M_LIN",
                                                            "DAC_FC_LIN",
                                                            "DAC_KRUM_CURR_LIN",
                                                            "DAC_REF_KRUM_LIN",
                                                            "DAC_COMP_LIN",
                                                            "DAC_COMP_TA_LIN",
                                                            "DAC_GDAC_L_LIN",
                                                            "DAC_GDAC_R_LIN",
                                                            "DAC_GDAC_M_LIN",
                                                            "DAC_LDAC_LIN"}; // @CONST@

    for(auto& cRegItem: pRD53RegMap)
        if(((cRegItem.second.fPrmptCfg == true) && (registerBlackList.find(cRegItem.first) == registerBlackList.end())) || (registerWhileList.find(cRegItem.first) != registerWhileList.end()))
        {
            if(cRegItem.first == "CDR_CONFIG")
            {
                RD53Interface::SendCommand(pRD53, RD53BCmd::Clear{});
                std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
            }

            RD53Interface::WriteChipReg(pChip, cRegItem.first, cRegItem.second.fDefValue, pVerifLoop);
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
    serialize(RD53BCmd::WrReg{RD53BConstants::BROADCAST_CHIPID, RD53BConstants::GLOBAL_PULSE_ADDR, route}, cmdStream);
    serialize(RD53BCmd::WrReg{RD53BConstants::BROADCAST_CHIPID, RD53BConstants::GLOBAL_PULSE_ADDR + 1, pulseDuration}, cmdStream);
    serialize(RD53BCmd::GlobalPulse{RD53BConstants::BROADCAST_CHIPID}, cmdStream);
    SendChipCommands(pBoard, cmdStream, -1);
}

void RD53BInterface::InitRD53Downlink(const BeBoard* pBoard)
{
    this->setBoard(pBoard->getId());

    LOG(INFO) << GREEN << "Down-link phase initialization..." << RESET;

    // #######################################################################
    // # Switching to chip-register configuration, instead of the hard-wired #
    // #######################################################################
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "GCR_DEFAULT_CONFIG", 0xAC75);
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "GCR_DEFAULT_CONFIG_B", 0x538A);

    // ##########
    // # Resets #
    // ##########
    RD53BInterface::SendGlobalPulseBroadcast(pBoard, 0b111, 0xFF); // ResetChannelSynchronizer, ResetCommandDecoder, ResetGlobalConfiguration

    // ###################
    // # Ring oscillator #
    // ###################
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "RingOscConfig", 0b111111111111111);
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "RingOscConfig", 0b101111011111111);
    // # bit 15:   RingOscBClear
    // # bit 14:   RingOscBEnBL
    // # bit 13:   RingOscBEnBR
    // # bit 12:   RingOscBEnCAPA
    // # bit 11:   RingOscBEnFF
    // # bit 10:   RingOscBEnLVT
    // # bit 9:    RingOscAClear
    // # bits 1:8: RingOscAEnable[7:0]

    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
}

void RD53BInterface::InitRD53Uplinks(ReadoutChip* pChip)
{
    this->setBoard(pChip->getBeBoardId());
    auto pRD53 = static_cast<RD53*>(pChip);

    LOG(INFO) << GREEN << "Configuring up-link lanes and monitoring..." << RESET;

    RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_AURORA, false);
    // # bits 7-8: SerSelOut3[1:0]
    // # bits 5-6: SerSelOut2[1:0]
    // # bits 3-4: SerSelOut1[1:0]
    // # bits 1-2: SerSelOut0[1:0]
    RD53Interface::WriteChipReg(pChip, "CML_CONFIG", 0b1111, false);
    // # bits 7-8: SER_INV_TAP[1:0]
    // # bits 5-6: SER_EN_TAP[1:0]
    // # bits 1-4: SER_EN_LANE[3:0] --> External output lanes

    // ##############
    // # Link speed #
    // ##############
    RD53Interface::WriteChipReg(
        pChip, "CDR_CONFIG_SEL_SER_CLK", (pRD53->laneConfig.isPrimary == false ? RD53FWconstants::ReadoutSpeed::x320 : static_cast<RD53FWInterface*>(fBoardFW)->ReadoutSpeed()), false);
    RD53Interface::SendCommand(pRD53, RD53BCmd::Clear{});

    RD53Interface::WriteChipReg(pChip, "AuroraConfig", bits::pack<4, 6, 2>(RD53Shared::setBits(pRD53->laneConfig.nOutputLanes), 0b011001, 0b11), false);
    // # bit 14:    SendAltOutput
    // # bit 13:    EnablePRBS
    // # bits 9-12: ActiveLanes[3:0] --> Internal output lanes
    // # bits 3-8:  CCWait[5:0]
    // # bits 1-2:  CCSend[1:0]

    // ################
    // # Data merging #
    // ################
    RD53Interface::WriteChipReg(pChip, "DataMerging", bits::pack<4, 1, 1, 1, 5, 1>(0, 1, 0, 0, pRD53->laneConfig.serializeBits<bool, 5, 1>(pRD53->laneConfig.internalLanesEnabled), 1), false);
    // # bits 10-13: DataMergingInputPolarityInvert[3:0]
    // # bit 9:      EnOutputDataChipId   --> Map in FormatOptions: enableChipId
    // # bit 8:      EnGatingDataMergeClk1280
    // # bit 7:      SelDataMergeClk
    // # bits 3-6:   EnDataMergeLane[3:0] --> Internal input lanes
    // # bit 2:      MergeChBonding       --> Channel bonding
    // # bit 1:      DataMergingGpoSel
    uint16_t val =
        bits::pack<8, 8>(pRD53->laneConfig.serializeBits<uint8_t, 4, 2>(pRD53->laneConfig.inputLaneMapping), pRD53->laneConfig.serializeBits<uint8_t, 4, 2>(pRD53->laneConfig.outputLaneMapping));
    RD53Interface::WriteChipReg(pChip, "DataMergingMux", val, false); // Mux selection for input and output internal lane mapping to external lanes
    // # Internal inputs mapped to external inputs with 2 bits
    // # bits 15-16: DataMergingInMux_3[1:0]
    // # bits 13-14: DataMergingInMux_2[1:0]
    // # bits 11-12: DataMergingInMux_1[1:0]
    // # bits 9-10:  DataMergingInMux_0[1:0]
    // # Internal outputs mapped to external outputs with 2 bits
    // # bits 7-8:   DataMergingOutMux_3[1:0]
    // # bits 5-6:   DataMergingOutMux_2[1:0]
    // # bits 3-4:   DataMergingOutMux_1[1:0]
    // # bits 1-2:   DataMergingOutMux_0[1:0]
    RD53Interface::WriteChipReg(pChip, "ServiceDataConf", 0x100 | 50, false); // How many Data frames to skip before sending a Monitor Frame
    // # bit 9:    EnServiceData
    // # bits 1-8: ServiceFrameSkip [7:0]
    RD53Interface::WriteChipReg(pChip, "AURORA_CB_CONFIG0", 0x0FF1, false);
    // # bits 5-16: CBWait[11:0]
    // # bits 1-4:  CBSend[3:0]
    RD53Interface::WriteChipReg(pChip, "AURORA_CB_CONFIG1", 0x00, false);
    // # bits 1-8: CBWait[19:12]

    // ##########
    // # Resets #
    // ##########
    RD53BInterface::SendGlobalPulse(pChip, 0b110000, 0xFF); // ResetAurora, ResetSerializer

    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
}

std::vector<std::pair<uint16_t, uint16_t>> RD53BInterface::ReadRD53Reg(ReadoutChip* pChip, const std::string& regName)
{
    this->setBoard(pChip->getBeBoardId());

    auto nameAndValue(SetSpecialRegister(regName, 0, pChip->getRegMap()));
    RD53Interface::SendCommand(pChip, RD53BCmd::RdReg{pChip->getId(), pChip->getRegItem(nameAndValue.first).fAddress});
    auto regReadback = static_cast<RD53FWInterface*>(fBoardFW)->ReadChipRegisters(pChip);

    for(auto i = 0u; i < regReadback.size(); i++)
    {
        regReadback[i].first  = regReadback[i].first & static_cast<uint16_t>(RD53Shared::setBits(RD53Constants::NBIT_ADDR)); // Removing bit related to PIX_PORTAL register identification
        regReadback[i].second = RD53BInterface::GetSpecialRegisterValue(regName, regReadback[i].second, pChip->getRegMap());
    }

    return regReadback;
}

std::pair<std::string, uint16_t> RD53BInterface::SetSpecialRegister(std::string regName, uint16_t value, ChipRegMap& pRD53RegMap)
{
    auto it = RD53BInterface::specialRegMap.find(regName);
    if(it == RD53BInterface::specialRegMap.end())
        return {regName, value};
    else
    {
        ChipRegItem& specialReg = pRD53RegMap.at(regName);
        ChipRegItem& Reg        = pRD53RegMap.at(it->second.regName);
        return {it->second.regName, RD53Interface::SetFieldValue(Reg.fValue, value, it->second.start, specialReg.fBitSize)};
    }
}

uint16_t RD53BInterface::GetSpecialRegisterValue(std::string regName, uint16_t value, ChipRegMap& pRD53RegMap)
{
    auto it = RD53BInterface::specialRegMap.find(regName);
    if(it == RD53BInterface::specialRegMap.end())
        return value;
    else
    {
        ChipRegItem& specialReg = pRD53RegMap.at(regName);
        return RD53Interface::GetFieldValue(value, it->second.start, specialReg.fBitSize);
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
    for(auto suffix: {"_0", "_1", "_2"})
    {
        for(int i = 0; i < 2; i++)
        {
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
    const uint16_t        PIX_PORTAL_ADDR = pRD53->getRegItem("PIX_PORTAL").fAddress;
    const uint8_t         chipID          = pRD53->getId();
    auto&                 mask            = doDefault == true ? pRD53->getPixelsMaskDefault() : pRD53->getPixelsMask();

    // ############
    // # PIX_MODE #
    // ############
    // bit[0]: enable auto-row
    // bit[1]: select mask(0) or TDAC(1)
    // bit[2]: enable broadcast

    // ########################
    // # Save original status #
    // ########################
    auto pixMode = RD53Interface::ReadChipReg(pRD53, "PIX_MODE");

    if(doSparse == true)
    {
        // ############################
        // # Clear whole pixel matrix #
        // ############################
        for(auto col = 0u; col < RD53Constants::NROW_CORE; col += 2)
        {
            std::vector<uint16_t> dColConfigMask(RD53B::NROWS, 0x0);
            RD53BCmd::serialize(RD53BCmd::WrReg{chipID, REGION_COL_ADDR, col / 2}, commandList);
            RD53BCmd::serialize(RD53BCmd::WrReg{chipID, REGION_ROW_ADDR, 0x0}, commandList);
            RD53BCmd::serialize(RD53BCmd::WrReg{chipID, PIX_MODE_ADDR, 0x5}, commandList);
            RD53BCmd::serialize(RD53BCmd::WrRegLong{chipID, std::move(dColConfigMask)}, commandList);
        }
        RD53BCmd::serialize(RD53BCmd::WrReg{chipID, PIX_MODE_ADDR, 0x0}, commandList);

        for(auto col = 0u; col < RD53B::NCOLS; col += 2)
        {
            if((std::find(mask.Enable.begin() + (0 + RD53A::NROWS * col), mask.Enable.begin() + (RD53A::NROWS + RD53A::NROWS * col), true) ==
                (mask.Enable.begin() + (RD53A::NROWS + RD53A::NROWS * col))) &&
               (std::find(mask.Enable.begin() + (0 + RD53A::NROWS * (col + 1)), mask.Enable.begin() + (RD53A::NROWS + RD53A::NROWS * (col + 1)), true) ==
                (mask.Enable.begin() + (RD53A::NROWS + RD53A::NROWS * (col + 1)))))
                continue;

            RD53BCmd::serialize(RD53BCmd::WrReg{chipID, REGION_COL_ADDR, col / 2}, commandList);

            for(auto row = 0u; row < RD53B::NROWS; row++)
                if((mask.Enable[row + RD53B::NROWS * col] == true) || (mask.Enable[row + RD53B::NROWS * (col + 1)] == true))
                {
                    auto data = RD53BInterface::GetPixelConfig(mask, row, col);

                    RD53BCmd::serialize(RD53BCmd::WrReg{chipID, REGION_ROW_ADDR, row}, commandList);
                    RD53BCmd::serialize(RD53BCmd::WrReg{chipID, PIX_PORTAL_ADDR, data}, commandList);
                }
        }
    }
    else
    {
        for(auto col = 0u; col < RD53B::NCOLS; col += 2)
        {
            // #######################
            // # Starting pixel cell #
            // #######################
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
    }

    RD53BInterface::SendChipCommandsWithSync(pRD53, commandList);

    // ###########################
    // # Restore original status #
    // ###########################
    RD53Interface::WriteChipReg(pRD53, "PIX_MODE", pixMode);
}

void RD53BInterface::SendChipCommandsWithSync(RD53* pRD53, std::vector<uint16_t>& cmdStream)
{
    const int NSYNC_WORDS = 2;
    // Compute number of 16-bit words to which we add NSYNC_WORDS sync words every RD53Constants::NWORDS_TO_SYNC:
    // nWordsPerPacketExclSync + 2 * nWordsPerPacketExclSync / RD53Constants::NWORDS_TO_SYNC = totaNumb16bitWords ( = 2 * (1 << RD53FWconstants::NBIT_SLOWCMD_FIFO))
    constexpr size_t nWordsPerPacketExclSync = 2 * ((1 << RD53FWconstants::NBIT_SLOWCMD_FIFO) - 1) / (1 + 2. / RD53Constants::NWORDS_TO_SYNC);
    auto             begin                   = cmdStream.begin();

    while(begin != cmdStream.end())
    {
        size_t                nWordsThisPacketExclSync = std::min(nWordsPerPacketExclSync, size_t(cmdStream.end() - begin));
        std::vector<uint16_t> cmdPacket;
        cmdPacket.reserve(std::ceil(nWordsThisPacketExclSync + 2. * nWordsThisPacketExclSync / RD53Constants::NWORDS_TO_SYNC));

        auto it = begin;
        while(it != begin + nWordsThisPacketExclSync)
        {
            auto next = std::min(cmdStream.end(), std::min(it + RD53Constants::NWORDS_TO_SYNC, begin + nWordsThisPacketExclSync));
            std::copy(it, next, std::back_inserter(cmdPacket));
            it = next;
            for(auto i = 0; i < NSYNC_WORDS; i++) serialize(RD53BCmd::Sync{}, cmdPacket);
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

void RD53BInterface::WriteClockDataDelay(Chip* pChip, uint16_t value)
{
    RD53Interface::WriteChipReg(pChip, "CLK_DATA_DELAY", value, false);
    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(std::vector<uint16_t>(RD53Constants::NSYNC_WORDS, RD53BCmd::RD53BCmdEncoder::SYNC), -1);
    RD53Interface::WriteChipReg(pChip, "CLK_DATA_DELAY", value, true);
}

uint32_t RD53BInterface::ReadChipFuseID(Chip* pChip)
{
    uint16_t low  = RD53Interface::ReadChipReg(pChip, "EfusesReadData0");
    uint16_t high = RD53Interface::ReadChipReg(pChip, "EfusesReadData1");
    return low | (high << pChip->getNumberOfBits("EfusesReadData0"));
}

// ###########################
// # Dedicated to monitoring #
// ###########################

uint32_t RD53BInterface::getADCobservable(const std::string& observableName, bool& isCurrentNotVoltage)
// ############################################
// # Possible observable name values are also #
// # - INTERNAL_NTC                           #
// # - INTERNAL_NTC_VOLT                      #
// ############################################
{
    uint32_t voltageObservable(0), currentObservable(0);

    const std::unordered_map<std::string, uint32_t> currentMultiplexer = {{"Iref", 0x00},
                                                                          {"CDR_VCO_MAIN", 0x01},
                                                                          {"CDR_VCO_BUFFER", 0x02},
                                                                          {"CDR_CP_CURR", 0x03},
                                                                          {"CDR_CP_FD", 0x04},
                                                                          {"CDR_CP_BUFFER", 0x05},
                                                                          {"CML_DRIVER_TAP2_BIAS", 0x06},
                                                                          {"CML_DRIVER_TAP1_BIAS", 0x07},
                                                                          {"CML_DRIVER_MAIN", 0x08},
                                                                          {"NTC_CURR", 0x09},
                                                                          {"CAP_MEASURE_CIRC", 0x0A},
                                                                          {"CAP_MEASURE_PARASITIC", 0x0B},
                                                                          {"LIN_FE_PREAMP_MAIN", 0x0C},
                                                                          {"LIN_FE_COMPS_TAR", 0x0D},
                                                                          {"LIN_FE_COMPARATOR", 0x0E},
                                                                          {"LIN_FE_LDAC", 0x0F},
                                                                          {"LIN_FE_FC", 0x10},
                                                                          {"LIN_FE_KRUMCURR", 0x11},
                                                                          {"LIN_FE_PREAMP_LEFT", 0x13},
                                                                          {"LIN_FE_PREAMP_RIGHT", 0x15},
                                                                          {"LIN_FE_PREAMP_TOP_LEFT", 0x16},
                                                                          {"LIN_FE_PREAMP_TOP", 0x18},
                                                                          {"LIN_FE_PREAMP_TOP_RIGHT", 0x19},
                                                                          {"ANA_IN_CURR", 0x1C},
                                                                          {"ANA_SHUNT_CURR", 0x1D},
                                                                          {"DIG_IN_CURR", 0x1E},
                                                                          {"DIG_SHUNT_CURR", 0x1F}};

    const std::unordered_map<std::string, uint32_t> voltageMultiplexer = {{"Vref_ADC", 0x00},
                                                                          {"I_MUX", 0x01},
                                                                          {"NTC_VOLT", 0x02},
                                                                          {"Vref_CAL_DAC", 0x03},
                                                                          {"VDDA_CAPMEASURE", 0x04},
                                                                          {"POLY_TEMPSENS_TOP", 0x05},
                                                                          {"POLY_TEMPSENS_BOTTOM", 0x06},
                                                                          {"VCAL_HI", 0x07},
                                                                          {"VCAL_MED", 0x08},
                                                                          {"LIN_FE_REF_KRUMCURR", 0x09},
                                                                          {"LIN_FE_GDAC_MAIN", 0x0A},
                                                                          {"LIN_FE_GDAC_LEFT", 0x0B},
                                                                          {"LIN_FE_GDAC_RIGHT", 0x0C},
                                                                          {"RADSENS_ANA_SLDO", 0x0D},
                                                                          {"TEMPSENS_ANA_SLDO", 0x0E},
                                                                          {"RADSENS_DIG_SLDO", 0x0F},
                                                                          {"TEMPSENS_DIG_SLDO", 0x10},
                                                                          {"RADSENS_CENTER", 0x11},
                                                                          {"TEMPSENS_CENTER", 0x12},
                                                                          {"ANA_GND_0", 0x13},
                                                                          {"ANA_GND_1", 0x14},
                                                                          {"ANA_GND_2", 0x15},
                                                                          {"ANA_GND_3", 0x16},
                                                                          {"ANA_GND_4", 0x17},
                                                                          {"ANA_GND_5", 0x18},
                                                                          {"ANA_GND_6", 0x19},
                                                                          {"ANA_GND_7", 0x1A},
                                                                          {"ANA_GND_8", 0x1B},
                                                                          {"ANA_GND_9", 0x1C},
                                                                          {"ANA_GND_10", 0x1D},
                                                                          {"ANA_GND_11", 0x1E},
                                                                          {"Vref_CORE", 0x1F},
                                                                          {"Vref_PRE", 0x20},
                                                                          {"VINA", 0x21},
                                                                          {"VDDA", 0x22},
                                                                          {"VrefA", 0x23},
                                                                          {"VOFS", 0x24},
                                                                          {"VIND", 0x25},
                                                                          {"VDDD", 0x26},
                                                                          {"VrefD", 0x27}};

    auto search = currentMultiplexer.find(observableName);
    if(observableName == "INTERNAL_NTC")
    {
        currentObservable   = currentMultiplexer.find("NTC_CURR")->second;
        voltageObservable   = voltageMultiplexer.find("I_MUX")->second;
        isCurrentNotVoltage = true;
    }
    else if(observableName == "INTERNAL_NTC_VOLT")
    {
        voltageObservable   = voltageMultiplexer.find("NTC_VOLT")->second;
        isCurrentNotVoltage = false;
    }
    else if(search == currentMultiplexer.end())
    {
        if((search = voltageMultiplexer.find(observableName)) == voltageMultiplexer.end())
        {
            LOG(ERROR) << BOLDRED << "Wrong observable name: " << observableName << RESET;
            return -1;
        }
        else
            voltageObservable = search->second;
        isCurrentNotVoltage = false;
    }
    else
    {
        currentObservable   = search->second;
        isCurrentNotVoltage = true;
    }

    return bits::pack<1, 6, 6>(true, currentObservable, voltageObservable);
}

uint32_t RD53BInterface::measureADC(ReadoutChip* pChip, uint32_t data)
{
    this->setBoard(pChip->getBeBoardId());

    const uint16_t GlbPulseVal = RD53Interface::ReadChipReg(pChip, "GlobalPulseConf");

    RD53Interface::WriteChipReg(pChip, "MonitorConfig", data, false); // 14 bits: bit 12 enable, bits 6:11 I-Mon, bits 0:5 V-Mon
    RD53BInterface::SendGlobalPulse(pChip, 0x1000, 0xFF);             // Trigger Monitor Data to start conversion
    RD53Interface::WriteChipReg(pChip, "MonitorConfig", 0, false);    // Stop monitoring
    RD53BInterface::SendGlobalPulse(pChip, GlbPulseVal, 0xFF);        // Restore value in Global Pulse Route

    return RD53Interface::ReadChipReg(pChip, "MonitoringDataADC");
}

float RD53BInterface::measureTemperature(ReadoutChip* pChip, uint32_t data, const std::string& type, int beta)
// #####################
// # type == "POLY"    #
// # type == "ANA"     #
// # type == "DIG"     #
// # type == "CENTER"  #
// # type == "INT_NTC" #
// #####################
{
    // ################################################################################################
    // # Temperature measurement is done by measuring twice, once with high bias, once with low bias  #
    // # Temperature is calculated based on the difference of the two, with the formula on the bottom #
    // # idealityFactor = 5000 [1/1000] for Poly Sens Bottom                                          #
    // # idealityFactor = 2000 [1/1000] for Poly Sens Top                                             #
    // # idealityFactor = 1225 [1/1000] for the rest                                                  #
    // ################################################################################################

    // #####################
    // # Natural constants #
    // #####################
    const float       T0C            = 273.15;         // [Kelvin]
    const float       T25C           = 298.15;         // [Kelvin]
    const float       R25C           = 10;             // [kOhm]
    const float       kb             = 1.38064852e-23; // [J/K]
    const float       e              = 1.6021766208e-19;
    const float       R              = 15;   // By circuit design
    const uint8_t     sensorDEM      = 0x07; // Sensor Dynamic Element Matching bits needed to trim the thermistors
    const float       idealityFactor = pChip->getRegItem("TEMPSENS_IDEAL_FACTOR").fValue / 1e3;
    const std::string regName        = (type == "CENTER" ? "MON_SENS_ACB" : "MON_SENS_SLDO");

    uint16_t sensorConfigData; // Enable[5], DEM[4:1], SEL_BIAS[0] (x2 ... 10 bit in total for the sensors in each sensor config register)
    float    valueLow  = 0;
    float    valueHigh = 0;

    if(type == "INT_NTC")
    {
        bool     isCurrentNotVoltage;
        uint32_t observable = RD53BInterface::getADCobservable("INTERNAL_NTC_VOLT", isCurrentNotVoltage);
        float    voltage    = RD53Interface::convertADC2VorI(pChip, RD53BInterface::measureADC(pChip, observable));
        float    current    = RD53Interface::convertADC2VorI(pChip, RD53BInterface::measureADC(pChip, data), true);

        // ###############################################
        // # Calculate temperature with NTC Beta formula #
        // ###############################################
        float resistance  = 1e-3 * voltage / current;                               // [kOhm]
        float temperature = 1. / (1. / T25C + log(resistance / R25C) / beta) - T0C; // [Celsius]

        return temperature;
    }
    else if(type != "POLY")
    {
        // Get high bias voltage
        sensorConfigData = bits::pack<1, 4, 1>(true, sensorDEM, 0) << (type == "DIG" ? 6 : 0);
        RD53Interface::WriteChipReg(pChip, regName, sensorConfigData);
        valueLow = RD53Interface::convertADC2VorI(pChip, RD53BInterface::measureADC(pChip, data));

        // Get low bias voltage
        sensorConfigData = bits::pack<1, 4, 1>(true, sensorDEM, 1) << (type == "DIG" ? 6 : 0);
        RD53Interface::WriteChipReg(pChip, regName, sensorConfigData);
    }
    valueHigh = RD53Interface::convertADC2VorI(pChip, RD53BInterface::measureADC(pChip, data));

    // ####################
    // # Turn off sensing #
    // ####################
    RD53Interface::WriteChipReg(pChip, "MON_SENS_ACB", 0);
    RD53Interface::WriteChipReg(pChip, "MON_SENS_SLDO", 0);

    return e / (idealityFactor * kb * log(R)) * (valueHigh - valueLow) - T0C;
}

} // namespace Ph2_HwInterface
