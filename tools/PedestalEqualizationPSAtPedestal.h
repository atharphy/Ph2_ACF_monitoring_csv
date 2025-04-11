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
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramPedestalEqualizationPSAtPedestal.h"
#endif

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

    static std::string fCalibrationDescription;
    std::function<bool(const ChipContainer*)>        selectSSAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2); };
    std::string selectSSAfunctionName = "SelectSSAfunction";

    std::function<bool(const ChipContainer*)>        selectMPAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string selectMPAfunctionName = "SelectMPAfunction";

  private:
    bool fOriginalIsFullScan;
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramPedestalEqualizationPSAtPedestal fDQMHistogramPedestalEqualizationPSAtPedestal;
#endif
};

#endif
