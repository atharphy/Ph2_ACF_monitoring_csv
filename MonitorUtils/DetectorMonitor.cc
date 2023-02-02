#include "MonitorUtils//DetectorMonitor.h"
#include "Utils/Utilities.h"
#ifdef __USE_ROOT__
#include <TFile.h>
#endif

DetectorMonitor::DetectorMonitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig) : fDetectorMonitorConfig(theDetectorMonitorConfig)
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

    fMonitorFileName = monitorOutputDir + "/" + "MonitorDQM" + currentDateTime() + ".root";
    fOutputFile      = new TFile(fMonitorFileName.c_str(), "RECREATE");
#endif

    fTheSystemController = theSystemController;
    fKeepRunning         = true;
    startMonitor         = false;
}

DetectorMonitor::~DetectorMonitor()
{
    LOG(INFO) << BOLDRED << ">>> Destroying monitoring <<<" << RESET;
    DetectorMonitor::stopRunning();
    while(fMonitorFuture.wait_for(std::chrono::milliseconds(fDetectorMonitorConfig.fSleepTimeMs)) != std::future_status::ready)
    { LOG(INFO) << GREEN << "\t--> Waiting for monitoring to be completed..." << RESET; }
#ifdef __USE_ROOT__
    fOutputFile->Write();
    // fOutputFile->Close();
    // delete fOutputFile;
    // fOutputFile = nullptr;
    delete fMonitorPlotDQM;
#endif
}

void DetectorMonitor::operator()()
{
    while(fKeepRunning == true)
    {
        if(startMonitor == true) runMonitor();
        std::this_thread::sleep_for(std::chrono::milliseconds(fDetectorMonitorConfig.fSleepTimeMs));
    }
}

void DetectorMonitor::forkMonitor() { fMonitorFuture = std::async(std::launch::async, std::ref(*this)); }

time_t DetectorMonitor::DetectorMonitor::getTimeStamp()
{
    time_t rawtime;
    time(&rawtime);
    return rawtime;
}

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
