#include "OTTool.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

#include "../HWInterface/FEConfigurationInterface.h"
#include "../HWInterface/L1ReadoutInterface.h"
#include "../HWInterface/TriggerInterface.h"
#include "../Utils/ContainerFactory.h"

OTTool::OTTool() : Tool()
{
    fBoardRegContainer.reset();
    fBrdRegsToPerserve.clear();
    fChipRegsToPerserve.reset();
    fSuccess = false;
    fMyName  = "OTTool";
}

OTTool::~OTTool() {}
// Reset register on BeBoard + Chips
void OTTool::Reset()
{
    if(fReadoutMode == 1) return;

    LOG(INFO) << BOLDGREEN << "Resetting registers touched  by " << fMyName << RESET;
    // set everything back to original values .. like I wasn't here
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << fMyName << ":Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap)
        {
            // skip registers that I should perserve for this board
            if(std::find(fBrdRegsToPerserve.begin(), fBrdRegsToPerserve.end(), cReg.first) != fBrdRegsToPerserve.end())
            {
                LOG(INFO) << BOLDBLUE << "Will not reconfigure " << cReg.first << RESET;
                continue;
            }
            cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second));
        }
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);
    } // for the board - reset registers

    for(auto cBoard: *fDetectorContainer) // now reset Chip registers
    {
        auto& cChipRegsToPreserveThisBrd = fChipRegsToPerserve.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cChipRegsToPreserveThisOG = cChipRegsToPreserveThisBrd->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                LOG(INFO) << BOLDYELLOW << fMyName << ":Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;
                auto& cChipRegsToPreserveThisHybrd = cChipRegsToPreserveThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cChipRegsToPreserveThisChip = cChipRegsToPreserveThisHybrd->at(cChip->getIndex());
                    auto& cRegsToPerserve             = cChipRegsToPreserveThisChip->getSummary<std::vector<std::string>>();
                    // reset registers
                    auto cModMap = cChip->GetModifiedRegisterMap();
                    LOG(DEBUG) << BOLDYELLOW << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    std::vector<std::pair<std::string, uint16_t>> cRegList;
                    for(auto cMapItem: cModMap)
                    {
                        // skip registers that I should perserve for this Chip
                        if(std::find(cRegsToPerserve.begin(), cRegsToPerserve.end(), cMapItem.first) != cRegsToPerserve.end())
                        {
                            LOG(DEBUG) << BOLDBLUE << "Skipping reconfiguration of " << cMapItem.first << RESET;
                            continue;
                        }

                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        LOG(DEBUG) << BOLDYELLOW << fMyName << "::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to "
                                   << cMapItem.second.fValue << RESET;

                        cRegList.push_back(std::make_pair(cMapItem.first, cMapItem.second.fValue));
                    }
                    fReadoutChipInterface->WriteChipMultReg(cChip, cRegList);

                    // then clear modified register map
                    // and also disable register tracking for this chip
                    cChip->ClearModifiedRegisterMap();
                    cChip->setRegisterTracking(0);
                    LOG(DEBUG) << BOLDYELLOW << fMyName << "::Reset Chip#" << +cChip->getId() << " register tracking set to " << +cChip->getRegisterTracking() << RESET;
                }
            }
        }
    } // Chip registers
    resetPointers();
}

// Initialization function
void OTTool::Prepare()
{
    // retreive number of events from settings file
    fNevents = findValueInSettings<double>("Nevents", 10);

    if(fReadoutMode == 1) return;
    // retreive original settings for all chips and all back-end boards
    fBoardRegContainer.reset();
    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        auto&                cBoardRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        const BeBoardRegMap& cOrigRegMap  = static_cast<const BeBoard*>(cBoard)->getBeBoardRegMap();
        cBoardRegMap.insert(cOrigRegMap.begin(), cOrigRegMap.end());
    }

    // check sparsification
    for(auto cBoard: *fDetectorContainer)
    {
        uint32_t cSparsified = cBoard->getSparsification(); // this is set in the file parser .. so check using that
        LOG(INFO) << BOLDYELLOW << +cSparsified << RESET;
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", cSparsified);
        // make sure I am in un-sparsified mode
        LOG(INFO) << BOLDGREEN << "Setting sparsification on BeBoard#" << +cBoard->getId() << ((cSparsified == 1) ? " ON" : " OFF") << RESET;
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, cSparsified);
            }
        }
    }
    // for(auto cBoard: *fDetectorContainer)
    // {
    //     bool cSparsified = (fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable") == 1);
    //     cBoard->setSparsification(cSparsified);
    // }

    // clear map of modified registers
    // probably this should be a container per board
    // since we should be able to mix different types of boards
    // but need to decide if this is per board/OG/hybrid
    // TO-DO
    fWithCIC   = 0;
    fWithLpGBT = 0;
    fWithSSA   = 0;
    fWithMPA   = 0;
    fWithCBC   = 0;
    // set-up register tracking
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    cChip->setRegisterTracking(1);
                    cChip->ClearModifiedRegisterMap();
                    LOG(DEBUG) << BOLDYELLOW << fMyName << "::Prepare Chip#" << +cChip->getId() << " register tracking set to " << +cChip->getRegisterTracking() << RESET;
                } // chips
            }     // hybrids
        }         // optical groups
    }             // board

    // figure out what type of FEs are connected
    for(auto cBoard: *fDetectorContainer)
    {
        auto cConnectedFEs = cBoard->connectedFrontEndTypes();
        fWithSSA           = (std::find_if(cConnectedFEs.begin(), cConnectedFEs.end(), [](FrontEndType x) { return x == FrontEndType::SSA; }) != cConnectedFEs.end()) ? 1 : 0;
        fWithMPA           = (std::find_if(cConnectedFEs.begin(), cConnectedFEs.end(), [](FrontEndType x) { return x == FrontEndType::MPA; }) != cConnectedFEs.end()) ? 1 : 0;
        fWithCBC           = (std::find_if(cConnectedFEs.begin(), cConnectedFEs.end(), [](FrontEndType x) { return x == FrontEndType::CBC3; }) != cConnectedFEs.end()) ? 1 : 0;
        for(auto cOpticalGroup: *cBoard)
        {
            fWithLpGBT = (cOpticalGroup->flpGBT != nullptr) ? 1 : 0;
            fWithCIC   = fWithLpGBT;
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fWithCIC   = fWithCIC || (cCic != nullptr);
            } // hybrid
        }     // optical group
    }         // board

    // prepare list of Chip registers to perserve
    fDetectorDataContainer = &fChipRegsToPerserve;
    ContainerFactory::copyAndInitChip<std::vector<std::string>>(*fDetectorContainer, *fDetectorDataContainer);

    fSuccess = false;
}

