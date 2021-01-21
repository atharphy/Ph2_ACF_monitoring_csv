#include "../Utils/ConsoleColor.h"
#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"
#include "../Utils/easylogging++.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <inttypes.h>
#include <mutex>
#include <thread>

#ifdef __ANTENNA__
#include "Antenna.h"
#endif
#ifdef __HTTP__
#include "THttpServer.h"
#endif
#include "TGraph.h"
#include "TH1.h"
#include "TSystem.h"

#define CHIPSLAVE 4

using namespace CommandLineProcessing;

using namespace std;
INITIALIZE_EASYLOGGINGPP

// for threading
std::atomic<bool> gMonitoringRun;
std::thread       gThread;
std::mutex        gmutex;
int               gInterval = 3;

#ifdef __ANTENNA__
Antenna gAntenna;
#endif

#ifdef __HTTP__
THttpServer* gHttpServer;
#endif

std::ofstream gFile;
TGraph*       cTGraph;
TGraph*       cIGraph;

const std::string formatDateTime()
{
    struct tm tstruct;
    char      buf[80];
    time_t    cNow = std::time(nullptr);
    tstruct        = *localtime(&cNow);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%d-%m-%y %H:%M", &tstruct);
    return buf;
}

void print_choice()
{
    std::cout << BOLDYELLOW << "Press one of the following keys:" << std::endl;
    std::cout << "\t[h]elp:  print this list" << std::endl;
    std::cout << "\t[s]tart the monitoring" << std::endl;
    std::cout << "\t[e]nd the monitoring" << std::endl;
    std::cout << "\t[q]uit this application" << std::endl;
    std::cout << RESET << std::endl;
}

void monitoring_workloop()
{
#ifdef __ANTENNA__

    while(gMonitoringRun.load())
    {
        gmutex.lock();
        float  cTemp    = gAntenna.GetHybridTemperature(CHIPSLAVE);
        float  cCurrent = gAntenna.GetHybridCurrent(CHIPSLAVE);
        time_t cNow     = std::time(nullptr);
        cTGraph->SetPoint(cTGraph->GetN(), cNow, cTemp);
        cIGraph->SetPoint(cIGraph->GetN(), cNow, cCurrent);
        gFile << cNow << "\t" << cTemp << "\t" << cCurrent << std::endl;
        std::cout << cNow << "\t" << cTemp << "\t" << cCurrent << std::endl;
        cTGraph->GetHistogram()->GetXaxis()->SetLimits(cNow - 60 * 30, cNow + 2 * 60);
        cIGraph->GetHistogram()->GetXaxis()->SetLimits(cNow - 60 * 30, cNow + 2 * 60);
        cTGraph->GetHistogram()->GetXaxis()->SetTitle("Time [min]");
        cIGraph->GetHistogram()->GetXaxis()->SetTitle("Time [min]");
        cTGraph->GetHistogram()->GetXaxis()->SetTimeDisplay(1);
        cIGraph->GetHistogram()->GetXaxis()->SetTimeDisplay(1);
        cTGraph->GetHistogram()->GetYaxis()->SetTitle("Temperature [C]");
        cIGraph->GetHistogram()->GetYaxis()->SetTitle("Current [mA]");
        cTGraph->GetHistogram()->GetXaxis()->SetTimeFormat("%H:%M");
        cIGraph->GetHistogram()->GetXaxis()->SetTimeFormat("%H:%M");
#ifdef __HTTP__
        gSystem->ProcessEvents();
        // gHttpServer->ProcessRequests();
#endif
        gmutex.unlock();

        if(!gMonitoringRun.load())
            break;
        else
            std::this_thread::sleep_for(std::chrono::seconds(gInterval));
    }

#endif
}

void StartMonitoring()
{
    if(!gMonitoringRun.load())
    {
        gMonitoringRun = true;
        gThread        = std::thread(&monitoring_workloop);
        LOG(INFO) << "Starting Monitoring workloop!";
    }
    else
        LOG(INFO) << "Monitoring workloop already running";
}

void StopMonitoring()
{
    if(gMonitoringRun.load())
    {
        gMonitoringRun = false;

        if(gThread.joinable()) gThread.join();

        LOG(INFO) << "Stopping Monitoring workloop!";
    }
}

bool command_processor(std::string pInput)
{
    // all options without arguments
    // print help dialog
    if(pInput == "h")
    {
        print_choice();
        return false;
    }

    // start monitoring workloop
    else if(pInput == "s")
    {
        StartMonitoring();
        LOG(INFO) << "Starting the monitoring";
        return false;
    }

    // end monitoring
    else if(pInput == "e")
    {
        StopMonitoring();
        LOG(INFO) << "Stopping the monitoring";
        return false;
    }
    // quit application
    else if(pInput == "q")
    {
        StopMonitoring();
        LOG(INFO) << "Quitting application";
        return true;
    }
    else
    {
        LOG(INFO) << "Unrecognized option";
        print_choice();
        return false;
    }
}

