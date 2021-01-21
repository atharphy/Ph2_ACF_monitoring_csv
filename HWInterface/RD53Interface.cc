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

bool RD53Interface::ConfigureChip(Chip* pChip, bool pVerifLoop, uint32_t pBlockSize)
{
    this->setBoard(pChip->getBeBoardId());

    ChipRegMap& pRD53RegMap = pChip->getRegMap();

    // ###################################
    // # Initializing chip communication #
    // ###################################
    RD53Interface::InitRD53Aurora(pChip);

    // ################################################
    // # Programming global registers from white list #
    // ################################################
    static const char* registerWhileList[] = {"PA_IN_BIAS_LIN", "FC_BIAS_LIN", "KRUM_CURR_LIN", "LDAC_LIN", "COMP_LIN", "REF_KRUM_LIN", "Vthreshold_LIN"};

    for(auto i = 0u; i < arraySize(registerWhileList); i++)
    {
        auto it = pRD53RegMap.find(registerWhileList[i]);
        if(it != pRD53RegMap.end()) RD53Interface::WriteChipReg(pChip, it->first, it->second.fValue, true);
    }

    // ###############################
    // # Programmig global registers #
    // ###############################
    static const char* registerBlackList[] = {"HighGain_LIN",
                                              "INJECTION_SELECT_DELAY",
                                              "CLK_DATA_DELAY_CLK_DELAY",
                                              "CLK_DATA_DELAY_DATA_DELAY",
                                              "I_MONITOR_SELECT",
                                              "V_MONITOR_SELECT",
                                              "ADC_OFFSET_VOLT",
                                              "ADC_MAXIMUM_VOLT",
                                              "TEMPSENS_IDEAL_FACTOR"};

    for(const auto& cRegItem: pRD53RegMap)
        if(cRegItem.second.fPrmptCfg == true)
        {
            auto i = 0u;
            for(i = 0u; i < arraySize(registerBlackList); i++)
                if(cRegItem.first == registerBlackList[i]) break;
            if(i == arraySize(registerBlackList))
            {
                std::string regName = cRegItem.first;
                uint16_t    value   = cRegItem.second.fValue;

                // #################
                // # Special cases #
                // #################
                if(cRegItem.first == "ADC_MONITOR_CONFIG")
                {
                    value =
                        cRegItem.second.fValue | (pRD53RegMap["MONITOR_CONFIG"].fValue & (RD53Shared::setBits(pRD53RegMap["MONITOR_CONFIG"].fBitSize) ^ RD53Shared::setBits(cRegItem.second.fBitSize)));
                    regName = "MONITOR_CONFIG";
                }
                else if(cRegItem.first == "BG_MONITOR_CONFIG")
                {
                    value = (cRegItem.second.fValue << pRD53RegMap["ADC_MONITOR_CONFIG"].fBitSize) |
                            (pRD53RegMap["MONITOR_CONFIG"].fValue &
                             (RD53Shared::setBits(pRD53RegMap["MONITOR_CONFIG"].fBitSize) ^ (RD53Shared::setBits(cRegItem.second.fBitSize) << pRD53RegMap["ADC_MONITOR_CONFIG"].fBitSize)));
                    regName = "MONITOR_CONFIG";
                }

                RD53Interface::WriteChipReg(pChip, regName, value, true);
            }
        }

    // ###################################
    // # Programmig pixel cell registers #
    // ###################################
    RD53Interface::WriteRD53Mask(static_cast<RD53*>(const_cast<Chip*>(pChip)), false, true, true);

    return true;
}

void RD53Interface::InitRD53Aurora(Chip* pChip, int nActiveLanes)
{
    this->setBoard(pChip->getBeBoardId());

    // ####################################
    // # Data stream phase initialization #
    // ####################################
    LOG(INFO) << GREEN << "Data-stream phase initialization..." << RESET;
    do
    {
        static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(std::vector<uint16_t>(RD53Constants::NSYNC_WORS, RD53CmdEncoder::SYNC), -1);

        // ###############################################################
        // # Enable monitoring (needed for AutoRead register monitoring) #
        // ###############################################################
        RD53Interface::WriteChipReg(pChip, "GLOBAL_PULSE_ROUTE", 0x100, false); // 0x100 = start monitoring
        RD53Interface::sendCommand(pChip, RD53Cmd::GlobalPulse(pChip->getChipId(), 0x4));

        usleep(DEEPSLEEP);
    } while(RD53Interface::ReadRD53Reg(pChip, "VCAL_HIGH").size() == 0);
    RD53Interface::sendCommand(pChip, RD53Cmd::ECR());
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;

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

    RD53Interface::WriteChipReg(pChip, "OUTPUT_CONFIG", RD53Shared::setBits(nActiveLanes) << 2, true); // Number of active lanes [5:2]
    // bits [8:7]: number of 40 MHz clocks +2 for data transfer out of pixel matrix
    // Default 0 means 2 clocks, may need higher value in case of large propagation
    // delays, for example at low VDDD voltage after irradiation
    // bits [5:2]: Aurora lanes. Default 0001 means single lane mode
    RD53Interface::WriteChipReg(pChip, "CML_CONFIG", 0x0F, true);         // CML_EN_LANE[3:0]: the actual number of lanes is determined by OUTPUT_CONFIG
    RD53Interface::WriteChipReg(pChip, "GLOBAL_PULSE_ROUTE", 0x30, true); // 0x30 = reset Aurora AND Serializer
    RD53Interface::sendCommand(pChip, RD53Cmd::GlobalPulse(pChip->getChipId(), 0x01));

    usleep(DEEPSLEEP);
}

