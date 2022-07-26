/*!
  \file                  RD53AInterface.h
  \brief                 User interface to the RD53A readout chip
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#include "RD53AInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
bool RD53AInterface::ConfigureChip(Chip* pChip, bool pVerifLoop, uint32_t pBlockSize)
{
    this->setBoard(pChip->getBeBoardId());

    ChipRegMap& pRD53RegMap = pChip->getRegMap();

    // ################################################
    // # Programming global registers from white list #
    // ################################################
    static const char* registerWhileList[] = {"PA_IN_BIAS_LIN", "FC_BIAS_LIN", "KRUM_CURR_LIN", "LDAC_LIN", "COMP_LIN", "REF_KRUM_LIN", "Vthreshold_LIN"}; // @CONST@

    for(auto i = 0u; i < ArraySize(registerWhileList); i++)
    {
        auto it = pRD53RegMap.find(registerWhileList[i]);
        if(it != pRD53RegMap.end()) RD53Interface::WriteChipReg(pChip, it->first, it->second.fValue, pVerifLoop);
    }

    // #######################################
    // # Programming CLK_DATA_DELAY register #
    // #######################################
    static const char*               registerClkDataDelayList[] = {"CLK_DATA_DELAY", "CLK_DATA_DELAY_DATA", "CLK_DATA_DELAY_CLK", "CLK_DATA_DELAY_2INV"}; // @CONST@
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
        static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(std::vector<uint16_t>(RD53Constants::NSYNC_WORS, RD53ACmd::RD53ACmdEncoder::SYNC), -1);
        RD53Interface::WriteChipReg(pChip, nameAndValue.first, nameAndValue.second, pVerifLoop);
    }

    // ###############################
    // # Programmig global registers #
    // ###############################
    static const std::set<std::string> registerBlackList = {
        "HighGain_LIN", "ADC_OFFSET_VOLT", "ADC_MAXIMUM_VOLT", "TEMPSENS_IDEAL_FACTOR", "CLK_DATA_DELAY", "CLK_DATA_DELAY_DATA", "CLK_DATA_DELAY_CLK", "CLK_DATA_DELAY_2INV"};

    for(auto& cRegItem: pRD53RegMap)
        if(cRegItem.second.fPrmptCfg == true)
        {
            if(registerBlackList.find(cRegItem.first) != registerBlackList.end()) continue;

            if(cRegItem.first == "CDR_CONFIG")
            {
                RD53Interface::SendCommand(static_cast<RD53*>(pChip), RD53ACmd::ECR{});
                RD53Interface::SendCommand(static_cast<RD53*>(pChip), RD53ACmd::ECR{});
                std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
            }

            RD53Interface::WriteChipReg(pChip, cRegItem.first, cRegItem.second.fValue, pVerifLoop);
        }

    // ###################################
    // # Programmig pixel cell registers #
    // ###################################
    RD53AInterface::WriteRD53Mask(static_cast<RD53*>(const_cast<Chip*>(pChip)), false, true);

    return true;
}

void RD53AInterface::InitRD53Downlink(const BeBoard* pBoard)
{
    this->setBoard(pBoard->getId());

    LOG(INFO) << GREEN << "Down-link phase initialization..." << RESET;
    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(std::vector<uint16_t>(RD53Constants::NSYNC_WORS, RD53ACmd::RD53ACmdEncoder::SYNC), -1);

    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
}

void RD53AInterface::InitRD53UplinkSpeed(ReadoutChip* pChip)
{
    this->setBoard(pChip->getBeBoardId());

    ChipRegMap& pRD53RegMap                      = pChip->getRegMap();
    auto        auroraSpeed                      = static_cast<RD53FWInterface*>(fBoardFW)->ReadoutSpeed();
    pRD53RegMap["CDR_CONFIG_SEL_SER_CLK"].fValue = (auroraSpeed == RD53FWconstants::ReadoutSpeed::x1280 ? RD53Constants::CDRCONFIG_1Gbit : RD53Constants::CDRCONFIG_640Mbit);

    RD53Interface::WriteChipReg(pChip, "CDR_CONFIG_SEL_SER_CLK", auroraSpeed == RD53FWconstants::ReadoutSpeed::x1280 ? RD53Constants::CDRCONFIG_1Gbit : RD53Constants::CDRCONFIG_640Mbit, false);
    RD53Interface::SendCommand(pChip, RD53ACmd::ECR{});

    LOG(INFO) << GREEN << "Up-link speed set to: " << BOLDYELLOW << (auroraSpeed == RD53FWconstants::ReadoutSpeed::x1280 ? "1.28 Gbit/s" : "640 Mbit/s") << RESET;
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
}

void RD53AInterface::InitRD53Uplinks(ReadoutChip* pChip, int nActiveLanes)
{
    RD53AInterface::ConfigureChip(pChip, false);

    this->setBoard(pChip->getBeBoardId());

    // ##############################
    // # 1 Autora active lane       #
    // # OUTPUT_CONFIG = 0b00000100 #
    // # CML_CONFIG    = 0b00000001 #

    // # 2 Autora active lanes      #
    // # OUTPUT_CONFIG = 0b00001100 #
    // # CML_CONFIG    = 0b00000011 #

    // # 4 Autora active lanes      #
    // # OUTPUT_CONFIG = 0b00111100 #
    // # CML_CONFIG    = 0b00001111 #
    // ##############################

    LOG(INFO) << GREEN << "Configuring up-link lanes and monitoring..." << RESET;
    RD53Interface::WriteChipReg(pChip, "OUTPUT_CONFIG", RD53Shared::setBits(nActiveLanes) << 2, false); // Number of active lanes [5:2]
    // bits [8:7]: number of 40 MHz clocks +2 for data transfer out of pixel matrix
    // Default 0 means 2 clocks, may need higher value in case of large propagation
    // delays, for example at low VDDD voltage after irradiation
    // bits [5:2]: Aurora lanes. Default 0001 means single lane mode
    RD53Interface::WriteChipReg(pChip, "CML_CONFIG", 0x0F, false);         // CML_EN_LANE[3:0]: the actual number of lanes is determined by OUTPUT_CONFIG
    RD53Interface::WriteChipReg(pChip, "GLOBAL_PULSE_ROUTE", 0x30, false); // 0x30 = reset Aurora AND Serializer
    RD53Interface::SendCommand(pChip, RD53ACmd::GlobalPulse{pChip->getId(), 0x0001});
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    // ##############################
    // # Set standard AURORA output #
    // ##############################
    RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_AURORA, false);

    // ##A#############################################################
    // # Enable monitoring (needed for AutoRead register monitoring) #
    // ###############################################################
    RD53Interface::WriteChipReg(pChip, "GLOBAL_PULSE_ROUTE", 0x100, false); // 0x100 = start monitoring
    RD53Interface::SendCommand(pChip, RD53ACmd::GlobalPulse{pChip->getId(), 0x0004});
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;

    RD53AInterface::InitRD53UplinkSpeed(pChip);
}

std::pair<std::string, uint16_t> RD53AInterface::SplitSpecialRegisters(std::string regName, uint16_t value, ChipRegMap& pRD53RegMap)
{
    static const std::map<std::string, RD53Interface::SpecialRegInfo> specialRegMap = {{"CDR_CONFIG_SEL_SER_CLK", {"CDR_CONFIG", 0}},

                                                                                       {"CLK_DATA_DELAY_CMD_DELAY", {"CLK_DATA_DELAY", 0}},
                                                                                       {"CLK_DATA_DELAY_CLK_DELAY", {"CLK_DATA_DELAY", 4}},
                                                                                       {"CLK_DATA_DELAY_2INV_DELAY", {"CLK_DATA_DELAY", 5}},

                                                                                       {"MONITOR_CONFIG_ADC", {"MONITOR_CONFIG", 0}},
                                                                                       {"MONITOR_CONFIG_BG", {"MONITOR_CONFIG", 6}},

                                                                                       {"VOLTAGE_TRIM_DIG", {"VOLTAGE_TRIM", 0}},
                                                                                       {"VOLTAGE_TRIM_ANA", {"VOLTAGE_TRIM", 5}},

                                                                                       {"INJECTION_SELECT_DELAY", {"INJECTION_SELECT", 0}},

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

uint16_t RD53AInterface::GetPixelConfig(const pixelMask& mask, uint16_t NRows, uint16_t row, uint16_t col, bool highGain)
// ##############################################################################################################
// # Encodes the configuration for a pixel pair                                                                 #
// # In the LIN FE TDAC is unsigned and increasing it reduces the local threshold                               #
// # In the DIFF FE TDAC is signed and increasing it increases the local threshold                              #
// # To prevent having to deal with that in the rest of the code, we map the TDAC range of the DIFF FE like so: #
// # -15 -> 30, -14 -> 29, ... 0 -> 15, ... 15 -> 0                                                             #
// # So for the rest of the code the TDAC range of the DIFF FE is [0, 30] and                                   #
// # the only difference with the LIN FE is the number of possible values                                       #
// ##############################################################################################################
{
    if(col <= RD53A::SYNC.colStop)
        return bits::pack<8, 8>(bits::pack<1, 1, 1>(mask.HitBus[row + NRows * (col + 1)], mask.InjEn[row + NRows * (col + 1)], mask.Enable[row + NRows * (col + 1)]),
                                bits::pack<1, 1, 1>(mask.HitBus[row + NRows * (col + 0)], mask.InjEn[row + NRows * (col + 0)], mask.Enable[row + NRows * (col + 0)]));
    else if(col <= RD53A::LIN.colStop)
        return bits::pack<8, 8>(
            bits::pack<1, 4, 1, 1, 1>(highGain, mask.TDAC[row + NRows * (col + 1)], mask.HitBus[row + NRows * (col + 1)], mask.InjEn[row + NRows * (col + 1)], mask.Enable[row + NRows * (col + 1)]),
            bits::pack<1, 4, 1, 1, 1>(highGain, mask.TDAC[row + NRows * (col + 0)], mask.HitBus[row + NRows * (col + 0)], mask.InjEn[row + NRows * (col + 0)], mask.Enable[row + NRows * (col + 0)]));
    else
        return bits::pack<8, 8>(bits::pack<1, 4, 1, 1, 1>(mask.TDAC[row + NRows * (col + 1)] > 15,
                                                          abs(15 - mask.TDAC[row + NRows * (col + 1)]),
                                                          mask.HitBus[row + NRows * (col + 1)],
                                                          mask.InjEn[row + NRows * (col + 1)],
                                                          mask.Enable[row + NRows * (col + 1)]),
                                bits::pack<1, 4, 1, 1, 1>(mask.TDAC[row + NRows * (col + 0)] > 15,
                                                          abs(15 - mask.TDAC[row + NRows * (col + 0)]),
                                                          mask.HitBus[row + NRows * (col + 0)],
                                                          mask.InjEn[row + NRows * (col + 0)],
                                                          mask.Enable[row + NRows * (col + 0)]));
}

void RD53AInterface::WriteRD53Mask(RD53* pRD53, bool doSparse, bool doDefault)
{
    this->setBoard(pRD53->getBeBoardId());

    std::vector<uint16_t> commandList;
    const uint16_t        REGION_COL_ADDR = pRD53->getRegItem("REGION_COL").fAddress;
    const uint16_t        REGION_ROW_ADDR = pRD53->getRegItem("REGION_ROW").fAddress;
    const uint16_t        PIX_PORTAL_ADDR = pRD53->getRegItem("PIX_PORTAL").fAddress;
    const uint8_t         highGain        = pRD53->getRegItem("HighGain_LIN").fValue;
    const uint8_t         chipID          = pRD53->getId();
    auto&                 mask            = doDefault == true ? pRD53->getPixelsMaskDefault() : pRD53->getPixelsMask();

    // ##########################
    // # Disable default config #
    // ##########################
    RD53ACmd::serialize(RD53ACmd::WrReg{chipID, pRD53->getRegItem("PIX_DEFAULT_CONFIG").fAddress, 0x0}, commandList);

    // ############
    // # PIX_MODE #
    // ############
    // bit[5]: enable broadcast
    // bit[4]: enable auto-col
    // bit[3]: enable auto-row
    // bit[2]: broadcast to SYNC FE
    // bit[1]: broadcast to LIN FE
    // bit[0]: broadcast to DIFF FE

    if(doSparse == true)
    {
        RD53ACmd::serialize(RD53ACmd::WrReg{chipID, pRD53->getRegItem("PIX_MODE").fAddress, 0x27}, commandList);
        RD53ACmd::serialize(RD53ACmd::WrReg{chipID, pRD53->getRegItem("PIX_PORTAL").fAddress, 0x0}, commandList);
        RD53ACmd::serialize(RD53ACmd::WrReg{chipID, pRD53->getRegItem("PIX_MODE").fAddress, 0x0}, commandList);

        uint16_t data;

        for(auto col = 0u; col < RD53A::NCOLS; col += 2)
        {
            if(std::find(mask.Enable.begin() + (0 + RD53A::NROWS * col), mask.Enable.begin() + (RD53A::NROWS + RD53A::NROWS * col), true) ==
               (mask.Enable.begin() + (RD53A::NROWS + RD53A::NROWS * col)))
                continue;

            RD53ACmd::serialize(RD53ACmd::WrReg{chipID, REGION_COL_ADDR, col / 2}, commandList);

            for(auto row = 0u; row < RD53A::NROWS; row++)
            {
                if((mask.Enable[row + RD53A::NROWS * col] == true) || (mask.Enable[row + RD53A::NROWS * (col + 1)] == true))
                {
                    data = RD53AInterface::GetPixelConfig(mask, RD53A::NROWS, row, col, highGain);

                    RD53ACmd::serialize(RD53ACmd::WrReg{chipID, REGION_ROW_ADDR, row}, commandList);
                    RD53ACmd::serialize(RD53ACmd::WrReg{chipID, PIX_PORTAL_ADDR, data}, commandList);
                }
            }

            auto n16bitWords = commandList.size() + RD53A::NROWS * 2 + 1;
            if((n16bitWords / 2 + n16bitWords % 2) > (1 << RD53FWconstants::NBIT_SLOWCMD_FIFO))
            {
                static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pRD53->getHybridId());
                commandList.clear();
            }
        }
    }
    else
    {
        RD53ACmd::serialize(RD53ACmd::WrReg{chipID, pRD53->getRegItem("PIX_MODE").fAddress, 0x8}, commandList);

        RD53ACmd::WrRegLong wrRegLongCmd{chipID, PIX_PORTAL_ADDR, {}};

        for(auto col = 0u; col < RD53A::NCOLS; col += 2)
        {
            RD53ACmd::serialize(RD53ACmd::WrReg{chipID, REGION_COL_ADDR, col / 2}, commandList);
            RD53ACmd::serialize(RD53ACmd::WrReg{chipID, REGION_ROW_ADDR, 0x0}, commandList);

            size_t nValuesLongCmd = wrRegLongCmd.values.size();
            size_t nLongCommands  = RD53A::NROWS / nValuesLongCmd;

            for(auto longCmdId = 0u; longCmdId < nLongCommands; longCmdId++)
            {
                for(size_t i = 0; i < nValuesLongCmd; i++) wrRegLongCmd.values[i] = RD53AInterface::GetPixelConfig(mask, RD53A::NROWS, nValuesLongCmd * longCmdId + i, col, highGain);
                RD53ACmd::serialize(wrRegLongCmd, commandList);
            }

            for(auto row = nValuesLongCmd * nLongCommands; row < RD53A::NROWS; row++)
                RD53ACmd::serialize(RD53ACmd::WrReg{chipID, PIX_PORTAL_ADDR, RD53AInterface::GetPixelConfig(mask, RD53A::NROWS, row, col, highGain)}, commandList);

            auto n16bitWords = commandList.size() + RD53A::NROWS + 2;
            if((n16bitWords / 2 + n16bitWords % 2) > (1 << RD53FWconstants::NBIT_SLOWCMD_FIFO))
            {
                static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pRD53->getHybridId());
                commandList.clear();
            }
        }
    }

    if(commandList.size() != 0) static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pRD53->getHybridId());
}

void RD53AInterface::Reset(Ph2_HwDescription::ReadoutChip* pChip, const size_t resetType)
// ################################################
// # resetType = 0 --> Reset Channel Synchronizer #
// # resetType = 1 --> Reset Command Decoder      #
// # resetType = 2 --> Reset Global Configuration #
// # resetType = 3 --> Reset Service Data         #
// # resetType = 4 --> Reset Aurora               #
// # resetType = 5 --> Reset Serializer           #
// # resetType = 6 --> Reset ADC                  #
// # resetType = 7 --> Reset Aurora pattern       #
// ################################################
{
    this->setBoard(pChip->getBeBoardId());

    const int duration = 0x0004; // @CONST@

    if(resetType > 6)
        RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_AURORA, false);
    else
    {
        RD53Interface::SendCommand(pChip, RD53ACmd::WrReg{pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, (size_t)(1 << resetType)});
        RD53Interface::SendCommand(pChip, RD53ACmd::GlobalPulse{pChip->getId(), duration});
    }
}

void RD53AInterface::ChipErrorReport(ReadoutChip* pChip)
{
    RD53Interface::ChipErrorReport(pChip);

    LOG(INFO) << BOLDBLUE << "WNGFIFO_FULL_CNT_0  = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "WNGFIFO_FULL_CNT_0") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "WNGFIFO_FULL_CNT_1  = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "WNGFIFO_FULL_CNT_1") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "WNGFIFO_FULL_CNT_2  = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "WNGFIFO_FULL_CNT_2") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "WNGFIFO_FULL_CNT_3  = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "WNGFIFO_FULL_CNT_3") << std::setfill(' ') << std::setw(8) << "" << RESET;
}

void RD53AInterface::PackWriteCommand(Chip* pChip, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg)
{
    RD53ACmd::serialize(RD53ACmd::WrReg{(uint8_t)pChip->getId(), pChip->getRegItem(regName).fAddress, data}, chipCommandList);
    if(updateReg == true) pChip->setReg(regName, data);
}

} // namespace Ph2_HwInterface