// configure print-out options
void OTTool::ConfigurePrintout(PrintConfig pCnfg)
{
    fPrintConfig.fVerbose    = pCnfg.fVerbose;
    fPrintConfig.fPrintEvery = pCnfg.fPrintEvery;
}
// set list of board registers to perserve
void OTTool::SetBrdRegstoPerserve(std::vector<std::string> pListOfRegs)
{
    fBrdRegsToPerserve.clear();
    for(const auto& cRegName: pListOfRegs)
    {
        LOG(INFO) << BOLDBLUE << "Adding " << cRegName << " to list of Brd Regs to perserve..." << RESET;
        fBrdRegsToPerserve.push_back(cRegName);
    }
}
// set list of Chip registers to perserve
void OTTool::SetChipRegstoPerserve(FrontEndType pType, std::vector<std::string> pListOfRegs)
{
    LOG(INFO) << BOLDBLUE << fMyName << " setting registers to store on Chips." << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cChipRegsToPreserveThisBrd = fChipRegsToPerserve.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cChipRegsToPreserveThisOG = cChipRegsToPreserveThisBrd->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cChipRegsToPreserveThisHybrd = cChipRegsToPreserveThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() != pType) continue;

                    auto& cChipRegsToPreserveThisChip = cChipRegsToPreserveThisHybrd->at(cChip->getIndex());
                    auto& cRegsToPerserve             = cChipRegsToPreserveThisChip->getSummary<std::vector<std::string>>();
                    cRegsToPerserve.clear();
                    for(const auto& cRegName: pListOfRegs)
                    {
                        LOG(INFO) << BOLDBLUE << "Adding " << cRegName << " to list of Chip Regs to perserve...Chip#" << +cChip->getId() << RESET;
                        cRegsToPerserve.push_back(cRegName);
                    }
                } // Chips
            }     // Hybrds
        }         // OGs
    }             // brd
}

// read data from file
void OTTool::ReadDataFromFile(std::string pRawFileName)
{
    this->addFileHandler(pRawFileName, 'r');
    std::vector<uint32_t> cData;
    this->readFile(cData);
    LOG(INFO) << BOLDBLUE << "BeamTestCheck::ReadDataFromFile Read back " << +cData.size() << " 32-bit words from the .raw file : " << pRawFileName << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        size_t cNevents = fNevents;
        DecodeData(cBoard, cData, cNevents, fBeBoardInterface->getBoardType(cBoard));
        // const std::vector<Event*>& cEvents = GetEvents ();
        LOG(INFO) << BOLDBLUE << "BeamTestCheck::ReadDataFromFile decoded back " << +cNevents << " events from the .raw file [BeBoard#" << +cBoard->getId() << "]" << RESET;
        if(fPrintConfig.fVerbose) PrintData(cBoard);
    }
}

// print data
void OTTool::PrintData(BeBoard* pBoard)
{
    const std::vector<Event*>& cEvents = GetEvents();
    LOG(INFO) << BOLDRED << "Printing events from FC7.. collected : " << +cEvents.size() << " events." << RESET;
    fEventCountInt = 0;
    for(auto& cEvent: cEvents)
    {
        EventPrintout(pBoard, cEvent);
        fEventCountInt++;
    }
}

// wait for triggers
void OTTool::WaitForTriggers(BeBoard* pBoard)
{
    // get D19cFW Interface
    fBeBoardInterface->setBoard(pBoard->getId());
    auto cInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    auto cTriggerInterface = cInterface->getTriggerInterface();

    LOG(INFO) << BOLDBLUE << fMyName << "::WaitForTriggers with ReadData.. will wait to readout until I've seen " << fNevents << " triggers " << RESET;
    std::vector<uint32_t> cCompleteData(0);

    // stop triggers
    fBeBoardInterface->Stop(pBoard);
    size_t              cCounter = 0;
    uint32_t            cNevents = 0;
    bool                cBreak   = false;
    bool                cWait    = false;
    std::vector<size_t> cTriggerCounters(0);
    do
    {
        // check state of triggers FSM
        if(cTriggerInterface->GetTriggerState() != 1)
        {
            cTriggerInterface->Stop();
            cTriggerInterface->Start();
        }

        std::this_thread::sleep_for(std::chrono::microseconds(fReadoutPause));
        auto cTriggerCounter = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
        cTriggerCounters.push_back(cTriggerCounter);
        if(cCounter % 200 == 0 && cCounter > 0)
        { LOG(INFO) << BOLDMAGENTA << "BeamTestCheck continuousReadout loop ... " << +cTriggerCounters[cTriggerCounters.size() - 1] << " triggers received" << RESET; }
        cCounter++;
        cBreak = (cTriggerCounter >= fNevents);
    } while(!cBreak);
    // stop triggers
    cTriggerInterface->Stop();
    // wait for 100 ms after stopping triggers just in case
    // we are still reading out a very large event
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<uint32_t> cData(0);
    cNevents += ReadData(pBoard, cData, cWait);
    if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
    DecodeData(pBoard, cCompleteData, cNevents, fBeBoardInterface->getBoardType(pBoard));
    LOG(INFO) << BOLDYELLOW << fMyName << " : Mean trigger rate is " << cTriggerCounters[cTriggerCounters.size() - 1] / (cCounter * fReadoutPause * 1e-6) << " Hz"
              << " .... readout " << cNevents << " when " << fNevents << " were requested." << RESET;
}

