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
    bool SwitchOnSEH();
    // R/W functions using register name
    bool     WriteChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true) override;
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode) override;
    // R/W functions using register address
    bool     WriteReg(Ph2_HwDescription::Chip* pChip, uint16_t pAddress, uint16_t pValue, bool pVerifLoop = true);
    uint16_t ReadReg(Ph2_HwDescription::Chip* pChip, uint16_t pAddress);
    bool     WriteChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& RegVec, bool pVerifLoop = true) override;

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

#endif
    // Sets the flag used to select which lpGBT configuration interface to use
    void SetConfigMode(Ph2_HwDescription::Chip* pChip, bool pUseOpticalLink, bool pUseCPB, bool pToggleTC = false);
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

  private:
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
