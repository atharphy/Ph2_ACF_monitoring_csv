/*!
  \file                  ITchipTesting.cc
  \brief                 Specific RD53 chip characterization tests
  \author                Dominik Koukola
  \version               1.0
  \date                  23/02/21
  Support:               email to dominik.koukola@cern.ch
*/

#include "../DQMUtils/DQMInterface.h"
#include "../System/SystemController.h"
#include "../Utils/MiddlewareInterface.h"
#include "../Utils/RD53Shared.h"
#include "../Utils/argvparser.h"

#include "../tools/RD53PixelAlive.h"

#include "../ProductionTools/RD53ADCHistogram.h"
#include "../ProductionTools/RD53RingOscillator.h"

// #ifdef __POWERSUPPLY__
#include "../ProductionTools/RD53ADCPowerSupply.h"
#include "../ProductionTools/RD53ADCScan.h"
#include "../ProductionTools/RD53DACScan.h"
#include "../ProductionTools/RD53TempSensor.h"
// #endif

#include <chrono>
#include <thread>

#ifdef __USE_ROOT__
#include "TApplication.h"
#endif

#include <sys/wait.h>

// ##################
// # Default values #
// ##################
#define RUNNUMBER 0
#define FILERUNNUMBER "./RunNumber.txt"
#define BASEDIR "PH2ACF_BASE_DIR"
#define ARBITRARYDELAY 2 // [seconds]

INITIALIZE_EASYLOGGINGPP

using namespace Ph2_System;
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_ITchipTesting;