bool RD53Interface::WriteChipReg(Chip* pChip, const std::string& pRegNode, const uint16_t data, bool pVerifLoop)
{
    this->setBoard(pChip->getBeBoardId());

    RD53Interface::sendCommand(pChip, RD53Cmd::WrReg(pChip->getChipId(), pChip->getRegItem(pRegNode).fAddress, data));
    if((pRegNode == "VCAL_HIGH") || (pRegNode == "VCAL_MED")) usleep(VCALSLEEP); // @TMP@

    if(pVerifLoop == true)
    {
        if(pRegNode == "PIX_PORTAL")
        {
            auto pixMode = RD53Interface::ReadChipReg(pChip, "PIX_MODE");
            if(pixMode == 0)
            {
                auto regReadback = RD53Interface::ReadRD53Reg(pChip, pRegNode);
                auto row         = RD53Interface::ReadChipReg(pChip, "REGION_ROW");
                if(regReadback.size() == 0 /* @TMP@ */ || regReadback[0].first != row || regReadback[0].second != data)
                {
                    LOG(ERROR) << BOLDRED << "Error while writing into RD53 reg. " << BOLDYELLOW << pRegNode << RESET;
                    return false;
                }
            }
        }
        else if(data != RD53Interface::ReadChipReg(pChip, pRegNode))
            return false;
    }

    pChip->setReg(pRegNode, data);
    return true;
}

void RD53Interface::WriteBoardBroadcastChipReg(const BeBoard* pBoard, const std::string& pRegNode, const uint16_t data)
{
    this->setBoard(pBoard->getId());

    uint16_t address = static_cast<RD53*>(pBoard->at(0)->at(0)->at(0))->getRegItem(pRegNode).fAddress;
    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(RD53Cmd::WrReg(RD53Constants::BROADCAST_CHIPID, address, data).getFrames(), -1);
}

uint16_t RD53Interface::ReadChipReg(Chip* pChip, const std::string& pRegNode) // @TMP@
{
    this->setBoard(pChip->getBeBoardId());

    // auto regReadback = RD53Interface::ReadRD53Reg(static_cast<RD53*>(pChip), pRegNode);
    // return regReadback[0].second;

    const int nAttempts = 2;

    for(auto attempt = 0; attempt < nAttempts; attempt++)
    {
        auto regReadback = RD53Interface::ReadRD53Reg(static_cast<RD53*>(pChip), pRegNode);
        if(regReadback.size() == 0)
        {
            LOG(WARNING) << BLUE << "Empty register readback, attempt n. " << YELLOW << attempt << RESET;
            usleep(VCALSLEEP);
        }
        else
            return regReadback[0].second;
    }

    LOG(ERROR) << BOLDRED << "Empty register readback FIFO in " << BOLDYELLOW << nAttempts << BOLDRED " attempts" << RESET;
    return 0;
}

std::vector<std::pair<uint16_t, uint16_t>> RD53Interface::ReadRD53Reg(Chip* pChip, const std::string& pRegNode)
{
    this->setBoard(pChip->getBeBoardId());

    RD53Interface::sendCommand(pChip, RD53Cmd::RdReg(pChip->getChipId(), pChip->getRegItem(pRegNode).fAddress));
    auto regReadback = static_cast<RD53FWInterface*>(fBoardFW)->ReadChipRegisters(pChip);

    for(auto i = 0u; i < regReadback.size(); i++)
        // Removing bit related to PIX_PORTAL register identification
        regReadback[i].first = regReadback[i].first & static_cast<uint16_t>(RD53Shared::setBits(RD53Constants::NBIT_ADDR));

    return regReadback;
}

