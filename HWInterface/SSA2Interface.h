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

#include "BeBoardFWInterface.h"
#include "ReadoutChipInterface.h"
#include <vector>
#include <iostream>   // std::cout
#include <string>  
#include <fstream>

namespace Ph2_HwInterface
{ // start namespace

class SSA2Interface : public ReadoutChipInterface
{ // begin class
  public:
    SSA2Interface(const BeBoardFWMap& pBoardMap);
    ~SSA2Interface();
    bool     ConfigureChip(Ph2_HwDescription::Chip* pSSA2, bool pVerifLoop = true, uint32_t pBlockSize = 310) override; //FIXME
    void     DumpConfiguration(Ph2_HwDescription::Chip* pSSA2, std::string filename); //FIXME

    bool     setInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool pVerifLoop = true) override; //FIXME
    bool     enableInjection(Ph2_HwDescription::ReadoutChip* pChip, bool inject, bool pVerifLoop = true) override; //FIXME
    bool     setInjectionAmplitude(Ph2_HwDescription::ReadoutChip* pChip, uint8_t injectionAmplitude, bool pVerifLoop = true) override; //FIXME
    bool     maskChannelsGroup(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool pVerifLoop = true) override; //FIXME
    bool     maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool mask, bool inject, bool pVerifLoop = true) override; //FIXME
    bool     ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pSSA2, bool pVerifLoop = true, uint32_t pBlockSize = 310) override; //FIXME
    bool     MaskAllChannels(Ph2_HwDescription::ReadoutChip* pSSA2, bool mask, bool pVerifLoop = true) override; //FIXME
    bool     WriteChipReg(Ph2_HwDescription::Chip* pSSA2, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true) override; //FIXME
    bool     WriteChipMultReg(Ph2_HwDescription::Chip* pSSA2, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop = true) override; //FIXME
    bool     WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pSSA2, const std::string& dacName, ChipContainer& pValue, bool pVerifLoop = true) override; //FIXME
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pSSA2, const std::string& pRegNode) override;
    void     ReadASEvent(Ph2_HwDescription::ReadoutChip* pSSA2, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange = std::pair<uint32_t, uint32_t>({0, 0})); //FIXME
    void     Send_pulses(Ph2_HwDescription::ReadoutChip* pSSA2, uint32_t n_pulse); //FIXME

  private:
    uint8_t                        ReadChipId(Ph2_HwDescription::Chip* pChip); //FIXME
    bool                           WriteReg(Ph2_HwDescription::Chip* pCbc, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerifLoop = true); //FIXME
    bool                           WriteChipSingleReg(Ph2_HwDescription::Chip* pCbc, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true); //FIXME
    bool                           ConfigureAmux(Ph2_HwDescription::Chip* pChip, const std::string& pRegister); //FIXME
    std::map<std::string, uint8_t> fAmuxMap = {{"BoosterFeedback", 0}, //FIXME
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
