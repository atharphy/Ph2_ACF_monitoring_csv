/*!
 *
 * \file OTSSAtoSSAecv.h
 * \brief OTSSAtoSSAecv class
 * \author Fabio Ravera
 * \date 26/06/24
 *
 */

#ifndef OTSSAtoSSAecv_h__
#define OTSSAtoSSAecv_h__

#include "tools/OTSSAtoMPAecv.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTSSAtoSSAecv.h"
#endif

class OTSSAtoSSAecv : public OTSSAtoMPAecv
{
  public:
    OTSSAtoSSAecv();
    ~OTSSAtoSSAecv();

    void Initialise(void);

    // State machine
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;
    void Reset();

    static std::string fCalibrationDescription;

  private:
    void                                                        runSSAtoSSAecvScan();
    void                                                        setStubLogicParameters(Ph2_HwDescription::ReadoutChip* theMPA) override;
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>>          produceStripClusterList() override;
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>>          produceMatchingPixelClusterList(uint8_t colCoordinate) override;
    std::vector<std::vector<std::tuple<uint8_t, uint8_t, int>>> producePossibleStubVectorList(const std::vector<std::tuple<uint8_t, uint8_t, uint8_t>>& thePixelClusterList) override;
    std::vector<float>                                          fListOfSSAslvsCurrents{1, 4, 7};
    uint8_t                                                     fCurrentStripInjected;

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTSSAtoSSAecv fDQMHistogramOTSSAtoSSAecv;
#endif
};

#endif
