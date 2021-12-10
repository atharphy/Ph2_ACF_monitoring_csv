/*!
  \file                  D19clpGBTInterface.h
  \brief                 Interface to access and control the low-power Gigabit Transceiver chip
  \author                Younes Otarid
  \version               1.0
  \date                  03/03/20
  Support:               email to younes.otarid@cern.ch
*/

#ifndef D19clpGBTInterface_H
#define D19clpGBTInterface_H

#include "lpGBTInterface.h"
#ifdef __TCUSB__
#include "USB_a.h"
#include "USB_libusb.h"
#endif

namespace Ph2_HwInterface
{
class D19clpGBTInterface : public lpGBTInterface
{
  public:
    D19clpGBTInterface(const BeBoardFWMap& pBoardMap, bool pUseOpticalLink, bool pUseCPB) : lpGBTInterface(pBoardMap), fUseOpticalLink(pUseOpticalLink), fUseCPB(pUseCPB)
    {
        // configure during constructor now when configuring chip
        SetConfigMode(pUseOpticalLink, pUseCPB);
        // configure CPB - do this here rather than in SystemController? Not sure
        CPBconfig cCPBconfig;
        cCPBconfig.fEnable       = pUseCPB;
        cCPBconfig.fI2CFrequency = 3;
        cCPBconfig.fWait_us      = 0;    // TO-DO - make configurable from xml
        cCPBconfig.fReTry        = 1;    // TO-DO - make configurable from xml
        cCPBconfig.fVerbose      = 0;    // TO-DO - make configurable from xml
        cCPBconfig.fMaxAttempts  = 5000; // TO-DO - make configurable from xml
        cCPBconfig.fResetEn      = 1;    // TO-DO - make configurable from xml
        // configure FW for all boards
        for(auto cBoardMap: pBoardMap) { (cBoardMap.second)->ConfigureCPB(cCPBconfig); }
    }
    ~D19clpGBTInterface() {}

    // ###################################
    // # LpGBT register access functions #
    // ###################################
    // General configuration of the lpGBT chip from register file
    bool ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    bool SwitchOnSEH();
    
    // ###################################
    // # Outer Tracker specific funtions #
    // ###################################
#ifdef __TCUSB__
    void InitialiseTCUSBHandler();
#ifdef __ROH_USB__
    void      SetTCUSBHandler(TC_PSROH* pTC_PSROH) { fTC_USB = pTC_PSROH; }
    TC_PSROH* GetTCUSBHandler() { return fTC_USB; }
#elif __SEH_USB__
#ifdef __TCP_SERVER__
#else
    void      SetTCUSBHandler(TC_2SSEH* pTC_2SSEH) { fTC_USB = pTC_2SSEH; }
    TC_2SSEH* GetTCUSBHandler() { return fTC_USB; }
#endif
#endif

    // Sets the flag used to select which lpGBT configuration interface to use
    void SetConfigMode(bool pUseOpticalLink, bool pUseCPB, bool pToggleTC = false);
    // configure PS-ROH
    void ConfigurePSROH(Ph2_HwDescription::Chip* pChip);
    // configure 2S-SEH
    void        Configure2SSEH(Ph2_HwDescription::Chip* pChip);
    std::string getVariableValue(std::string variable, std::string buffer);
    // cbc read/write
    bool cbcWrite(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t pChipId, uint8_t pPage, uint8_t pRegistergAddress, uint8_t pRegisterValue, bool pReadBack = true, bool pSetPage = false)
    {
        return true;
    }
    uint32_t cbcRead(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t pChipId, uint8_t pPage, uint8_t pRegisterAddress) { return 0; }
    uint8_t  cbcSetPage(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t pChipId, uint8_t pPage) { return 0; }
    uint8_t  cbcGetPageRegister(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t cChipId) { return 0; }
    // cic read/write
    bool     cicWrite(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry = false);
    uint32_t cicRead(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint16_t pRegisterAddress);
    // ssa read/write
    bool     ssaWrite(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t pChipId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry = false);
    uint32_t ssaRead(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t pChipId, uint16_t pRegisterAddress);
    // mpa read/write
    bool     mpaWrite(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t pChipId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry = false);
    uint32_t mpaRead(Ph2_HwDescription::Chip* pChip, uint8_t pFeId, uint8_t pChipId, uint16_t pRegisterAddress);
    void     ContinuousPhaseAlignRx(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels);

