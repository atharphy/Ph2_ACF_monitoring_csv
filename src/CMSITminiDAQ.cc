/*!
  \file                  CMSITminiDAQ.cc
  \brief                 Mini DAQ to test RD53 readout chip
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
#define BASEDIR "PH2ACF_BASE_DIR"
#define ARBITRARYDELAY 2e6 // [us]

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

void readBinaryData(const std::string& binaryFile, SystemController& mySysCntr, std::vector<RD53FWInterface::Event>& decodedEvents)
{
    const unsigned int    wordDataSize = 32;
    unsigned int          errors       = 0;
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
            RD53FWInterface::PrintEvents({decodedEvents[i]});
        }

    LOG(INFO) << GREEN << "Corrupted events: " << BOLDYELLOW << std::setprecision(3) << errors << " (" << 1. * errors / decodedEvents.size() * 100. << "%)" << std::setprecision(-1) << RESET;
    int avgEventSize = data.size() / decodedEvents.size();
    LOG(INFO) << GREEN << "Average event size is " << BOLDYELLOW << avgEventSize * wordDataSize << RESET << GREEN << " bits over " << BOLDYELLOW << decodedEvents.size() << RESET << GREEN << " events"
              << RESET;
    mySysCntr.closeFileHandler();
}

int main(int argc, char** argv)
{
    // #############################
    // # Initialize command parser #
    // #############################
    CommandLineProcessing::ArgvParser cmd;

    cmd.setIntroductoryDescription("@@@ CMSIT Middleware System Test Application @@@");

    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hardware description file", CommandLineProcessing::ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("calib",
                     "Which calibration to run [latency pixelalive noise scurve gain threqu gainopt thrmin thradj "
                     "injdelay clockdelay physics eudaq prbstime prbsframes]",
                     CommandLineProcessing::ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("calib", "c");

    cmd.defineOption("binary", "Binary file to decode", CommandLineProcessing::ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("binary", "b");

    cmd.defineOption("prog", "Just program the system components", CommandLineProcessing::ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("prog", "p");

    cmd.defineOption("sup", "Run in producer(Middleware) - consumer(DQM) mode", CommandLineProcessing::ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("sup", "s");

    cmd.defineOption("eudaqRunCtr", "EUDAQ-IT run control address [e.g. tcp://localhost:44000]", CommandLineProcessing::ArgvParser::OptionRequiresValue);

    cmd.defineOption("reset", "Reset the backend board", CommandLineProcessing::ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("reset", "r");

    cmd.defineOption("capture", "Capture communication with board (extension .raw)", CommandLineProcessing::ArgvParser::OptionRequiresValue);

    cmd.defineOption("replay", "Replay previously captured communication (extension .raw)", CommandLineProcessing::ArgvParser::OptionRequiresValue);

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
    system(std::string("mkdir -p " + std::string(RD53Shared::RESULTDIR)).c_str());

    // ####################
    // # Retrieve options #
    // ####################
    std::string configFile = cmd.foundOption("file") == true ? cmd.optionValue("file") : "";
    std::string whichCalib = cmd.foundOption("calib") == true ? cmd.optionValue("calib") : "";
    std::string binaryFile = cmd.foundOption("binary") == true ? cmd.optionValue("binary") : "";
    bool        program    = cmd.foundOption("prog") == true ? true : false;
    bool        supervisor = cmd.foundOption("sup") == true ? true : false;
    bool        reset      = cmd.foundOption("reset") == true ? true : false;
    if(cmd.foundOption("capture") == true)
        RegManager::enableCapture(cmd.optionValue("capture").insert(0, std::string(RD53Shared::RESULTDIR) + "/Run" + RD53Shared::fromInt2Str(runNumber) + "_"));
    else if(cmd.foundOption("replay") == true)
        RegManager::enableReplay(cmd.optionValue("replay"));
    std::string eudaqRunCtr = cmd.foundOption("eudaqRunCtr") == true ? cmd.optionValue("eudaqRunCtr") : "tcp://localhost:44000";

    // ########################
    // # Configure the logger #
    // ########################
    std::string fileName("logs/CMSITminiDAQ" + RD53Shared::fromInt2Str(runNumber));
    if(whichCalib != "") fileName += "_" + whichCalib;
    fileName += ".log";
    el::Configurations conf(std::string(std::getenv(BASEDIR)) + "/settings/logger.conf");
    conf.set(el::Level::Global, el::ConfigurationType::Format, "|%thread|%levshort| %msg");
    conf.set(el::Level::Global, el::ConfigurationType::Filename, fileName);
    el::Loggers::reconfigureAllLoggers(conf);
    // el::Loggers::reconfigureAllLoggers(el::ConfigurationType::Filename, fileName);

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
            execv((std::string(std::getenv(BASEDIR)) + "/bin/RunController").c_str(), argv);
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

                    usleep(ARBITRARYDELAY);
                    theMiddlewareInterface.stop();
                    usleep(ARBITRARYDELAY);
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
                static_cast<RD53FWInterface*>(mySysCntr.fBeBoardFWMap[mySysCntr.fDetectorContainer->at(0)->getId()])->ResetSequence();
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
            mySysCntr.Configure(configFile);
            LOG(INFO) << BOLDMAGENTA << "@@@ Hardware initialization done @@@" << RESET;
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
            la.draw();
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

            // #############################################
            // # Address different subsets of the detector #
            // #############################################
            // @TMP@
            // const int detDivision = 8;
            // auto      query       = [](int indx, int division, int thr) { return (division / thr < 1 ? indx < 1 : indx < 2); };
            // for(auto i = 0; query(i, detDivision, 8); i++)
            // {
            //     auto detectorSubset = [i](const OpticalGroupContainer* theOpticalGroup) { return (theOpticalGroup->getId() % 2 == i); };
            //     if(query(1, detDivision, 8) == true) pa.fDetectorContainer->setOpticalGroupQueryFunction(detectorSubset);

            //     for(auto j = 0; query(j, detDivision, 4); j++)
            //     {
            //         auto detectorSubset = [j](const ModuleContainer* theModule) { return (theModule->getId() % 2 == j); };
            //         if(query(1, detDivision, 4) == true) pa.fDetectorContainer->setHybridQueryFunction(detectorSubset);

            //         for(auto k = 0; query(k, detDivision, 2); k++)
            //         {
            //             auto detectorSubset = [k](const ChipContainer* theChip) { return (theChip->getId() % 2 == k); };
            //             if(query(1, detDivision, 2) == true) pa.fDetectorContainer->setReadoutChipQueryFunction(detectorSubset);

            pa.run();
            pa.analyze();
            pa.draw();

            //             pa.fDetectorContainer->resetReadoutChipQueryFunction();
            //         }
            //         pa.fDetectorContainer->resetHybridQueryFunction();
            //     }
            //     pa.fDetectorContainer->resetOpticalGroupQueryFunction();
            // }
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
            pa.draw();
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
            sc.draw();
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
            ga.draw();
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
            go.draw();
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
            te.draw();
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
            tm.draw();
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
            ta.draw();
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
            id.draw();
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
            cd.draw();
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
            if(binaryFile == "")
            {
                ph.localConfigure(fileName, -1);
                ph.Start(runNumber);
                usleep(ARBITRARYDELAY);
                ph.Stop();
            }
            else
            {
                ph.localConfigure(fileName, runNumber);
                ph.analyze(true);
                ph.draw();
            }
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
        else if((whichCalib == "prbstime") || (whichCalib == "prbsframes"))
        {
            // #################
            // # Run PRBS test #
            // #################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing Pseudo Random Bit Sequence test @@@" << RESET;

            if(cmd.argument(0) == "")
            {
                if(whichCalib == "prbstime") { LOG(ERROR) << BOLDRED << "Failed to specify duration of PRBS test; use \"-c prbstime <TIME IN SECONDS>\"" << RESET; }
                else if(whichCalib == "prbsframes")
                {
                    LOG(ERROR) << BOLDRED << "Failed to specify number of frames for PRBS test; use \"-c prbsframes <NUMBER OF FRAMES>\"" << RESET;
                }

                exit(EXIT_FAILURE);
            }

            unsigned long long frames_or_time = strtoull(cmd.argument(0).c_str(), NULL, 0);
            bool               given_time     = false;
            if(whichCalib == "prbstime") given_time = true;

            for(const auto cBoard: *mySysCntr.fDetectorContainer)
                for(const auto cOpticalGroup: *cBoard)
                    for(const auto cHybrid: *cOpticalGroup)
                        for(const auto cChip: *cHybrid)
                        {
                            mySysCntr.fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "SER_SEL_OUT", 2, true);
                            LOG(INFO) << GREEN << "PRBS test for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                                      << +cChip->getId() << RESET << GREEN << "]: " << BOLDYELLOW
                                      << ((static_cast<RD53FWInterface*>(mySysCntr.fBeBoardFWMap[cBoard->getId()])->RunPRBStest(given_time, frames_or_time, cHybrid->getId(), cChip->getId()) == true)
                                              ? "PASSED"
                                              : "NOT PASSED")
                                      << RESET;
                            mySysCntr.fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "SER_SEL_OUT", 1, true);
                        }
        }
        else if((program == false) && (whichCalib != ""))
        {
            LOG(ERROR) << BOLDRED << "Option not recognized: " << BOLDYELLOW << whichCalib << RESET;
            exit(EXIT_FAILURE);
        }

        // ###########################
        // # Copy configuration file #
        // ###########################
        const auto configFileBasename = configFile.substr(configFile.find_last_of("/\\") + 1);
        const auto outputConfigFile   = std::string(RD53Shared::RESULTDIR) + "/Run" + RD53Shared::fromInt2Str(runNumber) + "_" + configFileBasename;
        system(("cp " + configFile + " " + outputConfigFile).c_str());

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
