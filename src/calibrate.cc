#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/Module.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/ChipInterface.h"
#include "../HWInterface/ReadoutChipInterface.h"
#include "../Utils/Timer.h"
#include "../Utils/argvparser.h"
#include "../tools/PedeNoise.h"
#include "../tools/PedestalEqualization.h"
#include "TApplication.h"
#include "TROOT.h"
#include "tools/BackEndAlignment.h"
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
    cmd.setIntroductoryDescription("CMS Ph2_ACF  calibration routine using K. Uchida's algorithm or a fast algorithm");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hw Description File . Default value: settings/Calibration8CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("output", "Output Directory . Default value: Results", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("skip", "skip scaning VCth vs Vplus", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("skip", "s");

    // cmd.defineOption( "old", "Use old calibration algorithm", ArgvParser::NoOptionAttribute );
    // cms.defineOptionAlternative ("old", "v" );

    cmd.defineOption("noise", "Perform noise scan after Offset tuning", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("noise", "n");

    cmd.defineOption("allChan", "Do calibration using all channels? Default: false", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("allChan", "a");

    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    cmd.defineOption("capture", "Capture communication with board (extension .raw).", ArgvParser::OptionRequiresValue);
    cmd.defineOption("replay", "Replay previously captured communication (extension .raw).", ArgvParser::OptionRequiresValue);

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile    = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Calibration8CBC.xml";
    std::string cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    cDirectory += "Calibration";
    bool cVplus = (cmd.foundOption("skip")) ? false : true;
    // bool cOld = ( cmd.foundOption( "old" ) ) ? true : false;

    bool cAllChan   = (cmd.foundOption("allChan")) ? true : false;
    bool batchMode  = (cmd.foundOption("batch")) ? true : false;
    bool cNoiseScan = (cmd.foundOption("noise")) ? true : false;
    if(cmd.foundOption("capture") == true)
        RegManager::enableCapture(cmd.optionValue("capture").insert(0, "./"));
    else if(cmd.foundOption("replay") == true)
        RegManager::enableReplay(cmd.optionValue("replay"));

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    Timer t;

    // create a genereic Tool Object, I can then construct all other tools from that using the Inherit() method
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
    cTool.InitResultFile("PedestalEqualizationResults");
    cTool.StartHttpServer();

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

    // cTool.ConfigureHw ();
    // if ( !cOld )
    //{
    t.start();

    ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(cTool.fDetectorContainer->at(0)->at(0)->at(0)->at(0));
    bool         fDisableStubLogic = true;
    if(cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA) fDisableStubLogic = false;

    // now create a PedestalEqualization object
    PedestalEqualization cPedestalEqualization;
    cPedestalEqualization.Inherit(&cTool);
    // second parameter disables stub logic on CBC3
    // cPedestalEqualization.Initialise ( false, true );
    cPedestalEqualization.Initialise(cAllChan, fDisableStubLogic);

    if(cVplus) cPedestalEqualization.FindVplus();

    cPedestalEqualization.FindOffsets();
    cPedestalEqualization.writeObjects();
    cPedestalEqualization.dumpConfigFiles();
    cPedestalEqualization.resetPointers();
    t.stop();
    t.show("Time to Calibrate the system: ");
    if(cNoiseScan)
    {
        t.start();
        // if this is true, I need to create an object of type PedeNoise from the members of PedestalEqualization
        // tool provides an Inherit(Tool* pTool) for this purpose
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit(&cTool);
        // second parameter disables stub logic on CBC3
        cPedeNoise.Initialise(cAllChan, fDisableStubLogic); // canvases etc. for fast calibration
        cPedeNoise.measureNoise();
        // cPedeNoise.measureNoise(200);

        // cPedeNoise.sweepSCurves (225);
        // cPedeNoise.sweepSCurves (205);

        cPedeNoise.Validate();
        cPedeNoise.writeObjects();
        cPedeNoise.dumpConfigFiles();
        cPedeNoise.resetPointers();
        t.stop();
        t.show("Time to Scan Pedestals and Noise");
    }

    cTool.SaveResults();
    cTool.WriteRootFile();
    cTool.CloseResultFile();
    cTool.Destroy();

    if(!batchMode) cApp.Run();
    return 0;
}
