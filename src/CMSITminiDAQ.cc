/*!
  \file                  CMSITminiDAQ.cc
  \brief                 Mini DAQ to test RD53 readout
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "../DQMUtils/DQMInterface.h"
#include "../System/SystemController.h"
#include "../Utils/MiddlewareInterface.h"
#include "../Utils/RD53Shared.h"
#include "../Utils/argvparser.h"

#include "../tools/RD53ClockDelay.h"
#include "../tools/RD53Gain.h"
#include "../tools/RD53GainOptimization.h"
#include "../tools/RD53InjectionDelay.h"
#include "../tools/RD53Latency.h"
#include "../tools/RD53Physics.h"
#include "../tools/RD53PixelAlive.h"
#include "../tools/RD53SCurve.h"
#include "../tools/RD53ThrAdjustment.h"
#include "../tools/RD53ThrEqualization.h"
#include "../tools/RD53ThrMinimization.h"

#ifdef __USE_ROOT__
#include "TApplication.h"
#endif

#ifdef __EUDAQ__
#include "../tools/RD53eudaqProducer.h"
#endif

#include <sys/wait.h>

// ##################
// # Default values #
// ##################
#define RUNNUMBER 0
#define SETBATCH 0 // Set batch mode when running supervisor
#define FILERUNNUMBER "./RunNumber.txt"

INITIALIZE_EASYLOGGINGPP

using namespace Ph2_System;
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

pid_t runControllerPid    = -1;
int   runControllerStatus = 0;

void interruptHandler(int handler)
{
    if((runControllerStatus != 0) && (runControllerPid > 0))
    {
        LOG(INFO) << BOLDBLUE << "Killing run controller pid: " << runControllerPid << " status: " << runControllerStatus << RESET;
        kill(runControllerPid, SIGKILL);
    }

    exit(EXIT_FAILURE);
}

void readBinaryData(std::string binaryFile, SystemController& mySysCntr, std::vector<RD53FWInterface::Event>& decodedEvents)
{
    unsigned int          errors = 0;
    std::vector<uint32_t> data;

    LOG(INFO) << BOLDMAGENTA << "@@@ Decoding binary data file @@@" << RESET;
    mySysCntr.addFileHandler(binaryFile, 'r');
    LOG(INFO) << BOLDBLUE << "\t--> Data are being readout from binary file" << RESET;
    mySysCntr.readFile(data, 0);

    RD53FWInterface::DecodeEventsMultiThreads(data, decodedEvents);
    LOG(INFO) << GREEN << "Total number of events in binary file: " << BOLDYELLOW << decodedEvents.size() << RESET;

    for(auto i = 0u; i < decodedEvents.size(); i++)
        if(RD53FWInterface::EvtErrorHandler(decodedEvents[i].evtStatus) == false)
        {
            LOG(ERROR) << BOLDBLUE << "\t--> Corrupted event n. " << BOLDYELLOW << i << RESET;
            errors++;
        }

    LOG(INFO) << GREEN << "Percentage of corrupted events: " << std::setprecision(3) << BOLDYELLOW << 1. * errors / decodedEvents.size() * 100. << "%" << std::setprecision(-1) << RESET;
}

int main(int argc, char** argv)
{
    // ########################
    // # Configure the logger #
    // ########################
    el::Configurations conf("../settings/logger.conf");
    conf.set(el::Level::Global, el::ConfigurationType::Format, "|%thread|%levshort| %msg");
    el::Loggers::reconfigureAllLoggers(conf);

    // #############################
    // # Initialize command parser #
    // #############################
    CommandLineProcessing::ArgvParser cmd;

    cmd.setIntroductoryDescription("@@@ CMSIT Middleware System Test Application @@@");

    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hardware description file. Default value: CMSIT.xml", CommandLineProcessing::ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("calib",
                     "Which calibration to run [latency pixelalive noise scurve gain threqu gainopt thrmin thradj "
                     "injdelay clockdelay physics eudaq]. Default: pixelalive",
                     CommandLineProcessing::ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("calib", "c");

    cmd.defineOption("binary", "Binary file to decode.", CommandLineProcessing::ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("binary", "b");

    cmd.defineOption("prog", "Program the system components.", CommandLineProcessing::ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("prog", "p");

    cmd.defineOption("sup", "Run in producer(Middleware) - consumer(DQM) mode.", CommandLineProcessing::ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("sup", "s");

    cmd.defineOption("eudaqRunCtr", "EUDA-IT run control address. Defaut: tcp://localhost:44000", CommandLineProcessing::ArgvParser::OptionRequiresValue);

    cmd.defineOption("reset", "Reset the backend board", CommandLineProcessing::ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("reset", "r");

    cmd.defineOption("capture", "Capture communication with board (extension .raw).", CommandLineProcessing::ArgvParser::OptionRequiresValue);

    cmd.defineOption("replay", "Replay previously captured communication (extension .raw).", CommandLineProcessing::ArgvParser::OptionRequiresValue);

    int result = cmd.parse(argc, argv);
    if(result != CommandLineProcessing::ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(EXIT_FAILURE);
    }

    // ###################
    // # Read run number #
    // ###################
    int           runNumber = RUNNUMBER;
    std::ifstream fileRunNumberIn;
    fileRunNumberIn.open(FILERUNNUMBER, std::ios::in);
    if(fileRunNumberIn.is_open() == true) fileRunNumberIn >> runNumber;
    fileRunNumberIn.close();
    system(std::string("mkdir " + std::string(RD53Shared::RESULTDIR)).c_str());

    // ####################
    // # Retrieve options #
    // ####################
    std::string configFile = cmd.foundOption("file") == true ? cmd.optionValue("file") : "CMSIT.xml";
    std::string whichCalib = cmd.foundOption("calib") == true ? cmd.optionValue("calib") : "pixelalive";
    std::string binaryFile = cmd.foundOption("binary") == true ? cmd.optionValue("binary") : "";
    bool        program    = cmd.foundOption("prog") == true ? true : false;
    bool        supervisor = cmd.foundOption("sup") == true ? true : false;
    bool        reset      = cmd.foundOption("reset") == true ? true : false;
    if(cmd.foundOption("capture") == true)
        RegManager::enableCapture(cmd.optionValue("capture").insert(0, std::string(RD53Shared::RESULTDIR) + "/Run" + RD53Shared::fromInt2Str(runNumber) + "_"));
    else if(cmd.foundOption("replay") == true)
        RegManager::enableReplay(cmd.optionValue("replay"));
    std::string eudaqRunCtr = cmd.foundOption("eudaqRunCtr") == true ? cmd.optionValue("eudaqRunCtr") : "tcp://localhost:44000";

    // ######################
    // # Supervisor section #
    // ######################
    if(supervisor == true)
    {
#ifdef __USE_ROOT__
        // #######################
        // # Run Supervisor Mode #
        // #######################
        int runControllerPidStatus = 0;

        runControllerPid = fork();
        if(runControllerPid == -1)
        {
            LOG(ERROR) << BOLDRED << "I can't fork RunController, error occured" << RESET;
            exit(EXIT_FAILURE);
        }
        else if(runControllerPid == 0)
        {
            char* argv[] = {(char*)"RunController", NULL};
            execv((std::string(getenv("BASE_DIR")) + "/bin/RunController").c_str(), argv);
            LOG(ERROR) << BOLDRED << "I can't run RunController, error occured" << RESET;
            exit(EXIT_FAILURE);
        }

        struct sigaction act;
        act.sa_handler = interruptHandler;
        sigaction(SIGINT, &act, NULL);

        // ##########################
        // # Instantiate Middleware #
        // ##########################
        MiddlewareInterface theMiddlewareInterface("127.0.0.1", 5000);
        theMiddlewareInterface.initialize();

        // ###################
        // # Instantiate DQM #
        // ###################
        gROOT->SetBatch(SETBATCH);
        TApplication theApp("App", NULL, NULL);
        DQMInterface theDQMInterface;

        // #######################
        // # Enter State Machine #
        // #######################
        enum
        {
            INITIAL,
            HALTED,
            CONFIGURED,
            RUNNING,
            STOPPED
        };
        int stateMachineStatus = HALTED;
        while(stateMachineStatus != STOPPED)
        {
            if((runControllerPidStatus == false) && ((runControllerPidStatus = waitpid(runControllerPid, &runControllerStatus, WNOHANG)) != false))
                LOG(INFO) << BOLDBLUE << "Run Controller status: " << BOLDYELLOW << runControllerStatus << RESET;
            else
            {
                LOG(INFO) << BOLDBLUE << "Run Controller status: " << BOLDYELLOW << runControllerStatus << RESET;

                switch(stateMachineStatus)
                {
                case HALTED:
                {
                    LOG(INFO) << BOLDBLUE << "Supervisor sending configure" << RESET;

                    LOG(INFO) << BOLDMAGENTA << "@@@ Initializing the Hardware @@@" << RESET;
                    theMiddlewareInterface.configure(cmd.optionValue("calib"), cmd.optionValue("file"));
                    theDQMInterface.configure(cmd.optionValue("calib"), cmd.optionValue("file"));
                    LOG(INFO) << BOLDMAGENTA << "@@@ Hardware initialization done @@@" << RESET;
                    std::cout << std::endl;

                    stateMachineStatus = CONFIGURED;
                    break;
                }
                case CONFIGURED:
                {
                    LOG(INFO) << BOLDBLUE << "Supervisor sending start" << RESET;

                    theDQMInterface.startProcessingData(RD53Shared::fromInt2Str(runNumber));
                    theMiddlewareInterface.start(RD53Shared::fromInt2Str(runNumber));

                    stateMachineStatus = RUNNING;
                    break;
                }
                case RUNNING:
                {
                    LOG(INFO) << BOLDBLUE << "Supervisor sending stop" << RESET;

                    usleep(2e6);
                    theMiddlewareInterface.stop();
                    usleep(2e6);
                    theDQMInterface.stopProcessingData();

                    stateMachineStatus = STOPPED;
                    break;
                }
                case STOPPED:
                {
                    LOG(INFO) << BOLDBLUE << "Supervisor everything stopped" << RESET;
                    break;
                }
                }
            }
        }

        LOG(INFO) << BOLDBLUE << "Out of supervisor state machine. Run Controller status: " << BOLDYELLOW << runControllerStatus << RESET;
        if(SETBATCH == false)
            theApp.Run();
        else
            theApp.Terminate(0);
#else
        LOG(WARNING) << BOLDBLUE << "ROOT flag was OFF during compilation" << RESET;
        exit(EXIT_FAILURE);
#endif
    }
    else
    {
        SystemController mySysCntr;

        if((reset == true) || (binaryFile != ""))
        {
            // ######################################
            // # Reset hardware or read binary file #
            // ######################################

            std::stringstream outp;
            mySysCntr.InitializeHw(configFile, outp, true, false);
            mySysCntr.InitializeSettings(configFile, outp);
            if(reset == true)
            {
                static_cast<RD53FWInterface*>(mySysCntr.fBeBoardFWMap[mySysCntr.fDetectorContainer->at(0)->getBeBoardId()])->ResetSequence();
                exit(EXIT_SUCCESS);
            }
            if(binaryFile != "") readBinaryData(binaryFile, mySysCntr, RD53FWInterface::decodedEvents);
        }
        else if(binaryFile == "")
        {
            // #######################
            // # Initialize Hardware #
            // #######################

            LOG(INFO) << BOLDMAGENTA << "@@@ Initializing the Hardware @@@" << RESET;
            mySysCntr.ConfigureHardware(configFile);
            LOG(INFO) << BOLDMAGENTA << "@@@ Hardware initialization done @@@" << RESET;
            if(program == true)
            {
                LOG(INFO) << BOLDMAGENTA << "@@@ End of CMSIT miniDAQ @@@" << RESET;
                exit(EXIT_SUCCESS);
            }
        }

        std::cout << std::endl;

        // ###################
        // # Run Calibration #
        // ###################
        if(whichCalib == "latency")
        {
            // ###################
            // # Run LatencyScan #
            // ###################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing Latency scan @@@" << RESET;

            std::string fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_Latency");
            Latency     la;
            la.Inherit(&mySysCntr);
            la.localConfigure(fileName, runNumber);
            la.run();
            la.analyze();
            la.draw(runNumber);
        }
        else if(whichCalib == "pixelalive")
        {
            // ##################
            // # Run PixelAlive #
            // ##################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing PixelAlive scan @@@" << RESET;

            std::string fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_PixelAlive");
            PixelAlive  pa;
            pa.Inherit(&mySysCntr);
            pa.localConfigure(fileName, runNumber);
            pa.run();
            pa.analyze();
            pa.draw(runNumber);
        }
        else if(whichCalib == "noise")
        {
            // #############
            // # Run Noise #
            // #############
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing Noise scan @@@" << RESET;

            std::string fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_NoiseScan");
            PixelAlive  pa;
            pa.Inherit(&mySysCntr);
            pa.localConfigure(fileName, runNumber);
            pa.run();
            pa.analyze();
            pa.draw(runNumber);
        }
        else if(whichCalib == "scurve")
        {
            // ##############
            // # Run SCurve #
            // ##############
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing SCurve scan @@@" << RESET;

            std::string fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_SCurve");
            SCurve      sc;
            sc.Inherit(&mySysCntr);
            sc.localConfigure(fileName, runNumber);
            sc.run();
            sc.analyze();
            sc.draw(runNumber);
        }
        else if(whichCalib == "gain")
        {
            // ############
            // # Run Gain #
            // ############
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing Gain scan @@@" << RESET;

            std::string fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_Gain");
            Gain        ga;
            ga.Inherit(&mySysCntr);
            ga.localConfigure(fileName, runNumber);
            ga.run();
            ga.analyze();
            ga.draw(runNumber);
        }
        else if(whichCalib == "gainopt")
        {
            // #########################
            // # Run Gain Optimization #
            // #########################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing Gain Optimization @@@" << RESET;

            std::string      fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_GainOptimization");
            GainOptimization go;
            go.Inherit(&mySysCntr);
            go.localConfigure(fileName, runNumber);
            go.run();
            go.analyze();
            go.draw(runNumber);
        }
        else if(whichCalib == "threqu")
        {
            // ##############################
            // # Run Threshold Equalization #
            // ##############################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing Threshold Equalization @@@" << RESET;

            std::string     fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_ThrEqualization");
            ThrEqualization te;
            te.Inherit(&mySysCntr);
            te.localConfigure(fileName, runNumber);
            te.run();
            te.analyze();
            te.draw(runNumber);
        }
        else if(whichCalib == "thrmin")
        {
            // ##############################
            // # Run Threshold Minimization #
            // ##############################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing Threshold Minimization @@@" << RESET;

            std::string     fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_ThrMinimization");
            ThrMinimization tm;
            tm.Inherit(&mySysCntr);
            tm.localConfigure(fileName, runNumber);
            tm.run();
            tm.analyze();
            tm.draw(runNumber);
        }
        else if(whichCalib == "thradj")
        {
            // ##############################
            // # Run Threshold Minimization #
            // ##############################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing Threshold Adjustment @@@" << RESET;

            std::string   fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_ThrAdjustment");
            ThrAdjustment ta;
            ta.Inherit(&mySysCntr);
            ta.localConfigure(fileName, runNumber);
            ta.run();
            ta.analyze();
            ta.draw(runNumber);
        }
        else if(whichCalib == "injdelay")
        {
            // #######################
            // # Run Injection Delay #
            // #######################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing Injection Delay scan @@@" << RESET;

            std::string    fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_InjectionDelay");
            InjectionDelay id;
            id.Inherit(&mySysCntr);
            id.localConfigure(fileName, runNumber);
            id.run();
            id.analyze();
            id.draw(runNumber);
        }
        else if(whichCalib == "clockdelay")
        {
            // ###################
            // # Run Clock Delay #
            // ###################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing Clock Delay scan @@@" << RESET;

            std::string fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_ClockDelay");
            ClockDelay  cd;
            cd.Inherit(&mySysCntr);
            cd.localConfigure(fileName, runNumber);
            cd.run();
            cd.analyze();
            cd.draw(runNumber);
        }
        else if(whichCalib == "physics")
        {
            // ###############
            // # Run Physics #
            // ###############
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing Phsyics data taking @@@" << RESET;

            std::string fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_Physics");
            Physics     ph;
            ph.Inherit(&mySysCntr);
            ph.localConfigure(fileName, -1);
            if(binaryFile == "")
            {
                ph.Start(runNumber);
                usleep(2e6);
                ph.Stop();
            }
            else
                ph.analyze(true);
            ph.draw();
        }
        else if(whichCalib == "eudaq")
        {
#ifdef __EUDAQ__
            // ######################
            // # Run EUDAQ producer #
            // ######################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing EUDAQ data taking @@@" << RESET;

#ifdef __USE_ROOT__
            gROOT->SetBatch(true);
#endif
            RD53eudaqProducer theEUDAQproducer(mySysCntr, configFile, "RD53eudaqProducer", eudaqRunCtr);
            try
            {
                LOG(INFO) << GREEN << "Connecting to EUDAQ run control" << RESET;
                theEUDAQproducer.Connect();
            }
            catch(...)
            {
                LOG(ERROR) << BOLDRED << "Can not connect to EUDAQ run control at " << eudaqRunCtr << RESET;
                exit(EXIT_FAILURE);
            }
            LOG(INFO) << BOLDBLUE << "\t--> Connected" << RESET;
            while(theEUDAQproducer.IsConnected() == true) std::this_thread::sleep_for(std::chrono::seconds(1));
            exit(EXIT_SUCCESS);
#else
            LOG(WARNING) << BOLDBLUE << "EUDAQ flag was OFF during compilation" << RESET;
            exit(EXIT_FAILURE);
#endif
        }
        else
        {
            LOG(ERROR) << BOLDRED << "Option not recognized: " << BOLDYELLOW << whichCalib << RESET;
            exit(EXIT_FAILURE);
        }

        // ###########################
        // # Copy configuration file #
        // ###########################
        std::string fName2Add(std::string(RD53Shared::RESULTDIR) + "/Run" + RD53Shared::fromInt2Str(runNumber) + "_");
        std::string output(RD53Shared::composeFileName(configFile, fName2Add));
        std::string command("cp " + configFile + " " + output);
        system(command.c_str());

        // #####################
        // # Update run number #
        // #####################
        std::ofstream fileRunNumberOut;
        runNumber++;
        fileRunNumberOut.open(FILERUNNUMBER, std::ios::out);
        if(fileRunNumberOut.is_open() == true) fileRunNumberOut << RD53Shared::fromInt2Str(runNumber) << std::endl;
        fileRunNumberOut.close();

        LOG(INFO) << BOLDMAGENTA << "@@@ End of CMSIT miniDAQ @@@" << RESET;
    }

    return EXIT_SUCCESS;
}
