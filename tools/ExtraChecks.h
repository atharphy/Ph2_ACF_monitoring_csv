/*!
 *
 * \file CicFEAlignment.h
 * \brief CIC FE alignment class, automated alignment procedure for CICs
 * connected to FEs
 * \author Sarah SEIF EL NASR-STOREY
 * \date 28 / 06 / 19
 *
 * \Support : sarah.storey@cern.ch
 *
 */

#ifndef ExtraChecks_h__
#define ExtraChecks_h__

#include "PedeNoise.h"
#include "Tool.h"

class ExtraChecks : public PedeNoise
{
    using RegisterVector      = std::vector<std::pair<std::string, uint8_t>>;
    using TestGroupChannelMap = std::map<int, std::vector<uint8_t>>;

  public:
    ExtraChecks();
    ~ExtraChecks();

    void                       Initialise();
    void                       L1Eye();
    void                       StubCheck(uint8_t pChipId = 0, bool pUseNoise = false, uint8_t pTestPulseAmplitude = 0xFF - 30, int pTPgroup = 1, int pAttempts = 1);
    void                       TimingScan();
    void                       ReconstructTP(uint8_t pTPamplitude = 0xFF - 30, uint8_t pGroup = 3, uint8_t pStep = 3);
    void                       Evaluate(int pSigma = 1, uint16_t pTriggerRate = 10, bool pDisableStubs = true);
    void                       MonitorAmux(bool pAll = true);
    void                       DataCheck(std::vector<uint8_t> pChipIds, uint16_t pTriggerRate = 10, uint8_t pSeed = 125, int pBend = 10, bool pScan = false);
    void                       DataCheckTP(std::vector<uint8_t> pChipIds, uint8_t pTPamplitude = 0xFF - 30, int pTPgroup = 1, int pBendCode = 0x0b, bool pScan = false);
    void                       QuickStubCheck(std::vector<uint8_t> pChipIds, uint16_t pTriggerRate = 10, uint8_t pSeed = 125, int pBend = 10);
    void                       ScanVcthDAC(uint16_t pStart = 0, uint16_t pStop = 1000, uint16_t pStep = 10);
    void                       ConsecutiveTriggers(uint8_t pNconsecutive = 5);
    void                       ExternalTriggers(uint16_t pNconsecutive, const std::string& pSource = "TLU");
    void                       FindShorts(uint16_t pThreshold = 500, uint16_t pTPamplitude = 50);
    void                       FindOpens();
    void                       OccupancyCheck(uint16_t pTriggerRate, bool pDisableStubs);
    std::pair<uint16_t, float> ReadAmux(uint8_t pFeId, uint8_t pChipId, std::string pValueToRead, bool pOptical = true);
    void                       Running() override;
    void                       Stop() override;
    void                       Pause() override;
    void                       Resume() override;
    void                       writeObjects();

  protected:
    std::map<std::string, uint8_t> fAmuxMap = {{"Floating", 0}, {"VCth", 5}, {"VBGbias", 6}, {"VBG_LDO", 7}};

  private:
    // Containers
    void                  zeroContainers();
    DetectorDataContainer fNoiseContainer, fPedestalContainer, fOccupancyContainer, fShortsContainer;
    DetectorDataContainer fHitCheckContainer, fStubCheckContainer;
    DetectorDataContainer fThresholds, fLogic, fHIPs;

// Canvases
// Counters

// Settings

// mapping of FEs for CIC

// booking histograms
#ifdef __USE_ROOT__
//  DQMHistogramCic fDQMHistogram;
#endif
    void SetStubWindowOffsets(uint8_t pBendCode, int pBend);
};

#endif