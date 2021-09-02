/*!
  \file                  RD53Interface.cc
  \brief                 User interface to the RD53 readout chip
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Interface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
RD53Interface::RD53Interface(const BeBoardFWMap& pBoardMap) : ReadoutChipInterface(pBoardMap) {}

int RD53Interface::CheckChipID(Chip* pChip, const int chipIDfromDB)
{
    // @TMP@ : to be implemented for RD53B
    auto chipID = RD53Interface::ReadChipReg(pChip, "CHIP_ID");
    if(chipID == chipIDfromDB)
        LOG(INFO) << GREEN << "Chip ID: " << BOLDYELLOW << chipID << RESET << GREEN << " --> same as in database: " << BOLDYELLOW << chipIDfromDB << RESET;
    else
        LOG(WARNING) << GREEN << "Chip ID: " << BOLDYELLOW << chipID << RESET << GREEN << " --> different from database: " << BOLDYELLOW << chipIDfromDB << RESET;
    return chipID;
}

bool RD53Interface::ConfigureChip(Chip* pChip, bool pVerifLoop, uint32_t pBlockSize)
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

            nameAndValue = RD53Interface::SplitSpecialRegisters(std::string(cRegItem->first), cRegItem->second, pRD53RegMap);

            if(cRegItem->first == "CLK_DATA_DELAY") break;
        }
    }

    if(doWriteClkDataDelay == true)
    {
        RD53Interface::WriteChipReg(pChip, nameAndValue.first, nameAndValue.second, false);
        static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(std::vector<uint16_t>(RD53Constants::NSYNC_WORS, RD53CmdEncoder::SYNC), -1);
        RD53Interface::WriteChipReg(pChip, nameAndValue.first, nameAndValue.second, pVerifLoop);
    }

    // ###############################
    // # Programmig global registers #
    // ###############################
    static const char* registerBlackList[] = {
        "HighGain_LIN", "ADC_OFFSET_VOLT", "ADC_MAXIMUM_VOLT", "TEMPSENS_IDEAL_FACTOR", "CLK_DATA_DELAY_CMD_DELAY", "CLK_DATA_DELAY_CLK_DELAY", "CLK_DATA_DELAY_2INV_DELAY"};

    for(const auto& cRegItem: pRD53RegMap)
        if(cRegItem.second.fPrmptCfg == true)
        {
            auto i = 0u;
            for(i = 0u; i < arraySize(registerBlackList); i++)
                if(cRegItem.first == registerBlackList[i]) break;
            if(i == arraySize(registerBlackList))
            {
                std::pair<std::string, uint16_t> nameAndValue(RD53Interface::SplitSpecialRegisters(std::string(cRegItem.first), cRegItem.second, pRD53RegMap));

                if(cRegItem.first == "CDR_CONFIG")
                {
                    RD53Interface::sendCommand(static_cast<RD53*>(pChip), RD53Cmd::ECR());
                    RD53Interface::sendCommand(static_cast<RD53*>(pChip), RD53Cmd::ECR());
                    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
                }

                RD53Interface::WriteChipReg(pChip, nameAndValue.first, nameAndValue.second, pVerifLoop);
            }
        }

    // ###################################
    // # Programmig pixel cell registers #
    // ###################################
    RD53Interface::WriteRD53Mask(static_cast<RD53*>(const_cast<Chip*>(pChip)), false, true);

    return true;
}

void RD53Interface::InitRD53Downlink(const BeBoard* pBoard)
{
    this->setBoard(pBoard->getId());

    LOG(INFO) << GREEN << "Down-link phase initialization..." << RESET;
    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(std::vector<uint16_t>(RD53Constants::NSYNC_WORS, RD53CmdEncoder::SYNC), -1);
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
}

void RD53Interface::InitRD53UplinkSpeed(ReadoutChip* pChip)
{
    this->setBoard(pChip->getBeBoardId());

    uint32_t auroraSpeed = static_cast<RD53FWInterface*>(fBoardFW)->ReadoutSpeed();
    RD53Interface::WriteChipReg(pChip, "CDR_CONFIG", (auroraSpeed == 0 ? RD53Constants::CDRCONFIG_1Gbit : RD53Constants::CDRCONFIG_640Mbit), false);
    RD53Interface::sendCommand(pChip, RD53Cmd::ECR());

    LOG(INFO) << GREEN << "Up-link speed: " << BOLDYELLOW << (auroraSpeed == 0 ? "1.28 Gbit/s" : "640 Mbit/s") << RESET;
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
}

void RD53Interface::InitRD53Uplinks(ReadoutChip* pChip, int nActiveLanes)
{
    RD53Interface::ConfigureChip(pChip, false);

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
    RD53Interface::sendCommand(pChip, RD53Cmd::GlobalPulse(pChip->getId(), 0x0001));
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    // ##############################
    // # Set standard AURORA output #
    // ##############################
    RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_AURORA, false);

    // ###############################################################
    // # Enable monitoring (needed for AutoRead register monitoring) #
    // ###############################################################
    RD53Interface::WriteChipReg(pChip, "GLOBAL_PULSE_ROUTE", 0x100, false); // 0x100 = start monitoring
    RD53Interface::sendCommand(pChip, RD53Cmd::GlobalPulse(pChip->getId(), 0x0004));
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;

    RD53Interface::InitRD53UplinkSpeed(pChip);
}

bool RD53Interface::WriteChipReg(Chip* pChip, const std::string& regName, const uint16_t data, bool pVerifLoop)
{
    this->setBoard(pChip->getBeBoardId());

    RD53Interface::sendCommand(static_cast<RD53*>(pChip), RD53Cmd::WrReg(pChip->getId(), pChip->getRegItem(regName).fAddress, data));
    if((regName == "VCAL_HIGH") || (regName == "VCAL_MED")) std::this_thread::sleep_for(std::chrono::microseconds(VCALSLEEP)); // @TMP@

    bool status = true;
    if(pVerifLoop == true)
    {
        if(regName == "PIX_PORTAL")
        {
            auto pixMode = RD53Interface::ReadChipReg(pChip, "PIX_MODE");
            if(pixMode == 0)
            {
                auto regReadback = RD53Interface::ReadRD53Reg(static_cast<RD53*>(pChip), regName);
                auto row         = RD53Interface::ReadChipReg(pChip, "REGION_ROW");
                if(regReadback.size() == 0 /* @TMP@ */ || regReadback[0].first != row || regReadback[0].second != data) status = false;
            }
        }
        else if(data != RD53Interface::ReadChipReg(pChip, regName))
            status = false;
    }

    if(status == false)
    {
        LOG(ERROR) << BOLDRED << "Error when reading back what was written into RD53 reg. " << BOLDYELLOW << regName << RESET;
        return false;
    }

    if((pVerifLoop == true) && (status == true)) pChip->setReg(regName, data);
    return true;
}

