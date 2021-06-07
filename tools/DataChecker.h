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

#ifndef DataChecker_h_
#define DataChecker_h_

#ifdef __USE_ROOT__
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

#include "TCanvas.h"
#include "TGraphErrors.h"
#include "TH2.h"
#include "TProfile.h"
#include "TProfile2D.h"
#include "TString.h"
#include "TText.h"
#include <map>

const uint8_t FAILED_DATA_TEST = 4;

class DataChecker : public Tool
{
  public:
    DataChecker();
    ~DataChecker();

    void Initialise();
    // check injected hit+stubs vs. output hits+stubs
    void TestPulse(std::vector<uint8_t> pChipIds);
    void DataCheck(std::vector<uint8_t> pChipIds, uint8_t pSeed = 125, int pBend = 10);
    void L1Eye(std::vector<uint8_t> pChipIds);
    void ClusterCheck(std::vector<uint8_t> pChannels);
    void StubCheckWNoise(std::vector<uint8_t> pChipIds);

    void MemoryCheck2SRaw();
    void MemoryCheck2SSparse();
    void MemoryCheck2S();
    // void TriggerBurstCheck();
    void CheckPSData(Ph2_HwDescription::BeBoard* pBoard, std::vector<Ph2_HwInterface::Injection> pInjections);
    void DigitalInjectionTest(bool pBypassCic = false, bool pShiftRegMode = true);
    void Eye_CIC();
    bool GenericFastCommands();

    bool SendGenericTestPulses(int pReSync = 0);
    bool ReadAfterGenericBlock(int pNExpected);
    void PrepareDigitalInjection(DetectorDataContainer& pInjectionScheme);
    void GenericTestPulse(int pReSync = 0);
    void FastCommandMemChecks2S(int pNTrials = 1);
    void FastCommandInjections(int pNTrials = 1);
    void PSTriggerTests();
    void PSNominal();

    void TriggerBurstCheck(Ph2_HwDescription::BeBoard* pBoard);
    void noiseCheck(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint8_t> pChipIds, std::pair<uint8_t, int> pExpectedStub);
    void matchEvents(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint8_t> pChipIds, std::pair<uint8_t, int> pExpectedStub);
    void AsyncTest();
    void ReadDataTest();
    void ReadNeventsTest();
    void WriteSlinkTest(std::string pDAQFileName = "");
    void StubCheck(std::vector<uint8_t> pChipIds);
    void MaskForStubs(Ph2_HwDescription::BeBoard* pBoard, uint16_t pSeed, bool pSeedLayer);
    void CollectEvents();

    void HitCheck2S(Ph2_HwDescription::BeBoard* pBoard);
    void HitCheck();
    void zeroContainers();
    void print(std::vector<uint8_t> pChipIds);
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    void writeObjects();

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

  protected:
    std::vector<uint16_t> fExpectedPipelineAddress;
    std::vector<uint8_t>  fFastCommands;
    std::vector<int>      fTriggeredBxs;
    int                   fNInjectedTriggers   = 0;
    int                   fTotalEventsExpected = 0;

  private:
    // masks
    ChannelGroup<254, 1> fCBCMask;

    // Containers
    DetectorDataContainer fRegMapContainer;
    DetectorDataContainer fHitCheckContainer, fStubCheckContainer;
    DetectorDataContainer fThresholds, fLogic, fHIPs;
    DetectorDataContainer fInjections;
    DetectorDataContainer fDataMismatches, fGoodEvents, fBadEvents;
    DetectorDataContainer fBxIdsMatches, fBxIdsMismatches;

    int fPhaseTap           = 8;
    int fAttempt            = 0;
    int fMissedEvent        = 0;
    int fEventCounter       = 0;
    int fTriggerTestCounter = 0;

    //
    TPconfig fTPconfig;

    //

    std::vector<float>                      GetBxIds(std::vector<float> pRawBxIds);
    std::vector<int>                        GenerateIds();
    void                                    PreparePSInjection(DetectorDataContainer& pInjectionScheme);
    std::vector<Ph2_HwInterface::Injection> GeneratePSInjections(int pMaxNstubs);
    std::vector<Ph2_HwInterface::Injection> GenerateInjections(int pMaxClusters = 1, int pMaxNstubs = 17);
    void                                    PSTriggerTest();
    uint32_t                                GenericTriggerConfig(Ph2_HwDescription::BeBoard* pBoard, int cNrepetitions = 1);

// booking histograms
#ifdef __USE_ROOT__
//  DQMHistogramCic fDQMHistogram;
#endif
};
#endif
#endif
