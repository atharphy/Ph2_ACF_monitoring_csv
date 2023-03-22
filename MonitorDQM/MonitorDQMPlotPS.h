/*!
        \file                MonitorDQMPlotPS.h
        \brief               DQM class for DQM example -> use it as a templare
        \author              Fabio Ravera
        \date                25/7/19
        Support :            mail to : fabio.ravera@cern.ch
*/

#ifndef __MonitorDQMPlotPS_H__
#define __MonitorDQMPlotPS_H__
#include "MonitorDQM/MonitorDQMPlotBase.h"
#include "Utils/Container.h"
#include "Utils/DataContainer.h"

class TFile;

/*!
 * \class MonitorDQMPlotPS
 * \brief Class for DQMExample monitoring Plots
 */
class MonitorDQMPlotPS : public MonitorDQMPlotBase
{
  public:
    /*!
     * constructor
     */
    MonitorDQMPlotPS();

    /*!
     * destructor
     */
    ~MonitorDQMPlotPS();

    /*!
     * \brief Book Plots
     * \param theOutputFile : where Plots will be saved
     * \param theDetectorStructure : Detector container as obtained after file parsing, used to create Plots for
     * all board/chip/hybrid/channel \param pSettingsMap : setting as for Tool setting map in case coe informations are
     * needed (i.e. FitSCurve)
     */
    void book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const DetectorMonitorConfig& detectorMonitorConfig) override;

    /*!
     * \brief fill : fill Plots from TCP stream, need to be overwritten to avoid compilation errors, but it is not
     * needed if you do not fo into the SoC \param dataBuffer : vector of char with the TCP datastream
     */
    bool fill(std::string& inputStream) override;

    /*!
     * \brief process : do something with the Plot like colors, fit, drawing canvases, etc
     */
    void process() override;

    /*!
     * \brief Reset Plot
     */
    void reset(void) override;

    /*!
     * \brief fillPSRegisterPlots
     * \param thePSRegisterContainer : Container with the hits you want to plot
     * \param timeStamp : timeStamp
     */
    void fillPSRegisterPlots(DetectorDataContainer& thePSRegisterContainer, const std::string& registerName);
    void fillSSA2RegisterPlots(DetectorDataContainer& thePSRegisterContainer, const std::string& registerName);
    void fillLpGBTRegisterPlots(DetectorDataContainer& thePSRegisterContainer, const std::string& registerName);

  private:
    std::map<std::string, DetectorDataContainer> fPSRegisterMonitorPlotMap;
    std::map<std::string, DetectorDataContainer> fSSA2RegisterMonitorPlotMap;
    std::map<std::string, DetectorDataContainer> fLpGBTRegisterMonitorPlotMap;
    const DetectorContainer*                     fDetectorContainer;

    void bookPSPlots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName);
    void bookSSA2Plots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName);
    void bookLpGBTPlots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName);
};
#endif