void RD53Interface::WriteBoardBroadcastChipReg(const BeBoard* pBoard, const std::string& regName, const uint16_t data)
{
    this->setBoard(pBoard->getId());

    ChipRegItem Reg = pBoard->at(0)->at(0)->at(0)->getRegItem(regName);
    Reg.fValue      = data;

    std::pair<std::string, uint16_t> nameAndValue(RD53Interface::SplitSpecialRegisters(regName, Reg, pBoard->at(0)->at(0)->at(0)->getRegMap()));
    const uint16_t                   address = pBoard->at(0)->at(0)->at(0)->getRegItem(nameAndValue.first).fAddress;

    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(RD53Cmd::WrReg(RD53Constants::BROADCAST_CHIPID, address, nameAndValue.second).getFrames(), -1);

    if((regName == "VCAL_HIGH") || (regName == "VCAL_MED")) std::this_thread::sleep_for(std::chrono::microseconds(VCALSLEEP)); // @TMP@
}

uint16_t RD53Interface::ReadChipReg(Chip* pChip, const std::string& regName)
{
    this->setBoard(pChip->getBeBoardId());

    const int nAttempts = 2; // @CONST@
    for(auto attempt = 0; attempt < nAttempts; attempt++)
    {
        auto regReadback = RD53Interface::ReadRD53Reg(static_cast<RD53*>(pChip), regName);
        if(regReadback.size() == 0)
        {
            LOG(WARNING) << BLUE << "Empty register readback, attempt n. " << YELLOW << attempt + 1 << BLUE << "/" << YELLOW << nAttempts << RESET;
            std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
        }
        else
            return regReadback[0].second;
    }

    LOG(ERROR) << BOLDRED << "Empty register readback FIFO after " << BOLDYELLOW << nAttempts << BOLDRED " attempts" << RESET;

    return 0;
}

