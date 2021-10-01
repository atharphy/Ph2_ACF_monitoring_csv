#include <cstring>

#include "DPInterface.h"
#include "OpenFinder.h"
#include "PSHybridTester.h"
#include "PedeNoise.h"
#include "PedestalEqualization.h"
#include "ShortFinder.h"
#include "Utils/Timer.h"
#include "Utils/Utilities.h"
#include "Utils/argvparser.h"
#include "tools/BackEndAlignment.h"
#include "tools/CicFEAlignment.h"
#include "tools/DataChecker.h"

#ifdef __USE_ROOT__
#include "TApplication.h"
#include "TROOT.h"
#endif

#define __NAMEDPIPE__

#ifdef __NAMEDPIPE__
#include "gui_logger.h"
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

    // cmd.defineOption("tuneOffsets", "tune offsets on readout chips connected to CIC.");
    // cmd.defineOptionAlternative("tuneOffsets", "t");

    // cmd.defineOption("measurePedeNoise", "measure pedestal and noise on readout chips connected to CIC.");
    // cmd.defineOptionAlternative("measurePedeNoise", "m");

    // cmd.defineOption("findShorts", "look for shorts", ArgvParser::NoOptionAttribute);
    // cmd.defineOption("findOpens", "perform latency scan with antenna on UIB", ArgvParser::NoOptionAttribute);
    // cmd.defineOption("mpaTest", "Check MPA input with Data Player Pattern", ArgvParser::NoOptionAttribute /*| ArgvParser::OptionRequires*/);
    cmd.defineOption("ssapair", "Debug selected SSA pair. Possible options: 01, 12, 23, 34, 45, 56, 67", ArgvParser::OptionRequiresValue);

    cmd.defineOption("hybridId", "Serial Number of front-end hybrid. Default value: xxxx", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);

    // cmd.defineOption("pattern", "Data Player Pattern", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    // cmd.defineOptionAlternative("pattern", "p");

    cmd.defineOption("withCIC", "Perform CIC alignment steps", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkAsync", "Check async readout", ArgvParser::NoOptionAttribute);

    // general
    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    cmd.defineOption("USB", "USB iProduct string to identify the test card when using the USB functionalities.", ArgvParser::OptionRequiresValue);
    cmd.defineOption("useGui",
                     "Support for running the test from the gui for hybrids testing. The named pipe for communication needs to be passed as the last parameter. Default: false",
                     ArgvParser::NoOptionAttribute);
    cmd.defineOption("output", "Output directory. Default: Results/");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile    = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Commissioning.xml";
    bool        batchMode  = (cmd.foundOption("batch")) ? true : false;
    std::string cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    std::string cHybridId  = (cmd.foundOption("hybridId")) ? cmd.optionValue("hybridId") : "xxxx";
    // uint8_t cPattern = ( cmd.foundOption ( "mpaTest" ) ) ? convertAnyInt ( cmd.optionValue ( "mpaTest" ).c_str() ) : 0;
    const std::string cSSAPair = (cmd.foundOption("ssapair")) ? cmd.optionValue("ssapair") : "";
    cDirectory += Form("FEH_PS_%s", cHybridId.c_str());

    std::string cUsbId = (cmd.foundOption("USB")) ? cmd.optionValue("USB") : ""; // Default option?
    bool        cGui   = (cmd.foundOption("useGui"));

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::string cResultfile = "Hybrid";
    Timer       t;

#ifdef __TCUSB__
#endif

    if(cGui)
    {
        // Initialize gui communication with named pipe
        gui::init(argv[argc - 1]);

        gui::status("Initializing test");
        gui::progress(0 / 10.0);
    }

#ifdef __TCUSB__
#endif

    std::stringstream outp;
    // hybrid testing tool
    // going to use this because it also
    // allows me to initialize voltages
    // and check voltages
    PSHybridTester cHybridTester;
    LOG(INFO) << "File " << cHWFile << RESET;
    LOG(INFO) << &cHWFile << RESET;
    cHybridTester.InitializeHw(cHWFile, outp);
    cHybridTester.InitializeSettings(cHWFile, outp);
    cHybridTester.CreateResultDirectory(cDirectory);
    cHybridTester.InitResultFile(cResultfile);
    cHybridTester.bookSummaryTree();
    // set voltage  on PS FEH
    // cHybridTester.SetHybridVoltage();
    // cHybridTester.RunHybridETest();
    LOG(INFO) << outp.str() << RESET;
    // cHybridTester.ConfigureHw ();

    // interface to data player
    DPInterface         cDPInterfacer;
    BeBoardFWInterface* cInterface = dynamic_cast<BeBoardFWInterface*>(cHybridTester.fBeBoardFWMap.find(0)->second);

    // Configure and Start DataPlayer
    uint8_t cDataPlayerPattern = 0xAA;
    cDPInterfacer.Configure(cInterface, cDataPlayerPattern);
    cDPInterfacer.Start(cInterface);
    if(cDPInterfacer.IsRunning(cInterface)) { LOG(INFO) << BOLDBLUE << "FE data player " << BOLDGREEN << " running correctly!" << RESET; }
    else
        LOG(INFO) << BOLDRED << "Could not start FE data player" << RESET;

    cDPInterfacer.Stop(cInterface);
    cDPInterfacer.CheckNPatterns(cInterface);

    cHybridTester.SaveResults();
    cHybridTester.WriteRootFile();
    cHybridTester.CloseResultFile();
    cHybridTester.Destroy();
    if(!batchMode) cApp.Run();
    LOG(INFO) << "Exiting" << RESET;
    return 0;
}
