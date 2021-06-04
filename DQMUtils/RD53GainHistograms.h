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
#include "../Utils/GainFit.h"
#include "../Utils/RD53Shared.h"
#include "DQMHistogramBase.h"

#include <TH1F.h>
#include <TH2F.h>

// #############
// # CONSTANTS #
// #############
#define INTERCEPT_HALFRANGE 30   // [ToT]
#define SLOPE_HALFRANGE 3e-2     // [ToT / VCal]
#define QUADRATIC_HALFRANGE 1e-5 // [ToT / VCal^2]
#define LOG_HALFRANGE 6          // [ToT / ln(VCal)]

class GainHistograms : public DQMHistogramBase
{
  public:
    void book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap) override;
    void process() override;
    bool fill(std::vector<char>& dataBuffer) override;
    void reset() override{};

    void fillOccupancy(const DetectorDataContainer& OccupancyContainer, int DELTA_VCAL);
    void fillGain(const DetectorDataContainer& GainContainer);

  private:
    DetectorDataContainer DetectorData;

    DetectorDataContainer Occupancy2D;
    DetectorDataContainer ErrorReadOut2D;
    DetectorDataContainer ErrorFit2D;

    DetectorDataContainer Intercept1D;
    DetectorDataContainer Slope1D;
    DetectorDataContainer Quadratic1D;
    DetectorDataContainer Log1D;
    DetectorDataContainer Chi2DoF1D;

    DetectorDataContainer Intercept2D;
    DetectorDataContainer Slope2D;
    DetectorDataContainer Quadratic2D;
    DetectorDataContainer Log2D;
    DetectorDataContainer Chi2DoF2D;

    size_t nEvents;
    size_t nSteps;
    size_t startValue;
    size_t stopValue;
    size_t offset;
};

#endif
