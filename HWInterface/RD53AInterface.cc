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

    for(auto i = 0u; i < arraySize(registerWhileList); i++)
    {
        auto it = pRD53RegMap.find(registerWhileList[i]);
        if(it != pRD53RegMap.end()) RD53Interface::WriteChipReg(pChip, it->first, it->second.fValue, pVerifLoop);
    }

    // #######################################
    // # Programming CLK_DATA_DELAY register #
    // #######################################
    static const char*               registerClkDataDelayList[] = {"CLK_DATA_DELAY", "CLK_DATA_DELAY_CMD_DELAY", "CLK_DATA_DELAY_CLK_DELAY", "CLK_DATA_DELAY_2INV_DELAY"}; // @CONST@
    std::pair<std::string, uint16_t> nameAndValue("CLK_DATA_DELAY", pRD53RegMap["CLK_DATA_DELAY"].fValue);
    bool                             doWriteClkDataDelay = false;

    for(auto i = 0u; i < arraySize(registerClkDataDelayList); i++)
    {
        auto cRegItem = pRD53RegMap.find(registerClkDataDelayList[i]);
        if((cRegItem != pRD53RegMap.end()) && (cRegItem->second.fPrmptCfg == true))
        {
            doWriteClkDataDelay = true;

            nameAndValue = SplitSpecialRegisters(std::string(cRegItem->first), cRegItem->second, pRD53RegMap);

            if(cRegItem->first == "CLK_DATA_DELAY") break;
        }
    }

    if(doWriteClkDataDelay == true)
    {
        RD53Interface::WriteChipReg(pChip, nameAndValue.first, nameAndValue.second, false);
        static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(std::vector<uint16_t>(RD53Constants::NSYNC_WORS, RD53ACmd::RD53CmdEncoder::SYNC), -1);
        RD53Interface::WriteChipReg(pChip, nameAndValue.first, nameAndValue.second, pVerifLoop);
    }

    // ###############################
    // # Programmig global registers #
    // ###############################
    static const char* registerBlackList[] = {
        "HighGain_LIN", "ADC_OFFSET_VOLT", "ADC_MAXIMUM_VOLT", "TEMPSENS_IDEAL_FACTOR", "CLK_DATA_DELAY", "CLK_DATA_DELAY_CMD_DELAY", "CLK_DATA_DELAY_CLK_DELAY", "CLK_DATA_DELAY_2INV_DELAY"};

    for(auto& cRegItem: pRD53RegMap)
        if(cRegItem.second.fPrmptCfg == true)
        {
            auto i = 0u;
            for(i = 0u; i < arraySize(registerBlackList); i++)
                if(cRegItem.first == registerBlackList[i]) break;
            if(i == arraySize(registerBlackList))
            {
                std::pair<std::string, uint16_t> nameAndValue(SplitSpecialRegisters(std::string(cRegItem.first), cRegItem.second, pRD53RegMap));

                if(cRegItem.first == "CDR_CONFIG")
                {
                    RD53Interface::sendCommand(static_cast<RD53*>(pChip), RD53ACmd::ECR{});
                    RD53Interface::sendCommand(static_cast<RD53*>(pChip), RD53ACmd::ECR{});
                    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
                }

                RD53Interface::WriteChipReg(pChip, nameAndValue.first, nameAndValue.second, pVerifLoop);
            }
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
    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(std::vector<uint16_t>(RD53Constants::NSYNC_WORS, RD53ACmd::RD53CmdEncoder::SYNC), -1);
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
}

void RD53AInterface::InitRD53UplinkSpeed(ReadoutChip* pChip)
{
    this->setBoard(pChip->getBeBoardId());

    uint32_t auroraSpeed = static_cast<RD53FWInterface*>(fBoardFW)->ReadoutSpeed();
    RD53Interface::WriteChipReg(pChip, "CDR_CONFIG", (auroraSpeed == 0 ? RD53Constants::CDRCONFIG_1Gbit : RD53Constants::CDRCONFIG_640Mbit), false);
    RD53Interface::sendCommand(pChip, RD53ACmd::ECR{});

    LOG(INFO) << GREEN << "Up-link speed set to: " << BOLDYELLOW << (auroraSpeed == 0 ? "1.28 Gbit/s" : "640 Mbit/s") << RESET;
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
    RD53Interface::sendCommand(pChip, RD53ACmd::GlobalPulse{(uint8_t)pChip->getId(), 0x0001});
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    // ##############################
    // # Set standard AURORA output #
    // ##############################
    RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_AURORA, false);

    // ##A#############################################################
    // # Enable monitoring (needed for AutoRead register monitoring) #
    // ###############################################################
    RD53Interface::WriteChipReg(pChip, "GLOBAL_PULSE_ROUTE", 0x100, false); // 0x100 = start monitoring
    RD53Interface::sendCommand(pChip, RD53ACmd::GlobalPulse{(uint8_t)pChip->getId(), 0x0004});
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;

    RD53AInterface::InitRD53UplinkSpeed(pChip);
}