std::vector<std::pair<uint16_t, uint16_t>> RD53Interface::ReadRD53Reg(ReadoutChip* pChip, const std::string& regName)
{
    this->setBoard(pChip->getBeBoardId());

    RD53Interface::sendCommand(pChip, RD53Cmd::RdReg(pChip->getId(), pChip->getRegItem(regName).fAddress));
    auto regReadback = static_cast<RD53FWInterface*>(fBoardFW)->ReadChipRegisters(pChip);

    for(auto i = 0u; i < regReadback.size(); i++)
        // Removing bit related to PIX_PORTAL register identification
        regReadback[i].first = regReadback[i].first & static_cast<uint16_t>(RD53Shared::setBits(RD53Constants::NBIT_ADDR));

    return regReadback;
}

std::pair<std::string, uint16_t> RD53Interface::SplitSpecialRegisters(std::string regName, const ChipRegItem& Reg, ChipRegMap& pRD53RegMap)
{
    uint16_t value = Reg.fValue;

    if(regName == "CLK_DATA_DELAY_CMD_DELAY") { value = Reg.fValue | (value & (RD53Shared::setBits(pRD53RegMap["CLK_DATA_DELAY"].fBitSize) - RD53Shared::setBits(Reg.fBitSize))); }
    else if(regName == "CLK_DATA_DELAY_CLK_DELAY")
    {
        value = (Reg.fValue << pRD53RegMap["CLK_DATA_DELAY_CMD_DELAY"].fBitSize) |
                (value & (RD53Shared::setBits(pRD53RegMap["CLK_DATA_DELAY"].fBitSize) - (RD53Shared::setBits(Reg.fBitSize) << pRD53RegMap["CLK_DATA_DELAY_CMD_DELAY"].fBitSize)));
    }
    else if(regName == "CLK_DATA_DELAY_2INV_DELAY")
    {
        value = (Reg.fValue << (pRD53RegMap["CLK_DATA_DELAY_CMD_DELAY"].fBitSize + pRD53RegMap["CLK_DATA_DELAY_CLK_DELAY"].fBitSize)) |
                (value & (RD53Shared::setBits(pRD53RegMap["CLK_DATA_DELAY"].fBitSize) -
                          (RD53Shared::setBits(Reg.fBitSize) << (pRD53RegMap["CLK_DATA_DELAY_CMD_DELAY"].fBitSize + pRD53RegMap["CLK_DATA_DELAY_CLK_DELAY"].fBitSize))));
    }
    else if(regName == "CLK_DATA_DELAY")
    {
        value = Reg.fValue;
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
    if(col <= RD53::SYNC.colStop)
        return bits::pack<8, 8>(bits::pack<1, 1, 1>(mask[col + 1].HitBus[row], mask[col + 1].InjEn[row], mask[col + 1].Enable[row]),
                                bits::pack<1, 1, 1>(mask[col + 0].HitBus[row], mask[col + 0].InjEn[row], mask[col + 0].Enable[row]));
    else if(col <= RD53::LIN.colStop)
        return bits::pack<8, 8>(bits::pack<1, 4, 1, 1, 1>(highGain, mask[col + 1].TDAC[row], mask[col + 1].HitBus[row], mask[col + 1].InjEn[row], mask[col + 1].Enable[row]),
                                bits::pack<1, 4, 1, 1, 1>(highGain, mask[col + 0].TDAC[row], mask[col + 0].HitBus[row], mask[col + 0].InjEn[row], mask[col + 0].Enable[row]));
    else
        return bits::pack<8, 8>(
            bits::pack<1, 4, 1, 1, 1>(mask[col + 1].TDAC[row] > 15, abs(15 - mask[col + 1].TDAC[row]), mask[col + 1].HitBus[row], mask[col + 1].InjEn[row], mask[col + 1].Enable[row]),
            bits::pack<1, 4, 1, 1, 1>(mask[col + 0].TDAC[row] > 15, abs(15 - mask[col + 0].TDAC[row]), mask[col + 0].HitBus[row], mask[col + 0].InjEn[row], mask[col + 0].Enable[row]));
}

void RD53Interface::WriteRD53Mask(RD53* pRD53, bool doSparse, bool doDefault)
{
    this->setBoard(pRD53->getBeBoardId());

    std::vector<uint16_t> commandList;
    std::vector<uint16_t> syncList(RD53Constants::NSYNC_WORS, RD53CmdEncoder::SYNC);

    const uint16_t                   REGION_COL_ADDR = pRD53->getRegItem("REGION_COL").fAddress;
    const uint16_t                   REGION_ROW_ADDR = pRD53->getRegItem("REGION_ROW").fAddress;
    const uint16_t                   PIX_PORTAL_ADDR = pRD53->getRegItem("PIX_PORTAL").fAddress;
    const uint8_t                    highGain        = pRD53->getRegItem("HighGain_LIN").fValue;
    const uint8_t                    chipID          = pRD53->getId();
    std::vector<perColumnPixelData>& mask            = doDefault == true ? *pRD53->getPixelsMaskDefault() : *pRD53->getPixelsMask();

    // ##########################
    // # Disable default config #
    // ##########################
    RD53Cmd::WrReg(chipID, pRD53->getRegItem("PIX_DEFAULT_CONFIG").fAddress, 0x0).appendTo(commandList);

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
        RD53Cmd::WrReg(chipID, pRD53->getRegItem("PIX_MODE").fAddress, 0x27).appendTo(commandList);
        RD53Cmd::WrReg(chipID, pRD53->getRegItem("PIX_PORTAL").fAddress, 0x0).appendTo(commandList);
        RD53Cmd::WrReg(chipID, pRD53->getRegItem("PIX_MODE").fAddress, 0x0).appendTo(commandList);

        uint16_t data;

        for(auto col = 0u; col < RD53::nCols; col += 2)
        {
            if((std::find(mask[col].Enable.begin(), mask[col].Enable.end(), true) == mask[col].Enable.end()) &&
               (std::find(mask[col + 1].Enable.begin(), mask[col].Enable.end(), true) == mask[col + 1].Enable.end()))
                continue;

            RD53Cmd::WrReg(chipID, REGION_COL_ADDR, col / 2).appendTo(commandList);

            for(auto row = 0u; row < RD53::nRows; row++)
            {
                if((mask[col].Enable[row] == true) || (mask[col + 1].Enable[row] == true))
                {
                    data = getPixelConfig(mask, row, col, highGain);

                    RD53Cmd::WrReg(chipID, REGION_ROW_ADDR, row).appendTo(commandList);
                    RD53Cmd::WrReg(chipID, PIX_PORTAL_ADDR, data).appendTo(commandList);
                }
            }
        }
    }
    else
    {
        RD53Cmd::WrReg(chipID, pRD53->getRegItem("PIX_MODE").fAddress, 0x8).appendTo(commandList);

        std::vector<uint16_t> data;

        for(auto col = 0u; col < RD53::nCols; col += 2)
        {
            RD53Cmd::WrReg(chipID, REGION_COL_ADDR, col / 2).appendTo(commandList);
            RD53Cmd::WrReg(chipID, REGION_ROW_ADDR, 0x0).appendTo(commandList);

            for(auto row = 0u; row < RD53::nRows; row++)
            {
                data.push_back(getPixelConfig(mask, row, col, highGain));

                if((row % RD53Constants::NREGIONS_LONGCMD) == (RD53Constants::NREGIONS_LONGCMD - 1))
                {
                    RD53Cmd::WrRegLong(chipID, PIX_PORTAL_ADDR, data).appendTo(commandList);
                    data.clear();
                }
            }
        }
    }

    if(commandList.size() != 0) static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pRD53->getHybridId());
}

