/*!
 *
 * \file DataChecker.h
 * \brief CIC FE alignment class, automated alignment procedure for CICs
 * connected to FEs
 * \author Sarah SEIF EL NASR-STOREY
 * \date 28 / 06 / 19
 *
 * \Support : sarah.storey@cern.ch
 *
 */

#ifndef MemoryCheck2S_h_
#define MemoryCheck2S_h_

#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerRecycleBin.h"
#include "Tool.h"

#ifndef ChannelList
typedef std::vector<uint8_t> ChannelList;
#endif
#ifndef EventTag
#ifndef EventId
typedef std::pair<uint16_t, uint16_t> EventId;
#endif
typedef std::pair<EventId, uint8_t> EventTag; // [L1Id, BxId],Tag
#endif
#ifndef EventsList
typedef std::vector<EventTag> EventsList;
#endif

#include <map>
#ifdef __USE_ROOT__
#include "TCanvas.h"
#include "TGraphErrors.h"
#include "TH2.h"
#include "TProfile.h"
#include "TProfile2D.h"
#include "TString.h"
#include "TText.h"
#include "TTree.h"
#endif

class DetectorContainer;
class Occupancy;

#ifndef MemEvents
struct MemEvent
{
    // type of test
    uint8_t fType = 0;
    // trial
    uint8_t fTrial = 0;
    // readout
    uint8_t fReadoutSuccess = 0;
    // event information
    uint8_t  fEventId              = 0;
    uint16_t fL1Id                 = 0;
    uint16_t fMemoryRow            = 0;
    uint16_t fMemoryColumnExp      = 0;
    uint16_t fMemoryColumnRep      = 0;
    uint16_t fCorrectValue         = 0;
    uint16_t fTriggeredBx          = 0;
    uint8_t  fTriggerNumberInBurst = 0;
    // latency information
    uint16_t fLatency     = 0;
    uint16_t fPkgDelay    = 0;
    uint16_t fStubLatency = 0;
    //
    uint16_t fHybridId = 0;
    uint16_t fChipId   = 0;
    // information about the test
    int fStartTime = 0;
    int fStopTime  = 0;
    // threshold and noise for this chip
    float fThreshold = 0;
    float fNoise     = 0;
    // pedestal set during this run
    uint16_t fPedestal = 0;
    // stubs
    uint8_t fSeedExp     = 0;
    uint8_t fBendExp     = 0;
    uint8_t fSeedRep     = 0xFF;
    uint8_t fBendRep     = 0xFF;
    uint8_t fMatchedStub = 0;
};
typedef std::vector<MemEvent> MemEvents;
#endif

#ifndef AdcMeasurements
struct AdcMeasurement
{
    // type of test
    uint8_t fADC = 0;
    //
    int fStartTime = 0;
    int fStopTime  = 0;
    //
    uint8_t     fLinkId   = 0;
    uint8_t     fHybridId = 0;
    uint8_t     fChipId   = 0xFF;
    std::string fDescription;
    // threshold and noise for this chip
    float fScaling = 1;
    float fMean    = 0;
    float fStdDev  = 0;
    // correction to Vref
    uint8_t fVrefCorr = 0;
};
typedef std::vector<AdcMeasurement> AdcMeasurements;
#endif

#ifndef PhyPortTap
struct PhyPortTap
{
    //
    uint8_t fHybridId = 0;
    // type of test
    uint8_t fPort    = 0;
    uint8_t fChannel = 0;
    uint8_t fTap     = 0;
    //
    int fStartTime = 0;
    int fStopTime  = 0;
};
#endif

class MemoryCheck2S : public Tool
{
  public:
    MemoryCheck2S();
    ~MemoryCheck2S();

    void Initialise();

