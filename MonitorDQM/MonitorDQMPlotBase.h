/*!
  \file                MonitorDQMPlotBase.h
  \brief               base class to create and fill monitoring histograms
  \author              Fabio Ravera, Lorenzo Uplegger
  \version             1.0
  \date                6/5/19
  Support :            mail to : fabio.ravera@cern.ch

*/

#ifndef __MonitorDQMPlotBASE_H__
#define __MonitorDQMPlotBASE_H__

#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "../MonitorUtils/DetectorMonitorConfig.h"
#include "../RootUtils/GraphContainer.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../System/SystemController.h"
#include "../Utils/Container.h"

#include <TAxis.h>
#include <TDatime.h>
#include <TGraph.h>

class DetectorDataContainer;
class DetectorContainer;
class TFile;

/*!
 * \class MonitorDQMPlotBase
 * \brief Base class for monitoring histograms
 */
class MonitorDQMPlotBase
{
  public:
    /*!
     * constructor
     */
    MonitorDQMPlotBase() { ; }

    /*!
     * destructor
     */
    virtual ~MonitorDQMPlotBase() { ; }

    /*!
     * \brief Book histograms
     * \param theDetectorStructure : Container of the Detector structure
     */
    virtual void book(TFile* outputFile, const DetectorContainer& theDetectorStructure, const DetectorMonitorConfig& detectorMonitorConfig) = 0;

    /*!
     * \brief Book histograms
     * \param configurationFileName : xml configuration file
     */
    virtual bool fill(std::vector<char>& dataBuffer) = 0;

    /*!
     * \brief SAve histograms
     * \param outFile : ouput file name
     */
    virtual void process() = 0;

    /*!
     * \brief Book histograms
     * \param configurationFileName : xml configuration file
     */
    virtual void reset(void) = 0;

    double findValueInSettings(const Ph2_System::SettingsMap& settingsMap, const std::string name, double defaultValue = 0.) const
    {
        auto setting = settingsMap.find(name);
        return (setting != std::end(settingsMap) ? boost::any_cast<double>(setting->second) : defaultValue);
    }

    uint32_t getTimeStampForRoot(time_t rawTime)
    {
        struct tm* timeinfo = localtime(&rawTime);
        char       timeStampString[80];
        strftime(timeStampString, sizeof(timeStampString), "%Y-%m-%d %H:%M:%S", timeinfo);

        TDatime rootTime(timeStampString);
        return rootTime.Convert();
    }

  protected:
    void bookImplementer(TFile*                   theOutputFile,
                         const DetectorContainer& theDetectorStructure,
                         DetectorDataContainer&   dataContainer,
                         GraphContainer<TGraph>&  graphContainer,
                         const char*              XTitle = nullptr,
                         const char*              YTitle = nullptr)
    {
        graphContainer.fTheGraph->GetXaxis()->SetTimeDisplay(1);
        graphContainer.fTheGraph->GetXaxis()->SetNdivisions(503);
        graphContainer.fTheGraph->GetXaxis()->SetTimeFormat("%Y-%m-%d %H:%M:%S");
        graphContainer.fTheGraph->GetXaxis()->SetTimeOffset(0, "gmt");
        if(XTitle != nullptr) graphContainer.fTheGraph->GetXaxis()->SetTitle(XTitle);
        if(YTitle != nullptr)
        {
            graphContainer.setNameTitle("DQM_" + std::string{YTitle}, "DQM_" + std::string{YTitle});
            graphContainer.fTheGraph->GetYaxis()->SetTitle(YTitle);
        }
        graphContainer.fTheGraph->SetMarkerStyle(20);
        graphContainer.fTheGraph->SetMarkerSize(0.4);

        RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, dataContainer, graphContainer);
    }
};

#endif