void workloop_local()
{
    print_choice();

    bool cQuit = false;

    while(!cQuit)
    {
        std::cout << ">";
        std::string cInput = "";
        std::getline(std::cin, cInput);
        std::cout << std::endl << "\r";

        std::stringstream ss;
        cQuit = command_processor(cInput);
    }
}

int main(int argc, char* argv[])
{
#ifdef __ANTENNA__
    // configure the logger
    el::Configurations conf("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  CBC3 UIB Temerpature and Current Monitor");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("interval", "polling interval - can not be smaller than 3 seconds (default value)", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("interval", "i");

    cmd.defineOption("port", "THttpServer port - default: 9099", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("port", "p");

    cmd.defineOption("output", "Output file - default: T_log_timestamp.txt", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("noserver", "disables use of THttp Server");
    cmd.defineOptionAlternative("noserver", "n");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // bool cUseTHttpServer = cmd.foundOption ("noserver") ? false : true;
    std::string cOutputFile;
    time_t      cTimestamp;
    int         cServerPort = 9099;

    // now query the parsing results
    cOutputFile = (cmd.foundOption("output")) ? cmd.optionValue("output") : "T_log_";
    cOutputFile += currentDateTime();
    cOutputFile += ".txt";

    gInterval = (cmd.foundOption("interval")) ? atoi(cmd.optionValue("interval").c_str()) : 3;

    if(gInterval < 3)
    {
        LOG(INFO) << RED
                  << "Error, monitoring interval can not be smaller than 3 seconds due to HW restrictions - setting 3 "
                     "seconds!"
                  << RESET;
        gInterval = 3;
    }

    cServerPort = (cmd.foundOption("port")) ? atoi(cmd.optionValue("port").c_str()) : 9099;

    // here create TGraphs for T and I monitoring
    cTGraph = new TGraph();
    cTGraph->SetName("HybridTemperature");
    cTGraph->SetTitle("Temperature on Hybrid");
    cTGraph->SetLineColor(kBlue);
    cTGraph->SetLineWidth(3);

    cIGraph = new TGraph();
    cIGraph->SetName("HybridCurrent");
    cIGraph->SetTitle("Hybrid Current Consumption");
    cIGraph->SetLineColor(kGreen);
    cIGraph->SetLineWidth(3);

    ////now, initialize the THttpServer
#ifdef __HTTP__
    char hostname[HOST_NAME_MAX];

    try
    {
        gHttpServer = new THttpServer(Form("http:%d", cServerPort));
        gHttpServer->SetReadOnly(true);
        // fHttpServer->SetTimer ( pRefreshTime, kTRUE );
        gHttpServer->SetTimer(100, kTRUE);
        gHttpServer->SetJSROOT("https://root.cern.ch/js/latest/");

        // configure the server
        // see: https://root.cern.ch/gitweb/?p=root.git;a=blob_plain;f=tutorials/http/httpcontrol.C;hb=HEAD
        gHttpServer->SetItemField("/", "_monitoring", "5000");
        gHttpServer->SetItemField("/", "_layout", "grid1x2");
        gHttpServer->SetItemField("/", "_drawitem", "[HybridTemperature,HybridCurrent]");

        gethostname(hostname, HOST_NAME_MAX);

        if(gHttpServer != nullptr)
        {
            gHttpServer->Register("/", cTGraph);
            gHttpServer->Register("/", cIGraph);
        }

        gSystem->ProcessEvents();
        // gHttpServer->ProcessRequests();
    }
    catch(std::exception& e)
    {
        LOG(ERROR) << "Exception when trying to start THttpServer: " << e.what();
    }

    LOG(INFO) << "Opening THttpServer on port " << cServerPort << ". Point your browser to: " << BOLDGREEN << hostname << ":" << cServerPort << RESET;
#else
    LOG(INFO) << "Error, ROOT version < 5.34 detected or not compiled with Http Server support!"
              << " No THttpServer available! - The webgui will fail to show plots!";
    LOG(INFO) << "ROOT must be built with '--enable-http' flag to use this feature.";
#endif

    ////open the file for dump of the values
    gmutex.lock();
    gFile.open(cOutputFile.c_str(), std::fstream::app | std::fstream::out);
    bool cFileOpen = gFile.is_open();

    // if (fFile.fail() )
    if(!cFileOpen)
        LOG(ERROR) << RED << "Error, could not open log file " << cOutputFile << RESET;
    else
    {
        cTimestamp = std::time(nullptr);
        LOG(INFO) << GREEN << "Successfully opened log file " << cOutputFile << RESET;
        gFile << "## File Format: ## " << std::endl;
        gFile << "## Timestamp\tTemperature\tCurrent ##" << std::endl;
        gFile << "Logfile opened " << cTimestamp << " which is " << formatDateTime() << std::endl;
    }

    gmutex.unlock();

    // THttpServer running, log file open, Graphs initialized, all ok - now just initialize the Antenna and get things
    // ready then start the workloop and keep the main thread listening for commands
    gAntenna.initializeAntenna();
    StartMonitoring();

    workloop_local();

#else
    LOG(ERROR) << "Error, this needs to be linked against the Antanna driver!";
#endif

    gFile.close();
    return 0;
}
