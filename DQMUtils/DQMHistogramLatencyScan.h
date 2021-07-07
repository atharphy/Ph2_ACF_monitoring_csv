/*!
        \file                DQMHistogramLatencyScan.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
*/

#ifndef __DQMHISTOGRAMLATENCYSCAN_H__
#define __DQMHISTOGRAMLATENCYSCAN_H__
#include "../DQMUtils/DQMHistogramBase.h"
#include "../Utils/Container.h"
#include "../Utils/DataContainer.h"

class TFile;

/*!
 * \class DQMHistogramLatencyScan
 * \brief Class for PedeNoise monitoring histograms
 */
class DQMHistogramLatencyScan : public DQMHistogramBase
{
  public:
    /*!
     * constructor
     */
    DQMHistogramLatencyScan();

    /*!
     * destructor
     */
    ~DQMHistogramLatencyScan();

    /*!
     * Book histograms
     */
    void book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap) override;

    /*!
     * Fill histogram
     */
    bool fill(std::vector<char>& dataBuffer) override;

    /*!
     * Save histogram
     */
    void process() override;

    /*!
     * Reset histogram
     */
    void reset(void) override;
    // virtual void summarizeHistos();

    // Histogram Fillers
    void fillLatencyPlots(DetectorDataContainer& theLatency);
    void fillStubLatencyPlots(DetectorDataContainer& theStubLatency);
    void fill2DLatencyPlots(DetectorDataContainer& the2DLatency);
    void fillTriggerTDCPlots(DetectorDataContainer& theTriggerTDC);

  private:
    void parseSettings(const Ph2_System::SettingsMap& pSettingsMap);

    DetectorDataContainer fDetectorData;
    DetectorDataContainer fLatencyHistograms;
    DetectorDataContainer fStubHistograms;
    DetectorDataContainer fLatencyScan2DHistograms;
    DetectorDataContainer fTriggerTDCHistograms;

    uint32_t fStartLatency;
    uint32_t fLatencyRange;
};
#endif