    // 0 [RHS], 1 [LHS]
    // active reset functions
    void cicReset(Ph2_HwDescription::Chip* pChip, bool pEnable, uint8_t pSide = 0)
    {
        if(pSide == 0)
            ConfigureGPIOLevel(pChip, {fReset_RHS_CIC}, (pEnable) ? 0 : 1);
        else
            ConfigureGPIOLevel(pChip, {fReset_LHS_CIC}, (pEnable) ? 0 : 1);
    }
    void ssaReset(Ph2_HwDescription::Chip* pChip, bool pEnable, uint8_t pSide = 0)
    {
        if(pSide == 0)
            ConfigureGPIOLevel(pChip, {fReset_RHS_SSA}, (pEnable) ? 0 : 1);
        else
            ConfigureGPIOLevel(pChip, {fReset_LHS_SSA}, (pEnable) ? 0 : 1);
    }
    void mpaReset(Ph2_HwDescription::Chip* pChip, bool pEnable, uint8_t pSide = 0)
    {
        if(pSide == 0)
            ConfigureGPIOLevel(pChip, {fReset_RHS_MPA}, (pEnable) ? 0 : 1);
        else
            ConfigureGPIOLevel(pChip, {fReset_LHS_MPA}, (pEnable) ? 0 : 1);
    }
    void cbcReset(Ph2_HwDescription::Chip* pChip, bool pEnable, uint8_t pSide = 0)
    {
        if(pSide == 0)
            ConfigureGPIOLevel(pChip, {fReset_RHS_CBC}, (pEnable) ? 1 : 0);
        else
            ConfigureGPIOLevel(pChip, {fReset_LHS_CBC}, (pEnable) ? 1 : 0);
    }
    // 0 [RHS], 1 [LHS]
    // send reset functions
    void resetCIC(Ph2_HwDescription::Chip* pChip, uint8_t pSide = 0)
    {
        cicReset(pChip, 1, pSide);
        std::this_thread::sleep_for(std::chrono::microseconds(fResetMinPeriod));
        cicReset(pChip, 0, pSide);
        std::this_thread::sleep_for(std::chrono::microseconds(fResetMinPeriod));
    }
    void resetSSA(Ph2_HwDescription::Chip* pChip, uint8_t pSide = 0)
    {
        ssaReset(pChip, 1, pSide);
        std::this_thread::sleep_for(std::chrono::microseconds(fResetMinPeriod));
        ssaReset(pChip, 0, pSide);
        std::this_thread::sleep_for(std::chrono::microseconds(fResetMinPeriod));
    }
    void resetMPA(Ph2_HwDescription::Chip* pChip, uint8_t pSide = 0)
    {
        mpaReset(pChip, 1, pSide);
        std::this_thread::sleep_for(std::chrono::microseconds(fResetMinPeriod));
        mpaReset(pChip, 0, pSide);
        std::this_thread::sleep_for(std::chrono::microseconds(fResetMinPeriod));
    }
    void resetCBC(Ph2_HwDescription::Chip* pChip, uint8_t pSide = 0)
    {
        cbcReset(pChip, 1, pSide);
        std::this_thread::sleep_for(std::chrono::microseconds(fResetMinPeriod));
        cbcReset(pChip, 0, pSide);
        std::this_thread::sleep_for(std::chrono::microseconds(fResetMinPeriod));
    }

