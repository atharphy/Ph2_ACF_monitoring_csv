/*!
        \file                DQMHistogramLatencyScan.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
*/

#ifndef __DQMHISTOGRAMKIRA_H__
#define __DQMHISTOGRAMKIRA_H__
#include "../DQMUtils/DQMHistogramBase.h"
#include "../Utils/Container.h"
#include "../Utils/DataContainer.h"

class TFile;

/*!
 * \class DQMHistogramLatencyScan
 * \brief Class for PedeNoise monitoring histograms
 */
class DQMHistogramKira : public DQMHistogramBase
{
  public:
    /*!
     * constructor
     */
    DQMHistogramKira();

    /*!
     * destructor
     */
    ~DQMHistogramKira();

    /*!
     * Book histograms
     */
    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap) override;

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
    void fillLatencyPlots(uint16_t pLatency, uint16_t pTriggerId, DetectorDataContainer& pTDCsummary, uint32_t pNevents);
    void fillBottomSensorPlots(DetectorDataContainer& pTDCsummary, uint32_t pNevents, uint16_t pLED);
    void fillTopSensorPlots(DetectorDataContainer& pTDCsummary, uint32_t pNevents, uint16_t pLED);

  private:
    void parseSettings(const Ph2_System::SettingsMap& pSettingsMap);

    DetectorDataContainer fDetectorData;
    DetectorDataContainer fLatencyHitMaps;
    DetectorDataContainer fLatencyTDCHistograms;
    DetectorDataContainer fLatencyHistograms;
    DetectorDataContainer fKIRAHitsBottomSensor;
    DetectorDataContainer fKIRAHitsTopSensor;
    DetectorDataContainer fLatencyHistogramsS0;
    DetectorDataContainer fLatencyHistogramsS1;
    DetectorDataContainer fStubHistograms;
    DetectorDataContainer fLatencyScan2DHistograms;
    DetectorDataContainer fTriggerTDCHistograms;
    DetectorDataContainer fClusterOccupancyHistograms;
    // stub hit maps - one per sensor [0 == bottom ; 1 == top ]
    DetectorDataContainer fStubMapS0Histograms;
    DetectorDataContainer fStubMapS1Histograms;
    // hit maps = one per sensor [ 0 == bottom; 1 == top ]
    DetectorDataContainer fHitMapS0Histograms;
    DetectorDataContainer fHitMapS1Histograms;
    // Bend distributions
    DetectorDataContainer fBendHistrograms;
    DetectorDataContainer fStubCountHistrograms;
    DetectorDataContainer fEventCountHistrograms;
    // Correlation plots
    DetectorDataContainer fCorrelationS0S1Histograms;
    DetectorDataContainer fCorrelationStubS0Histograms;
    DetectorDataContainer fCorrelationStubS1Histograms;
    DetectorDataContainer fCorrelationLinksS0Histogram;

    uint32_t fStartLatency;
    uint32_t fLatencyRange;
};
#endif
