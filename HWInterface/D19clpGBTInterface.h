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
    D19clpGBTInterface(const BeBoardFWMap& pBoardMap, bool pUseOpticalLink, bool pUseCPB) : lpGBTInterface(pBoardMap), fUseOpticalLink(pUseOpticalLink), fUseCPB(pUseCPB) {}
    ~D19clpGBTInterface() {}

    // ###################################
    // # LpGBT register access functions #
    // ###################################
    // General configuration of the lpGBT chip from register file
    bool ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;

    // ###################################
    // # Outer Tracker specific funtions #
    // ###################################
    // #ifdef __TCUSB__
    //     void InitialiseTCUSBHandler();
    // #ifdef __ROH_USB__
    //     void      SetTCUSBHandler(TC_PSROH* pTC_PSROH) { fTC_USB = pTC_PSROH; }
    //     TC_PSROH* GetTCUSBHandler() { return fTC_USB; }
    // #elif __SEH_USB__
    //     void                                              SetTCUSBHandler(TC_2SSEH* pTC_2SSEH) { fTC_USB = pTC_2SSEH; }
    //     TC_2SSEH*                                         GetTCUSBHandler() { return fTC_USB; }
    // #endif
    // #endif

    // Sets the flag used to select which lpGBT configuration interface to use
    void SetConfigMode(Ph2_HwDescription::Chip* pChip, bool pUseOpticalLink, bool pUseCPB, bool pToggleTC = false);
    // configure PS-ROH
    void ConfigurePSROH(Ph2_HwDescription::Chip* pChip);
    // configure 2S-SEH
    void Configure2SSEH(Ph2_HwDescription::Chip* pChip);
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

    // reset
    uint8_t fResetMinPeriod = 100; // ms was 100

    // number of read transactions 
    uint8_t fI2CReads_M0 = 0 ; 
    uint8_t fI2CReads_M1 = 0 ; 
    uint8_t fI2CReads_M2 = 0 ; 

    // clocks
    uint8_t fClock_RHS_Hybrid = 1;
    uint8_t fClock_LHS_Hybrid = 11;
    uint8_t fClock_LHS_CIC    = 6;
    uint8_t fClock_RHS_CIC    = 26;

    // reset GPIOs
    uint8_t fReset_LHS_CIC = 0;
    uint8_t fReset_LHS_MPA = 1;
    uint8_t fReset_LHS_SSA = 3;
    uint8_t fReset_LHS_CBC = 3;
    // rhs
    uint8_t fReset_RHS_CIC = 6;
    uint8_t fReset_RHS_MPA = 9;
    uint8_t fReset_RHS_SSA = 12;
    uint8_t fReset_RHS_CBC = 8;
};
} // namespace Ph2_HwInterface
#endif
