/*!

        \file                                            GbtInterface.h
        \brief                                           User Interface to the Cics
        \version                                         1.0

 */

#ifndef __GbtInterface_H__
#define __GbtInterface_H__

#include "BeBoardFWInterface.h"

/*!
 * \namespace Ph2_HwInterface
 * \brief Namespace regrouping all the interfaces to the hardware
 */

// add break codes here

namespace Ph2_HwInterface
{
using SCAI2C = std::pair<uint32_t, uint32_t>;

/*!
 * \class GbtInterface
 * \brief Class representing the User Interface to the Cic on different boards
 */
class GbtInterface
{
  public:
    /*!
     * \brief Constructor of the GbtInterface Class
     * \param pBoardMap
     */
    GbtInterface();
    /*!
     * \brief Destructor of the GbtInterface Class
     */
    ~GbtInterface();

    // GBT specific functions
    // Slow contron mux
    void selectMux(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pLinkId, uint32_t cWait_ms = 100);

    // SCA - enable I2C master interfaces, GPIO, ADC
    uint8_t scaEnable(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint16_t cI2Cmaster = 0x00);
    void    scaConfigure(Ph2_HwInterface::BeBoardFWInterface* pInterface);
    bool    scaSetGPIO(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t cChannel, uint8_t cLevel);
    uint8_t scaStatus(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pMaster);
    // configure gpio [sca]
    void     scaConfigureGPIO(Ph2_HwInterface::BeBoardFWInterface* pInterface);
    uint16_t readAdcChn(Ph2_HwInterface::BeBoardFWInterface* pInterface, std::string pValueToRead, bool pConvertRawReading = false);
    uint32_t readAdcCalibration(Ph2_HwInterface::BeBoardFWInterface* pInterface);
    float    convertAdcReading(uint16_t pReading, std::string pValueToRead);

    // GBTx configuration
    void gbtxSelectEdgeTx(Ph2_HwInterface::BeBoardFWInterface* pInterface, bool pRising = true);
    void gbtxSelectEdge(Ph2_HwInterface::BeBoardFWInterface* pInterface, bool pRising = true);
    void gbtxSetPhase(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pPhase = 11);
    void gbtxConfigureChargePumps(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pStrength = 0x04);
    // set clocks
    void gbtxSetClocks(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pFrequency = 3, uint8_t cDriveStrength = 0x0a, uint8_t cCoarsePhase = 0, uint8_t cFinePhase = 0);
    // read clocks
    void gbtxReadClocks(Ph2_HwInterface::BeBoardFWInterface* pInterface);
    // reset clock plls
    void gbtxResetPhaseShifterClocks(Ph2_HwInterface::BeBoardFWInterface* pInterface);
    // configure e-links
    // 0 -- disable, 1 -- 80 Mb/s , 2 -- 160 Mbps , 3 -- 320 Mbps
    void gbtxConfigureTxMode(Ph2_HwInterface::BeBoardFWInterface* pInterface, std::vector<uint8_t> cGroups, uint8_t pDataRate = 0);
    void gbtxConfigureTxClocks(Ph2_HwInterface::BeBoardFWInterface* pInterface, std::vector<uint8_t> cGroups, uint8_t pDataRate = 0);
    void gbtxConfigureRxClocks(Ph2_HwInterface::BeBoardFWInterface* pInterface, std::vector<uint8_t> cGroups, uint8_t pDataRate = 0);
    void gbtxEnableTxChannel(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pGroup, std::vector<uint8_t> pChannels);
    void gbtxEnableRxChannel(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pGroup, std::vector<uint8_t> pChannels);
    void gbtxConfigureLinks(Ph2_HwInterface::BeBoardFWInterface* pInterface);
    void gbtxDisableAllLinks(Ph2_HwInterface::BeBoardFWInterface* pInterface);
    void gbtxResetFrameAlignerDLL(Ph2_HwInterface::BeBoardFWInterface* pInterface, std::vector<uint8_t> pGroups);
    void gbtxFrameAlignerDLL(Ph2_HwInterface::BeBoardFWInterface* pInterface, std::vector<uint8_t> pGroups, uint8_t pDLLcurrent = 11, uint8_t pLockMode = 7);
    void gbtxConfigure(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pDLLcurrent = 11, uint8_t pDLLlockMode = 7);
    void gbtxSelectTerminationRx(Ph2_HwInterface::BeBoardFWInterface* pInterface, bool pEnable = true);
    void gbtxSetDriveStrength(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pStrength = 0xA);

