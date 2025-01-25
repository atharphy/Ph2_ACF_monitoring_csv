/*!
        \file                DQMHistogramOTPatternCheckerTest.h
        \brief               DQM class for OTPatternCheckerTest
        \author              Fabio Ravera
        \date                24/01/25
*/

#ifndef DQMHistogramOTPatternCheckerTest_h_
#define DQMHistogramOTPatternCheckerTest_h_
#include "DQMUtils/DQMHistogramBase.h"
#include "Utils/Container.h"
#include "Utils/DataContainer.h"

class TFile;

/*!
 * \class DQMHistogramOTPatternCheckerTest
 * \brief Class for OTPatternCheckerTest monitoring histograms
 */
class DQMHistogramOTPatternCheckerTest : public DQMHistogramBase
{
  public:
    /*!
     * constructor
     */
    DQMHistogramOTPatternCheckerTest();

    /*!
     * destructor
     */
    ~DQMHistogramOTPatternCheckerTest();

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

    void fillErrorCounter(DetectorDataContainer& theErrorCountainer, uint8_t line);


  private:
    DetectorContainer*    fDetectorContainer;
    DetectorDataContainer fPatternErrorRateHistogram;
    DetectorDataContainer fPatternBitCounterHistogram;
    uint8_t fNumberOfLines;
};
#endif