bool RD53Interface::ConfigureChipOriginalMask(ReadoutChip* pChip, bool pVerifLoop, uint32_t pBlockSize)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    RD53Interface::WriteRD53Mask(pRD53, false, true);

    return true;
}

bool RD53Interface::MaskAllChannels(ReadoutChip* pChip, bool mask, bool pVerifLoop)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    if(mask == true)
        pRD53->disableAllPixels();
    else
        pRD53->enableAllPixels();

    RD53Interface::WriteRD53Mask(pRD53, false, false);

    return true;
}

bool RD53Interface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const ChannelGroupBase* group, bool mask, bool inject, bool pVerifLoop)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    for(auto row = 0u; row < RD53::nRows; row++)
        for(auto col = 0u; col < RD53::nCols; col++)
        {
            if(mask == true) pRD53->enablePixel(row, col, group->isChannelEnabled(row, col) && (*pRD53->getPixelsMaskDefault())[col].Enable[row]);
            if(inject == true)
                pRD53->injectPixel(row, col, group->isChannelEnabled(row, col) && (*pRD53->getPixelsMaskDefault())[col].Enable[row]);
            else
                pRD53->injectPixel(row, col, group->isChannelEnabled(row, col) && (*pRD53->getPixelsMaskDefault())[col].Enable[row] && (*pRD53->getPixelsMaskDefault())[col].InjEn[row]);
        }

    RD53Interface::WriteRD53Mask(pRD53, true, false);

    return true;
}

