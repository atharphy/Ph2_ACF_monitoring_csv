/*!
        \file                DQMHistogramOTCMNoise.h
        \brief               DQM class for OTCMNoise
        \author              Lesya Horyn
        \date                17/02/22
*/

#ifndef DQMHistogramOTCMNoise_h_
#define DQMHistogramOTCMNoise_h_
#include "../DQMUtils/DQMHistogramBase.h"
#include "../Utils/Container.h"
#include "../Utils/DataContainer.h"

class TFile;

/*!
 * \class DQMHistogramOTCMNoise
 * \brief Class for OTCMNoise monitoring histograms
 */
class DQMHistogramOTCMNoise : public DQMHistogramBase
{
  public:
    /*!
     * constructor
     */
    DQMHistogramOTCMNoise();

    /*!
     * destructor
     */
    ~DQMHistogramOTCMNoise();

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
    bool fill2DHitPlots(DetectorDataContainer& theHitData);

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
    DetectorDataContainer fModuleHitHistograms;
    DetectorDataContainer fModuleHitHistogramsEven;
    DetectorDataContainer fModuleHitHistogramsOdd;
    DetectorDataContainer f2DChipHitHistograms;
    DetectorDataContainer f2DHybridHitHistograms;
    DetectorDataContainer f2DModuleHitHistograms;
    DetectorDataContainer f2DModuleHitHistogramsEven;
    DetectorDataContainer f2DModuleHitHistogramsOdd;

    uint32_t fNevents;
    bool f2DHistograms; 

    //fitting function
    bool fitCMNoise(TH1F* pHitCountHist, TF1* pFit, uint32_t pRange);
    double findMaximum(TH1F* pHistogram);
    double inverse_hitProbability(double probability);

};
#endif
