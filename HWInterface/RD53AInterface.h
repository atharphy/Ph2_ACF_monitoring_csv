/*!
  \file                  RD53AInterface.h
  \brief                 User interface to the RD53A readout chip
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53AInterface_H
#define RD53AInterface_H

#include "../HWDescription/RD53A.h"
#include "../HWDescription/RD53ACommands.h"
#include "RD53Interface.h"

namespace Ph2_HwInterface
{
class RD53AInterface : public RD53Interface
{
  public:
    using RD53Interface::RD53Interface;

    bool     ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    void     Reset(Ph2_HwDescription::ReadoutChip* pChip, const size_t resetType, const size_t duration = 0x4) override;
    void     ChipErrorReport(Ph2_HwDescription::ReadoutChip* pChip) override;
    void     InitRD53Downlink(const Ph2_HwDescription::BeBoard* pBoard) override;
    void     InitRD53Uplinks(Ph2_HwDescription::ReadoutChip* pChip, int nActiveLanes = 1) override;
    void     PackWriteCommand(Ph2_HwDescription::Chip* pChip, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg = true) override;
    void     PackWriteBroadcastCommand(const Ph2_HwDescription::BeBoard* pBoard, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg = true) override;
    void     WriteClokDataDelay(Ph2_HwDescription::Chip* pChip, uint16_t value) override;
    uint32_t ReadChipFuseID(Ph2_HwDescription::Chip* pChip) override { return pChip->getId(); }

  private:
    void                                       WriteRD53Mask(Ph2_HwDescription::RD53* pRD53, bool doSparse, bool doDefault) override;
    std::vector<std::pair<uint16_t, uint16_t>> ReadRD53Reg(Ph2_HwDescription::ReadoutChip* pChip, const std::string& regName);
    std::pair<std::string, uint16_t>           SetSpecialRegister(std::string regName, uint16_t value, Ph2_HwDescription::ChipRegMap& pRD53RegMap) override;
    uint16_t                                   GetSpecialRegisterValue(std::string regName, uint16_t value, Ph2_HwDescription::ChipRegMap& pRD53RegMap) override;

    uint16_t GetPixelConfig(const Ph2_HwDescription::pixelMask& mask, uint16_t row, uint16_t col, bool highGain);

    const std::map<std::string, RD53Interface::SpecialRegInfo> specialRegMap = {{"CDR_CONFIG_SEL_SER_CLK", {"CDR_CONFIG", 0}},

                                                                                {"CLK_DATA_DELAY_DATA", {"CLK_DATA_DELAY", 0}},
                                                                                {"CLK_DATA_DELAY_CLK", {"CLK_DATA_DELAY", 4}},
                                                                                {"CLK_DATA_DELAY_2INV", {"CLK_DATA_DELAY", 5}},

                                                                                {"MONITOR_CONFIG_ADC", {"MONITOR_CONFIG", 0}},
                                                                                {"MONITOR_CONFIG_BG", {"MONITOR_CONFIG", 6}},

                                                                                {"VOLTAGE_TRIM_DIG", {"VOLTAGE_TRIM", 0}},
                                                                                {"VOLTAGE_TRIM_ANA", {"VOLTAGE_TRIM", 5}},

                                                                                {"CML_CONFIG_EN_LANE", {"CML_CONFIG", 0}},
                                                                                {"CML_CONFIG_SER_EN_TAP", {"CML_CONFIG", 4}},
                                                                                {"CML_CONFIG_SER_INV_TAP", {"CML_CONFIG", 6}},

                                                                                {"SER_SEL_OUT_0", {"SER_SEL_OUT", 0}},
                                                                                {"SER_SEL_OUT_1", {"SER_SEL_OUT", 2}},
                                                                                {"SER_SEL_OUT_2", {"SER_SEL_OUT", 4}},
                                                                                {"SER_SEL_OUT_3", {"SER_SEL_OUT", 6}},

                                                                                {"CAL_EDGE_FINE_DELAY", {"INJECTION_SELECT", 0}},
                                                                                {"DIGITAL_INJ_EN", {"INJECTION_SELECT", 4}},
                                                                                {"ANALOG_INJ_MODE", {"INJECTION_SELECT", 5}}};
};

} // namespace Ph2_HwInterface

#endif
