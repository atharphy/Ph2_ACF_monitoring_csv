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
#include "tools/PSAlignment.h"

#include "../System/SystemController.h"

#include "../DQMUtils/DQMEvent.h"
#include "../DQMUtils/SLinkDQMHistogrammer.h"
#include "../RootUtils/publisher.h"
#include <atomic>
#include "TROOT.h"
#include <thread>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

std::atomic<bool> keepRunning(false);

void sendResync(Tool& theTool, uint32_t numberOfTriggersAfterResync, uint32_t triggerFrequency, uint32_t& numberOfResyncs)
{
    uint32_t microSecondSleepTime = float(numberOfTriggersAfterResync)/float(triggerFrequency) * 1000000;
    LOG(INFO) << BOLDGREEN << "Sleeping for " << microSecondSleepTime << " us before sending a resync" << RESET;
    while(!keepRunning) {/* waiting to start*/}
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
    cmd.defineOption("limitTriggers", "Limit accepted trigges to the number of events required", ArgvParser::NoOptionAttribute);
    cmd.defineOption("sendResync", "Send continuosly resyncs every N triggers", ArgvParser::OptionRequiresValue);

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
    if( cmd.foundOption("alignPS"))
    {
        // align ASICs on PS module
        PSAlignment cPSAlignment;
        cPSAlignment.Inherit(&cTool);
        cPSAlignment.Initialise();
        // map MPA outputs for PS module
        cPSAlignment.MapMPAOutputs();
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

    uint32_t numberOfResyncs=0;
    if(cmd.foundOption("sendResync"))
    {
        uint32_t numberOfTriggersAfterResync = convertAnyInt(cmd.optionValue("sendResync").c_str());
        uint32_t triggerFrequency = 1000 * cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_cnfg.fast_command_block.user_trigger_frequency");
        std::thread theResyncThread(sendResync, std::ref(cTool), numberOfTriggersAfterResync, triggerFrequency, std::ref(numberOfResyncs));
        theResyncThread.detach();
    }

    for(auto cBoard: *cTool.fDetectorContainer)
    {
        BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);

        // make sure triggers have stopped
        // and that the readout has been reset
        dynamic_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->Stop();

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
            if( cmd.foundOption("limitTriggers")) cTool.fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", pEventsperVcth);
            uint32_t cNevents = 0;
            size_t   cIter    = 0;

            std::vector<uint32_t> cCompleteData(0);
            cTool.fBeBoardInterface->Start(cBeBoard);
            keepRunning = true;

            // size_t cNoData=0;
            do
            {
                std::this_thread::sleep_for(std::chrono::microseconds(100000));
                std::vector<uint32_t> cData(0);
                cNevents += cTool.ReadData(cBeBoard, cData, false);
                // if( cData.size() == 0 )
                // { 
                //     if( cNoData%2500 == 0 )
                //         LOG (INFO) << BOLDMAGENTA << "\t...No events read-back from board .. waiting for more .." << RESET;
                //     cNoData++;
                //     cIter++;
                //     continue;
                // }
                if(cIter % 2500 == 0)
                    LOG(INFO) << BOLDBLUE << "Nevents is " << +cNevents << " number of words in vector is " << cData.size() << " size of complete data is " << cCompleteData.size() << RESET;
                std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
                cIter++;
                if(cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter") >= pEventsperVcth) break;
            } while(cNevents < pEventsperVcth);
            LOG(INFO) << BOLDBLUE << "Stopping triggers..." << RESET;
            cTool.fBeBoardInterface->Stop(cBeBoard);
            keepRunning = false;
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
            

            std::vector<uint32_t> cData(0);
            cNevents += cTool.ReadData(cBeBoard, cData, false);
            std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // LOG (INFO) << BOLDBLUE << "Number of words in vector is "  << cCompleteData.size() << RESET;
            // decoding data
            cTool.DecodeData(cBeBoard, cCompleteData, cNevents, cTool.fBeBoardInterface->getBoardType(cBeBoard));
        }

        // process collected events
        bool                       cPostscale   = cmd.foundOption("postscale");
        int                        cScaleFactor = cPostscale ? atoi(cmd.optionValue("postscale").c_str()) : 1;
        const std::vector<Event*>& cPh2Events   = cTool.GetEvents();
        LOG(INFO) << BOLDBLUE << "Need to process " << +cPh2Events.size() << " events from this board." << RESET;
        uint32_t               cEventCounter = 0;
        std::vector<DQMEvent*> cDQMEvents;

        // auto increment = [](uint16_t L1Counter) -> uint16_t
        // {
        //     L1Counter++;
        //     if(L1Counter>=512) L1Counter = 0;
        //     return L1Counter;
        // }

        // uint16_t lastL1WithTimeout = 0;
        // bool timeoutErrorInPreviousEvent = false;
        // bool isFirstUnrecoverableEvent   = true;
        // uint16_t previousL1ID = 0;
        for(auto& cEvent: cPh2Events)
        {
            if(cEventCounter >= pEventsperVcth) continue;

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

            //if(cEventCounter % (pEventsperVcth/10) == 0)
            //{
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        if( cBoard->getFrontEndType() == FrontEndType::CIC || cBoard->getFrontEndType() == FrontEndType::CIC2 )
                        {

                            uint16_t L1Status = static_cast<D19cCic2Event*>(cEvent)->L1Status(cHybrid->getId());
                            LOG(INFO) << BOLDBLUE << "Event#" << +cEvent->GetEventCount() << " trigger Id " 
                                << +cEvent->GetExternalTriggerId() 
                                << " Hybrid#" << +cHybrid->getId()
                                << " L1 Id is " << static_cast<D19cCic2Event*>(cEvent)->L1Id( cHybrid->getId(), 0 ) 
                                << " L1 status flag = " << std::bitset<9>(L1Status) 
                                << RESET;
                            for(auto cChip : *cHybrid)
                            {
                                if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
                                uint16_t numberOfPixelClusters = static_cast<D19cCic2Event*>(cEvent)->GetPixelClusters(cHybrid->getId(), cChip->getId()).size();
                                uint16_t numberOfStripClusters = static_cast<D19cCic2Event*>(cEvent)->GetStripClusters(cHybrid->getId(), cChip->getId()).size();
                                if(numberOfPixelClusters>0 || numberOfPixelClusters>0)
                                {
                                    LOG(INFO) << BOLDYELLOW << "Chip ID = " << +cChip->getId() << RESET;
                                    LOG(INFO) << BOLDYELLOW << "Number of pixel clusters = "<< numberOfPixelClusters << RESET;
                                    LOG(INFO) << BOLDYELLOW << "Number of strip clusters = "<< numberOfStripClusters << RESET;
                                } 
                                // LOG(INFO) << BOLDYELLOW << "Number of stubs          = "<<static_cast<D19cCic2Event*>(cEvent)->StubVector      (cHybrid->getId(), cChip->getId()).size() << RESET;
                            }

                            // if(previousL1ID != static_cast<D19cCic2Event*>(cEvent)->L1Id(cHybrid->getId(), 0) -1)
                            //     LOG(INFO) << BOLDRED << "L1Id reset after " << previousL1ID << RESET;
                            // previousL1ID = static_cast<D19cCic2Event*>(cEvent)->L1Id(cHybrid->getId(), 0);

                            // if( (L1Status & 0x1) == 1 && (L1Status & 0x1FE) != 0 ) 
                            // {
                            //     if(isFirstUnrecoverableEvent && timeoutErrorInPreviousEvent)
                            //     {
                            //         auto theLastCorrectEvent = cPh2Events.at(cEventCounter-2);

                            //         isFirstUnrecoverableEvent = false;
                            //         LOG(INFO) << BOLDYELLOW << "Event before unrecoverable occupancy" << RESET;
                            //         for(auto cChip : *cHybrid)
                            //         {
                            //             LOG(INFO) << BOLDYELLOW << "Chip ID = " << +cChip->getId() << RESET;
                            //             LOG(INFO) << BOLDYELLOW << "Number of pixel clusters = "<<static_cast<D19cCic2Event*>(theLastCorrectEvent)->GetPixelClusters(cHybrid->getId(), cChip->getId()).size() << RESET;
                            //             LOG(INFO) << BOLDYELLOW << "Number of strip clusters = "<<static_cast<D19cCic2Event*>(theLastCorrectEvent)->GetStripClusters(cHybrid->getId(), cChip->getId()).size() << RESET;
                            //             LOG(INFO) << BOLDYELLOW << "Number of stubs          = "<<static_cast<D19cCic2Event*>(theLastCorrectEvent)->StubVector      (cHybrid->getId(), cChip->getId()).size() << RESET;
                            //         }
                            //     }
                            //     timeoutErrorInPreviousEvent = true;
                            //     LOG(WARNING) << BOLDRED << "No packet from MPA to CIC, L1 status flag = " << std::bitset<9>(L1Status) << RESET;

                            //     auto cL1IdFirstROC = static_cast<D19cCic2Event*>(cEvent)->L1Id( cHybrid->getId(), 0 ); 
                            //     LOG(INFO) << BOLDBLUE << "Event#" << +cEvent->GetEventCount() << " trigger Id " 
                            //         << +cEvent->GetExternalTriggerId() 
                            //         << " Hybrid#" << +cHybrid->getId()
                            //         << " L1 Id is " << +cL1IdFirstROC 
                            //         << RESET;

                            // }
                            // else
                            // {
                            //     if(timeoutErrorInPreviousEvent) LOG(INFO) << BOLDGREEN << "RECOVERED" << RESET;
                            //     timeoutErrorInPreviousEvent = false;
                            // }
                        }
                    }// hybrid
                }// optical group
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
        uint32_t cNtriggers = cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        // LOG(INFO) << BOLDGREEN << "Number of triggers received = " << cNtriggers << RESET;

        LOG(INFO) << "Number of triggers received         = " << cNtriggers                                 << RESET;
        LOG(INFO) << "Number of events recorded           = " << cPh2Events.size()                          << RESET;
        LOG(INFO) << "Last event GetEventCount            = " << +cPh2Events.back()->GetEventCount()        << RESET;
        LOG(INFO) << "Last event GetExternalTriggerId     = " << +cPh2Events.back()->GetExternalTriggerId() << RESET;
        LOG(INFO) << "Number or resyncs                   = " << numberOfResyncs                            << RESET;
        LOG(INFO) << "Number or resyncs + events recorded = " << numberOfResyncs + cPh2Events.size()        << RESET;

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
    // BeBoard* pBoard = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));

    // // make event counter start at 1 as does the L1A counter
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

    // // done with the acquistion, now clean up
    // if(cDAQFile)
    //     // this closes the DAQ file
    //     delete cDAQFileHandler;

    // if(cDQM)
    // {
    //     // save and publish
    //     // Create the DQM plots and generate the root file
    //     // first of all, strip the folder name
    //     std::vector<std::string> tokens;

    //     tokenize(cOutputFile, tokens, "/");
    //     std::string fname = tokens.back();

    //     // now form the output Root filename
    //     tokens.clear();
    //     tokenize(fname, tokens, ".");
    //     std::string runLabel    = tokens[0];
    //     std::string dqmFilename = runLabel + "_dqm.root";
    //     dqmH->saveHistograms(dqmFilename, runLabel + "_flat.root");

    //     // find the folder (i.e DQM page) where the histograms will be published
    //     std::string cDirBasePath;

    //     if(cmd.foundOption("output"))
    //     {
    //         cDirBasePath = cmd.optionValue("output");
    //         cDirBasePath += "/";
    //     }
    //     else
    //         cDirBasePath = "Results/";

    //     // now read back the Root file and publish the histograms on the DQM page
    //     RootWeb::makeDQMmonitor(dqmFilename, cDirBasePath, runLabel);
    //     LOG(INFO) << "Saving root file to " << dqmFilename << " and webpage to " << cDirBasePath;
    // }

    return 0;
}
