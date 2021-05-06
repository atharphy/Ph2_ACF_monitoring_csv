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
#include <vector>

#include "../RootUtils/GraphContainer.h"
#include "../Utils/Container.h"
#include "../System/SystemController.h"
#include "../MonitorUtils/DetectorMonitorConfig.h"

#include <TDatime.h>
#include <unistd.h>

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
        return (setting != std::end(settingsMap) ? setting->second : defaultValue);
    }

    uint32_t getTimeStampForRoot(time_t rawTime)
    {
        struct tm* timeinfo = localtime(&rawTime);
        char       timeStampString[80];
        strftime(timeStampString, sizeof(timeStampString), "%Y-%m-%d %H:%M:%S", timeinfo);

        TDatime rootTime(timeStampString);
        return rootTime.Convert();
    }
    
};

#endif
