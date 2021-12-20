/*!
        \file                DQMHistogramOTCommonNoise.h
        \brief               DQM class for OTCommonNoise
        \author              Fabio Ravera
        \date                17/09/21
*/

#ifndef DQMHistogramOTCommonNoise_h_
#define DQMHistogramOTCommonNoise_h_
#include "../DQMUtils/DQMHistogramBase.h"
#include "../Utils/Container.h"
#include "../Utils/DataContainer.h"

class TFile;

/*!
 * \class DQMHistogramOTCommonNoise
 * \brief Class for OTCommonNoise monitoring histograms
 */
class DQMHistogramOTCommonNoise : public DQMHistogramBase
{
  public:
    /*!
     * constructor
     */
    DQMHistogramOTCommonNoise();

    /*!
     * destructor
     */
    ~DQMHistogramOTCommonNoise();

    /*!
     * \brief Book histograms
     * \param theOutputFile : where histograms will be saved
     * \param theDetectorStructure : Detector container as obtained after file parsing, used to create histograms for
     * all board/chip/hybrid/channel \param pSettingsMap : setting as for Tool setting map in case coe informations are
     * needed (i.e. FitSCurve)
     */
    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap) override;

    /*!
     * \brief fill : fill histograms from TCP stream, need to be overwritten to avoid compilation errors, but it is not
     * needed if you do not fo into the SoC \param dataBuffer : vector of char with the TCP datastream
     */
    bool fill(std::vector<char>& dataBuffer) override;

    bool fillHitPlots(DetectorDataContainer& theHitData);

    /*!
     * \brief process : do something with the histogram like colors, fit, drawing canvases, etc
     */
    void process() override;

    /*!
     * \brief Reset histogram
     */
    void reset(void) override;

  private:
    DetectorDataContainer fDetectorData;
    DetectorDataContainer fChipHitHistograms;
    DetectorDataContainer fHybridHitHistograms;

    //fitting function
    bool fitCMNoise(TH1F* pHitCountHist, TF1* pFit, uint32_t pRange);
    double findMaximum(TH1F* histogram);
    double hitProbability(double threshold);
    double inverse_hitProbability(double probability);
};
#endif
