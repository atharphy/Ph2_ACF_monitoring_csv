#include "CommonVisitors.h"
#include "Timer.h"
#include "UsbUtilities.h"
#include "Watchdog.h"
#include "argvparser.h"
#include <iostream>

#include "BiasSweep.h"
#include "PedeNoise.h"
#include "PedestalEqualization.h"
#include "StubSweep.h"

#include "TApplication.h"
#include "TROOT.h"

#ifdef __USBINST__
#include "AppLock.cc"
#include "ArdNanoController.h"
#include "HMP4040Client.h"
#include "Ke2110Controller.h"
#include <zmq.hpp>
using namespace Ph2_UsbInst;
#endif

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

INITIALIZE_EASYLOGGINGPP

// I hate global variables but hey...

std::atomic<bool> gRunDAQ;
std::string       gResultDirectory;

void notifyme()
{
    LOG(INFO) << BOLDBLUE << "Watchdog timer timed out - stopping the DAQ!" << RESET;
    gRunDAQ = false;
}
void exitme()
{
    LOG(ERROR) << BOLDRED << "Quitting application due to watchdog timeout - something must have got stuck!" << RESET;
    std::string cSystemeCommand = "touch ";
    cSystemeCommand += gResultDirectory;
    cSystemeCommand += "/crashed.txt";
    system(cSystemeCommand.c_str());
    exit(1);
}

