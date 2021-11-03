/*!

        \file                                            CICInterface.h
        \brief                                           User Interface to the Cics
        \version                                         1.0

 */

#ifndef __CICINTERFACE_H__
#define __CICINTERFACE_H__
#include "ChipInterface.h"
#include "D19clpGBTInterface.h"

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
    void CheckConfig(Ph2_HwDescription::Chip* pChip);

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

    std::pair<bool, uint16_t>         ReadChipRegItem(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem pRegItem);
    bool                              SetOptimalTap(Ph2_HwDescription::Chip* pChip, uint8_t pPhyPort, uint8_t pPhyPortChannel, int pOffset = 0);
    bool                              SetOptimalTaps(Ph2_HwDescription::Chip* pChip, int pOffset = 0);
    uint8_t                           GetOptimalTap(Ph2_HwDescription::Chip* pChip, uint8_t pPhyPortChannel, uint8_t pInput);
    std::vector<uint8_t>              GetOptimalTaps(Ph2_HwDescription::Chip* pChip, uint8_t pFEId);
    std::vector<std::vector<uint8_t>> GetOptimalTaps(Ph2_HwDescription::Chip* pChip);
    bool                              SetSparsification(Ph2_HwDescription::Chip* pChip, uint8_t pState = 0);
    bool                              PhaseAlignerPorts(Ph2_HwDescription::Chip* pChip, uint8_t pState);
    bool                              SetStaticPhaseAlignment(Ph2_HwDescription::Chip* pChip);
    // bool                              SetStaticPhaseAlignment(Ph2_HwDescription::Chip* pChip, uint8_t pFeId , uint8_t pLineId , uint8_t pPhase );
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
    bool                              StartUp(Ph2_HwDescription::Chip* pChip, uint8_t pDriveStrength = 7, uint8_t pUseNegEdge = 1);
    bool                              ManualBx0Alignment(Ph2_HwDescription::Chip* pChip, uint8_t pBx0delay = 8);
    std::vector<std::vector<uint8_t>> GetWordAlignmentValues(Ph2_HwDescription::Chip* pChip);
    bool                              SelectMode(Ph2_HwDescription::Chip* pChip, uint8_t pMode = 0);
    bool                              SelectOutput(Ph2_HwDescription::Chip* pChip, bool pFixedPattern = true);
    bool                              EnableFEs(Ph2_HwDescription::Chip* pChip, std::vector<uint8_t> pFEs = {0, 1, 2, 3, 4, 5, 6, 7}, bool pEnable = true);
    bool                              PhaseAlignerStatus(Ph2_HwDescription::Chip* pChip, std::vector<std::vector<uint8_t>>& pPhaseTaps, std::vector<std::vector<uint8_t>>& pPhaseTapsFEs);
    bool                              AutoBx0Alignment(Ph2_HwDescription::Chip* pChip, uint8_t pStatus);
    bool                              SelectMux(Ph2_HwDescription::Chip* pChip, uint8_t pPhyPort);
    bool                              ControlMux(Ph2_HwDescription::Chip* pChip, uint8_t pEnable);
    bool                              ConfigureStubOutput(Ph2_HwDescription::Chip* pChip, uint8_t pLineSel = 0);
    bool                              ConfigureTermination(Ph2_HwDescription::Chip* pChip, uint8_t pClkTerm = 1, uint8_t pRxTerm = 1);
    bool                              ConfigureDriveStrength(Ph2_HwDescription::Chip* pChip, uint8_t pDriveStrength=3 ); 
    bool                              ConfigureFCMDEdge(Ph2_HwDescription::Chip* pChip, uint8_t pUseNegEdge=1 );
    bool                              GetResyncRequest(Ph2_HwDescription::Chip* pChip);
    //
    bool                          runVerification(Ph2_HwDescription::Chip* pChip, uint8_t pValue, std::string pRegName);
    std::pair<uint16_t, uint16_t> getRetrySummary() { return std::make_pair(fReW, fReWR); }
    std::pair<int, float>         getWRattempts();
    std::pair<float, float>       getMinMaxWRattempts();
    std::pair<uint16_t, uint16_t> getReadBackErrorSummary() { return std::make_pair(fReadBackErrors, fRegisterWrites); }
    std::pair<uint16_t, uint16_t> getWriteErrorSummary() { return std::make_pair(fWriteErrors, fRegisterWrites); }
    std::pair<uint16_t, uint16_t> getConfigSumary() { return std::make_pair(fSuccRegisterWrites, fSuccRegisterRbs); }
    void                          resetStatusLog() { fI2CStatus.clear(); }
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
    }
    void printErrorSummary();
    void setRetryI2C(bool pRetry) { fRetryI2C = pRetry; }
    void setMaxI2CAttempts(uint8_t pMaxAttempts) { fMaxI2CAttempts = pMaxAttempts; }
    // return information on phase aligners
    std::vector<std::bitset<6>> getFeStates() { return fFeStates; }
    std::vector<std::bitset<4>> getPortStates() { return fPortStates; }
    std::vector<uint8_t>        getI2CStatus() { return fI2CStatus; }
    void                        setWith8CBC3(bool cIsWith8CBC3) { fWith8CBC3 = cIsWith8CBC3; }
    void                        setWithlpGBT(uint8_t pIsWithLpGBT) {}

  private:
    bool    fWith8CBC3      = false;
    bool    fRetryI2C       = true;
    uint8_t fMaxI2CAttempts = 20;

    bool                           WriteReg(Ph2_HwDescription::Chip* pCic, uint8_t pRegisterAddress, uint8_t pRegisterValue, bool pVerifLoop = true);
    bool                           WriteRegs(Ph2_HwDescription::Chip* pCic, const std::vector<std::pair<uint8_t, uint8_t>> pRegs, bool pVerifLoop = true);
    std::map<uint8_t, std::string> fMap;
    std::map<uint8_t, uint16_t>    fReWMap;
    std::map<uint8_t, uint16_t>    fReWrMap;
    std::map<uint8_t, uint16_t>    fWriteErrorMap;
    std::map<uint8_t, uint16_t>    fReadBackErrorMap;
    std::vector<uint8_t>           fI2CStatus;

    uint16_t fAttemptedWrites    = 0;
    uint16_t fSuccRegisterWrites = 0;
    uint16_t fSuccRegisterRbs    = 0;
    uint16_t fRegisterWrites     = 0;
    uint16_t fReadBackErrors     = 0;
    uint16_t fWriteErrors        = 0;
    uint16_t fReW                = 0;
    uint16_t fReWR               = 0;

    std::vector<uint8_t> getMapping(Ph2_HwDescription::Chip* pChip)
    {
        std::string          cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "CBCMPA_SEL" : "FE_CONFIG";
        auto                 cFeType    = this->ReadChipReg(pChip, cRegName);
        bool                 c2S        = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cFeType == 0) : ((cFeType & 0x01) == 0);
        std::vector<uint8_t> cFeMapping = c2S ? fFeMapping2S : fFeMappingPSR;
        if(!c2S)
            cFeMapping = (pChip->getHybridId() % 2 == 0) ? fFeMappingPSR : fFeMappingPSL;
        else if(fWith8CBC3)
            cFeMapping = fFeMapping8BC3;
        return cFeMapping;
    }

  protected:
    std::vector<uint8_t> fFeMapping2S{0, 1, 2, 3, 7, 6, 5, 4};    // Index CIC FE Id , Value Hybrid FE Id
    std::vector<uint8_t> fFeMapping8BC3{3, 2, 1, 0, 4, 5, 6, 7};  // Index CIC FE Id , Value Hybrid FE Id
    std::vector<uint8_t> fFeMapping8CBC3{3, 2, 1, 0, 4, 5, 6, 7}; // Index CIC FE Id , Value Hybrid FE Id
    std::vector<uint8_t> fFeMappingPSR{6, 7, 3, 2, 1, 0, 4, 5};   // Index hybrid FE Id , Value CIC FE Id
    std::vector<uint8_t> fFeMappingPSL{1, 0, 4, 5, 6, 7, 3, 2};   // Index hybrid FE Id , Value CIC FE Id

    void                         UpdateExternalWordAlignmentValues(Ph2_HwDescription::Chip* pChip);
    bool                         ConfigureExternalWordAlignment(Ph2_HwDescription::Chip* pChip);
    bool                         ReadOptimalTap(Ph2_HwDescription::Chip* pChip, uint8_t pPhyPortChannel, std::vector<std::vector<uint8_t>>& pPhaseTaps);
    std::map<uint8_t, uint8_t>   fTxDriveStrength  = {{0, 0}, {1, 2}, {2, 6}, {3, 1}, {4, 3}, {5, 7}};
    uint8_t                      fMaxDriveStrength = 5;
    std::vector<std::bitset<24>> fPhaseValues;
    // 4 channels per phyPort ... 12 phyPorts per CIC
    std::vector<std::vector<uint8_t>> fPhaseTaps;
    std::vector<std::bitset<6>>       fFeStates;
    std::vector<std::bitset<4>>       fPortStates;
    std::vector<std::vector<uint8_t>> fWordAlignmentVals;

    // register map
};
} // namespace Ph2_HwInterface

#endif
