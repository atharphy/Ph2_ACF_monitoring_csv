/*!

        \file                                            PSInterface.h
        \brief                                           User Interface to the PSs
        \author                                          Lorenzo BIDEGAIN, Nicolas PIERRE
        \version                                         1.0
        \date                        31/07/14
        Support :                    mail to : lorenzo.bidegain@gmail.com, nico.pierre@icloud.com

 */

#ifndef __PSINTERFACE_H__
#define __PSINTERFACE_H__

#include "BeBoardFWInterface.h"
#include "MPAInterface.h"
#include "ReadoutChipInterface.h"
#include "SSAInterface.h"

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
 * \class PSInterface
 * \brief Class representing the User Interface to the PS on different boards
 */

class PSInterface : public ReadoutChipInterface
{ // begin class
  private:
    // I2C config
    bool    fRetryI2C       = true;
    uint8_t fMaxI2CAttempts = 20;

  public:
    PSInterface(const BeBoardFWMap& pBoardMap);
    ~PSInterface();
    Ph2_HwInterface::SSAInterface* theSSAInterface;
    Ph2_HwInterface::MPAInterface* theMPAInterface;
    void                           setFileHandler(FileHandler* pHandler);
    bool                           ConfigureChip(Ph2_HwDescription::Chip* pPS, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    uint32_t                       ReadData(Ph2_HwDescription::BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait);
    void                           ReadPS(Ph2_HwDescription::ReadoutChip* pPS);

    bool     WriteChipReg(Ph2_HwDescription::Chip* pPS, const std::string& pRegName, uint16_t pValue, bool pVerifLoop = true) override;
    bool     WriteChipMultReg(Ph2_HwDescription::Chip* pPS, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop = true) override;
    bool     WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pPS, const std::string& dacName, ChipContainer& pValue, bool pVerifLoop = true) override;
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pPS, const std::string& pRegName) override;
    void     StartPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip) override {}
    void     StopPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip) override {}

    void                  Pix_write(Ph2_HwDescription::ReadoutChip* cPS, Ph2_HwDescription::ChipRegItem cRegItem, uint32_t row, uint32_t pixel, uint32_t data);
    uint32_t              Pix_read(Ph2_HwDescription::ReadoutChip* cPS, Ph2_HwDescription::ChipRegItem cRegItem, uint32_t row, uint32_t pixel);
    void                  activate_I2C_chip();
    std::vector<uint16_t> ReadoutCounters_PS(uint32_t raw_mode_en);
    void                  PS_Open_shutter(uint32_t duration = 0);
    void                  PS_Close_shutter(uint32_t duration = 0);
    void                  PS_Clear_counters(uint32_t duration = 0);
    void                  PS_Start_counters_read(uint32_t duration = 0);
    void                  Activate_async(Ph2_HwDescription::Chip* pPS);
    void                  Activate_sync(Ph2_HwDescription::Chip* pPS);
    void                  Activate_pp(Ph2_HwDescription::Chip* pPS, uint8_t win = 0);
    void                  Activate_ss(Ph2_HwDescription::Chip* pPS, uint8_t win = 0);
    void                  Activate_ps(Ph2_HwDescription::Chip* pPS, uint8_t win = 8);

    void Enable_pix_counter(Ph2_HwDescription::ReadoutChip* pPS, uint32_t p);
    void Enable_pix_sync(Ph2_HwDescription::ReadoutChip* pPS, uint32_t p);
    void Disable_pixel(Ph2_HwDescription::ReadoutChip* pPS, uint32_t p);
    void Enable_pix_digi(Ph2_HwDescription::ReadoutChip* pPS, uint32_t p);
    // uint32_t Read_pixel_counter(Ph2_HwDescription::ReadoutChip* pPS, uint32_t p);

    void             digiInjection(Ph2_HwDescription::ReadoutChip* pChip, std::vector<Injection> pInjections, uint8_t pPattern = 0xFF);
    std::vector<int> decodeBendCode(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pBendCode);
    void             ReadASEvent(Ph2_HwDescription::ReadoutChip* pPS, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange = std::pair<uint32_t, uint32_t>({0, 0}));
    void             Pix_Smode(Ph2_HwDescription::ReadoutChip* pPS, uint32_t p, std::string smode);
    void             Enable_pix_BRcal(Ph2_HwDescription::ReadoutChip* pPS, uint32_t p, std::string polarity = "rise", std::string smode = "edge");
    void             Pix_Set_enable(Ph2_HwDescription::ReadoutChip* pPS,
                                    uint32_t                        p,
                                    uint32_t                        PixelMask,
                                    uint32_t                        Polarity,
                                    uint32_t                        EnEdgeBR,
                                    uint32_t                        EnLevelBR,
                                    uint32_t                        Encount,
                                    uint32_t                        DigCal,
                                    uint32_t                        AnCal,
                                    uint32_t                        BRclk);

    void Set_calibration(Ph2_HwDescription::Chip* pPS, uint32_t cal);
    void Set_threshold(Ph2_HwDescription::Chip* pPS, uint32_t th);

    void Send_pulses(uint32_t n_pulse, uint32_t duration = 0);
    bool enableInjection(Ph2_HwDescription::ReadoutChip* pChip, bool inject, bool pVerifLoop = true);

    bool maskChannelsGroup(Ph2_HwDescription::ReadoutChip* pPS, const ChannelGroupBase* group, bool pVerifLoop) { return true; }
    //
    bool maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool mask, bool inject, bool pVerifLoop) { return true; }
    //
    bool ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pPS, bool pVerifLoop, uint32_t pBlockSize) { return true; }
    //
    bool MaskAllChannels(Ph2_HwDescription::ReadoutChip* pPS, bool mask, bool pVerifLoop) { return true; }

    void Cleardata();

    //
    void setRetryI2C(bool pRetry)
    {
        fRetryI2C = pRetry;
        theSSAInterface->setRetryI2C(fRetryI2C);
    }
    void setMaxI2CAttempts(uint8_t pMaxAttempts)
    {
        fMaxI2CAttempts = pMaxAttempts;
        theSSAInterface->setMaxI2CAttempts(fMaxI2CAttempts);
    }
    std::pair<uint16_t, uint16_t> getSsaRetrySummary() { return theSSAInterface->getRetrySummary(); };
    std::pair<int, float>         getSsaWRattempts() { return theSSAInterface->getWRattempts(); };
    std::pair<float, float>       getSsaMinMaxWRattempts() { return theSSAInterface->getMinMaxWRattempts(); };
    std::pair<uint16_t, uint16_t> getSsaReadBackErrorSummary() { return theSSAInterface->getReadBackErrorSummary(); };
    std::pair<uint16_t, uint16_t> getSsaWriteErrorSummary() { return theSSAInterface->getWriteErrorSummary(); };
    void                          resetSsaRetrySummary() { theSSAInterface->resetRetrySummary(); };
    void                          resetSsaErrorSummary() { theSSAInterface->resetErrorSummary(); };
    // void                              printErrorSummary();
};
} // namespace Ph2_HwInterface

#endif
