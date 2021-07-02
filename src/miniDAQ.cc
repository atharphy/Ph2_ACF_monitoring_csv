#include "TString.h"
#include <cstring>
#include <fstream>
#include <inttypes.h>
#include <sys/stat.h>

#include "pugixml.hpp"
#include <boost/filesystem.hpp>

#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/Hybrid.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/ChipInterface.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"
#include "tools/BackEndAlignment.h"
#include "tools/CicFEAlignment.h"
#include "tools/DataChecker.h"
#include "tools/PSAlignment.h"

#include "../System/SystemController.h"

#include "../DQMUtils/DQMEvent.h"
#include "../DQMUtils/SLinkDQMHistogrammer.h"
#include "../RootUtils/publisher.h"
#include "TROOT.h"
#include <atomic>
#include <thread>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

std::atomic<bool> keepRunning(false);

void sendResync(Tool& theTool, uint32_t numberOfTriggersAfterResync, uint32_t triggerFrequency, uint32_t& numberOfResyncs)
{
    uint32_t microSecondSleepTime = float(numberOfTriggersAfterResync) / float(triggerFrequency) * 1000000;
    LOG(INFO) << BOLDGREEN << "Sleeping for " << microSecondSleepTime << " us before sending a resync" << RESET;
    while(!keepRunning)
    { /* waiting to start*/
    }
    // wait 1/2 trigger period, not sure how much helps
    // std::this_thread::sleep_for(std::chrono::microseconds(uint32_t(float(1000000/2)/float(triggerFrequency))));

    while(keepRunning)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(microSecondSleepTime));
        theTool.fBeBoardInterface->getFirmwareInterface()->ChipReSync();
        ++numberOfResyncs;
    }
}

