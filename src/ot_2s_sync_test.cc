#include <cstring>

#include "ExtraChecks.h"
#include "Utils/Timer.h"
#include "Utils/Utilities.h"
#include "Utils/argvparser.h"
#include "boost/format.hpp"
#include "tools/BackEndAlignment.h"
#include "tools/StubBackEndAlignment.h"
#include "tools/BeamTestCheck2S.h"
#include "tools/CicFEAlignment.h"
#include "tools/DataChecker.h"
#include "tools/MemoryCheck2S.h"
#include "tools/PSAlignment.h"
#include "tools/PedeNoise.h"
#include "tools/PedeNoiseTime.h"
#include "tools/PedestalEqualization.h"
#include "tools/RegisterTester.h"
#include "tools/LinkAlignmentOT.h"
#include "tools/LatencyScan.h"

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

// reference volage for lpgBT
float VREF_LPGBT        = 1.0;
float cConversionFactor = VREF_LPGBT / 1024.;

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

    cmd.defineOption("reconfigure", "Reconfigure Hardware");
    cmd.defineOptionAlternative("reconfigure", "r");

    cmd.defineOption("measurePedeNoise", "measure pedestal and noise on readout chips connected to CIC.", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("measurePedeNoise", "m");

    cmd.defineOption("tuneOffsets", "Tune Offsets  ", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("tuneOffsets", "t");
    
    cmd.defineOption("save", "Save the data to a raw file.  ", ArgvParser::NoOptionAttribute);
    // general
    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");
    // injection source 
    cmd.defineOption("injectionSource", "Source for injection", ArgvParser::OptionRequiresValue);
    
    cmd.defineOption("useReadNEvents", "Check ReadNEvents method... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("limitTriggers", "Only accept exactly the correct number of triggers", ArgvParser::NoOptionAttribute);
    cmd.defineOption("events", "Number of Events . Default value: 10", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("events", "e");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile           = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Commissioning.xml";
    bool        batchMode         = (cmd.foundOption("batch")) ? true : false;
    bool cSaveToFile = cmd.foundOption("save");
    std::string   cInjectionSource = (cmd.foundOption("injectionSource")) ? cmd.optionValue("injectionSource") : "none";
    auto          cRunNumber = returnRunNumber("RunNumbers.dat");
    auto cDisableStubs = (cmd.foundOption("measurePedeNoise")) ? convertAnyInt(cmd.optionValue("measurePedeNoise").c_str()) : 1;
    
    std::ofstream cRunLog;
    cRunLog.open("RunNumbers.dat", std::fstream::app);
    cRunLog << cRunNumber << "\n";
    cRunLog.close();
    LOG(INFO) << BOLDBLUE << "Run number is " << +cRunNumber << RESET;
    std::stringstream cDirectory; 
    cDirectory << "OT_ModuleTest_" << cRunNumber;
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
    if(cSaveToFile)
    {
        char cRawFileName[80];
        std::snprintf(cRawFileName, sizeof(cRawFileName), "Run%.05d.raw", cRunNumber);
        std::string cRawFile = cRawFileName;
        cTool.addFileHandler(cRawFile, 'w');
        LOG(INFO) << BOLDBLUE << "Writing Binary Rawdata to:   " << cRawFile;
    }
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    cTool.CreateResultDirectory(cDirectory.str(), false, false);
    cTool.InitResultFile(cResultfile);

    if( cmd.foundOption("reconfigure"))
    {
        cTool.ConfigureHw();
        // align CIC-lpGBT-BE 
        LinkAlignmentOT cLinkAlignment; 
        cLinkAlignment.Inherit(&cTool);
        cLinkAlignment.Start(0);
        cLinkAlignment.waitForRunToBeCompleted();
        cLinkAlignment.dumpConfigFiles();

        if( !cLinkAlignment.getStatus() ) 
        {
            LOG (INFO) << BOLDRED << "Could not align link in the BE... stopping here." << RESET;
            return(666);
        }

        // align FEs - CIC
        CicFEAlignment cCicAligner;
        cCicAligner.Inherit(&cTool);
        cCicAligner.Start(0);
        cCicAligner.waitForRunToBeCompleted();
        cCicAligner.dumpConfigFiles();
        
        // LinkAlignmentOT cLinkAlignment; 
        // cLinkAlignment.Inherit(&cTool);
        // cLinkAlignment.Initialise();
        // cLinkAlignment.AlignStubPackage();    
        // cLinkAlignment.Reset();
        
        // time align stubs with L1 data in the BE 
        // LOG (INFO) << BOLDBLUE << "Performing time alignment of stub data with L1 data in the BE " << RESET;
        // StubBackEndAlignment cStubBackEndAligner;
        // cStubBackEndAligner.Inherit(&cTool);
        // cStubBackEndAligner.Start(0);
        // cStubBackEndAligner.waitForRunToBeCompleted();
    }

    LinkAlignmentOT cLinkAlignment; 
    cLinkAlignment.Inherit(&cTool);
    cLinkAlignment.Initialise();
    cLinkAlignment.AlignStubPackage();    
    cLinkAlignment.Reset();

   
    // Tune offsets
    if( cmd.foundOption("tuneOffsets"))
    {
        t.start();
        PedestalEqualization cPedestalEqualization;
        cPedestalEqualization.Inherit(&cTool);
        cPedestalEqualization.Inherit(&cTool);
        cPedestalEqualization.Initialise(true, true);
        cPedestalEqualization.FindVplus();
        cPedestalEqualization.FindOffsets();
        cPedestalEqualization.Reset();
        cPedestalEqualization.writeObjects();
        cPedestalEqualization.dumpConfigFiles();
        cPedestalEqualization.resetPointers();
        cPedestalEqualization.Reset();
        t.show("Time to tune the front-ends on the system: ");
    }
    
    // measure noise on FE chips
    if(cmd.foundOption("measurePedeNoise"))
    {
        LOG(INFO) << BOLDMAGENTA << "Measuring pedestal and noise" << RESET;
        t.start();
        // if this is true, I need to create an object of type PedeNoise from the members of Calibration
        // tool provides an Inherit(Tool* pTool) for this purpose
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit(&cTool);
        cPedeNoise.Initialise(true, (cDisableStubs==1)); // canvases etc. for fast calibration
        cPedeNoise.measureNoise();
        cPedeNoise.writeObjects();
        cPedeNoise.dumpConfigFiles();
        cPedeNoise.Reset();
        t.stop();
        t.show("Time to Scan Pedestals and Noise");
    }

    // run test for 2S     
    BeamTestCheck2S cCheck2S;
    cCheck2S.Inherit(&cTool);
    cCheck2S.Initialise();
    if( cInjectionSource.find("testPulse") != std::string::npos ) cCheck2S.CheckWithTP();
    else if( cInjectionSource.find("external") != std::string::npos )  cCheck2S.CheckWithExternal();
    cCheck2S.writeObjects();


    if(!batchMode) cApp.Run();
    cGlobalTimer.stop();
    cGlobalTimer.show("Total execution time: ");

    std::ofstream cGoodRuns;
    cGoodRuns.open("GoodRunNumbers.dat", std::fstream::app);
    cGoodRuns << cRunNumber << "\n";
    cGoodRuns.close();

    cTool.PrintRegCount();
    cTool.dumpConfigFiles();
    cTool.SaveResults();
    cTool.WriteRootFile();
    cTool.CloseResultFile();
    cTool.Destroy();
    return 0;
}
