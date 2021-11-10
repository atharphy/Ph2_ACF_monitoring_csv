/*!
 *
 * \file LinkAlignment.h
 * \brief Link alignment class, automated alignment procedure for CIC-lpGBT-BE
 * connected to FEs
 * \author Sarah SEIF EL NASR-STOREY
 * \date 28 / 06 / 19
 *
 * \Support : sarah.storey@cern.ch
 *
 */

#ifndef BeamTestCheck2S_h__
#define BeamTestCheck2S_h__

#include "../Utils/ContainerRecycleBin.h"
#include "OTTool.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/DQMHistogramBeamTestCheck.h"
#include "TH1.h"
#endif

/*!
 * \class LatencyScan
 * \brief Class to perform latency and threshold scans
 */
class Occupancy;

using namespace Ph2_HwDescription;

class BeamTestCheck2S : public OTTool
{
  public:
    BeamTestCheck2S();
    ~BeamTestCheck2S();

    void CheckWithTP(uint8_t pContinousReadout = 1);
    void ValidateTP();
    void CheckWithInternal(uint8_t pContinousReadout = 1);
    void CheckWithExternal(uint8_t pContinousReadout = 1);
    void ValidateExternal();
    void Initialise();
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    // void Reset();
    void writeObjects();
    // void ReadDataFromFile(std::string pRawFileName);
    // void SetReadoutPause(uint32_t pReadoutPause) { fReadoutPause = pReadoutPause; }
    void DisableAllFEs();
    void ScanStubLatency(uint8_t pContinousReadout);
    void ScanL1Latency(uint8_t pContinousReadout);

  protected:
    void initializeRecycleBin() { fRecycleBin.setDetectorContainer(fDetectorContainer); }
    void cleanContainerMap()
    {
        for(auto container: fSCurveOccupancyMap) fRecycleBin.free(container.second);
        fSCurveOccupancyMap.clear();
    }
    void defInjection()
    {
        Ph2_HwInterface::Injection cInjection;
        // inject one cluster into each MPA-SSA pair
        cInjection.fRow    = 100;
        cInjection.fColumn = 12;
        fInjections.clear();
        fInjections.push_back(cInjection); // 0
    }

  private:
    // Containers
    // TDC
    DetectorDataContainer fTDCContainer;
    // Latency
    DetectorDataContainer fLatencyContainer, fStubLatencyContainer;
    DetectorDataContainer fLatencyContainerS0, fLatencyContainerS1;
    // Cluster size
    DetectorDataContainer fClusterOccupancy;
    DetectorDataContainer fClusterOccupancyS0, fClusterOccupancyS1;
    // Pedestals
    DetectorDataContainer fPedestalContainer;
    DetectorDataContainer fSignalContainer;
    // Optimal Latencies
    DetectorDataContainer fOptimalL1Latency, fOptimalStubLatency;
    // Hit Containers
    DetectorDataContainer fHitOccupancyS0, fHitOccupancyS1;
    DetectorDataContainer fHitContainerTDC;
    DetectorDataContainer fStubOccupancy;
    // Hit Maps
    DetectorDataContainer fHitMap, fStubMap;
    // Bend Maps
    DetectorDataContainer fBendMap;

    std::map<uint16_t, DetectorDataContainer*> fSCurveOccupancyMap;
    ContainerRecycleBin<Occupancy>             fRecycleBin;

    uint8_t  fTPamplitude{255};
    uint8_t  fTPdelay{0};
    uint16_t fThreshold{0};
    uint16_t fStartLatency{0};
    uint16_t fLatencyRange{0};
    uint16_t fOptimalLatency;

    void PrepareForInternal(Ph2_HwDescription::BeBoard* pBoard, uint8_t pLimitTriggers = 1);
    void PrepareForTP(Ph2_HwDescription::BeBoard* pBoard);
    void PrepareForExternal(Ph2_HwDescription::BeBoard* pBoard);
    void ScanLatency(Ph2_HwDescription::BeBoard* pBoard, uint8_t pContinousReadout);
    void ScanThreshold(Ph2_HwDescription::BeBoard* pBoard);
    void UpdateClusterContainers(Ph2_HwDescription::BeBoard* pBoard, const std::vector<Ph2_HwInterface::Event*> pEvents, size_t pIndx);
    void ProcessEvents(Ph2_HwDescription::BeBoard* pBoard);
    void Count(const std::vector<Ph2_HwInterface::Event*> pEvents, size_t pTriggerId, uint8_t pPrintOut = 0);
    void Validate();

    size_t fThStep{0};

    std::vector<Ph2_HwInterface::Injection> fInjections;

#ifdef __USE_ROOT__
    DQMHistogramBeamTestCheck fDQMHistogrammer;
#endif
};
#endif
