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

#include "../Utils/gui_logger.h"

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

    cmd.defineOption("gui", "Support for running the test from a guig. The named pipe for communication needs to be passed as parameter. Default: /tmp/guiDummyPipe", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("gui", "g");
    cmd.defineOption("output", "Output folder fore the results", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");


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
    std::string cHWFile           = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Commissioning.xml";
    bool        cMeasurePedeNoise = (cmd.foundOption("measurePedeNoise"));
    bool        cGui              = (cmd.foundOption("gui"));
    std::string cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";




    LOG(INFO) << "HW File: " << cHWFile;
    LOG(INFO) << "Directory: " << cDirectory;
    std::string copycommand = "cp -a /home/readout/DummyResultFiles/. " + cDirectory;
    LOG(INFO) << copycommand;

    LOG(INFO) << system(copycommand.c_str());
    std::string guiPipe = (cGui) ? cmd.optionValue("gui") : "/tmp/guiDummyPipe";

    gui::init(guiPipe.c_str());
    gui::status("Initializing test");
    gui::progress(0);

    LOG(INFO) << "Initializing test";

    usleep(500000);

    gui::message("Hardware configured");
    gui::status("Reading CBC serial numbers");
    gui::progress(0.5 / 10.0);

    gui::status("Aligning backend");
    gui::progress(0.75 / 10.0);

    LOG(INFO) << "Aligning backend";

    usleep(500000);

    gui::message("Backend aligned successfully");
    gui::status("Aligning CIC");
    gui::progress(1.75 / 10.0);

    LOG(INFO) << "Backend aligned successfully";
    usleep(500000);

    gui::message("CIC aligned successfully");
    gui::status("Calibrating front-end chips");
    gui::progress(2.75 / 10.0);

    LOG(INFO) << "CIC aligned successfully";
    usleep(500000);

    gui::message("Front-end chips calibrated successfully.");
    gui::progress(3.75 / 10.0);

    LOG(INFO) << "Front-end chips calibrated successfully.";
    usleep(500000);

    if (cMeasurePedeNoise)
    {
        gui::message("Measure Noise");
        gui::progress(4.75 / 10.0);
        usleep(500000);
        LOG(INFO) << "Noise measured";
    }

    gui::message("Evaluation done");
    gui::progress(5.75 / 10.0);

    LOG(INFO) << "Evaluation done";
    usleep(500000);

    gui::message("Data quality check done");
    gui::progress(6.75 / 10.0);

    LOG(INFO) << "Data quality check done";
    usleep(500000);

    gui::message("Short finding done");
    gui::progress(7.75 / 10.0);

    LOG(INFO) << "Short finding done";
    gui::status("Saving test results...");

    usleep(500000);

    gui::message("Results saved");
    gui::status("Test done");
    gui::progress(10.0 / 10.0);

    return 0;
}
