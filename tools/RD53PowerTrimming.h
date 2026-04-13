/*!
  \file                  RD53PowerTrimming.h
  \brief                 Header of threshold adjustment
  \author                Luca GUZZI
  \version               1.0
  \date                  27/01/26
  Support:               email to luca.guzzi@cern.ch
*/

#ifndef RD53PowerTrimming_H
#define RD53PowerTrimming_H

#include "RD53CalibBase.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

using dataType = std::vector<std::pair<uint16_t, float>>;

#ifdef __USE_ROOT__
#include "DQMUtils/RD53PowerTrimmingHistograms.h"
#else
typedef bool PowerTrimmingHistograms;
#endif

class PowerTrimming : public CalibBase
{ 
  public:
    ~PowerTrimming()
    {
      WriteRootFile();
      delete histos;
    }
    PowerTrimming() : 
    histos(nullptr), 
    COMP_CURRENT_mA(0), PREAMP_CURRENT_mA(0), LDAC_CURRENT_mA(0),
    MAX_PREAMP(1<<8)  , MAX_COMP(1<<10)      , MAX_LDAC(1<<10)   {}

    void  Running()                  override ;
    void  Stop()                     override ;
    void  ConfigureCalibration()     override ;
    void  sendData()                 override ;
    void  run()                      override ;
    void  draw(bool saveData = true) override ;
    void  localConfigure(const std::string& histoFileName, int currentRun) override;
    void  analyze() {};

    PowerTrimmingHistograms* histos;

  private:

    float COMP_CURRENT_mA;
    float PREAMP_CURRENT_mA;
    float LDAC_CURRENT_mA;

    uint16_t MAX_PREAMP;
    uint16_t MAX_COMP;
    uint16_t MAX_LDAC;

    std::vector<PowerTrimmingData> fPowerTrimmingResults;

    void fillHisto() override;
    dataType linearScanBottomUp(RD53* chip, const std::vector<std::string>& regNames, uint16_t startValue, uint16_t maxValue, const std::string targetName, float& targetDiff);

  protected:
    DetectorDataContainer theComparatorCurrentContainer;
    DetectorDataContainer thePreamplifierCurrentContainer;
    DetectorDataContainer theLDACCurrentContainer;
};

#endif
