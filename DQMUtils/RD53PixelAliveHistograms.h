/*!
  \file                  RD53PixelAliveHistograms.h
  \brief                 Header file of PixelAlive calibration histograms
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53PixelAliveHistograms_H
#define RD53PixelAliveHistograms_H

#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/GenericDataArray.h"
#include "DQMHistogramBase.h"

#include <TH1F.h>
#include <TH2F.h>

class PixelAliveHistograms : public DQMHistogramBase
{
  public:
    void book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap) override;
    void process() override;
    bool fill(std::vector<char>& dataBuffer) override;
    void reset() override{};

    void fill(const DetectorDataContainer& DataContainer);
    void fillBCID(const DetectorDataContainer& DataContainer);
    void fillTrgID(const DetectorDataContainer& DataContainer);

  private:
    DetectorDataContainer DetectorData;

    DetectorDataContainer Occupancy1D;
    DetectorDataContainer Occupancy2D;
    DetectorDataContainer ErrorReadOut2D;
    DetectorDataContainer ToT;
    DetectorDataContainer BCID;
    DetectorDataContainer TriggerID;

    size_t nEvents;
};

#endif
