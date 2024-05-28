/*!
 *
 * \file OTverifyECVlpGBTCIC.h
 * \brief OTverifyECVlpGBTCIC class
 * \author Irene Zoi
 * \date 23/05/24
 *
 */

#ifndef OTverifyECVlpGBTCIC_h__
#define OTverifyECVlpGBTCIC_h__

#include "tools/Tool.h"
#include "tools/OTverifyBoardDataWord.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTverifyECVlpGBTCIC.h"
#endif

class OTverifyECVlpGBTCIC : public OTverifyBoardDataWord
{
  public:
    OTverifyECVlpGBTCIC();
    ~OTverifyECVlpGBTCIC();

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
    void runECV();
    std::vector<std::pair<uint8_t,uint8_t>> stubPatterns{
      std::make_pair(0xea,0xaa), // default
      std::make_pair(0x75,0x55), // shift 1 -> patterns
      std::make_pair(0xba,0xaa), // shift 2 ->  or <- patterns
      std::make_pair(0xD5,0x55)  // shift 1 <- patterns
    };

    std::vector<uint32_t> L1Patterns{
      0x0ffffffe, // default
      0x07ffffff, // shift 1 -> patterns
      0x03ffffff, // shift 2 -> patterns
      0x1ffffffc, // shift 1 <- patterns
      0x3ffffffe  // shift 2 <- patterns
    };
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTverifyECVlpGBTCIC fDQMHistogramOTverifyECVlpGBTCIC;
#endif
};

#endif