// poll boards for number of triggers
void OTTool::TriggerMonitor(uint32_t pDelta_s)
{
    // launch thread to catch ctrl+c from command line
    std::thread cCatchStopTh;

    // make sure that triggers have been started on all board
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        auto cInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        auto cTriggerInterface = cInterface->getTriggerInterface();
        if(cTriggerInterface->GetTriggerState() == 0) cTriggerInterface->Start();
    }

    // get start time for monitoring
    auto startTimeUTC_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // create text file to store data
    // file name :
    std::stringstream cFileName;
    cFileName << fDirectoryName << "TriggerMonitor_" << startTimeUTC_us << ".dat";
    // file format :
    std::ofstream cLogFile;
    cLogFile.open(cFileName.str(), std::ios::out | std::ios::app);
    cLogFile.close();
    LOG(INFO) << fMyName << ":Starting trigger monitor ... Results will be saved to " << cFileName.str() << RESET;
    cCatchStopTh = std::thread(&OTTool::CatchStop, this);
    try
    {
        size_t cLoopCounter = 0;
        // initialize container to hold trigger counters
        DetectorDataContainer cTrigCounters;
        ContainerFactory::copyAndInitBoard<std::vector<uint32_t>>(*fDetectorContainer, cTrigCounters);
        for(auto cBoard: *fDetectorContainer)
        {
            auto& cCounterThisBrd = cTrigCounters.at(cBoard->getIndex())->getSummary<std::vector<uint32_t>>();
            cCounterThisBrd.clear();
        }
        auto cTime0 = startTimeUTC_us;
        do
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(pDelta_s * 1000));
            auto currentTimeUTC_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            auto cDeltaTime_us     = currentTimeUTC_us - cTime0;
            cLogFile.open(cFileName.str(), std::ios::out | std::ios::app);
            for(auto cBoard: *fDetectorContainer)
            {
                fBeBoardInterface->setBoard(cBoard->getId());
                auto  cInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
                auto  cTriggerInterface = cInterface->getTriggerInterface();
                auto  cTriggerState     = cTriggerInterface->GetTriggerState();
                auto& cCounterThisBrd   = cTrigCounters.at(cBoard->getIndex())->getSummary<std::vector<uint32_t>>();
                cCounterThisBrd.push_back(fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter"));
                auto cDeltaTriggers   = (cCounterThisBrd.size() == 1) ? cCounterThisBrd[0] : cCounterThisBrd[cCounterThisBrd.size() - 1] - cCounterThisBrd[cCounterThisBrd.size() - 2];
                auto cTriggerSource   = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
                auto cTriggerRateInst = cDeltaTriggers / (cDeltaTime_us * 1e-6); // Hz
                // output to terminal
                LOG(INFO) << BOLDBLUE << "Monitoring triggers...BeBoard#" << +cBoard->getId() << " --- received " << cCounterThisBrd[cCounterThisBrd.size() - 1] << " triggers so far "
                          << " --- " << cDeltaTriggers << " triggers since the last check "
                          << " Inst. Trigger rate " << std::scientific << std::setprecision(3) << cTriggerRateInst << std::dec << " Hz "
                          << " [ trigger source is " << +cTriggerSource << " ]"
                          << " [ trigger state is " << +cTriggerState << " ]" << RESET;
                // send to file once you've got more than one point
                if(cCounterThisBrd.size() > 1) cLogFile << +cBoard->getId() << "\t" << currentTimeUTC_us << "\t" << cDeltaTriggers << "\n";
            }
            cLogFile.close();
            cTime0 = currentTimeUTC_us;
            cLoopCounter++;
        } while(fStopTriggerMonitor == 0);
    }
    catch(const std::exception& e)
    {
        LOG(INFO) << BOLDBLUE << "CatchStop caught ctrl+c ... will now exit main monitoring thread" << RESET;
        // wait for all threads to finish
        cCatchStopTh.join();
        cLogFile.close();
    }
}
// thread to monitor exit signal from main program
void OTTool::CatchStop()
{
    try
    {
        signal(SIGINT, StopTriggerMonitor);
    }
    catch(const std::exception& e)
    {
        LOG(INFO) << BOLDBLUE << "Caught stop signal from terminal..." << RESET;
        fStopTriggerMonitor = 1;
    }
}
// poll board from data
// one thread per BeBoard connected to this computer
void OTTool::ContinousReadout()
{
    // std::vector<uint32_t> cBrdEvntCntrs(fDetectorContainer->size(), 0);
    // reset all event counters
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        cInterface->ResetEventCounter();
        // cBrdEvntCntrs[cBoard->getIndex()] = cInterface->GetEventCounter();
    }

    // temporary thread object representing a new thread
    // there will be one check thread per board
    std::thread* cStartThreads = new std::thread[fDetectorContainer->size()];
    for(auto cBoard: *fDetectorContainer)
    {
        // launch threads for continuous readout
        cStartThreads[cBoard->getIndex()] = std::thread(&OTTool::StartReadoutTh, this, cBoard->getIndex());
    }
    for(auto cBoard: *fDetectorContainer)
    {
        // launch threads for continuous readout
        cStartThreads[cBoard->getIndex()].join(); // pauses until first finishes
    }

    // temporary thread object representing a new thread
    // there will be one check thread per board
    std::thread* cCheckDoneThreads = new std::thread[fDetectorContainer->size()];
    for(auto cBoard: *fDetectorContainer)
    {
        // launch threads for continuous readout
        cCheckDoneThreads[cBoard->getIndex()] = std::thread(&OTTool::CheckFinishedTh, this, cBoard->getIndex());
    }

    // temporary thread object representing a new thread
    // there will be one readout thread per board
    std::thread* cReadoutThreads = new std::thread[fDetectorContainer->size()];
    for(auto cBoard: *fDetectorContainer)
    {
        // launch threads for continuous readout
        cReadoutThreads[cBoard->getIndex()] = std::thread(&OTTool::ContinousReadoutTh, this, cBoard->getIndex());
    }

    // start triggers on all boards
    // for(auto cBoard: *fDetectorContainer) { fBeBoardInterface->Start(cBoard); }

    // wait until you have all events from all boards
    // exit condition for this run
    // bool   cAllFinished = false;
    // size_t cWaitCounter = 0;
    // LOG(DEBUG) << BOLDBLUE << fMyName << ":Main thread - starting to wait for events to be readout.." << RESET;
    // do
    // {
    //     size_t cNFinished = 0;
    //     for(auto cBoard: *fDetectorContainer)
    //     {
    //         fBeBoardInterface->setBoard(cBoard->getId());
    //         auto cInterface                   = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    //         cBrdEvntCntrs[cBoard->getIndex()] = cInterface->GetEventCounter();
    //         if(cWaitCounter % 1000 == 0)
    //             LOG(DEBUG) << BOLDBLUE << fMyName << ":Main thread ... read-back " << cBrdEvntCntrs[cBoard->getIndex()] << " events from BeBoard#" << +cBoard->getId() << RESET;
    //         // finished if I've received all events or if someone else has stopped triggers for me
    //         if(cBrdEvntCntrs[cBoard->getIndex()] >= fNevents || cInterface->GetTriggerState() == 0)
    //         {
    //             LOG(INFO) << BOLDBLUE << fMyName << ":Main thread ... finished collecting all requested events from BeBoard" << +cBoard->getId() << RESET;
    //             cNFinished++;
    //         }
    //     }
    //     cWaitCounter++;
    //     cAllFinished = (cNFinished == fDetectorContainer->size());
    // } while(!cAllFinished);

    // make double sure that all triggers have
    // been stopped here
    // stop triggers on all boards
    // for(auto cBoard: *fDetectorContainer) { fBeBoardInterface->Stop(cBoard); }

    // wait for all threads to finish
    // this will just do in sequence for all boards
    for(auto cBoard: *fDetectorContainer)
    {
        // launch threads for continuous readout
        cReadoutThreads[cBoard->getIndex()].join();   // pauses until first finishes
        cCheckDoneThreads[cBoard->getIndex()].join(); // pauses until first finishes
    }

    // now decode data sequentially
    // was having trouble doing this in the separate thread
    // for(auto cBoard : *fDetectorContainer)
    // {
    //     DecodeData(cBoard, fReadoutData[cBoard->getId()], fNevents, fBeBoardInterface->getBoardType(cBoard));
    //     LOG(INFO) << BOLDYELLOW << fMyName <<  " .... decoded " << fNevents << " events from the FC7" << RESET;
    // }
}
// poll board to check if readout has completed
void OTTool::StartReadoutTh(uint8_t cBrdId)
{
    // get D19cFW Interface
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBrdId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBrdId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    auto cTriggerInterface = cInterface->getTriggerInterface();
    auto cReadoutInterface = cInterface->getL1ReadoutInterface();
    cReadoutInterface->ResetReadout();
    cTriggerInterface->Start();
    LOG(INFO) << BOLDMAGENTA << "Started triggers on BeBoard#" << +cBrdId << RESET;
}
// continuous readout
// this will continue to read data until the stop triggers command
// has been reached
void OTTool::ContinousReadout(BeBoard* pBoard)
{
    // get D19cFW Interface
    fBeBoardInterface->setBoard(pBoard->getId());
    auto cInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    auto cTriggerInterface = cInterface->getTriggerInterface();

    LOG(INFO) << BOLDBLUE << fMyName << "::ContinousReadout ... until I've received " << fNevents << " events in the readout" << RESET;
    std::vector<uint32_t> cCompleteData(0);
    // stop triggers
    cTriggerInterface->Start();
    fEventCounter                = 0;
    size_t              cCounter = 0;
    bool                cBreak   = false;
    bool                cWait    = false;
    std::vector<size_t> cTriggerCounters(0);
    do
    {
        // check state of triggers FSM
        if(cTriggerInterface->GetTriggerState() != 1)
        {
            cTriggerInterface->Stop();
            cTriggerInterface->Start();
        }
        std::this_thread::sleep_for(std::chrono::microseconds(fReadoutPause));
        auto                  cTriggerCounter = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
        std::vector<uint32_t> cData(0);
        fEventCounter += ReadData(pBoard, cData, cWait);
        if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
        cTriggerCounters.push_back(cTriggerCounter);
        if(cCounter % 200 == 0 && cCounter > 0)
        { LOG(INFO) << BOLDMAGENTA << "BeamTestCheck continuousReadout loop ... " << +cTriggerCounters[cTriggerCounters.size() - 1] << " triggers received" << RESET; }
        cCounter++;
        cBreak = (fEventCounter >= fNevents);
    } while(!cBreak);
    cTriggerInterface->Stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<uint32_t> cData(0);
    fEventCounter += ReadData(pBoard, cData, cWait);
    if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
    DecodeData(pBoard, cCompleteData, fEventCounter, fBeBoardInterface->getBoardType(pBoard));
    LOG(INFO) << BOLDYELLOW << fMyName << " : Mean trigger rate is " << cTriggerCounters[cTriggerCounters.size() - 1] / (cCounter * fReadoutPause * 1e-6) << " Hz"
              << " .... readout " << fEventCounter << RESET;
}
// poll board to check if readout has completed
void OTTool::CheckFinishedTh(uint8_t cBrdId)
{
    // get D19cFW Interface
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBrdId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBrdId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    auto cTriggerInterface = cInterface->getTriggerInterface();

    // wait until triggers have started
    size_t cWaitCounter = 0;
    size_t cMaxWait     = 10000;
    do
    {
        std::this_thread::sleep_for(std::chrono::microseconds(fThreadWait));
        if(cWaitCounter % 100 == 0) LOG(DEBUG) << BOLDBLUE << "\t\t" << fMyName << ":Waiting for triggers to start on BeBoard#" << +cBrdId << RESET;
        cWaitCounter++;
    } while(cTriggerInterface->GetTriggerState() != 1 && cWaitCounter < cMaxWait);

    auto cCounter = cInterface->GetEventCounter();
    do
    {
        if(cCounter >= fNevents || cTriggerInterface->GetTriggerState() == 0)
        { LOG(INFO) << BOLDBLUE << fMyName << ":Main thread ... finished collecting all requested events from BeBoard" << +cBrdId << RESET; }
        std::this_thread::sleep_for(std::chrono::microseconds(fReadoutPause));
        cCounter = cInterface->GetEventCounter();
    } while(cCounter < fNevents);
    LOG(INFO) << BOLDBLUE << fMyName << ": check finished thread ... finished collecting all requested events from BeBoard" << +cBrdId << " .. will stop triggers.. " << RESET;
    cInterface->Stop();
}
// poll board from data
// one thread per BeBoard connected to this computer
void OTTool::ContinousReadoutTh(uint8_t cBrdId)
{
    fReadoutData[cBrdId].clear();
    LOG(DEBUG) << BOLDBLUE << fMyName << ":Starting continuous readout thread for BeBoard#" << +cBrdId << RESET;
    // get D19cFW Interface
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBrdId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBrdId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    auto cTriggerInterface = cInterface->getTriggerInterface();
    // wait until triggers have started
    size_t cWaitCounter = 0;
    size_t cMaxWait     = 10000;
    do
    {
        std::this_thread::sleep_for(std::chrono::microseconds(fThreadWait));
        if(cWaitCounter % 100 == 0) LOG(DEBUG) << BOLDBLUE << "\t\t" << fMyName << ":Waiting for triggers to start on BeBoard#" << +cBrdId << RESET;
        cWaitCounter++;
    } while(cTriggerInterface->GetTriggerState() != 1 && cWaitCounter < cMaxWait);

    if(cWaitCounter == cMaxWait)
    {
        LOG(INFO) << BOLDRED << "Triggers not started on this board.. start them myself!" << RESET;
        cTriggerInterface->Start();
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(fThreadWait));
            LOG(INFO) << BOLDRED << " ... waiting  for triggers to start... " << RESET;
        } while(cTriggerInterface->GetTriggerState() == 0);
    }

    LOG(DEBUG) << BOLDBLUE << fMyName << ":Triggers started .. now polling readout.." << RESET;
    bool                cWait = false;
    std::vector<size_t> cTriggerCounters(0);
    // repeat until triggers have stopped
    cWaitCounter        = 0;
    size_t cLclEvntCntr = 0;
    auto   cStartTime = std::chrono::high_resolution_clock::now(), cEndTime = cStartTime;
    size_t cAccumulatedWaits = 0;
    do
    {
        cAccumulatedWaits += fReadoutPause;
        std::this_thread::sleep_for(std::chrono::microseconds(fReadoutPause));
        auto                  cTriggerState   = cTriggerInterface->GetTriggerState();
        auto                  cTriggerSource  = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.fast_command_block.trigger_source");
        auto                  cTriggerCounter = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_stat.fast_command_block.trigger_in_counter");
        std::vector<uint32_t> cData(0);
        // cLclEvntCntr += ReadData(*cBoardIter, cData, cWait);
        cLclEvntCntr += fBeBoardInterface->ReadData((*cBoardIter), false, cData, cWait);
        if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(fReadoutData[cBrdId]));
        cTriggerCounters.push_back(cTriggerCounter);
        if(cWaitCounter % 100 == 0 && cWaitCounter > 0)
        {
            LOG(DEBUG) << BOLDBLUE << "\t\t" << fMyName << ":Waiting for triggers to be stopped on BeBoard#" << +cBrdId << " continuousReadout loop ... "
                       << +cTriggerCounters[cTriggerCounters.size() - 1] << " triggers received"
                       << " trigger source is " << +cTriggerSource << " trigger state is " << +cTriggerState << " and " << cLclEvntCntr << " events in the readout so far ... " << RESET;
        }
        cWaitCounter++;
    } while(cTriggerInterface->GetTriggerState() == 1);
    // now decode data
    cAccumulatedWaits += 100 * 1e3;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::vector<uint32_t> cData(0);
    // cLclEvntCntr += ReadData(*cBoardIter, cData, cWait);
    cLclEvntCntr += fBeBoardInterface->ReadData((*cBoardIter), false, cData, cWait);
    cEndTime       = std::chrono::high_resolution_clock::now();
    auto cDuration = std::chrono::duration_cast<std::chrono::milliseconds>(cEndTime - cStartTime).count();
    if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(fReadoutData[cBrdId]));
    LOG(INFO) << BOLDYELLOW << "BeBoard#" << +cBrdId << " readData found " << cLclEvntCntr << " events...\tAccumulated " << cAccumulatedWaits * 1e-3 << " ms of waits.. read-out "
              << fReadoutData[cBrdId].size() << " 32-bit words in " << cDuration << " ms." << RESET;

    DecodeData(*cBoardIter, fReadoutData[cBrdId], fNevents, fBeBoardInterface->getBoardType(*cBoardIter));
    LOG(INFO) << BOLDYELLOW << fMyName << "  Th for BeBoard#" << +cBrdId << " .... finished decoding " << fNevents << " events from the FC7" << RESET;
}
// event print-out
void OTTool::EventPrintout(BeBoard* pBoard, Event* pEvent)
{
    auto cSparsified = pBoard->getSparsification();
    if(cSparsified)
        LOG(DEBUG) << BOLDBLUE << "Checking with internal - sparisified data" << RESET;
    else
        LOG(DEBUG) << BOLDBLUE << "Checking with internal - un-sparisified data" << RESET;

    if(pEvent->GetEventCount() % fPrintConfig.fPrintEvery != 0) return;
    std::stringstream cEvntHeader;
    cEvntHeader << "Event#" << +pEvent->GetEventCount() << " -- " << +fEventCountInt << " in readout..." << RESET;
    std::stringstream cHeader;

    std::ofstream cOutFile_LT, cOutFile_RT, cOutFile_LB, cOutFile_RB;
    cOutFile_LT.open("OTTool_LT.dat", std::ios_base::app);
    cOutFile_RT.open("OTTool_RT.dat", std::ios_base::app);
    cOutFile_LB.open("OTTool_LB.dat", std::ios_base::app);
    cOutFile_RB.open("OTTool_RB.dat", std::ios_base::app);
    for(auto cOpticalGroup: *pBoard)
    {
        std::vector<uint16_t> cBxIds(0);
        std::vector<uint16_t> cL1Ids(0);
        std::stringstream     cOutOfSync;

        for(auto cHybrid: *cOpticalGroup)
        {
            auto cL1IdCIC  = static_cast<D19cCic2Event*>(pEvent)->L1Id(cHybrid->getId(), 0);
            auto cL1Status = static_cast<D19cCic2Event*>(pEvent)->L1Status(cHybrid->getId());
            auto cBxId     = (pEvent)->BxId(cHybrid->getId());
            auto cStubStat = static_cast<D19cCic2Event*>(pEvent)->Status(cHybrid->getId());

            if(pEvent->GetEventCount() < 10000)
            {
                std::vector<uint32_t> cHits_TopSensor(cHybrid->size() * 127, 0);
                std::vector<uint32_t> cHits_BottomSensor(cHybrid->size() * 127, 0);
                uint32_t              cTotalNHits = 0;
                for(auto cChip: *cHybrid)
                {
                    uint16_t cOffset = cChip->getId() * cChip->size() / 2.;
                    auto     cHits   = pEvent->GetHits(cHybrid->getId(), cChip->getId());
                    for(auto cChnl = 0; cChnl < (int)cChip->size(); cChnl++)
                    {
                        uint16_t cStripOffset = cOffset; //(cChnlIndx % 2 == 0) ? cOffset : (cNchannels*8) / 2 + cOffset;
                        uint16_t cStripId     = cStripOffset + cChnl / 2;
                        if(cHybrid->getId() % 2 == 0)
                        {
                            cStripOffset = (cChip->size() * 8) / 2 - 1; //(cChnlIndx % 2 == 0) ? (cNchannels*8) / 2 : (cNchannels*8);
                            cStripId     = cStripOffset - (cChip->getId() * cChip->size() / 2 + cChnl / 2);
                        }
                        auto cHitFound = std::find(cHits.begin(), cHits.end(), cChnl) != cHits.end();
                        cTotalNHits += (cHitFound) ? 1 : 0;
                        if(cChnl % 2 == 0)
                            cHits_BottomSensor[cStripId] = cHitFound ? 1 : 0;
                        else
                            cHits_TopSensor[cStripId] = cHitFound ? 1 : 0;
                    }
                }
                // if(cTotalNHits >= 0 )
                //{
                std::stringstream cEventPrintout_TopSensor;
                std::stringstream cEventPrintout_BottomSensor;
                cEventPrintout_TopSensor << cBxId << "\t";
                cEventPrintout_BottomSensor << cBxId << "\t";
                for(auto cStripId = 0; cStripId < cHybrid->size() * 127; cStripId++)
                {
                    if(cStripId < cHybrid->size() * 127 - 1)
                    {
                        cEventPrintout_TopSensor << cHits_TopSensor[cStripId] << "\t";
                        cEventPrintout_BottomSensor << cHits_BottomSensor[cStripId] << "\t";
                    }
                    else
                    {
                        cEventPrintout_TopSensor << cHits_TopSensor[cStripId];
                        cEventPrintout_BottomSensor << cHits_BottomSensor[cStripId];
                    }
                }

                if(cHybrid->getId() % 2 == 0)
                {
                    cOutFile_RT << cEventPrintout_TopSensor.str() << "\n";
                    cOutFile_RB << cEventPrintout_BottomSensor.str() << "\n";
                }
                else
                {
                    cOutFile_LT << cEventPrintout_TopSensor.str() << "\n";
                    cOutFile_LB << cEventPrintout_BottomSensor.str() << "\n";
                }
                //}
            } // checking

            std::stringstream cOutStubs;
            std::stringstream cOutL1;
            std::stringstream cOutAna;
            std::stringstream cOutEvntHeader;
            if(cStubStat != 0x00 || cL1Status != 0x00)
            {
                cOutEvntHeader << cEvntHeader.str() << BOLDRED << " Hybrid#" << +cHybrid->getId() << " on Link#" << +cOpticalGroup->getId() << " L1Id " << +cL1IdCIC << " Stub status is "
                               << std::bitset<8>(cStubStat) << " L1 status [FEs] is " << std::bitset<8>(cL1Status) << " L1 status [CIC] is " << std::bitset<1>(cL1Status & 0x1) << " BxId is "
                               << +cBxId;
                cHeader << BOLDRED << "\t\t.. "
                        << " Hybrid#" << +cHybrid->getId() << " on Link#" << +cOpticalGroup->getId() << " L1Id " << +cL1IdCIC << " Stub status is " << std::bitset<8>(cStubStat)
                        << " L1 status [FEs] is " << std::bitset<8>(cL1Status) << " L1 status [CIC] is " << std::bitset<1>(cL1Status & 0x1) << " BxId is " << +cBxId << "\n"
                        << RESET;
            }
            else
            {
                cOutEvntHeader << cEvntHeader.str() << BOLDGREEN << " Hybrid#" << +cHybrid->getId() << " on Link#" << +cOpticalGroup->getId() << " L1Id " << +cL1IdCIC << " Stub status is "
                               << std::bitset<8>(cStubStat) << " L1 status [FEs] is " << std::bitset<8>(cL1Status) << " L1 status [CIC] is " << std::bitset<1>(cL1Status & 0x1) << " BxId is "
                               << +cBxId;
                cHeader << BOLDGREEN << "\t\t.. "
                        << " Hybrid#" << +cHybrid->getId() << " on Link#" << +cOpticalGroup->getId() << " L1Id " << +cL1IdCIC << " Stub status is " << std::bitset<8>(cStubStat)
                        << " L1 status [FEs] is " << std::bitset<8>(cL1Status) << " L1 status [CIC] is " << std::bitset<1>(cL1Status & 0x1) << " BxId is " << +cBxId << "\n"
                        << RESET;
            }

            if(cBxIds.size() == 0)
            {
                cBxIds.push_back(cBxId);
                cL1Ids.push_back(cL1IdCIC);
            }
            else
            {
                if(std::find(cBxIds.begin(), cBxIds.end(), cBxId) == cBxIds.end()) cBxIds.push_back(cBxId);
                if(std::find(cL1Ids.begin(), cL1Ids.end(), cL1IdCIC) == cL1Ids.end()) cL1Ids.push_back(cL1IdCIC);
            }

            // if( cL1IdCIC == 511 ) LOG (INFO) << BOLDYELLOW << "OVERFLOW " << cOutEvntHeader.str() << RESET;
            // if( pEvent->GetEventCount() != fEventCountInt ) LOG (INFO) << BOLDRED << " Mismatch in event counter " << cOutEvntHeader.str() << RESET;
            // LOG(INFO) << BOLDYELLOW << cOutEvntHeader.str() << RESET;

            // bool cStubFound=false;
            // bool cClusterFound=false;
            // bool cAna=false;
            // for(auto cChip: *cHybrid)
            // {
            //     if(!cSparsified) break;
            //     if(cChip->getFrontEndType() == FrontEndType::SSA ) continue;

            //     auto  cStubs   = pEvent->StubVector(cHybrid->getId(), cChip->getId());
            //     if(cChip->getFrontEndType() == FrontEndType::CBC3 )
            //     {
            //         auto cClusters = (pEvent)->getClusters(cHybrid->getId(), cChip->getId());
            //         cOutL1 << BOLDBLUE << "\t..Chip#" << +cChip->getId() << " has " << +cClusters.size() << " clusters." << RESET;
            //     }
            //     else
            //     {
            //         auto cStripClusters = static_cast<D19cCic2Event*>(pEvent)->GetStripClusters(cHybrid->getId(), cChip->getId());
            //         auto cPxlClusters = static_cast<D19cCic2Event*>(pEvent)->GetPixelClusters(cHybrid->getId(), cChip->getId());
            //         if( cStubs.size() > 0 )
            //         {
            //             cStubFound=true;
            //             cOutStubs << BOLDYELLOW << "\t..Chip#" << +cChip->getId() << " has "
            //                 << +cStubs.size() << " stubs"
            //                 << "\t : ";
            //             uint8_t cIndx=0;
            //             for(auto cStub : cStubs )
            //             {
            //                 cOutStubs << " Stub#" << +cIndx << " : Seed " << +cStub.getPosition()
            //                      << " Row " << +cStub.getRow()
            //                      << " Bend " << +cStub.getBend()
            //                      << " \t";
            //                 cIndx++;
            //             }
            //         }
            //         if( cStripClusters.size() > 0 &&  cPxlClusters.size() > 0 )
            //         {
            //             cClusterFound=true;
            //             cOutL1 << BOLDBLUE << "\t..Chip#" << +cChip->getId() << " has "
            //                 << +cStripClusters.size() << " S-clusters and "
            //                 << +cPxlClusters.size() << " P-clusters\t";
            //             for( auto cPxlCluster : cPxlClusters )
            //             {
            //                 cOutL1 << BOLDMAGENTA << "\tP-Address " << +cPxlCluster.fAddress << " P-Position " << +cPxlCluster.fZpos << " P_Width " << +cPxlCluster.fWidth << ";";
            //             }

            //             for( auto cStripCluster : cStripClusters )
            //             {
            //                 cOutL1 << BOLDGREEN << "\tS-Address " << +cStripCluster.fAddress << " S_Width " << +cStripCluster.fWidth << ";";
            //             }
            //         }
            //         if( cStripClusters.size() == 1 &&  cPxlClusters.size() == 1 )
            //         {
            //             cAna = true;
            //             float cCenterOfMassP=0;
            //             for( auto cPxlCluster : cPxlClusters )
            //             {
            //                 cCenterOfMassP += cPxlCluster.fAddress + cPxlCluster.fWidth;
            //             }
            //             cCenterOfMassP /= cPxlClusters.size();

            //             float cCenterOfMassS=0;
            //             for( auto cStripCluster : cStripClusters )
            //             {
            //                 cCenterOfMassS += cStripCluster.fAddress + cStripCluster.fWidth;
            //             }
            //             cCenterOfMassS /= cStripClusters.size();

            //             cOutAna << BOLDYELLOW << "\t..Chip#" << +cChip->getId() << " has "
            //                 << +cStripClusters.size() << " S-clusters and "
            //                 << +cPxlClusters.size() << " P-clusters\t"
            //                 << "..Center of mass P : " << cCenterOfMassP << " Center of mass S : " << cCenterOfMassS
            //                 << " # of stubs is " << cStubs.size()
            //                 << "\n";
            //         }
            //     }
            // }
            // if( (cClusterFound || cStubFound) && pEvent->GetEventCount() < 10  )
            //     LOG (INFO) << cOutEvntHeader.str() << RESET;
            // if( (cClusterFound && cStubFound) && pEvent->GetEventCount() < 10 )
            // {
            //     LOG(INFO) << BOLDGREEN << " Clusters and stubs in the same event " << RESET;
            //     LOG(INFO) << cOutStubs.str() << RESET;
            //     LOG(INFO) << cOutL1.str() << RESET;
            // }
            // if( (cClusterFound && !cStubFound) && pEvent->GetEventCount() < 10 )
            // {
            //     LOG(INFO) << BOLDRED << " Clusters and no stubs in the same event " << RESET;
            //     LOG(INFO) << cOutL1.str() << RESET;
            // }
            // if( (!cClusterFound && cStubFound) && pEvent->GetEventCount() < 10 )
            // {
            //     LOG(INFO) << BOLDRED << " Stubs but no Clusters in the same event " << RESET;
            //     LOG(INFO) << cOutStubs.str() << RESET;
            // }
            // if( cAna && pEvent->GetEventCount() < 10 )
            // {
            //     LOG(INFO) << BOLDYELLOW << " Event with exactly one cluster in both sensors " << RESET;
            //     LOG(INFO) << cOutAna.str() << RESET;
            // }
        }
        // if(cBxIds.size() > 1)
        // {
        //     LOG(INFO) << BOLDRED << cEvntHeader.str() << " ... OUT OF SYNC " << RESET;
        //     LOG(INFO) << cHeader.str() << RESET;
        // }
        // else
        // {
        //     if( pEvent->GetEventCount() != fEventCountInt )
        //     {
        //         LOG(INFO) << BOLDGREEN << cEvntHeader.str() << " ... IN SYNC " << RESET;
        //         LOG(INFO) << "Event#" << +fEventCountInt << " in readout..." << cHeader.str() << RESET;
        //     }
        // }
    }

    cOutFile_LT.close();
    cOutFile_RT.close();
    cOutFile_LB.close();
    cOutFile_RB.close();
}

