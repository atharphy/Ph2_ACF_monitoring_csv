/*!
  \file                  RD53lpGBTInterface.h
  \brief                 Interface to access and control the Low-power Gigabit Transceiver chip
  \author                Mauro DINARDO
  \version               1.0
  \date                  03/03/20
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53lpGBTInterface_H
#define RD53lpGBTInterface_H

#include "../HWInterface/RD53FWInterface.h"
#include "../HWInterface/ReadoutChipInterface.h"
/* #include "../Utils/Container.h" */
#include "../Utils/RD53Shared.h"
#include "lpGBTInterface.h"

// ##########################
// # LpGBT useful constants #
// ##########################
namespace RD53lpGBTconstants
{
const uint8_t MAXATTEMPTS       = 10;   // Maximum number of attempts
const uint8_t LPGBTADDRESS      = 0x70; // LpGBT chip address
const uint8_t PATTERN_PRBS      = 0x1;  // Start PRBS pattern
const uint8_t PATTERN_NORMAL    = 0x0;  // Start normal-mode pattern
const uint8_t fictitiousGroup   = 6;    // Fictitious group used when no need to speficy frontend chip
const uint8_t fictitiousChannel = 0;    // Fictitious channel used when no need to speficy frontend chip
} // namespace RD53lpGBTconstants

namespace Ph2_HwInterface
{
class RD53lpGBTInterface : public lpGBTInterface
{
  public:
    RD53lpGBTInterface(const BeBoardFWMap& pBoardMap) : lpGBTInterface(pBoardMap) {}

    // #############################
    // # Override member functions #
    // #############################
    bool     ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    bool     WriteChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true) override;
    bool     WriteChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& RegVec, bool pVerifLoop = true) override;
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode) override;
    double   RunBERtest(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel, bool given_time, double frames_or_time, uint8_t frontendSpeed = 0) override;
    void     StartPRBSpattern(Ph2_HwDescription::Chip* pChip) override;
    void     StopPRBSpattern(Ph2_HwDescription::Chip* pChip) override;
    // #############################

    void ExternalPhaseAlignRx(Ph2_HwDescription::Chip*               pChip,
                              const Ph2_HwDescription::BeBoard*      pBoard,
                              const Ph2_HwDescription::OpticalGroup* pOpticalGroup,
                              Ph2_HwInterface::BeBoardFWInterface*   pBeBoardFWInterface,
                              ReadoutChipInterface*                  pReadoutChipInterface);

  private:
    bool     WriteReg(Ph2_HwDescription::Chip* pChip, uint16_t pAddress, uint16_t pValue, bool pVerifLoop = true);
    uint16_t ReadReg(Ph2_HwDescription::Chip* pChip, uint16_t pAddress);

    // #######################################
    // # LpGBT block configuration functions #
    // #######################################
    void ConfigureRxGroups(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, uint8_t pDataRate, uint8_t pTrackMode);
    void ConfigureRxChannels(Ph2_HwDescription::Chip*    pChip,
                             const std::vector<uint8_t>& pGroups,
                             const std::vector<uint8_t>& pChannels,
                             uint8_t                     pEqual,
                             uint8_t                     pTerm,
                             uint8_t                     pAcBias,
                             uint8_t                     pInvert,
                             uint8_t                     pPhase);
    void ConfigureTxGroups(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, uint8_t pDataRate);
    void ConfigureTxChannels(Ph2_HwDescription::Chip*    pChip,
                             const std::vector<uint8_t>& pGroups,
                             const std::vector<uint8_t>& pChannels,
                             uint8_t                     pDriveStr,
                             uint8_t                     pPreEmphMode,
                             uint8_t                     pPreEmphStr,
                             uint8_t                     pPreEmphWidth,
                             uint8_t                     pInvert);
    void ConfigureClocks(Ph2_HwDescription::Chip*    pChip,
                         const std::vector<uint8_t>& pClock,
                         uint8_t                     pFreq,
                         uint8_t                     pDriveStr,
                         uint8_t                     pInvert,
                         uint8_t                     pPreEmphWidth,
                         uint8_t                     pPreEmphMode,
                         uint8_t                     pPreEmphStr);
    void ConfigureHighSpeedPolarity(Ph2_HwDescription::Chip* pChip, uint8_t pOutPolarity, uint8_t pInPolarity);
    void ConfigureDPPattern(Ph2_HwDescription::Chip* pChip, uint32_t pPattern);
    void ConfigureRxPRBS(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels, bool pEnable);
    void ConfigureRxSource(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, uint8_t pSource);
    void ConfigureTxSource(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, uint8_t pSource);
    void ConfigureRxPhase(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel, uint8_t pPhase);
    void ConfigurePhShifter(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pClocks, uint8_t pFreq, uint8_t pDriveStr, uint8_t pEnFTune, uint16_t pDelay);

    // ####################################
    // # LpGBT specific routine functions #
    // ####################################
    void PhaseTrainRx(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, bool pTrain);
    void InternalPhaseAlignRx(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGroups, const std::vector<uint8_t>& pChannels);

    // ################################
    // # LpGBT block status functions #
    // ################################
    void    PrintChipMode(Ph2_HwDescription::Chip* pChip);
    uint8_t GetChipRate(Ph2_HwDescription::Chip* pChip);
    uint8_t GetPUSMStatus(Ph2_HwDescription::Chip* pChip);
    uint8_t GetRxPhase(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, uint8_t pChannel);
    bool    IsRxLocked(Ph2_HwDescription::Chip* pChip, uint8_t pGroup, const std::vector<uint8_t>& pChannels);
    uint8_t GetRxDllStatus(Ph2_HwDescription::Chip* pChip, uint8_t pGroup);

    // ########################
    // # LpGBT GPIO functions #
    // ########################
    void ConfigureGPIO(Ph2_HwDescription::Chip* pChip, const std::vector<uint8_t>& pGPIOs, uint8_t pInOut, uint8_t pHighLow, uint8_t pDriveStr, uint8_t pPullEn, uint8_t pPullUpDown);

    // ###########################
    // # LpGBT ADC-DAC functions #
    // ###########################
    void     ConfigureADC(Ph2_HwDescription::Chip* pChip, uint8_t pGainSelect, uint8_t pADCCoreDiffEnable);
    uint16_t ReadADC(Ph2_HwDescription::Chip* pChip, const std::string& pADCInput);
    uint16_t ReadADCDiff(Ph2_HwDescription::Chip* pChip, const std::string& pADCInputP, const std::string& pADCInputN);

    // ##############
    // # LpGBT BERT #
    // ##############
    uint64_t GetBERTErrors(Ph2_HwDescription::Chip* pChip);

    std::map<uint8_t, uint8_t> fGroup2BERTsourceCourse      = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}};
    std::map<uint8_t, uint8_t> fChannelSpeed2BERTsourceFine = {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {6, 5}, {8, 6}}; // channel + 4 * (2 - frontendSpeed)

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

    std::map<uint8_t, std::string> fPUSMStatusMap = {{0, "ARESET"},
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
                                                     {18, "READY"}};

    std::map<std::string, uint8_t> revertedPUSMStatusMap;
};
} // namespace Ph2_HwInterface
#endif
