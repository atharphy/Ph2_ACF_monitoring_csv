/*!
  \file                  lpGBT.h
  \brief                 lpGBT description class, config of the lpGBT
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef lpGBT_H
#define lpGBT_H

#include "Chip.h"
#include "Utils/ConsoleColor.h"
#include "Utils/Utilities.h"
#include "Utils/easylogging++.h"

#include <iomanip>

namespace Ph2_HwDescription
{
class lpGBT : public Chip
{
  public:
    struct eportProperties
    {
        uint8_t Group;
        uint8_t Channel;
        uint8_t Polarity;
    };

    lpGBT(uint8_t pBeBoardId, uint8_t FMCId, uint8_t pOpticalGroupId, uint8_t pChipId, const std::string& fileName);

    lpGBT(const lpGBT&) = delete;

    void initializeFreeRegisters() override;

    void loadfRegMap(const std::string& fileName) override;

    std::stringstream getRegMapStream() override;
    uint8_t           getNumberOfBits(const std::string& dacName) override { return 0; }

    void    setVersion(uint8_t pVersion) { fVersion = pVersion; }
    uint8_t getVersion() const { return fVersion; }
    void    setInvertClock(uint8_t hybridId, bool pInvertClock);

    void setPhaseRxAligned(const bool done) { phaseRxAligned = done; };
    bool getPhaseRxAligned() { return phaseRxAligned; };

    void setRxHSLPolarity(uint8_t pRxHSLPolarity) { fRxHSLPolarity = pRxHSLPolarity; }
    void setTxHSLPolarity(uint8_t pTxHSLPolarity) { fTxHSLPolarity = pTxHSLPolarity; }

    void addRxGroups(const std::vector<uint8_t>& pRxGroups) { addNoDuplicate<uint8_t>(fRxGroups, pRxGroups); }
    void addRxProperty(uint8_t pRxGroup, uint8_t pRxChannel, uint8_t pRxPolarity) { fRxProperties.push_back({pRxGroup, pRxChannel, pRxPolarity}); };
    void setRxDataRate(uint16_t pRxDataRate) { fRxDataRate = pRxDataRate; }

    void addTxProperty(uint8_t pTxGroup, uint8_t pTxChannel, uint8_t pTxPolarity) { fTxProperties.push_back({pTxGroup, pTxChannel, pTxPolarity}); };
    void setTxDataRate(uint16_t pTxDataRate) { fTxDataRate = pTxDataRate; }

    std::vector<uint8_t>         getRxGroups() { return fRxGroups; }
    std::vector<eportProperties> getRxProperties() { return fRxProperties; }
    uint16_t                     getRxDataRate() { return fRxDataRate; }

    std::vector<eportProperties> getTxProperties() { return fTxProperties; }
    uint16_t                     getTxDataRate() { return fTxDataRate; }

    uint8_t getRxHSLPolarity() { return fRxHSLPolarity; }
    uint8_t getTxHSLPolarity() { return fTxHSLPolarity; }

    void     updateWriteCount(uint8_t pMasterId, uint32_t fIncrement = 1) { fI2CWrites[pMasterId] += fIncrement; }
    void     updateReadCount(uint8_t pMasterId, uint32_t fIncrement = 1) { fI2CReads[pMasterId] += fIncrement; }
    uint32_t getWriteCount(uint8_t pMasterId) { return fI2CWrites[pMasterId]; }
    uint32_t getReadCount(uint8_t pMasterId) { return fI2CReads[pMasterId]; }

    void                    setTuneVrefADC(std::string pTuneVrefADC) { fTuneVrefADC = pTuneVrefADC; }
    void                    setTuneVrefVoltage(uint16_t pTuneVrefVoltage) { fTuneVrefVoltage = pTuneVrefVoltage; }
    std::string             getTuneVrefADC() { return fTuneVrefADC; }
    uint16_t                getTuneVrefVoltage() { return fTuneVrefVoltage; }
    std::pair<float, float> getTemperatureCoefficients() { return fTemperatureCoefficients; }

  private:
    bool                         phaseRxAligned; // @TMP@
    uint16_t                     fRxDataRate, fTxDataRate, fChipAddress;
    uint8_t                      fVersion, fRxHSLPolarity, fTxHSLPolarity;
    std::vector<uint8_t>         fRxGroups;
    std::vector<eportProperties> fRxProperties, fTxProperties;

    // #########################################################
    // # Number of write transactions - one element per master #
    // #########################################################
    std::vector<uint32_t> fI2CWrites{0, 0, 0};

    // ########################################################
    // # Number of read transactions - one element per master #
    // ########################################################
    std::vector<uint32_t> fI2CReads{0, 0, 0};

    // #######################################################
    // # ADC Channel and Voltage to manually tune Vref to 1V #
    // #######################################################
    std::string             fTuneVrefADC{"ADC2"};                                    // ADC2 = LV monitor line of 2S module, ADC7 for PS modules (2.55V line monitor)
    uint16_t                fTuneVrefVoltage{500};                                   // Voltage is given in mV!
    std::pair<float, float> fTemperatureCoefficients{std::make_pair(0.0021, 0.475)}; // In V per Celsius and Volt coming from the lpGBTv0 manual
};
} // namespace Ph2_HwDescription

#endif
