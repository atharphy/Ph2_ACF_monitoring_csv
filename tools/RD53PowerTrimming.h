/*!
  \file                  RD53PowerTrimming.h
  \brief                 Header of power trimming
  \author                Luca GUZZI
  \version               1.0
  \date                  27/01/26
  Support:               email to luca.guzzi@cern.ch
*/

#ifndef RD53PowerTrimming_H
#define RD53PowerTrimming_H

#include "RD53CalibBase.h"

#ifdef __USE_ROOT__
#include "DQMUtils/RD53PowerTrimmingHistograms.h"
#else
typedef bool PowerTrimmingHistograms;
struct PowerTrimmingData
{
    double   timestamp;
    uint16_t bit;
    float    ChipCurrent;
    float    ANA_IN_CURR;
    float    DIG_IN_CURR;
    float    ANA_SHUNT_CURR;
    float    DIG_SHUNT_CURR;
    float    VINA;
    float    VDDA;
    float    VIND;
    float    VDDD;
    float    Iref;
};
#endif

class PowerTrimming : public CalibBase
{
  public:
    ~PowerTrimming()
    {
        WriteRootFile();
        delete histos;
    }

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;
    void run() override;
    void draw(bool saveData = true) override;
    void localConfigure(const std::string& histoFileName, int currentRun) override;
    void analyze();

    PowerTrimmingHistograms* histos;

  private:
    void fillHisto() override;
    void linearScanBottomUp(Ph2_HwDescription::RD53* pChip, const std::vector<const char*>& regNames, uint16_t startValue, uint16_t maxValue, float& targetDiff, const uint16_t COMPdefaultVal);

    uint16_t   MAX_PREAMP;
    uint16_t   MAX_COMP;
    uint16_t   MAX_LDAC;
    const bool doDebug{true};

    std::vector<PowerTrimmingData> fPowerTrimmingResults;

    DetectorDataContainer theComparatorCurrentContainer;
    DetectorDataContainer thePreamplifierCurrentContainer;
    DetectorDataContainer theLDACCurrentContainer;

  protected:
    // ######################################
    // # Parameters from configuration file #
    // ######################################
    float PREAMP_CURRENT_mA;
    float COMP_CURRENT_mA;
    float LDAC_CURRENT_mA;
    bool  doDisplay;
    bool  doUpdateChip;
};

#endif
