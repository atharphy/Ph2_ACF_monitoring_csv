/*!
  \file                  RD53VoltageTuningHistograms.h
  \brief                 Header file of Voltage Tuning histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53VoltageTuningHistograms_H
#define RD53VoltageTuningHistograms_H

#include "../System/SystemController.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "DQMHistogramBase.h"

#include <TH1F.h>

class VoltageTuningHistograms : public DQMHistogramBase
{
  public:
    void book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap) override;
    void process() override;
    bool fill(std::vector<char>& dataBuffer) override;
    void reset() override{};

    void fillDig(const DetectorDataContainer& DataContainer);
    void fillAna(const DetectorDataContainer& DataContainer);

  private:
    DetectorDataContainer DetectorData;
    DetectorDataContainer VoltageDig;
    DetectorDataContainer VoltageAna;
};

#endif
