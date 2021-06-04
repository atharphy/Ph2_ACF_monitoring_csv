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

#include "../System/SystemController.h"

#include "../DQMUtils/DQMEvent.h"
#include "../DQMUtils/SLinkDQMHistogrammer.h"
#include "../RootUtils/publisher.h"
#include "TROOT.h"

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

    cmd.defineOption("withCIC", "With CIC. Default : false", ArgvParser::NoOptionAttribute);
    cmd.defineOption("maskROCs", "List of ROCs to mask", ArgvParser::OptionRequiresValue);
    cmd.defineOption("maskChannels", "List of channels to mask", ArgvParser::OptionRequiresValue);
    cmd.defineOption("useReadNEvents", "Check ReadNEvents method... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("limitTriggers", "Only accept exactly the correct number of triggers", ArgvParser::NoOptionAttribute);
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

    bool cWithCIC = (cmd.foundOption("withCIC"));

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

    // bool cPostscale   = cmd.foundOption("postscale");
    // int  cScaleFactor = 1;

    // if(cPostscale) cScaleFactor = atoi(cmd.optionValue("postscale").c_str());

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

    // if CIC is enabled then align CIC first
    if(cWithCIC)
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
    cBackEndAligner.Initialise();
    cBackEndAligner.Align();
    // reset all chip and board registers
    // to what they were before this tool was called
    cBackEndAligner.resetPointers();

    // list of ROCs to mask
    if(cmd.foundOption("maskROCs"))
    {
        std::string          cArgsStr = cmd.optionValue("maskROCs");
        std::vector<uint8_t> cROCsToMask;
        std::stringstream    cArgsSS(cArgsStr);
        int                  i;
        while(cArgsSS >> i)
        {
            cROCsToMask.push_back(i);
            if(cArgsSS.peek() == ',') cArgsSS.ignore();
        };

        // and if we're also going to mask channels on the active ROCs
        std::vector<uint8_t> cChannelsToMask;
        if(cmd.foundOption("maskChannels"))
        {
            std::string       cArgsStr = cmd.optionValue("maskChannels");
            std::stringstream cArgsSS(cArgsStr);
            int               i;
            while(cArgsSS >> i)
            {
                cChannelsToMask.push_back(i);
                if(cArgsSS.peek() == ',') cArgsSS.ignore();
            };
        }

        for(auto cBoard: *cTool.fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        // mask ROCs in list
                        if(std::find(cROCsToMask.begin(), cROCsToMask.end(), cChip->getId()) != cROCsToMask.end())
                            cTool.fReadoutChipInterface->MaskAllChannels(cChip, true);
                        else
                        {
                            ChannelGroup<254, 1> cChannelMask;
                            cChannelMask.enableAllChannels();
                            for(auto cChannelToMask: cChannelsToMask) cChannelMask.disableChannel(cChannelToMask);
                            cTool.fReadoutChipInterface->maskChannelsGroup(cChip, &cChannelMask);
                        }
                    } // chip
                }     // hybrid
            }         // optical group
        }
    }

    // make event counter start at 1 as does the L1A counter
    // uint32_t cN      = 1;
    // uint32_t cNthAcq = 0;
    // uint32_t count   = 0;
    // cTool.fBeBoardInterface->Start(pBoard);
    // while(cN <= pEventsperVcth)
    // {
    //     uint32_t cPacketSize = cTool.ReadData(pBoard);

    //     if(cN + cPacketSize >= pEventsperVcth) cTool.fBeBoardInterface->Stop(pBoard);

    //     const std::vector<Event*>& events = cTool.GetEvents();
    //     std::vector<DQMEvent*>     cDQMEvents;

    //     for(auto& ev: events)
    //     {
    //         // if we write a DAQ file or want to run the DQM, get the SLink format
    //         if(cDAQFile || cDQM)
    //         {
    //             SLinkEvent cSLev = ev->GetSLinkEvent(pBoard);

    //             if(cDAQFile)
    //             {
    //                 auto data = cSLev.getData<uint32_t>();
    //                 cDAQFileHandler->setData(data);
    //             }

    //             // if DQM histos are enabled and we are treating the first event, book the histograms
    //             if(cDQM && cN == 1)
    //             {
    //                 DQMEvent* cDQMEv = new DQMEvent(&cSLev);
    //                 dqmH->bookHistograms(cDQMEv->trkPayload().feReadoutMapping());
    //             }

    //             if(cDQM)
    //             {
    //                 if(count % cScaleFactor == 0) cDQMEvents.emplace_back(new DQMEvent(&cSLev));
    //             }
    //         }

    //         if(cPostscale)
    //         {
    //             if(count % cScaleFactor == 0)
    //             {
    //                 LOG(INFO) << ">>> Event #" << count;
    //                 outp.str("");
    //                 outp << *ev << std::endl;
    //                 LOG(INFO) << outp.str();
    //             }
    //         }

    //         if(count % 100 == 0) LOG(INFO) << ">>> Recorded Event #" << count;

    //         // increment event counter
    //         count++;
    //         cN++;
    //     }

    //     // finished  processing the events from this acquisition
    //     // thus now fill the histograms for the DQM
    //     if(cDQM)
    //     {
    //         dqmH->fillHistograms(cDQMEvents);
    //         cDQMEvents.clear();
    //     }

    //     cNthAcq++;
    // }

    for(auto cBoard: *cTool.fDetectorContainer)
    {
        BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);

        // make sure triggers have stopped
        // and that the readout has been reset
        dynamic_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->Stop();
        //
        // cTool.fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.readout_block.timeout", 0x1);
        //
        bool cLimitTriggers = cmd.foundOption("limitTriggers");
        if(cLimitTriggers) cTool.fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", pEventsperVcth);
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
            uint32_t              cNevents = 0;
            std::vector<uint32_t> cCompleteData(0);
            cTool.fBeBoardInterface->Start(cBeBoard);
            bool cBreak = false;
            // if( cLimitTriggers )
            // {
            // try to only readout once I know I have enough events
            do
            {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                cBreak = (cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter") >= pEventsperVcth);
            } while(!cBreak);
            cTool.fBeBoardInterface->Stop(cBeBoard);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::vector<uint32_t> cData(0);
            cNevents += cTool.ReadData(cBeBoard, cData, false);
            if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));

            // size_t   cIter    = 0;
            // do
            // {
            //     std::this_thread::sleep_for(std::chrono::milliseconds(1));
            //     std::vector<uint32_t> cData(0);
            //     cNevents += cTool.ReadData(cBeBoard, cData, false);
            //     if( cData.size() == 0 )
            //     {
            //         if( cIter%100 == 0 )
            //             LOG (INFO) << BOLDBLUE << "No events read-back from board .. waiting for more .." << RESET;
            //     }
            //     else
            //     {
            //         LOG (INFO) << BOLDBLUE << "\t... Read back.." << +cData.size() << " words." << RESET;
            //         std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
            //     }
            //     cBreak = (cNevents >= pEventsperVcth);
            //     if( cLimitTriggers )
            //         cBreak = cBreak || ( cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter") >= pEventsperVcth);
            //     cIter++;
            // } while(!cBreak && cIter < 1000 );
            // LOG(INFO) << BOLDBLUE << "Stopping triggers..." << RESET;
            // cTool.fBeBoardInterface->Stop(cBeBoard);
            // std::this_thread::sleep_for(std::chrono::milliseconds(1));
            LOG(INFO) << BOLDBLUE << "Data size after stop is " << cCompleteData.size() << " total number of events I expect is " << +cNevents << RESET;

            // until number of events have stopped increasing
            for(size_t cAttempt = 0; cAttempt < 1; cAttempt++)
            {
                if(cLimitTriggers) continue;

                size_t cCurrentDataSize = 0;
                size_t cDataSize        = cCompleteData.size();
                do
                {
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
