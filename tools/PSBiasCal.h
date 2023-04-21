/*!
 *
 * \file PSBiasCal.h
 * \brief CIC FE alignment class, automated alignment procedure for CICs
 * connected to FEs
 * \author Sarah SEIF EL NASR-STOREY
 * \date 28 / 06 / 19
 *
 * \Support : sarah.storey@cern.ch
 *
 */

#ifndef PSBiasCal_h__
#define PSBiasCal_h__

#include "tools/Tool.h"

#include "HWInterface/BeBoardFWInterface.h"
#include "HWInterface/D19clpGBTInterface.h"
#include "HWInterface/ReadoutChipInterface.h"
#ifdef __USE_ROOT__
#include "DQMUtils/DQMHistogramPSBiasCal.h"
#endif
#include <map>

class PSBiasCal : public Tool
{
  public:
    PSBiasCal();
    ~PSBiasCal();

    void Initialise();
    void CalibrateADC();

    uint32_t CalibrateChipBias(Ph2_HwDescription::Chip* cChip, Ph2_HwDescription::Chip* clpGBT, uint32_t point, uint32_t block, uint32_t DAC_val, float exp_val, float gnd_corr, std::string dac_str, float VREFmeasured = SSA2_VREF_EXPECTED);
    void     CalibrateBias();
    void     DisableTest(Ph2_HwDescription::Chip* cChip);
    float    MeasureGnd(Ph2_HwDescription::Chip* cChip, Ph2_HwDescription::Chip* clpGBT, std::string dac_str);
    float    CalibrateVREF(Ph2_HwDescription::Chip* cChip, std::string VBGstring, std::string VREFstring, float VBGexpected, float VREFexpected);
    float    MeasureVREF(Ph2_HwDescription::Chip* cChip, std::string VBGstring, std::string VREFstring); //, float VBGexpected, float VREFexpected, float VREFmin, float VREFmax);


    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    void writeObjects();
    void Reset();

    // get alignment results
    bool getStatus() const { return fSuccess; }

  protected:
  private:
    // status
    bool fSuccess;
    // Containers
    DetectorDataContainer fRegMapContainer;
    DetectorDataContainer fBoardRegContainer;

// booking histograms
#ifdef __USE_ROOT__
  DQMHistogramPSBiasCal fDQMHistogramPSBiasCal;
#endif
};
#endif
