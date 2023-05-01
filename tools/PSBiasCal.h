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

    uint32_t CalibrateChipBias(Ph2_HwDescription::Chip* cChip, Ph2_HwDescription::Chip* clpGBT, uint32_t point, uint32_t block, uint32_t DAC_val, float exp_val, float gnd_corr, std::string dac_str, float VREFmeasured );
    uint8_t  TuneDAC(Ph2_HwDescription::Chip* cChip, float slope, float exp_val, std::string DAC, uint8_t DAC_val, bool isVref = false);
    void     CalibrateBias();
    void     DisableTest(Ph2_HwDescription::Chip* cChip);
    float    MeasureGnd(Ph2_HwDescription::Chip* cChip, Ph2_HwDescription::Chip* clpGBT, std::string dac_str);
    float    CalibrateVREF(Ph2_HwDescription::Chip* cChip, std::string VBGstring, std::string VREFstring, float VBGexpected, float VREFexpected);
    float    MeasureVREF(Ph2_HwDescription::Chip* cChip, std::string VBGstring, std::string VREFstring, uint8_t *DAC); //, float VBGexpected, float VREFexpected, float VREFmin, float VREFmax);


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

    typedef std::pair<uint8_t, uint8_t> hybrid_chip;
    // FNAL PSv2.1 Bandgaps
    const   std::map<hybrid_chip, float> SSA2_VBG_MEASURED_TABLE = { // in volts
      {std::make_pair(0, 0), 0.2755}, {std::make_pair(0, 1), 0.2770}, {std::make_pair(0, 2), 0.2749}, {std::make_pair(0, 3), 0.0000}, {std::make_pair(0, 4), 0.0000}, {std::make_pair(0, 5), 0.2719}, {std::make_pair(0, 6), 0.2757}, {std::make_pair(0, 7), 0.2786}, 
      {std::make_pair(1, 0), 0.2752}, {std::make_pair(1, 1), 0.2731}, {std::make_pair(1, 2), 0.2731}, {std::make_pair(1, 3), 0.2733}, {std::make_pair(1, 4), 0.2767}, {std::make_pair(1, 5), 0.2733}, {std::make_pair(1, 6), 0.2773}, {std::make_pair(1, 7), 0.2741}};

    // FNAL PSv2 Bandgaps // I SUBTRACTED the GND, not sure it's correct. Waiting answer from Anvesh
    const   std::map<hybrid_chip, float> MPA2_VBG_MEASURED_TABLE = { // in volts
      {std::make_pair(0, 8), 0.276181}, {std::make_pair(0, 9), 0.276695}, {std::make_pair(0, 10), 0.280825}, {std::make_pair(0, 11), 0.279712}, {std::make_pair(0, 12), 0.276814}, {std::make_pair(0, 13), 0.273418}, {std::make_pair(0, 14), 0.274185}, {std::make_pair(0, 15), 0.273296}, 
      {std::make_pair(1, 8), 0.275776}, {std::make_pair(1, 9), 0.279588}, {std::make_pair(1, 10), 0.281165}, {std::make_pair(1, 11), 0.275365}, {std::make_pair(1, 12), 0.278102}, {std::make_pair(1, 13), 0.275695}, {std::make_pair(1, 14), 0.272185}, {std::make_pair(1, 15), 0.274752}};

    // // FNAL PSv2.1 Bandgaps FIXMEEEE need wafer 4 measurements from Anvesh
    // const   std::map<hybrid_chip, float> MPA2_VBG_VOLTS = { // in volts
    //   {std::make_pair(0, 0), 0.}, {std::make_pair(0, 1), 0.}, {std::make_pair(0, 2), 0.}, {std::make_pair(0, 3), 0.}, {std::make_pair(0, 4), 0.}, {std::make_pair(0, 5), 0.}, {std::make_pair(0, 6), 0.}, {std::make_pair(0, 7), 0.}, 
    //   {std::make_pair(1, 0), 0.}, {std::make_pair(1, 1), 0.}, {std::make_pair(1, 2), 0.}, {std::make_pair(1, 3), 0.}, {std::make_pair(1, 4), 0.}, {std::make_pair(1, 5), 0.}, {std::make_pair(1, 6), 0.}, {std::make_pair(1, 7), 0.}};


// booking histograms
#ifdef __USE_ROOT__
  DQMHistogramPSBiasCal fDQMHistogramPSBiasCal;
#endif
};
#endif
