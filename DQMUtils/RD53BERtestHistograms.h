/*!
  \file                  RD53BERtestHistograms.h
  \brief                 Header file of BERtest calibration histograms
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53BERtestHistograms_H
#define RD53BERtestHistograms_H

#include "../System/SystemController.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "DQMHistogramBase.h"

#include <TH1F.h>

class BERtestHistograms : public DQMHistogramBase
{
  public:
    void book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap) override;
    void process() override;
    bool fill(std::vector<char>& dataBuffer) override;
    void reset() override{};

    void fillBERtest(const DetectorDataContainer& BERtestContainer);

  private:
    DetectorDataContainer DetectorData;

    DetectorDataContainer BERtest;
};

#endif
