/*!
  \file                  lpGBTInterface.h
  \brief                 Interface to access and control the low-power Gigabit Transceiver chip
  \author                Younes Otarid
  \version               1.0
  \date                  03/03/20
  Support:               email to younes.otarid@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef lpGBTInterface_H
#define lpGBTInterface_H

#include "../HWDescription/lpGBT.h"
#include "../Utils/RD53Shared.h"
#include "ChipInterface.h"
#include "ReadoutChipInterface.h"
#if defined(__TCUSB__)
#include "TCInterface.h"
#endif

// ##########################
// # LpGBT useful constants #
// ##########################
namespace lpGBTconstants
{
const uint8_t PATTERN_PRBS      = 0x1; // Start PRBS pattern
const uint8_t PATTERN_NORMAL    = 0x0; // Start normal-mode pattern
const uint8_t PATTERN_CONST     = 0x4; // Constant pattern set by DP pattern
const uint8_t PATTERN_CONST_INV = 0x5; // Inverted constant pattern
const uint8_t fictitiousGroup   = 6;   // Fictitious group used when no need to speficy frontend chip
const uint8_t fictitiousChannel = 0;   // Fictitious channel used when no need to speficy frontend chip
const uint8_t rxPhaseTracking   = 2;   // Rx phase tracking mode [0 = no-tracking, 2 = automatic-tracking]
const uint8_t SUPERDEEPSLEEP    = 10;  // [milliseconds]
} // namespace lpGBTconstants

namespace Ph2_HwInterface
{
#if defined(__TCUSB__)
#if defined(__ROH_USB__)
using TestCardInterface = TCInterface<TC_PSROH>;
#elif defined(__SEH_USB__)
using TestCardInterface = TCInterface<TC_2SSEH>;
#endif
#endif

struct lpGBTClockConfig
{
    uint8_t fClkFreq = 4, fClkDriveStr = 1, fClkInvert = 1;
    uint8_t fClkPreEmphWidth = 0, fClkPreEmphMode = 0, fClkPreEmphStr = 0;
};

class lpGBTInterface : public ChipInterface
{
  protected:
#if defined(__TCUSB__) && (defined(__ROH_USB__) || defined(__SEH_USB__))
    TestCardInterface* fExternalController{nullptr};
#endif

  public:
#if defined(__TCUSB__)
    void InitializeExternalController()
    {
#if defined(__ROH_USB__)
        LOG(INFO) << BOLDYELLOW << "Initializing controller (via usb) for PS-ROH test system..." << RESET;
        fExternalController = new TestCardInterface("ROH_USB");
#elif defined(__SEH_USB__)
        LOG(INFO) << BOLDYELLOW << "Initializing controller (via usb) for 2S-SEH test system..." << RESET;
        fExternalController = new TestCardInterface("SEH_USB");
#endif
    }
#if defined(__ROH_USB__) || defined(__SEH_USB__)
    TestCardInterface* GetExternalController() const { return fExternalController; }
#endif
#endif

    lpGBTInterface(const BeBoardFWMap& pBoardMap) : ChipInterface(pBoardMap) {}
    virtual ~lpGBTInterface() {}

    void StartPRBSpattern(Ph2_HwDescription::Chip* pChip);
    void StopPRBSpattern(Ph2_HwDescription::Chip* pChip);

    // ################################
    // # Chip configuration functions #
    // ################################
    bool     WriteChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pDacName, uint16_t pDacValue, bool pVerify = true) override;
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode) override;

    // #######################################
    // # LpGBT block configuration functions #
    // #######################################
    void ConfigureRxChannels(Ph2_HwDescription::Chip*    pChip,
                             const std::vector<uint8_t>& pGroups,
                             const std::vector<uint8_t>& pChannels,
                             uint8_t                     pEqual,
                             uint8_t                     pTerm,
                             uint8_t                     pAcBias,
                             uint8_t                     pInvert,
                             uint8_t                     pPhase);

    // #######################################
    // # LpGBT block configuration functions #
    // #######################################
    void ConfigureDPPattern(Ph2_HwDescription::Chip* pChip, uint32_t pPattern);
    void ConfigureRxPRBS(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, bool pEnable);
    void ConfigureRxSource(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, uint8_t pSource);
    void ConfigureTxSource(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, uint8_t pSource);
    void ConfigureTxGroups(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, uint8_t pDataRate);

    // ########################
    // # LpGBT GPIO functions #
    // ########################
    void ConfigureGPIODirection(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pDir);
    void ConfigureGPIOLevel(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pOut);
    bool ReadGPIO(Ph2_HwDescription::Chip* pChip, uint8_t pGPIO);

    // ###########################
    // # LpGBT ADC-DAC functions #
    // ###########################
    uint16_t ReadADC(Ph2_HwDescription::Chip* pChip, const std::string& pADCInputP, const std::string& pADCInputN = "VREF/2", uint8_t pGain = 0);
    void     ConfigureInternalMonitoring(Ph2_HwDescription::Chip* pChip, uint8_t pEnable);

    // ##############################
    // # LpGBT retreive temperature #
    // ##############################
    float GetInternalTemperature(Ph2_HwDescription::Chip* pChip);
    float ReadResistance(Ph2_HwDescription::Chip* pChip, const std::string& pADC, const std::vector<uint8_t>& pCurrents, uint8_t pGain = 0);

    // ####################################
    // # LpGBT eye opening monitor tester #
    // ####################################
    void     ConfigureEOM(Ph2_HwDescription::Chip* pChip, uint8_t pEndOfCountSelect, bool pByPassPhaseInterpolator = false, bool pEnableEOM = true);
    void     SelectEOMVof(Ph2_HwDescription::Chip* pChip, uint8_t pVof);
    void     SelectEOMPhase(Ph2_HwDescription::Chip* pChip, uint8_t pPhase);
    void     StartEOM(Ph2_HwDescription::Chip* pChip, bool pStartEOM = true);
    uint8_t  GetEOMStatus(Ph2_HwDescription::Chip* pChip);
    uint16_t GetEOMCounter(Ph2_HwDescription::Chip* pChip);

    // ##############
    // # LpGBT BERT #
    // ##############
    void   ConfigureBERT(Ph2_HwDescription::Chip* pChip, uint8_t pCoarseSource, uint8_t pFineSource, uint8_t pMeasTime, bool pSkipDisable = false);
    void   ConfigureBERTPattern(Ph2_HwDescription::Chip* pChip, uint32_t pPattern);
    double GetBERTResult(Ph2_HwDescription::Chip* pChip);
    double RunBERtest(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel, bool given_time, double frames_or_time, uint8_t frontendSpeed);
    double BERtestCL(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel, bool given_time, double bits_or_time, float pConfidenceLevel);

    // ############################################
    // # LpGBT Manual phase alignment of Rx ports #
    // ############################################
    void ManualPhaseAlignRx(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel);

    // ##############################################
    // # LpGBT I2C Masters functions (Slow Control) #
    // ##############################################
    void     ResetI2C(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pMasters);
    void     ConfigureI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMaster, uint8_t pFreq, uint8_t pNBytes, uint8_t pSCLDriveMode);
    bool     WriteI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMaster, uint8_t pSlaveAddress, uint32_t pData, uint8_t pNBytes, uint8_t pFreq = 3 /* 3   1 MHz */);
    uint32_t ReadI2C(Ph2_HwDescription::Chip* pChip, uint8_t pMaster, uint8_t pSlaveAddress, uint8_t pNBytes, uint8_t pFreq = 3 /* 3   1 MHz */);
    uint8_t  GetI2CStatus(Ph2_HwDescription::Chip* pChip, uint8_t pMaster);
    bool     IsI2CSuccess(Ph2_HwDescription::Chip* pChip, uint8_t pMaster);

    // ###########################
    // # LpGBT ADC-DAC functions #
    // ###########################
    void ConfigureADC(Ph2_HwDescription::Chip* pChip, uint8_t pGainSelect, bool pADCEnable, bool pStartConversion);
    void ConfigureCurrentDAC(Ph2_HwDescription::Chip* pChip, const std::vector<std::string>& pCurrentDACChannels, uint8_t pCurrentDACOutput);
    bool IsReadADCDone(Ph2_HwDescription::Chip* pChip);

    // ################################
    // # LpGBT retreive configuration #
    // ################################
    uint16_t GetRxDataRate(Ph2_HwDescription::Chip* pChip, uint8_t pGroup);
    uint8_t  GetChipRate(Ph2_HwDescription::Chip* pChip);

    // #######################
    // # LpGBT Vref function #
    // #######################
    bool ConfigureVref(Ph2_HwDescription::Chip* pChip, uint8_t pEnable, uint8_t pCorrection);

    void    PhaseAlignRx(Ph2_HwDescription::Chip* pChip, const Ph2_HwDescription::OpticalGroup* pOpticalGroup);
    uint8_t AutoPhaseAlignRx(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels);
    void    ConfigureRxPhase(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel, uint8_t pPhase);
    void    ConfigureTxChannels(Ph2_HwDescription::Chip*    pChip,
                                const std::vector<uint8_t>& pGroups,
                                const std::vector<uint8_t>& pChannels,
                                uint8_t                     pDriveStr,
                                uint8_t                     pPreEmphMode,
                                uint8_t                     pPreEmphStr,
                                uint8_t                     pPreEmphWidth,
                                uint8_t                     pInvert);
    void    SetPhaseTap(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel, uint8_t pPhase);
    uint8_t GetPhaseTap(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel); // To-Do: change to a map

    std::map<std::string, uint8_t> fADCInputMap = {{"ADC0", 0},
                                                   {"ADC1", 1},
                                                   {"ADC2", 2},
                                                   {"ADC3", 3},
                                                   {"ADC4", 4},
                                                   {"ADC5", 5},
                                                   {"ADC6", 6},
                                                   {"ADC7", 7},
                                                   {"EOM_DAC", 8},
                                                   {"VDDIO", 9},
                                                   {"VDDTX", 10},
                                                   {"VDDRX", 11},
                                                   {"VDD", 12},
                                                   {"VDDA", 13},
                                                   {"TEMP", 14},
                                                   {"VREF/2", 15}};

  protected:
    const float fClockSpeed = 40e6; // 40 MHz clock for the LpGBT

    bool WriteChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& RegVec, bool pVerify = true) override;

    // #######################################
    // # LpGBT block configuration functions #
    // #######################################
    void SetPUSMDone(Ph2_HwDescription::Chip* pChip, bool pPllConfigDone, bool pDllConfigDone);
    void ConfigureRxGroups(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, uint8_t pDataRate, uint8_t pTrackMode);
    void ConfigureRxAlignmentMode(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, uint8_t pTrackMode);
    void ConfigureClocks(Ph2_HwDescription::Chip*    pChip,
                         const std::vector<uint8_t>& pClock,
                         uint8_t                     pFreq,
                         uint8_t                     pDriveStr,
                         uint8_t                     pInvert,
                         uint8_t                     pPreEmphWidth,
                         uint8_t                     pPreEmphMode,
                         uint8_t                     pPreEmphStr);
    void ConfigureHighSpeedPolarity(Ph2_HwDescription::Chip* pChip, uint8_t pOutPolarity, uint8_t pInPolarity);
    void ConfigurePhShifter(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pClocks, uint8_t pFreq, uint8_t pDriveStr, uint8_t pEnFTune, uint16_t pDelay);

    // ####################################
    // # LpGBT specific routine functions #
    // ####################################
    void PhaseTrainRx(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups);
    void InternalPhaseAlignRx(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels);
    void ResetRxDll(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups);

    // ################################
    // # LpGBT block status functions #
    // ################################
    bool    IsPUSMDone(Ph2_HwDescription::Chip* pChip);
    void    PrintChipMode(Ph2_HwDescription::Chip* pChip);
    uint8_t GetPUSMStatus(Ph2_HwDescription::Chip* pChip);
    uint8_t GetRxPhase(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel);
    bool    IsRxLocked(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, const std::vector<uint8_t>& pChannels);
    uint8_t GetRxDllStatus(Ph2_HwDescription::Chip* pChip, uint8_t pGroup);

    // ########################
    // # LpGBT GPIO functions #
    // ########################
    void ConfigureGPIODriverStrength(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pDriveStr);
    void ConfigureGPIOPull(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pPullEn, uint8_t pPullUpDown);

    // ##############
    // # LpGBT BERT #
    // ##############
    void     StartBERT(Ph2_HwDescription::Chip* pChip, bool pStartBERT = true);
    uint8_t  GetBERTStatus(Ph2_HwDescription::Chip* pChip);
    bool     IsBERTDone(Ph2_HwDescription::Chip* pChip);
    bool     IsBERTEmptyData(Ph2_HwDescription::Chip* pChip);
    uint64_t GetBERTErrors(Ph2_HwDescription::Chip* pChip);

    // ##############
    // # LpGBT maps #
    // ##############
    std::map<uint16_t, uint8_t> fClockFrequencyMap = {{0, 0}, {40, 1}, {80, 2}, {160, 3}, {320, 4}, {640, 5}, {1280, 7}};
    std::map<uint16_t, uint8_t> fTxDataRateMap     = {{0, 0}, {80, 1}, {160, 2}, {320, 3}};
    std::map<uint16_t, uint8_t> f5GRxDataRateMap   = {{0, 0}, {160, 1}, {320, 2}, {640, 3}};
    std::map<uint16_t, uint8_t> f10GRxDataRateMap  = {{0, 0}, {320, 1}, {640, 2}, {1280, 3}};

    std::map<uint8_t, uint8_t> fGroup2BERTsourceCourse      = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}};
    std::map<uint8_t, uint8_t> fChannelSpeed2BERTsourceFine = {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {6, 5}, {8, 6}}; // channel + 4 * (2 - frontendSpeed)

    // Power Up State Machine maps for both LpGBT-v0 and LpGBT-v1
    // map read as : map[lpgbt_version][state_id] = state_description
    std::map<uint8_t, std::map<uint8_t, std::string>> fPUSMStatusMap = {{0,
                                                                         {{0, "ARESET"},
                                                                          {1, "RESET"},
                                                                          {2, "WAIT_VDD_STABLE"},
                                                                          {3, "WAIT_VDD_HIGHER_THAN_0V90"},
                                                                          {4, "FUSE_SAMPLING"},
                                                                          {5, "UPDATE_FROM_FUSES"},
                                                                          {6, "WAIT_FOR_PLL_CONFIG"},
                                                                          {7, "WAIT_POWER_GOOD"},
                                                                          {8, "RESETOUT"},
                                                                          {9, "I2C_TRANS"},
                                                                          {10, "RESET_PLL"},
                                                                          {11, "WAIT_PLL_LOCK"},
                                                                          {12, "INIT_SCRAM"},
                                                                          {13, "PAUSE_FOR_DLL_CONFIG"},
                                                                          {14, "RESET_DLLS"},
                                                                          {15, "WAIT_DLL_LOCK"},
                                                                          {16, "RESET_LOGIC_USING_DLL"},
                                                                          {17, "WAIT_CHNS_LOCKED"},
                                                                          {18, "READY"}}},
                                                                        {1,
                                                                         {{0, "ARESET"},
                                                                          {1, "RESET"},
                                                                          {2, "WAIT_VDD_STABLE"},
                                                                          {3, "WAIT_VDD_HIGHER_THAN_0V90"},
                                                                          {4, "STATE_COPY_FUSES"},
                                                                          {5, "STATE_CALCULATE_CHECKSUM"},
                                                                          {6, "COPY_ROM"},
                                                                          {7, "PAUSE_FOR_PLL_CONFIG"},
                                                                          {8, "WAIT_POWER_GOOD"},
                                                                          {9, "RESET_PLL"},
                                                                          {10, "WAIT_PLL_LOCK"},
                                                                          {11, "INIT_SCRAM"},
                                                                          {12, "RESETOUT"},
                                                                          {13, "I2C_TRANS"},
                                                                          {14, "PAUSE_FOR_DLL_CONFIG"},
                                                                          {15, "RESET_DLLS"},
                                                                          {16, "WAIT_DLL_LOCK"},
                                                                          {17, "RESET_LOGIC_USING_DLL"},
                                                                          {18, "WAIT_CHNS_LOCKED"},
                                                                          {19, "READY"}}}};

    std::map<std::string, uint8_t> revertedPUSMStatusMap;
    std::map<uint8_t, double>      fBERTMeasTimeMap = {{0, RD53Shared::setBits(5) + 1},
                                                  {1, RD53Shared::setBits(7) + 1},
                                                  {2, RD53Shared::setBits(9) + 1},
                                                  {3, RD53Shared::setBits(11) + 1},
                                                  {4, RD53Shared::setBits(13) + 1},
                                                  {5, RD53Shared::setBits(15) + 1},
                                                  {6, RD53Shared::setBits(17) + 1},
                                                  {7, RD53Shared::setBits(19) + 1},
                                                  {8, RD53Shared::setBits(21) + 1},
                                                  {9, RD53Shared::setBits(23) + 1},
                                                  {10, RD53Shared::setBits(25) + 1},
                                                  {11, RD53Shared::setBits(27) + 1},
                                                  {12, RD53Shared::setBits(29) + 1},
                                                  {13, RD53Shared::setBits(31) + 1},
                                                  {14, RD53Shared::setBits(33) + 1},
                                                  {15, RD53Shared::setBits(35) + 1}};

    std::map<uint8_t, std::string> fEOMStatusMap = {{0, "smIdle"}, {1, "smResetCounters"}, {2, "smCount"}, {3, "smEndOfCount"}};

    std::map<uint8_t, std::string> fI2CStatusMap = {{4, "TransactionSucess"}, {8, "SDAPulledLow"}, {32, "InvalidCommand"}, {64, "NotACK"}};
    std::map<std::string, uint8_t> fPhaseTapMap  = {
        {"Group0Channel0", 0},
        {"Group0Channel2", 0},
        {"Group1Channel0", 0},
        {"Group1Channel2", 0},
        {"Group2Channel0", 0},
        {"Group2Channel2", 0},
        {"Group3Channel0", 0},
        {"Group3Channel2", 0},
        {"Group4Channel0", 0},
        {"Group4Channel2", 0},
        {"Group5Channel0", 0},
        {"Group5Channel2", 0},
        {"Group6Channel0", 0},
        {"Group6Channel2", 0},
    };
};

} // namespace Ph2_HwInterface

#endif
