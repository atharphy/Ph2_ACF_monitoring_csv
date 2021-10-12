#include "BackEndAlignment.h"
#include "CicFEAlignment.h"
#include "ExtraChecks.h"
#include "LatencyScan.h"
#include "PedeNoise.h"
#include "PedestalEqualization.h"
#include "argvparser.h"
#include "tools/DataChecker.h"
#include <cstring>

#ifdef __USE_ROOT__
#include "TApplication.h"
#include "TROOT.h"
#endif

#include "../Utils/Timer.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

INITIALIZE_EASYLOGGINGPP

int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  routine to test basic CIC functionality.");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("fe", "Front-End to modify threshold of [-1 means all]", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);

    cmd.defineOption(
        "vcth", "Threshold in VCth units (hex (including 0x) or decimal) . Default values from HW description .XML file", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("vcth", "v");

    cmd.defineOption("tuneOffsets", "tune offsets on readout chips connected to CIC.");
    cmd.defineOptionAlternative("tuneOffsets", "t");

    cmd.defineOption("measurePedeNoise", "measure pedestal and noise on readout chips connected to CIC.");
    cmd.defineOptionAlternative("measurePedeNoise", "m");

    cmd.defineOption("allChan", "Do calibration using all channels? Default: false", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("allChan", "a");

    cmd.defineOption("file", "Hw Description File . Default value: settings/Calibration8CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("output", "Output Directory . Default value: Results", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    cmd.defineOption("serial", "Serial Number of mezzanine . Default value: xxxx", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("serial", "s");

    cmd.defineOption("triggerRate", "Trigger Rate [kHz]. Default value: 10", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOption("disableStubs", "Disable stub logic", ArgvParser::NoOptionAttribute);
    cmd.defineOption("testPulse", "Reconstruct TP of a given amplitude. Default value: 10", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);

    cmd.defineOption("evaluate", "Run some more detailed tests... ", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("evaluate", "e");

    cmd.defineOption("daq", "Store s-link data into daq file [taking [n] consecutive triggers]", ArgvParser::OptionRequiresValue);

    cmd.defineOption("externalTriggers", "Run with external triggers... [taking [n] consecutive triggers] ", ArgvParser::OptionRequiresValue);

    cmd.defineOption("occupancyCheck", "Checking occupancy.... ", ArgvParser::NoOptionAttribute);

    cmd.defineOption("dataTest", "Take 100 events and look at the data output", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("dataTest", "d");

    cmd.defineOption("latency", "Run latency scan using external triggers", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("latency", "l");

    cmd.defineOption("minimum", "minimum value for latency scan", ArgvParser::OptionRequiresValue);

    cmd.defineOption("range", "range in clock cycles for latency scan", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("range", "r");

    cmd.defineOption("cicAlignment", "Perform CIC alignment", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("cicAlignment", "c");

    cmd.defineOption("scopeL1Alignment", "Debug alignment of L1 data in back-end", ArgvParser::NoOptionAttribute);
    cmd.defineOption("scopeStubAlignment", "Debug alignment of Stub data in back-end", ArgvParser::NoOptionAttribute);

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile           = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Calibration8CBC.xml";
    std::string cSerial           = (cmd.foundOption("serial")) ? cmd.optionValue("serial") : "xxxx";
    std::string cDirectory        = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    bool        cDataTest         = (cmd.foundOption("dataTest"));
    bool        cTune             = (cmd.foundOption("tuneOffsets"));
    bool        cEvaluate         = (cmd.foundOption("evaluate"));
    bool        cMeasurePedeNoise = (cmd.foundOption("measurePedeNoise"));
    bool        cAllChannels      = (cmd.foundOption("allChan")) ? true : false;
    int         cTriggerRate      = (cmd.foundOption("triggerRate")) ? convertAnyInt(cmd.optionValue("triggerRate").c_str()) : 10;
    bool        cDisableStubLogic = (cmd.foundOption("disableStubs"));
    bool        cDAQ              = cmd.foundOption("daq");
    bool        cDoCicAlignment   = cmd.foundOption("cicAlignment");
    int         cSigma            = cmd.foundOption("evaluate") ? convertAnyInt(cmd.optionValue("evaluate").c_str()) : 3;
    bool        cCheckOccupancy   = cmd.foundOption("occupancyCheck");
    bool        cScopeL1A         = cmd.foundOption("scopeL1Alignment");
    bool        cScopeStubs       = cmd.foundOption("scopeStubAlignment");

    cDirectory += Form("Cic_%s", cSerial.c_str());
    bool         batchMode = (cmd.foundOption("batch")) ? true : false;
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
    t.start();
    cTool.ConfigureHw();
    t.stop();
    t.show("Time to configure the front-end objects: ");

    cTool.CreateResultDirectory(cDirectory);
    cTool.InitResultFile("CicResults");
    // cTool.StartHttpServer();

    t.start();
    // align front-end [i.e. between readout chips and CICs]
    CicFEAlignment cCicAligner;
    cCicAligner.Inherit(&cTool);
    cCicAligner.Initialise();
    if(cDoCicAlignment)
    {
        bool cPhaseAligned = cCicAligner.PhaseAlignment(50);
        if(!cPhaseAligned)
        {
            LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " phase alignment step on CIC input .. " << RESET;
            exit(0);
        }
        LOG(INFO) << BOLDGREEN << "SUCCESSFUL " << BOLDBLUE << " phase alignment on CIC inputs... " << RESET;
        bool cWordAligned = cCicAligner.WordAlignment(10);
        if(!cWordAligned)
        {
            LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << "word alignment step on CIC input .. " << RESET;
            exit(0);
        }
        LOG(INFO) << BOLDGREEN << "SUCCESSFUL " << BOLDBLUE << " word alignment on CIC inputs... " << RESET;

        // manual alignment
        // bool cBxAligned = cCicAligner.SetBx0Delay(8);

        // automatic alignment
        bool cBxAligned = cCicAligner.Bx0Alignment(0, 4, 1, 100);
        if(!cBxAligned)
        {
            LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " bx0 alignment step in CIC ... " << RESET;
            exit(0);
        }
        LOG(INFO) << BOLDGREEN << "SUCCESSFUL " << BOLDBLUE << " bx0 alignment step in CIC ... " << RESET;
    }

    // align back-end .. if this moves to firmware then we can get rid of this step
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Initialise();
    cBackEndAligner.SetL1Debug(cScopeL1A);
    cBackEndAligner.SetStubDebug(cScopeStubs);
    bool cAligned = cBackEndAligner.Align();
    cBackEndAligner.resetPointers();
    if(!cAligned)
    {
        LOG(ERROR) << BOLDRED << "Failed to align back-end" << RESET;
        exit(0);
    }
    t.stop();
    t.show("Time to tune the back-end on the system: ");
    // equalize thresholds on readout chips
    if(cTune)
    {
        PedestalEqualization cTuning;
        cTuning.Inherit(&cTool);
        cTuning.Initialise(cAllChannels, cDisableStubLogic);
        // make sure trigger rate is set
        t.start();
        cTuning.FindVplus();
        cTuning.FindOffsets();
        cTuning.writeObjects();
        cTuning.dumpConfigFiles();
        cTuning.resetPointers();
        t.stop();
        t.show("Time to tune the front-ends on the system: ");
    }
    // meaure pedestal and noise
    if(cMeasurePedeNoise)
    {
        LOG(INFO) << BOLDBLUE << "Going to measure pedestal and noise " << RESET;
        // make sure trigger rate is set
        // dynamic_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(0,
        // cTriggerRate);//was 10
        t.start();
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit(&cTool);
        cPedeNoise.Initialise(cAllChannels, cDisableStubLogic); // canvases etc. for fast calibration
        cPedeNoise.measureNoise();
        cPedeNoise.writeObjects();
        cPedeNoise.dumpConfigFiles();
        cPedeNoise.resetPointers();
        t.stop();
        t.show("Time to Scan Pedestals and Noise");
    }

    cCicAligner.dumpConfigFiles();
    cCicAligner.writeObjects();
    cCicAligner.resetPointers();

    // some extra stuff ...
    ExtraChecks cExtra;
    cExtra.Inherit(&cTool);
    cExtra.Initialise();
    if(cEvaluate)
    {
        LOG(INFO) << BOLDBLUE << "Measuring noise and setting thresholds to " << +cSigma << " noise units away from pedestal...." << RESET;
        cExtra.Evaluate(cSigma, cTriggerRate, cDisableStubLogic);
    }

    if(cDataTest)
    {
        // data check with noise injection

        t.start();
        for(auto cBoard: *cExtra.fDetectorContainer)
        {
            BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cFe: *cOpticalGroup)
                {
                    // matching
                    uint16_t cTh1 = (cFe->getId() % 2 == 0) ? 900 : 1;
                    uint16_t cTh2 = (cFe->getId() % 2 == 0) ? 1 : 900;
                    for(auto cChip: *cFe)
                    {
                        ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                        if(cChip->getId() % 2 == 0)
                            static_cast<CbcInterface*>(cExtra.fReadoutChipInterface)->WriteChipReg(theChip, "VCth", cTh1);
                        else
                            static_cast<CbcInterface*>(cExtra.fReadoutChipInterface)->WriteChipReg(theChip, "VCth", cTh2);
                    }
                }
            }
            cExtra.ReadNEvents(theBoard, 1);
            const std::vector<Event*>& cEvents = cExtra.GetEvents();
            uint32_t                   cN      = 0;
            for(auto& cEvent: cEvents)
            {
                LOG(INFO) << ">>> Event #" << cN++;
                outp.str("");
                outp << *cEvent;
                LOG(INFO) << outp.str();
            }
        }
    }

    // reconstruct TP
    // if( cTPamplitude > 0 )
    // {
    //     t.start();
    //     cExtra.ReconstructTP(cTPamplitude);
    //     t.stop();
    // }

    // if( cLatency )
    // {
    //     LatencyScan cLatencyScan;
    //     cLatencyScan.Inherit (&cTool);
    //     cLatencyScan.Initialize (cStartLatency, cLatencyRange); // fix this so its consistent with all other tools
    //     if( cExternal )
    //         cLatencyScan.ConfigureTrigger("TLU", cNconsecutiveTriggers );
    //     else
    //         cLatencyScan.ConfigureTrigger("TestPulse", cNconsecutiveTriggers );

    //     cLatencyScan.ScanLatency (cStartLatency, cLatencyRange);
    //     cLatencyScan.writeObjects( );
    //     cLatencyScan.resetPointers();
    // }
    // if( cExternal )
    // {
    //     if( !cLatency )
    //         cExtra.ExternalTriggers(cNconsecutiveTriggers);
    // }

    if(cCheckOccupancy) { cExtra.OccupancyCheck(cTriggerRate, cDisableStubLogic); }
    if(cDAQ)
    {
        // cExtra.ConsecutiveTriggers(cNconsecutiveTriggers);
    }
    cExtra.dumpConfigFiles();
    cExtra.writeObjects();
    cExtra.resetPointers();
    t.stop();

    t.stop();
    cTool.WriteRootFile();
    cTool.SaveResults();
    cTool.CloseResultFile();
    cTool.Destroy();

    if(!batchMode) cApp.Run();
    return 0;
}
