#include <cstring>

#include "ExtraChecks.h"
#include "Utils/Timer.h"
#include "Utils/Utilities.h"
#include "Utils/argvparser.h"
#include "tools/BackEndAlignment.h"
#include "tools/CicFEAlignment.h"
#include "tools/DataChecker.h"
#include "tools/LatencyScan.h"
#include "tools/OpenFinder.h"
#include "tools/PedeNoise.h"
#include "tools/PedestalEqualization.h"
#include "tools/ShortFinder.h"

#ifdef __USE_ROOT__
#include "TApplication.h"
#include "TROOT.h"
#endif

#define __NAMEDPIPE__

#ifdef __NAMEDPIPE__
#include "gui_logger.h"
#endif

#ifdef __ANTENNA__
#include "Antenna.h"
#endif

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

#define CHIPSLAVE 4

int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf("settings/logger.conf");
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

    cmd.defineOption("tuneOffsets", "tune offsets on readout chips connected to CIC.");
    cmd.defineOptionAlternative("tuneOffsets", "t");

    cmd.defineOption("measurePedeNoise", "measure pedestal and noise on readout chips connected to CIC.");
    cmd.defineOptionAlternative("measurePedeNoise", "m");

    cmd.defineOption("findOpens", "perform latency scan with antenna on UIB", ArgvParser::NoOptionAttribute);
    cmd.defineOption("findShorts", "look for shorts", ArgvParser::NoOptionAttribute);

    cmd.defineOption("save", "Save the data to a raw file.  ", ArgvParser::OptionRequiresValue);

    // general
    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    cmd.defineOption("allChan", "Do pedestal and noise measurement using all channels? Default: false", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("allChan", "a");

    cmd.defineOption("hybridId", "Serial Number of mezzanine . Default value: xxxx", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOption("threshold", "Threshold value to set on chips for open and short finding", ArgvParser::OptionRequiresValue);

    cmd.defineOption("checkData", "Compare injected hits and stubs with output [please provide a comma seperated list of chips to check]", ArgvParser::OptionRequiresValue);

    cmd.defineOption("antennaDelay", "Delay between the antenna pulse and the delay [25 ns]", ArgvParser::OptionRequiresValue);
    cmd.defineOption("latencyRange", "Range of latencies around pulse to scan [25 ns]", ArgvParser::OptionRequiresValue);
    cmd.defineOption("evaluate", "Run some more detailed tests... ", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("evaluate", "e");

    cmd.defineOption("withCIC", "With CIC. Default : false", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkClusters", "Check CIC2 sparsification... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkSLink", "Check S-link ... data saved to file ", ArgvParser::OptionRequiresValue);
    cmd.defineOption("checkStubs", "Check Stubs... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkReadData", "Check ReadData method... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkAsync", "Check Async readout methods [PS objects only]... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkReadNEvents", "Check ReadNEvents method... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("noiseInjection", "Check noise injection...", ArgvParser::NoOptionAttribute);
    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile           = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Commissioning.xml";
    bool        cWithCIC          = (cmd.foundOption("withCIC"));
    bool        cTune             = (cmd.foundOption("tuneOffsets"));
    bool        cMeasurePedeNoise = (cmd.foundOption("measurePedeNoise"));
    bool        cFindOpens        = (cmd.foundOption("findOpens")) ? true : false;
    bool        cShortFinder      = (cmd.foundOption("findShorts")) ? true : false;
    bool        batchMode         = (cmd.foundOption("batch")) ? true : false;
    bool        cAllChan          = (cmd.foundOption("allChan")) ? true : false;
    bool        cCheckData        = (cmd.foundOption("checkData"));
    bool        cEvaluate         = (cmd.foundOption("evaluate"));

    bool cSaveToFile = cmd.foundOption("save");

    uint32_t    cThreshold = (cmd.foundOption("threshold")) ? convertAnyInt(cmd.optionValue("threshold").c_str()) : 560;
    std::string cHybridId  = (cmd.foundOption("hybridId")) ? cmd.optionValue("hybridId") : "xxxx";
    std::string cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    cDirectory += Form("FEH_2S_%s", cHybridId.c_str());

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::string cResultfile = "Hybrid";
    Timer       t;
    Timer       cGlobalTimer;
    cGlobalTimer.start();

// measure hybrid current and temperature
#ifdef __ANTENNA__
    char    cBuffer[120];
    Antenna cAntenna;
    // cAntenna.setId("UIBV2-CMSPH2-BRD00050");
    cAntenna.ConfigureSlaveADC(CHIPSLAVE);
    float cTemp    = cAntenna.GetHybridTemperature(CHIPSLAVE);
    float cCurrent = cAntenna.GetHybridCurrent(CHIPSLAVE);
    sprintf(cBuffer, "Hybrid %s [pre-configuration with default setttings]: temperature reading %.2f °C, current reading %.2f mA", cHybridId.c_str(), cTemp, cCurrent);
    LOG(INFO) << BOLDBLUE << cBuffer << RESET;
#endif

    std::stringstream outp;
    Tool              cTool;
    if(cSaveToFile)
    {
        std::string cRawFile = cmd.optionValue("save");
        cTool.addFileHandler(cRawFile, 'w');
        LOG(INFO) << BOLDBLUE << "Writing Binary Rawdata to:   " << cRawFile;
    }
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    cTool.CreateResultDirectory(cDirectory);
    cTool.InitResultFile(cResultfile);
    // for some reason this does not work
    // error I get is new TRootSnifferFull("sniff");
    // cTool.StartHttpServer();
    cTool.ConfigureHw();

// measure hybrid current and temperature
#ifdef __ANTENNA__
    cTemp    = cAntenna.GetHybridTemperature(CHIPSLAVE);
    cCurrent = cAntenna.GetHybridCurrent(CHIPSLAVE);
    sprintf(cBuffer, "Hybrid %s [after configuration with default setttings]: temperature reading %.2f °C, current reading %.2f mA", cHybridId.c_str(), cTemp, cCurrent);
    LOG(INFO) << BOLDBLUE << cBuffer << RESET;
    cAntenna.close();
#endif

    // align back-end
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Start(0);
    // reset all chip and board registers
    // to what they were before this tool was called
    cBackEndAligner.Reset();

    // if CIC is enabled then align CIC first
    if(cWithCIC)
    {
        CicFEAlignment cCicAligner;
        cCicAligner.Inherit(&cTool);
        cCicAligner.Start(0);
        // reset all chip and board registers
        // to what they were before this tool was called
        cCicAligner.Reset();
        cCicAligner.dumpConfigFiles();
    }

    // measure some of the AMUX output voltages using ADC on UIB
    // MonitorAmux & hybridTester does not exist in this branch, nor it should...
    // HybridTester cHybridTester;
    // cHybridTester.Inherit (&cTool);
    // cHybridTester.Initialize();
    // monitor AMUX
    // cHybridTester.MonitorAmux("VBG_LDO");
    // cHybridTester.MonitorAmux("VBGbias");

    // equalize thresholds on readout chips
    if(cTune)
    {
        t.start();
        // now create a PedestalEqualization object
        PedestalEqualization cPedestalEqualization;
        cPedestalEqualization.Inherit(&cTool);
        // second parameter disables stub logic on CBC3
        cPedestalEqualization.Initialise(cAllChan, true);
        cPedestalEqualization.FindVplus();
        cPedestalEqualization.FindOffsets();
        cPedestalEqualization.writeObjects();
        cPedestalEqualization.dumpConfigFiles();
        cPedestalEqualization.resetPointers();
        t.show("Time to tune the front-ends on the system: ");
    }

#ifdef __ANTENNA__
    Antenna cAntenna2;
    cAntenna2.ConfigureSlaveADC(CHIPSLAVE);
    cTemp    = cAntenna2.GetHybridTemperature(CHIPSLAVE);
    cCurrent = cAntenna2.GetHybridCurrent(CHIPSLAVE);
    sprintf(cBuffer, "Hybrid %s [after calibration]: temperature reading %.2f °C, current reading %.2f mA", cHybridId.c_str(), cTemp, cCurrent);
    LOG(INFO) << BOLDBLUE << cBuffer << RESET;
    cAntenna2.close();
#endif

    // measure noise on FE chips
    if(cMeasurePedeNoise)
    {
        t.start();
        // if this is true, I need to create an object of type PedeNoise from the members of Calibration
        // tool provides an Inherit(Tool* pTool) for this purpose
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit(&cTool);
        // second parameter disables stub logic on CBC3
        cPedeNoise.Initialise(cAllChan, true); // canvases etc. for fast calibration
        cPedeNoise.measureNoise();
        cPedeNoise.writeObjects();
        cPedeNoise.dumpConfigFiles();
        t.stop();
        t.show("Time to Scan Pedestals and Noise");
    }
    if(cEvaluate)
    {
        int cSigma = cmd.foundOption("evaluate") ? convertAnyInt(cmd.optionValue("evaluate").c_str()) : 3;
        // some extra stuff ...
        ExtraChecks cExtra;
        cExtra.Inherit(&cTool);
        cExtra.Initialise();
        LOG(INFO) << BOLDBLUE << "Measuring noise and setting thresholds to " << +cSigma << " noise units away from pedestal...." << RESET;
        cExtra.Evaluate(cSigma, 0, true);
        cExtra.writeObjects();
        cExtra.resetPointers();
    }
    // inject hits and stubs using mask and compare input against output
    if(cCheckData)
    {
        std::string          cArgsStr = cmd.optionValue("checkData");
        std::vector<uint8_t> cArgs;
        std::stringstream    cArgsSS(cArgsStr);
        int                  i;
        while(cArgsSS >> i)
        {
            cArgs.push_back(i);
            if(cArgsSS.peek() == ',') cArgsSS.ignore();
        };

        t.start();
        DataChecker cDataChecker;
        cDataChecker.Inherit(&cTool);
        cDataChecker.Initialise();
        if(cmd.foundOption("checkClusters")) cDataChecker.ClusterCheck(cArgs);
        if(cmd.foundOption("checkSLink")) cDataChecker.WriteSlinkTest(cmd.optionValue("checkSLink"));
        if(cmd.foundOption("checkStubs")) cDataChecker.StubCheck(cArgs);
        if(cmd.foundOption("noiseInjection")) cDataChecker.StubCheckWNoise(cArgs);
        if(cmd.foundOption("checkReadData")) cDataChecker.ReadDataTest();
        if(cmd.foundOption("checkAsync")) cDataChecker.AsyncTest();
        if(cmd.foundOption("checkReadNEvents")) cDataChecker.ReadNeventsTest();
        if(cSaveToFile) cDataChecker.CollectEvents();

        // cDataChecker.ReadNeventsTest();
        // cDataChecker.DataCheck(cFEsToCheck,0,0);
        // cDataChecker.ReadDataTest();
        // cDataChecker.HitCheck();
        cDataChecker.writeObjects();
        cDataChecker.resetPointers();
        t.show("Time to check data of the front-ends on the system: ");
    }

    // For next step... set all thresholds on CBCs to 560
    if(cmd.foundOption("threshold"))
    {
        cTool.setSameDac("VCth", cThreshold);
        LOG(INFO) << BOLDBLUE << "Threshold for next steps is set to " << +cThreshold << " DAC units." << RESET;
    }
    // Inject charge with antenna circuit and look for opens
    if(cFindOpens)
    {
#ifdef __ANTENNA__
        int cAntennaDelay = (cmd.foundOption("antennaDelay")) ? convertAnyInt(cmd.optionValue("antennaDelay").c_str()) : -1;
        int cLatencyRange = (cmd.foundOption("latencyRange")) ? convertAnyInt(cmd.optionValue("latencyRange").c_str()) : -1;

        OpenFinder::Parameters cOfp;
        // hard coded for now TODO: make this configurable
        cOfp.potentiometer = 0x265;
        // antenna group
        auto cSetting     = cTool.fSettingsMap.find("AntennaGroup");
        cOfp.antennaGroup = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : (0);

        // antenna delay
        if(cAntennaDelay > 0)
            cOfp.antennaDelay = cAntennaDelay;
        else
        {
            auto cSetting     = cTool.fSettingsMap.find("AntennaDelay");
            cOfp.antennaDelay = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : (200);
        }

        // scan range for latency
        if(cLatencyRange > 0)
            cOfp.latencyRange = cLatencyRange;
        else
        {
            auto cSetting     = cTool.fSettingsMap.find("ScanRange");
            cOfp.latencyRange = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : (10);
        }

        OpenFinder cOpenFinder;
        cOpenFinder.Inherit(&cTool);
        cOpenFinder.Initialise(cOfp);
        LOG(INFO) << BOLDBLUE << "Starting open finding measurement [antenna potentiometer set to 0x" << std::hex << cOfp.potentiometer << std::dec << " written to the potentiometer" << RESET;
        cOpenFinder.FindOpens();
        // TODO: write this one from cLatencyScan.writeObjects();
        // cOpenFinder.writeObjects();
#endif
    }
    // inject charge with TP and look for shorts
    if(cShortFinder)
    {
        ShortFinder cShortFinder;
        cShortFinder.Inherit(&cTool);
        cShortFinder.Initialise();
        cShortFinder.Start();
        cShortFinder.Stop();
    }

    cTool.SaveResults();
    cTool.WriteRootFile();
    cTool.CloseResultFile();
    cTool.Destroy();

    if(!batchMode) cApp.Run();
    cGlobalTimer.stop();
    cGlobalTimer.show("Total execution time: ");

    return 0;
}
