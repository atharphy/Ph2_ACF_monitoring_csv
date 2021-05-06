/*!

        \file                                            MPAInterface.h
        \brief                                           User Interface to the MPAs
        \author                                          Lorenzo BIDEGAIN, Nicolas PIERRE
        \version                                         1.0
        \date                        31/07/14
        Support :                    mail to : lorenzo.bidegain@gmail.com, nico.pierre@icloud.com

 */

#ifndef __MPAINTERFACE_H__
#define __MPAINTERFACE_H__

#include "BeBoardFWInterface.h"
#include "D19clpGBTInterface.h"
#include "ReadoutChipInterface.h"
#include "pugixml.hpp"
#include <vector>

// pixelEnable bits
const std::map<std::string, uint8_t> PIXEL_ENABLE_TABLE =
    {{"PixelMask", 0}, {"Polarity", 1}, {"EnEdgeBR", 2}, {"EnLvlBR", 3}, {"CounterEnable", 4}, {"DigitalInjection", 5}, {"AnalogueInjection", 6}, {"BrClk", 7}};
const std::map<std::string, uint8_t> ECM_TABLE = {{"StubWindow", 0}, {"StubMode", 6}};

// periphery config register map
const std::map<std::string, uint8_t> PERI_CONFIG_TABLE = {{"ReadoutMode", 0},
                                                          {"ECM", 1},
                                                          {"RetimePix", 2},
                                                          {"ErrorL1", 40},
                                                          {"LatencyRx320", 34},
                                                          {"PhaseShift", 32},
                                                          {"OutSetting_0", 14},
                                                          {"OutSetting_1", 15},
                                                          {"OutSetting_2", 16},
                                                          {"OutSetting_3", 17},
                                                          {"OutSetting_4", 18},
                                                          {"OutSetting_5", 19}};

// row config register map
const std::map<std::string, uint8_t> ROW_CONFIG_TABLE = {{"L1Offset_1", 1}, {"L1Offset_2", 2}, {"ClrRst", 3}};
// pixel config register map
const std::map<std::string, uint8_t> PIXEL_CONFIG_TABLE =
    {{"PixelEnable", 0}, {"ModelSel", 1}, {"TrimDAC", 2}, {"ClusterCut", 3}, {"HipCut", 4}, {"DigiPattern", 5}, {"ACCounter_LSB", 9}, {"ACCounter_MSB", 10}, {"SEUCounter", 11}};

/*!
 * \namespace Ph2_HwInterface
 * \brief Namespace regrouping all the interfaces to the hardware
 */
namespace Ph2_HwInterface
{
using BeBoardFWMap = std::map<uint16_t, BeBoardFWInterface*>; /*!< Map of Board connected */

/*!
 * \class MPAInterface
 * \brief Class representing the User Interface to the MPA on different boards
 */

struct Stubs
{
    std::vector<uint8_t>              nst;
    std::vector<std::vector<uint8_t>> pos;
    std::vector<std::vector<uint8_t>> row;
    std::vector<std::vector<uint8_t>> cur;
};

struct L1data
{
    uint8_t              strip_counter;
    uint8_t              pixel_counter;
    std::vector<uint8_t> pos_strip;
    std::vector<uint8_t> width_strip;
    std::vector<uint8_t> MIP;
    std::vector<uint8_t> pos_pixel;
    std::vector<uint8_t> width_pixel;
    std::vector<uint8_t> Z;
};

struct Injection
{
    uint8_t fRow;
    uint8_t fColumn;
    uint8_t fFeId;
};

class MPAInterface : public ReadoutChipInterface
{ // begin class
  public:
    MPAInterface(const BeBoardFWMap& pBoardMap);
    ~MPAInterface();

    void     setFileHandler(FileHandler* pHandler);
    bool     ConfigureChip(Ph2_HwDescription::Chip* pMPA, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    uint32_t ReadData(Ph2_HwDescription::BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait);
    void     ReadMPA(Ph2_HwDescription::ReadoutChip* pMPA);

    bool     WriteChipReg(Ph2_HwDescription::Chip* pMPA, const std::string& pRegName, uint16_t pValue, bool pVerifLoop = true) override;
    bool     WriteChipMultReg(Ph2_HwDescription::Chip* pMPA, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop = true) override;
    bool     WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pMPA, const std::string& dacName, ChipContainer& pValue, bool pVerifLoop = true) override;
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pMPA, const std::string& pRegName) override;

