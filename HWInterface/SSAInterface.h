/*!
        \file                                            SSAInterface.h
        \brief                                           User Interface to the SSAs
        \author                                          Marc Osherson
        \version                                         1.0
        \date                        31/07/19
        Support :                    mail to : oshersonmarc@gmail.com

 */

#ifndef __SSAINTERFACE_H__
#define __SSAINTERFACE_H__

#include "BeBoardFWInterface.h"
#include "ReadoutChipInterface.h"
#include <vector>

namespace Ph2_HwInterface
{ // start namespace

class SSAInterface : public ReadoutChipInterface
{ // begin class
  public:
    SSAInterface(const BeBoardFWMap& pBoardMap);
    ~SSAInterface();
    bool     ConfigureChip(Ph2_HwDescription::Chip* pSSA, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    bool     setInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool pVerifLoop = true) override;
    bool     enableInjection(Ph2_HwDescription::ReadoutChip* pChip, bool inject, bool pVerifLoop = true) override;
    bool     setInjectionAmplitude(Ph2_HwDescription::ReadoutChip* pChip, uint8_t injectionAmplitude, bool pVerifLoop = true) override;
    bool     maskChannelsGroup(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool pVerifLoop = true) override;
    bool     maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool mask, bool inject, bool pVerifLoop = true) override;
    bool     ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pSSA, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    bool     MaskAllChannels(Ph2_HwDescription::ReadoutChip* pSSA, bool mask, bool pVerifLoop = true) override;
    bool     WriteChipReg(Ph2_HwDescription::Chip* pSSA, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true) override;
    bool     WriteChipMultReg(Ph2_HwDescription::Chip* pSSA, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop = true) override;
    bool     WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pSSA, const std::string& dacName, ChipContainer& pValue, bool pVerifLoop = true) override;
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pSSA, const std::string& pRegNode) override;
    void     ReadASEvent(Ph2_HwDescription::ReadoutChip* pSSA, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange = std::pair<uint32_t, uint32_t>({0, 0}));
    void     Send_pulses(Ph2_HwDescription::ReadoutChip* pSSA, uint32_t n_pulse);

  private:
    uint8_t                        ReadChipId(Ph2_HwDescription::Chip* pChip);
    bool                           WriteReg(Ph2_HwDescription::Chip* pCbc, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerifLoop = true);
    bool                           WriteChipSingleReg(Ph2_HwDescription::Chip* pCbc, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true);
    bool                           ConfigureAmux(Ph2_HwDescription::Chip* pChip, const std::string& pRegister);
    std::map<std::string, uint8_t> fAmuxMap = {{"BoosterFeedback", 0},
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
