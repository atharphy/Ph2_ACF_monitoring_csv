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

    cmd.defineOption("tuneOffsets", "tune offsets on readout chips connected to CIC.");
    cmd.defineOptionAlternative("tuneOffsets", "t");

    cmd.defineOption("measurePedeNoise", "measure pedestal and noise on readout chips connected to CIC.");
    cmd.defineOptionAlternative("measurePedeNoise", "m");

    cmd.defineOption("findShorts", "look for shorts", ArgvParser::NoOptionAttribute);
    cmd.defineOption("findOpens", "perform latency scan with antenna on UIB", ArgvParser::NoOptionAttribute);
    cmd.defineOption("mpaTest", "Check MPA input with Data Player Pattern [provide pattern]", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    cmd.defineOption("ssapair", "Debug selected SSA pair. Possible options: 01, 12, 23, 34, 45, 56, 67", ArgvParser::OptionRequiresValue);

    cmd.defineOption("threshold", "Threshold value to set on chips for open and short finding", ArgvParser::OptionRequiresValue);
    cmd.defineOption("hybridId", "Serial Number of front-end hybrid. Default value: xxxx", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);

    cmd.defineOption("pattern", "Data Player Pattern", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    cmd.defineOptionAlternative("pattern", "p");

    cmd.defineOption("withCIC", "Perform CIC alignment steps", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkAsync", "Check async readout", ArgvParser::NoOptionAttribute);

    // general
    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Commissioning.xml";
    // bool cFindOpens = (cmd.foundOption ("findOpens") )? true : false;
    // bool cShortFinder = ( cmd.foundOption ( "findShorts" ) ) ? true : false;
    bool              batchMode  = (cmd.foundOption("batch")) ? true : false;
    std::string       cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    std::string       cHybridId  = (cmd.foundOption("hybridId")) ? cmd.optionValue("hybridId") : "xxxx";
    uint8_t           cPattern   = (cmd.foundOption("mpaTest")) ? convertAnyInt(cmd.optionValue("mpaTest").c_str()) : 0;
    const std::string cSSAPair   = (cmd.foundOption("ssapair")) ? cmd.optionValue("ssapair") : "";
    cDirectory += Form("FEH_PS_%s", cHybridId.c_str());

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::string cResultfile = "Hybrid";
    Timer       t;

#ifdef __TCUSB__
#endif

    std::stringstream outp;
    // hybrid testing tool
    // going to use this because it also
    // allows me to initialize voltages
    // and check voltages
    PSHybridTester cHybridTester;
    cHybridTester.InitializeHw(cHWFile, outp);
    cHybridTester.InitializeSettings(cHWFile, outp);
    cHybridTester.CreateResultDirectory(cDirectory);
    cHybridTester.InitResultFile(cResultfile);
    // set voltage  on PS FEH
    cHybridTester.SetHybridVoltage();
    // LOG (INFO) << BOLDBLUE << "PS FEH current consumption pre-configuration..." << RESET;
    // cHybridTester.CheckHybridCurrents();
    // check voltage on PS FEH
    cHybridTester.CheckHybridVoltages();
    LOG(INFO) << outp.str();
    // select CIC readout
    // cHybridTester.SelectCIC(true);
    cHybridTester.ConfigureHw();
    // LOG (INFO) << BOLDBLUE << "PS FEH current consumption post-configuration..." << RESET;
    // cHybridTester.CheckHybridCurrents();

    // interface to data player
    DPInterface         cDPInterfacer;
    BeBoardFWInterface* cInterface = dynamic_cast<BeBoardFWInterface*>(cHybridTester.fBeBoardFWMap.find(0)->second);
    // need to do this if
    // reading out CIC
    // or testing MPA
    if(cmd.foundOption("withCIC") || cmd.foundOption("mpaTest"))
    {
        cHybridTester.SelectCIC(true);
        // align back-end
        BackEndAlignment cBackEndAligner;
        cBackEndAligner.Inherit(&cHybridTester);
        cBackEndAligner.Start(0);
        // reset all chip and board registers
        // to what they were before this tool was called
        cBackEndAligner.Reset();

        // Check if data player is running
        if(cDPInterfacer.IsRunning(cInterface))
        {
            LOG(INFO) << BOLDBLUE << " STATUS : Data Player is running and will be stopped " << RESET;
            cDPInterfacer.Stop(cInterface);
        }

        // Configure and Start DataPlayer
        // to send phase alignment pattern
        uint8_t cPhaseAlignmentPattern = 0x55;
        cDPInterfacer.Configure(cInterface, cPhaseAlignmentPattern);
        cDPInterfacer.Start(cInterface);
        if(cDPInterfacer.IsRunning(cInterface)) { LOG(INFO) << BOLDBLUE << "FE data player " << BOLDGREEN << " running correctly!" << RESET; }
        else
            LOG(INFO) << BOLDRED << "Could not start FE data player" << RESET;

        // align CIC inputs
        CicFEAlignment cCicAligner;
        cCicAligner.Inherit(&cHybridTester);
        cCicAligner.PhaseAlignmentMPA(100);
        cDPInterfacer.Stop(cInterface);
        cDPInterfacer.CheckNPatterns(cInterface);

        // // still needs to be de-bugged!!
        // // does not work yet
        // // Configure and Start DataPlayer
        // // to send word alignment pattern
        // uint8_t cWordAlignmentPattern = 0x75;
        // cDPInterfacer.ConfigureEmulator(cInterface, cWordAlignmentPattern);
        // cDPInterfacer.StartEmulator(cInterface);
        // if( cDPInterfacer.EmulatorIsRunning(cInterface) )
        // {
        //     LOG (INFO) << BOLDBLUE << "FE data player " << BOLDGREEN << " running correctly!" << RESET;
        // }
        // else
        //     LOG (INFO) << BOLDRED << "Could not start FE data player" << RESET;

        // cCicAligner.WordAlignmentMPA(100);
        // reset all chip and board registers
        // to what they were before this tool was called
        // cCicAligner.dumpConfigFiles();
    }
    if(cmd.foundOption("checkAsync"))
    {
        DataChecker cDataChecker;
        cDataChecker.Inherit(&cHybridTester);
        cDataChecker.AsyncTest();
        // cDataChecker.resetPointers();
    }

    // // equalize thresholds on readout chips
    if(cmd.foundOption("tuneOffsets"))
    {
        t.start();
        // now create a PedestalEqualization object
        PedestalEqualization cPedestalEqualization;
        cPedestalEqualization.Inherit(&cHybridTester);
        // second parameter disables stub logic on CBC3
        cPedestalEqualization.Initialise(true, true);
        cPedestalEqualization.FindVplus();
        cPedestalEqualization.FindOffsets();
        cPedestalEqualization.writeObjects();
        cPedestalEqualization.dumpConfigFiles();
        cPedestalEqualization.resetPointers();
        t.show("Time to tune the front-ends on the system: ");
    }
    // measure noise on FE chips
    if(cmd.foundOption("measurePedeNoise"))
    {
        t.start();
        // if this is true, I need to create an object of type PedeNoise from the members of Calibration
        // tool provides an Inherit(Tool* pTool) for this purpose
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit(&cHybridTester);
        // second parameter disables stub logic on CBC3
        cPedeNoise.Initialise(true, true); // canvases etc. for fast calibration
        cPedeNoise.measureNoise();
        cPedeNoise.writeObjects();
        cPedeNoise.dumpConfigFiles();
        t.stop();
        t.show("Time to Scan Pedestals and Noise");
    }

    if(cmd.foundOption("findOpens"))
    {
        OpenFinder cOpenFinder;
        cOpenFinder.Inherit(&cHybridTester);
        cOpenFinder.FindOpensPS();
    }
    if(cmd.foundOption("findShorts"))
    {
        ShortFinder cShortFinder;
        cShortFinder.Inherit(&cHybridTester);
        cShortFinder.Initialise();
        cShortFinder.FindShorts();
    }
    // test MPA outputs
    if(cmd.foundOption("mpaTest"))
    {
        cHybridTester.SelectCIC(true);
        // Configure and Start DataPlayer
        for(uint8_t cAttempt = 0; cAttempt < 1; cAttempt++)
        {
            // Check if data player is running
            if(cDPInterfacer.IsRunning(cInterface))
            {
                LOG(INFO) << BOLDBLUE << " STATUS : Data Player is running and will be stopped " << RESET;
                cDPInterfacer.Stop(cInterface);
            }

            if(cAttempt == 0)
                LOG(INFO) << BOLDBLUE << "Attempt " << +cAttempt << RESET;
            else if(cAttempt == 1)
                LOG(INFO) << BOLDGREEN << "Attempt " << +cAttempt << RESET;
            else if(cAttempt == 2)
                LOG(INFO) << BOLDMAGENTA << "Attempt " << +cAttempt << RESET;
            else if(cAttempt == 3)
                LOG(INFO) << BOLDYELLOW << "Attempt " << +cAttempt << RESET;

            cDPInterfacer.Configure(cInterface, cPattern);
            cDPInterfacer.Start(cInterface);
            cHybridTester.MPATest(cPattern);
            cDPInterfacer.Stop(cInterface);
            cDPInterfacer.CheckNPatterns(cInterface);
        }
        cHybridTester.SelectCIC(false);
    }
    // ssa pair tests
    if(!cSSAPair.empty())
    {
        LOG(INFO) << BOLDRED << "SSAOutput POGO debug" << RESET;
        // configure SSA to output something on stub lines
        cHybridTester.SSATestStubOutput(cSSAPair);
        // still needs to be debugged
        // configure SSA to output something on L1 lines
        // cHybridTester.SSATestL1Output(cSSAPair);
        // put it back in normal readout mode
        // and make sure we're in normal readout mode
        // i.e. synchronous
        // auto cNevents  = 9;//cTool.findValueInSettings<double>("Nevents" ,10);
        // for(auto cBoard : *cHybridTester.fDetectorContainer)
        // {
        //     BeBoard* cBeBoard = static_cast<BeBoard*>( cBoard );
        //     for(auto cOpticalGroup : *cBoard)
        //     {
        //         for(auto cHybrid : *cOpticalGroup)
        //         {
        //             for (auto cReadoutChip : *cHybrid)
        //             {
        //                 if( cReadoutChip->getFrontEndType() != FrontEndType::SSA )
        //                     continue;
        //                 cHybridTester.fReadoutChipInterface->WriteChipReg(cReadoutChip, "Sync",1);
        //                 cHybridTester.fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern7/FIFOconfig",
        //                 0x3);
        //             }//chip
        //         }//hybrid
        //     }// hybrid
        //     // check if i can read anything
        //     for( uint32_t cThreshold=0; cThreshold < 20; cThreshold++)
        //     {
        //         cHybridTester.setSameDac("Threshold", cThreshold);
        //         LOG (INFO) << BOLDRED << "Threshold is " << +cThreshold << RESET;
        //         cHybridTester.ReadNEvents( cBeBoard , cNevents);
        //     }
        // }
    }

    cHybridTester.SaveResults();
    cHybridTester.WriteRootFile();
    cHybridTester.CloseResultFile();
    cHybridTester.Destroy();

    if(!batchMode) cApp.Run();
    return 0;
}
