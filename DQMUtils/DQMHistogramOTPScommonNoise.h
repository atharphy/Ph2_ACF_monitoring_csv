/*!
        \file                DQMHistogramOTPScommonNoise.h
        \brief               DQM class for OTPScommonNoise
        \author              Irene Zoi
        \date                15/08/24
*/

#ifndef DQMHistogramOTPScommonNoise_h_
#define DQMHistogramOTPScommonNoise_h_
#include "DQMUtils/DQMHistogramBase.h"
#include "Utils/Container.h"
#include "Utils/DataContainer.h"

class TFile;

/*!
 * \class DQMHistogramOTPScommonNoise
 * \brief Class for OTPScommonNoise monitoring histograms
 */
class DQMHistogramOTPScommonNoise : public DQMHistogramBase
{
  public:
    /*!
     * constructor
     */
    DQMHistogramOTPScommonNoise();

    /*!
     * destructor
     */
    ~DQMHistogramOTPScommonNoise();

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

  private:
    DetectorContainer*    fDetectorContainer;
};
#endif
