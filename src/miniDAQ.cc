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
#include "D19cDebugFWInterface.h"
#include "tools/BackEndAlignment.h"
#include "tools/CicFEAlignment.h"
#include "tools/DataChecker.h"
#include "tools/PSAlignment.h"
#include "tools/StubBackEndAlignment.h"

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
    cmd.defineOption("limitTriggers", "Only accept exactly the correct number of triggers", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkData", "Check data..", ArgvParser::NoOptionAttribute);
    cmd.defineOption("skipAlignment", "Skip the back-end alignment step ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("useReadNEvents", "Check ReadNEvents method... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("continuousReadout", "Readout triggers as they come : argument to provide is how often to poll the readout [in us]", ArgvParser::OptionRequiresValue);
    cmd.defineOption("scopeData", "Scope L1 data", ArgvParser::OptionRequiresValue);

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
    cOutputFile             = "Data/" + string_format("run_%04d.raw", cRunNumber);
    pEventsperVcth          = (cmd.foundOption("events")) ? convertAnyInt(cmd.optionValue("events").c_str()) : 10;
    size_t cScopingAttempts = (cmd.foundOption("scopeData")) ? convertAnyInt(cmd.optionValue("scopeData").c_str()) : 10;

    std::string  cDAQFileName;
    FileHandler* cDAQFileHandler = nullptr;
    bool         cDAQFile        = cmd.foundOption("daq");
    int          cReadoutPause   = (cmd.foundOption("continuousReadout")) ? convertAnyInt(cmd.optionValue("continuousReadout").c_str()) : 10;
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

    // Align lpGBT-CIC first
    CicFEAlignment cCicAligner;
    cCicAligner.Inherit(&cTool);
    cCicAligner.Initialise();
    cCicAligner.CicLpGbtAlignment();

    // align back-end
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Start(cRunNumber);
    cBackEndAligner.waitForRunToBeCompleted();

    // if CIC is enabled then align CIC first
    if(cmd.foundOption("alignCIC")) { cCicAligner.AlignInputs(); }
    cCicAligner.Reset();
    cCicAligner.dumpConfigFiles();

    // align ASICs on PS module
    PSAlignment cPSAlignment;
    cPSAlignment.Inherit(&cTool);
    cPSAlignment.Initialise();
    // map MPA outputs for PS module
    cPSAlignment.MapMPAOutputs();

    // time align stubs in back-end
    StubBackEndAlignment cStubBackEndAligner;
    cStubBackEndAligner.Inherit(&cTool);
    cStubBackEndAligner.Initialise();
    if(!cmd.foundOption("skipAlignment"))
    {
        cStubBackEndAligner.FindPackageDelay();
        cStubBackEndAligner.FindStubLatency();
    }
    cStubBackEndAligner.Reset();

    if(!cmd.foundOption("skipAlignment")) { cPSAlignment.Align(); }
    cPSAlignment.dumpConfigFiles();

    int cPSmoduleSSAth  = cTool.findValueInSettings<double>("PSmoduleSSAthreshold", 100);
    int cPSmoduleMPAth  = cTool.findValueInSettings<double>("PSmoduleMPAthreshold", 100);
    int cPSmoduleLat    = cTool.findValueInSettings<double>("PSmoduleTriggerLatency", 100);
    int cPSmoduleWindow = cTool.findValueInSettings<double>("PSmoduleStubWindow", 100);

    for(auto board: *cTool.fDetectorContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    if(chip->getFrontEndType() == FrontEndType::SSA || chip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        cTool.fReadoutChipInterface->WriteChipReg(chip, "ENFLAGS_ALL", 0x1);
                        cTool.fReadoutChipInterface->WriteChipReg(chip, "Threshold", cPSmoduleSSAth);
                        cTool.fReadoutChipInterface->WriteChipReg(chip, "TriggerLatency", cPSmoduleLat - 1);
                    }
                    if(chip->getFrontEndType() == FrontEndType::MPA || chip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        cTool.fReadoutChipInterface->WriteChipReg(chip, "ENFLAGS_ALL", 0x5F);
                        cTool.fReadoutChipInterface->WriteChipReg(chip, "Threshold", cPSmoduleMPAth);
                        cTool.fReadoutChipInterface->WriteChipReg(chip, "TriggerLatency", cPSmoduleLat);
                        cTool.fReadoutChipInterface->WriteChipReg(chip, "StubWindow", cPSmoduleWindow);
                    }
                }
            }
        }
    }
    // when I get here .. I want to update the common_stub_data_delay
    // then I am sure that I should see stubs as long as the
    // correct hit latency is set
    for(auto cBoard: *cTool.fDetectorContainer)
    {
        auto     cStubOffset     = cBoard->getStubOffset();
        uint16_t cTriggerLatency = 0;
        uint8_t  cReTimePix      = 0;
        for(auto cOpticalReadout: *cBoard)
        {
            if(cOpticalReadout->getIndex() > 0) break;
            for(auto cHybrid: *cOpticalReadout)
            {
                if(cHybrid->getIndex() > 0) break;
                for(auto cReadoutChip: *cHybrid)
                {
                    if(cReadoutChip->getFrontEndType() == FrontEndType::SSA || cReadoutChip->getFrontEndType() == FrontEndType::SSA2) continue;
                    if(cTriggerLatency != 0) continue;
                    cTriggerLatency = cTool.fReadoutChipInterface->ReadChipReg(cReadoutChip, "TriggerLatency");
                    if(cReadoutChip->getFrontEndType() == FrontEndType::MPA || cReadoutChip->getFrontEndType() == FrontEndType::MPA2)
                        cReTimePix = cTool.fReadoutChipInterface->ReadChipReg(cReadoutChip, "RetimePix");
                }
            }
        }
        uint32_t cStubDataDelay = cTriggerLatency - (cStubOffset + cReTimePix);
        LOG(INFO) << BOLDMAGENTA << "Trigger latency on FEs connected to BeBoard#" << +cBoard->getIndex() << " set to " << cTriggerLatency << RESET;
        LOG(INFO) << BOLDMAGENTA << "Stub latency on FEs connected to BeBoard#" << +cBoard->getIndex() << " will be set to " << cStubDataDelay << RESET;
        cTool.fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubDataDelay);
    }

    // check for TP
    for(auto cBoard: *cTool.fDetectorContainer)
    {
        uint16_t cTriggerSource = cTool.fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
        if(cTriggerSource == 6)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getIndex() > 2)
                        {
                            LOG(INFO) << BOLDMAGENTA << "Since I am use the TP .. want to make sure I see stubs from only one chip "
                                      << " by disabling injection on Chip#" << +cChip->getId() << RESET;
                            cTool.fReadoutChipInterface->enableInjection(cChip, false);
                        }
                    }
                }
            } //
        }     //
    }         //

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

    if(cmd.foundOption("scopeData"))
    {
        // for(auto cBoard: *cTool.fDetectorContainer)
        // {
        //     for(auto cOpticalGroup: *cBoard)
        //     {
        //         for(auto cHybrid: *cOpticalGroup)
        //         {
        //             auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        //             //cTool.fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6 }, true);
        //         } // hybrid
        //     }// optical group
        // }// board

        size_t cCorrectHeaders = 0;
        size_t cHeadersFound   = 0;
        size_t cComparedL1s    = 0;
        size_t cIncorrectL1s   = 0;
        for(size_t cAttempts = 0; cAttempts < cScopingAttempts; cAttempts++)
        {
            LOG(DEBUG) << BOLDBLUE << "Attempt#" << +cAttempts << RESET;
            auto                  cInterface      = static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface());
            D19cDebugFWInterface* cDebugInterface = cInterface->getDebugInterface();
            auto                  cBuffer         = cDebugInterface->L1ADebug(1, false);
            // search for L1 headers
            size_t                cSearch = 0;
            size_t                cPos    = 0;
            auto                  cFound  = cBuffer.find("111111111111111111111110", cPos);
            std::stringstream     cL1DataHeaders;
            size_t                cNcorrect = 0;
            size_t                cNfound   = 0;
            std::vector<uint16_t> cL1Ids(0);
            do {
                cSearch      = cBuffer.find("111111111111111111111110", cPos);
                auto cHeader = cBuffer.substr(cSearch - 8, 32);
                cNfound++;
                size_t cCount1s = std::count_if(cHeader.begin(), cHeader.end(), [](char c) { return c == '1'; });
                cNcorrect += (cCount1s == 27);
                auto cStatus = cBuffer.substr(cSearch - 8 + 32, 9);
                auto cL1Id   = cBuffer.substr(cSearch - 8 + 32 + 9, 9);
                cL1Ids.push_back(std::stoi(cL1Id, 0, 2));
                if(cCount1s == 27)
                    cL1DataHeaders << BOLDGREEN << cStatus << "-" << cL1Id << "[" << std::stoi(cL1Id, 0, 2) << "]:";
                else
                    cL1DataHeaders << BOLDRED << cStatus << "-" << cL1Id << "[" << std::stoi(cL1Id, 0, 2) << "]:";
                // LOG (INFO) << BOLDYELLOW << cHeader << " - " << cStatus << " - " << cL1Id << RESET;
                cPos   = cSearch - 8 + 32;
                cFound = cBuffer.find("111111111111111111111110", cPos);
            } while(cFound != std::string::npos);
            LOG(INFO) << BOLDBLUE << "\t " << cL1DataHeaders.str() << RESET;
            LOG(INFO) << BOLDBLUE << "\t\t Found " << cNcorrect << " correct headers out of " << cNfound << RESET;
            auto cIter = cL1Ids.begin() + 1;
            do {
                if(*(cIter - 1) != 511) // don't compare after 511
                {
                    uint16_t cXor = *(cIter - 1) ^ (*cIter);
                    LOG(DEBUG) << BOLDYELLOW << std::bitset<9>(cXor) << RESET;
                    cComparedL1s += 2;
                    cIncorrectL1s += (cXor >> 8);
                }
                cIter++;
            } while(cIter < cL1Ids.begin() + 2);
            cCorrectHeaders += cNcorrect;
            cHeadersFound += cNfound;
        }
        LOG(INFO) << BOLDBLUE << " Found " << (cHeadersFound - cCorrectHeaders) << " incorrect headers out of " << cHeadersFound << " headers compared - "
                  << (float)(cHeadersFound - cCorrectHeaders) / cHeadersFound << RESET;
        LOG(INFO) << BOLDBLUE << " Found " << cIncorrectL1s << " incorrect headers out of " << cComparedL1s << " L1Ids compared - " << (float)(cIncorrectL1s) / cComparedL1s << RESET;
    }

    if(!cmd.foundOption("scopeData"))
    {
        bool cLimitTriggers = cmd.foundOption("limitTriggers");
        if(cmd.foundOption("continuousReadout"))
        {
            for(auto cBoard: *cTool.fDetectorContainer)
            {
                BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);
                if(cLimitTriggers)
                    cTool.fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", pEventsperVcth);
                else
                    cTool.fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0);

                std::vector<uint32_t> cCompleteData(0);
                cTool.fBeBoardInterface->Start(cBeBoard);
                size_t   cCounter = 0;
                uint32_t cNevents = 0;
                bool     cBreak   = false;
                bool     cWait    = false;
                do {
                    std::this_thread::sleep_for(std::chrono::microseconds(cReadoutPause));
                    std::vector<uint32_t> cData(0);
                    cNevents += cTool.ReadData(cBeBoard, cData, cWait);
                    auto cTriggerCounter = cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
                    if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
                    LOG(INFO) << BOLDMAGENTA << "miniDAQ continuousReadout loop ... " << +cTriggerCounter << " triggers received and " << +cNevents << " events readout so far... " << RESET;
                    cCounter++;
                    cBreak = (cNevents >= pEventsperVcth);
                } while(!cBreak);
                cTool.fBeBoardInterface->Stop(cBeBoard);
                std::this_thread::sleep_for(std::chrono::milliseconds(cReadoutPause));
                std::vector<uint32_t> cData(0);
                cNevents += cTool.ReadData(cBeBoard, cData, false);
                if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
                cTool.DecodeData(cBeBoard, cCompleteData, cNevents, cTool.fBeBoardInterface->getBoardType(cBeBoard));
            }
        }
        else
        {
            for(auto cBoard: *cTool.fDetectorContainer)
            {
                BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);
                // make sure triggers have stopped
                // and that the readout has been reset
                dynamic_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->Stop();
                auto cStubLatency = cTool.fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
                LOG(INFO) << BOLDMAGENTA << "Common stub data delay set to " << cStubLatency << RESET;
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
                    if(cLimitTriggers)
                        cTool.fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", pEventsperVcth);
                    else
                        cTool.fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0);

                    // try to only readout once I know I have enough events
                    size_t cCounter = 0;
                    do {
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                        cBreak = (cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter") >= pEventsperVcth);
                        if(cCounter % 1000 == 0)
                            LOG(INFO) << BOLDMAGENTA << "\t\t.. " << cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter")
                                      << " ... triggers received... " << RESET;
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

                // // process collected events
                // bool                       cPostscale   = cmd.foundOption("postscale");
                // int                        cScaleFactor = cPostscale ? atoi(cmd.optionValue("postscale").c_str()) : 1;
                // const std::vector<Event*>& cPh2Events   = cTool.GetEvents();
                // uint32_t                   cNtriggers   = cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
                // LOG(INFO) << BOLDBLUE << "Read-back " << +cPh2Events.size() << " events from this board." << RESET;
                // LOG(INFO) << BOLDBLUE << "Number of triggers received is " << +cNtriggers << "." << RESET;
                // uint32_t               cEventCounter = 0;
                // std::vector<DQMEvent*> cDQMEvents;
                // uint32_t               cEventId, cTriggerId;
                // for(auto& cEvent: cPh2Events)
                // {
                //     // if(cEventCounter >= pEventsperVcth) continue;
                //     cEventId   = cEvent->GetEventCount();
                //     cTriggerId = cEvent->GetExternalTriggerId();
                //     // if we write a DAQ file or want to run the DQM, get the SLink format
                //     if(cDAQFile || cDQM)
                //     {
                //         SLinkEvent cSLev = cEvent->GetSLinkEvent(cBeBoard);
                //         if(cDAQFile)
                //         {
                //             auto data = cSLev.getData<uint32_t>();
                //             cDAQFileHandler->setData(data);
                //         }

                //         if(cDQM && cEventCounter == 0)
                //         {
                //             DQMEvent* cDQMEv = new DQMEvent(&cSLev);
                //             dqmH->bookHistograms(cDQMEv->trkPayload().feReadoutMapping());
                //         }
                //         if(cDQM && cEventCounter % cScaleFactor == 0) { cDQMEvents.emplace_back(new DQMEvent(&cSLev)); }
                //     }

                //     // if(cEventCounter % 1000 == 0)
                //     //{
                //     auto cL1Id = (static_cast<D19cCic2Event*>(cEvent))->L1Id(0, 0);
                //     LOG(INFO) << BOLDBLUE << "Event#" << +cEventId << " trigger Id " << +cTriggerId << " L1 Id is " << +cL1Id << RESET;
                //     for(auto cOpticalGroup: *cBoard)
                //     {
                //         for(auto cHybrid: *cOpticalGroup)
                //         {
                //             for(auto cChip: *cHybrid)
                //             {
                //                 auto cHits = cEvent->GetHits( cHybrid->getId() , cChip->getId() );
                //                 auto cStubVector = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                //                 if( cHits.size() > 0 )
                //                 {
                //                     LOG (INFO) << BOLDMAGENTA << "Chip#" << +cChip->getId() << " Hybrid#" << +cHybrid->getId() << " found " << +cHits.size() << " hits and " << +cStubVector.size()
                //                     << " stubs." << RESET;
                //                 }
                //             }
                //         } // hybrid
                //     }     // optical group
                //     // outp.str("");
                //     // outp << *cEvent;
                //     // LOG(INFO) << outp.str() << RESET;
                //     //}
                //     cEventCounter++;
                // }

                // // finished  processing the events from this acquisition
                // // thus now fill the histograms for the DQM
                // if(cDQM)
                // {
                //     dqmH->fillHistograms(cDQMEvents);
                //     cDQMEvents.clear();
                // }
                // cNtriggers = cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
                // // LOG(INFO) << BOLDGREEN << "Number of triggers received = " << cNtriggers << RESET;

                // LOG(INFO) << "Number of triggers received         = " << cNtriggers << RESET;
                // LOG(INFO) << "Number of events recorded           = " << cPh2Events.size() << RESET;
                // if(cPh2Events.size() != 0)
                // {
                //     LOG(INFO) << "Last event GetEventCount            = " << +cPh2Events.back()->GetEventCount() << RESET;
                //     LOG(INFO) << "Last event GetExternalTriggerId     = " << +cPh2Events.back()->GetExternalTriggerId() << RESET;
                //     LOG(INFO) << "Number or resyncs                   = " << numberOfResyncs << RESET;
                //     LOG(INFO) << "Number or resyncs + events recorded = " << numberOfResyncs + cPh2Events.size() << RESET;
                // }
            }
        }

        // process
        for(auto cBeBoard: *cTool.fDetectorContainer)
        {
            bool                       cPostscale   = cmd.foundOption("postscale");
            int                        cScaleFactor = cPostscale ? atoi(cmd.optionValue("postscale").c_str()) : 1;
            const std::vector<Event*>& cPh2Events   = cTool.GetEvents();
            uint32_t                   cNtriggers   = cTool.fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
            LOG(INFO) << BOLDBLUE << "Read-back " << +cPh2Events.size() << " events from this board." << RESET;
            LOG(INFO) << BOLDBLUE << "Number of triggers received is " << +cNtriggers << "." << RESET;
            uint32_t               cEventCounter = 0;
            std::vector<DQMEvent*> cDQMEvents;
            uint32_t               cEventId, cTriggerId;

            uint8_t cFirstHybridId = 0;
            uint8_t cFirstChipId   = 0;
            for(auto cOpticalGroup: *cBeBoard)
            {
                if(cOpticalGroup->getIndex() > 0) continue;
                for(auto cHybrid: *cOpticalGroup)
                {
                    if(cHybrid->getIndex() > 0) continue;
                    cFirstHybridId = cHybrid->getId();
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getIndex() > 0) continue;
                        cFirstChipId = cChip->getId();
                    }
                }
            }
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

                auto cL1Id = (static_cast<D19cCic2Event*>(cEvent))->L1Id(cFirstHybridId, cFirstChipId);
                LOG(INFO) << BOLDBLUE << "Event#" << +cEventId << " trigger Id " << +cTriggerId << " L1 Id is " << +cL1Id << RESET;
                // for(auto cOpticalGroup: *cBeBoard)
                // {
                //     for(auto cHybrid: *cOpticalGroup)
                //     {
                //         size_t cNclusters=0;
                //         for(auto cChip: *cHybrid)
                //         {
                //             //cNclusters += static_cast<D19cCic2Event*>(cEvent)->getClusters( cHybrid->getId(), cChip->getId() ).size();
                //             // if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                //             // auto cHits       = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                //             // auto cStubVector = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                //             // LOG(INFO) << BOLDBLUE << "\t\t.. found " << +cHits.size() << " and " << +cStubVector.size() << " stubs." << RESET;
                //             // for(auto cHit: cHits)
                //             // {
                //             //     uint16_t cRow    = (cChip->getFrontEndType() == FrontEndType::CBC3) ? cHit : ((cHit >> 8) & 0x7F);
                //             //     uint16_t cColumn = (cChip->getFrontEndType() == FrontEndType::CBC3) ? 0 : ((cHit >> 24) & 0x7);
                //             //     uint16_t cId     = (cChip->getFrontEndType() == FrontEndType::CBC3) ? 0 : (cHit & 0x7);
                //             //     cRow += cId;
                //             //     if(cColumn == 0) { LOG(INFO) << BOLDYELLOW << "\t\t\t Hit in Strip ASIC" << +cChip->getId() % 8 << " row " << +cRow << RESET; }
                //             //     else
                //             //     {
                //             //         cColumn = cColumn - 1;
                //             //         LOG(INFO) << BOLDCYAN << "\t\t\t.. Hit in Pixel ASIC" << +cChip->getId() % 8 << " row " << +cRow << " column " << +cColumn << RESET;
                //             //     }
                //             // } // hit vector
                //         // }
                //         //LOG (INFO) << BOLDBLUE << "\t\t.." << cNclusters << RESET;
                //     } // hybrid
                // }     // optical group

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
    }

    return 0;
}