// ##################
// # PRBS generator #
// ##################

void RD53Interface::StartPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip) { RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_PRBS, false); }
void RD53Interface::StopPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip) { RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_AURORA, false); }

void RD53Interface::Reset(Ph2_HwDescription::ReadoutChip* pChip, const int resetType)
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
        RD53Interface::sendCommand(pChip, RD53Cmd::WrReg(pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 0)); // Reset Channel Synchronizer
        RD53Interface::sendCommand(pChip, RD53Cmd::GlobalPulse(pChip->getId(), duration));
        break;

    case 1:
        RD53Interface::sendCommand(pChip, RD53Cmd::WrReg(pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 1)); // Reset Command Decoder
        RD53Interface::sendCommand(pChip, RD53Cmd::GlobalPulse(pChip->getId(), duration));
        break;

    case 2:
        RD53Interface::sendCommand(pChip, RD53Cmd::WrReg(pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 2)); // Reset Global Configuration
        RD53Interface::sendCommand(pChip, RD53Cmd::GlobalPulse(pChip->getId(), duration));
        break;

    case 3:
        RD53Interface::sendCommand(pChip, RD53Cmd::WrReg(pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 3)); // Reset Monitor Data
        RD53Interface::sendCommand(pChip, RD53Cmd::GlobalPulse(pChip->getId(), duration));
        break;

    case 4:
        RD53Interface::sendCommand(pChip, RD53Cmd::WrReg(pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 4)); // Reset Aurora
        RD53Interface::sendCommand(pChip, RD53Cmd::GlobalPulse(pChip->getId(), duration));
        break;

    case 5:
        RD53Interface::sendCommand(pChip, RD53Cmd::WrReg(pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 5)); // Reset Serializer
        RD53Interface::sendCommand(pChip, RD53Cmd::GlobalPulse(pChip->getId(), duration));
        break;

    case 6:
        RD53Interface::sendCommand(pChip, RD53Cmd::WrReg(pChip->getId(), RD53Constants::GLOBAL_PULSE_ADDR, 1 << 6)); // Reset ADC
        RD53Interface::sendCommand(pChip, RD53Cmd::GlobalPulse(pChip->getId(), duration));
        break;

    default: RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_AURORA, false); break;
    }
}

void RD53Interface::ChipErrorReport(ReadoutChip* pChip)
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

bool RD53Interface::WriteChipAllLocalReg(ReadoutChip* pChip, const std::string& regName, ChipContainer& pValue, bool pVerifLoop)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    for(auto row = 0u; row < RD53::nRows; row++)
        for(auto col = 0u; col < RD53::nCols; col++) pRD53->setTDAC(row, col, pValue.getChannel<uint16_t>(row, col));

    RD53Interface::WriteRD53Mask(pRD53, false, false);

    return true;
}

void RD53Interface::ReadChipAllLocalReg(ReadoutChip* pChip, const std::string& regName, ChipContainer& pValue)
{
    for(auto row = 0u; row < RD53::nRows; row++)
        for(auto col = 0u; col < RD53::nCols; col++) pValue.getChannel<uint16_t>(row, col) = static_cast<RD53*>(pChip)->getTDAC(row, col);
}

void RD53Interface::PackChipCommands(ReadoutChip* pChip, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg)
{
    RD53Cmd::WrReg(pChip->getId(), pChip->getRegItem(regName).fAddress, data).appendTo(chipCommandList);
    if(updateReg == true) pChip->setReg(regName, data);
}

void RD53Interface::SendChipCommandsPack(const BeBoard* pBoard, const std::vector<uint16_t>& chipCommandList, int hybridId)
{
    this->setBoard(pBoard->getId());
    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(chipCommandList, hybridId);
}

