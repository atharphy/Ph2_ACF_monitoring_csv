#include "../DQMUtils/DQMInterface.h"
#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/Module.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/ChipInterface.h"
#include "../Utils/MiddlewareInterface.h"
#include "../Utils/argvparser.h"

#include <cstring>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <TApplication.h>

#include "../Utils/easylogging++.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

INITIALIZE_EASYLOGGINGPP

static bool  controlC            = false;
static pid_t runControllerPid    = -1;
static int   runControllerStatus = 0;

void interruptHandler(int handler)
{
    std::cout << __PRETTY_FUNCTION__ << " Sig handler: " << handler << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "Run controller pid: " << runControllerPid << " status: " << runControllerStatus << std::endl;
    if(runControllerStatus != 0 && runControllerPid > 0)
    {
        std::cout << __PRETTY_FUNCTION__ << "Killing run controller pid: " << runControllerPid << " status: " << runControllerStatus << std::endl;
        kill(runControllerPid, SIGKILL);
    }
    exit(EXIT_FAILURE);

    controlC = true;
}

bool checkExitStatus(int status, std::string programName)
{
    if(WIFEXITED(status) && !WEXITSTATUS(status))
    {
        std::cout << __PRETTY_FUNCTION__ << programName << " executed successfully." << std::endl;
        return true;
    }
    else if(WIFEXITED(status) && WEXITSTATUS(status))
    {
        if(WEXITSTATUS(status) == 127)
        {
            // execv failed
            std::cout << __PRETTY_FUNCTION__ << programName << " execv failed." << std::endl;
            return false;
        }
        else
        {
            std::cout << __PRETTY_FUNCTION__ << programName << " terminated normally, but returned a non-zero status." << std::endl;
            return true;
        }
    }
    else
    {
        std::cout << __PRETTY_FUNCTION__ << programName << " didn't terminate normally. Status: " << status << std::endl;
        return false;
    }
}

