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

    if((regName == "VCAL_HIGH") || (regName == "VCAL_MED"))
        std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::firstChip->getFEtype(RD53Shared::firstChip->getNCols() / 2, RD53Shared::firstChip->getNCols() / 2)->VCalSleepTime));

    bool     status      = true;
    uint16_t actualValue = 0;
    if(pVerifLoop == true)
    {
        if(regName == "PIX_PORTAL")
        {
            auto pixMode = RD53Interface::ReadChipReg(pChip, "PIX_MODE");
            if((pixMode & RD53Shared::firstChip->getFEtype(RD53Shared::firstChip->getNCols() / 2, RD53Shared::firstChip->getNCols() / 2)->AutoIncrementMask) == 0) // Check only auto-increment bits
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

    if((regName == "VCAL_HIGH") || (regName == "VCAL_MED"))
        std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::firstChip->getFEtype(RD53Shared::firstChip->getNCols() / 2, RD53Shared::firstChip->getNCols() / 2)->VCalSleepTime));
}

uint16_t RD53Interface::ReadChipReg(Chip* pChip, const std::string& regName)
{
    this->setBoard(pChip->getBeBoardId());

    const int nAttempts = 20; // @CONST@
    for(auto attempt = 0; attempt < nAttempts; attempt++)
    {
        auto regReadback = ReadRD53Reg(static_cast<RD53*>(pChip), regName);
        if(regReadback.size() == 0)
        {
            LOG(WARNING) << BLUE << "Empty register readback, attempt n. " << YELLOW << attempt + 1 << BLUE << "/" << YELLOW << nAttempts << RESET;
            static_cast<RD53FWInterface*>(fBoardFW)->ResetReadBkFIFO(); // @TMP@ : temporary fix
            std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
        }
        else
            return regReadback[0].second;
    }

    LOG(ERROR) << BOLDRED << "Empty register (" << BOLDYELLOW << regName << BOLDRED << ") readback FIFO after " << BOLDYELLOW << nAttempts << BOLDRED " attempts" << RESET;

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

    for(auto& cRegItem: RD53Shared::firstChip->getRegMap())
    {
        auto value = RD53Interface::ReadChipReg(pChip, cRegItem.first);
        std::cout << "\t--> Register " << std::left << std::setfill(' ') << std::setw(24) << cRegItem.first << " = " << std::setw(8) << std::dec << value << std::hex << "(0x" << value << ")"
                  << std::endl;
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

void RD53Interface::StartPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip) { RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_PRBS, false); }
void RD53Interface::StopPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip) { RD53Interface::WriteChipReg(pChip, "SER_SEL_OUT", RD53Constants::PATTERN_AURORA, false); }

bool RD53Interface::WriteChipAllLocalReg(ReadoutChip* pChip, const std::string& regName, const ChipContainer& pValue, bool pVerifLoop)
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

float RD53Interface::ReadChipMonitor(ReadoutChip* pChip, const std::string& observableName)
{
    this->setBoard(pChip->getBeBoardId());

    const float measError = 4.0; // Current or Voltage measurement error due to MonitorConfig resolution [%]
    float       value;
    bool        isCurrentNotVoltage;
    uint32_t    observable = getADCobservable(observableName, isCurrentNotVoltage);

    if((observableName.find("TEMPSENS") != std::string::npos) || (observableName.find("RADSENS") != std::string::npos) || (observableName.find("INTERNAL_NTC") != std::string::npos))
    {
        std::string type = "CENTER";
        if(observableName.find("POLY") != std::string::npos)
            type = "POLY";
        else if(observableName.find("ANA") != std::string::npos)
            type = "ANA";
        else if(observableName.find("DIG") != std::string::npos)
            type = "DIG";
        else if(observableName.find("INTERNAL_NTC") != std::string::npos)
            type = "INT_NTC";
        else
            type = "CENTER";

        value = measureTemperature(pChip, observable, type);
        LOG(INFO) << BOLDBLUE << "\t--> " << BOLDYELLOW << observableName << BOLDBLUE << ": " << BOLDYELLOW << std::setprecision(3) << value << " +/- " << value * measError / 100 << BOLDBLUE << " C"
                  << std::setprecision(-1) << RESET;
    }
    else
    {
        value = RD53Interface::measureVoltageCurrent(pChip, observable, isCurrentNotVoltage);
        LOG(INFO) << BOLDBLUE << "\t--> " << BOLDYELLOW << observableName << BOLDBLUE << ": " << BOLDYELLOW << std::setprecision(3) << value << " +/- " << value * measError / 100 << BOLDBLUE
                  << (isCurrentNotVoltage == true ? " uA" : " V") << std::setprecision(-1) << RESET;
    }

    return value;
}

uint32_t RD53Interface::ReadChipADC(Ph2_HwDescription::ReadoutChip* pChip, const std::string& observableName)
{
    bool isCurrentNotVoltage;
    return measureADC(pChip, getADCobservable(observableName, isCurrentNotVoltage));
}

float RD53Interface::convertADC2VorI(ReadoutChip* pChip, uint32_t value, bool isCurrentNotVoltage)
// ######################################
// # Voltage output units: Volt         #
// # Current output units: micro-Ampere #
// ######################################
{
    // ######################################################################
    // # ADCoffset     =  63 [1/10 mV] Offset due to ground shift           #
    // # actualVrefADC = 839 [mV]      Lower than VrefADC due to parasitics #
    // ######################################################################

    const float resistorI2V   = 0.01; // [MOhm]
    const float ADCoffset     = pChip->getRegItem("ADC_OFFSET_VOLT").fValue / 1e4;
    const float actualVrefADC = pChip->getRegItem("ADC_MAXIMUM_VOLT").fValue / 1e3;

    const float ADCslope = (actualVrefADC - ADCoffset) / (RD53Shared::setBits(pChip->getNumberOfBits("MonitoringDataADC")) + 1); // [V/ADC]
    const float voltage  = ADCoffset + ADCslope * value;

    return voltage / (isCurrentNotVoltage == true ? resistorI2V : 1);
}

float RD53Interface::measureVoltageCurrent(ReadoutChip* pChip, uint32_t data, bool isCurrentNotVoltage)
{
    const float safetyMargin = 0.9; // @CONST@

    auto ADC = measureADC(pChip, data);
    if(ADC > (RD53Shared::setBits(pChip->getNumberOfBits("MonitoringDataADC")) + 1.) * safetyMargin)
        LOG(WARNING) << BOLDRED << "\t\t--> ADC measurement in saturation (ADC = " << BOLDYELLOW << ADC << BOLDRED
                     << "): likely the IMUX resistor, that converts the current into a voltage, is not connected" << RESET;

    return RD53Interface::convertADC2VorI(pChip, ADC, isCurrentNotVoltage);
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