    void configureClockSettings(Ph2_HwDescription::Chip* pChip, uint8_t pClk, lpGBTClockConfig pClkCnfg)
    {
        fClkConfig.fClkFreq         = pClkCnfg.fClkFreq;
        fClkConfig.fClkInvert       = pClkCnfg.fClkInvert;
        fClkConfig.fClkDriveStr     = pClkCnfg.fClkDriveStr;
        fClkConfig.fClkInvert       = pClkCnfg.fClkInvert;
        fClkConfig.fClkPreEmphWidth = pClkCnfg.fClkPreEmphWidth;
        fClkConfig.fClkPreEmphMode  = pClkCnfg.fClkPreEmphMode;
        fClkConfig.fClkPreEmphStr   = pClkCnfg.fClkPreEmphStr;

        std::string cClkHReg = "EPCLK" + std::to_string(pClk) + "ChnCntrH";
        std::string cClkLReg = "EPCLK" + std::to_string(pClk) + "ChnCntrL";
        WriteChipReg(pChip, cClkHReg, fClkConfig.fClkInvert << 6 | fClkConfig.fClkDriveStr << 3 | fClkConfig.fClkFreq);
        WriteChipReg(pChip, cClkLReg, fClkConfig.fClkPreEmphStr << 5 | fClkConfig.fClkPreEmphMode << 3 | fClkConfig.fClkPreEmphWidth);
    }
    void cicClock(Ph2_HwDescription::Chip* pChip, lpGBTClockConfig pClkCnfg, uint8_t pSide = 0) { configureClockSettings(pChip, (pSide == 0) ? fClock_RHS_CIC : fClock_LHS_CIC, pClkCnfg); }
    void hybridClock(Ph2_HwDescription::Chip* pChip, lpGBTClockConfig pClkCnfg, uint8_t pSide = 0) { configureClockSettings(pChip, (pSide == 0) ? fClock_RHS_Hybrid : fClock_LHS_Hybrid, pClkCnfg); }

    void                 setFrontEndType(FrontEndType pType) { fFeType = pType; }
    FrontEndType         getFrontEndType() { return fFeType; }
    std::vector<uint8_t> getGPIOs()
    {
        if(fFeType == FrontEndType::OuterTracker2S) return {fReset_LHS_CIC, fReset_LHS_CBC, fReset_RHS_CIC, fReset_RHS_CBC};
        if(fFeType == FrontEndType::OuterTrackerPS) return {fReset_LHS_CIC, fReset_LHS_MPA, fReset_LHS_SSA, fReset_RHS_CIC, fReset_RHS_MPA, fReset_RHS_SSA};
        return {};
    }

  private:
    // default clock configuration
    lpGBTClockConfig fClkConfig;
    // front-end type
    FrontEndType fFeType;

    // ###################################
    // # Outer Tracker specific objects  #
    // ###################################
    bool fUseOpticalLink = true;
    bool fUseCPB         = true;
#ifdef __TCUSB__

#ifdef __ROH_USB__
    TC_PSROH*                                    fTC_USB;
    std::map<std::string, TC_PSROH::measurement> fResetLines = {{"L_MPA", TC_PSROH::measurement::L_MPA_RST},
                                                                {"L_CIC", TC_PSROH::measurement::L_CIC_RST},
                                                                {"L_SSA", TC_PSROH::measurement::L_SSA_RST},
                                                                {"R_MPA", TC_PSROH::measurement::R_MPA_RST},
                                                                {"R_CIC", TC_PSROH::measurement::R_CIC_RST},
                                                                { "R_SSA",
                                                                  TC_PSROH::measurement::R_SSA_RST }};

#elif __SEH_USB__
#ifdef __TCP_SERVER__
#else

    TC_2SSEH*                                         fTC_USB;
    std::map<std::string, TC_2SSEH::resetMeasurement> fSehResetLines = {{"RST_CBC_R", TC_2SSEH::resetMeasurement::RST_CBC_R},
                                                                        {"RST_CIC_R", TC_2SSEH::resetMeasurement::RST_CIC_R},
                                                                        {"RST_CBC_L", TC_2SSEH::resetMeasurement::RST_CBC_L},
                                                                        {"RST_CIC_L", TC_2SSEH::resetMeasurement::RST_CIC_L}};
#endif
#endif
#endif
};
} // namespace Ph2_HwInterface
#endif