int main(int argc, char* argv[])
{
    if(getenv("BASE_DIR") == nullptr)
    {
        std::cout << "You must source setup.sh or export the BASE_DIR environmental variable. Exiting..." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string baseDir = std::string(getenv("BASE_DIR")) + "/";
    std::string binDir  = baseDir + "bin/";

    // configure the logger
    el::Configurations conf(baseDir + "settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  calibration routine using K. Uchida's algorithm or a fast algorithm");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hw Description File", ArgvParser::OptionRequiresValue | ArgvParser::OptionRequired);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("calibration", "Calibration to run", ArgvParser::OptionRequiresValue | ArgvParser::OptionRequired);
    cmd.defineOptionAlternative("calibration", "c");

    cmd.defineOption("output", "Output Directory. Default value: Results", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("allChan", "Do calibration using all channels? Default: false", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("allChan", "a");

    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile    = cmd.optionValue("file");
    std::string cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    cDirectory += cmd.optionValue("calibration");

    bool batchMode = (cmd.foundOption("batch")) ? true : false;

    // pid_t  runControllerPid = -1;
    // pid_t  dqmControllerPid = -1;
    int runControllerPidStatus = 0;

    std::cout << __PRETTY_FUNCTION__ << "Forking RunController" << std::endl;
    runControllerPid = fork();
    if(runControllerPid == -1) // pid == -1 means error occured
    {
        LOG(ERROR) << "Can't fork RunController, error occured";
        exit(EXIT_FAILURE);
    }
    else if(runControllerPid == 0) // pid == 0 means child process created
    {
        // getpid() returns process id of calling process
        // printf("Child runControllerPid, pid = %u\n",getpid());

        // the argv list first argument should point to
        // filename associated with file being executed
        // the array pointer must be terminated by NULL
        // pointer
        char* argv[] = {(char*)"RunController", NULL};

        // the execv() only return if error occured.
        // The return value is -1
        execv((binDir + "RunController").c_str(), argv);
        LOG(ERROR) << "Can't run RunController, error occured";
        exit(0);
    }
    // usleep(10000000);
    //	std::cout << "forking dqm" << std::endl;
    //	dqmControllerPid = fork();
    //	if (dqmControllerPid == -1)// pid == -1 means error occured
    //	{
    //		LOG (ERROR) << "Can't fork DQMHistogrammer, error occured";
    //		exit(EXIT_FAILURE);
    //	}
    //
    //	else if (dqmControllerPid == 0)// pid == 0 means child process created
    //	{
    //		char * argv[] = {"DQMController", NULL};
    //		execv((binDir + "DQMController").c_str(),NULL);
    //		LOG (ERROR) << "Can't run DQMController, error occured";
    //		exit(EXIT_FAILURE);
    //	}
    //
    //	// a positive number is returned for the pid of
    //	// parent process
    //	// getppid() returns process id of parent of
    //	// calling process
    //	printf("Parent process, pid = %u\n",getppid());

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
    int                 stateMachineStatus = INITIAL;
    MiddlewareInterface theMiddlewareInterface("127.0.0.1", 5000);
    theMiddlewareInterface.initialize();

    // int main ( int argc, char* argv[] )
    // std::cout << argc << "-" << argv[2] << std::endl;
    // exit(0);
    int   tAppArgc = 1;
    char* tAppArgv[2];
    tAppArgv[0] = argv[0];
    tAppArgv[1] = (char*)"-b";
    if(batchMode) tAppArgc = 2;
    TApplication theApp("App", &tAppArgc, tAppArgv);

    DQMInterface theDQMInterface;

    stateMachineStatus = HALTED;

    // int runControllerPidStatus = 0;
    // int dqmControllerPidStatus = 0;
    std::cout << __PRETTY_FUNCTION__ << "runControllerPid: " << runControllerPid << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "runControllerPid: " << runControllerPid << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "runControllerPid: " << runControllerPid << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "runControllerPid: " << runControllerPid << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "runControllerPid: " << runControllerPid << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "runControllerPid: " << runControllerPid << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "runControllerPid: " << runControllerPid << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "runControllerPid: " << runControllerPid << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "runControllerPid: " << runControllerPid << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "runControllerPid: " << runControllerPid << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "runControllerPid: " << runControllerPid << std::endl;
    bool done = false;
    while(!done)
    {
        if(runControllerPidStatus == 0 && (runControllerPidStatus = waitpid(runControllerPid, &runControllerStatus, WNOHANG)) != 0)
        {
            std::cout << __PRETTY_FUNCTION__ << "1Run Controller status: " << runControllerStatus << std::endl;
            if(!checkExitStatus(runControllerStatus, "RunController")) exit(EXIT_FAILURE);
        }
        //		if(dqmControllerPidStatus == 0 && (dqmControllerPidStatus = waitpid(dqmControllerPid,
        //&dqmControllerStatus, WNOHANG)) != 0)
        //		{
        //			if(!checkExitStatus(dqmControllerStatus,"DQMController"))
        //			{
        //	    		kill(runControllerPid,SIGKILL);
        //	    		exit(EXIT_FAILURE);
        //			}
        //
        //		}
        //    	if(runControllerPidStatus != 0)// && dqmControllerPidStatus != 0)
        //    	{
        //    		std::cout << __PRETTY_FUNCTION__ << "3Run Controller pid status: " << runControllerPidStatus <<
        //    std::endl; 		done = true;
        //    	}
        else
        {
            std::cout << __PRETTY_FUNCTION__ << "Supervisor Run Controller status: " << runControllerStatus << std::endl;
            switch(stateMachineStatus)
            {
            case HALTED:
            {
                std::cout << __PRETTY_FUNCTION__ << "Supervisor Sending Configure!!!" << std::endl;
                theMiddlewareInterface.configure(cmd.optionValue("calibration"), baseDir + cmd.optionValue("file"));
                std::string configurationFile = baseDir + cmd.optionValue("file");
                std::string calibrationName   = cmd.optionValue("calibration");
                theDQMInterface.configure(calibrationName, configurationFile);
                stateMachineStatus = CONFIGURED;
                break;
            }
            case CONFIGURED:
            {
                std::cout << __PRETTY_FUNCTION__ << "Supervisor Sending Start!!!" << std::endl;
                std::string runNumber = "5";
                theDQMInterface.startProcessingData(runNumber);
                theMiddlewareInterface.start(runNumber);
                stateMachineStatus = RUNNING;
                break;
            }
            case RUNNING:
            {
                std::cout << __PRETTY_FUNCTION__ << "Supervisor Sending Stop!!!" << std::endl;
                usleep(2e6);
                theMiddlewareInterface.stop();
                usleep(1e6);
                theDQMInterface.stopProcessingData();
                stateMachineStatus = STOPPED;
                break;
            }
            case STOPPED:
            {
                std::cout << __PRETTY_FUNCTION__ << "Supervisor Everything Stoped!!! Exiting..." << std::endl;
                done = true;
                break;
            }
            }
            if(!done)
            {
                std::cout << __PRETTY_FUNCTION__ << "Supervisor SLEEPING!!!" << std::endl;
                usleep(1000000);
            }
        }
    }

    std::cout << __PRETTY_FUNCTION__ << "Out of supervisor state machine!. Run Controller status: " << runControllerStatus << std::endl;
    checkExitStatus(runControllerStatus, "RunController");
    // checkExitStatus(dqmControllerStatus,"DQMController");

    theApp.Run();

    return EXIT_SUCCESS;
}
