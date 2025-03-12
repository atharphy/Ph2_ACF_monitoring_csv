/*!
 *
 * \file OTSSAtoMPAecv.h
 * \brief OTSSAtoMPAecv class
 * \author Fabio Ravera
 * \date 11/06/24
 *
 */

#ifndef OTSSAtoMPAecv_h__
#define OTSSAtoMPAecv_h__

#include "tools/OTverifyMPASSAdataWord.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTSSAtoMPAecv.h"
#endif

class OTSSAtoMPAecv : public OTverifyMPASSAdataWord
{
  public:
    OTSSAtoMPAecv();
    ~OTSSAtoMPAecv();

    void Initialise(void);

    // State machine
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;
    void Reset();

    static std::string fCalibrationDescription;

  protected:
    std::vector<Cluster> produceMatchingPixelClusterList(uint8_t stubRow, uint8_t stubSeed) override;
    void                                               matchAllPossibleStubPatterns(uint8_t                                        numberOfBytesInSinglePacket,
                                                                                    size_t                                         numberOfLines,
                                                                                    std::vector<std::pair<PatternMatcher, float>>& thePatternAndEfficiencyList,
                                                                                    const std::vector<uint32_t>&                   concatenatedStubPackage,
                                                                                    Ph2_HwDescription::ReadoutChip*                theMPA) override;
    void                                               resetPatternMatchingEfficiencyContainer();

  private:
    void               runSSAtoMPAecvScan();
    void               runSSAtoMPAecvScanForStubs(uint8_t slvsCurrent);
    void               runSSAtoMPAecvScanForL1(uint8_t slvsCurrent);
    std::vector<float> fListOfSSAslvsCurrents{1, 4, 7};

    int                   fMinimum320PhaseShift = -1;
    int                   fMaximum320PhaseShift = +1;
    DetectorDataContainer fOriginalPhaseContainer;

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTSSAtoMPAecv fDQMHistogramOTSSAtoMPAecv;
#endif
};

#endif
