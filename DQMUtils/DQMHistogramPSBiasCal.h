/*!
        \file                DQMHistogramPSBiasCal.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger, Irene Zoi
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
        Support :            mail to : irene.zoi@cern.ch
*/

#ifndef __DQMHISTOGRAMPSBIASCAL_H__
#define __DQMHISTOGRAMPSBIASCAL_H__
#include "DQMUtils/DQMHistogramBase.h"
#include "Utils/Container.h"
#include "Utils/DataContainer.h"
#define fGraphSize 2

class TFile;

/*!
 * \class DQMHistogramPSBiasCal
 * \brief Class for PedeNoise monitoring histograms
 */
class DQMHistogramPSBiasCal : public DQMHistogramBase
{
  public:
    /*!
     * constructor
     */
    DQMHistogramPSBiasCal();

    /*!
     * destructor
     */
    ~DQMHistogramPSBiasCal();

    /*!
     * Book histograms
     */
    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap) override;

    /*!
     * Fill histogram
     */
    bool fill(std::string& inputStream) override;

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
     * \param theSlope : DataContainer for the slope info
     */
    void fillSlopePlots(DetectorDataContainer& theSlope);

    /*!
     * \brief Fill validation histograms
     * \param theDAC : DataContainer for the VREF DAC & VREF value info
     */
    void fillDACPlots(DetectorDataContainer& theDAC);

        /*!
     * \brief Fill validation histograms
     * \param theVDD : DataContainer for the AVDD ADC & voltage value info
     */
    void fillVDDPlots(DetectorDataContainer& theVDD, bool isAVDD);

  private:
    DetectorContainer*    fDetectorContainer;

    DetectorDataContainer fChipStripSlopeGraphs;
    DetectorDataContainer fChipPixelSlopeGraphs;
    DetectorDataContainer fChipStripAVDDHistograms;
    DetectorDataContainer fChipPixelAVDDHistograms;
    DetectorDataContainer fChipStripDVDDHistograms;
    DetectorDataContainer fChipPixelDVDDHistograms;
    DetectorDataContainer fChipStripVrefHistograms;
    DetectorDataContainer fChipPixelVrefHistograms;

    bool fWithSSA = false;
    bool fWithMPA = false;

};
#endif