// @TMP@
uint16_t getPixelConfig(const std::vector<perColumnPixelData>& mask, uint16_t row, uint16_t col, bool highGain)
// #################################################################################################################################################
// # Encodes the configuration for a pixel pair # # In the LIN FE tdac is unsigned and increasing it reduces the local
// threshold                                                                  # # In the DIFF FE tdac is signed and
// increasing it reduces the local threshold                                                                   # # To
// prevent having to deal with that in the rest of the code, we map the tdac range of the DIFF FE like so: # # -15 ->
// 30, -14 -> 29, ... 0 -> 15, ... 15 -> 0 # # So for the rest of the code the tdac range of the DIFF FE is [0, 30] and
// the only difference with the LIN FE is the number of possible values #
// #################################################################################################################################################
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

void RD53Interface::WriteRD53Mask(RD53* pRD53, bool doSparse, bool doDefault, bool pVerifLoop)
{
    this->setBoard(pRD53->getBeBoardId());

    std::vector<uint16_t> commandList;

    const uint16_t REGION_COL_ADDR = pRD53->getRegItem("REGION_COL").fAddress;
    const uint16_t REGION_ROW_ADDR = pRD53->getRegItem("REGION_ROW").fAddress;
    const uint16_t PIX_PORTAL_ADDR = pRD53->getRegItem("PIX_PORTAL").fAddress;
    const uint8_t  highGain        = pRD53->getRegItem("HighGain_LIN").fValue;
    const uint8_t  chipID          = pRD53->getChipId();

    std::vector<perColumnPixelData>& mask = doDefault ? *pRD53->getPixelsMaskDefault() : *pRD53->getPixelsMask();

    // ##########################
    // # Disable default config #
    // ##########################
    RD53Interface::WriteChipReg(pRD53, "PIX_DEFAULT_CONFIG", 0x0, pVerifLoop);

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
        RD53Interface::WriteChipReg(pRD53, "PIX_MODE", 0x27, pVerifLoop);
        RD53Interface::WriteChipReg(pRD53, "PIX_PORTAL", 0x00, pVerifLoop);
        RD53Interface::WriteChipReg(pRD53, "PIX_MODE", 0x00, pVerifLoop);

        uint16_t data;

        for(auto col = 0u; col < RD53::nCols; col += 2)
        {
            if(std::find(mask[col].Enable.begin(), mask[col].Enable.end(), 1) == mask[col].Enable.end()) continue;

            RD53Cmd::WrReg(chipID, REGION_COL_ADDR, col / 2).appendTo(commandList);

            for(auto row = 0u; row < RD53::nRows; row++)
            {
                if((mask[col].Enable[row] == 1) || (mask[col + 1].Enable[row] == 1))
                {
                    data = getPixelConfig(mask, row, col, highGain);

                    RD53Cmd::WrReg(chipID, REGION_ROW_ADDR, row).appendTo(commandList);
                    RD53Cmd::WrReg(chipID, PIX_PORTAL_ADDR, data).appendTo(commandList);
                }

                if(commandList.size() > RD53Constants::FIELDS_SHORTCMD * NPIXCMD)
                {
                    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pRD53->getFeId()); // @TMP@
                    commandList.clear();
                }
            }
        }
    }
    else
    {
        RD53Interface::WriteChipReg(pRD53, "PIX_MODE", 0x8, false);

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

                if((commandList.size() > RD53Constants::FIELDS_LONGCMD * NPIXCMD) || (row == (RD53::nRows - 1)))
                {
                    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pRD53->getFeId()); // @TMP@
                    commandList.clear();
                }
            }
        }
    }

    if(commandList.size() != 0) static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pRD53->getFeId()); // @TMP@
}

bool RD53Interface::ConfigureChipOriginalMask(ReadoutChip* pChip, bool pVerifLoop, uint32_t pBlockSize)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    RD53Interface::WriteRD53Mask(pRD53, false, true, pVerifLoop);

    return true;
}

bool RD53Interface::MaskAllChannels(ReadoutChip* pChip, bool mask, bool pVerifLoop)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    if(mask == true)
        pRD53->disableAllPixels();
    else
        pRD53->enableAllPixels();

    RD53Interface::WriteRD53Mask(pRD53, false, false, pVerifLoop);

    return true;
}