    struct RegConfig
    {
        uint8_t fFeId            = 0;
        uint8_t fChipId          = 0;
        uint8_t fPage            = 0;
        uint8_t fRegisterAddress = 0;
        uint8_t fRegisterValue   = 0;
    };
    // read and write functions for CIC and CBC
    uint8_t  cbcGetPageRegister(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pFeId, uint8_t pChipId);
    uint8_t  cbcSetPage(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pFeId, uint8_t pChipId, uint8_t pPage);
    uint32_t cbcRead(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pFeId, uint8_t pChipId, uint8_t pPage, uint8_t pRegisterAddress);
    bool     cbcWrite(Ph2_HwInterface::BeBoardFWInterface* pInterface,
                      uint8_t                              pFeId,
                      uint8_t                              pChipId,
                      uint8_t                              pPage,
                      uint8_t                              pRegisterAddress,
                      uint8_t                              pRegisterValue,
                      bool                                 pReadBack = true,
                      bool                                 pSetPage  = false);
    uint32_t cicRead(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pFeId, uint8_t pRegisterAddress);
    bool     cicWrite(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint8_t pFeId, uint8_t pRegisterAddress, uint8_t pRegisterValue, bool pReadBack = true);

    // multi-register write
    // bool cbcWrite(Ph2_HwInterface::BeBoardFWInterface* pInterface, const std::vector<uint32_t>& pVecSend);
    bool i2cWrite(Ph2_HwInterface::BeBoardFWInterface* pInterface, const std::vector<uint32_t>& pVecSend, std::vector<uint32_t>& pReplies, bool pReadBack = true);

  private:
    // GBTX ec
    void     ecReset(Ph2_HwInterface::BeBoardFWInterface* pInterface);
    uint32_t ecWrite(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint16_t pI2Cmaster, uint32_t pCommand, uint32_t pData = 0x00);
    uint32_t ecWrite(BeBoardFWInterface* pInterface, uint16_t pI2Cmaster, const std::vector<std::pair<uint32_t, uint32_t>>& pCommands);
    uint32_t ecRead(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint16_t pI2Cmaster, uint32_t pCommand, uint32_t pData = 0x00);
    // GBTx ic
    void     icReset(Ph2_HwInterface::BeBoardFWInterface* pInterface);
    void     icWrite(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint32_t pAddress, uint32_t pData);
    uint32_t icRead(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint32_t pAddress, uint32_t pNwords);
    // Temp replacement for new CPB
    uint8_t  configI2C(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint16_t pMaster, uint8_t pNBytes = 2, uint8_t pSclMode = 0, int pFrequency = 1000);
    uint32_t readI2C(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint16_t pMaster, uint8_t pSlave, uint8_t pNBytes);
    uint8_t  writeI2C(Ph2_HwInterface::BeBoardFWInterface* pInterface, uint16_t pMaster, uint8_t pSlave, uint32_t pData, uint8_t pNBytes);

    // Thermistor readout conversion
    float convAdcToTemp(float pAdcValue, std::string pThermistor);

  protected:
    uint8_t                                          fGBTxAddress  = 0x01;
    uint16_t                                         fSCAMaster    = 0x11; // I2C serial masters 14 (0x11) and 15 (0x12) are used on the SEH v3.1
    std::map<std::string, uint8_t>                   fScaAdcChnMap = {{"AMUX_L", 0}, {"AMUX_R", 30}, {"VMIN", 14}, {"VM1V5", 21}, {"VM2V5", 27}, {"VRSSI", 24}, {"EXT_TEMP", 25}, {"INT_TEMP", 31}};
    std::map<std::string, std::pair<int, int>>       fScaAdcVoltageDeviderMap = {{"AMUX_L", std::make_pair(0, 1)},
                                                                           {"AMUX_R", std::make_pair(0, 1)},
                                                                           {"VMIN", std::make_pair(91000, 4700)},
                                                                           {"VM1V5", std::make_pair(100000, 110000)},
                                                                           {"VM2V5", std::make_pair(100, 47)},
                                                                           {"VRSSI", std::make_pair(0, 1)},
                                                                           {"EXT_TEMP", std::make_pair(0, 10000)},
                                                                           {"INT_TEMP", std::make_pair(0, 1)}};
    std::map<std::string, std::tuple<int, int, int>> fNTCThermistorMap        = {{"NCP15XM331J03RC", std::make_tuple(298, 330, 3500)}}; // tuple contains <refTemp, resistance at refTemp, BConstant>
};
} // namespace Ph2_HwInterface

#endif
