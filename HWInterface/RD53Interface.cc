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

bool RD53Interface::WriteChipReg(Chip* pChip, const std::string& regName, const uint16_t data, bool pVerifLoop)
{
    this->setBoard(pChip->getBeBoardId());

    auto                  nameAndValue(SetSpecialRegister(regName, data, RD53Shared::firstChip->getRegMap()));
    std::vector<uint16_t> cmdStream;
    PackWriteCommand(pChip, nameAndValue.first, nameAndValue.second, cmdStream, pVerifLoop);
    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(cmdStream, pChip->getHybridId());

    if((regName == "VCAL_HIGH") || (regName == "VCAL_MED")) std::this_thread::sleep_for(std::chrono::microseconds(VCALSLEEP));

    bool     status      = true;
    uint16_t actualValue = 0;
    if(pVerifLoop == true)
    {
        if(regName == "PIX_PORTAL")
        {
            auto pixMode = RD53Interface::ReadChipReg(pChip, "PIX_MODE");
            if((pChip->getFrontEndType() == FrontEndType::RD53A ? pixMode & RD53AConstants::AUTO_INCREMENT_MASK : pixMode & RD53BConstants::AUTO_INCREMENT_MASK) == 0) // Check only auto-increment bits
            {
                auto regReadback = ReadRD53Reg(static_cast<RD53*>(pChip), regName);
                actualValue      = regReadback[0].second;
                auto row         = RD53Interface::ReadChipReg(pChip, "REGION_ROW");
                if(regReadback.size() == 0 /* @TMP@ */ || regReadback[0].first != row || regReadback[0].second != data) status = false;
            }
        }
        else
        {
            actualValue = RD53Interface::ReadChipReg(pChip, nameAndValue.first);
            if(nameAndValue.second != actualValue) status = false;
        }
    }

    if(status == false)
    {
        LOG(ERROR) << BOLDRED << "Error when reading back what was written into RD53 reg. " << BOLDYELLOW << regName << BOLDRED << ": wrote = " << BOLDYELLOW << nameAndValue.second << BOLDRED
                   << ", read = " << BOLDYELLOW << actualValue << RESET;
    }
    else if((pVerifLoop == true) && (status == true))
    {
        // LOG(INFO) << BOLDBLUE << "\t--> Succesfully configured chip register " << BOLDYELLOW << regName << RESET; // @TMP@
    }

    // #######################################
    // # Update both real and fake registers #
    // #######################################
    pChip->getRegItem(regName).fValue            = data;
    pChip->getRegItem(nameAndValue.first).fValue = nameAndValue.second;

    return status;
}

void RD53Interface::WriteBoardBroadcastChipReg(const BeBoard* pBoard, const std::string& regName, const uint16_t data)
{
    this->setBoard(pBoard->getId());

    auto                  nameAndValue(SetSpecialRegister(regName, data, RD53Shared::firstChip->getRegMap()));
    std::vector<uint16_t> cmdStream;
    PackWriteBroadcastCommand(pBoard, nameAndValue.first, nameAndValue.second, cmdStream);
    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(cmdStream, -1);

    if((regName == "VCAL_HIGH") || (regName == "VCAL_MED")) std::this_thread::sleep_for(std::chrono::microseconds(VCALSLEEP));
}

uint16_t RD53Interface::ReadChipReg(Chip* pChip, const std::string& regName)
{
    this->setBoard(pChip->getBeBoardId());

    const int nAttempts = 2; // @CONST@
    for(auto attempt = 0; attempt < nAttempts; attempt++)
    {
        auto regReadback = ReadRD53Reg(static_cast<RD53*>(pChip), regName);
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

bool RD53Interface::ConfigureChipOriginalMask(ReadoutChip* pChip, bool pVerifLoop, uint32_t pBlockSize)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    WriteRD53Mask(pRD53, false, true);

    return true;
}

bool RD53Interface::MaskAllChannels(ReadoutChip* pChip, bool mask, bool pVerifLoop)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    if(mask == true)
        pRD53->disableAllPixels();
    else
        pRD53->enableAllPixels();

    WriteRD53Mask(pRD53, false, false);

    return true;
}

bool RD53Interface::maskChannelsAndSetInjectionSchema(ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop)
{
    RD53* pRD53          = static_cast<RD53*>(pChip);
    auto& pixMaskDefault = pRD53->getPixelsMaskDefault();
    auto& pixMask        = pRD53->getPixelsMask();

    if(mask == true)
        std::transform(pixMaskDefault.Enable.begin(), pixMaskDefault.Enable.end(), static_cast<const RD53ChannelGroup*>(group.get())->getMask().begin(), pixMask.Enable.begin(), std::logical_and<>{});
    std::transform(pixMaskDefault.Enable.begin(), pixMaskDefault.Enable.end(), static_cast<const RD53ChannelGroup*>(group.get())->getMask().begin(), pixMask.InjEn.begin(), std::logical_and<>{});
    if(inject == false) std::transform(pixMask.InjEn.begin(), pixMask.InjEn.end(), pixMaskDefault.InjEn.begin(), pixMask.InjEn.begin(), std::logical_and<>{});

    WriteRD53Mask(pRD53, true, false);

    return true;
}

