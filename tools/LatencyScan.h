/*!

        \file                   LatencyScan.h
        \brief                 class to do latency and threshold scans
        \author              Georg AUZINGER
        \version                1.0
        \date                   20/01/15
        Support :               mail to : georg.auzinger@cern.ch

 */

#ifndef LATENCYSCAN_H__
#define LATENCYSCAN_H__

#include "../Utils/CommonVisitors.h"
#include "../Utils/ContainerRecycleBin.h"
#include "../Utils/Visitor.h"
#include "Tool.h"
#ifdef __USE_ROOT__
#include "../DQMUtils/DQMHistogramLatencyScan.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TGaxis.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TString.h"
#endif

using namespace Ph2_System;

/*!
 * \class LatencyScan
 * \brief Class to perform latency and threshold scans
 */

class LatencyScan : public Tool
{
  public:
    LatencyScan();
    ~LatencyScan();
    void Initialize();
    // this is used by commission, and supervisor
    void ScanLatency();
    // this is used by MPALatency -- only defined if USE_ROOT -- ideally should be replaced to avoid duplication
    std::map<HybridContainer*, uint8_t> ScanStubLatency(uint8_t pStartLatency = 0, uint8_t pLatencyRange = 20);
    // this is used by MPALatency -- only defined if USE_ROOT -- ideally should be replaced to avoid duplication
    std::map<HybridContainer*, uint8_t> ScanLatency_root(uint16_t pStartLatency = 0, uint16_t pLatencyRange = 20);
    void                                MeasureTriggerTDC();
    void                                ScanLatency2D();
    void                                StubLatencyScan();
    void                                writeObjects();

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;

  private:
    int  countStubs(Ph2_HwDescription::Hybrid* pFe, const Ph2_HwInterface::Event* pEvent, std::string pHistName, uint8_t pParameter);
    int  countHitsLat(BeBoard* pBoard, const std::vector<Event*> pEventVec, std::string pHistName, uint16_t pParameter, uint32_t pStartLatency);
    void updateHists(std::string pHistName, bool pFinal);
    void parseSettings();

    //  Members
    uint32_t fNevents;
    // uint32_t fInitialThreshold;
    uint32_t fHoleMode;
    uint32_t fStartLatency;
    uint32_t fLatencyRange;
    uint32_t fNCbc;
    uint8_t  fTestPulseAmplitude;
    uint32_t trigSource;

    const uint32_t fTDCBins = 8;

    int convertLatencyPhase(uint32_t pStartLatency, uint32_t cLatency, uint32_t cPhase)
    {
        int result = (int(cLatency) - int(pStartLatency) + 1) * fTDCBins + (int)cPhase;
        return result;

        // original
        // int result = (int (cLatency) - int (pStartLatency));
        // result *= fTDCBins;
        // result += fTDCBins - 1 - cPhase;
        // return result + 1;
    }

    const std::vector<std::string> getStubLatencyName(const BoardType pBoardType)
    {
        std::vector<std::string> cRegVec;

        if(pBoardType == BoardType::D19C)
            cRegVec.push_back("fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
        else
            cRegVec.push_back("not recognized");

        return cRegVec;
    }

#ifdef __USE_ROOT__
    DQMHistogramLatencyScan fDQMHistogramLatencyScan;
#endif
};

#endif
