/*!
  \file                  RD53GainHistograms.h
  \brief                 Header file of Gain calibration histograms
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53GainHistograms_H
#define RD53GainHistograms_H

#include "../System/SystemController.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/GainAndIntercept.h"
#include "../Utils/RD53Shared.h"
#include "DQMHistogramBase.h"

#include <TH1F.h>
#include <TH2F.h>

// #############
// # CONSTANTS #
// #############
#define INTERCEPT_HALFRANGE 6 // [ToT]

class GainHistograms : public DQMHistogramBase
{
  public:
    void book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap) override;
    void process() override;
    bool fill(std::vector<char>& dataBuffer) override;
    void reset() override{};

    void fillOccupancy(const DetectorDataContainer& OccupancyContainer, int DELTA_VCAL);
    void fillGainAndIntercept(const DetectorDataContainer& GainAndInterceptContainer);

  private:
    DetectorDataContainer DetectorData;

    DetectorDataContainer Occupancy2D;
    DetectorDataContainer ErrorReadOut2D;
    DetectorDataContainer ErrorFit2D;
    DetectorDataContainer Gain1D;
    DetectorDataContainer Intercept1D;
    DetectorDataContainer Gain2D;
    DetectorDataContainer Intercept2D;

    size_t nEvents;
    size_t nSteps;
    size_t startValue;
    size_t stopValue;
    size_t offset;
};

#endif
