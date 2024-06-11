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

#include "tools/OTverifyCICdataWord.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTSSAtoMPAecv.h"
#endif

class OTSSAtoMPAecv : public OTverifyCICdataWord
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
    
  private:
    void injectStubsPS(Ph2_HwDescription::ReadoutChip* theMPA, uint8_t chipIdForCIC, Ph2_HwInterface::D19cFWInterface* theFWInterface, uint8_t numberOfBytesInSinglePacket) override;
    void injectL1PS(Ph2_HwDescription::ReadoutChip* theMPA, uint8_t chipIdForCIC, Ph2_HwInterface::D19cFWInterface* theFWInterface, uint8_t numberOfBytesInSinglePacket) override;
    DetectorDataContainer fScanEfficiencyContainer;
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTSSAtoMPAecv fDQMHistogramOTSSAtoMPAecv;
#endif
};

#endif
