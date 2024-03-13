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

#include "HWInterface/BeBoardFWInterface.h"
#include "HWInterface/MPA2Interface.h"
#include "HWInterface/MPAInterface.h"
#include "HWInterface/ReadoutChipInterface.h"
#include "HWInterface/SSA2Interface.h"
#include "HWInterface/SSAInterface.h"

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
// const std::map<FrontEndType, ReadoutChipInterface*> CHIP_INTERFACE
// ={{FrontEndType::SSA,Ph2_HwInterface::SSAInterface*},{FrontEndType::SSA2,Ph2_HwInterface::SSA2Interface*},{FrontEndType::MPA,Ph2_HwInterface::MPAInterface*},{FrontEndType::MPA2,Ph2_HwInterface::MPA2Interface*}};
class PSInterface : public ReadoutChipInterface
{ // begin class
  private:
    // I2C config
    bool                 fRetryI2C              = true;
    uint8_t              fMaxI2CAttempts        = 20;
    std::vector<uint8_t> fWordAlignmentPatterns = {0x7A, 0x7A, 0x7A, 0x7A, 0x7A};

  public:
    PSInterface(const BeBoardFWMap& pBoardMap);
    ~PSInterface();

    Ph2_HwInterface::SSAInterface*  theSSAInterface;
    Ph2_HwInterface::MPAInterface*  theMPAInterface;
    Ph2_HwInterface::SSA2Interface* theSSA2Interface;
    Ph2_HwInterface::MPA2Interface* theMPA2Interface;

    std::map<FrontEndType, ReadoutChipInterface*> CHIP_INTERFACE;
    ReadoutChipInterface*                         getInterface(Ph2_HwDescription::Chip* pPS);

    void                 setFileHandler(FileHandler* pHandler);
    bool                 ConfigureChip(Ph2_HwDescription::Chip* pPS, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    std::vector<uint8_t> readLUT(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pMode = 0);

    bool                                          WriteChipReg(Ph2_HwDescription::Chip* pPS, const std::string& pRegName, uint16_t pValue, bool pVerifLoop = true) override;
    bool                                          WriteChipMultReg(Ph2_HwDescription::Chip* pPS, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop = true) override;
    bool                                          WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pPS, const std::string& dacName, const ChipContainer& pValue, bool pVerifLoop = true) override;
    uint16_t                                      ReadChipReg(Ph2_HwDescription::Chip* pPS, const std::string& pRegName) override;
    std::vector<std::pair<std::string, uint16_t>> ReadChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::string>& theRegisterList) override;

    void                 producePhaseAlignmentPattern(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pWait_ms = 10) override;
    void                 produceWordAlignmentPattern(Ph2_HwDescription::ReadoutChip* pChip) override;
    std::vector<uint8_t> getWordAlignmentPatterns() override { return fWordAlignmentPatterns; }

    void             digiInjection(Ph2_HwDescription::ReadoutChip* pChip, std::vector<Injection> pInjections, uint8_t pPattern = 0x01);
    std::vector<int> decodeBendCode(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pBendCode);
    bool             enableInjection(Ph2_HwDescription::ReadoutChip* pChip, bool inject, bool pVerifLoop = true);

    bool maskChannelGroup(Ph2_HwDescription::ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop);

    bool maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop = false);

    bool setInjectionSchema(Ph2_HwDescription::ReadoutChip* pCbc, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop = false);

    //
    bool ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pPS, bool pVerifLoop, uint32_t pBlockSize);
    //
    bool MaskAllChannels(Ph2_HwDescription::ReadoutChip* pPS, bool mask, bool pVerifLoop) { return true; }

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
    void SetOptical()
    {
        bool cFoundLpgbt = this->lpGBTFound();
        theSSAInterface->setWithLpGBT(cFoundLpgbt);
        theMPAInterface->setWithLpGBT(cFoundLpgbt);
    }
    std::pair<uint16_t, uint16_t> getSsaRetrySummary() { return theSSAInterface->getRetrySummary(); };
    std::pair<int, float>         getSsaWRattempts() { return theSSAInterface->getWRattempts(); };
    std::pair<float, float>       getSsaMinMaxWRattempts() { return theSSAInterface->getMinMaxWRattempts(); };
    std::pair<uint16_t, uint16_t> getSsaReadBackErrorSummary() { return theSSAInterface->getReadBackErrorSummary(); };
    std::pair<uint16_t, uint16_t> getSsaWriteErrorSummary() { return theSSAInterface->getWriteErrorSummary(); };
    void                          resetSsaRetrySummary() { theSSAInterface->resetRetrySummary(); };
    void                          resetSsaErrorSummary() { theSSAInterface->resetErrorSummary(); };
    bool                          injectNoiseClusters(Ph2_HwDescription::ReadoutChip* pPS, std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> theClusterList);
    bool                          injectNoiseStubs(Ph2_HwDescription::ReadoutChip* pMPA, Ph2_HwDescription::ReadoutChip* pSSA, std::vector<std::tuple<uint8_t, uint8_t, int>> theStubVector);
    // void                              printErrorSummary();
};
} // namespace Ph2_HwInterface

#endif