std::vector<std::pair<uint16_t, uint16_t>> RD53AInterface::ReadRD53Reg(ReadoutChip* pChip, const std::string& regName)
{
    this->setBoard(pChip->getBeBoardId());

    RD53Interface::sendCommand(pChip, RD53ACmd::RdReg{(uint8_t)pChip->getId(), pChip->getRegItem(regName).fAddress});
    auto regReadback = static_cast<RD53FWInterface*>(fBoardFW)->ReadChipRegisters(pChip);

    for(auto i = 0u; i < regReadback.size(); i++)
        // Removing bit related to PIX_PORTAL register identification
        regReadback[i].first = regReadback[i].first & static_cast<uint16_t>(RD53Shared::setBits(RD53Constants::NBIT_ADDR));

    return regReadback;
}

std::pair<std::string, uint16_t> RD53AInterface::SplitSpecialRegisters(std::string regName, ChipRegItem& Reg, ChipRegMap& pRD53RegMap)
{
    uint16_t value = Reg.fValue;

    if(regName == "CLK_DATA_DELAY_CMD_DELAY")
    {
        value                                   = Reg.fValue | (value & (RD53Shared::setBits(pRD53RegMap["CLK_DATA_DELAY"].fBitSize) - RD53Shared::setBits(Reg.fBitSize)));
        pRD53RegMap["CLK_DATA_DELAY"].fPrmptCfg = true;
    }
    else if(regName == "CLK_DATA_DELAY_CLK_DELAY")
    {
        value = (Reg.fValue << pRD53RegMap["CLK_DATA_DELAY_CMD_DELAY"].fBitSize) |
                (value & (RD53Shared::setBits(pRD53RegMap["CLK_DATA_DELAY"].fBitSize) - (RD53Shared::setBits(Reg.fBitSize) << pRD53RegMap["CLK_DATA_DELAY_CMD_DELAY"].fBitSize)));
        pRD53RegMap["CLK_DATA_DELAY"].fPrmptCfg = true;
    }
    else if(regName == "CLK_DATA_DELAY_2INV_DELAY")
    {
        value = (Reg.fValue << (pRD53RegMap["CLK_DATA_DELAY_CMD_DELAY"].fBitSize + pRD53RegMap["CLK_DATA_DELAY_CLK_DELAY"].fBitSize)) |
                (value & (RD53Shared::setBits(pRD53RegMap["CLK_DATA_DELAY"].fBitSize) -
                          (RD53Shared::setBits(Reg.fBitSize) << (pRD53RegMap["CLK_DATA_DELAY_CMD_DELAY"].fBitSize + pRD53RegMap["CLK_DATA_DELAY_CLK_DELAY"].fBitSize))));
        pRD53RegMap["CLK_DATA_DELAY"].fPrmptCfg = true;
    }
    else if(regName == "MONITOR_CONFIG_ADC")
    {
        value   = Reg.fValue | (pRD53RegMap["MONITOR_CONFIG"].fValue & (RD53Shared::setBits(pRD53RegMap["MONITOR_CONFIG"].fBitSize) - RD53Shared::setBits(Reg.fBitSize)));
        regName = "MONITOR_CONFIG";
    }
    else if(regName == "MONITOR_CONFIG_BG")
    {
        value =
            (Reg.fValue << pRD53RegMap["MONITOR_CONFIG_ADC"].fBitSize) |
            (pRD53RegMap["MONITOR_CONFIG"].fValue & (RD53Shared::setBits(pRD53RegMap["MONITOR_CONFIG"].fBitSize) - (RD53Shared::setBits(Reg.fBitSize) << pRD53RegMap["MONITOR_CONFIG_ADC"].fBitSize)));
        regName = "MONITOR_CONFIG";
    }
    else if(regName == "VOLTAGE_TRIM_DIG")
    {
        value   = Reg.fValue | (pRD53RegMap["VOLTAGE_TRIM"].fValue & (RD53Shared::setBits(pRD53RegMap["VOLTAGE_TRIM"].fBitSize) - RD53Shared::setBits(Reg.fBitSize)));
        regName = "VOLTAGE_TRIM";
    }
    else if(regName == "VOLTAGE_TRIM_ANA")
    {
        value = (Reg.fValue << pRD53RegMap["VOLTAGE_TRIM_DIG"].fBitSize) |
                (pRD53RegMap["VOLTAGE_TRIM"].fValue & (RD53Shared::setBits(pRD53RegMap["VOLTAGE_TRIM"].fBitSize) - (RD53Shared::setBits(Reg.fBitSize) << pRD53RegMap["VOLTAGE_TRIM_DIG"].fBitSize)));
        regName = "VOLTAGE_TRIM";
    }
    else if(regName == "INJECTION_SELECT_DELAY")
    {
        value   = Reg.fValue | (pRD53RegMap["INJECTION_SELECT"].fValue & (RD53Shared::setBits(pRD53RegMap["INJECTION_SELECT"].fBitSize) - RD53Shared::setBits(Reg.fBitSize)));
        regName = "INJECTION_SELECT";
    }
    else if(regName == "CML_CONFIG_SER_EN_TAP")
    {
        value = (Reg.fValue << pRD53RegMap["CML_CONFIG_EN_LANE"].fBitSize) |
                (pRD53RegMap["CML_CONFIG"].fValue & (RD53Shared::setBits(pRD53RegMap["CML_CONFIG"].fBitSize) - (RD53Shared::setBits(Reg.fBitSize) << pRD53RegMap["CML_CONFIG_EN_LANE"].fBitSize)));
        regName = "CML_CONFIG";
    }
    else if(regName == "CML_CONFIG_SER_INV_TAP")
    {
        value = (Reg.fValue << (pRD53RegMap["CML_CONFIG_EN_LANE"].fBitSize + pRD53RegMap["CML_CONFIG_SER_EN_TAP"].fBitSize)) |
                (pRD53RegMap["CML_CONFIG"].fValue & (RD53Shared::setBits(pRD53RegMap["CML_CONFIG"].fBitSize) -
                                                     (RD53Shared::setBits(Reg.fBitSize) << (pRD53RegMap["CML_CONFIG_EN_LANE"].fBitSize + pRD53RegMap["CML_CONFIG_SER_EN_TAP"].fBitSize))));
        regName = "CML_CONFIG";
    }
    else if(regName == "SER_SEL_OUT_0")
    {
        value   = Reg.fValue | (pRD53RegMap["SER_SEL_OUT"].fValue & 0xFC);
        regName = "SER_SEL_OUT";
    }
    else if(regName == "SER_SEL_OUT_1")
    {
        value   = (Reg.fValue << pRD53RegMap["SER_SEL_OUT_0"].fBitSize) | (pRD53RegMap["SER_SEL_OUT"].fValue & 0xF3);
        regName = "SER_SEL_OUT";
    }
    else if(regName == "SER_SEL_OUT_2")
    {
        value   = (Reg.fValue << (pRD53RegMap["SER_SEL_OUT_0"].fBitSize + pRD53RegMap["SER_SEL_OUT_1"].fBitSize)) | (pRD53RegMap["SER_SEL_OUT"].fValue & 0xCF);
        regName = "SER_SEL_OUT";
    }
    else if(regName == "SER_SEL_OUT_3")
    {
        value   = (Reg.fValue << (pRD53RegMap["SER_SEL_OUT_0"].fBitSize + pRD53RegMap["SER_SEL_OUT_1"].fBitSize + pRD53RegMap["SER_SEL_OUT_2"].fBitSize)) | (pRD53RegMap["SER_SEL_OUT"].fValue & 0x3F);
        regName = "SER_SEL_OUT";
    }

    return std::pair<std::string, uint16_t>(regName, value);
}