int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  Integrated validation test performs the following actions:");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hw Description File . Default value: settings/Cbc3HWDescription.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("output", "Output Directory . Default value: Results/Irrad_Xrays", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("events", "minimum Number of Events . Default value: 300k for this application", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("events", "e");

    cmd.defineOption(
        "vcth", "Threshold in VCth units (hex (including 0x) or decimal) . Default values from HW description .XML file", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("vcth", "v");

    cmd.defineOption("dqm", "Print every i-th event.  ", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("dqm", "d");

    cmd.defineOption("hostname", "Hostname of the machine that runs the LV and Temperature monitoring servers. Default: localhost", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("hostname", "H");

    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    cmd.defineOption("standalone", "launch irradtest without spawning the monitoring servers from code", ArgvParser::NoOptionAttribute /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("standalone", "s");

    cmd.defineOption("full", "run full characterization, default: false", ArgvParser::NoOptionAttribute /*| ArgvParser::OptionRequired*/);

    cmd.defineOption("webmonitor", "start THttpServer on port - default: 8090", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("webmonitor", "w");

    cmd.defineOption("timeout", "timeout after which DAQ should stop", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile   = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Cbc3HWDescription.xml";
    gResultDirectory      = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/Irrad_Xrays/";
    std::string cHostname = (cmd.foundOption("hostname")) ? cmd.optionValue("hostname") : "localhost";

    // uint32_t pEventsperVcth = ( cmd.foundOption ( "events" ) ) ? convertAnyInt ( cmd.optionValue ( "events" ).c_str()
    // ) : 500000;
    uint32_t cHttpPort = (cmd.foundOption("webmonitor")) ? convertAnyInt(cmd.optionValue("webmonitor").c_str()) : 8090;
    uint32_t cTimout   = (cmd.foundOption("timeout")) ? convertAnyInt(cmd.optionValue("timeout").c_str()) : 1200;

    bool batchMode   = (cmd.foundOption("batch")) ? true : false;
    bool cStandalone = (cmd.foundOption("standalone")) ? true : false;
    bool cVcthset    = cmd.foundOption("vcth");
    bool cWebMon     = (cmd.foundOption("webmonitor")) ? true : false;
    bool cFull       = (cmd.foundOption("full")) ? true : false;

    Watchdog cDog;
    cDog.Start(5, &exitme);

    std::string cResultfile = "Cbc3RadiationCycle";
    gResultDirectory += "Cbc3RadiationCycle";

    if(cFull) gResultDirectory += "_FULL";

    gResultDirectory += currentDateTime();

    LOG(INFO) << "Creating directory: " << gResultDirectory;
    std::string cCommand = "mkdir -p " + gResultDirectory;
    system(cCommand.c_str());

    // now make sure that the log file ends up where I want it
    std::string cLogFileName = gResultDirectory + "/CbcDAQ.log";
    el::Loggers::reconfigureAllLoggers(el::ConfigurationType::Filename, cLogFileName);
    LOG(INFO) << BLUE << "Changing log file to: " << MAGENTA << cLogFileName << RESET;

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

        // prepare the LVClient and the KeController
#ifdef __USBINST__
    Ke2110Controller* cKeController = nullptr;
    HMP4040Client*    cLVClient     = nullptr;
#endif

    cDog.Reset(15);

    // Start server to communicate with HMP404 instrument via usbtmc and SCPI
    bool cPSStatus, cDMMStatus;

    // get current working directory
    char        buffer[256];
    std::string currentDir = getcwd(buffer, sizeof(buffer));

    std::string cPowerSupplyOutputFile = currentDir + "/" + gResultDirectory + "/Current_log.txt";
    std::string cDMMOutputFile         = currentDir + "/" + gResultDirectory + "/Temperature_log.txt";

    if(!cStandalone)
    {
#ifdef __USBINST__
        PortsInfo cPowerSupplyPortsInfo = std::make_pair(8081, 8080);
        PortsInfo cDMMPortsInfo         = std::make_pair(8083, 8082);
        cPSStatus                       = false;
        cDMMStatus                      = false;

        int cMonitoringInterval = 2;
        // Start the server to communicate with the LV power supply
        cPSStatus = InitializeMonitoring(cHostname, "HMP4040", cPowerSupplyPortsInfo, cMonitoringInterval);

        // Start server to communicate with Keithley DMM instrument via usbtmc and SCPI
        cDMMStatus = InitializeMonitoring(cHostname, "Ke2110", cDMMPortsInfo, cMonitoringInterval);
#endif
    }
    else
    {
        LOG(INFO) << BOLDBLUE << "Standalone option set, thus requiring servers to be running already!" << RESET;
        cPSStatus  = true;
        cDMMStatus = true;
    }

    if(cPSStatus && cDMMStatus) // Verify child process terminated without error.
    {
        cDog.Reset(15);
        // at the beginning of the test start the monitoring and initalize the connection to the Arduino relay
        // controller
#ifdef __USBINST__
        cLVClient = new HMP4040Client(cHostname, cPowerSupplyPortsInfo.first);
        LOG(INFO) << BOLDBLUE << "Starting monitoring of power supply currents on the HMP4040." << RESET;
        cLVClient->SetLogFileName(cPowerSupplyOutputFile);
        cLVClient->StartMonitoring();
        // make sure power supply is switched  on before doing anything else
        LOG(INFO) << BLUE << "Making sure that the power supply is ON at the start of the test!" << RESET;
        cLVClient->ToggleOutput(1);

        cKeController = new Ke2110Controller();
        cKeController->InitializeClient(cHostname, cDMMPortsInfo.first);
        LOG(INFO) << BOLDBLUE << "Starting monitoring of the ambient temperature on the Ke2110 DMM." << RESET;
        cKeController->SendLogFileName(cDMMOutputFile);
        cKeController->SendMonitoringStart();
#endif
        // here the startup is complete

        cDog.Reset(5);
        std::stringstream outp;

        Tool cTool;
        cTool.InitializeHw(cHWFile, outp);
        cTool.InitializeSettings(cHWFile, outp);
        LOG(INFO) << outp.str();
        outp.str("");
        cTool.CreateResultDirectory(gResultDirectory, false, false);
        cTool.InitResultFile(cResultfile);

        if(cWebMon) cTool.StartHttpServer(cHttpPort);

        cTool.ConfigureHw();

        cDog.Reset(15);

        if(cFull)
        {
            Timer t;
#ifdef __USE_ROOT__

// then sweep a bunch of biases
#ifdef __USBINST__
            BiasSweep cBiasSweep(cLVClient, cKeController);
#else
            BiasSweep cBiasSweep;
#endif

            cBiasSweep.Inherit(&cTool);
            cBiasSweep.Initialize();
            cDog.Reset(5);
            std::vector<std::string> cBiases{
                "VCth", "CAL_Vcasc", "VPLUS1", "VPLUS2", "VBGbias", "VBG_LDO", "Vpafb", "VDDA", "Nc50", "Ipa", "Ipre1", "Ipre2", "CAL_I", "Ibias", "Ipsf", "Ipaos", "Icomp", "Ihyst"};

            for(auto cBoard: *cBiasSweep.fDetectorContainer)
            {
                for(auto cOpticalGroup: *cBoard)
                    for(auto cFe: *cOpticalGroup)
                    {
                        for(auto cCbc: *cFe)
                        {
                            for(auto cBias: cBiases)
                            {
                                cDog.Reset(160);
                                t.start();
                                cBiasSweep.SweepBias(cBias, static_cast<ReadoutChip*>(cCbc));
                                t.stop();
                                t.show("Time for this bias");
                            }

                            cDog.Reset(25);
                            cBiasSweep.MeasureMinPower(static_cast<BeBoard*>(cBoard), static_cast<ReadoutChip*>(cCbc));
                        }
                    }
            }
#endif
        }

        ////first, run offset tuning
        cDog.Reset(50);
        PedestalEqualization cPedestalEqualization;
        cPedestalEqualization.Inherit(&cTool);
        // cPedestalEqualization.ConfigureHw();
        cPedestalEqualization.Initialise(false);
        cPedestalEqualization.FindVplus();
        cPedestalEqualization.FindOffsets();
        cPedestalEqualization.writeObjects();
        cPedestalEqualization.dumpConfigFiles();

        ////now run a noise scan
        cDog.Reset(120);
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit(&cTool);
        cPedeNoise.Initialise();
        // measure noise actually extracts the noise and pedestal values
        cPedeNoise.measureNoise();
        // measure a second set with the TP
        // maximal tp amplitude is 0, minimal is 255
        // so in orde to get 30 we need 225
        // this only records SCurves
        cPedeNoise.sweepSCurves();
        cPedeNoise.writeObjects();

        // get the pedestal determined in the step above so I can adjust the threshold for the stub sweep and the DAQ
        uint16_t cPedestal = 0;
        // sweep the stubs
        ThresholdVisitor cVisitor(cTool.fReadoutChipInterface, 0);

        if(cFull)
        {
            cDog.Reset(50);

            if(cVcthset)
            {
                // hard coded threshold so I see stuff with the stub sweep
                uint16_t cThreshold = cPedestal - 20;
                LOG(INFO) << YELLOW << "Changing threshold to " << cThreshold << " to see the test pulse for stub sweeps" << RESET;
                cVisitor.setThreshold(cThreshold);
                cTool.accept(cVisitor);
            }

#ifdef __USE_ROOT__
            StubSweep cStubSweep;
            cStubSweep.Inherit(&cTool);
            cStubSweep.Initialize();
            cStubSweep.SweepStubs(1);
#endif
        }

        // close and save root file here
        cTool.SaveResults();
        cTool.CloseResultFile();

        if(!cFull)
        {
            // here I stop the original watchdog and restart it with another callback that just updates  a condition
            // variable thus, once the 1200 seconds (20 minutes) time out, the callback will just change the variable,
            // the DAQ will stop and thats it?
            cDog.Stop();
            cDog.Start(cTimout, &notifyme);
            // now take some data and save the binary files
            std::string cBinaryDataFileName = gResultDirectory + "/DAQ_data.raw";
            cTool.addFileHandler(cBinaryDataFileName, 'w');
            cTool.initializeWriteFileHandler();
            cTool.ConfigureHw();
            BeBoard* pBoard  = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));
            uint32_t cN      = 1;
            uint32_t cNthAcq = 0;
            uint32_t count   = 0;

            if(cVcthset)
            {
                // hardcoded threshold so the data has ~50% occupancy
                uint16_t cThreshold = cPedestal - 2;
                LOG(INFO) << YELLOW << "Changing threshold to " << cThreshold << " to have some occupancy in the data" << RESET;
                cVisitor.setThreshold(cThreshold);
                cTool.accept(cVisitor);
            }

            int cAcqSize     = 4000;
            int cAcqMultiple = 50;
            // by default disable the file handler
            cTool.fBeBoardInterface->disableFileHandler(pBoard);

            // for (int iAcq = 0; iAcq < pEventsperVcth / cAcqSize; iAcq++)
            gRunDAQ = true;

            while(gRunDAQ.load())
            {
                if(cNthAcq % cAcqMultiple == 0)
                {
                    // if  the acquisition is a multiple of what I want, enable it
                    cTool.fBeBoardInterface->enableFileHandler(pBoard);
                    cTool.ReadNEvents(pBoard, cAcqSize);
                    cTool.fBeBoardInterface->disableFileHandler(pBoard);
                }
                // otherwise just read data
                else
                    cTool.ReadNEvents(pBoard, cAcqSize);

                const std::vector<Event*>& events = cTool.GetEvents(pBoard);

                for(auto& ev: events)
                {
                    count++;
                    cN++;

                    if(cmd.foundOption("dqm"))
                    {
                        if(count % atoi(cmd.optionValue("dqm").c_str()) == 0)
                        {
                            LOG(INFO) << ">>> Event #" << count;
                            outp.str("");
                            outp << *ev << std::endl;
                            LOG(INFO) << outp.str();
                        }
                    }

                    if(count % cAcqSize == 0) LOG(INFO) << ">>> Recorded Event #" << count;
                }

                cNthAcq++;
            }
        }

        cTool.Destroy();

        // finished everything we want to do, so turn everything off and be happy
#ifdef __USBINST__

        // LOG (INFO) << YELLOW << "Stopping the monitoring and exiting the server for the HMP4040!" << RESET;
        // maybe also want to switch off the power supply at the end of the test
        // cLVClient->ToggleOutput (0);
        // cLVClient->StopMonitoring();
        // LOG (INFO) << YELLOW << "Stopping the monitoring and exiting the server for the Ke2110!" << RESET;
        // cKeController->SendMonitoringStop();

        // cLVClient->Quit();
        // cKeController->SendQuit();
#endif
    }
    else
    {
        LOG(ERROR) << "Either PowerSupply or DMM server did not start up correctly!";
        cDog.Stop();
    }

    cDog.Stop();

    if(!batchMode) cApp.Run();

#ifdef __USBINST__

    if(cLVClient) delete cLVClient;

    if(cKeController) delete cKeController;

#endif

    LOG(INFO) << GREEN << "Irradiation Test exited normally!" << RESET;

    return 0;
}
