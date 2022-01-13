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

#ifdef __TCUSB__
#include "USB_a.h"
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
    cmd.defineOption("mpaTest", "Check MPA input with Data Player Pattern", ArgvParser::NoOptionAttribute /*| ArgvParser::OptionRequires*/);
    cmd.defineOption("ssapair", "Debug selected SSA pair. Possible options: 01, 12, 23, 34, 45, 56, 67", ArgvParser::OptionRequiresValue);

    cmd.defineOption("hybridId", "Serial Number of front-end hybrid. Default value: xxxx", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);

    cmd.defineOption("pattern", "Data Player Pattern", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    cmd.defineOptionAlternative("pattern", "p");

    cmd.defineOption("withCIC", "Perform CIC alignment steps", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkAsync", "Check async readout", ArgvParser::NoOptionAttribute);

    cmd.defineOption("checkI2C", "Check I2C read of a non-zero register. Infinite loop", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkCountersRead", "Check I2C read of a non-zero register. Infinite loop", ArgvParser::NoOptionAttribute);

    // general
    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    // GUI support
    cmd.defineOption("USB", "USB iProduct string to identify the test card when using the USB functionalities.", ArgvParser::OptionRequiresValue);
    cmd.defineOption("USBBus", "USB device bus number", ArgvParser::OptionRequiresValue);
    cmd.defineOption("USBDev", "USB device device number", ArgvParser::OptionRequiresValue);
    cmd.defineOption("useGui",
                     "Support for running the test from the gui for hybrids testing. The named pipe for communication needs to be passed as the last parameter. Default: false",
                     ArgvParser::NoOptionAttribute);

    cmd.defineOption("output", "Output directory. Default: Results/", ArgvParser::OptionRequiresValue);

    cmd.defineOption("antennaValue", "Antenna value for test.", ArgvParser::OptionRequiresValue);

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
    bool        batchMode  = (cmd.foundOption("batch")) ? true : false;
    std::string cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    std::string cHybridId  = (cmd.foundOption("hybridId")) ? cmd.optionValue("hybridId") : "xxxx";
    // uint8_t cPattern = ( cmd.foundOption ( "mpaTest" ) ) ? convertAnyInt ( cmd.optionValue ( "mpaTest" ).c_str() ) : 0;
    const std::string cSSAPair = (cmd.foundOption("ssapair")) ? cmd.optionValue("ssapair") : "";
    cDirectory += Form("FEH_PS_%s", cHybridId.c_str());

    std::string cUsbId  = (cmd.foundOption("USB")) ? cmd.optionValue("USB") : "";                             // Default option?
    uint32_t    cUsbBus = (cmd.foundOption("USBBus")) ? (uint32_t)(std::stoi(cmd.optionValue("USBBus"))) : 0; // Default option?
    uint8_t     cUsbDev = (cmd.foundOption("USBDev")) ? (uint32_t)(std::stoi(cmd.optionValue("USBDev"))) : 0; // Default option?
    bool        cGui    = (cmd.foundOption("useGui"));

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::string cResultfile = "Hybrid";
    Timer       t;

    if(cGui)
    {
        // Initialize gui communication with named pipe
        gui::init(argv[argc - 1]);

        gui::status("Initializing test");
        gui::progress(0 / 10.0);
    }

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
    if(cGui)
    {
        gui::message("");
        gui::status("Setting voltage of the hybrid");
        gui::progress(0.5 / 10.0);

        gui::data("ResultsDirectory", cHybridTester.getDirectoryName());
    }

    if(cmd.foundOption("USBBus") && cmd.foundOption("USBDev"))
    {
#ifdef __TCUSB__
        TC_PSFE cTC_PSFE(cUsbBus, cUsbDev);
#endif
    }

    cHybridTester.SetHybridVoltage(cUsbBus, cUsbDev);
    // LOG (INFO) << BOLDBLUE << "PS FEH current consumption pre-configuration..." << RESET;
    // cHybridTester.CheckHybridCurrents();
    // check voltage on PS FEH
    // cHybridTester.CheckHybridVoltages();
    if (cSSAPair.empty() ) { cHybridTester.RunHybridETest(); } 
    LOG(INFO) << outp.str() << RESET;
    // select CIC readout
    // cHybridTester.SelectCIC(true);

    if(cGui)
    {
        gui::message("Voltage set");
        gui::status("Configuring hardware");
        gui::progress(1 / 10.0);
    }

    cHybridTester.ConfigureHw();
    // LOG (INFO) << BOLDBLUE << "PS FEH current consumption post-configuration..." << RESET;
    // cHybridTester.CheckHybridCurrents();
    if(cmd.foundOption("checkI2C")) cHybridTester.CheckI2C();

    // cHybridTester.ReadSSABias("MonitorGround");
    // cHybridTester.ReadSSABias("MonitorVoltageBias");
    // cHybridTester.ReadSSABias("MonitorCurrentBias");

    if (cSSAPair.empty() ) { cHybridTester.CalibrateSSABias(); } 

    if(cGui)
    {
        gui::message("Hardware configured");
        gui::status("");
        gui::progress(1.5 / 10.0);
    }

    // interface to data player
    DPInterface         cDPInterfacer;
    BeBoardFWInterface* cInterface = dynamic_cast<BeBoardFWInterface*>(cHybridTester.fBeBoardFWMap.find(0)->second);

    // // ***************** for dummy hybrid test ***********************
    //     cHybridTester.SelectCIC(true);
    //     //Configure and Start DataPlayer
    //     uint8_t cDataPlayerPattern=0xAA;
    //     cDPInterfacer.Configure(cInterface, cDataPlayerPattern);
    //     cDPInterfacer.Start(cInterface);
    //     if( cDPInterfacer.IsRunning(cInterface) )
    //     {
    //         LOG (INFO) << BOLDBLUE << "FE data player " << BOLDGREEN << " running correctly!" << RESET;
    //     }
    //     else
    //         LOG (INFO) << BOLDRED << "Could not start FE data player" << RESET;

    //     cDPInterfacer.Stop(cInterface);
    //     cDPInterfacer.CheckNPatterns(cInterface);
    // // ************************end dummy test************************************

    // need to do this if
    // reading out CIC
    // or testing MPA
    if(cmd.foundOption("withCIC") || cmd.foundOption("mpaTest"))
    {
        cHybridTester.SelectCIC(true);

        bool    cAligned = false;
        double  cAlignedDouble;
        uint8_t cPhaseAlignmentPattern = 0xAA;
        for(int i = 0; i < 3; i++)
        {
            // Check if data player is running
            if(cDPInterfacer.IsRunning(cInterface))
            {
                LOG(INFO) << BOLDBLUE << " STATUS : Data Player is running and will be stopped " << RESET;
                cDPInterfacer.Stop(cInterface);
            }

            // Configure and Start DataPlayer
            // to send phase alignment pattern
            cDPInterfacer.Configure(cInterface, cPhaseAlignmentPattern);
            cDPInterfacer.Start(cInterface);
            // cDPInterfacer.StartSyncPlaying(cInterface);
            if(cDPInterfacer.IsRunning(cInterface)) { LOG(INFO) << BOLDBLUE << "FE data player " << BOLDGREEN << " running correctly!" << RESET; }
            else
                LOG(INFO) << BOLDRED << "Could not start FE data player" << RESET;

            // align CIC inputs
            CicFEAlignment cCicAligner;
            cCicAligner.Inherit(&cHybridTester);

            LOG(INFO) << "Phase alignment MPA" << RESET;
            cAligned       = cCicAligner.PhaseAlignmentMPA(100);
            cAlignedDouble = cAligned ? 1.0 : 0.0;
#ifdef __USE_ROOT__
            cHybridTester.fillSummaryTree(Form("MPA Alignment attemp %d", i + 1), cAlignedDouble);
#endif
            // for (uint8_t value = 0; value < 16; value ++ ) {
            //     cAligned = cCicAligner.ManualPhaseAlignment(value);
            //     if (cAligned)
            //         LOG(INFO) << "Phase: " << +value << RESET;
            //     else
            //         LOG(INFO) << BOLDRED << "Phase: " << +value << ". Not set correctly" << RESET;

            //     cHybridTester.SweepPhaseAlignment(value);
            // }
            if(cAligned) break;
        }

        // and then re-align back-end just because
        cHybridTester.AlignCICout(cPhaseAlignmentPattern);

        cDPInterfacer.Stop(cInterface);
        cDPInterfacer.CheckNPatterns(cInterface);
    }

    if(cmd.foundOption("checkAsync"))
    {
        DataChecker cDataChecker;
        cDataChecker.Inherit(&cHybridTester);
        cDataChecker.AsyncTest();
        // cDataChecker.resetPointers();
    }

    OpenFinder cOpenFinder;
    cOpenFinder.Inherit(&cHybridTester);
    std::string antennaValue = (cmd.foundOption("antennaValue")) ? cmd.optionValue("antennaValue") : "512";
    cOpenFinder.SelectAntennaPosition("Disable", 512);
    // cOpenFinder.SelectAntennaPosition("Enable", 550 );
    if(cmd.foundOption("antennaValue"))
    {
        cOpenFinder.SelectAntennaPosition("Enable", std::stoi(antennaValue));
        LOG(INFO) << "Setting antenna" << RESET;
    }

    // measure noise on FE chips before calibration
    if(cmd.foundOption("measurePedeNoise") && cmd.foundOption("antennaValue"))
    {
        if(cGui)
        {
            gui::status("Measuring noise on front-end chips before calibration");
            gui::message("");
            gui::progress(3.5 / 10.0);
        }
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
        if(cGui) { gui::message("Noise measured"); }

        // cOpenFinder.SelectAntennaPosition("Disable", 512);
    }
    // cHybridTester.SetTrim("GAINTRIMMING",7);
    // // equalize thresholds on readout chips
    if(cmd.foundOption("tuneOffsets"))
    {
        if(cGui)
        {
            gui::status("Calibrating front-end chips");
            gui::message("");
            gui::progress(2 / 10.0);
        }

        t.start();
        // now create a PedestalEqualization object
        PedestalEqualization cPedestalEqualization;
        cPedestalEqualization.Inherit(&cHybridTester);
        cPedestalEqualization.Initialise(true, true);
        cPedestalEqualization.FindVplus();

        cHybridTester.ReadSSABias("CalLevel");

        cPedestalEqualization.FindOffsets();
        cPedestalEqualization.writeObjects();
        cPedestalEqualization.dumpConfigFiles();
        cPedestalEqualization.resetPointers();
        t.show("Time to tune the front-ends on the system: ");
        if(cGui)
        {
            gui::message("Front-end chips calibrated successfully.");
            gui::progress(3 / 10.0);
        }
    }

    // cHybridTester.CalibrateGainTrim();

    // measure noise on FE chips
    if(cmd.foundOption("measurePedeNoise"))
    {
        if(cGui)
        {
            gui::status("Measuring noise on front-end chips");
            gui::message("");
            gui::progress(3.5 / 10.0);
        }
        t.start();
        // if this is true, I need to create an object of type PedeNoise from the members of Calibration
        // tool provides an Inherit(Tool* pTool) for this purpose
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit(&cHybridTester);
        cPedeNoise.Initialise(true, true); // canvases etc. for fast calibration
        cPedeNoise.measureNoise();
        cPedeNoise.writeObjects();
        cPedeNoise.dumpConfigFiles();
        t.stop();
        t.show("Time to Scan Pedestals and Noise");
        if(cGui) { gui::message("Noise measured"); }
    }

    if(cmd.foundOption("checkCountersRead")) { cHybridTester.CheckCounters(); }
    if(cmd.foundOption("findShorts"))
    {
        if(cGui)
        {
            gui::status("Running short finding procedure...");
            gui::message("");
            gui::progress(5 / 10.0);
        }
        ShortFinder cShortFinder;
        cShortFinder.Inherit(&cHybridTester);
        cShortFinder.Initialise();
        cShortFinder.FindShorts();

        if(cGui)
        {
            gui::message("Short finding done");
            gui::progress(5.5 / 10.0);
        }
    }
    if(cmd.foundOption("findOpens"))
    {
        if(cGui)
        {
            gui::status("Running open finding procedure...");
            gui::message("");
            gui::progress(6 / 10.0);
        }

        OpenFinder cOpenFinder;
        cOpenFinder.Inherit(&cHybridTester);
        cOpenFinder.FindOpensPS();

        if(cGui)
        {
            gui::message("Open finding done");
            gui::progress(7 / 10.0);
        }
    }
    // test MPA outputs
    // test MPA outputs
    if(cmd.foundOption("mpaTest"))
    {
        if(cGui)
        {
            gui::status("Starting CIC input lines test");
            gui::message("");
            gui::progress(7.5 / 10.0);
        }

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

            // Is this needed?
            if(cAttempt == 0)
                LOG(INFO) << BOLDBLUE << "Attempt " << +cAttempt << RESET;
            else if(cAttempt == 1)
                LOG(INFO) << BOLDGREEN << "Attempt " << +cAttempt << RESET;
            else if(cAttempt == 2)
                LOG(INFO) << BOLDMAGENTA << "Attempt " << +cAttempt << RESET;
            else if(cAttempt == 3)
                LOG(INFO) << BOLDYELLOW << "Attempt " << +cAttempt << RESET;

            cHybridTester.MPATest(); // The pattern is not being set by the function anymore
            cDPInterfacer.Stop(cInterface);
        }

        if(cGui)
        {
            gui::message("MPA input test done.");
            gui::progress(8 / 10.0);
        }
    }
    // ssa pair tests
    if(!cSSAPair.empty())
    {
        if(cGui)
        {
            gui::status("Testing SSA stubs...");
            gui::progress(5 / 10.0);
        }

        BackEndAlignment cBackendAlignment;
        cBackendAlignment.Inherit(&cHybridTester);

        LOG(INFO) << BOLDRED << "SSAOutput POGO debug" << RESET;
        // configure SSA to output something on stub lines
        if(!cSSAPair.empty())
        {

            BackEndAlignment cBackendAlignment;
            cBackendAlignment.Inherit(&cHybridTester);

            LOG(INFO) << BOLDRED << "SSAOutput POGO debug" << RESET;
            // configure SSA to output something on stub lines
            if(cSSAPair != "ALL") {
                cHybridTester.SSAPairSelect(cSSAPair);
                for(auto cBoard : *cHybridTester.fDetectorContainer)
                {
                    cBackendAlignment.PSAlignment(cBoard, cSSAPair);
                }
                cHybridTester.SSATestStubOutput(cSSAPair); 
                cHybridTester.SSATestL1Output(cSSAPair);
                cHybridTester.SSATestLateralCommunication(cSSAPair);
            }
            else
            {
                std::string cCurrentSSAPair;
                for(int i = 0; i < 7; i += 2)
                {
                    cCurrentSSAPair = std::to_string(i) + std::to_string(i + 1);
                    cHybridTester.SSAPairSelect(cCurrentSSAPair);
                    for(auto cBoard : *cHybridTester.fDetectorContainer)
                    {
                        cBackendAlignment.PSAlignment(cBoard, cCurrentSSAPair);
                    }
                    cHybridTester.SSATestStubOutput(cCurrentSSAPair);
                    cHybridTester.SSATestL1Output(cCurrentSSAPair);
                }
                
                for(int i = 0; i < 7; i ++)
                {
                    cCurrentSSAPair = std::to_string(i) + std::to_string(i + 1);
                    cHybridTester.SSAPairSelect(cCurrentSSAPair);
                    for(auto cBoard : *cHybridTester.fDetectorContainer)
                    {
                        cBackendAlignment.PSAlignment(cBoard, cCurrentSSAPair);
                    }
                    cHybridTester.SSATestLateralCommunication(cCurrentSSAPair);
                }
            }
        }
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

    if(cGui)
    {
        gui::status("Saving test results...");
        gui::progress(9 / 10.0);
    }

    cHybridTester.SaveResults();
    cHybridTester.WriteRootFile();
    cHybridTester.CloseResultFile();
    cHybridTester.Destroy();

    if(cGui)
    {
        gui::message("Results saved");
        gui::status("Test done");
        gui::progress(10.0 / 10.0);
    }

    if(!batchMode) cApp.Run();
    return 0;
}
