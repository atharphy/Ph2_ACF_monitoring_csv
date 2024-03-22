/*!
        \file                DQMHistogramOTPSADCCalibration.h
        \brief               DQM class for OTPSADCCalibration
        \author              Irene Zoi
        \date                22/03/24
*/

#ifndef DQMHistogramOTPSADCCalibration_h_
#define DQMHistogramOTPSADCCalibration_h_
#include "DQMUtils/DQMHistogramBase.h"
#include "Utils/Container.h"
#include "Utils/DataContainer.h"

class TFile;

/*!
 * \class DQMHistogramOTPSADCCalibration
 * \brief Class for OTPSADCCalibration monitoring histograms
 */
class DQMHistogramOTPSADCCalibration : public DQMHistogramBase
{
  public:
    /*!
     * constructor
     */
    DQMHistogramOTPSADCCalibration();

    /*!
     * destructor
     */
    ~DQMHistogramOTPSADCCalibration();

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