// @TMP@
uint16_t getPixelConfig(const std::vector<perColumnPixelData>& mask, uint16_t row, uint16_t col, bool highGain)
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
        return bits::pack<8, 8>(bits::pack<1, 1, 1>(mask[col + 1].HitBus[row], mask[col + 1].InjEn[row], mask[col + 1].Enable[row]),
                                bits::pack<1, 1, 1>(mask[col + 0].HitBus[row], mask[col + 0].InjEn[row], mask[col + 0].Enable[row]));
    else if(col <= RD53A::LIN.colStop)
        return bits::pack<8, 8>(bits::pack<1, 4, 1, 1, 1>(highGain, mask[col + 1].TDAC[row], mask[col + 1].HitBus[row], mask[col + 1].InjEn[row], mask[col + 1].Enable[row]),
                                bits::pack<1, 4, 1, 1, 1>(highGain, mask[col + 0].TDAC[row], mask[col + 0].HitBus[row], mask[col + 0].InjEn[row], mask[col + 0].Enable[row]));
    else
        return bits::pack<8, 8>(
            bits::pack<1, 4, 1, 1, 1>(mask[col + 1].TDAC[row] > 15, abs(15 - mask[col + 1].TDAC[row]), mask[col + 1].HitBus[row], mask[col + 1].InjEn[row], mask[col + 1].Enable[row]),
            bits::pack<1, 4, 1, 1, 1>(mask[col + 0].TDAC[row] > 15, abs(15 - mask[col + 0].TDAC[row]), mask[col + 0].HitBus[row], mask[col + 0].InjEn[row], mask[col + 0].Enable[row]));
}