    void ConfigureVref();
    void SaveOptimalTaps();
    void MonitorTemperature();
    void MonitorInputVoltage();
    void MonitorAnalogue();
    void SetThreshold(float pSigma = 3);
    void EvaluatePedeNoise(int pNevents = 100, int pScanRange = 25);
    void DataCheck(std::vector<uint8_t> pActiveCbcs, int pMeanTriggerSeparation = 500, bool pAllOnes = true);
    void MemoryCheck2SRaw(bool pAllOnes = true);
    void MemoryCheck2SSparse();
    void RegisterCheck();
    void OptimizeTPdelay();

    void print(std::vector<uint8_t> pChipIds);
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    void writeObjects();
    void Reset();
    void initializeRecycleBin() { fRecycleBin.setDetectorContainer(fDetectorContainer); }

    class TPconfig
    {
      public:
        uint8_t  firmwareTPdelay = 80;
        uint16_t tpDelay         = 200;
        uint16_t tpSequence      = 400;
        uint16_t tpFastReset     = 0;
        uint8_t  tpAmplitude     = 100;
    };
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
    

    void Reconfigure();

  protected:
    std::vector<uint16_t> fExpectedPipelineAddress;
    std::vector<uint8_t>  fFastCommands;
    std::vector<uint8_t>  fTrialCount;
    std::vector<int>      fTriggeredBxs;
    std::vector<int>      fTriggerNumberInBurst;
    int                   fNInjectedTriggers   = 0;
    size_t                fTotalEventsExpected = 0;

    DetectorDataContainer*         fPackageDelays;
    DetectorDataContainer*         fThresholdAndNoiseContainer;
    ContainerRecycleBin<Occupancy> fRecycleBin;

  private:
    // MemEvent
    AdcMeasurement fADCmeasurement;
    MemEvent       fMemEvent;
    MemEvent       fStubEvent;
    MemEvent       fStubCheck;
    PhyPortTap     fPhyPort;
    uint8_t        fReadoutSuccess = 0;
    
    // timing
    std::chrono::seconds::rep fStartTime;
    std::chrono::seconds::rep fStopTime;

    // masks
    ChannelGroup<254, 1> fCBCMask;

    // Containers
    DetectorDataContainer fChipMasks;
    //
    DetectorDataContainer fRegMapContainer;
    DetectorDataContainer fBoardRegContainer;
    //
    DetectorDataContainer fHitCheckContainer, fStubCheckContainer;
    DetectorDataContainer fInjections;
    DetectorDataContainer fDataMismatches, fGoodEvents, fBadEvents;
    DetectorDataContainer fExpectedOccupancy;
    DetectorDataContainer fExpectedStubs;
    DetectorDataContainer fThresholds;
    std::vector<uint8_t>  fVrefCorrections;
    //
    int fTrial              = 0;
    int fTypeOfTest         = 0;
    int fAttempt            = 0;
    int fMissedEvent        = 0;
    int fEventCounter       = 0;
    int fTriggerTestCounter = 0;
    //
    TPconfig fTPconfig;