int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    uint32_t pEventsperVcth;

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  Data acquisition test and Data dump");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hw Description File . Default value: settings/HWDescription_2CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("events", "Number of Events . Default value: 10", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("events", "e");

    cmd.defineOption("dqm", "Create DQM histograms");
    cmd.defineOptionAlternative("dqm", "q");

    cmd.defineOption("postscale", "Print only every i-th event (only send every i-th event to DQM Histogramer)", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("postscale", "p");

    cmd.defineOption("raw", "Save the data into a .raw file using the Ph2ACF format  ", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("raw", "r");

    cmd.defineOption("daq", "Save the data into a .daq file using the phase-2 Tracker data format.  ", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("daq", "d");

    cmd.defineOption("output", "Output Directory for DQM plots & page. Default value: Results", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("alignCIC", "Perform CIC alignment steps", ArgvParser::NoOptionAttribute);
    cmd.defineOption("alignPS", "Perform SSA-MPA alignment steps", ArgvParser::NoOptionAttribute);
    cmd.defineOption("useReadNEvents", "Check ReadNEvents method... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("limitTriggers", "Only accept exactly the correct number of triggers", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkData", "Check data..", ArgvParser::NoOptionAttribute);
    cmd.defineOption("skipAlignment", "Skip the back-end alignment step ", ArgvParser::NoOptionAttribute);

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // bool cSaveToFile = false;
    std::string cOutputFile;
    // now query the parsing results
    std::string cHWFile = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/HWDescription_2CBC.xml";

    const char* cDirectory = "Data";
    mkdir(cDirectory, 777);
    int cRunNumber = 0;
    getRunNumber("${PH2ACF_BASE_DIR}", cRunNumber);
    cOutputFile    = "Data/" + string_format("run_%04d.raw", cRunNumber);
    pEventsperVcth = (cmd.foundOption("events")) ? convertAnyInt(cmd.optionValue("events").c_str()) : 10;

    std::string  cDAQFileName;
    FileHandler* cDAQFileHandler = nullptr;
    bool         cDAQFile        = cmd.foundOption("daq");

    if(cDAQFile)
    {
        cDAQFileName    = cmd.optionValue("daq");
        cDAQFileHandler = new FileHandler(cDAQFileName, 'w');
        LOG(INFO) << "Writing DAQ File to:   " << cDAQFileName << " - ConditionData, if present, parsed from " << cHWFile;
    }

    bool                                  cDQM = cmd.foundOption("dqm");
    std::unique_ptr<SLinkDQMHistogrammer> dqmH = nullptr;

    if(cDQM) dqmH = std::unique_ptr<SLinkDQMHistogrammer>(new SLinkDQMHistogrammer(0));

    std::stringstream outp;
    Tool              cTool;
    if(cmd.foundOption("raw"))
    {
        std::string cRawFile = cmd.optionValue("raw");
        cTool.addFileHandler(cRawFile, 'w');
        LOG(INFO) << BOLDBLUE << "Writing Binary Rawdata to:   " << cRawFile;
    }
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    outp.str("");
    cTool.ConfigureHw();

    cTool.addFileHandler(cOutputFile, 'w');

    if(cmd.foundOption("alignPS"))
    {
        // align ASICs on PS module
        PSAlignment cPSAlignment;
        cPSAlignment.Inherit(&cTool);
        cPSAlignment.Initialise();
        // map MPA outputs for PS module
        cPSAlignment.MapMPAOutputs();
    }

    // if CIC is enabled then align CIC first
    if(cmd.foundOption("alignCIC"))
    {
        CicFEAlignment cCicAligner;
        cCicAligner.Inherit(&cTool);
        cCicAligner.Start(0);
        cCicAligner.waitForRunToBeCompleted();
        // reset all chip and board registers
        // to what they were before this tool was called
        cCicAligner.Reset();
        // cCicAligner.dumpConfigFiles();
    }

    // align back-end
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    if(!cmd.foundOption("skipAlignment"))
    {
        cBackEndAligner.Start(0);
        cBackEndAligner.waitForRunToBeCompleted();
        cBackEndAligner.Reset();
        // when I get here .. I want to update the common_stub_data_delay 
        // then I am sure that I should see stubs as long as the 
        // correct hit latency is set 
        for(auto cBoard: *cBackEndAligner.fDetectorContainer)
        {
            auto cStubOffset    = cBoard->getStubOffset();
            uint16_t cTriggerLatency=0; 
            uint8_t  cReTimePix=0;
            for(auto cOpticalReadout: *cBoard)
            {
                if(cOpticalReadout->getIndex() > 0) break;
                for(auto cHybrid: *cOpticalReadout)
                {
                    if(cHybrid->getIndex() > 0) break;
                    for(auto cReadoutChip: *cHybrid)
                    {
                        if( cReadoutChip->getFrontEndType() == FrontEndType::SSA ) continue;
                        if( cTriggerLatency != 0 ) continue; 
                        cTriggerLatency = cBackEndAligner.fReadoutChipInterface->ReadChipReg(cReadoutChip, "TriggerLatency");
                        if( cReadoutChip->getFrontEndType() == FrontEndType::MPA) cReTimePix = cBackEndAligner.fReadoutChipInterface->ReadChipReg(cReadoutChip, "RetimePix");
                    }
                }
            }
            uint32_t cStubDataDelay = cTriggerLatency - (cStubOffset + cReTimePix);
            LOG (INFO) << BOLDMAGENTA << "Trigger latency on FEs connected to BeBoard#" << +cBoard->getIndex() << " set to " << cTriggerLatency << RESET;
            LOG (INFO) << BOLDMAGENTA << "Stub latency on FEs connected to BeBoard#" << +cBoard->getIndex() << " will be set to " << cStubDataDelay << RESET;
            cBackEndAligner.fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubDataDelay);
        }
    }


    for(auto board: *cTool.fDetectorContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    if(chip->getFrontEndType() == FrontEndType::SSA)
                    {
                        static_cast<PSInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(chip, "ENFLAGS_ALL", 0x1);
                        static_cast<PSInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(chip, "Threshold", 100);
                    }
                    if(chip->getFrontEndType() == FrontEndType::MPA)
                    {
                        static_cast<PSInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(chip, "ENFLAGS_ALL", 0x7);
                        static_cast<PSInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(chip, "Threshold", 90);
                    }
                }
            }
        }
    }

    // // check for TP
    // for(auto cBoard: *cTool.fDetectorContainer)
    // {
    //     uint16_t cTriggerSource = cTool.fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    //     if(cTriggerSource == 6)
    //     {
    //         for(auto cOpticalGroup: *cBoard)
    //         {
    //             for(auto cHybrid: *cOpticalGroup)
    //             {
    //                 for(auto cChip: *cHybrid)
    //                 {
    //                     if(cChip->getIndex() > 0)
    //                     {
    //                         LOG(INFO) << BOLDMAGENTA << "Since I am use the TP .. want to make sure I see stubs from only one chip "
    //                                   << " by disabling injection on Chip#" << +cChip->getId() << RESET;
    //                         cTool.fReadoutChipInterface->enableInjection(cChip, false);

    //                     }
    //                 }
    //             }
    //         } //
    //     }     //
    // }         //

    uint32_t numberOfResyncs = 0;
    if(cmd.foundOption("sendResync"))
    {
        uint32_t    numberOfTriggersAfterResync = convertAnyInt(cmd.optionValue("sendResync").c_str());
        uint32_t    triggerFrequency            = 1000 * cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_cnfg.fast_command_block.user_trigger_frequency");
        std::thread theResyncThread(sendResync, std::ref(cTool), numberOfTriggersAfterResync, triggerFrequency, std::ref(numberOfResyncs));
        theResyncThread.detach();
    }

    if(cmd.foundOption("checkData"))
    {
        cTool.CreateResultDirectory("Results/DataChecker");
        cTool.InitResultFile("DataLog");
        DataChecker cDataChecker;
        cDataChecker.Inherit(&cTool);
        cDataChecker.InjectionTestPS(pEventsperVcth);
        // for(auto cBoard: *cTool.fDetectorContainer)
        // {
        //     cDataChecker.ReadDataTestPS( cBoard, pEventsperVcth);
        // }
        cDataChecker.dumpConfigFiles();
        cDataChecker.writeObjects();
        cDataChecker.SaveResults();
        cDataChecker.WriteRootFile();
        cDataChecker.CloseResultFile();
        return 0;
    }

    for(auto cBoard: *cTool.fDetectorContainer)
    {
        BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);
        // make sure triggers have stopped
        // and that the readout has been reset
        dynamic_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->Stop();
        auto cStubLatency = cTool.fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
        LOG (INFO) << BOLDMAGENTA << "Common stub data delay set to " << cStubLatency << RESET;
        bool cLimitTriggers = cmd.foundOption("limitTriggers");
        if(cLimitTriggers)
            cTool.fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", pEventsperVcth);
        else
            cTool.fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0);

        // if readNevents is used
        if(cmd.foundOption("useReadNEvents"))
        {
            // collect events
            std::vector<Event*> cPh2Events;
            cTool.ReadNEvents(cBeBoard, pEventsperVcth);
        }
        // default is to use ReadData
        else
        {
            LOG(INFO) << BOLDBLUE << "MiniDAQ running using ReadData" << RESET;
            uint32_t              cNevents = 0;
            std::vector<uint32_t> cCompleteData(0);
            cTool.fBeBoardInterface->Start(cBeBoard);
            bool cBreak = false;
            // if( cLimitTriggers )
            // {
            // try to only readout once I know I have enough events
            size_t cCounter=0;
            do {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                cBreak = (cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter") >= pEventsperVcth);
                if( cCounter%100 == 0 ) LOG (INFO) << BOLDMAGENTA << "\t\t.. " 
                    << cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter") 
                    << " ... triggers received... "
                    << RESET;
                cCounter++;
            } while(!cBreak);
            cTool.fBeBoardInterface->Stop(cBeBoard);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::vector<uint32_t> cData(0);
            cNevents += cTool.ReadData(cBeBoard, cData, false);
            if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));

            LOG(INFO) << BOLDBLUE << "Data size after stop is " << cCompleteData.size() << " total number of events I expect is " << +cNevents << RESET;

            // until number of events have stopped increasing
            for(size_t cAttempt = 0; cAttempt < 1; cAttempt++)
            {
                if(cLimitTriggers) continue;

                size_t cCurrentDataSize = 0;
                size_t cDataSize        = cCompleteData.size();
                do {
                    cCurrentDataSize = cCompleteData.size();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    std::vector<uint32_t> cData(0);
                    cNevents += cTool.ReadData(cBeBoard, cData, false);
                    if(cData.size() == 0) continue;
                    std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
                    cDataSize = cCompleteData.size();
                } while(cCurrentDataSize != cDataSize);
            }
            LOG(INFO) << BOLDBLUE << "Data size before starting to decode is " << cCompleteData.size() << " total number of events I expect is " << +cNevents << RESET;
            // decoding data
            cTool.DecodeData(cBeBoard, cCompleteData, cNevents, cTool.fBeBoardInterface->getBoardType(cBeBoard));
        }

        // process collected events
        bool                       cPostscale   = cmd.foundOption("postscale");
        int                        cScaleFactor = cPostscale ? atoi(cmd.optionValue("postscale").c_str()) : 1;
        const std::vector<Event*>& cPh2Events   = cTool.GetEvents();
        uint32_t                   cNtriggers   = cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        LOG(INFO) << BOLDBLUE << "Read-back " << +cPh2Events.size() << " events from this board." << RESET;
        LOG(INFO) << BOLDBLUE << "Number of triggers received is " << +cNtriggers << "." << RESET;
        uint32_t               cEventCounter = 0;
        std::vector<DQMEvent*> cDQMEvents;
        uint32_t               cEventId, cTriggerId;
        for(auto& cEvent: cPh2Events)
        {
            // if(cEventCounter >= pEventsperVcth) continue;
            cEventId   = cEvent->GetEventCount();
            cTriggerId = cEvent->GetExternalTriggerId();
            // if we write a DAQ file or want to run the DQM, get the SLink format
            if(cDAQFile || cDQM)
            {
                SLinkEvent cSLev = cEvent->GetSLinkEvent(cBeBoard);
                if(cDAQFile)
                {
                    auto data = cSLev.getData<uint32_t>();
                    cDAQFileHandler->setData(data);
                }

                if(cDQM && cEventCounter == 0)
                {
                    DQMEvent* cDQMEv = new DQMEvent(&cSLev);
                    dqmH->bookHistograms(cDQMEv->trkPayload().feReadoutMapping());
                }
                if(cDQM && cEventCounter % cScaleFactor == 0) { cDQMEvents.emplace_back(new DQMEvent(&cSLev)); }
            }

            // if(cEventCounter % 1000 == 0)
            //{
            auto cL1Id = (static_cast<D19cCic2Event*>(cEvent))->L1Id(0, 0);
            LOG(INFO) << BOLDBLUE << "Event#" << +cEventId << " trigger Id " << +cTriggerId << " L1 Id is " << +cL1Id << RESET;
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto cHits = cEvent->GetHits( cHybrid->getId() , cChip->getId() );
                        auto cStubVector = cEvent->StubVector(cHybrid->getId(), cChip->getId()); 
                        if( cHits.size() > 0 )
                        {
                            LOG (INFO) << BOLDMAGENTA << "Chip#" << +cChip->getId() << " Hybrid#" << +cHybrid->getId() << " found " << +cHits.size() << " hits and " << +cStubVector.size() << " stubs." << RESET;
                        }
                    }
                } // hybrid
            }     // optical group
            // outp.str("");
            // outp << *cEvent;
            // LOG(INFO) << outp.str() << RESET;
            //}
            cEventCounter++;
        }

        // finished  processing the events from this acquisition
        // thus now fill the histograms for the DQM
        if(cDQM)
        {
            dqmH->fillHistograms(cDQMEvents);
            cDQMEvents.clear();
        }
        cNtriggers = cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        // LOG(INFO) << BOLDGREEN << "Number of triggers received = " << cNtriggers << RESET;

        LOG(INFO) << "Number of triggers received         = " << cNtriggers << RESET;
        LOG(INFO) << "Number of events recorded           = " << cPh2Events.size() << RESET;
        if(cPh2Events.size() != 0)
        {
            LOG(INFO) << "Last event GetEventCount            = " << +cPh2Events.back()->GetEventCount() << RESET;
            LOG(INFO) << "Last event GetExternalTriggerId     = " << +cPh2Events.back()->GetExternalTriggerId() << RESET;
            LOG(INFO) << "Number or resyncs                   = " << numberOfResyncs << RESET;
            LOG(INFO) << "Number or resyncs + events recorded = " << numberOfResyncs + cPh2Events.size() << RESET;
        }
    }

    // done with the acquistion, now clean up
    if(cDAQFile)
        // this closes the DAQ file
        delete cDAQFileHandler;

    if(cDQM)
    {
        // save and publish
        // Create the DQM plots and generate the root file
        // first of all, strip the folder name
        std::vector<std::string> tokens;

        tokenize(cOutputFile, tokens, "/");
        std::string fname = tokens.back();

        // now form the output Root filename
        tokens.clear();
        tokenize(fname, tokens, ".");
        std::string runLabel    = tokens[0];
        std::string dqmFilename = runLabel + "_dqm.root";
        dqmH->saveHistograms(dqmFilename, runLabel + "_flat.root");

        // find the folder (i.e DQM page) where the histograms will be published
        std::string cDirBasePath;

        if(cmd.foundOption("output"))
        {
            cDirBasePath = cmd.optionValue("output");
            cDirBasePath += "/";
        }
        else
            cDirBasePath = "Results/";

        // now read back the Root file and publish the histograms on the DQM page
        RootWeb::makeDQMmonitor(dqmFilename, cDirBasePath, runLabel);
        LOG(INFO) << "Saving root file to " << dqmFilename << " and webpage to " << cDirBasePath;
    }

    return 0;
}
