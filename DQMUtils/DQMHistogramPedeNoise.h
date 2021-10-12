/*!
        \file                DQMHistogramPedeNoise.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
*/

#ifndef __DQMHISTOGRAMPEDENOISE_H__
#define __DQMHISTOGRAMPEDENOISE_H__
#include "../DQMUtils/DQMHistogramBase.h"
#include "../Utils/Container.h"
#include "../Utils/DataContainer.h"

class TFile;

/*!
 * \class DQMHistogramPedeNoise
 * \brief Class for PedeNoise monitoring histograms
 */
class DQMHistogramPedeNoise : public DQMHistogramBase
{
  public:
    /*!
     * constructor
     */
    DQMHistogramPedeNoise();

    /*!
     * destructor
     */
    ~DQMHistogramPedeNoise();

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

    /*!
     * \brief Fill validation histograms
     * \param theOccupancy : DataContainer for the occupancy
     */
    void fillValidationPlots(DetectorDataContainer& theOccupancy);

    /*!
     * \brief Fill validation histograms
     * \param theOccupancy : DataContainer for pedestal and occupancy
     */
    void fillPedestalAndNoisePlots(DetectorDataContainer& thePedestalAndNoise);

    /*!
     * \brief Fill SCurve histograms
     * \param fSCurveOccupancyMap : maps of Vthr and DataContainer
     */
    void fillSCurvePlots(uint16_t vcthr, DetectorDataContainer& fSCurveOccupancy);

  private:
    void fitSCurves();

    DetectorDataContainer fThresholdAndNoiseContainer;

    DetectorDataContainer fDetectorSCurveHistograms;
    DetectorDataContainer fDetectorChannelSCurveHistograms;
    DetectorDataContainer fDetectorValidationHistograms;
    DetectorDataContainer fDetectorPedestalHistograms;
    DetectorDataContainer fDetectorNoiseHistograms;
    DetectorDataContainer fDetectorStripNoiseHistograms;
    DetectorDataContainer fDetectorStripPedestalHistograms;
    DetectorDataContainer fDetectorStripNoiseEvenHistograms;
    DetectorDataContainer fDetectorStripNoiseOddHistograms;
    DetectorDataContainer fDetectorHybridNoiseHistograms;
    DetectorDataContainer fDetectorHybridStripNoiseHistograms;
    DetectorDataContainer fDetectorHybridStripNoiseEvenHistograms;
    DetectorDataContainer fDetectorHybridStripNoiseOddHistograms;
    DetectorDataContainer fDetectorData;

    bool cWithCBC = true;
    bool cWithSSA = false;
    bool cWithMPA = false;

    bool fPlotSCurves{false};
    bool fFitSCurves{false};
};
#endif
