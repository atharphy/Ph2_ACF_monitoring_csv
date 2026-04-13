/*!
  \file                  RD53PowerTrimmingHistograms.h
  \brief                 Header file of Power Trimming histograms
  \author                Luca GUZZI
  \version               1.0
  \date                  03/02/26
  Support:               email to luca.guzzi@cern.ch
*/

#ifndef RD53PowerTrimmingHistograms_H
#define RD53PowerTrimmingHistograms_H
#include "DQMHistogramBase.h"
#include "Utils/ContainerFactory.h"

#include <TH1F.h>

#define NBINS_C 1024

struct PowerTrimmingData {
    float timestamp;
    uint16_t bit;
    float ANA_IN_CURR;
    float DIG_IN_CURR;
    float VINA;
    float VDDA;
    float VIND;
    float VDDD;
    float Iref;
    float ANA_SHUNT_CURR;
    float DIG_SHUNT_CURR;
};
class PowerTrimmingHistograms : public DQMHistogramBase
{
  public:
    PowerTrimmingHistograms() : fDetectorContainer(nullptr) {
    };
    ~PowerTrimmingHistograms() {
    };
    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& settingsMap) override;
    bool fill(std::string& inputStream) override;
    void process()                      override;
    void reset()                        override {};
    
    void fillHisto                  (const DetectorDataContainer& dataContainer, const DetectorDataContainer& dataHistogram);
    void fillComparatorCurrentHisto (const DetectorDataContainer& dataContainer);
    void fillPreamplifierCurrentHisto(const DetectorDataContainer& dataContainer);
    void fillLDACCurrentHisto       (const DetectorDataContainer& dataContainer);
    void fillCustomHistos(const std::vector<PowerTrimmingData>& dataList);

    bool AreHistoBooked = false;

  private:
    DetectorContainer* fDetectorContainer;

    DetectorDataContainer theComparatorCurrentContainer;
    DetectorDataContainer thePreamplifierCurrentContainer;
    DetectorDataContainer theLDACCurrentContainer;

    DetectorDataContainer hAnaInCurrContainer;
    DetectorDataContainer hDigInCurrContainer;
    DetectorDataContainer hVINAContainer;
    DetectorDataContainer hVDDAContainer;
    DetectorDataContainer hVINDContainer;
    DetectorDataContainer hVDDDContainer;
    DetectorDataContainer hIrefContainer;
    DetectorDataContainer hAnaShuntContainer;
    DetectorDataContainer hDigShuntContainer;
};

#endif
