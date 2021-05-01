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

#include "Tool.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerRecycleBin.h"

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
#endif

class DetectorContainer;
class Occupancy;

#ifndef MemEvents
struct MemEvent
{
    uint8_t  fEventId = 0;
    uint16_t fL1Id    = 0;
    uint16_t fMemoryRow = 0; 
    uint16_t fMemoryColumnExp = 0;
    uint16_t fMemoryColumnRep = 0; 
    uint16_t fCorrectValue    = 0; 
    uint16_t fTriggeredBx     = 0;
    uint16_t fChipId          = 0; 
};
typedef std::vector<MemEvent> MemEvents;
#endif

class  MemoryCheck2S : public Tool
{
  public:
    MemoryCheck2S();
    ~MemoryCheck2S();

    void Initialise();
    
    void EvaluatePedeNoise(int pNevents=100, int pScanRange=15);  
    void DataCheck(int pMeanTriggerSeparation=500); 
    void MemoryCheck2SRaw();
    void MemoryCheck2SSparse();
    
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
        public : 
            uint8_t fTrigger   = 0xC9; // trigger
            uint8_t fTestPulse = 0xC5; // trigger
            uint8_t fBC0       = 0xC3; // BC0
            uint8_t fResync    = 0xD1; // Resync
            uint8_t fClear     = 0xD3; // ReSync+BC0
            uint8_t fEmpty     = 0xC1; // empty 
    };
    struct StatsSum
    {
        public : 
            float fMean;
            float fSum; 
            float fSqSum;
            float fStdDev; 
            float fNentries;
            float fMin;
            float fMax;
    };
    

  protected:
    std::vector<uint16_t> fExpectedPipelineAddress;
    std::vector<uint8_t> fFastCommands;
    std::vector<int>  fTriggeredBxs; 
    int fNInjectedTriggers=0;
    int fTotalEventsExpected = 0; 

    DetectorDataContainer* fThresholdAndNoiseContainer;
    ContainerRecycleBin<Occupancy>             fRecycleBin;
    
  private:
    // timing 
    std::chrono::seconds::rep fStartTime; 
    std::chrono::seconds::rep fStopTime; 

    // masks
    ChannelGroup<254, 1> fCBCMask;

    // Containers
    DetectorDataContainer fRegMapContainer;
    DetectorDataContainer fBoardRegContainer;
    // 
    DetectorDataContainer fHitCheckContainer, fStubCheckContainer;
    DetectorDataContainer fInjections;
    DetectorDataContainer fDataMismatches, fGoodEvents, fBadEvents;
    DetectorDataContainer fExpectedOccupancy;
    DetectorDataContainer fThresholds; 

    int fAttempt      = 0;
    int fMissedEvent  = 0;
    int fEventCounter = 0;
    int fTriggerTestCounter =0;
    //
    TPconfig fTPconfig;
    // generic triggers 
    void GenericTriggers(int pReSync=0);
    bool SendGenericTriggers(int pTriggerSeparation=500);
    // generic TP 
    uint32_t GenericTriggerConfig(Ph2_HwDescription::BeBoard* pBoard, int cNrepetitions=1);
    bool ReadAfterGenericBlock(int pNExpected);
    // 
    bool SendGenericTestPulses(int pReSync=0);
    void GenericTestPulse(int pReSync=0);
    //
    void Check();

    // summarize stats 
    // while removing NANs 
    template<typename T>
    StatsSum SummarizeStats(std::vector<T> cData)
    {
        T cInitVal = (T)(0);
        // remove NANs
        cData.erase(std::remove_if(cData.begin(),  cData.end(), [](T x){return std::isnan(x);}),cData.end());
        // calculate stats 
        StatsSum cStatsSum; 
        cStatsSum.fSum = std::accumulate(cData.begin(), cData.end(), cInitVal);
        cStatsSum.fMean = cStatsSum.fSum/cData.size();
        cStatsSum.fMax = *(std::max_element(cData.begin(), cData.end()));
        cStatsSum.fMin = *(std::min_element(cData.begin(), cData.end()));
        cStatsSum.fSqSum = std::inner_product(cData.begin(), cData.end(), cData.begin(), 0.0);
        cStatsSum.fStdDev = std::sqrt(cStatsSum.fSqSum / cData.size() - cStatsSum.fMean * cStatsSum.fMean);
        cStatsSum.fNentries = cData.size();
        return cStatsSum;
    }
    void zeroContainers();
    
// booking histograms
#ifdef __USE_ROOT__
//  DQMHistogramCic fDQMHistogram;
#endif
};
#endif