void RD53Interface::PackHybridCommands(const BeBoard* pBoard, const std::vector<uint16_t>& chipCommandList, int hybridId, std::vector<uint32_t>& hybridCommandList)
{
    this->setBoard(pBoard->getId());
    static_cast<RD53FWInterface*>(fBoardFW)->ComposeAndPackChipCommands(chipCommandList, hybridId, hybridCommandList);
}

void RD53Interface::SendHybridCommandsPack(const BeBoard* pBoard, const std::vector<uint32_t>& hybridCommandList)
{
    this->setBoard(pBoard->getId());
    static_cast<RD53FWInterface*>(fBoardFW)->SendChipCommandsPack(hybridCommandList);
}

// ###########################
// # Dedicated to monitoring #
// ###########################

uint32_t RD53Interface::getADCobservable(const std::string& observableName, bool* isCurrentNotVoltage)
{
    uint32_t voltageObservable(0), currentObservable(0);

    const std::unordered_map<std::string, uint32_t> currentMultiplexer = {
        {"Iref", 0x00},          {"IBIASP1_SYNC", 0x01}, {"IBIASP2_SYNC", 0x02},  {"IBIAS_DISC_SYNC", 0x03}, {"IBIAS_SF_SYNC", 0x04},  {"ICTRL_SYNCT_SYNC", 0x05}, {"IBIAS_KRUM_SYNC", 0x06},
        {"COMP_LIN", 0x07},      {"FC_BIAS_LIN", 0x08},  {"KRUM_CURR_LIN", 0x09}, {"LDAC_LIN", 0x0A},        {"PA_IN_BIAS_LIN", 0x0B}, {"COMP_DIFF", 0x0C},        {"PRECOMP_DIFF", 0x0D},
        {"FOL_DIFF", 0x0E},      {"PRMP_DIFF", 0x0F},    {"LCC_DIFF", 0x10},      {"VFF_DIFF", 0x11},        {"VTH1_DIFF", 0x12},      {"VTH2_DIFF", 0x13},        {"CDR_CP_IBIAS", 0x14},
        {"VCO_BUFF_BIAS", 0x15}, {"VCO_IBIAS", 0x16},    {"CML_TAP0_BIAS", 0x17}, {"CML_TAP1_BIAS", 0x18},   {"CML_TAP2_BIAS", 0x19}};

    const std::unordered_map<std::string, uint32_t> voltageMultiplexer = {
        {"ADCbandgap", 0x00},      {"CAL_MED", 0x01},         {"CAL_HI", 0x02},         {"TEMPSENS_1", 0x03},      {"RADSENS_1", 0x04},       {"TEMPSENS_2", 0x05},      {"RADSENS_2", 0x06},
        {"TEMPSENS_4", 0x07},      {"RADSENS_4", 0x08},       {"VREF_VDAC", 0x09},      {"VOUT_BG", 0x0A},         {"IMUXoutput", 0x0B},      {"CAL_MED", 0x0C},         {"CAL_HI", 0x0D},
        {"RADSENS_3", 0x0E},       {"TEMPSENS_3", 0x0F},      {"REF_KRUM_LIN", 0x10},   {"Vthreshold_LIN", 0x11},  {"VTH_SYNC", 0x12},        {"VBL_SYNC", 0x13},        {"VREF_KRUM_SYNC", 0x14},
        {"VTH_HI_DIFF", 0x15},     {"VTH_LO_DIFF", 0x16},     {"VIN_ana_ShuLDO", 0x17}, {"VOUT_ana_ShuLDO", 0x18}, {"VREF_ana_ShuLDO", 0x19}, {"VOFF_ana_ShuLDO", 0x1A}, {"VIN_dig_ShuLDO", 0x1D},
        {"VOUT_dig_ShuLDO", 0x1E}, {"VREF_dig_ShuLDO", 0x1F}, {"VOFF_dig_ShuLDO", 0x20}};

    auto search = currentMultiplexer.find(observableName);
    if(search == currentMultiplexer.end())
    {
        if((search = voltageMultiplexer.find(observableName)) == voltageMultiplexer.end())
        {
            LOG(ERROR) << BOLDRED << "Wrong observable name: " << observableName << RESET;
            return -1;
        }
        else
            voltageObservable = search->second;
        if(isCurrentNotVoltage != nullptr) *isCurrentNotVoltage = false;
    }
    else
    {
        currentObservable = search->second;
        voltageObservable = voltageMultiplexer.find("IMUXoutput")->second;
        if(isCurrentNotVoltage != nullptr) *isCurrentNotVoltage = true;
    }

    return bits::pack<1, 6, 7>(true, currentObservable, voltageObservable);
}