int main(int argc, char** argv)
{
    // #############################
    // # Initialize command parser #
    // #############################
    CommandLineProcessing::ArgvParser cmd;

    cmd.setIntroductoryDescription("@@@ IT Middleware Chip Test Application @@@");

    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hardware description file", CommandLineProcessing::ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("calib", "Which calibration to run [pixelalive adchist adcps adcscan dacscan ringosc tempsens]", CommandLineProcessing::ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("calib", "c");

    // cmd.defineOption("binary", "Binary file to decode", CommandLineProcessing::ArgvParser::OptionRequiresValue);
    // cmd.defineOptionAlternative("binary", "b");

    cmd.defineOption("prog", "Just program the system components", CommandLineProcessing::ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("prog", "p");

    // cmd.defineOption("sup", "Run in producer(Middleware) - consumer(DQM) mode", CommandLineProcessing::ArgvParser::NoOptionAttribute);
    // cmd.defineOptionAlternative("sup", "s");

    // cmd.defineOption("eudaqRunCtr", "EUDAQ-IT run control address [e.g. tcp://localhost:44000]", CommandLineProcessing::ArgvParser::OptionRequiresValue);

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
    // std::string binaryFile = cmd.foundOption("binary") == true ? cmd.optionValue("binary") : "";
    bool program = cmd.foundOption("prog") == true ? true : false;
    // bool        supervisor = cmd.foundOption("sup") == true ? true : false;
    bool reset = cmd.foundOption("reset") == true ? true : false;
    if(cmd.foundOption("capture") == true)
        RegManager::enableCapture(cmd.optionValue("capture").insert(0, std::string(RD53Shared::RESULTDIR) + "/Run" + RD53Shared::fromInt2Str(runNumber) + "_"));
    else if(cmd.foundOption("replay") == true)
        RegManager::enableReplay(cmd.optionValue("replay"));

    // ########################
    // # Configure the logger #
    // ########################
    std::string fileName("logs/ITchipTesting" + RD53Shared::fromInt2Str(runNumber));
    if(whichCalib != "") fileName += "_" + whichCalib;
    fileName += ".log";
    el::Configurations conf(std::string(std::getenv(BASEDIR)) + "/settings/logger.conf");
    conf.set(el::Level::Global, el::ConfigurationType::Format, "|%thread|%levshort| %msg");
    conf.set(el::Level::Global, el::ConfigurationType::Filename, fileName);
    el::Loggers::reconfigureAllLoggers(conf);
    // el::Loggers::reconfigureAllLoggers(el::ConfigurationType::Filename, fileName);

    // #################################
    // # Configure power supply maybe? #
    // #################################

    {
        SystemController mySysCntr;

        if((reset == true))
        {
            // ###################
            // # Reset hardware  #
            // ###################

            std::stringstream outp;
            mySysCntr.InitializeHw(configFile, outp, true, false);
            mySysCntr.InitializeSettings(configFile, outp);
            // if(reset == true)
            {
                if(mySysCntr.fDetectorContainer->at(0)->at(0)->flpGBT == nullptr)
                    static_cast<RD53FWInterface*>(mySysCntr.fBeBoardFWMap[mySysCntr.fDetectorContainer->at(0)->getId()])->ResetSequence("160");
                else
                    static_cast<RD53FWInterface*>(mySysCntr.fBeBoardFWMap[mySysCntr.fDetectorContainer->at(0)->getId()])->ResetSequence("320");
                exit(EXIT_SUCCESS);
            }

            LOG(WARNING) << BOLDBLUE << "Reset not implemented" << RESET;
            exit(EXIT_FAILURE);
        }
        else
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
        if(whichCalib == "pixelalive")
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
            pa.draw();
        }
        else if(whichCalib == "adchist")
        {
            // ##################
            // # Run ADCHistogram #
            // ##################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing ADCHistogram @@@" << RESET;

            std::string  fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_ADCHistogram");
            ADCHistogram adch;
            adch.Inherit(&mySysCntr);
            adch.run();
            adch.draw();
        }
        else if(whichCalib == "ringosc")
        {
            // ##################
            // # Run RingOscillator #
            // ##################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing RingOscillator @@@" << RESET;

            std::string    fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_RingOscillator");
            RingOscillator ros;
            ros.Inherit(&mySysCntr);
            ros.run();
            ros.draw();
        }
        else if(whichCalib == "adcps")
        {
            // #ifdef __POWERSUPPLY__
            // #######################
            // # ADC Power supply tests  #
            // #######################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing ADC Power Supply Test @@@" << RESET;

            // std::string     fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_ADCPowerSupply");
            ADCPowerSupply aps;
            aps.Inherit(&mySysCntr);
            // ps.localConfigure(fileName, runNumber);
            aps.run(configFile);
            // ps.analyze();
            aps.draw();
            // #else
            //             LOG(WARNING) << BOLDBLUE << "POWERSUPPLY flag was OFF during compilation" << RESET;
            //             exit(EXIT_FAILURE);
            // #endif
        }
        else if(whichCalib == "adcscan")
        {
            // #ifdef __POWERSUPPLY__
            // ##################
            // # Run ADCScan #
            // ##################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing ADCScan @@@" << RESET;

            std::string fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_ADCScan");
            ADCScan     adc;
            adc.Inherit(&mySysCntr);
            adc.run(configFile);
            adc.draw();
            // #else
            //             LOG(WARNING) << BOLDBLUE << "POWERSUPPLY flag was OFF during compilation" << RESET;
            //             exit(EXIT_FAILURE);
            // #endif
        }
        else if(whichCalib == "dacscan")
        {
            // #ifdef __POWERSUPPLY__
            // ##################
            // # Run DACScan #
            // ##################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing DACScan @@@" << RESET;

            std::string fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_DACScan");
            DACScan     dac;
            dac.Inherit(&mySysCntr);
            dac.run(configFile);
            dac.draw();
            // #else
            //             LOG(WARNING) << BOLDBLUE << "POWERSUPPLY flag was OFF during compilation" << RESET;
            //             exit(EXIT_FAILURE);
            // #endif
        }
        else if(whichCalib == "tempsens")
        {
            // #ifdef __POWERSUPPLY__
            // ##################
            // # Run TempSensor #
            // ##################
            LOG(INFO) << BOLDMAGENTA << "@@@ Performing TempSensor @@@" << RESET;

            std::string fileName("Run" + RD53Shared::fromInt2Str(runNumber) + "_TempSensor");
            TempSensor  tsn;
            tsn.Inherit(&mySysCntr);
            tsn.run(configFile);
            tsn.draw();
            // #else
            //             LOG(WARNING) << BOLDBLUE << "POWERSUPPLY flag was OFF during compilation" << RESET;
            //             exit(EXIT_FAILURE);
            // #endif
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

        // #############################
        // # Destroy System Controller #
        // #############################
        mySysCntr.Destroy();
        // fDetectorMonitor->startMonitoring();

        LOG(INFO) << BOLDMAGENTA << "@@@ End of IT Chip Testing @@@" << RESET;
    }

    return EXIT_SUCCESS;
}
