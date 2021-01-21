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

#include "Tool.h"

#ifndef ChannelList
typedef std::vector<uint8_t> ChannelList;
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
    void Start(int currentRun) override;
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

  protected:
  private:
    // masks
    ChannelGroup<254, 1> fCBCMask;

    // Containers
    DetectorDataContainer fRegMapContainer;
    DetectorDataContainer fHitCheckContainer, fStubCheckContainer;
    DetectorDataContainer fThresholds, fLogic, fHIPs;
    DetectorDataContainer fInjections;
    DetectorDataContainer fDataMismatches;

    int fPhaseTap     = 8;
    int fAttempt      = 0;
    int fMissedEvent  = 0;
    int fEventCounter = 0;

    //
    TPconfig fTPconfig;

// booking histograms
#ifdef __USE_ROOT__
//  DQMHistogramCic fDQMHistogram;
#endif
};
#endif
