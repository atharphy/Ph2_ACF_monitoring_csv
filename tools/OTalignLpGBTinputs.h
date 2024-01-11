/*!
 *
 * \file OTalignLpGBTinputs.h
 * \brief OTalignLpGBTinputs class
 * \author Fabio Ravera
 * \date 11/01/24
 *
 */

#ifndef OTalignLpGBTinputs_h__
#define OTalignLpGBTinputs_h__

#include "Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histrgrammer here
#include "DQMUtils/DQMHistogramOTalignLpGBTinputs.h"
#endif

class OTalignLpGBTinputs : public Tool
{
  public:
    OTalignLpGBTinputs();
    ~OTalignLpGBTinputs();

    void Initialise(void);

    // State machine
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;
    void Reset();

  private:
    void AlignLpGBTInputs();

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTalignLpGBTinputs fDQMHistogramOTalignLpGBTinputs;
#endif
};

#endif