bool RD53Interface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const ChannelGroupBase* group, bool mask, bool inject, bool pVerifLoop)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    for(auto row = 0u; row < RD53::nRows; row++)
        for(auto col = 0u; col < RD53::nCols; col++)
        {
            if(mask == true) pRD53->enablePixel(row, col, group->isChannelEnabled(row, col) && (*pRD53->getPixelsMaskDefault())[col].Enable[row]);
            if(inject == true) pRD53->injectPixel(row, col, group->isChannelEnabled(row, col));
        }

    RD53Interface::WriteRD53Mask(pRD53, true, false, pVerifLoop);

    return true;
}

bool RD53Interface::WriteChipAllLocalReg(ReadoutChip* pChip, const std::string& regName, ChipContainer& pValue, bool pVerifLoop)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    for(auto row = 0u; row < RD53::nRows; row++)
        for(auto col = 0u; col < RD53::nCols; col++) pRD53->setTDAC(row, col, pValue.getChannel<uint16_t>(row, col));

    RD53Interface::WriteRD53Mask(pRD53, false, false, pVerifLoop);

    return true;
}

void RD53Interface::ReadChipAllLocalReg(ReadoutChip* pChip, const std::string& regName, ChipContainer& pValue)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    for(auto row = 0u; row < RD53::nRows; row++)
        for(auto col = 0u; col < RD53::nCols; col++) pValue.getChannel<uint16_t>(row, col) = pRD53->getTDAC(row, col);
}

// ###########################
// # Dedicated to minitoring #
// ###########################

float RD53Interface::ReadChipMonitor(Chip* pChip, const char* observableName)
{
    this->setBoard(pChip->getBeBoardId());

    const float measError = 4.0; // Current or Voltage measurement error due to MONITOR_CONFIG resolution [%]
    float       value;
    bool        isCurrentNotVoltage;
    uint32_t    voltageObservable(0), currentObservable(0), observable;

    const std::unordered_map<std::string, uint32_t> currentMultiplexer = {
        {"Iref", 0x00},          {"IBIASP1_SYNC", 0x01}, {"IBIASP2_SYNC", 0x02},  {"IBIAS_DISC_SYNC", 0x03}, {"IBIAS_SF_SYNC", 0x04},  {"ICTRL_SYNCT_SYNC", 0x05}, {"IBIAS_KRUM_SYNC", 0x06},
        {"COMP_LIN", 0x07},      {"FC_BIAS_LIN", 0x08},  {"KRUM_CURR_LIN", 0x09}, {"LDAC_LIN", 0x0A},        {"PA_IN_BIAS_LIN", 0x0B}, {"COMP_DIFF", 0x0C},        {"PRECOMP_DIFF", 0x0D},
        {"FOL_DIFF", 0x0E},      {"PRMP_DIFF", 0x0F},    {"LCC_DIFF", 0x10},      {"VFF_DIFF", 0x11},        {"VTH1_DIFF", 0x12},      {"VTH2_DIFF", 0x13},        {"CDR_CP_IBIAS", 0x14},
        {"VCO_BUFF_BIAS", 0x15}, {"VCO_IBIAS", 0x16},    {"CML_TAP_BIAS0", 0x17}, {"CML_TAP_BIAS1", 0x18},   {"CML_TAP_BIAS2", 0x19}};

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
        isCurrentNotVoltage = false;
    }
    else
    {
        currentObservable   = search->second;
        voltageObservable   = voltageMultiplexer.find("IMUXoutput")->second;
        isCurrentNotVoltage = true;
    }

    observable = bits::pack<1, 6, 7>(true, currentObservable, voltageObservable);
    if(std::string(observableName).find("TEMPSENS") != std::string::npos)
    {
        value = RD53Interface::measureTemperature(pChip, observable);
        LOG(INFO) << BOLDBLUE << "\t--> " << observableName << ": " << BOLDYELLOW << std::setprecision(3) << value << " +/- " << std::setprecision(1) << value * measError / 100 << BOLDBLUE << " C"
                  << std::setprecision(-1) << RESET;
    }
    else
    {
        value = measureVoltageCurrent(pChip, observable, isCurrentNotVoltage);
        LOG(INFO) << BOLDBLUE << "\t--> " << observableName << ": " << BOLDYELLOW << std::setprecision(3) << value << " +/- " << std::setprecision(1) << value * measError / 100 << BOLDBLUE
                  << (isCurrentNotVoltage == true ? " A" : " V") << std::setprecision(-1) << RESET;
    }

    return value;
}

