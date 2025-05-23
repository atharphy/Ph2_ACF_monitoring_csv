/*!
 *
 * \file PedestalEqualizationPSAtPedestal.h
 * \brief PedestalEqualizationPSAtPedestal class
 * \author Irene Zoi
 * \date 11/04/25
 *
 */

#ifndef PedestalEqualizationPSAtPedestal_h__
#define PedestalEqualizationPSAtPedestal_h__

#include "tools/Tool.h"
#include "tools/PedestalEqualization.h"
#include "tools/PedestalEqualizationPSFullScan.h"
#include "Utils/ContainerRecycleBin.h"
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramPedestalEqualizationPSAtPedestal.h"
#endif
class Occupancy;
class PedestalEqualizationPSAtPedestal : public PedestalEqualization
{
  public:
    // PedestalEqualizationPSAtPedestal();
    // ~PedestalEqualizationPSAtPedestal();

    void Initialise(bool pAllChan = false, bool pDisableStubLogic = true);
    // State machine
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;
    void Reset();
    void PrepareForInjection();
    void ScanThreshold();
    void ScanThresholdChip(uint16_t boardId,uint16_t OGId, uint16_t hybridId,uint16_t ChipId);
    void FillMaxOccupancyMap(std::vector<DetectorDataContainer> detectorContainerVector, DetectorDataContainer& dacOccupancyContainers, uint16_t boardId,uint16_t OGId, uint16_t hybridId,uint16_t ChipId);
    void GetMaximumOccupancyThreshold(const DetectorDataContainer& dacOccupancyContainers);
    void GetLowestAndHighestMaxOccupancyThreshold();
    void FindTargetThreshold();
    void TuneVtrim();
    void TuneVtrimBinary();
    void TuneTrimBits();
    void TuneTrimBitsBinary();
    void SetTargetThreshold();

    static std::string fCalibrationDescription;
    bool                  fWithSSA = false;
    bool                  fWithMPA = false;
    DetectorDataContainer fEventTypes;
    // Settings
    bool     fTestPulse{false};
    uint8_t  fTestPulseAmplitude{0};
    uint8_t  fTestPulseAmplitudePix{0};   
    uint32_t fEventsPerPoint{10};
    uint16_t fStripTargetVcth{0x0};
    uint16_t fPixelTargetVcth{0x0};
    uint8_t  fTargetOffset{0x80};
    bool     fCheckLoop{true};
    bool     fAllChan{true};
    bool     fDisableStubLogic{true};
    bool     fPedestalEqualizationMaskUntrimmed{false};
    uint32_t fMaxNevents{65535};
    int      fNEventsPerBurst{-1};
    // float    fOccupancyAtPedestal{0.56};
    uint8_t  fUseMean{1};
    uint32_t fPedestalEqualizationFullScanStart{110};
    float    fPedestalEqualizationFullScanCAP{1.0};

    std::vector<uint16_t>                  dacList;

    DetectorDataContainer fTheMaxOccupancyThresholdContainers;
    DetectorDataContainer fTheTargetThresholdContainers;
    DetectorDataContainer fTheSmallestThresholdAtMaxOccupancyContainer;
    DetectorDataContainer fTheLargestThresholdAtMaxOccupancyContainer;
    uint16_t fStopValue;
    uint16_t fStartValue;
  private:
    bool fOriginalIsFullScan;    
    // bool     fOriginalUseFixRange;
    // uint16_t fOriginalMinThreshold;
    // uint16_t fOriginalMaxThreshold;
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramPedestalEqualizationPSAtPedestal fDQMHistogramPedestalEqualizationPSAtPedestal;
#endif
};

#endif
