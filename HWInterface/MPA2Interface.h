/*!

        \file                   MPA2.h
        \brief                  MPA2 Description class, config of the MPA2s
        \author                 Kevin Nash
        \version                1.0
        \date                   12/06/21
        Support :               mail to : knash201@gmail.com

 */

#ifndef __MPA2INTERFACE_H__
#define __MPA2INTERFACE_H__

#include "HWInterface/BeBoardFWInterface.h"
#include "HWInterface/D19clpGBTInterface.h"
#include "HWInterface/MPAInterface.h"
#include "HWInterface/ReadoutChipInterface.h"
#include "pugixml.hpp"
#include <vector>

/*!
 * \namespace Ph2_HwInterface
 * \brief Namespace regrouping all the interfaces to the hardware
 */
namespace Ph2_HwInterface
{
using BeBoardFWMap = std::map<uint16_t, BeBoardFWInterface*>; /*!< Map of Board connected */

/*!
 * \class MPA2Interface
 * \brief Class representing the User Interface to the MPA on different boards
 */

class MPA2Interface : public ReadoutChipInterface
{ // begin class
  public:
    MPA2Interface(const BeBoardFWMap& pBoardMap);
    ~MPA2Interface();

    void     setFileHandler(FileHandler* pHandler);
    bool     ConfigureChip(Ph2_HwDescription::Chip* pMPA, bool pVerify = false, uint32_t pBlockSize = 310) override;
    uint32_t ReadData(Ph2_HwDescription::BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait);
    void     ReadMPA(Ph2_HwDescription::ReadoutChip* pMPA);

