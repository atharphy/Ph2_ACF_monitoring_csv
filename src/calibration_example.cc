#include "../Utils/Timer.h"
#include "../Utils/argvparser.h"
#include "../tools/CalibrationExample.h"
#include "TApplication.h"
#include "TROOT.h"
#include <cstring>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

INITIALIZE_EASYLOGGINGPP

int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF calibration example");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hw Description File . Default value: settings/Calibration8CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("output", "Output Directory . Default value: Results", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile    = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Calibration8CBC.xml";
    std::string cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    cDirectory += "CalibrationExample";
    bool batchMode = (cmd.foundOption("batch")) ? true : false;

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    // create a generic Tool Object, I can then construct all other tools from that using the Inherit() method
    // this tool stays on the stack and lives until main finishes - all other tools will update the HWStructure from
    // cTool
    Tool              cTool;
    std::stringstream outp;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    outp.str("");
    cTool.ConfigureHw();
    cTool.CreateResultDirectory(cDirectory);
    cTool.InitResultFile("CalibrationResults");

    Timer t;
    t.start();

    // now create a calibration object
    CalibrationExample theCalibrationExample;
    theCalibrationExample.Inherit(&cTool);
    theCalibrationExample.Initialise();
    theCalibrationExample.runCalibrationExample();
    theCalibrationExample.writeObjects();

    // Tool old style command (some of them will vanish/merged)
    cTool.dumpConfigFiles();
    cTool.resetPointers();
    cTool.SaveResults();
    cTool.WriteRootFile();
    cTool.CloseResultFile();
    cTool.Destroy();
    t.stop();
    t.show("Time to Run Calibration example");

    if(!batchMode) cApp.Run();
    return 0;
}
