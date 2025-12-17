#include "MonitorUtils//DetectorMonitor.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/Utilities.h"
#ifdef __USE_ROOT__
#include <TFile.h>
#include <algorithm>
#endif

DetectorMonitor::DetectorMonitor(const Ph2_System::SystemController* theSystemController, const DetectorMonitorConfig& theDetectorMonitorConfig) : fDetectorMonitorConfig(theDetectorMonitorConfig)
{
#ifdef __USE_ROOT__
    std::string monitorOutputDir = "MonitorResults";
    std::string cCommand         = "mkdir -p " + monitorOutputDir;
    try
    {
        system(cCommand.c_str());
    }
    catch(std::exception& e)
    {
        LOG(ERROR) << "Exceptin when trying to create MonitorResults directory: " << e.what();
    }

    std::string timeStamp = getTimeStampString();
    std::replace(timeStamp.begin(), timeStamp.end(), ' ', '_');
    std::replace(timeStamp.begin(), timeStamp.end(), ':', '-');
    if(std::getenv("GIPHT_RESULT_FOLDER"))
    {
        LOG(INFO) << "OT Module GUI (GIPHT) result directory environmental variable set: " << std::getenv("GIPHT_RESULT_FOLDER") << RESET;
        monitorOutputDir = std::getenv("GIPHT_RESULT_FOLDER");
        LOG(INFO) << "Use " << monitorOutputDir << " for file dump" << RESET;
    }
    else { LOG(DEBUG) << "Use default directory " << monitorOutputDir << " for file dump" << RESET; }

    fMonitorFileName = monitorOutputDir + "/" + "MonitorDQM_" + timeStamp + ".root";
    fOutputFile      = new TFile(fMonitorFileName.c_str(), "RECREATE");
#endif

    fTheSystemController = theSystemController;
    fKeepRunning         = true;
    fEnableMonitor       = false;
    fIsMonitorRunning    = false;
}

DetectorMonitor::~DetectorMonitor()
{
    LOG(INFO) << BOLDRED << ">>> Destroying monitoring <<<" << RESET;
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        std::string  doneWithRunMessage = END_OF_TRANSMISSION_MESSAGE;
        PacketHeader thePacketHeader;
        thePacketHeader.addPacketHeader(doneWithRunMessage);
        fTheSystemController->fMonitorDQMStreamer->broadcast(doneWithRunMessage);
    }
#ifdef __USE_ROOT__
    fOutputFile->Write();
    LOG(INFO) << GREEN << "Closing monitor result file: " << BOLDYELLOW << fMonitorFileName << RESET;
    delete fMonitorPlotDQM;
#endif
}

void DetectorMonitor::operator()()
{
    while(fKeepRunning == true)
    {
        if(fEnableMonitor == true)
        {
            fIsMonitorRunning = true;
            runMonitor();
            if(!fEnableMonitor) // Stop monitoring immediately
                continue;
        }
        else { fIsMonitorRunning = false; }

        int waitTime = fDetectorMonitorConfig.fSleepTimeMs;
        while(waitTime > 0 && fKeepRunning && (fIsMonitorRunning || !fEnableMonitor)) // Start first loop ~immediately with ::startMonitoring
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(fLoopWaitTimeMs));
            waitTime -= fLoopWaitTimeMs;
        }
    }
}

void DetectorMonitor::forkMonitor() { fMonitorFuture = std::async(std::launch::async, std::ref(*this)); }

std::string DetectorMonitor::getMonitorName()
{
    int32_t     status;
    std::string className     = abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status);
    std::string emptyTemplate = "<> ";
    size_t      found         = className.find(emptyTemplate);
    while(found != std::string::npos)
    {
        className.erase(found, emptyTemplate.length());
        found = className.find(emptyTemplate);
    }
    return className;
}

std::string DetectorMonitor::getMonitorFileName()
{
#ifdef __USE_ROOT__
    return fMonitorFileName;
#else
    return "";
#endif
}

void DetectorMonitor::waitForMonitorToStop()
{
    int waitTime  = fMaximumStopTentatives * fDetectorMonitorConfig.fSleepTimeMs;
    int printTime = -1;
    while(fMonitorFuture.wait_for(std::chrono::milliseconds(fLoopWaitTimeMs)) != std::future_status::ready && waitTime > 0)
    {
        if(printTime < 0)
        {
            LOG(INFO) << GREEN << "\t--> Waiting for monitoring to be completed..." << RESET;
            printTime = 10000;
        }
        waitTime -= fLoopWaitTimeMs;
        printTime -= fLoopWaitTimeMs;
    }
    if(waitTime <= 0) throw std::runtime_error("Failed to stop monitoring process");
}

void DetectorMonitor::pauseMonitoring()
{
    fEnableMonitor = false;

    int waitTime  = fMaximumStopTentatives * fDetectorMonitorConfig.fSleepTimeMs;
    int printTime = -1;
    while(fIsMonitorRunning && waitTime > 0)
    {
        if(printTime < 0)
        {
            LOG(INFO) << GREEN << "\t--> Waiting for monitoring to pause..." << RESET;
            printTime = 10000;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(fLoopWaitTimeMs));
        waitTime -= fLoopWaitTimeMs;
        printTime -= fLoopWaitTimeMs;
    }
    if(waitTime <= 0) throw std::runtime_error("Failed to pause monitoring process");
}