float RD53Interface::ReadChipMonitor(ReadoutChip* pChip, const std::string& observableName)
{
    this->setBoard(pChip->getBeBoardId());

    const float measError = 4.0; // Current or Voltage measurement error due to MONITOR_CONFIG resolution [%]
    float       value;
    bool        isCurrentNotVoltage;
    uint32_t    observable;

    observable = RD53Interface::getADCobservable(observableName, &isCurrentNotVoltage);

    if(observableName.find("TEMPSENS") != std::string::npos)
    {
        value = RD53Interface::measureTemperature(pChip, observable);
        LOG(INFO) << BOLDBLUE << "\t--> " << observableName << ": " << BOLDYELLOW << std::setprecision(3) << value << " +/- " << value * measError / 100 << BOLDBLUE << " C" << std::setprecision(-1)
                  << RESET;
    }
    else
    {
        value = measureVoltageCurrent(pChip, observable, isCurrentNotVoltage);
        LOG(INFO) << BOLDBLUE << "\t--> " << observableName << ": " << BOLDYELLOW << std::setprecision(3) << value << " +/- " << value * measError / 100 << BOLDBLUE
                  << (isCurrentNotVoltage == true ? " uA" : " V") << std::setprecision(-1) << RESET;
    }

    return value;
}

uint32_t RD53Interface::ReadChipADC(Ph2_HwDescription::ReadoutChip* pChip, const std::string& observableName)
{
    uint32_t observable = RD53Interface::getADCobservable(observableName, nullptr);
    return RD53Interface::measureADC(pChip, observable);
}

uint32_t RD53Interface::measureADC(ReadoutChip* pChip, uint32_t data)
{
    this->setBoard(pChip->getBeBoardId());

    const uint16_t GLOBAL_PULSE_ROUTE = pChip->getRegItem("GLOBAL_PULSE_ROUTE").fAddress;
    const uint8_t  chipID             = pChip->getId();
    const uint16_t trimADC            = bits::pack<1, 5, 6>(true, pChip->getRegItem("MONITOR_CONFIG_BG").fValue, pChip->getRegItem("MONITOR_CONFIG_ADC").fValue);
    // [10:6] band-gap trim [5:0] ADC trim. According to wafer probing they should give an average VrefADC of 0.9 V
    const uint16_t GlbPulseVal = RD53Interface::ReadChipReg(pChip, "GLOBAL_PULSE_ROUTE");

    std::vector<uint16_t> commandList;

    RD53Cmd::WrReg(chipID, pChip->getRegItem("MONITOR_CONFIG").fAddress, trimADC).appendTo(commandList);
    RD53Cmd::WrReg(chipID, GLOBAL_PULSE_ROUTE, 0x0040).appendTo(commandList); // Reset Monitor Data
    RD53Cmd::GlobalPulse(pChip->getId(), 0x0004).appendTo(commandList);
    RD53Cmd::WrReg(chipID, GLOBAL_PULSE_ROUTE, 0x0008).appendTo(commandList); // Clear Monitor Data
    RD53Cmd::GlobalPulse(pChip->getId(), 0x0004).appendTo(commandList);
    RD53Cmd::WrReg(chipID, pChip->getRegItem("MONITOR_SELECT").fAddress, data).appendTo(commandList); // 14 bits: bit 13 enable, bits 7:12 I-Mon, bits 0:6 V-Mon
    RD53Cmd::WrReg(chipID, GLOBAL_PULSE_ROUTE, 0x1000).appendTo(commandList);                         // Trigger Monitor Data to start conversion
    RD53Cmd::GlobalPulse(pChip->getId(), 0x0004).appendTo(commandList);
    RD53Cmd::WrReg(chipID, GLOBAL_PULSE_ROUTE, GlbPulseVal).appendTo(commandList); // Restore value in Global Pulse Route

    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pChip->getHybridId());
    return RD53Interface::ReadChipReg(pChip, "MONITORING_DATA_ADC");
}

