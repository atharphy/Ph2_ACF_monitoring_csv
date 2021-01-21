#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/Module.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/ChipInterface.h"
#include "../tools/CMTester.h"
#include "../tools/PedeNoise.h"
#include "../tools/PedestalEqualization.h"
#include <cstring>

#include "../Utils/argvparser.h"
#include <TApplication.h>
//#include "../Utils/easylogging++.h"
#include "TROOT.h"

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
    cmd.setIntroductoryDescription("CMS Ph2_ACF  applicaton to take data and analyze it with respect to common mode noise");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hw Description File . Default value: settings/CMTest2CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("output", "Output Directory . Default value: Results/", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("scan", "scan for noisy strips", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("scan", "s");

    cmd.defineOption("calibrate", "Find offsets and Vcth, in case they are not set in the xml", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("calibrate", "c");

    cmd.defineOption("pedestalshift", "Set Vcth to pedestal + X. Default value: 0.", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("pedestalshift", "p");

    cmd.defineOption("manualvcth", "Set Vcth by hand", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("manualvcth", "m");

    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    cmd.defineOption("gui", "option only suitable when launching from gui", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("gui", "g");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile        = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/CMTest2CBC.xml";
    std::string cDirectory     = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    int         cPedestalShift = (cmd.foundOption("pedestalshift")) ? convertAnyInt(cmd.optionValue("pedestalshift").c_str()) : 0;
    int         cManualVcth    = (cmd.foundOption("manualvcth")) ? convertAnyInt(cmd.optionValue("manualvcth").c_str()) : 0;
    cDirectory += "CMTest";
    bool cScan      = (cmd.foundOption("scan")) ? true : false;
    bool cCalibrate = (cmd.foundOption("calibrate")) ? true : false;
    bool batchMode  = (cmd.foundOption("batch")) ? true : false;

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::stringstream outp;

    Tool cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    outp.str("");
    cTool.CreateResultDirectory(cDirectory);
    cTool.InitResultFile("CMTest");
    cTool.StartHttpServer();
    cTool.ConfigureHw();

    if(cCalibrate) // Calibrate Voffset
    {
        // Find offsets
        PedestalEqualization cPedestalEqualization;
        cPedestalEqualization.Inherit(&cTool);
        cPedestalEqualization.Initialise(false);
        cPedestalEqualization.FindVplus();
        cPedestalEqualization.FindOffsets();
        cPedestalEqualization.writeObjects();
        // cPedestalEqualization.dumpConfigFiles();
    }

    // Measure noise (this is not optional, since it is an input in the CMTest plot)

    PedeNoise cPedeNoise;
    cPedeNoise.Inherit(&cTool);
    cPedeNoise.Initialise(true); // true = all channels (as opposed to test groups)
    cPedeNoise.measureNoise();
    // cPedeNoise.Validate(); // This masks noisy channels, already done optionally by CMTester ScanNoiseChannels
    cPedeNoise.writeObjects();

    // Set Vcth to pedestal, or overload with manual setting
    std::vector<double_t> cNoiseV;
    ThresholdVisitor      cVisitor(cTool.fReadoutChipInterface, 0);
    ModuleContainer*      cFe = cPedeNoise.fDetectorContainer->at(0)->at(0)->at(0);
    int                   i   = 0;
    for(auto cCbc: *cFe)
    {
        uint16_t cPedestal = 0;  // round (cPedeNoise.getPedestal (cCbc) );
        double   cNoise    = 0.; // cPedeNoise.getNoise (cCbc);
        cNoiseV.push_back(cNoise);

        if(cManualVcth == 0)
        {
            ReadoutChip* theCbc = static_cast<ReadoutChip*>(cCbc);
            cVisitor.setThreshold(cPedestal + cPedestalShift);
            cVisitor.visitReadoutChip(*theCbc); // Visit a specific CBC
            theCbc->accept(cVisitor);           // Should probably make a special Visitor to set a vector of Vcth's
            LOG(INFO) << BOLDRED << "CBC" << i << ": set threshold to pedestal (" << cPedestal << ") plus " << cPedestalShift << ": " << cPedestal + cPedestalShift << RESET;
        }
        else
        {
            cVisitor.setThreshold(cManualVcth);
            cTool.accept(cVisitor); // Do this for all CBCs
            LOG(INFO) << BOLDRED << "CBC" << i << ": set threshold manually to " << cManualVcth << RESET;
        }

        i++;
    }

#ifdef __USE_ROOT__

    // Runs on 10*Nevents
    CMTester cTester;
    cTester.Inherit(&cTool);
    cTester.StartHttpServer(8082);
    cTester.Initialize();
    cTester.SetTotalNoise(cNoiseV);

    if(cScan) cTester.ScanNoiseChannels();

    cTester.TakeData();
    cTester.FinishRun();
    cTester.SaveResults();
    cTester.CloseResultFile();
    cTester.Destroy();
#endif

    if(!batchMode) cApp.Run();

    return 0;
}
