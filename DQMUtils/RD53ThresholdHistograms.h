/*!
  \file                  RD53ThresholdHistograms.h
  \brief                 Header file of Threshold calibration histograms
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53ThresholdHistograms_H
#define RD53ThresholdHistograms_H

#include "../System/SystemController.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "DQMHistogramBase.h"

#include <TH1F.h>

class ThresholdHistograms : public DQMHistogramBase
{
  public:
    void book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap) override;
    void process() override;
    bool fill(std::vector<char>& dataBuffer) override;
    void reset() override{};

    void fill(const DetectorDataContainer& DataContainer);

  private:
    DetectorDataContainer DetectorData;

    DetectorDataContainer Threhsold;
};

#endif
