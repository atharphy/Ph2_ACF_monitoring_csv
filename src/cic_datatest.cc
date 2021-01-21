#include "../Utils/Timer.h"
#include "../Utils/argvparser.h"
#include "../tools/CicFEAlignment.h"
#include "../tools/PedeNoise.h"
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
    int         cVcth      = (cmd.foundOption("vcth")) ? convertAnyInt(cmd.optionValue("vcth").c_str()) : 0;
    int         cFeChip    = (cmd.foundOption("fe")) ? convertAnyInt(cmd.optionValue("fe").c_str()) : -1;

    cDirectory += "CIC";
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
    cTool.ConfigureHw();
    cTool.CreateResultDirectory(cDirectory);
    cTool.InitResultFile("CicResults");
    cTool.StartHttpServer();
    t.start();

    // CIC FE alignment tool
    CicFEAlignment cCicAligner;
    cCicAligner.Inherit(&cTool);
    cCicAligner.Initialise();
    BeBoard* pBoard = static_cast<BeBoard*>(cCicAligner.fDetectorContainer->at(0));

    bool cPhaseAligned = cCicAligner.PhaseAlignment();
    if(cPhaseAligned)
    {
        bool cWordAligned = cCicAligner.WordAlignment();
        if(cWordAligned)
        {
            t.stop();
            t.show("Time to prepare the CIC for data taking ");
            LOG(INFO) << BOLDGREEN << "Now trying to take data with the CIC [unsparsified 2S for the moment..]" << RESET;
            // cCicAligner.TestI2C();
            // cTool.fBeBoardInterface->setBoard ( pBoard->getBeBoardId() );
            if(dynamic_cast<D19cFWInterface*>(cCicAligner.fBeBoardInterface->getFirmwareInterface())->Bx0Alignment())
            {
                // init threshold visitior
                LOG(INFO) << BOLDBLUE << "Setting Vcth to " << +cVcth << " units." << RESET;
                ThresholdVisitor cThresholdVisitor(cCicAligner.fReadoutChipInterface, cVcth);
                if(cFeChip < 0)
                    cCicAligner.accept(cThresholdVisitor);
                else
                {
                    for(auto cOpticalGroup: *pBoard)
                    {
                        for(auto cFe: *cOpticalGroup)
                        {
                            for(auto cChip: *cFe)
                            {
                                if(cChip->getId() == cFeChip)
                                {
                                    cThresholdVisitor.setThreshold(cVcth);
                                    static_cast<ReadoutChip*>(cChip)->accept(cThresholdVisitor);
                                }
                            }
                        }
                    }
                }
                dynamic_cast<D19cFWInterface*>(cCicAligner.fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(0, 75);
                // read n events from the board
                cCicAligner.ReadNEvents(pBoard, 10);
                const std::vector<Event*>& events = cCicAligner.GetEvents(pBoard);
                LOG(INFO) << BOLDBLUE << "Read back " << +events.size() << " events from FC7." << RESET;
                LOG(INFO) << BOLDBLUE << "Vcth = " << +cVcth << " DAC units." << RESET;
                int               cNevents = 0;
                std::stringstream outp;
                for(auto& ev: events)
                {
                    LOG(INFO) << ">>> Event #" << cNevents;
                    outp.str("");
                    outp << *ev;
                    LOG(INFO) << outp.str();
                    cNevents++;
                }
            }
        }
    }

    t.stop();
    cTool.WriteRootFile();
    cTool.SaveResults();
    cTool.CloseResultFile();
    cTool.Destroy();

    if(!batchMode) cApp.Run();
    return 0;
}
