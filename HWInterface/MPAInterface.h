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
    {{"ENFLAGS", 0}, {"ModelSel", 1}, {"TrimDAC", 2}, {"ClusterCut", 3}, {"HipCut", 4}, {"DigiPattern", 5}, {"ACCounter_LSB", 9}, {"ACCounter_MSB", 10}, {"SEUCounter", 11}};

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


struct Injection
{
    uint8_t fRow;
    uint8_t fColumn;
    uint8_t fFeId;
};

const bool VERIFY_MPA = false;
class MPAInterface : public ReadoutChipInterface
{ // begin class
  public:
    MPAInterface(const BeBoardFWMap& pBoardMap);
    ~MPAInterface();

    bool ConfigureChip(Ph2_HwDescription::Chip* pMPA, bool pVerifLoop = VERIFY_MPA, uint32_t pBlockSize = 310) override;

    bool     WriteChipReg(Ph2_HwDescription::Chip* pMPA, const std::string& pRegName, uint16_t pValue, bool pVerifLoop = true) override;
    bool     WriteChipMultReg(Ph2_HwDescription::Chip* pMPA, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop = VERIFY_MPA) override;
    bool     WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pMPA, const std::string& dacName, ChipContainer& pValue, bool pVerifLoop = VERIFY_MPA) override;
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pMPA, const std::string& pRegName) override;

    void producePhaseAlignmentPattern(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pWait_ms = 10) override;
    void produceWordAlignmentPattern(Ph2_HwDescription::ReadoutChip* pChip) override;

    bool enableInjection(Ph2_HwDescription::ReadoutChip* pChip, bool inject, bool pVerifLoop = VERIFY_MPA);

    bool maskChannelGroup(Ph2_HwDescription::ReadoutChip* pMPA, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop = VERIFY_MPA);
    //
    bool maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop = VERIFY_MPA);

    bool setInjectionSchema(Ph2_HwDescription::ReadoutChip* pCbc, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop = VERIFY_MPA);

    bool ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pMPA, bool pVerifLoop, uint32_t pBlockSize);
    //
    bool MaskAllChannels(Ph2_HwDescription::ReadoutChip* pMPA, bool mask, bool pVerifLoop) { return true; }

    std::vector<uint8_t> getWordAlignmentPatterns() override { return fWordAlignmentPatterns; }
    //
    void                 digiInjection(Ph2_HwDescription::ReadoutChip* pChip, std::vector<Injection> pInjections, uint8_t pPattern = 0xFF);
    std::vector<int>     decodeBendCode(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pBendCode);
    std::vector<uint8_t> readLUT(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pMode = 0);

  private:
    std::map<uint16_t, std::string> fMap;
    std::vector<uint8_t>            fWordAlignmentPatterns = {0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A};

    // pixel configuration functions
    bool     configPixel(Ph2_HwDescription::Chip* pChip, std::string cReg, int pPixelNum, uint8_t pValue, bool pVerifLoop = VERIFY_MPA);
    uint16_t readPixel(Ph2_HwDescription::Chip* pChip, std::string cReg, int pPixelNum);
    bool     configRow(Ph2_HwDescription::Chip* pChip, std::string cReg, int pRowNum, uint8_t pValue, bool pVerifLoop = VERIFY_MPA);
    bool     configPeri(Ph2_HwDescription::Chip* pChip, std::string cReg, uint8_t pValue, bool pVerifLoop = VERIFY_MPA);
    uint16_t readPeri(Ph2_HwDescription::Chip* pChip, std::string cReg);
    uint16_t regPixel(Ph2_HwDescription::Chip* pChip, int pBaseRegister, int pRow, int pColumn);
    uint16_t regPeri(Ph2_HwDescription::Chip* pChip, int pBaseRegister);
    uint16_t regRow(Ph2_HwDescription::Chip* pChip, int pBaseRegister, int pRow);
    bool     maskRowCol(Ph2_HwDescription::Chip* pChip, int pRow, int pColumn, uint8_t pMask, bool pVerifLoop = VERIFY_MPA);
    bool     maskPixel(Ph2_HwDescription::Chip* pChip, int pPixelNum, uint8_t pMask, bool pVerifLoop = VERIFY_MPA);
    bool     enablePixelInjection(Ph2_HwDescription::Chip* pChip, int pPixelNum, uint8_t pInj, bool pVerifLoop = VERIFY_MPA);

    //
    void Set_calibration(Ph2_HwDescription::Chip* pMPA, uint32_t cal);
    void Set_threshold(Ph2_HwDescription::Chip* pMPA, uint32_t th);

    bool     WriteReg(Ph2_HwDescription::Chip* pMPA, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerifLoop = VERIFY_MPA);
    bool     WriteRegs(Ph2_HwDescription::Chip* pMPA, const std::vector<std::pair<uint16_t, uint16_t>> pRegs, bool pVerifLoop = VERIFY_MPA);
    bool     WriteChipSingleReg(Ph2_HwDescription::Chip* pMPA, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = VERIFY_MPA);
    uint16_t ReadReg(Ph2_HwDescription::Chip* pMPA, uint16_t pRegisterAddress, bool pVerifLoop = VERIFY_MPA);
    // FIX-ME .. these need to be moved from here
    // Stubs  Format_stubs(std::vector<std::vector<uint8_t>> rawstubs);
    // L1data Format_l1(std::vector<uint8_t> rawl1, bool verbose = false);
};
} // namespace Ph2_HwInterface

#endif