//
void OTTool::InjectPattern(BeBoard* pBoard, std::vector<Injection> pInjections, int pChipId)
{
    bool cInjectAll = false;
    LOG(DEBUG) << BOLDMAGENTA << "Injecting " << +pInjections.size() << " in PS module.." << RESET;
    // inject pixel clusters
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                // fReadoutChipInterface->WriteChipReg(cChip, "ReadoutMode",0x0);
                if(cChip->getId() % 8 != pChipId && pChipId > 0) continue;
                LOG(DEBUG) << BOLDMAGENTA << "Injecting patterns in Chip#" << +cChip->getId() << RESET;
                // make sure L1 latency is configured
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    if(fInjectionType == 0)
                    {
                        // for digi injection .. explicity disable all other pixels
                        fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                        (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, pInjections, 0x01);
                    }
                    else
                    {
                        for(auto cInjection: pInjections)
                        {
                            if(cInjectAll) continue;
                            auto cPxl = cInjection.fColumn * NSSACHANNELS + (uint32_t)cInjection.fRow;
                            LOG(INFO) << BOLDBLUE << " Injecting in pixel " << +cPxl << " column " << +cInjection.fColumn << " row " << +cInjection.fRow << RESET;
                            std::stringstream cRegName;
                            cRegName << "ENFLAGS_P" << +cPxl;
                            fReadoutChipInterface->WriteChipReg(cChip, cRegName.str(), 0x5F, false);
                        }
                        if(cInjectAll) fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x5F);
                    }
                }
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    // for digi injection .. explicity disable all other strips
                    if(pInjections.size() > 0 && fInjectionType == 0)
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                        fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_L_ALL", 0x00);
                        fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_H_ALL", 0x00);
                        fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x01);
                    }
                    for(auto cInjection: pInjections)
                    {
                        if(fInjectionType == 0)
                        {
                            std::stringstream cRegNameEn;
                            cRegNameEn << "ENFLAGS_S" << +cInjection.fRow;
                            fReadoutChipInterface->WriteChipReg(cChip, cRegNameEn.str(), 0x8);
                            std::stringstream cRegNamePattern;
                            cRegNamePattern << "DigCalibPattern_L_S" << +cInjection.fRow;
                            fReadoutChipInterface->WriteChipReg(cChip, cRegNamePattern.str(), 0x01);
                        }
                        else
                        {
                            if(cInjectAll) continue;
                            LOG(INFO) << BOLDGREEN << " Inection in Strip#" << +cInjection.fRow << RESET;
                            fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x08);
                            fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cInjection.fRow), 0x11);
                        }
                    }
                    if(fInjectionType == 1 && cInjectAll)
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x08);
                        fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x11);
                    }
                }
            } // chip
        }     // hybrid
    }         // optica]l group
}
void OTTool::UpdateFromRegMap(BeBoard* pBoard)
{
    // important registers for beBoard
    std::vector<std::string> cBoardRegs{
        "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", "fc7_daq_cnfg.fast_command_block.trigger_source"};
    cBoardRegs.push_back("fc7_daq_cnfg.tlu_block.trigger_id_delay");
    cBoardRegs.push_back("fc7_daq_cnfg.tlu_block.tlu_enabled");
    cBoardRegs.push_back("fc7_daq_cnfg.tlu_block.handshake_mode");
    BeBoardRegMap cRegMap = pBoard->getBeBoardRegMap();
    for(auto cReg: cBoardRegs)
    {
        LOG(INFO) << BOLDBLUE << "Setting " << cReg << " to " << +cRegMap[cReg] << RESET;
        fBeBoardInterface->WriteBoardReg(pBoard, cReg, cRegMap[cReg]);
    }

    // set thresholds
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                uint32_t cThreshold = 0;
                if(cChip->getFrontEndType() == FrontEndType::CBC3) cThreshold = (cChip->getReg("VCth1") + (cChip->getReg("VCth2") << 8));
                if(cChip->getFrontEndType() == FrontEndType::SSA) cThreshold = cChip->getReg("Bias_THDAC");
                if(cChip->getFrontEndType() == FrontEndType::MPA) cThreshold = cChip->getReg("ThDAC0");
                LOG(INFO) << BOLDMAGENTA << "Setting threshold on Chip#" << +cChip->getId() << " to 0x" << std::hex << +cThreshold << std::dec << RESET;
                fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
            }
        }
    }
    // set stub mode from xml
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    auto cMode = cChip->getReg("ECM");
                    fReadoutChipInterface->WriteChipReg(cChip, "ECM", cMode);
                    LOG(INFO) << BOLDMAGENTA << "Setting StubMode regisger on Chip#" << +cChip->getId() << " to " << cMode << RESET;
                }
            }
        }
    }
    // set hit mode from xml
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    auto cMode = cChip->getReg("ModeSel_ALL");
                    fReadoutChipInterface->WriteChipReg(cChip, "ModeSel_ALL", cMode);
                    LOG(INFO) << BOLDMAGENTA << "Setting HitLogicMode register on Chip#" << +cChip->getId() << " to " << cMode << RESET;
                }
                else if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    auto cMode = cChip->getReg("SAMPLINGMODE_ALL");
                    fReadoutChipInterface->WriteChipReg(cChip, "SAMPLINGMODE_ALL", cMode);
                    LOG(INFO) << BOLDMAGENTA << "Setting HitLogicMode register on Chip#" << +cChip->getId() << " to " << cMode << RESET;
                }
            }
        }
    }
    // charge injection
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                std::string cRegName = "";
                if(cChip->getFrontEndType() == FrontEndType::MPA) { cRegName = "CalDAC0"; }
                else if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    cRegName = "Bias_CALDAC";
                }
                else if(cChip->getFrontEndType() == FrontEndType::CBC3)
                {
                    cRegName = "MiscTestPulseCtrl&AnalogMux";
                }
                uint8_t cInjectedCharge = cChip->getReg(cRegName);
                if(cChip->getFrontEndType() == FrontEndType::CBC3) cInjectedCharge = (cInjectedCharge >> 6) & 0x3F;
                fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", cInjectedCharge);
                LOG(INFO) << BOLDMAGENTA << "Setting Charge injection register on Chip#" << +cChip->getId() << " to " << +cInjectedCharge << RESET;
            }
        }
    }
    // latency
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                uint16_t cLatency = 0;
                if(cChip->getFrontEndType() == FrontEndType::MPA) { cLatency = cChip->getReg("L1Offset_2_ALL") << 8 | cChip->getReg("L1Offset_1_ALL"); }
                else if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    cLatency = cChip->getReg("L1-Latency_MSB") << 8 | cChip->getReg("L1-Latency_LSB");
                }
                else if(cChip->getFrontEndType() == FrontEndType::CBC3)
                {
                    auto cRegValueFirst  = cChip->getReg("FeCtrl&TrgLat2");
                    auto cRegValueSecond = cChip->getReg("TriggerLatency1");
                    cLatency             = ((cRegValueFirst & 0x1) << 8) | cRegValueSecond;
                }
                LOG(INFO) << BOLDYELLOW << "Setting latency on Chip#" << +cChip->getId() << " to " << cLatency << RESET;
                fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency);
            }
        }
    }
    // configure HIPs
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                uint16_t    cCut = 0;
                std::string cRegName;
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    cRegName = "HipCut_ALL";
                    cCut     = cChip->getReg(cRegName);
                }
                else if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    cRegName = "HIPCUT_ALL";
                    cCut     = cChip->getReg(cRegName);
                }
                else if(cChip->getFrontEndType() == FrontEndType::CBC3)
                {
                    cCut = cChip->getReg("HIP&TestMode");
                }
                LOG(INFO) << BOLDYELLOW << "Setting HIP register on Chip#" << +cChip->getId() << " to " << cCut << RESET;
                fReadoutChipInterface->WriteChipReg(cChip, cRegName, cCut);
            }
        }
    }
    // configure sampling delay
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    std::vector<std::string> cRegNames{"PhaseShiftClock", "ClockDeskewing"};
                    for(auto cRegName: cRegNames)
                    {
                        auto cValueInMemory = cChip->getReg(cRegName);
                        fReadoutChipInterface->WriteChipReg(cChip, cRegName, cValueInMemory);
                    }
                }
                else if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    std::vector<std::string> cRegNames{"PhaseShift", "ConfDLL"};
                    for(auto cRegName: cRegNames)
                    {
                        auto cValueInMemory = cChip->getReg(cRegName);
                        fReadoutChipInterface->WriteChipReg(cChip, cRegName, cValueInMemory);
                    }
                }
                // To-Do add CBC
                // else if( cChip->getFrontEndType() == FrontEndType::CBC3 )
                // {
                // }
            }
        }
    }

    // make sure MPAs have both modes enabled
    // and that the disabled strips in the SSAs are really disabled
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                std::string cRegName = "";
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0F);
                    LOG(INFO) << BOLDMAGENTA << "Setting ENFLAGS_ALL on Chip#" << +cChip->getId() << " to enable both modes.." << RESET;
                }
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    for(size_t cIndx = 0; cIndx < NSSACHANNELS; cIndx++)
                    {
                        std::stringstream cRegName;
                        cRegName << "ENFLAGS_S" << +(cIndx + 1);
                        auto cValueInMemory = cChip->getReg(cRegName.str());
                        if((cValueInMemory & 0x1) == 0) // strip is masked
                        { fReadoutChipInterface->WriteChipReg(cChip, cRegName.str(), cValueInMemory); }
                    }
                }
            }
        }
    }

    // make sure all CBC logic registers are re-configured
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;

                std::vector<std::string> cRegNames{"TestPulsePotNodeSel",
                                                   "MiscTestPulseCtrl&AnalogMux",
                                                   "HIP&TestMode",
                                                   "Pipe&StubInpSel&Ptwidth",
                                                   "CoincWind&Offset34",
                                                   "CoincWind&Offset12",
                                                   "LayerSwap&CluWidth",
                                                   "40MhzClk&Or254"};
                for(auto cRegName: cRegNames)
                {
                    auto cValueInMemory = cChip->getReg(cRegName);
                    fReadoutChipInterface->WriteChipReg(cChip, cRegName, cValueInMemory);
                    LOG(DEBUG) << BOLDMAGENTA << "Configuring CBC#" << +cChip->getId() << " register " << cRegName << " to 0x" << std::hex << +cValueInMemory << std::dec << RESET;
                }
                // masks
                for(auto cMapItem: cChip->getRegMap())
                {
                    if(cMapItem.first.find("MaskChannel") != std::string::npos)
                    {
                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        fReadoutChipInterface->WriteChipReg(cChip, cMapItem.first, cValueInMemory);
                        LOG(DEBUG) << BOLDMAGENTA << "Configuring CBC#" << +cChip->getId() << " register " << cMapItem.first << " to 0x" << std::hex << +cValueInMemory << std::dec << RESET;
                    }
                }
            }
        }
    }
}
