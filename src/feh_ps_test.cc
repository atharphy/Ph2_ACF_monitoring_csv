#include <cstring>

#include "Utils/Utilities.h"
#include "Utils/Timer.h"
#include "PSHybridTester.h"
#include "PedestalEqualization.h"
#include "PedeNoise.h"
#include "OpenFinder.h"
#include "ShortFinder.h"
#include "DPInterface.h"
#include "tools/CicFEAlignment.h"
#include "tools/BackEndAlignment.h"
#include "tools/DataChecker.h"
#include "Utils/argvparser.h"

#ifdef __USE_ROOT__
    #include "TROOT.h"
    #include "TApplication.h"
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

int main ( int argc, char* argv[] )
{
    //configure the logger
    el::Configurations conf ("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers (conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription ( "CMS Ph2_ACF  Commissioning tool to perform the following procedures:\n-Timing / Latency scan\n-Threshold Scan\n-Stub Latency Scan" );
    // error codes
    cmd.addErrorCode ( 0, "Success" );
    cmd.addErrorCode ( 1, "Error" );
    // options
    cmd.setHelpOption ( "h", "help", "Print this help page" );

    cmd.defineOption ( "file", "Hw Description File . Default value: settings/Commission_2CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
    cmd.defineOptionAlternative ( "file", "f" );

    cmd.defineOption ( "tuneOffsets", "tune offsets on readout chips connected to CIC.");
    cmd.defineOptionAlternative ( "tuneOffsets", "t" );
    
    cmd.defineOption ( "measurePedeNoise", "measure pedestal and noise on readout chips connected to CIC.");
    cmd.defineOptionAlternative ( "measurePedeNoise", "m" );
   
    cmd.defineOption ( "findShorts", "look for shorts", ArgvParser::NoOptionAttribute );
    cmd.defineOption ( "findOpens", "perform latency scan with antenna on UIB",  ArgvParser::NoOptionAttribute );
    cmd.defineOption("mpaTest", "Check MPA input with Data Player Pattern [provide pattern]", ArgvParser::NoOptionAttribute /*| ArgvParser::OptionRequires*/);
    cmd.defineOption ( "ssapair", "Debug selected SSA pair. Possible options: 01, 12, 23, 34, 45, 56, 67", ArgvParser::OptionRequiresValue);

    cmd.defineOption ( "threshold", "Threshold value to set on chips for open and short finding",  ArgvParser::OptionRequiresValue );
    cmd.defineOption ( "hybridId", "Serial Number of front-end hybrid. Default value: xxxx", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
    
    cmd.defineOption("pattern", "Data Player Pattern", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    cmd.defineOptionAlternative("pattern", "p");

    cmd.defineOption ( "withCIC", "Perform CIC alignment steps", ArgvParser::NoOptionAttribute );
    cmd.defineOption ( "checkAsync", "Check async readout", ArgvParser::NoOptionAttribute );
    
    // general 
    cmd.defineOption ( "batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute );
    cmd.defineOptionAlternative ( "batch", "b" );

    // GUI support    
    cmd.defineOption ( "USB", "USB iProduct string to identify the test card when using the USB functionalities.", ArgvParser::OptionRequiresValue );
    cmd.defineOption ( "useGui", "Support for running the test from the gui for hybrids testing. The named pipe for communication needs to be passed as the last parameter. Default: false", ArgvParser::NoOptionAttribute );

    cmd.defineOption ( "output", "Output directory. Default: Results/" );

    int result = cmd.parse ( argc, argv );

    if ( result != ArgvParser::NoParserError )
    {
        LOG (INFO) << cmd.parseErrorDescription ( result );
        exit ( 1 );
    }

    // now query the parsing results
    std::string cHWFile = ( cmd.foundOption ( "file" ) ) ? cmd.optionValue ( "file" ) : "settings/Commissioning.xml";
    // bool cFindOpens = (cmd.foundOption ("findOpens") )? true : false;
    // bool cShortFinder = ( cmd.foundOption ( "findShorts" ) ) ? true : false;
    bool batchMode = ( cmd.foundOption ( "batch" ) ) ? true : false;
    std::string cDirectory = ( cmd.foundOption ( "output" ) ) ? cmd.optionValue ( "output" ) : "Results/";
    std::string cHybridId = ( cmd.foundOption ( "hybridId" ) ) ? cmd.optionValue ( "hybridId" ) : "xxxx";
    uint8_t cPattern = ( cmd.foundOption ( "mpaTest" ) ) ? convertAnyInt ( cmd.optionValue ( "mpaTest" ).c_str() ) : 0; 
    const std::string cSSAPair = ( cmd.foundOption ( "ssapair" ) )   ?   cmd.optionValue ( "ssapair" ) : "";
    cDirectory += Form("FEH_PS_%s",cHybridId.c_str());

    std::string cUsbId = ( cmd.foundOption ("USB") ) ? cmd.optionValue ( "USB" ) : ""; //Default option?
    bool cGui = ( cmd.foundOption ( "useGui" ) ) ;
    
    TApplication cApp ( "Root Application", &argc, argv );
    
    if ( batchMode ) 
        gROOT->SetBatch ( true );
    else 
        TQObject::Connect ( "TCanvas", "Closed()", "TApplication", &cApp, "Terminate()" );

    std::string cResultfile = "Hybrid";
    Timer t;

    if ( cGui ){
        //Initialize gui communication with named pipe
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
    cHybridTester.InitializeHw ( cHWFile, outp);
    cHybridTester.InitializeSettings ( cHWFile, outp );
    cHybridTester.CreateResultDirectory ( cDirectory );
    cHybridTester.InitResultFile ( cResultfile );    
    cHybridTester.bookSummaryTree();
    //set voltage  on PS FEH 
    if ( cGui ){
        gui::message("");
        gui::status("Setting voltage of the hybrid");
        gui::progress(0.5 / 10.0);        

        gui::data("ResultsDirectory", cHybridTester.getDirectoryName());
    }

    cHybridTester.SetHybridVoltage();
    //LOG (INFO) << BOLDBLUE << "PS FEH current consumption pre-configuration..." << RESET;
    //cHybridTester.CheckHybridCurrents();
    //check voltage on PS FEH 
    cHybridTester.CheckHybridVoltages();
    LOG (INFO) << outp.str();
    //select CIC readout 
    //cHybridTester.SelectCIC(true);

    if ( cGui ){
        gui::message("Voltage set");
        gui::status("Configuring hardware");
        gui::progress(1 / 10.0);        
    }

    cHybridTester.ConfigureHw ();
    //LOG (INFO) << BOLDBLUE << "PS FEH current consumption post-configuration..." << RESET;
    //cHybridTester.CheckHybridCurrents();

    if ( cGui ){
        gui::message("Hardware configured");
        gui::status("");
        gui::progress(1.5 / 10.0);        
    }


    // interface to data player 
    DPInterface cDPInterfacer;
    BeBoardFWInterface* cInterface = dynamic_cast<BeBoardFWInterface*>( cHybridTester.fBeBoardFWMap.find(0)->second );
    // need to do this if 
    // reading out CIC 
    // or testing MPA 
    if ( cmd.foundOption ( "withCIC" ) || cmd.foundOption ( "mpaTest" ) )
    {
        cHybridTester.SelectCIC(true);
        // align back-end 
        BackEndAlignment cBackEndAligner;
        cBackEndAligner.Inherit (&cHybridTester);
        cBackEndAligner.Start(0);
        //reset all chip and board registers 
        // to what they were before this tool was called 
        cBackEndAligner.Reset(); 
   
        //Check if data player is running
        if (cDPInterfacer.IsRunning(cInterface))
        {
            LOG (INFO) << BOLDBLUE << " STATUS : Data Player is running and will be stopped " << RESET;
            cDPInterfacer.Stop(cInterface);
        }

        //Configure and Start DataPlayer
        // to send phase alignment pattern 
        uint8_t cPhaseAlignmentPattern=0x55;
        cDPInterfacer.Configure(cInterface, cPhaseAlignmentPattern);
        cDPInterfacer.Start(cInterface);
        if( cDPInterfacer.IsRunning(cInterface) )
        {
            LOG (INFO) << BOLDBLUE << "FE data player " << BOLDGREEN << " running correctly!" << RESET;
        }
        else
            LOG (INFO) << BOLDRED << "Could not start FE data player" << RESET;

        // align CIC inputs 
        CicFEAlignment cCicAligner;
        cCicAligner.Inherit (&cHybridTester);
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
        //reset all chip and board registers 
        // to what they were before this tool was called 
        //cCicAligner.dumpConfigFiles();
    }
    if( cmd.foundOption ( "checkAsync" ) )
    {
        DataChecker cDataChecker;
        cDataChecker.Inherit (&cHybridTester);
        cDataChecker.AsyncTest();
        //cDataChecker.resetPointers();
    }
    
    // // equalize thresholds on readout chips
    if( cmd.foundOption ( "tuneOffsets" ) ) 
    { 
        if (cGui) {
            gui::status("Calibrating front-end chips"); 
            gui::message(""); 
            gui::progress(2 / 10.0);     
        }

        t.start();
        // now create a PedestalEqualization object
        PedestalEqualization cPedestalEqualization;
        cPedestalEqualization.Inherit (&cHybridTester);
        // second parameter disables stub logic on CBC3
        cPedestalEqualization.Initialise ( true, true );
        cPedestalEqualization.FindVplus();
        cPedestalEqualization.FindOffsets();
        cPedestalEqualization.writeObjects();
        cPedestalEqualization.dumpConfigFiles();
        cPedestalEqualization.resetPointers();
        t.show ( "Time to tune the front-ends on the system: " );
        if ( cGui ){
            gui::message("Front-end chips calibrated successfully.");
            gui::progress(3 / 10.0);        
        }

    }
    // measure noise on FE chips 
    if (cmd.foundOption( "measurePedeNoise"))
    {
        if ( cGui ){
            gui::status("Measuring noise on front-end chips"); 
            gui::message(""); 
            gui::progress(3.5 / 10.0);     
        }


        t.start();
        //if this is true, I need to create an object of type PedeNoise from the members of Calibration
        //tool provides an Inherit(Tool* pTool) for this purpose
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit (&cHybridTester);
        //second parameter disables stub logic on CBC3
        cPedeNoise.Initialise (true, true); // canvases etc. for fast calibration
        cPedeNoise.measureNoise();
        cPedeNoise.writeObjects();
        cPedeNoise.dumpConfigFiles();
        t.stop();
        t.show ( "Time to Scan Pedestals and Noise" );
        if ( cGui ){
            gui::message("Noise measured");
            gui::progress(4.5 / 10.0);        
        }

    }
    
    if( cmd.foundOption("findOpens"))
    {
        if ( cGui ){
            gui::status("Running open finding procedure...");  
            gui::message(""); 
            gui::progress(5 / 10.0);     
        }

        OpenFinder cOpenFinder;
        cOpenFinder.Inherit (&cHybridTester);
        cOpenFinder.FindOpensPS();

        if ( cGui ){
            gui::message("Open finding done"); 
            gui::progress(6 / 10.0);        
        }    

    }
    if( cmd.foundOption("findShorts"))
    {
        if ( cGui ){
            gui::status("Running short finding procedure..."); 
            gui::message(""); 
            gui::progress(6.5 / 10.0);     
        }  

        ShortFinder cShortFinder;
        cShortFinder.Inherit (&cHybridTester);
        cShortFinder.Initialise();
        cShortFinder.FindShorts();

        if ( cGui ){
            gui::message("Short finding done"); 
            gui::progress(7 / 10.0);        
        }    

    }
    // test MPA outputs 
    if( cmd.foundOption ( "mpaTest" ) )
    {

        if ( cGui ){
            gui::status("Starting MPA input test");  
            gui::message(""); 
            gui::progress(7.5 / 10.0);     
        }  


        cHybridTester.SelectCIC(true);
        //Configure and Start DataPlayer
        for( uint8_t cAttempt=0; cAttempt < 1; cAttempt++)
        {
            //Check if data player is running
            if (cDPInterfacer.IsRunning(cInterface))
            {
                LOG (INFO) << BOLDBLUE << " STATUS : Data Player is running and will be stopped " << RESET;
                cDPInterfacer.Stop(cInterface);
            }

            if( cAttempt == 0 )
                LOG (INFO) << BOLDBLUE << "Attempt " << +cAttempt << RESET;
            else if( cAttempt == 1 )
                LOG (INFO) << BOLDGREEN << "Attempt " << +cAttempt << RESET;
            else if( cAttempt == 2 )
                LOG (INFO) << BOLDMAGENTA << "Attempt " << +cAttempt << RESET;
            else if( cAttempt == 3 )
                LOG (INFO) << BOLDYELLOW << "Attempt " << +cAttempt << RESET;

            cDPInterfacer.Configure(cInterface, cPattern);
            cDPInterfacer.Start(cInterface);
            cHybridTester.MPATest(cPattern);
            cDPInterfacer.Stop(cInterface);
            cDPInterfacer.CheckNPatterns(cInterface);
        }
        cHybridTester.SelectCIC(false);    

        if ( cGui ){
            gui::message("MPA input test done."); 
            gui::progress(8 / 10.0);     
        }  

    }  
    // ssa pair tests 
    if ( !cSSAPair.empty() )
    {
       LOG(INFO) << BOLDRED << "SSAOutput POGO debug" << RESET;
        // configure SSA to output something on stub lines 
        cHybridTester.SSATestStubOutput(cSSAPair);
        // still needs to be debugged 
        // configure SSA to output something on L1 lines
        //cHybridTester.SSATestL1Output(cSSAPair);
        // put it back in normal readout mode 
        // and make sure we're in normal readout mode 
        // i.e. synchronous
        // auto cNevents  = 9;//cTool.findValueInSettings("Nevents" ,10);
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
        //                 cHybridTester.fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern7/FIFOconfig", 0x3);
        //             }//chip
        //         }//hybrid 
        //     }// module 
        //     // check if i can read anything     
        //     for( uint32_t cThreshold=0; cThreshold < 20; cThreshold++)
        //     {
        //         cHybridTester.setSameDac("Threshold", cThreshold);
        //         LOG (INFO) << BOLDRED << "Threshold is " << +cThreshold << RESET;
        //         cHybridTester.ReadNEvents( cBeBoard , cNevents);
        //     }
        // }
    }

    if ( cGui ){
        gui::status("Saving test results..."); 
        gui::progress(9 / 10.0);     
    }


    cHybridTester.SaveResults();
    cHybridTester.WriteRootFile();
    cHybridTester.CloseResultFile();
    cHybridTester.Destroy();

    if ( cGui ){
        gui::message("Results saved"); 
        gui::status("Test done"); 
        gui::progress(10.0 / 10.0);        
    }    


    if ( !batchMode ) cApp.Run();
    return 0;

}
