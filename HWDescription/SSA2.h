/*!

        \file                   SSA2.h
        \brief                  SSA2 Description class, config of the SSAs
        \author                 Marc Osherson (copying from SSA.h, SSA2 main difference is the implementation of the registers)
        \version                1.0
        \date                   28/12/2020
        Support :               mail to : oshersonmarc@gmail.com

 */

#ifndef SSA2_h__
#define SSA2_h__

#include "ChipRegItem.h"
#include "FrontEndDescription.h"
#include "ReadoutChip.h"
#include "Utils/Exception.h"
#include "Utils/Visitor.h"
#include "Utils/easylogging++.h"
#include <iostream>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>

namespace Ph2_HwDescription
{ // open namespace

using SSARegPair = std::pair<std::string, ChipRegItem>;
using CommentMap = std::map<int, std::string>;

class SSA2 : public ReadoutChip
{ // open class def
  public:
    // C'tors which take BeBoardId, FMCId, HybridId, ChipId
    SSA2(uint8_t pBeBoardId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pChipId, uint8_t pPartnerId, uint8_t pSSASide, const std::string& filename);
    // C'tors with object FE Description
    SSA2(const FrontEndDescription& pFeDesc, uint8_t pChipId, uint8_t pPartnerId, uint8_t pSSASide, const std::string& filename);
    SSA2(const SSA2&) = delete;

    void initializeFreeRegisters() override;
    void setReg(const std::string& pReg, uint16_t psetValue, bool pPrmptCfg, uint8_t pStatusReg) override;

    uint8_t           fPartnerId;
    uint8_t           getPartid() { return fPartnerId; }
    virtual void      accept(HwDescriptionVisitor& pVisitor) { pVisitor.visitChip(*this); }
    void              loadfRegMap(const std::string& filename) override;
    std::stringstream getRegMapStream() override;
    uint32_t          getNumberOfChannels() const override { return NSSACHANNELS; }
    bool              isDACLocal(const std::string& dacName) override // FIXME: what does thsi do? Ask Kevin.
    {
        if(dacName.find("THTRIMMING_S", 0, 12) != std::string::npos)
            return true;
        else if(dacName.find("ThresholdTrim") != std::string::npos)
            return true;
        else
            return false;
    }
    uint8_t getNumberOfBits(const std::string& dacName) override
    {
        if(dacName.find("THTRIMMING_S", 0, 12) != std::string::npos)
            return 5;
        else if(dacName.find("ThresholdTrim") != std::string::npos)
            return 5;
        else
            return 8;
    }

    bool                          isTopSensor(Ph2_HwDescription::ReadoutChip* pChip, uint16_t pLocalColumn = 0) override;
    std::pair<uint16_t, uint16_t> getGlobalCoordinates(Ph2_HwDescription::ReadoutChip* pChip, uint16_t pLocalColumn, uint16_t pLocalRow) override;
    static std::string            getStripRegisterName(const std::string& theRegisterName, uint16_t strip);

    void                         setIsCalibrationDataUpdated(bool isLoaded) override { fIsCalibrationMapUpdated = isLoaded; }
    bool                         getIsCalibrationDataUpdated() const { return fIsCalibrationMapUpdated; }
    std::map<std::string, float> getADCCalibrationMap() const { return fADCcalibrationMap; }
    void                         setADCCalibrationMap(const std::map<std::string, float>& theInputMap) override;

  private:
    // #############################################################
    // # ADC Channel and Voltage to manually tune Vref to 0.850 mV #
    // #############################################################
    bool fIsCalibrationMapUpdated{false};
    // Default values, will be overwritten once the calibration is performed
    std::map<std::string, float> fADCcalibrationMap = {
        {"ADC_SLOPE", 0.0002}, // In volts, calculated as the target Vref (0.850 V) / ADC range (4095)
        {"ADC_OFFSET", 0.},    // In volts, assumed 0. It depends on the ground value
        // {"VREF_VALUE", 0.850},  // In volts, default value
        // {"VBG_VALUE",  0.250}   // In volts, default value
    };

  protected:
    static std::vector<std::string> fListOfGlobalRegisters;
}; // close class def

} // namespace Ph2_HwDescription

#endif
