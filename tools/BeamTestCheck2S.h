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

    void CheckWithTP();
    void CheckWithInternal(uint8_t pContinousReadout = 0);
    void CheckWithExternal(uint8_t pContinousReadout = 0);
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

  protected:
    void initializeRecycleBin() { fRecycleBin.setDetectorContainer(fDetectorContainer); }
    void cleanContainerMap()
    {
        for(auto container: fSCurveOccupancyMap) fRecycleBin.free(container.second);
        fSCurveOccupancyMap.clear();
    }

  private:
    // Containers
    // Latency
    DetectorDataContainer fLatencyContainer;
    DetectorDataContainer fLatencyContainerS0, fLatencyContainerS1;
    // Cluster size
    DetectorDataContainer fClusterOccupancy;
    DetectorDataContainer fClusterOccupancyS0, fClusterOccupancyS1;
    // Pedestals
    DetectorDataContainer fPedestalContainer;
    DetectorDataContainer fSignalContainer;

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
    void ScanLatency(Ph2_HwDescription::BeBoard* pBoard, uint8_t pContinousReadout );
    void ScanThreshold(Ph2_HwDescription::BeBoard* pBoard);
    void UpdateClusterContainers(Ph2_HwDescription::BeBoard* pBoard, const std::vector<Ph2_HwInterface::Event*> pEvents, size_t pIndx);
    void ProcessEvents(Ph2_HwDescription::BeBoard* pBoard);

    size_t fThStep{0};

#ifdef __USE_ROOT__
    DQMHistogramBeamTestCheck fDQMHistogrammer;
#endif
};
#endif