void RD53AInterface::WriteRD53Mask(RD53* pRD53, bool doSparse, bool doDefault)
{
    this->setBoard(pRD53->getBeBoardId());

    std::vector<uint16_t> commandList;
    std::vector<uint16_t> syncList(RD53Constants::NSYNC_WORS, RD53ACmd::RD53CmdEncoder::SYNC);

    const uint16_t REGION_COL_ADDR = pRD53->getRegItem("REGION_COL").fAddress;
    const uint16_t REGION_ROW_ADDR = pRD53->getRegItem("REGION_ROW").fAddress;
    const uint16_t PIX_PORTAL_ADDR = pRD53->getRegItem("PIX_PORTAL").fAddress;
    const uint8_t  highGain        = pRD53->getRegItem("HighGain_LIN").fValue;
    const uint8_t  chipID          = pRD53->getId();
    auto&          mask            = doDefault == true ? pRD53->getPixelsMaskDefault() : pRD53->getPixelsMask();

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
            if((std::find(mask[col].Enable.begin(), mask[col].Enable.end(), true) == mask[col].Enable.end()) &&
               (std::find(mask[col + 1].Enable.begin(), mask[col].Enable.end(), true) == mask[col + 1].Enable.end()))
                continue;

            RD53ACmd::serialize(RD53ACmd::WrReg{chipID, REGION_COL_ADDR, (uint16_t)(col / 2)}, commandList);

            for(auto row = 0u; row < RD53A::NROWS; row++)
            {
                if((mask[col].Enable[row] == true) || (mask[col + 1].Enable[row] == true))
                {
                    data = getPixelConfig(mask, row, col, highGain);

                    RD53ACmd::serialize(RD53ACmd::WrReg{chipID, REGION_ROW_ADDR, (uint16_t)row}, commandList);
                    RD53ACmd::serialize(RD53ACmd::WrReg{chipID, PIX_PORTAL_ADDR, (uint16_t)data}, commandList);
                }
            }

            if((commandList.size() * 2 + RD53A::NROWS + 1) > (1 << RD53FWconstants::NBIT_SLOWCMD_FIFO))
            {
                static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pRD53->getHybridId());
                commandList.clear();
            }
        }
    }
    else
    {
        RD53ACmd::serialize(RD53ACmd::WrReg{chipID, pRD53->getRegItem("PIX_MODE").fAddress, 0x8}, commandList);

        std::vector<uint16_t> data;

        for(auto col = 0u; col < RD53A::NCOLS; col += 2)
        {
            RD53ACmd::serialize(RD53ACmd::WrReg{chipID, REGION_COL_ADDR, (uint16_t)(col / 2)}, commandList);
            RD53ACmd::serialize(RD53ACmd::WrReg{chipID, REGION_ROW_ADDR, 0x0}, commandList);

            for(auto row = 0u; row < RD53A::NROWS; row++)
            {
                data.push_back(getPixelConfig(mask, row, col, highGain));

                if((row % RD53Constants::NREGIONS_LONGCMD) == (RD53Constants::NREGIONS_LONGCMD - 1))
                {
                    RD53ACmd::serialize(RD53ACmd::WrRegLong{chipID, PIX_PORTAL_ADDR, data}, commandList);
                    data.clear();
                }
            }

            if((commandList.size() + RD53A::NROWS + 1) * 2 > (1 << RD53FWconstants::NBIT_SLOWCMD_FIFO))
            {
                static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pRD53->getHybridId());
                commandList.clear();
            }
        }
    }

    if(commandList.size() != 0) static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pRD53->getHybridId());
}