float RD53Interface::measureVoltageCurrent(ReadoutChip* pChip, uint32_t data, bool isCurrentNotVoltage)
{
    const float safetyMargin = 0.9; // @CONST@

    auto ADC = RD53Interface::measureADC(pChip, data);
    if(ADC > (RD53Shared::setBits(pChip->getNumberOfBits("MONITORING_DATA_ADC")) + 1.) * safetyMargin)
        LOG(WARNING) << BOLDRED << "\t\t--> ADC measurement in saturation (ADC = " << BOLDYELLOW << ADC << BOLDRED
                     << "): likely the IMUX resistor, that converts the current into a voltage, is not connected" << RESET;

    return RD53Interface::convertADC2VorI(pChip, ADC, isCurrentNotVoltage);
}

float RD53Interface::measureTemperature(ReadoutChip* pChip, uint32_t data)
{
    // ################################################################################################
    // # Temperature measurement is done by measuring twice, once with high bias, once with low bias  #
    // # Temperature is calculated based on the difference of the two, with the formula on the bottom #
    // # idealityFactor = 1225 [1/1000]                                                               #
    // ################################################################################################

    // #####################
    // # Natural constants #
    // #####################
    const float   T0C            = 273.15;         // [Kelvin]
    const float   kb             = 1.38064852e-23; // [J/K]
    const float   e              = 1.6021766208e-19;
    const float   R              = 15;   // By circuit design
    const uint8_t sensorDEM      = 0x0E; // Sensor Dynamic Element Matching bits needed to trim the thermistors
    const float   idealityFactor = pChip->getRegItem("TEMPSENS_IDEAL_FACTOR").fValue / 1e3;

    uint16_t sensorConfigData; // Enable[5], DEM[4:1], SEL_BIAS[0] (x2 ... 10 bit in total for the sensors in each sensor config register)

    // Get high bias voltage
    sensorConfigData = bits::pack<1, 4, 1, 1, 4, 1>(true, sensorDEM, 0, true, sensorDEM, 0);
    RD53Interface::WriteChipReg(pChip, "SENSOR_CONFIG_0", sensorConfigData);
    RD53Interface::WriteChipReg(pChip, "SENSOR_CONFIG_1", sensorConfigData);
    auto valueLow = RD53Interface::convertADC2VorI(pChip, RD53Interface::measureADC(pChip, data));

    // Get low bias voltage
    sensorConfigData = bits::pack<1, 4, 1, 1, 4, 1>(true, sensorDEM, 1, true, sensorDEM, 1);
    RD53Interface::WriteChipReg(pChip, "SENSOR_CONFIG_0", sensorConfigData);
    RD53Interface::WriteChipReg(pChip, "SENSOR_CONFIG_1", sensorConfigData);
    auto valueHigh = RD53Interface::convertADC2VorI(pChip, RD53Interface::measureADC(pChip, data));

    return e / (idealityFactor * kb * log(R)) * (valueHigh - valueLow) - T0C;
}

float RD53Interface::convertADC2VorI(ReadoutChip* pChip, uint32_t value, bool isCurrentNotVoltage)
{
    // #####################################################################
    // # ADCoffset     =  63 [1/10mV] Offset due to ground shift           #
    // # actualVrefADC = 839 [mV]     Lower than VrefADC due to parasitics #
    // #####################################################################
    const float resistorI2V   = 0.01; // [MOhm]
    const float ADCoffset     = pChip->getRegItem("ADC_OFFSET_VOLT").fValue / 1e4;
    const float actualVrefADC = pChip->getRegItem("ADC_MAXIMUM_VOLT").fValue / 1e3;

    const float ADCslope = (actualVrefADC - ADCoffset) / (RD53Shared::setBits(pChip->getNumberOfBits("MONITORING_DATA_ADC")) + 1); // [V/ADC]
    const float voltage  = ADCoffset + ADCslope * value;
    return voltage / (isCurrentNotVoltage == true ? resistorI2V : 1);
}

float RD53Interface::ReadHybridTemperature(ReadoutChip* pChip)
{
    this->setBoard(pChip->getBeBoardId());
    return static_cast<RD53FWInterface*>(fBoardFW)->ReadHybridTemperature(pChip->getHybridId());
}

float RD53Interface::ReadHybridVoltage(ReadoutChip* pChip)
{
    this->setBoard(pChip->getBeBoardId());
    return static_cast<RD53FWInterface*>(fBoardFW)->ReadHybridVoltage(pChip->getHybridId());
}

} // namespace Ph2_HwInterface
