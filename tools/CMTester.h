/*!

        \file                   CMTester.h
        \brief                 class for performing Common Mode noise studies
        \author              Georg AUZINGER
        \version                1.0
        \date                   29/10/14
        Support :               mail to : georg.auzinger@cern.ch

 */
#ifndef CMTESTER_H__
#define CMTESTER_H__

#include "Tool.h"
#ifdef __USE_ROOT__

#include "../Utils/CommonVisitors.h"

// ROOT
#include "TCanvas.h"
#include "TDirectoryFile.h"
#include "TF1.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TLegend.h"
#include "TLine.h"
#include "TMath.h"
#include "TProfile.h"
#include "TProfile2D.h"
#include "TString.h"
#include <math.h>

#include "CMFits.h"

using namespace Ph2_System;

typedef std::map<Ph2_HwDescription::Chip*, std::map<std::string, TObject*>> CbcHistogramMap;
// typedef std::map<Chip*, TCanvas*> CanvasMap;
typedef std::map<Ph2_HwDescription::Module*, std::map<std::string, TObject*>> ModuleHistogramMap;

/*!
 * \class CMTester
 * \brief Class to perform Common Mode noise studies
 */

class CMTester : public Tool
{
  public:
    CMTester();
    ~CMTester();
    void Initialize();
    void ScanNoiseChannels();
    void TakeData();
    void FinishRun();
    void SetTotalNoise(std::vector<double> pTotalNoise);

  private:
    void updateHists(bool pFinal = false);
    void parseSettings();
    void analyze(Ph2_HwDescription::BeBoard* pBoard, const Ph2_HwInterface::Event* pEvent);
    bool randHit(float pProbability);
    bool isMasked(Ph2_HwDescription::ReadoutChip* pCbc, int pChan);
    bool isMasked(int pGlobalChannel);

    uint32_t            fNevents, fDoSimulate, fSimOccupancy;
    std::vector<double> fTotalNoise;
    uint32_t            fVcth;

    std::map<ChipContainer*, std::set<int>> fNoiseStripMap;
};

#endif
#endif
