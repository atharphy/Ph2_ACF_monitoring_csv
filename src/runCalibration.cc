#include <cstring>

#include "Utils/Timer.h"
#include "Utils/Utilities.h"
#include "Utils/argvparser.h"
#include "Utils/easylogging++.h"
#include "boost/format.hpp"
#include "miniDAQ/MiddlewareStateMachine.h"

#include "TApplication.h"
#include "TROOT.h"

using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

sig_atomic_t killProcess  = 0;
sig_atomic_t runCompleted = 0;

void interruptHandler(int handler) { killProcess = 1; }

void killProcessFunction(MiddlewareStateMachine* theMiddlewareStateMachine)
{
    while(1)
    {
        usleep(250000);
        if(killProcess || runCompleted) break;
    }
    if(killProcess)
    {
        theMiddlewareStateMachine->abort();
        abort();
    }
}

int returnRunNumber(std::string cFileName)
{
    std::string   cLine;
    int           cRunNumber = -1;
    std::ifstream cStream(cFileName);
    if(cStream.is_open())
    {
        while(std::getline(cStream, cLine))
        {
            std::istringstream cIStream(cLine);
            cIStream >> cRunNumber;
            // LOG(INFO) << BOLDMAGENTA << cRunNumber << RESET;
        }
    }

    cRunNumber++;
    std::ofstream cRunLog;
    cRunLog.open(cFileName, std::fstream::app);
    cRunLog << cRunNumber << "\n";
    cRunLog.close();

    return cRunNumber;
}

int main(int argc, char* argv[])
{
    MiddlewareStateMachine theMiddlewareStateMachine;

    if(std::getenv("PH2ACF_BASE_DIR") == nullptr)
    {
        std::cout << "You must source setup.sh or export the PH2ACF_BASE_DIR environmental variable. Exiting..." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string baseDir = std::string(std::getenv("PH2ACF_BASE_DIR")) + "/";
    std::string binDir  = baseDir + "bin/";

    // configure the logger
    el::Configurations conf(baseDir + "settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF main script");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hw Description File", ArgvParser::OptionRequiresValue | ArgvParser::OptionRequired);
    cmd.defineOptionAlternative("file", "f");

    std::string calibrationHelpMessage = "Calibration to run. List of available calibrations:\n";
    for(const auto& calibration: theMiddlewareStateMachine.getCombinedCalibrationFactory().getAvailableCalibrations()) calibrationHelpMessage += (calibration + "\n");

    cmd.defineOption("calibration", calibrationHelpMessage, ArgvParser::OptionRequiresValue | ArgvParser::OptionRequired);
    cmd.defineOptionAlternative("calibration", "c");

    cmd.defineOption("allChan", "Do calibration using all channels? Default: false", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("allChan", "a");

    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(EXIT_FAILURE);
    }

    // now query the parsing results
    std::string configurationFile = cmd.optionValue("file");
    std::string cDirectory        = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    cDirectory += cmd.optionValue("calibration");

    bool batchMode = (cmd.foundOption("batch")) ? true : false;

    Timer cGlobalTimer;
    cGlobalTimer.start();

    std::thread softKillThread(killProcessFunction, &theMiddlewareStateMachine);
    softKillThread.detach();

    struct sigaction act;
    act.sa_handler = interruptHandler;
    sigaction(SIGINT, &act, NULL);

    enum
    {
        INITIAL,
        HALTED,
        CONFIGURED,
        RUNNING,
        STOPPED
    };
    int stateMachineStatus = INITIAL;

    theMiddlewareStateMachine.initialize();

    // int main ( int argc, char* argv[] )
    // std::cout << argc << "-" << argv[2] << std::endl;
    // exit(0);
    int   tAppArgc = 1;
    char* tAppArgv[2];
    tAppArgv[0] = argv[0];
    tAppArgv[1] = (char*)"-b";
    if(batchMode) tAppArgc = 2;
    TApplication theApp("App", &tAppArgc, tAppArgv);

    stateMachineStatus = HALTED;

    bool done = false;
    while(!done)
    {
        switch(stateMachineStatus)
        {
        case HALTED:
        {
            std::string calibrationName   = cmd.optionValue("calibration");
            std::string configurationFile = cmd.optionValue("file");
            theMiddlewareStateMachine.configure(calibrationName, configurationFile);
            stateMachineStatus = CONFIGURED;
            break;
        }
        case CONFIGURED:
        {
            int runNumber = returnRunNumber("RunNumbers.dat");
            theMiddlewareStateMachine.start(runNumber);
            stateMachineStatus = RUNNING;
            break;
        }
        case RUNNING:
        {
            if(cmd.optionValue("calibration") != "psphysics" && cmd.optionValue("calibration") != "2sphysics")
            {
                while(theMiddlewareStateMachine.status() == MiddlewareStateMachine::Status::RUNNING) { usleep(5e5); }
            }
            else
                usleep(20e6);
            std::cout << __PRETTY_FUNCTION__ << "Supervisor Sending Stop!!!" << std::endl;
            theMiddlewareStateMachine.stop();
            stateMachineStatus = STOPPED;
            break;
        }
        case STOPPED:
        {
            theMiddlewareStateMachine.halt();
            done = true;
            break;
        }
        }
    }

    signal(SIGINT, SIG_DFL);
    runCompleted = 1;

    if(!batchMode) theApp.Run();

    cGlobalTimer.stop();
    cGlobalTimer.show("Total execution time: ");

    return EXIT_SUCCESS;
}
