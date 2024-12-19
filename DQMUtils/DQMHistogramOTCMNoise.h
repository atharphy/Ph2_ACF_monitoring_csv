/*!
        \file                DQMHistogramOTCMNoise.h
        \brief               DQM class for OTCMNoise
        \author              Lesya Horyn, Martin Delcourt
        \date                17/02/22
*/

#ifndef DQMHistogramOTCMNoise_h_
#define DQMHistogramOTCMNoise_h_
#include "DQMUtils/DQMHistogramBase.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/Container.h"
#include "Utils/DataContainer.h"

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
    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap) override;

    /*!
     * \brief fill : fill histograms from TCP stream, need to be overwritten to avoid compilation errors, but it is not
     * needed if you do not fo into the SoC \param dataBuffer : vector of char with the TCP datastream
     */
    bool fill(std::string& inputStream) override;

    // Fill correlation between top & bottom sensors, split by detector structure
    void fillSensorChipCorrelationPlots(DetectorDataContainer& theHitData);
    void fillSensorHybridCorrelationPlots(DetectorDataContainer& theHitData);
    void fillSensorModuleCorrelationPlots(DetectorDataContainer& theHitData);

    void fill2DHitPlots(DetectorDataContainer& theHitData);
    void fillHybridCorrelationPlots(DetectorDataContainer& theHybridData);
    void fillChipCorrelationPlots(DetectorDataContainer& theHybridData);
    void fillHitProfile(DetectorDataContainer& theHitData); // Not used at the moment

    // Fill number of hits distribution, split by detector structure
    void fillChipHitPlots(DetectorDataContainer& theHitData, bool pFitDistributions);
    void fillChipHitPlots(DetectorDataContainer& theHitData);
    void fillHybridHitPlots(DetectorDataContainer& theHitData);
    void fillModuleHitPlots(DetectorDataContainer& theHitData);

    template <typename T1, typename T2, typename T3, typename T4>
    bool processInputStream(std::string streamName, std::string& inputStream, void (DQMHistogramOTCMNoise::*function)(DetectorDataContainer&))
    {
        ContainerSerialization theSerializer(streamName);
        try
        {
            if(theSerializer.attachDeserializer(inputStream))
            {
                LOG(INFO) << "Matched stream " << streamName << "!" << RESET;
                DetectorDataContainer fDetectorData = theSerializer.deserializeOpticalGroupContainer<T1, T2, T3, T4>(fDetectorContainer);
                (this->*function)(fDetectorData);
                return true;
            }
        }
        catch(const std::exception& e) // reference to the base of a polymorphic object
        {
            LOG(INFO) << BOLDRED << " Unable to read stream " << streamName << ": " << e.what() << RESET;
        }
        return false;
    }

    /*!
     * \brief process : do something with the histogram like colors, fit, drawing canvases, etc
     */
    void process() override;

    /*!
     * \brief Reset histogram
     */
    void reset(void) override;

  private:
    DetectorContainer*    fDetectorContainer;
    DetectorDataContainer fChipHitHistograms;
    DetectorDataContainer fChipHitHistogramsBottom;
    DetectorDataContainer fChipHitHistogramsTop;
    DetectorDataContainer fHybridHitHistograms;
    DetectorDataContainer fHybridHitHistogramsBottom;
    DetectorDataContainer fHybridHitHistogramsTop;
    DetectorDataContainer fModuleHitHistograms;
    DetectorDataContainer fModuleHitHistogramsBottom;
    DetectorDataContainer fModuleHitHistogramsTop;
    DetectorDataContainer f2DChipHitHistograms;
    DetectorDataContainer f2DHybridHitHistograms;
    DetectorDataContainer f2DModuleHitHistograms;
    DetectorDataContainer f2DModuleHitHistogramsBottom;
    DetectorDataContainer f2DModuleHitHistogramsTop;
    DetectorDataContainer f2DHybridHitHistograms_chip;
    DetectorDataContainer f2DModuleHitHistograms_chip;
    DetectorDataContainer f2DModuleHitHistogramsBottom_chip;
    DetectorDataContainer f2DModuleHitHistogramsTop_chip;
    DetectorDataContainer f2DModuleSensorCorrelation;
    DetectorDataContainer f2DHybridSensorCorrelation;
    DetectorDataContainer f2DChipSensorCorrelation;
    DetectorDataContainer f2DHybridCorrelation;
    DetectorDataContainer f2DChipCorrelation;

    uint32_t fNevents;
    bool     f2DHistograms;

    // fitting function
    void   fitCMNoise(TH1F* pHitCountHist, TF1* pFit, uint32_t pRange);
    double findMaximum(TH1F* pHistogram);
    double inverse_hitProbability(double probability);
};
#endif
