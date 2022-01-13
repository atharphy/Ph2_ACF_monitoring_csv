#include <cstring>

#include "Utils/Timer.h"
#include "Utils/Utilities.h"
#include "Utils/argvparser.h"
#include "boost/format.hpp"
#include "tools/BackEndAlignment.h"
#include "tools/BeamTestCheck2S.h"
#include "tools/CicFEAlignment.h"
#include "tools/DataChecker.h"
#include "tools/LatencyScan.h"
#include "tools/LinkAlignmentOT.h"
#include "tools/MemoryCheck2S.h"
#include "tools/PSAlignment.h"
#include "tools/PedeNoise.h"
#include "tools/PedeNoiseTime.h"
#include "tools/PedestalEqualization.h"
#include "tools/RegisterTester.h"
#include "tools/StubBackEndAlignment.h"

#ifdef __POWERSUPPLY__
// Libraries
#include "DeviceHandler.h"
#include "PowerSupply.h"
#include "PowerSupplyChannel.h"
#endif

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

uint16_t returnRunNumber(std::string cFileName)
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
    return (uint16_t)(cRunNumber + 1);
}

std::vector<uint8_t> getArgs(std::string pArgsStr)
{
    std::vector<uint8_t> cSides;
    std::stringstream    cArgsSS(pArgsStr);
    int                  i;
    while(cArgsSS >> i)
    {
        cSides.push_back(i);
        if(cArgsSS.peek() == ',') cArgsSS.ignore();
    };
    return cSides;
}

int main(int argc, char* argv[])
{
    auto* baseDirChar_p = std::getenv("PH2ACF_BASE_DIR");
    if(baseDirChar_p == nullptr)
    {
        LOG(ERROR) << "Error, the environment variable PH2ACF_BASE_DIR is not initialized (hint: source setup.sh)";
        exit(1);
    }

    std::string loggerConfigFile = std::getenv("PH2ACF_BASE_DIR");
    loggerConfigFile += "/settings/logger.conf";
    el::Configurations conf(loggerConfigFile);

    el::Loggers::reconfigureAllLoggers(conf);

    el::Helpers::installLogDispatchCallback<gui::LogDispatcher>("GUILogDispatcher");
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
    cmd.defineOption("output", "output directory for result files.");
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("measurePedeNoise", "measure pedestal and noise on readout chips connected to CIC.");
    cmd.defineOptionAlternative("measurePedeNoise", "m");

    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    cmd.defineOption("reconfigure", "Reconfigure Hardware"); 
    cmd.defineOption("gui", "Support for running the test from a guig. The named pipe for communication needs to be passed as parameter. Default: /tmp/guiDummyPipe", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("gui", "g");


    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile          = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Commissioning.xml";
    bool        batchMode        = (cmd.foundOption("batch")) ? true : false;
    std::string cInjectionSource = (cmd.foundOption("injectionTest")) ? cmd.optionValue("injectionTest") : "digital";
    std::string cDirectory       = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";

    bool        cGui              = (cmd.foundOption("gui"));

    std::string guiPipe = (cGui) ? cmd.optionValue("gui") : "/tmp/guiDummyPipe";

    gui::init(guiPipe.c_str());
    gui::status("Initializing test");
    gui::progress(0);

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::string cResultfile = "Hybrid";
    Timer       t;
    Timer       cGlobalTimer;
    cGlobalTimer.start();

    std::stringstream outp;
    Tool              cTool;

    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    cTool.CreateResultDirectory(cDirectory, false, false);
    cTool.InitResultFile(cResultfile);

    gui::message("Hardware configured");
    gui::progress(0.5 / 10.0);





    // align CIC-lpGBT-BE

    bool cIgnoreI2c    = false;
    bool cReInitialize = true;

    gui::status("Aligning links");
    gui::progress(0.75 / 10.0);

    cTool.ConfigureHw(cIgnoreI2c, cReInitialize);

    // map MPA outputs for PS module
    PSAlignment cPSAlignment;
    cPSAlignment.Inherit(&cTool);
    cPSAlignment.Initialise();
    cPSAlignment.MapMPAOutputs();
    cPSAlignment.ConfigureDefaultAlignmentParameters();
    cPSAlignment.Reset();

    LinkAlignmentOT cLinkAlignment;
    cLinkAlignment.Inherit(&cTool);
    try
    {
        cLinkAlignment.Start(0);
    }
    catch(const std::exception& e)
    {
        LOG(INFO) << BOLDRED << "Could not align link in the BE... stopping here." << RESET;
        return (666);
    }
    cLinkAlignment.waitForRunToBeCompleted();
    cLinkAlignment.dumpConfigFiles();
    if(!cLinkAlignment.getStatus())
    {
        LOG(INFO) << BOLDRED << "Could not align link in the BE... stopping here." << RESET;
        return (666);
    }

    gui::message("Backend aligned successfully");
    gui::status("Aligning CIC");
    gui::progress(1.75 / 10.0);
    // align FEs - CIC
    CicFEAlignment cCicAligner;
    cCicAligner.Inherit(&cTool);
    cCicAligner.Start(0);
    cCicAligner.waitForRunToBeCompleted();
    cCicAligner.dumpConfigFiles();


    // equalize thresholds on readout chips
    gui::message("CIC aligned successfully");
    gui::status("Tuning front-end chips");
    gui::progress(2.75 / 10.0);

    bool cAllChan = true;//(cmd.foundOption("allChan")) ? true : false;
    t.start();
    // now create a PedestalEqualization object
    PedestalEqualization cPedestalEqualization;
    cPedestalEqualization.Inherit(&cTool);
    cPedestalEqualization.Initialise(cAllChan, true);
    cPedestalEqualization.FindVplus();
    cPedestalEqualization.FindOffsets();
    cPedestalEqualization.Reset();
    // second parameter disables stub logic on CBC3
    cPedestalEqualization.writeObjects();
    cPedestalEqualization.dumpConfigFiles();
    cPedestalEqualization.resetPointers();
    t.show("Time to tune the front-ends on the system: ");

    gui::message("Front-end chips calibrated successfully.");
    gui::progress(3.75 / 10.0);

    // measure noise on FE chips
    if(cmd.foundOption("measurePedeNoise"))
    {
        gui::message("Measure Noise");
        gui::progress(4.75 / 10.0);
        bool cAllChan = true;//(cmd.foundOption("allChan")) ? true : false;
        LOG(INFO) << BOLDMAGENTA << "Measuring pedestal and noise" << RESET;
        t.start();
        // if this is true, I need to create an object of type PedeNoise from the members of Calibration
        // tool provides an Inherit(Tool* pTool) for this purpose
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit(&cTool);
        cPedeNoise.Initialise(cAllChan, true); // canvases etc. for fast calibration
        // cPedeNoise.scanScurves();
        cPedeNoise.measureNoise();
        // cPedeNoise.Validate();
        cPedeNoise.writeObjects();
        cPedeNoise.dumpConfigFiles();
        cPedeNoise.Reset();
        t.stop();
        t.show("Time to Scan Pedestals and Noise");
    }


    cTool.SaveResults();
    cTool.WriteRootFile();
    cTool.CloseResultFile();
    cTool.Destroy();
    if(!batchMode) cApp.Run();
    cGlobalTimer.stop();
    cGlobalTimer.show("Total execution time: ");

    gui::message("Results saved");
    gui::status("Test done");
    gui::progress(10.0 / 10.0);
    return 0;
}