    bool WriteChipRegBits(Ph2_HwDescription::Chip* pMPA, const std::string& pRegNode, uint16_t pValue, const std::string& pMaskReg, uint8_t mask, bool pVerify = true);
    bool WriteChipReg(Ph2_HwDescription::Chip* pMPA, const std::string& pRegName, uint16_t pValue, bool pVerify = true) override;
    bool WriteChipMultReg(Ph2_HwDescription::Chip* pMPA, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerify = false) override;
    bool WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pMPA, const std::string& dacName, const ChipContainer& pValue, bool pVerify = false) override;

    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pMPA, const std::string& pRegName) override;

    void producePhaseAlignmentPattern(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pWait_ms = 10) override;
    void produceWordAlignmentPattern(Ph2_HwDescription::ReadoutChip* pChip) override;

    void     Pix_write(Ph2_HwDescription::ReadoutChip* cMPA, Ph2_HwDescription::ChipRegItem cRegItem, uint32_t row, uint32_t pixel, uint32_t data);
    uint32_t Pix_read(Ph2_HwDescription::ReadoutChip* cMPA, Ph2_HwDescription::ChipRegItem cRegItem, uint32_t row, uint32_t pixel);

    // void                  Activate_async(Ph2_HwDescription::Chip* pMPA);
    // void                  Activate_sync(Ph2_HwDescription::Chip* pMPA);
    void Activate_pp(Ph2_HwDescription::Chip* pMPA, uint8_t win = 0);
    void Activate_ss(Ph2_HwDescription::Chip* pMPA, uint8_t win = 0);
    void Activate_ps(Ph2_HwDescription::Chip* pMPA, uint8_t win = 8);
    // uint32_t Read_pixel_counter(Ph2_HwDescription::ReadoutChip* pMPA, uint32_t p);
    void ReadASEvent(Ph2_HwDescription::ReadoutChip* pMPA, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange = std::pair<uint32_t, uint32_t>({0, 0}));
    void Pix_Smode(Ph2_HwDescription::ReadoutChip* pMPA, uint32_t p, std::string smode);
    void Enable_pix_BRcal(Ph2_HwDescription::ReadoutChip* pMPA, uint32_t p, std::string polarity = "rise", std::string smode = "edge");
    void Pix_Set_enable(Ph2_HwDescription::ReadoutChip* pMPA,
                        uint32_t                        p,
                        uint32_t                        PixelMask,
                        uint32_t                        Polarity,
                        uint32_t                        EnEdgeBR,
                        uint32_t                        EnLevelBR,
                        uint32_t                        Encount,
                        uint32_t                        DigCal,
                        uint32_t                        AnCal,
                        uint32_t                        BRclk);

    bool Set_calibration(Ph2_HwDescription::Chip* pMPA, uint32_t cal);
    bool Set_threshold(Ph2_HwDescription::Chip* pMPA, uint32_t th);

    void Send_pulses(uint32_t n_pulse, uint32_t duration = 0);
    bool enableInjection(Ph2_HwDescription::ReadoutChip* pChip, bool inject, bool pVerify = false);

    bool maskChannelGroup(Ph2_HwDescription::ReadoutChip* pMPA, const std::shared_ptr<ChannelGroupBase> group, bool pVerify = false) override;
    //
    bool maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerify = false);

    bool setInjectionSchema(Ph2_HwDescription::ReadoutChip* pCbc, const std::shared_ptr<ChannelGroupBase> group, bool pVerify = false);

    bool ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pMPA, bool pVerify, uint32_t pBlockSize);
    //
    bool MaskAllChannels(Ph2_HwDescription::ReadoutChip* pMPA, bool mask, bool pVerify) { return true; }

    std::vector<uint8_t> getWordAlignmentPatterns() override { return fWordAlignmentPatterns; }
    void                 Cleardata();
    //
    void                 digiInjection(Ph2_HwDescription::ReadoutChip* pChip, std::vector<Injection> pInjections, uint8_t pPattern = 0xFF);
    std::vector<int>     decodeBendCode(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pBendCode);
    std::vector<uint8_t> readLUT(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pMode = 0);
    bool                 configPixel(Ph2_HwDescription::Chip* pChip, std::string cReg, int pPixelNum, uint8_t pValue, bool pVerify = false);
    uint16_t             readPixel(Ph2_HwDescription::Chip* pChip, std::string cReg, int pPixelNum);
    bool                 configRow(Ph2_HwDescription::Chip* pChip, std::string cReg, int pRowNum, uint8_t pValue, bool pVerify = false);
    bool                 configPeri(Ph2_HwDescription::Chip* pChip, std::string cReg, uint8_t pValue, bool pVerify = false);
    uint16_t             readPeri(Ph2_HwDescription::Chip* pChip, std::string cReg);
    void                 loadVref(Ph2_HwDescription::Chip* pMPA2);
    float                ADCMeasure(Ph2_HwDescription::Chip* pMPA2, uint32_t nreads = 5);
    bool                 selectBlock(Ph2_HwDescription::Chip* pMPA2, uint8_t block, uint8_t testPoint = 0, uint8_t swEn = 0);
    float                measureGnd(Ph2_HwDescription::Chip* pMPA2);
    float                measureBg(Ph2_HwDescription::Chip* pMPA2);
    uint16_t             ReadADC(Ph2_HwDescription::ReadoutChip* pChip, std::string pRegName);

    float calculateADCLSB(Ph2_HwDescription::Chip* pMPA2, float vrefExp = 0.850);

  private:
    // pixelEnable bits
    const std::map<std::string, uint8_t> PIXEL_ENABLE_TABLE =
        {{"PixelMask", 0}, {"Polarity", 1}, {"EnEdgeBR", 2}, {"EnLvlBR", 3}, {"CounterEnable", 4}, {"DigitalInjection", 5}, {"AnalogueInjection", 6}, {"BrClk", 7}};
    const std::map<std::string, uint8_t> ECM_TABLE = {{"StubWindow", 0}, {"StubMode", 6}};

    const std::map<std::string, uint8_t> CONTROL_TABLE = {{"ReadoutMode", 0}, {"RetimePix", 2}, {"PhaseShift", 5}}; // I think the doc should read 3,3,2 for the bits -- to check

    // MPA2 ADC Multiplexer control 
    // < register name , < block for AMUX selection, switch sel (shift) for analog bias >>
    typedef std::pair<uint8_t, uint8_t> block_switch;
    const std::map<std::string, block_switch> ADC_CONTROL_TABLE = {
        {"disabled", std::make_pair(0, 0)}, // not sure on the shift selection when we are not looking at a bias
        {"A0", std::make_pair(1, 0)},           {"A1", std::make_pair(2, 0)},      {"A2", std::make_pair(3, 0)},      {"A3", std::make_pair(4, 0)},      {"A4", std::make_pair(5, 0)},
        {"A5", std::make_pair(6, 0)},           {"A6", std::make_pair(7, 0)},      {"B0", std::make_pair(1, 1)},      {"B1", std::make_pair(2, 1)},
        {"B2", std::make_pair(3, 1)},           {"B3", std::make_pair(4, 1)},      {"B4", std::make_pair(5, 1)},      {"B5", std::make_pair(6, 1)},      {"B6", std::make_pair(7, 1)},
        {"C0", std::make_pair(1, 2)},           {"C1", std::make_pair(2, 2)},      {"C2", std::make_pair(3, 2)},      {"C3", std::make_pair(4, 2)},
        {"C4", std::make_pair(5, 2)},           {"C5", std::make_pair(6, 2)},      {"C6", std::make_pair(7, 2)},      {"D0", std::make_pair(1, 3)},      {"D1", std::make_pair(2, 3)},
        {"D2", std::make_pair(3, 3)},           {"D3", std::make_pair(4, 3)},      {"D4", std::make_pair(5, 3)},      {"D5", std::make_pair(6, 3)},      {"D6", std::make_pair(7, 3)},
        {"E0", std::make_pair(1, 4)},           {"E1", std::make_pair(2, 4)},      {"E2", std::make_pair(3, 4)},      {"E3", std::make_pair(4, 4)},
        {"E4", std::make_pair(5, 4)},           {"E5", std::make_pair(6, 4)},      {"E6", std::make_pair(7, 4)},
        {"ThDAC0", std::make_pair(1, 5)},       {"ThDAC1", std::make_pair(2, 5)},  {"ThDAC2", std::make_pair(3, 5)},  {"ThDAC3",  std::make_pair(4, 5)},  {"ThDAC4", std::make_pair(5, 5)},
        {"ThDAC5", std::make_pair(6, 5)},       {"ThDAC6", std::make_pair(7, 5)},  {"CalDAC0", std::make_pair(1, 6)}, {"CalDAC1", std::make_pair(2, 6)},
        {"CalDAC2", std::make_pair(3, 6)},      {"CalDAC3", std::make_pair(4, 6)}, {"CalDAC4", std::make_pair(5, 6)}, {"CalDAC5", std::make_pair(6, 6)}, {"CalDAC6", std::make_pair(7, 6)},
        {"GND", std::make_pair(1, 7)},     {"VBG", std::make_pair(8, 0)}, {"dac_ref", std::make_pair(9, 0)}, {"vref", std::make_pair(10, 0)},
        {"temperature", std::make_pair(11, 0)}, {"avdd", std::make_pair(12, 0)},   {"io_vdd", std::make_pair(13, 0)}, {"dvdd", std::make_pair(14, 0)}};

    // MPA2 periphery config register map
    const std::map<std::string, std::pair<uint8_t, uint8_t>> PERI_CONFIG_TABLE = {{"Control_1", std::pair<uint8_t, uint8_t>{0x11, 0}}, // MPA2 has 0x11 and 0x12 peri blocks
                                                                                  {"ECM", std::pair<uint8_t, uint8_t>{0x11, 1}},
                                                                                  {"ErrorL1", std::pair<uint8_t, uint8_t>{0x12, 0}},
                                                                                  {"LatencyRx320", std::pair<uint8_t, uint8_t>{0x11, 24}},
                                                                                  {"OutSetting_1_0", std::pair<uint8_t, uint8_t>{0x11, 12}},
                                                                                  {"OutSetting_2_1", std::pair<uint8_t, uint8_t>{0x11, 13}},
                                                                                  {"OutSetting_4_3", std::pair<uint8_t, uint8_t>{0x11, 14}}};

    // MPA2 row config register map -- some overlap in naming, to find a better way
    const std::map<std::string, uint8_t> ROW_CONFIG_TABLE = {{"MemoryControl_1", 0}, {"MemoryControl_2", 1}, {"PixelControl", 2}, {"Mask", 13}, {"L1Offset_1", 0}, {"L1Offset_2", 1}};

    // MPA2 pixel config register map
    const std::map<std::string, uint8_t> PIXEL_CONFIG_TABLE = {{"ENFLAGS", 0}, {"TrimDAC", 1}, {"DigiPattern", 2}, {"ACCounter_LSB", 4}, {"ACCounter_MSB", 5}};
    void                                 readFuseID(Ph2_HwDescription::Chip* pMPA2);
    std::map<uint16_t, std::string>      fMap;
    std::vector<uint8_t>                 fWordAlignmentPatterns = {0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A};

    bool     WriteReg(Ph2_HwDescription::Chip* pMPA, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerify = false);
    bool     WriteRegs(Ph2_HwDescription::Chip* pMPA, const std::vector<std::pair<uint16_t, uint16_t>> pRegs, bool pVerify = false);
    bool     WriteChipSingleReg(Ph2_HwDescription::Chip* pMPA, const std::string& pRegNode, uint16_t pValue, bool pVerify = false);
    uint16_t ReadReg(Ph2_HwDescription::Chip* pMPA, uint16_t pRegisterAddress, bool pVerify = false);
    bool     maskPixel(Ph2_HwDescription::Chip* pChip, int pPixelNum, uint8_t pMask, bool pVerify = false);
    bool     enablePixelInjection(Ph2_HwDescription::Chip* pChip, int pPixelNum, uint8_t pInj, bool pVerify = false);
    bool     maskRowCol(Ph2_HwDescription::Chip* pChip, int pRow, int pColumn, uint8_t pMask, bool pVerify = false);
    uint16_t regPixel(Ph2_HwDescription::Chip* pChip, int pBaseRegister, int pRow, int pColumn);
    uint16_t regPeri(Ph2_HwDescription::Chip* pChip, int cBlock, int pBaseRegister);
    uint16_t regRow(Ph2_HwDescription::Chip* pChip, int pBaseRegister, int pRow);
    void     readAllBias(Ph2_HwDescription::Chip* pMPA);
};
} // namespace Ph2_HwInterface

#endif