void RD53AInterface::Reset(Ph2_HwDescription::ReadoutChip* pChip, const int resetType)
// ################################################
// # resetType = 0 --> Reset Channel Synchronizer #
// # resetType = 1 --> Reset Command Decoder      #
// # resetType = 2 --> Reset Global Configuration #
// # resetType = 3 --> Reset Monitor Data         #
// # resetType = 4 --> Reset Aurora               #
// # resetType = 5 --> Reset Serializer           #
// # resetType = 6 --> Reset ADC                  #
// # default       --> Reset Aurora pattern       #
// ################################################
{
    this->setBoard(pChip->getBeBoardId());

    const int duration = 0x0004; // @CONST@

    switch(resetType)
    {
    case 0:
        RD53Interface::sendCommand(pChip, RD53ACmd::WrReg{(uint8_t)pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 0}); // Reset Channel Synchronizer
        RD53Interface::sendCommand(pChip, RD53ACmd::GlobalPulse{(uint8_t)pChip->getId(), duration});
        break;

    case 1:
        RD53Interface::sendCommand(pChip, RD53ACmd::WrReg{(uint8_t)pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 1}); // Reset Command Decoder
        RD53Interface::sendCommand(pChip, RD53ACmd::GlobalPulse{(uint8_t)pChip->getId(), duration});
        break;

    case 2:
        RD53Interface::sendCommand(pChip, RD53ACmd::WrReg{(uint8_t)pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 2}); // Reset Global Configuration
        RD53Interface::sendCommand(pChip, RD53ACmd::GlobalPulse{(uint8_t)pChip->getId(), duration});
        break;

    case 3:
        RD53Interface::sendCommand(pChip, RD53ACmd::WrReg{(uint8_t)pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 3}); // Reset Monitor Data
        RD53Interface::sendCommand(pChip, RD53ACmd::GlobalPulse{(uint8_t)pChip->getId(), duration});
        break;

    case 4:
        RD53Interface::sendCommand(pChip, RD53ACmd::WrReg{(uint8_t)pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 4}); // Reset Aurora
        RD53Interface::sendCommand(pChip, RD53ACmd::GlobalPulse{(uint8_t)pChip->getId(), duration});
        break;

    case 5:
        RD53Interface::sendCommand(pChip, RD53ACmd::WrReg{(uint8_t)pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 5}); // Reset Serializer
        RD53Interface::sendCommand(pChip, RD53ACmd::GlobalPulse{(uint8_t)pChip->getId(), duration});
        break;

    case 6:
        RD53Interface::sendCommand(pChip, RD53ACmd::WrReg{(uint8_t)pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 6}); // Reset ADC
        RD53Interface::sendCommand(pChip, RD53ACmd::GlobalPulse{(uint8_t)pChip->getId(), duration});
        break;

    default: RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_AURORA, false); break;
    }
}

void RD53AInterface::ChipErrorReport(ReadoutChip* pChip)
{
    LOG(INFO) << BOLDBLUE << "LOCKLOSS_CNT        = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "LOCKLOSS_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "BITFLIP_WNG_CNT     = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "BITFLIP_WNG_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "BITFLIP_ERR_CNT     = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "BITFLIP_ERR_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "CMDERR_CNT          = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "CMDERR_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "SKIPPED_TRIGGER_CNT = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "SKIPPED_TRIGGER_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "HITOR_0_CNT         = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "HITOR_0_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "HITOR_1_CNT         = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "HITOR_0_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "HITOR_2_CNT         = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "HITOR_0_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "HITOR_3_CNT         = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "HITOR_0_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "BCID_CNT            = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "BCID_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
    LOG(INFO) << BOLDBLUE << "TRIG_CNT            = " << BOLDYELLOW << RD53Interface::ReadChipReg(pChip, "TRIG_CNT") << std::setfill(' ') << std::setw(8) << "" << RESET;
}

void RD53AInterface::PackWriteCommand(ReadoutChip* pChip, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg)
{
    RD53ACmd::serialize(RD53ACmd::WrReg{(uint8_t)pChip->getId(), pChip->getRegItem(regName).fAddress, data}, chipCommandList);
    if(updateReg == true) pChip->setReg(regName, data);
}

} // namespace Ph2_HwInterface
