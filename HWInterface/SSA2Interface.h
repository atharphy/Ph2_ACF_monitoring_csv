/*!
        \file                                            SSA2Interface.h
        \brief                                           User Interface to the SSA2s
        \author                                          Marc Osherson
        \version                                         1.0
        \date                        Jan 20201
        Support :                    mail to : oshersonmarc@gmail.com

 */

#ifndef __SSA2INTERFACE_H__
#define __SSA2INTERFACE_H__

#include "HWInterface/BeBoardFWInterface.h"
#include "HWInterface/ReadoutChipInterface.h"
#include <fstream>
#include <iostream> // std::cout
#include <string>
#include <vector>

namespace Ph2_HwInterface
{ // start namespace

class SSA2Interface : public ReadoutChipInterface
{ // begin class
  public:
    SSA2Interface(const BeBoardFWMap& pBoardMap);
    ~SSA2Interface();
    bool ConfigureChip(Ph2_HwDescription::Chip* pSSA2, bool pVerify = false, uint32_t pBlockSize = 310) override; // FIXME
    void DumpConfiguration(Ph2_HwDescription::Chip* pSSA2, std::string filename);                                 // FIXME

    void producePhaseAlignmentPattern(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pWait_ms = 10) override {}

    bool setInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerify = true) override;                                        // FIXME
    bool enableInjection(Ph2_HwDescription::ReadoutChip* pChip, bool inject, bool pVerify = true) override;                                                                             // FIXME
    bool setInjectionAmplitude(Ph2_HwDescription::ReadoutChip* pChip, uint8_t injectionAmplitude, bool pVerify = true) override;                                                        // FIXME
    bool maskChannelGroup(Ph2_HwDescription::ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerify = true) override;                                          // FIXME
    bool maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerify = true) override; // FIXME
    bool ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pSSA2, bool pVerify = true, uint32_t pBlockSize = 310) override;                                                     // FIXME
    bool MaskAllChannels(Ph2_HwDescription::ReadoutChip* pSSA2, bool mask, bool pVerify = true) override;                                                                               // FIXME
    bool WriteChipReg(Ph2_HwDescription::Chip* pSSA2, const std::string& pRegNode, uint16_t pValue, bool pVerify = true) override;                                                      // FIXME
    bool WriteChipMultReg(Ph2_HwDescription::Chip* pSSA2, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerify = true) override;                                  // FIXME
    bool WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pSSA2, const std::string& dacName, const ChipContainer& pValue, bool pVerify = true) override;                            // FIXME

    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pSSA2, const std::string& pRegNode) override;
    uint16_t ReadADC(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pInput);
    uint16_t ReadADC(Ph2_HwDescription::ReadoutChip* pChip, std::string pRegName);

    float    CalculateADCLSB(Ph2_HwDescription::Chip* pSSA2, float vrefExp = 0.850);
    uint16_t MeasureGND(Ph2_HwDescription::Chip* pSSA2);

  private:
    uint8_t ReadChipId(Ph2_HwDescription::Chip* pChip);                                                                                                                      // FIXME
    bool    WriteChipRegBits(Ph2_HwDescription::Chip* pSSA2, const std::string& pRegNode, uint16_t pValue, const std::string& pMaskReg, uint8_t mask, bool pVerify = false); // FIXME
    bool    ConfigureAmux(Ph2_HwDescription::Chip* pChip, const std::string& pRegister, bool pVerify = true);                                                                // FIXME

    const std::map<std::string, uint8_t> SSA2_ADC_CONTROL_TABLE = {
        {"highimpedence", 0}, {"Bias_D5BFEED", 1},   {"Bias_D5PREAMP", 2}, {"Bias_D5TDR", 3}, {"Bias_D5ALLV", 4}, {"Bias_D5ALLI", 5}, {"Bias_CALDAC", 6}, {"Bias_BOOSTERBASELINE", 7},
        {"Bias_THDAC", 8},    {"Bias_THDACHIGH", 9}, {"Bias_D5DAC8", 10},  {"VBG", 11},       {"GND", 12},        {"ADC_IREF", 13},   {"ADC_VREF", 14},   {"TESTPAD", 15},
        {"Temperature", 16},  {"AVDD", 17},          {"PVDD", 18},         {"DVDD", 19}};

    std::map<std::string, uint8_t> fAmuxMap = {{"BoosterFeedback", 0}, // FIXME
                                               {"PreampBias", 1},
                                               {"Trim", 2},
                                               {"VoltageBias", 3},
                                               {"CurrentBias", 4},
                                               {"CalLevel", 5},
                                               {"BoosterBaseline", 6},
                                               {"Threshold", 7},
                                               {"HipThreshold", 8},
                                               {"DAC", 9},
                                               {"Bandgap", 10},
                                               {"GND", 11},
                                               {"HighZ", 12}};

}; // end class

} // namespace Ph2_HwInterface

#endif