uint32_t RD53Interface::measureADC(Chip* pChip, uint32_t data)
{
    const uint16_t GLOBAL_PULSE_ROUTE = pChip->getRegItem("GLOBAL_PULSE_ROUTE").fAddress;
    const uint8_t  chipID             = pChip->getChipId();
    const uint16_t trimADC            = bits::pack<1, 5, 6>(true, pChip->getRegItem("BG_MONITOR_CONFIG").fValue, pChip->getRegItem("ADC_MONITOR_CONFIG").fValue);
    // [10:6] band-gap trim [5:0] ADC trim. According to wafer probing they should give an average VrefADC of 0.9 V
    const uint16_t GlbPulseVal = RD53Interface::ReadChipReg(pChip, "GLOBAL_PULSE_ROUTE");

    std::vector<uint16_t> commandList;

    RD53Cmd::WrReg(chipID, pChip->getRegItem("MONITOR_CONFIG").fAddress, trimADC).appendTo(commandList);
    RD53Cmd::WrReg(chipID, GLOBAL_PULSE_ROUTE, 0x0040).appendTo(commandList); // Reset Monitor Data
    RD53Cmd::GlobalPulse(pChip->getChipId(), 0x0004).appendTo(commandList);
    RD53Cmd::WrReg(chipID, GLOBAL_PULSE_ROUTE, 0x0008).appendTo(commandList); // Clear Monitor Data
    RD53Cmd::GlobalPulse(pChip->getChipId(), 0x0004).appendTo(commandList);
    RD53Cmd::WrReg(chipID, pChip->getRegItem("MONITOR_SELECT").fAddress, data).appendTo(commandList); // 14 bits: bit 13 enable, bits 7:12 I-Mon, bits 0:6 V-Mon
    RD53Cmd::WrReg(chipID, GLOBAL_PULSE_ROUTE, 0x1000).appendTo(commandList);                         // Trigger Monitor Data to start conversion
    RD53Cmd::GlobalPulse(pChip->getChipId(), 0x0004).appendTo(commandList);
    RD53Cmd::WrReg(chipID, GLOBAL_PULSE_ROUTE, GlbPulseVal).appendTo(commandList); // Restore value in Global Pulse Route

    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(commandList, pChip->getFeId());

    return RD53Interface::ReadChipReg(pChip, "MONITORING_DATA_ADC");
}

float RD53Interface::measureVoltageCurrent(Chip* pChip, uint32_t data, bool isCurrentNotVoltage)
{
    const float safetyMargin = 0.9;

    auto ADC = RD53Interface::measureADC(pChip, data);
    if(ADC > (RD53Shared::setBits(pChip->getNumberOfBits("MONITORING_DATA_ADC")) + 1.) * safetyMargin)
        LOG(WARNING) << BOLDRED << "\t--> ADC measurement in saturation (ADC = " << BOLDYELLOW << ADC << BOLDRED
                     << "): likely the R-IMUX resistor, that converts the current into a voltage, is not connected" << RESET;

    return RD53Interface::convertADC2VorI(pChip, ADC, isCurrentNotVoltage);
}

float RD53Interface::measureTemperature(Chip* pChip, uint32_t data)
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

    uint16_t sensorConfigData; // Enable[5], DEM[4:1], SEL_BIAS[0] (x2 ... 10 bit in total for the sensors in each
                               // sensor config register)

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

float RD53Interface::convertADC2VorI(Chip* pChip, uint32_t value, bool isCurrentNotVoltage)
{
    // #####################################################################
    // # ADCoffset     =  63 [1/10mV] Offset due to ground shift           #
    // # actualVrefADC = 839 [mV]     Lower than VrefADC due to parasitics #
    // #####################################################################
    const float resistorI2V   = 10000; // [Ohm]
    const float ADCoffset     = pChip->getRegItem("ADC_OFFSET_VOLT").fValue / 1e4;
    const float actualVrefADC = pChip->getRegItem("ADC_MAXIMUM_VOLT").fValue / 1e3;

    const float ADCslope = (actualVrefADC - ADCoffset) / (RD53Shared::setBits(pChip->getNumberOfBits("MONITORING_DATA_ADC")) + 1); // [V/ADC]
    const float voltage  = ADCoffset + ADCslope * value;

    return voltage / (isCurrentNotVoltage == true ? resistorI2V : 1);
}

float RD53Interface::ReadHybridTemperature(Chip* pChip)
{
    auto hybridId = static_cast<RD53*>(pChip)->getFeId(); // @TMP@
    return static_cast<RD53FWInterface*>(fBoardFW)->ReadHybridTemperature(hybridId);
}

float RD53Interface::ReadHybridVoltage(Chip* pChip)
{
    auto hybridId = static_cast<RD53*>(pChip)->getFeId(); // @TMP@
    return static_cast<RD53FWInterface*>(fBoardFW)->ReadHybridVoltage(hybridId);
}
} // namespace Ph2_HwInterface
