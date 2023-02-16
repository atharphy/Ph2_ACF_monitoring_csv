/*!
        \file                MonitorDQMPlotCBC.h
        \brief               DQM class for DQM example -> use it as a templare
        \author              Fabio Ravera
        \date                25/7/19
        Support :            mail to : fabio.ravera@cern.ch
*/

#ifndef __MonitorDQMPlotCBC_H__
#define __MonitorDQMPlotCBC_H__
#include "MonitorDQM/MonitorDQMPlotBase.h"
#include "Utils/Container.h"
#include "Utils/DataContainer.h"

class TFile;

/*!
 * \class MonitorDQMPlotCBC
 * \brief Class for DQMExample monitoring Plots
 */
class MonitorDQMPlotCBC : public MonitorDQMPlotBase
{
  public:
    /*!
     * constructor
     */
    MonitorDQMPlotCBC();

    /*!
     * destructor
     */
    ~MonitorDQMPlotCBC();

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
    bool fill(std::vector<char>& dataBuffer) override;

    /*!
     * \brief process : do something with the Plot like colors, fit, drawing canvases, etc
     */
    void process() override;

    /*!
     * \brief Reset Plot
     */
    void reset(void) override;

    /*!
     * \brief fillCBCRegisterPlots
     * \param theCBCRegisterContainer : Container with the hits you want to plot
     * \param timeStamp : timeStamp
     */
    void fillCBCRegisterPlots(DetectorDataContainer& theCBCRegisterContainer, const std::string& registerName);
    void fillLpGBTRegisterPlots(DetectorDataContainer& theCBCRegisterContainer, const std::string& registerName);

  private:
    std::map<std::string, DetectorDataContainer> fCBCRegisterMonitorPlotMap;
    std::map<std::string, DetectorDataContainer> fLpGBTRegisterMonitorPlotMap;
    const DetectorContainer*    fDetectorContainer;

    void bookCBCPlots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName);
    void bookLpGBTPlots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName);
};
#endif
