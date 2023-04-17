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
     * \param theDAC : DataContainer for the DAC value info
     */
    void fillDACPlots(DetectorDataContainer& theDAC);

  private:
    DetectorContainer*    fDetectorContainer;
    void                  fitSlopes();

    DetectorDataContainer fDetectorChipStripSlopeHistograms;
    DetectorDataContainer fDetectorChipPixelSlopeHistograms;
    DetectorDataContainer fChipStripVrefHistograms;
    DetectorDataContainer fChipPixelVrefHistograms;

    bool fWithSSA = false;
    bool fWithMPA = false;

};
#endif