    void StartPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip) override {}
    void StopPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip) override {}

    void                  Pix_write(Ph2_HwDescription::ReadoutChip* cMPA, Ph2_HwDescription::ChipRegItem cRegItem, uint32_t row, uint32_t pixel, uint32_t data);
    uint32_t              Pix_read(Ph2_HwDescription::ReadoutChip* cMPA, Ph2_HwDescription::ChipRegItem cRegItem, uint32_t row, uint32_t pixel);
    void                  activate_I2C_chip();
    std::vector<uint16_t> ReadoutCounters_MPA(uint32_t raw_mode_en);
    void                  PS_Open_shutter(uint32_t duration = 0);
    void                  PS_Close_shutter(uint32_t duration = 0);
    void                  PS_Clear_counters(uint32_t duration = 0);
    void                  PS_Start_counters_read(uint32_t duration = 0);
    void                  Activate_async(Ph2_HwDescription::Chip* pMPA);
    void                  Activate_sync(Ph2_HwDescription::Chip* pMPA);
    void                  Activate_pp(Ph2_HwDescription::Chip* pMPA, uint8_t win = 0);
    void                  Activate_ss(Ph2_HwDescription::Chip* pMPA, uint8_t win = 0);
    void                  Activate_ps(Ph2_HwDescription::Chip* pMPA, uint8_t win = 8);
    void                  Enable_pix_counter(Ph2_HwDescription::ReadoutChip* pMPA, uint32_t p);
    void                  Enable_pix_sync(Ph2_HwDescription::ReadoutChip* pMPA, uint32_t p);
    void                  Disable_pixel(Ph2_HwDescription::ReadoutChip* pMPA, uint32_t p);
    void                  Enable_pix_digi(Ph2_HwDescription::ReadoutChip* pMPA, uint32_t p);
    // uint32_t Read_pixel_counter(Ph2_HwDescription::ReadoutChip* pMPA, uint32_t p);
    void readAllBias(Ph2_HwDescription::Chip* pMPA);
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

    void Set_calibration(Ph2_HwDescription::Chip* pMPA, uint32_t cal);
    void Set_threshold(Ph2_HwDescription::Chip* pMPA, uint32_t th);

    void Send_pulses(uint32_t n_pulse, uint32_t duration = 0);
    bool enableInjection(Ph2_HwDescription::ReadoutChip* pChip, bool inject, bool pVerifLoop = true);

    bool maskChannelsGroup(Ph2_HwDescription::ReadoutChip* pMPA, const ChannelGroupBase* group, bool pVerifLoop) { return true; }
    //
    bool maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool mask, bool inject, bool pVerifLoop) { return true; }
    //
    bool ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pMPA, bool pVerifLoop, uint32_t pBlockSize) { return true; }
    //
    bool MaskAllChannels(Ph2_HwDescription::ReadoutChip* pMPA, bool mask, bool pVerifLoop) { return true; }

    Stubs  Format_stubs(std::vector<std::vector<uint8_t>> rawstubs);
    L1data Format_l1(std::vector<uint8_t> rawl1, bool verbose = false);

    void Cleardata();
    void LinkLpGBT(Ph2_HwInterface::D19clpGBTInterface* pLpGBTInterface, Ph2_HwDescription::lpGBT* pLpGBT);
    //
    void                 digiInjection(Ph2_HwDescription::ReadoutChip* pChip, std::vector<Injection> pInjections, uint8_t pPattern = 0xFF);
    std::vector<int>     decodeBendCode(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pBendCode);
    std::vector<uint8_t> readLUT(Ph2_HwDescription::ReadoutChip* pChip);
    bool                 configPixel(Ph2_HwDescription::Chip* pChip, std::string cReg, int pPixelNum, uint8_t pValue, bool pVerifLoop = true);
    uint16_t             readPixel(Ph2_HwDescription::Chip* pChip, std::string cReg, int pPixelNum);
    bool                 configRow(Ph2_HwDescription::Chip* pChip, std::string cReg, int pRowNum, uint8_t pValue, bool pVerifLoop = true);
    bool                 configPeri(Ph2_HwDescription::Chip* pChip, std::string cReg, uint8_t pValue, bool pVerifLoop = true);
    uint16_t             readPeri(Ph2_HwDescription::Chip* pChip, std::string cReg);

  private:
    std::map<uint16_t, std::string> fMap;
    D19clpGBTInterface*             flpGBTInterface = nullptr;
    Ph2_HwDescription::lpGBT*       flpGBT          = nullptr;

    bool     WriteReg(Ph2_HwDescription::Chip* pMPA, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerifLoop = true);
    bool     WriteRegs(Ph2_HwDescription::Chip* pMPA, const std::vector<std::pair<uint16_t, uint16_t>> pRegs, bool pVerifLoop = true);
    bool     WriteChipSingleReg(Ph2_HwDescription::Chip* pMPA, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true);
    uint16_t ReadReg(Ph2_HwDescription::Chip* pMPA, uint16_t pRegisterAddress, bool pVerifLoop = true);
    bool     maskPixel(Ph2_HwDescription::Chip* pChip, int pPixelNum, uint8_t pMask, bool pVerifLoop);
    bool     maskRowCol(Ph2_HwDescription::Chip* pChip, int pRow, int pColumn, uint8_t pMask, bool pVerifLoop);
    uint16_t regPixel(Ph2_HwDescription::Chip* pChip, int pBaseRegister, int pRow, int pColumn);
    uint16_t regPeri(Ph2_HwDescription::Chip* pChip, int pBaseRegister);
    uint16_t regRow(Ph2_HwDescription::Chip* pChip, int pBaseRegister, int pRow);
};
} // namespace Ph2_HwInterface

#endif