void RD53Interface::DumpChipRegisters(ReadoutChip* pChip)
{
    this->setBoard(pChip->getBeBoardId());

    LOG(INFO) << GREEN << "Chip " << BOLDYELLOW << pChip->getId() << RESET << GREEN << " register content:" << RESET;
    for(auto& cRegItem: RD53Shared::firstChip->getRegMap())
    {
        auto value = RD53Interface::ReadChipReg(pChip, cRegItem.first);
        std::cout << "\t--> Register " << std::left << std::setfill(' ') << std::setw(24) << cRegItem.first << " = " << std::dec << value << std::hex << " (0x" << value << ")" << std::endl;
    }
}

void RD53Interface::ChipErrorReport(ReadoutChip* pChip)
{
    this->setBoard(pChip->getBeBoardId());

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

uint16_t RD53Interface::SetFieldValue(uint16_t regValue, uint16_t fieldValue, uint8_t start, uint8_t size)
{
    uint16_t mask = ((1 << size) - 1) << start;
    return regValue ^ ((regValue ^ (fieldValue << start)) & mask);
}

uint16_t RD53Interface::GetFieldValue(uint16_t regValue, uint8_t start, uint8_t size)
{
    uint16_t mask = (1 << size) - 1;
    return (regValue >> start) & mask;
}

// ##################
// # PRBS generator #
// ##################

void RD53Interface::StartPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip)
{
    auto regValue = RD53Constants::PATTERN_PRBS;
    if((pChip->getRegItem("SER_SEL_OUT").fPrmptCfg == true) || (pChip->getRegItem("SER_SEL_OUT_0").fPrmptCfg == true) || (pChip->getRegItem("SER_SEL_OUT_1").fPrmptCfg == true) ||
       (pChip->getRegItem("SER_SEL_OUT_2").fPrmptCfg == true) || (pChip->getRegItem("SER_SEL_OUT_3").fPrmptCfg == true))
        regValue = RD53Constants::PATTERN_PRBS | (pChip->getRegItem("SER_SEL_OUT").fValue & 0xFC); // @TMP@
    RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", regValue, false);
}
void RD53Interface::StopPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip) { RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_AURORA, false); }

bool RD53Interface::WriteChipAllLocalReg(ReadoutChip* pChip, const std::string& regName, ChipContainer& pValue, bool pVerifLoop)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);

    for(auto col = 0u; col < pRD53->getNCols(); col++)
        for(auto row = 0u; row < pRD53->getNRows(); row++) pRD53->setTDAC(row, col, pValue.getChannel<uint16_t>(row, col));

    WriteRD53Mask(pRD53, false, false);

    return true;
}

void RD53Interface::ReadChipAllLocalReg(ReadoutChip* pChip, const std::string& regName, ChipContainer& pValue)
{
    RD53* pRD53 = static_cast<RD53*>(pChip);
    for(auto col = 0u; col < pRD53->getNCols(); col++)
        for(auto row = 0u; row < pRD53->getNRows(); row++) pValue.getChannel<uint16_t>(row, col) = static_cast<RD53*>(pChip)->getTDAC(row, col);
}

void RD53Interface::SendChipCommands(const BeBoard* pBoard, const std::vector<uint16_t>& chipCommandList, int hybridId)
{
    this->setBoard(pBoard->getId());
    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(chipCommandList, hybridId);
}

void RD53Interface::PackHybridCommands(const BeBoard* pBoard, const std::vector<uint16_t>& chipCommandList, int hybridId, std::vector<uint32_t>& hybridCommandList)
{
    this->setBoard(pBoard->getId());
    static_cast<RD53FWInterface*>(fBoardFW)->ComposeAndPackChipCommands(chipCommandList, hybridId, hybridCommandList);
}

void RD53Interface::SendHybridCommands(const BeBoard* pBoard, const std::vector<uint32_t>& hybridCommandList)
{
    this->setBoard(pBoard->getId());
    static_cast<RD53FWInterface*>(fBoardFW)->SendChipCommands(hybridCommandList);
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

    RD53ACmd::serialize(RD53ACmd::WrReg{chipID, pChip->getRegItem("MONITOR_CONFIG").fAddress, trimADC}, commandList);
    RD53ACmd::serialize(RD53ACmd::WrReg{chipID, GLOBAL_PULSE_ROUTE, 0x0040}, commandList); // Reset Monitor Data
    RD53ACmd::serialize(RD53ACmd::GlobalPulse{pChip->getId(), 0x0004}, commandList);
    RD53ACmd::serialize(RD53ACmd::WrReg{chipID, GLOBAL_PULSE_ROUTE, 0x0008}, commandList); // Clear Monitor Data
    RD53ACmd::serialize(RD53ACmd::GlobalPulse{pChip->getId(), 0x0004}, commandList);
    RD53ACmd::serialize(RD53ACmd::WrReg{chipID, pChip->getRegItem("MONITOR_SELECT").fAddress, data}, commandList); // 14 bits: bit 13 enable, bits 7:12 I-Mon, bits 0:6 V-Mon
    RD53ACmd::serialize(RD53ACmd::WrReg{chipID, GLOBAL_PULSE_ROUTE, 0x1000}, commandList);                         // Trigger Monitor Data to start conversion
    RD53ACmd::serialize(RD53ACmd::GlobalPulse{pChip->getId(), 0x0004}, commandList);
    RD53ACmd::serialize(RD53ACmd::WrReg{chipID, GLOBAL_PULSE_ROUTE, GlbPulseVal}, commandList); // Restore value in Global Pulse Route

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
