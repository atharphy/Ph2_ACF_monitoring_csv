//#include <cstring>
//#include "../HWDescription/Chip.h"
//#include "../HWDescription/Module.h"
//#include "../HWDescription/BeBoard.h"
//#include "../HWInterface/ChipInterface.h"
//#include "../HWInterface/BeBoardInterface.h"
//#include "../HWDescription/Definition.h"
//#include "../Utils/argvparser.h"
#include "../Utils/easylogging++.h"
#include <chrono>
#include <errno.h>
#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

//#include "../Utils/MiddlewareInterface.h"
//#include "../DQMUtils/DQMInterface.h"
//#include <TApplication.h>

// using namespace Ph2_HwDescription;
// using namespace Ph2_HwInterface;
// using namespace Ph2_System;
// using namespace CommandLineProcessing;
//
INITIALIZE_EASYLOGGINGPP

static bool  controlC            = false;
static pid_t networkServerPid    = -1;
static pid_t networkClientPid    = -1;
static int   networkServerStatus = 0;
static int   networkClientStatus = 0;

//========================================================================================================================
void interruptHandler(int handler)
{
    std::cout << __PRETTY_FUNCTION__ << " Sig handler: " << handler << std::endl;

    std::cout << __PRETTY_FUNCTION__ << "networkServerPid: " << networkServerPid << " status: " << networkServerStatus << std::endl;
    if(networkServerStatus != 0 && networkServerPid > 0)
    {
        std::cout << __PRETTY_FUNCTION__ << "Killing run controller pid: " << networkServerPid << " status: " << networkServerStatus << std::endl;
        kill(networkServerPid, SIGKILL);
    }
    std::cout << __PRETTY_FUNCTION__ << "networkClientPid: " << networkClientPid << " status: " << networkClientStatus << std::endl;
    if(networkClientStatus != 0 && networkClientPid > 0)
    {
        std::cout << __PRETTY_FUNCTION__ << "Killing run controller pid: " << networkClientPid << " status: " << networkClientStatus << std::endl;
        kill(networkClientPid, SIGKILL);
    }
    exit(EXIT_FAILURE);

    controlC = true;
}

//========================================================================================================================
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

//========================================================================================================================
int main(int argc, char* argv[])
{
    if(getenv("BASE_DIR") == nullptr)
    {
        std::cout << "You must source setup.sh or export the BASE_DIR environmental variable. Exiting..." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string baseDir = std::string(getenv("BASE_DIR")) + "/";
    std::string binDir  = baseDir + "bin/";

    int networkServerPidStatus = 0;
    int networkClientPidStatus = 0;

    std::cout << __PRETTY_FUNCTION__ << "Forking network_server" << std::endl;
    networkServerPid = fork();
    if(networkServerPid == -1) // pid == -1 means error occured
    {
        std::cout << __PRETTY_FUNCTION__ << "Can't fork networkServer, error occured";
        exit(EXIT_FAILURE);
    }
    else if(networkServerPid == 0) // pid == 0 means child process created
    {
        // getpid() returns process id of calling process
        // printf("Child runControllerPid, pid = %u\n",getpid());

        // the argv list first argument should point to
        // filename associated with file being executed
        // the array pointer must be terminated by NULL
        // pointer
        char* argv[] = {(char*)"network_server", NULL};

        // the execv() only return if error occured.
        // The return value is -1
        std::this_thread::sleep_for(std::chrono::seconds(2));
        execv((binDir + "network_server").c_str(), argv);
        std::cout << __PRETTY_FUNCTION__ << "Can't run network_server, error occured";
        exit(0);
    }

    std::cout << __PRETTY_FUNCTION__ << "Forking network_client" << std::endl;
    networkClientPid = fork();
    if(networkClientPid == -1) // pid == -1 means error occured
    {
        std::cout << __PRETTY_FUNCTION__ << "Can't fork networkServer, error occured";
        exit(EXIT_FAILURE);
    }
    else if(networkClientPid == 0) // pid == 0 means child process created
    {
        // getpid() returns process id of calling process
        // printf("Child runControllerPid, pid = %u\n",getpid());

        // the argv list first argument should point to
        // filename associated with file being executed
        // the array pointer must be terminated by NULL
        // pointer
        char* argv[] = {(char*)"network_client", NULL};

        // the execv() only return if error occured.
        // The return value is -1
        execv((binDir + "network_client").c_str(), argv);
        std::cout << __PRETTY_FUNCTION__ << "Can't run network_client, error occured";
        exit(0);
    }

    struct sigaction act;
    act.sa_handler = interruptHandler;
    sigaction(SIGINT, &act, NULL);

    //	enum{INITIAL,HALTED,CONFIGURED,RUNNING,STOPPED};
    //	int stateMachineStatus = INITIAL;
    bool done = false;
    while(!done)
    {
        if(networkServerPidStatus == 0 && (networkServerPidStatus = waitpid(networkServerPid, &networkServerStatus, WNOHANG)) != 0)
        {
            std::cout << __PRETTY_FUNCTION__ << "networkServerStatus: " << networkServerStatus << std::endl;
            if(!checkExitStatus(networkServerStatus, "network_server")) exit(EXIT_FAILURE);
        }
        else if(networkClientPidStatus == 0 && (networkClientPidStatus = waitpid(networkClientPid, &networkClientStatus, WNOHANG)) != 0)
        {
            std::cout << __PRETTY_FUNCTION__ << "networkClientStatus: " << networkClientStatus << std::endl;
            if(!checkExitStatus(networkClientStatus, "network_client")) exit(EXIT_FAILURE);
        }
        else
        {
            std::cout << __PRETTY_FUNCTION__ << "Supervisor networkServerStatus: " << networkServerStatus << std::endl;
            std::cout << __PRETTY_FUNCTION__ << "Supervisor networkClientStatus: " << networkClientStatus << std::endl;
            //			switch(stateMachineStatus)
            //			{
            //			case HALTED:
            //				std::cout << __PRETTY_FUNCTION__ << "Supervisor Sending Configure!!!" << std::endl;
            //				theMiddlewareInterface.configure(cmd.optionValue("calibration"), baseDir +
            // cmd.optionValue("file")); 				theDQMInterface.configure(); 				stateMachineStatus = CONFIGURED; 				break; 			case
            // CONFIGURED: 				std::cout << __PRETTY_FUNCTION__ << "Supervisor Sending Start!!!" << std::endl;
            //				theDQMInterface.startProcessingData("5");
            //				theMiddlewareInterface.start("5");
            //				stateMachineStatus = RUNNING;
            //				break;
            //			case RUNNING:
            //				std::cout << __PRETTY_FUNCTION__ << "Supervisor Sending Stop!!!" << std::endl;
            //				theMiddlewareInterface.stop();
            //				theDQMInterface.stopProcessingData();
            //				stateMachineStatus = STOPPED;
            //				break;
            //			case STOPPED:
            //				std::cout << __PRETTY_FUNCTION__ << "Supervisor Everything Stoped!!! Exiting..." <<
            // std::endl; 				done = true; 				break;
            //			}
            if(!done)
            {
                std::cout << __PRETTY_FUNCTION__ << "Supervisor SLEEPING!!!" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    std::cout << __PRETTY_FUNCTION__ << "Out of supervisor state machine!. networkServerStatus: " << networkServerStatus << std::endl;
    checkExitStatus(networkServerStatus, "network_server");
    std::cout << __PRETTY_FUNCTION__ << "Out of supervisor state machine!. networkClientStatus: " << networkClientStatus << std::endl;
    checkExitStatus(networkClientStatus, "network_client");

    return EXIT_SUCCESS;
}