    // generic triggers
    void GenericTriggers(int pReSync = 0, int pMaxBurstLength = 3);
    bool SendGenericTriggers(int pTriggerSeparation = 500);
    // generic TP
    uint32_t GenericTriggerConfig(Ph2_HwDescription::BeBoard* pBoard, int cNrepetitions = 1);
    bool     ReadAfterGenericBlock(int pNExpected);
    //
    bool SendGenericTestPulses(int pReSync = 0);
    void GenericTestPulse(int pReSync = 0);
    //
    void Check();
    void CopyEvent(MemEvent& pMemEvent, MemEvent pEvent)
    {
        pMemEvent.fType  = pEvent.fType;
        pMemEvent.fTrial = pEvent.fTrial;
        //
        pMemEvent.fReadoutSuccess = pEvent.fReadoutSuccess;
        // timing information
        pMemEvent.fStartTime = pEvent.fStartTime;
        pMemEvent.fStopTime  = pEvent.fStopTime;
        // event
        pMemEvent.fEventId = pEvent.fEventId;
        pMemEvent.fL1Id    = pEvent.fL1Id;
        //
        pMemEvent.fMemoryRow       = pEvent.fMemoryRow;
        pMemEvent.fMemoryColumnExp = pEvent.fMemoryColumnExp;
        pMemEvent.fMemoryColumnRep = pEvent.fMemoryColumnRep;
        //
        pMemEvent.fTriggeredBx          = pEvent.fTriggeredBx;
        pMemEvent.fTriggerNumberInBurst = pEvent.fTriggerNumberInBurst;
        //
        pMemEvent.fHybridId = pEvent.fHybridId;
        pMemEvent.fChipId   = pEvent.fChipId;
        //
        pMemEvent.fType      = pEvent.fType;
        pMemEvent.fThreshold = pEvent.fThreshold;
        pMemEvent.fNoise     = pEvent.fNoise;
        pMemEvent.fPedestal  = pEvent.fPedestal;
        //
        pMemEvent.fLatency     = pEvent.fLatency;
        pMemEvent.fPkgDelay    = pEvent.fPkgDelay;
        pMemEvent.fStubLatency = pEvent.fStubLatency;
        //
        pMemEvent.fCorrectValue = pEvent.fCorrectValue;
    }
    void PrintADCMeasurement(AdcMeasurement pEvent)
    {
        LOG(INFO) << BOLDGREEN << "Hybrid#" << +pEvent.fHybridId << " Chip#" << +pEvent.fChipId << "\t.. test started at " << +pEvent.fStartTime << "\t.. test completed at " << +pEvent.fStopTime
                  << "\t.. measuring " << pEvent.fDescription << "\t.. mean value is " << pEvent.fMean * 1e3 << "mV\t.. std dev is   " << pEvent.fStdDev * 1e3 << "mV\t.. Vref correction is "
                  << +pEvent.fVrefCorr << RESET;
    }
    void PrintMemEvent(MemEvent pEvent)
    {
        if(pEvent.fCorrectValue == 1)
        {
            LOG(INFO) << BOLDGREEN << "Chip#" << +pEvent.fChipId << " : memory report from testType#" << +pEvent.fType << "\t.. channel [memory row]" << +pEvent.fMemoryRow
                      << "\t.. pipeline address is [memory column] " << +pEvent.fMemoryColumnRep << "\t.. expected pipeline address is [memory column] " << +pEvent.fMemoryColumnExp << "\t.. L1Id is "
                      << +pEvent.fL1Id << "\t.. match in cell is " << +pEvent.fCorrectValue << "\t.. test started at " << +pEvent.fStartTime << "\t.. test completed at " << +pEvent.fStopTime
                      << "\t.. noise on this channel is " << pEvent.fNoise << "\t.. pedestal on this channel is " << pEvent.fPedestal << "\t.. threshold during test is  " << pEvent.fThreshold
                      << RESET;
        }
        else
            LOG(INFO) << BOLDRED << "Chip#" << +pEvent.fChipId << " : memory report from testType#" << +pEvent.fType << "\t.. channel [memory row]" << +pEvent.fMemoryRow
                      << "\t.. pipeline address is [memory column] " << +pEvent.fMemoryColumnRep << "\t.. expected pipeline address is [memory column] " << +pEvent.fMemoryColumnExp << "\t.. L1Id is "
                      << +pEvent.fL1Id << "\t.. match in cell is " << +pEvent.fCorrectValue << "\t.. test started at " << +pEvent.fStartTime << "\t.. test completed at " << +pEvent.fStopTime
                      << "\t.. noise on this channel is " << pEvent.fNoise << "\t.. pedestal on this channel is " << pEvent.fPedestal << "\t.. threshold during test is  " << pEvent.fThreshold
                      << RESET;
    }
    
    // resets
    void ReconfigureOffsets();

    void zeroContainers();

// booking histograms
#ifdef __USE_ROOT__
//  DQMHistogramCic fDQMHistogram;
#endif
};
#endif
