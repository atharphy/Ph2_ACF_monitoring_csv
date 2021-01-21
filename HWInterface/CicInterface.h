/*!

        \file                                            CICInterface.h
        \brief                                           User Interface to the Cics
        \version                                         1.0

 */

#ifndef __CICINTERFACE_H__
#define __CICINTERFACE_H__

#include "ChipInterface.h"

/*!
 * \namespace Ph2_HwInterface
 * \brief Namespace regrouping all the interfaces to the hardware
 */
namespace Ph2_HwInterface
{
/*!
 * \class CicInterface
 * \brief Class representing the User Interface to the Cic on different boards
 */
class CicInterface : public ChipInterface
{
  public:
    /*!
     * \brief Constructor of the CICInterface Class
     * \param pBoardMap
     */
    CicInterface(const BeBoardFWMap& pBoardMap);
    /*!
     * \brief Destructor of the CICInterface Class
     */
    ~CicInterface();
    /*!
     * \brief Configure the Cic with the Cic Config File
     * \param pCic: pointer to CIC object
     * \param pVerifLoop: perform a readback check
     * \param pBlockSize: the number of registers to be written at once, default is 310
     */
    bool ConfigureChip(Ph2_HwDescription::Chip* pCic, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;

    /*!
     * \brief Write the designated register in both Chip and Chip Config File
     * \param pChip
     * \param pRegNode : Node of the register to write
     * \param pValue : Value to write
     */
    bool WriteChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true) override;

    /*!
     * \brief Write several registers in both Chip and Chip Config File
     * \param pChip
     * \param pVecReq : Vector of pair: Node of the register to write versus value to write
     */
    bool WriteChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop = true) override;

    /*!
     * \brief Read the designated register in the Chip
     * \param pChip
     * \param pRegNode : Node of the register to read
     */
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode) override;

    // CIC specific functions
    std::pair<bool, uint16_t>         ReadChipReg(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem pRegItem);
    std::vector<std::vector<uint8_t>> GetOptimalTaps(Ph2_HwDescription::Chip* pChip);
    bool                              SetSparsification(Ph2_HwDescription::Chip* pChip, uint8_t pState = 0);
    bool                              PhaseAlignerPorts(Ph2_HwDescription::Chip* pChip, uint8_t pState);
    bool                              SetStaticPhaseAlignment(Ph2_HwDescription::Chip* pChip, uint8_t pFeId = 0, uint8_t pLineId = 0, uint8_t pPhase = 0);
    bool                              SetStaticPhaseAlignment(Ph2_HwDescription::Chip* pChip, std::vector<std::vector<uint8_t>> pPhaseTaps);
    bool                              SetAutomaticPhaseAlignment(Ph2_HwDescription::Chip* pChip, bool pAuto = true);
    bool                              SetStaticWordAlignment(Ph2_HwDescription::Chip* pChip, uint8_t pValue = 5);
    bool                              CheckPhaseAlignerLock(Ph2_HwDescription::Chip* pChip, uint8_t pCheckValue = 0xFF);
    bool                              ResetPhaseAligner(Ph2_HwDescription::Chip* pChip, uint16_t pWait_ms = 100);
    bool                              ResetDLL(Ph2_HwDescription::Chip* pChip, uint16_t pWait_ms = 100);
    bool                              CheckDLL(Ph2_HwDescription::Chip* pChip);
    bool                              CheckFastCommandLock(Ph2_HwDescription::Chip* pChip);
    bool                              ConfigureAlignmentPatterns(Ph2_HwDescription::Chip* pChip, std::vector<uint8_t> pAlignmentPatterns);
    bool                              AutomatedWordAlignment(Ph2_HwDescription::Chip* pChip, std::vector<uint8_t> pAlignmentPatterns, int pWait_ms = 1000);
    bool                              ConfigureBx0Alignment(Ph2_HwDescription::Chip* pChip, std::vector<uint8_t> pPatterns, uint8_t pFEId = 0, uint8_t pLineId = 0);
    std::pair<bool, uint8_t>          CheckBx0Alignment(Ph2_HwDescription::Chip* pChip); // success , delay
    bool                              CheckReSync(Ph2_HwDescription::Chip* pChip);
    bool                              SoftReset(Ph2_HwDescription::Chip* pChip, uint32_t cWait_ms = 100);
    bool                              CheckSoftReset(Ph2_HwDescription::Chip* pChip);
    bool                              StartUp(Ph2_HwDescription::Chip* pChip, uint8_t pDriveStrength = 7);
    bool                              ManualBx0Alignment(Ph2_HwDescription::Chip* pChip, uint8_t pBx0delay = 8);
    std::vector<std::vector<uint8_t>> ReadWordAlignmentValues(Ph2_HwDescription::Chip* pChip);
    bool                              SelectMode(Ph2_HwDescription::Chip* pChip, uint8_t pMode = 0);
    bool                              SelectOutput(Ph2_HwDescription::Chip* pChip, bool pFixedPattern = true);
    bool                              EnableFEs(Ph2_HwDescription::Chip* pChip, std::vector<uint8_t> pFEs = {0, 1, 2, 3, 4, 5, 6, 7}, bool pEnable = true);
    bool                              PhaseAlignerStatus(Ph2_HwDescription::Chip* pChip, std::vector<std::vector<uint8_t>>& pPhaseTaps, std::vector<std::vector<uint8_t>>& pPhaseTapsFEs);
    bool                              AutoBx0Alignment(Ph2_HwDescription::Chip* pChip, uint8_t pStatus);
    bool                              SelectMux(Ph2_HwDescription::Chip* pChip, uint8_t pPhyPort);
    bool                              ControlMux(Ph2_HwDescription::Chip* pChip, uint8_t pEnable);

    // return information on phase aligners
    std::vector<std::bitset<6>> getFeStates() { return fFeStates; }
    std::vector<std::bitset<4>> getPortStates() { return fPortStates; }

  protected:
    bool                        ReadOptimalTap(Ph2_HwDescription::Chip* pChip, uint8_t pPhyPortChannel, std::vector<std::vector<uint8_t>>& pPhaseTaps);
    std::map<uint8_t, uint8_t>  fTxDriveStrength  = {{0, 0}, {1, 2}, {2, 6}, {3, 1}, {4, 3}, {5, 7}};
    uint8_t                     fMaxDriveStrength = 5;
    std::vector<std::bitset<6>> fFeStates;
    std::vector<std::bitset<4>> fPortStates;
};
} // namespace Ph2_HwInterface

#endif
