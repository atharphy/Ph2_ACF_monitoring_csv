/*!
        \file                DQMHistogramPedestalEqualizationPSAtPedestal.h
        \brief               DQM class for PedestalEqualizationPSAtPedestal
        \author              Irene Zoi
        \date                11/04/25
*/

#ifndef DQMHistogramPedestalEqualizationPSAtPedestal_h_
#define DQMHistogramPedestalEqualizationPSAtPedestal_h_
#include "DQMUtils/DQMHistogramBase.h"
#include "Utils/Container.h"
#include "Utils/DataContainer.h"

class TFile;

/*!
 * \class DQMHistogramPedestalEqualizationPSAtPedestal
 * \brief Class for PedestalEqualizationPSAtPedestal monitoring histograms
 */
class DQMHistogramPedestalEqualizationPSAtPedestal : public DQMHistogramBase
{
  public:
    /*!
     * constructor
     */
    DQMHistogramPedestalEqualizationPSAtPedestal();

    /*!
     * destructor
     */
    ~DQMHistogramPedestalEqualizationPSAtPedestal();

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

    /*!
     * \brief process : do something with the histogram like colors, fit, drawing canvases, etc
     */
    void process() override;

    /*!
     * \brief Reset histogram
     */
    void reset(void) override;
    DetectorDataContainer fDetectorChipStripSCurveHistograms;
    DetectorDataContainer fDetectorChipPixelSCurveHistograms;
    DetectorDataContainer fDetectorChipStripTrimCurveHistograms;
    DetectorDataContainer fDetectorChipPixelTrimCurveHistograms;
    DetectorDataContainer fDetectorChipStripMaxHistograms;
    DetectorDataContainer fDetectorChipPixelMaxHistograms;
    DetectorDataContainer fDetectorChipPixelMax2DHistograms;
    DetectorDataContainer fDetectorChipMaxHistograms;
    DetectorDataContainer fDetectorPixelTrimBitsHistograms;
    DetectorDataContainer fDetectorStripTrimBitsHistograms;
    DetectorDataContainer fDetectorChipStripSmallestHistograms;
    DetectorDataContainer fDetectorChipStripLargestHistograms;
    DetectorDataContainer fDetectorChipPixelSmallestHistograms;
    DetectorDataContainer fDetectorChipPixelLargestHistograms;
    void                  fillSCurvePlotsVector(const std::vector<DetectorDataContainer>& detectorContainerVector, const std::vector<uint16_t>& dacList);
    void                  fillSCurvePlots(const DetectorDataContainer& detectorContainer, uint16_t dacIt);
    void                  fillTrimCurvePlots(const DetectorDataContainer& detectorContainer, uint16_t dacIt);
    void                  fillTrimCurvePlotsVector(const std::vector<DetectorDataContainer>& detectorContainerVector, const std::vector<uint16_t>& dacList);
    void fillReferenceChannelPlots(const DetectorDataContainer& theThresholdAtMaxOccupancyContainer, bool isSmallest);
    void fillMaxPlots(const DetectorDataContainer& dacOccupancyContainers);
    void fillTrimBitsPlots(const DetectorDataContainer& TrimBitContainers);

  private:
    DetectorContainer* fDetectorContainer;
};
#endif
