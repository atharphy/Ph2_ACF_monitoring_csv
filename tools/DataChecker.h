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

#ifndef PSEvent
struct PSEvent
{
    // readout
    uint8_t fReadoutSuccess = 0;
    // event information
    uint32_t fEventId       = 0;
    uint32_t fInjectionId   = 0;
    uint32_t fAttemptId     = 0;
    uint32_t fBurstId       = 0;
    uint16_t fTriggerId     = 0;
    uint16_t fBxId          = 0;
    uint16_t fL1IdCic       = 0;
    uint8_t  fPackageDelay  = 0;
    int      fStubOffset    = 0;
    int      fLatencyOffset = 0;
    //
    uint16_t fMPALatency  = 0;
    uint16_t fSSALatency  = 0;
    uint32_t fStubLatency = 0;
    //
    uint16_t fHybridId = 0;
    uint16_t fChipId   = 0;
    uint16_t fChipL1Id = 0;
    //
    uint16_t fNSclusters = 0;
    uint16_t fNPclusters = 0;
    //
    uint16_t fPClusterSize = 0;
    uint16_t fSClusterSize = 0;
    uint16_t fStubSize     = 0;
    //
    uint16_t fL1Status = 0;
};
typedef std::vector<PSEvent> PSEvents;
#endif

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

    void AnaInjectionTestPS(uint32_t pMaxTriggersToAccept = 1000);
    void InjectionTestPS(uint32_t pMaxTriggersToAccept = 1000);
    void ReadDataTestPS(Ph2_HwDescription::BeBoard* pBoard, uint32_t pNevents = 10000);
    void CheckPSData(Ph2_HwDescription::BeBoard* pBoard, std::vector<Ph2_HwInterface::Injection> pInjections);
    void DigitalInjectionTest(bool pBypassCic = false, bool pShiftRegMode = true);
    void Eye_CIC();
    bool GenericFastCommands();

    void PrepareDigitalInjection(DetectorDataContainer& pInjectionScheme);
    void FastCommandInjections(int pNTrials = 1);
    void PSTriggerTests();
    void PSNominal();

    void noiseCheck(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint8_t> pChipIds, std::pair<uint8_t, int> pExpectedStub);
    void matchEvents(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint8_t> pChipIds, std::pair<uint8_t, int> pExpectedStub);
    void AsyncTest();
    void ReadDataTest();
    void ReadNeventsTest();
    void WriteSlinkTest(std::string pDAQFileName = "");
    void StubCheck(std::vector<uint8_t> pChipIds);
    void MaskForStubs(Ph2_HwDescription::BeBoard* pBoard, uint16_t pSeed, bool pSeedLayer);
    void CollectEvents();

    void                                    HitCheck2S(Ph2_HwDescription::BeBoard* pBoard);
    void                                    HitCheck();
    void                                    zeroContainers();
    void                                    print(std::vector<uint8_t> pChipIds);
    void                                    Running() override;
    void                                    Stop() override;
    void                                    Pause() override;
    void                                    Resume() override;
    void                                    writeObjects();
    std::vector<Ph2_HwInterface::Injection> GeneratePSInjections(int pMaxNstubs);

    class TPconfig
    {
      public:
        uint8_t  firmwareTPdelay = 80;
        uint16_t tpDelay         = 200;
        uint16_t tpSequence      = 400;
        uint16_t tpFastReset     = 0;
        uint8_t  tpAmplitude     = 100;
    };

  protected:
    std::vector<uint8_t> fFastCommands;
    std::vector<int>     fTriggeredBxs;
    std::vector<int>     fBurstIds;
    int                  fNInjectedTriggers = 0;

  private:
    //
    PSEvent fPSevent;
    // masks
    std::shared_ptr<ChannelGroup<1, NCHANNELS>> fCBCMask;

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
    void     printPSevent()
    {
        if(fPSevent.fL1IdCic != 511 && fPSevent.fL1Status == 0x00)
            LOG(INFO) << BOLDBLUE << "\t...Event#" << +fPSevent.fEventId << " trigger Id " << +fPSevent.fTriggerId << " Hybrid#" << +fPSevent.fHybridId << " L1 Id is " << fPSevent.fL1IdCic
                      << " BX Id is " << fPSevent.fBxId << " L1 status flag = " << std::bitset<9>(fPSevent.fL1Status) << " " << fPSevent.fPClusterSize << " p clusters [total] and "
                      << " " << fPSevent.fSClusterSize << " s clusters [total]" << RESET;
        else if(fPSevent.fL1IdCic == 511)
            LOG(INFO) << BOLDRED << "\t...Event#" << +fPSevent.fEventId << " trigger Id " << +fPSevent.fTriggerId << " Hybrid#" << +fPSevent.fHybridId << " L1 Id is " << fPSevent.fL1IdCic
                      << " BX Id is " << fPSevent.fBxId << " L1 status flag = " << std::bitset<9>(fPSevent.fL1Status) << " " << fPSevent.fPClusterSize << " p clusters [total] and "
                      << " " << fPSevent.fSClusterSize << " s clusters [total]" << RESET;
        else
            LOG(INFO) << BOLDMAGENTA << "\t...Event#" << +fPSevent.fEventId << " trigger Id " << +fPSevent.fTriggerId << " Hybrid#" << +fPSevent.fHybridId << " L1 Id is " << fPSevent.fL1IdCic
                      << " BX Id is " << fPSevent.fBxId << " L1 status flag = " << std::bitset<9>(fPSevent.fL1Status) << " " << fPSevent.fPClusterSize << " p clusters [total] and "
                      << " " << fPSevent.fSClusterSize << " s clusters [total]" << RESET;
    }
    //

    std::vector<float>                      GetBxIds(std::vector<float> pRawBxIds);
    std::vector<int>                        GenerateIds();
    void                                    PreparePSInjection(DetectorDataContainer& pInjectionScheme);
    std::vector<uint8_t>                    GeneratePSstrpClusters(int pMaxNSclusters);
    std::vector<Ph2_HwInterface::Injection> GeneratePSpxlClusters(int pMaxNPclusters);
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
