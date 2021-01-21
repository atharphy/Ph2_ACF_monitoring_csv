/*!
  \file                  RD53LatencyHistograms.h
  \brief                 Header file of Latency calibration histograms
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53LatencyHistograms_H
#define RD53LatencyHistograms_H

#include "../System/SystemController.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/RD53Shared.h"
#include "DQMHistogramBase.h"

#include <TH1F.h>

class LatencyHistograms : public DQMHistogramBase
{
  public:
    void book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap) override;
    void process() override;
    bool fill(std::vector<char>& dataBuffer) override;
    void reset() override{};

    void fillOccupancy(const DetectorDataContainer& OccupancyContainer);
    void fillLatency(const DetectorDataContainer& LatencyContainer);

  private:
    DetectorDataContainer DetectorData;

    DetectorDataContainer Occupancy1D;
    DetectorDataContainer Latency;

    size_t nTRIGxEvent;
    size_t startValue;
    size_t stopValue;
};

#endif
