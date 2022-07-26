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

    ChipRegMap& pRD53RegMap = pChip->getRegMap();

    RD53Interface::WriteChipReg(pChip, "PIX_DEFAULT_CONFIG", 0x9CE2, pVerifLoop);
    RD53Interface::WriteChipReg(pChip, "PIX_DEFAULT_CONFIG_B", 0x631D, pVerifLoop);

    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    RD53Interface::WriteChipReg(pChip, "TwoLevelTrigger", 0, pVerifLoop);
    RD53Interface::WriteChipReg(pChip, "EnEoS", 0, pVerifLoop);
    RD53Interface::WriteChipReg(pChip, "NumOfEventsInStream", 0, pVerifLoop);
    RD53Interface::WriteChipReg(pChip, "BinaryReadOut", 0, pVerifLoop);
    RD53Interface::WriteChipReg(pChip, "RawData", 0, pVerifLoop);
    RD53Interface::WriteChipReg(pChip, "EnOutputDataChipId", 0, pVerifLoop);
    /*
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
    static const std::set<std::string> registerBlackList = {
        "HighGain_LIN", "ADC_OFFSET_VOLT", "ADC_MAXIMUM_VOLT", "TEMPSENS_IDEAL_FACTOR", "CLK_DATA_DELAY", "CLK_DATA_DELAY_DATA", "CLK_DATA_DELAY_CLK"};

    for(auto& cRegItem: pRD53RegMap)
        if(cRegItem.second.fPrmptCfg == true)
        {
            if(registerBlackList.find(cRegItem.first) != registerBlackList.end()) continue;

            if(cRegItem.first == "CDR_CONFIG")
            {
                RD53Interface::SendCommand(static_cast<RD53*>(pChip), RD53BCmd::Clear{});
                RD53Interface::SendCommand(static_cast<RD53*>(pChip), RD53BCmd::Clear{});
                std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
            }

            RD53Interface::WriteChipReg(pChip, cRegItem.first, cRegItem.second.fValue, pVerifLoop);
        }
    */
    // ###################################
    // # Programmig pixel cell registers #
    // ###################################
    RD53BInterface::WriteRD53Mask(static_cast<RD53*>(const_cast<Chip*>(pChip)), false, true);

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

    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "GCR_DEFAULT_CONFIG", 0xAC75);
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "GCR_DEFAULT_CONFIG_B", 0x538A);
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "CMDERR_CNT", 0);

    // ##############
    // # Link speed #
    // ##############
    // @TMP@ : why thisone is here?
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "CDR_CONFIG_SEL_SER_CLK", static_cast<RD53FWInterface*>(fBoardFW)->ReadoutSpeed() == RD53FWconstants::ReadoutSpeed::x1280 ? 0 : 1);
    RD53BInterface::SendGlobalPulseBroadcast(pBoard, 7, 0xFF); // ResetChannelSynchronizer, ResetCommandDecoder, ResetGlobalConfiguration

    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "RingOscConfig", 0x7FFF);
    RD53Interface::WriteBoardBroadcastChipReg(pBoard, "RingOscConfig", 0x7FFF);
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

    RD53BInterface::SendGlobalPulse(pChip, 0b110000, 0xFF);
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    RD53Interface::SendCommand(pChip, RD53BCmd::Clear{pChip->getId()});

    // ##############
    // # Link speed #
    // ##############
    RD53Interface::WriteChipReg(pChip, "CDR_CONFIG_SEL_SER_CLK", static_cast<RD53FWInterface*>(fBoardFW)->ReadoutSpeed() == RD53FWconstants::ReadoutSpeed::x1280 ? 0 : 1, false);
    RD53Interface::SendCommand(pChip, RD53BCmd::Clear{});

    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
}

std::pair<std::string, uint16_t> RD53BInterface::SplitSpecialRegisters(std::string regName, uint16_t value, ChipRegMap& pRD53RegMap)
{
    static const std::map<std::string, RD53Interface::SpecialRegInfo> specialRegMap = {{"CDR_CONFIG_SEL_SER_CLK", {"CDR_CONFIG", 0}},

                                                                                       {"CLK_DATA_DELAY_DATA", {"CLK_DATA_DELAY", 0}},
                                                                                       {"CLK_DATA_DELAY_CLK", {"CLK_DATA_DELAY", 7}},

                                                                                       {"MON_ADC_TRIM", {"MON_ADC", 0}},

                                                                                       {"VOLTAGE_TRIM_DIG", {"VOLTAGE_TRIM", 0}},
                                                                                       {"VOLTAGE_TRIM_ANA", {"VOLTAGE_TRIM", 5}},

                                                                                       {"CalibrationConfig_DELAY", {"CalibrationConfig", 0}},

                                                                                       {"CML_CONFIG_SER_EN_TAP", {"CML_CONFIG", 0}},
                                                                                       {"CML_CONFIG_SER_INV_TAP", {"CML_CONFIG", 2}},

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

void RD53BInterface::WriteRD53Mask(Ph2_HwDescription::RD53* pRD53, bool doSparse, bool doDefault) {}

void RD53BInterface::Reset(Ph2_HwDescription::ReadoutChip* pChip, const size_t resetType)
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
    {
        RD53Interface::SendCommand(pChip, RD53BCmd::WrReg{pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, (size_t)(1 << resetType)});
        RD53Interface::SendCommand(pChip, RD53BCmd::GlobalPulse{pChip->getId()});
    }
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

uint32_t RD53BInterface::ReadChipFuseID(Ph2_HwDescription::Chip* pChip)
{
    uint16_t low  = RD53Interface::ReadChipReg(pChip, "EfusesReadData0");
    uint16_t high = RD53Interface::ReadChipReg(pChip, "EfusesReadData1");
    return low | (high << pChip->getNumberOfBits("EfusesReadData0"));
}

} // namespace Ph2_HwInterface
