/*!
 *
 * \file PedeNoiseTime.h
 * \brief Calibration class, calibration of the hardware
 * \author Georg AUZINGER
 * \date 12 / 11 / 15
 *
 * \Support : georg.auzinger@cern.ch
 *
 */

#ifndef PedeNoiseTime_h__
#define PedeNoiseTime_h__

#include "../Utils/CommonVisitors.h"
#include "../Utils/ContainerRecycleBin.h"
#include "../Utils/Visitor.h"
#include "Tool.h"
#ifdef __USE_ROOT__
#include "../DQMUtils/DQMHistogramPedeNoise.h"
#include "TTree.h"
#endif

#include <map>
#include <random>
using namespace Ph2_System;

class DetectorContainer;
class Occupancy;

#ifndef DataLogEventPedeNoiseTime
struct DataLogEventPedeNoiseTime
{
    // readout
    int fEventLoss = 0;
    // event information
    uint32_t fEventCnt = 0;
    uint16_t fEventId  = 0;
    //
    uint16_t fTriggeredBx = 0 ; 
    uint16_t fIter       = 0 ;
    //
    uint16_t fL1Latency  = 0;
    uint16_t fL1Id       = 0;
    uint8_t  fL1Mismatch = 0;
    //
    uint8_t fHybridId = 0;
    uint8_t fChipId   = 0;
    // threshold for this chip
    float fThreshold = 0;
    // hit list for this chip
    std::vector<uint8_t> fHits;
};
typedef std::vector<DataLogEventPedeNoiseTime> DataLogNoiseEvents;
#endif

class PedeNoiseTime : public Tool
{
  public:
    PedeNoiseTime();
    ~PedeNoiseTime();
    void clearDataMembers();

    void Initialise(bool pAllChan = false, bool pDisableStubLogic = true);
    bool GetDataFromFC7();
    void measureNoise(); // method based on the one below that actually analyzes the scurves and extracts the noise
    void sweepSCurves(); // actual methods to measure SCurves
    void Validate(uint32_t pNoiseStripThreshold = 1, uint32_t pMultiple = 100);
    void writeObjects();

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;

  protected:
    uint16_t findPedestal(bool forceAllChannels = false);
    void     measureSCurves(uint16_t pStartValue = 0);
    void     extractPedeNoiseTime();
    void     disableStubLogic();
    void     reloadStubLogic();
    void     cleanContainerMap();
    void     initializeRecycleBin() { fRecycleBin.setDetectorContainer(fDetectorContainer); }

    // generic fast commands
    // generic triggers
    void     GenericTriggers(size_t pNtriggersToSend = 10, int pReSync = 0, int pMaxBurstLength = 1);
    bool     SendGenericTriggers(size_t pNtriggersToSend = 10000, int pTriggerSeparation = 500);
    uint32_t GenericTriggerConfig(Ph2_HwDescription::BeBoard* pBoard, int cNrepetitions = 1);
    bool     DataFromRandomTriggers(int pTriggerSeparation = 500);
    // external triggers
    bool     DataFromExternalTriggers();
    // 
    void     CalculateOccupancy(DetectorDataContainer* pOccupancyContainer); 

    uint8_t  fPulseAmplitude{0};
    uint32_t fEventsPerPoint{0};
    uint32_t fMaxNevents{65535};
    int      fNEventsPerBurst{-1};

    DetectorDataContainer* fThresholdAndNoiseContainer;

  private:
    // to hold the original register values
    DetectorDataContainer  fTriggerCounter;
    DetectorDataContainer* fStubLogicValue;
    DetectorDataContainer* fHIPCountValue;
    bool                   cWithCBC = true;
    bool                   cWithSSA = false;
    bool                   cWithMPA = false;

    // Settings
    bool fPlotSCurves{false};
    bool fFitSCurves{false};
    bool fDisableStubLogic{true};

    void producePedeNoiseTimePlots();

    // for validation
    void setThresholdtoNSigma(BoardContainer* board, uint32_t pNSigma);

    // helpers for SCurve measurement

    std::map<uint16_t, DetectorDataContainer*> fSCurveOccupancyMap;
    ContainerRecycleBin<Occupancy>             fRecycleBin;
    DataLogEventPedeNoiseTime                  fEvent;
    StatsSum fNoiseStats, fPedestalStats; 
     
   
    class FCMDs
    {
      public:
        uint8_t fTrigger   = 0xC9; // trigger
        uint8_t fTestPulse = 0xC5; // trigger
        uint8_t fBC0       = 0xC3; // BC0
        uint8_t fResync    = 0xD1; // Resync
        uint8_t fClear     = 0xD3; // ReSync+BC0
        uint8_t fEmpty     = 0xC1; // empty
    };
    const FCMDs          fFCMDs;
    std::vector<uint8_t> fIters;
    std::vector<uint8_t> fFastCommands;
    std::vector<uint16_t> fTriggeredBxs;
    size_t               fTriggersSent = 0 ;
    size_t               fNInjectedTriggers = 0;
    size_t               fReps              = 0;
    size_t               fPerAttempt        = 0;

#ifdef __USE_ROOT__
    DQMHistogramPedeNoise fDQMHistogramPedeNoise;
#endif
};

#endif
