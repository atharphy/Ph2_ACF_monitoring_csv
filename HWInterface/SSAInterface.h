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
#include "D19clpGBTInterface.h"
#include "ReadoutChipInterface.h"
#include <vector>

// pixelEnable bits
const std::map<std::string, uint8_t> STRIP_ENABLE_TABLE = {{"StripMask", 0}, {"Polarity", 1}, {"CounterEnable", 2}, {"DigitalInjection", 3}, {"AnalogueInjection", 4}};

namespace Ph2_HwInterface
{ // start namespace

class SSAInterface : public ReadoutChipInterface
{ // begin class
  public:
    SSAInterface(const BeBoardFWMap& pBoardMap);
    ~SSAInterface();
    // FIXME temporary fix to use 1/2 PS skeleton
    // void     LinkLpGBT(Ph2_HwInterface::D19clpGBTInterface* pLpGBTInterface, Ph2_HwDescription::lpGBT* pLpGBT);
    bool     ConfigureChip(Ph2_HwDescription::Chip* pSSA, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    bool     setInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop = true) override;
    bool     enableInjection(Ph2_HwDescription::ReadoutChip* pChip, bool inject, bool pVerifLoop = true) override;
    bool     setInjectionAmplitude(Ph2_HwDescription::ReadoutChip* pChip, uint8_t injectionAmplitude, bool pVerifLoop = true) override;
    bool     maskChannelGroup(Ph2_HwDescription::ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop = true) override;
    bool     maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop = true) override;
    bool     ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pSSA, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    bool     MaskAllChannels(Ph2_HwDescription::ReadoutChip* pSSA, bool mask, bool pVerifLoop = true) override;
    bool     WriteChipReg(Ph2_HwDescription::Chip* pSSA, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true) override;
    bool     WriteChipMultReg(Ph2_HwDescription::Chip* pSSA, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop = true) override;
    bool     WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pSSA, const std::string& dacName, ChipContainer& pValue, bool pVerifLoop = true) override;
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pSSA, const std::string& pRegNode) override;
    void     ReadASEvent(Ph2_HwDescription::ReadoutChip* pSSA, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange = std::pair<uint32_t, uint32_t>({0, 0}));
    void     Send_pulses(Ph2_HwDescription::ReadoutChip* pSSA, uint32_t n_pulse);

    // std::pair<uint16_t,uint16_t>      getReadBackErrorSummary(){ return std::make_pair(fReadBackErrors, fRegisterWrites); }
    // std::pair<uint16_t,uint16_t>      getWriteErrorSummary(){ return std::make_pair(fWriteErrors, fRegisterWrites); }
    // void                              resetErrorSummary(){fWriteErrorMap.clear(); fReadBackErrorMap.clear(); fRegisterWrites=0; };
    // void                              printErrorSummary();
    // bool     runVerification(Ph2_HwDescription::Chip* pSSA, uint16_t pValue,  std::string pRegName);
    bool                          runVerification(Ph2_HwDescription::Chip* pChip, uint16_t pValue, std::string pRegName);
    std::pair<uint16_t, uint16_t> getRetrySummary() { return std::make_pair(fReW, fReWR); }
    std::pair<int, float>         getWRattempts();
    std::pair<float, float>       getMinMaxWRattempts();
    std::pair<uint16_t, uint16_t> getReadBackErrorSummary() { return std::make_pair(fReadBackErrors, fRegisterWrites); }
    std::pair<uint16_t, uint16_t> getWriteErrorSummary() { return std::make_pair(fWriteErrors, fRegisterWrites); }
    void                          resetRetrySummary()
    {
        fReWMap.clear();
        fReWrMap.clear();
        fReW  = 0;
        fReWR = 0;
    }
    void resetErrorSummary()
    {
        fWriteErrorMap.clear();
        fReadBackErrorMap.clear();
        fRegisterWrites = 0;
        fReadBackErrors = 0;
        fWriteErrors    = 0;
        resetRetrySummary();
    };
    void printErrorSummary();
    void setRetryI2C(bool pRetry) { fRetryI2C = pRetry; }
    void setMaxI2CAttempts(uint8_t pMaxAttempts) { fMaxI2CAttempts = pMaxAttempts; }

    void producePhaseAlignmentPattern(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pWait_ms = 10) override;
    void produceWordAlignmentPattern(Ph2_HwDescription::ReadoutChip* pChip) override;

  private:
    // I2C config
    bool    fRetryI2C       = false;
    uint8_t fMaxI2CAttempts = 20;

    // D19clpGBTInterface*             flpGBTInterface = nullptr;
    // Ph2_HwDescription::lpGBT*       flpGBT          = nullptr;
    std::map<uint16_t, std::string> fMap;
    // re-try counters
    std::map<uint8_t, uint16_t> fReWMap;
    std::map<uint8_t, uint16_t> fReWrMap;
    // error counters
    std::map<uint8_t, uint16_t> fWriteErrorMap;
    std::map<uint8_t, uint16_t> fReadBackErrorMap;
    uint16_t                    fReadBackErrors = 0;
    uint16_t                    fWriteErrors    = 0;
    // register write counter
    uint16_t fRegisterWrites = 0;
    // re-tries
    uint16_t fReW  = 0;
    uint16_t fReWR = 0;

    void Set_calibration(Ph2_HwDescription::Chip* pSSA, uint32_t cal);
    void Set_threshold(Ph2_HwDescription::Chip* pSSA, uint32_t th);

    uint8_t                        ReadChipId(Ph2_HwDescription::Chip* pSSA);
    bool                           WriteReg(Ph2_HwDescription::Chip* pSSA, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerifLoop = true);
    bool                           WriteRegs(Ph2_HwDescription::Chip* pSSA, const std::vector<std::pair<uint16_t, uint16_t>> pRegs, bool pVerifLoop = true);
    uint16_t                       ReadReg(Ph2_HwDescription::Chip* pSSA, uint16_t pRegisterAddress, bool pVerifLoop = true);
    bool                           WriteChipSingleReg(Ph2_HwDescription::Chip* pSSA, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true);
    bool                           ConfigureAmux(Ph2_HwDescription::Chip* pSSA, const std::string& pRegister);
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
                                               {"HighZ", 12},
                                               {"ADC_IREF",13},
                                               {"ADC_VREF",14}};



}; // end class

} // namespace Ph2_HwInterface

#endif
