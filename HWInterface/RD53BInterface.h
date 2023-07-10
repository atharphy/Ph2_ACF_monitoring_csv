/*!
  \file                  RD53BInterface.h
  \brief                 User interface to the RD53B readout chip
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53BInterface_H
#define RD53BInterface_H

#include "HWDescription/RD53B.h"
#include "HWDescription/RD53BCommands.h"
#include "HWInterface/RD53Interface.h"

namespace Ph2_HwInterface
{
class RD53BInterface : public RD53Interface
{
  public:
    using RD53Interface::RD53Interface;

    bool     ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerify = true, uint32_t pBlockSize = 310) override;
    void     Reset(Ph2_HwDescription::ReadoutChip* pChip, const size_t resetType, const size_t duration = 0x4) override;
    void     ChipErrorReport(Ph2_HwDescription::ReadoutChip* pChip) override;
    void     InitRD53Downlink(const Ph2_HwDescription::BeBoard* pBoard) override;
    void     InitRD53Uplinks(Ph2_HwDescription::ReadoutChip* pChip) override;
    void     TAP0slaveOptimization(const Ph2_HwDescription::BeBoard* pBoard, const Ph2_HwDescription::Hybrid* pHybrid) override;
    void     PackWriteCommand(Ph2_HwDescription::Chip* pChip, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg = true) override;
    void     PackWriteBroadcastCommand(const Ph2_HwDescription::BeBoard* pBoard, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg = true) override;
    void     WriteClockDataDelay(Ph2_HwDescription::Chip* pChip, uint16_t value) override;
    uint32_t ReadChipFuseID(Ph2_HwDescription::Chip* pChip) override;

  private:
    void                                       WriteRD53Mask(Ph2_HwDescription::RD53* pRD53, bool doSparse, bool doDefault) override;
    std::vector<std::pair<uint16_t, uint16_t>> ReadRD53Reg(Ph2_HwDescription::ReadoutChip* pChip, const std::string& regName);
    std::pair<std::string, uint16_t>           SetSpecialRegister(std::string regName, uint16_t value, Ph2_HwDescription::ChipRegMap& pRD53RegMap) override;
    uint16_t                                   GetSpecialRegisterValue(std::string regName, uint16_t value, Ph2_HwDescription::ChipRegMap& pRD53RegMap) override;

    uint16_t GetPixelConfig(const Ph2_HwDescription::pixelMask& mask, uint16_t row, uint16_t col);
    uint16_t GetPixelConfigMask(const Ph2_HwDescription::pixelMask& mask, uint16_t row, uint16_t col);
    uint16_t GetPixelConfigTDAC(const Ph2_HwDescription::pixelMask& mask, uint16_t row, uint16_t col);
    void     SendGlobalPulse(Ph2_HwDescription::Chip* pChip, uint16_t route, uint16_t pulseDuration);
    void     SendGlobalPulseBroadcast(const Ph2_HwDescription::BeBoard* pBoard, uint16_t route, uint16_t pulseDuration);
    void     SendChipCommandsWithSync(Ph2_HwDescription::RD53* pRD53, std::vector<uint16_t>& cmdStream);
    void     ResetCoreColumns(Ph2_HwDescription::RD53* pRD53);

    const std::map<std::string, RD53Interface::SpecialRegInfo> specialRegMap = {{"CDR_CONFIG_SEL_SER_CLK", {"CDR_CONFIG", 0}},
                                                                                {"CDR_CONFIG_SEL_PD", {"CDR_CONFIG", 3}},

                                                                                {"CLK_DATA_DELAY_DATA", {"CLK_DATA_DELAY", 0}},
                                                                                {"CLK_DATA_DELAY_CLK", {"CLK_DATA_DELAY", 6}},

                                                                                {"MON_ADC_TRIM", {"MON_ADC", 0}},

                                                                                {"VOLTAGE_TRIM_DIG", {"VOLTAGE_TRIM", 0}},
                                                                                {"VOLTAGE_TRIM_ANA", {"VOLTAGE_TRIM", 4}},

                                                                                {"CML_CONFIG_SER_EN_TAP", {"CML_CONFIG", 4}},
                                                                                {"CML_CONFIG_SER_INV_TAP", {"CML_CONFIG", 6}},

                                                                                {"SER_SEL_OUT_0", {"SER_SEL_OUT", 0}},
                                                                                {"SER_SEL_OUT_1", {"SER_SEL_OUT", 2}},
                                                                                {"SER_SEL_OUT_2", {"SER_SEL_OUT", 4}},
                                                                                {"SER_SEL_OUT_3", {"SER_SEL_OUT", 6}},

                                                                                {"CAL_EDGE_FINE_DELAY", {"CalibrationConfig", 0}},
                                                                                {"ANALOG_INJ_MODE", {"CalibrationConfig", 6}},
                                                                                {"DIGITAL_INJ_EN", {"CalibrationConfig", 7}},

                                                                                {"HIT_SAMPLE_MODE", {"PIX_MODE", 3}},
                                                                                {"EN_SEU_COUNT", {"PIX_MODE", 4}},

                                                                                {"SEL_CAL_RANGE", {"MEAS_CAP", 0}},
                                                                                {"EN_INJCAP_MEAS", {"MEAS_CAP", 1}},
                                                                                {"EN_INJCAP_PAR_MEAS", {"MEAS_CAP", 2}},

                                                                                {"ToT6to4Mapping", {"ToTConfig", 9}},
                                                                                {"ToTDualEdgeCount", {"ToTConfig", 10}},

                                                                                {"EnEoS", {"DataConcentratorConf", 8}},
                                                                                {"EnLv1Id", {"DataConcentratorConf", 9}},
                                                                                {"EnBCId", {"DataConcentratorConf", 10}},
                                                                                {"EnCRC", {"DataConcentratorConf", 11}},

                                                                                {"RawData", {"CoreColEncoderConf", 7}},
                                                                                {"BinaryReadOut", {"CoreColEncoderConf", 8}},

                                                                                {"EnOutputDataChipId", {"DataMerging", 8}},

                                                                                {"SelfTriggerMultiplier", {"SelfTriggerConfig_0", 0}},
                                                                                {"SelfTriggerDelay", {"SelfTriggerConfig_0", 5}},

                                                                                {"SelfTriggerEn", {"SelfTriggerConfig_1", 5}},

                                                                                {"ServiceFrameSkip", {"ServiceDataConf", 0}},
                                                                                {"EnServiceData", {"ServiceDataConf", 8}}};

    // ###########################
    // # Dedicated to monitoring #
    // ###########################
  private:
    uint32_t getADCobservable(const std::string& observableName, bool& isCurrentNotVoltage) override;
    uint32_t measureADC(Ph2_HwDescription::ReadoutChip* pChip, uint32_t data) override;
    float    measureTemperature(Ph2_HwDescription::ReadoutChip* pChip, uint32_t data, const std::string& type = "", int beta = 3435) override;
};

} // namespace Ph2_HwInterface

#endif
