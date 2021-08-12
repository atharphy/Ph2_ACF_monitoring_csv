#include <cstring>

//#include "../Utils/easylogging++.h"
#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../tools/AntennaTester.h"
#include "../tools/CBCPulseShape.h"
#include "../tools/LatencyScan.h"
#include "../tools/PedeNoise.h"
#include "../tools/SignalScan.h"
#include "../tools/SignalScanFit.h"
#include "tools/BackEndAlignment.h"
#include "tools/CicFEAlignment.h"

#include "../Utils/argvparser.h"
#include "TApplication.h"
#include "TROOT.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  Commissioning tool to perform the following procedures:\n-Timing / "
                                   "Latency scan\n-Threshold Scan\n-Stub Latency Scan");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hw Description File . Default value: settings/Commission_2CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("latency", "scan the trigger latency", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("latency", "l");

    cmd.defineOption("triggerTdc", "measure trigger time of arrival", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("triggerTdc", "t");

    cmd.defineOption("antenna", "perform latency scan with antenna on UIB", ArgvParser::OptionRequiresValue);

    cmd.defineOption("stublatency", "scan the stub latency", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("stublatency", "s");

    cmd.defineOption("noise", "scan the CBC noise per strip", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("noise", "n");

    cmd.defineOption("signal", "Scan the threshold using physics triggers", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("signal", "S");

    cmd.defineOption("signalFit", "Scan the threshold and fit for signal Vcth", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("signalFit", "F");

    cmd.defineOption("output", "Output Directory . Default value: Results/", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    cmd.defineOption("allChan", "Do pedestal and noise measurement using all channels? Default: false", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("allChan", "a");

    cmd.defineOption("pulseShape", "Scan the threshold and fit for signal Vcth", ArgvParser::NoOptionAttribute);

    cmd.defineOption("withCIC", "With CIC. Default : false", ArgvParser::NoOptionAttribute);

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile      = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Commissioning.xml";
    bool        cLatency     = (cmd.foundOption("latency")) ? true : false;
    bool        cTriggerTDC  = (cmd.foundOption("triggerTdc")) ? true : false;
    bool        cStubLatency = (cmd.foundOption("stublatency")) ? true : false;
    bool        cSignal      = (cmd.foundOption("signal")) ? true : false;
    bool        cSignalFit   = (cmd.foundOption("signalFit")) ? true : false;
    // bool cHitOR = ( cmd.foundOption ( "hitOR" ) ) ? true : false;
    bool cNoise      = (cmd.foundOption("noise")) ? true : false;
    bool cAntenna    = (cmd.foundOption("antenna")) ? true : false;
    bool cPulseShape = (cmd.foundOption("pulseShape")) ? true : false;

    bool        cWithCIC   = (cmd.foundOption("withCIC"));
    std::string cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";

    if(cNoise)
        cDirectory += "NoiseScan";
    else if(cSignalFit)
        cDirectory += "SignalFit";
    else
        cDirectory += "Commissioning";

    bool batchMode = (cmd.foundOption("batch")) ? true : false;
    bool cAllChan  = (cmd.foundOption("allChan")) ? true : false;

    int     cSignalRange      = (cmd.foundOption("signal")) ? convertAnyInt(cmd.optionValue("signal").c_str()) : 30;
    int     cSignalFitRange   = (cmd.foundOption("signalFit")) ? convertAnyInt(cmd.optionValue("signalFit").c_str()) : 10;
    uint8_t cAntennaPotential = (cmd.foundOption("antenna")) ? convertAnyInt(cmd.optionValue("antenna").c_str()) : 0;

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::string cResultfile;

    if(cLatency || cStubLatency || cTriggerTDC)
        cResultfile = "Latency";
    else if(cSignal)
        cResultfile = "SignalScan";
    else if(cSignalFit)
        cResultfile = "SignalScanFit";
    else
        cResultfile = "Commissioning";

    std::stringstream outp;
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    cTool.CreateResultDirectory(cDirectory);
    cTool.InitResultFile(cResultfile);
    cTool.StartHttpServer();
    cTool.ConfigureHw();

    // align back-end .. if this moves to firmware then we can get rid of this step
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Initialise();
    bool cAligned = cBackEndAligner.Align();
    cBackEndAligner.resetPointers();
    if(!cAligned)
    {
        LOG(ERROR) << BOLDRED << "Failed to align back-end" << RESET;
        exit(0);
    }

    // if CIC is enabled then align CIC first
    if(cWithCIC)
    {
        CicFEAlignment cCicAligner;
        cCicAligner.Inherit(&cTool);
        cCicAligner.Start(0);
        cCicAligner.waitForRunToBeCompleted();
        // reset all chip and board registers
        // to what they were before this tool was called
        cCicAligner.Reset();
        cCicAligner.dumpConfigFiles();
    }

#ifdef __ANTENNA__
    AntennaTester cAntennaTester;
    cAntennaTester.Inherit(&cTool);
    cAntennaTester.Initialize();
#endif

    if(cLatency || cStubLatency)
    {
        LatencyScan cLatencyScan;
        cLatencyScan.Inherit(&cTool);
        cLatencyScan.Initialize();

        // Here comes our Part:
        if(cAntenna) LOG(INFO) << BOLDBLUE << "Enabling antenna with " << +cAntennaPotential << " written to the potentiometer" << RESET;

        if(cLatency)
        {
#ifdef __ANTENNA__
            if(cAntenna) cAntennaTester.EnableAntenna(cAntenna, cAntennaPotential);
#endif

            cLatencyScan.ScanLatency();
        }

        if(cStubLatency) cLatencyScan.StubLatencyScan();

// if antenna was being used ... then disable it again at the end
#ifdef __ANTENNA__

        if(cAntenna)
        {
            LOG(INFO) << BOLDBLUE << "Disable antenna with " << +cAntennaPotential << " written to the potentiometer" << RESET;
            cAntennaTester.EnableAntenna(false, cAntennaPotential);
        }
#endif

        cLatencyScan.writeObjects();
    }

    else if(cTriggerTDC)
    {
        LatencyScan cLatencyScan;
        cLatencyScan.Inherit(&cTool);
        cLatencyScan.Initialize();
        cLatencyScan.MeasureTriggerTDC();
        cLatencyScan.writeObjects();
    }

    else if(cSignal)
    {
#ifdef __USE_ROOT__
        SignalScan cSignalScan;
        cSignalScan.Inherit(&cTool);
        cSignalScan.Initialize();
        cSignalScan.ScanSignal(600, 600 - cSignalRange);
        cSignalScan.writeObjects();
#endif
    }

    else if(cSignalFit)
    {
#ifdef __USE_ROOT__
        SignalScanFit cSignalScanFit;
        cSignalScanFit.Inherit(&cTool);
        cSignalScanFit.Initialize();
        cSignalScanFit.ScanSignal(cSignalFitRange); // Particle means that we trigger on a particle
#endif
    }

    else if(cNoise)
    {
        Timer     t;
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit(&cTool);

#ifdef __ANTENNA__

        if(cAntenna) LOG(INFO) << BOLDBLUE << "Enabling antenna with " << +cAntennaPotential << " written to the potentiometer" << RESET;
        if(cAntenna) cAntennaTester.EnableAntenna(cAntenna, cAntennaPotential);
#endif

        cPedeNoise.Initialise(cAllChan); // canvases etc. for fast calibration
        t.start();
        cPedeNoise.measureNoise();
        t.stop();
        t.show("Time for noise measurement");
        // cPedeNoise.Validate();

#ifdef __ANTENNA__

        if(cAntenna)
        {
            LOG(INFO) << BOLDBLUE << "Disable antenna with " << +cAntennaPotential << " written to the potentiometer" << RESET;
            cAntennaTester.EnableAntenna(false, cAntennaPotential);
        }
#endif

        cPedeNoise.writeObjects();
        cPedeNoise.dumpConfigFiles();
    }

    else if(cPulseShape)
    {
        Timer t;
        t.start();
        CBCPulseShape cCBCPulseShape;
        cCBCPulseShape.Inherit(&cTool);
        cCBCPulseShape.Initialise();
        cCBCPulseShape.runCBCPulseShape();
        cCBCPulseShape.writeObjects();
        t.stop();
        t.show("Time for pulseShape plot measurement");
        t.reset();
    }

    cTool.SaveResults();
    cTool.WriteRootFile();
    cTool.CloseResultFile();
    cTool.Destroy();

    if(!batchMode) cApp.Run();

    return 0;
}
