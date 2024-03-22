/*!

        \file                   OTCMNoise.h
        \brief                 class for performing Common Mode noise studies
        \author                 Georg AUZINGER adapted by Lesya Horyn
        \version                1.0
        \date                   29/10/14 -- LH 2/17/22
        Support :               mail to : georg.auzinger@cern.ch || lesya.horyn@cern.ch

 */
#ifndef OTCMNoise_H__
#define OTCMNoise_H__

#include "tools/Tool.h"

#include "Utils/CommonVisitors.h"

// ROOT

#ifdef __USE_ROOT__
#include "DQMUtils/DQMHistogramOTCMNoise.h"
#endif

#include <math.h>

using namespace Ph2_System;

// typedef std::map<Ph2_HwDescription::Chip*, std::map<std::string, TObject*>> CbcHistogramMap;
// typedef std::map<Chip*, TCanvas*> CanvasMap;
// typedef std::map<Ph2_HwDescription::Hybrid*, std::map<std::string, TObject*>> HybridHistogramMap;

/*!
 * \class OTCMNoise
 * \brief Class to perform Common Mode noise studies
 */

class OTCMNoise : public Tool
{
  public:
    OTCMNoise();
    ~OTCMNoise();
    void Initialize();
    void SetThresholds();
    void TakeData();

    void writeObjects();
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;

  private:
    void parseSettings();

    uint32_t fNevents;
    uint32_t fVcth;
    bool     f2DHistograms;
    uint32_t fManualVcth;

#ifdef __USE_ROOT__
    DQMHistogramOTCMNoise fDQMHistogramOTCMNoise;
#endif
};

#endif
