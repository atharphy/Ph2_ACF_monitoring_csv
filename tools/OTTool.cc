#include "OTTool.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

#include "../Utils/ContainerFactory.h"

OTTool::OTTool() : Tool()
{
    fBoardRegContainer.reset();
    fBrdRegsToPerserve.clear();
    fROCRegsToPerserve.reset();
    fSuccess = false;
    fMyName  = "OTTool";
}

OTTool::~OTTool() {}
// Reset register on BeBoard + ROCs
void OTTool::Reset()
{
    if( fReadoutMode == 1 ) return;

    LOG(INFO) << BOLDGREEN << "Resetting registers touched  by " << fMyName << RESET;
    // set everything back to original values .. like I wasn't here
    bool cWithPS = false;
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

        auto& cROCRegsToPreserveThisBrd = fROCRegsToPerserve.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cROCRegsToPreserveThisOG = cROCRegsToPreserveThisBrd->at(cOpticalGroup->getIndex());
            bool  cWithLpGBT               = (cOpticalGroup->flpGBT != nullptr);
            for(auto cHybrid: *cOpticalGroup)
            {
                auto cType    = FrontEndType::SSA;
                bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                cType         = FrontEndType::MPA;
                bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                bool cIsPS    = (cWithSSA && cWithMPA) && cWithLpGBT;
                cWithPS       = cWithPS || cIsPS;
                LOG(DEBUG) << BOLDBLUE << fMyName << ":Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;
                auto& cROCRegsToPreserveThisHybrd = cROCRegsToPreserveThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cROCRegsToPreserveThisROC = cROCRegsToPreserveThisHybrd->at(cChip->getIndex());
                    auto& cRegsToPerserve           = cROCRegsToPreserveThisROC->getSummary<std::vector<std::string>>();
                    if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->UpdateModifiedRegisterMap(cChip);
                    auto cModMap = fReadoutChipInterface->GetModifiedRegisterMap(cChip);
                    LOG(DEBUG) << BOLDBLUE << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    for(auto cMapItem: cModMap)
                    {
                        // skip registers that I should perserve for this ROC
                        if(std::find(cRegsToPerserve.begin(), cRegsToPerserve.end(), cMapItem.first) != cRegsToPerserve.end())
                        {
                            LOG(DEBUG) << BOLDBLUE << "Skipping reconfiguration of " << cMapItem.first << RESET;
                            continue;
                        }

                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        LOG(DEBUG) << BOLDBLUE << fMyName << "::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to "
                                   << cMapItem.second.fValue << RESET;
                        fReadoutChipInterface->WriteChipReg(cChip, cMapItem.first, cMapItem.second.fValue);
                    }
                }
            }
        }
    }
    if(fReadoutChipInterface != nullptr)
    {
        fReadoutChipInterface->ClearModifiedRegisterMap();
        if(cWithPS) static_cast<PSInterface*>(fReadoutChipInterface)->ResetModifiedRegisterMap();
    }
    resetPointers();

    // for  now .. keep triggers running on all boards
    for(auto cBoard: *fDetectorContainer) fBeBoardInterface->Start(cBoard);
}

// Initialization function
void OTTool::Prepare()
{
    // retreive number of events from settings file
    fNevents = findValueInSettings("Nevents", 10);

    if( fReadoutMode == 1 ) return;
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
        bool cSparsified = (fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable") == 1);
        cBoard->setSparsification(cSparsified);
    }

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
    if(fReadoutChipInterface != nullptr)
    {
        fReadoutChipInterface->ClearModifiedRegisterMap();
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                fWithLpGBT = (cOpticalGroup->flpGBT != nullptr) ? 1 : 0;
                fWithCIC   = fWithLpGBT;
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fWithCIC   = fWithCIC || (cCic != nullptr);
                    fWithSSA =
                        fWithSSA || (std::find_if(cHybrid->begin(), cHybrid->end(), [](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == FrontEndType::SSA; }) != cHybrid->end()) ? 1 : 0;
                    fWithMPA =
                        fWithMPA || (std::find_if(cHybrid->begin(), cHybrid->end(), [](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == FrontEndType::MPA; }) != cHybrid->end()) ? 1 : 0;
                    fWithCBC =
                        fWithCBC || (std::find_if(cHybrid->begin(), cHybrid->end(), [](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == FrontEndType::CBC3; }) != cHybrid->end()) ? 1 : 0;
                }
            }
        }
        bool cIsPS = (fWithSSA && fWithMPA) && fWithLpGBT;
        if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->ResetModifiedRegisterMap();
    }

    // prepare list of ROC registers to perserve
    fDetectorDataContainer = &fROCRegsToPerserve;
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
// set list of ROC registers to perserve
void OTTool::SetROCRegstoPerserve(FrontEndType pType, std::vector<std::string> pListOfRegs)
{
    LOG(INFO) << BOLDBLUE << fMyName << " setting registers to store on ROCs." << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cROCRegsToPreserveThisBrd = fROCRegsToPerserve.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cROCRegsToPreserveThisOG = cROCRegsToPreserveThisBrd->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cROCRegsToPreserveThisHybrd = cROCRegsToPreserveThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() != pType) continue;

                    auto& cROCRegsToPreserveThisROC = cROCRegsToPreserveThisHybrd->at(cChip->getIndex());
                    auto& cRegsToPerserve           = cROCRegsToPreserveThisROC->getSummary<std::vector<std::string>>();
                    cRegsToPerserve.clear();
                    for(const auto& cRegName: pListOfRegs)
                    {
                        LOG(INFO) << BOLDBLUE << "Adding " << cRegName << " to list of ROC Regs to perserve...ROC#" << +cChip->getId() << RESET;
                        cRegsToPerserve.push_back(cRegName);
                    }
                } // ROCs
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
    LOG(INFO) << BOLDBLUE << "BeamTestCheck2S::ReadDataFromFile Read back " << +cData.size() << " 32-bit words from the .raw file : " << pRawFileName << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        size_t cNevents = fNevents;
        DecodeData(cBoard, cData, cNevents, fBeBoardInterface->getBoardType(cBoard));
        // const std::vector<Event*>& cEvents = GetEvents ();
        LOG(INFO) << BOLDBLUE << "BeamTestCheck2S::ReadDataFromFile decoded back " << +cNevents << " events from the .raw file [BeBoard#" << +cBoard->getId() << "]" << RESET;
        if(fPrintConfig.fVerbose) PrintData(cBoard);
    }
}

// print data
void OTTool::PrintData(BeBoard* pBoard)
{
    const std::vector<Event*>& cEvents = GetEvents();
    LOG(INFO) << BOLDBLUE << "Printing events from FC7.. collected : " << +cEvents.size() << " events." << RESET;
    for(auto& cEvent: cEvents) { EventPrintout(pBoard, cEvent); }
}

// wait for triggers
void OTTool::WaitForTriggers(BeBoard* pBoard)
{
    // get D19cFW Interface
    fBeBoardInterface->setBoard(pBoard->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

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
        if(cInterface->GetTriggerState() != 1)
        {
            fBeBoardInterface->Stop(pBoard);
            fBeBoardInterface->Start(pBoard);
        }

        std::this_thread::sleep_for(std::chrono::microseconds(fReadoutPause));
        auto cTriggerCounter = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
        cTriggerCounters.push_back(cTriggerCounter);
        if(cCounter % 200 == 0 && cCounter > 0)
        { LOG(INFO) << BOLDMAGENTA << "BeamTestCheck2S continuousReadout loop ... " << +cTriggerCounters[cTriggerCounters.size() - 1] << " triggers received" << RESET; }
        cCounter++;
        cBreak = (cTriggerCounter >= fNevents);
    } while(!cBreak);
    // stop triggers
    fBeBoardInterface->Stop(pBoard);
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
        auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        if(cInterface->GetTriggerState() == 0) fBeBoardInterface->Start(cBoard);
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
                auto  cInterface      = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
                auto  cTriggerState   = cInterface->GetTriggerState();
                auto& cCounterThisBrd = cTrigCounters.at(cBoard->getIndex())->getSummary<std::vector<uint32_t>>();
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
    // temporary thread object representing a new thread
    // there will be one readout thread per board
    std::thread* cReadoutThreads = new std::thread[fDetectorContainer->size()];
    for(auto cBoard: *fDetectorContainer)
    {
        // launch threads for continuous readout
        cReadoutThreads[cBoard->getIndex()] = std::thread(&OTTool::ContinousReadoutTh, this, cBoard->getIndex());
    }

    std::vector<uint32_t> cBrdEvntCntrs(fDetectorContainer->size(), 0);
    // reset all event counters
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        cInterface->ResetEventCounter();
        cBrdEvntCntrs[cBoard->getIndex()] = cInterface->GetEventCounter();
    }

    // start triggers on all boards
    for(auto cBoard: *fDetectorContainer) { fBeBoardInterface->Start(cBoard); }

    // wait until you have all events from all boards
    // exit condition for this run
    bool   cAllFinished = false;
    size_t cWaitCounter = 0;
    LOG(DEBUG) << BOLDBLUE << fMyName << ":Main thread - starting to wait for events to be readout.." << RESET;
    do
    {
        size_t cNFinished = 0;
        for(auto cBoard: *fDetectorContainer)
        {
            fBeBoardInterface->setBoard(cBoard->getId());
            auto cInterface                   = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
            cBrdEvntCntrs[cBoard->getIndex()] = cInterface->GetEventCounter();
            if(cWaitCounter % 1000 == 0)
                LOG(DEBUG) << BOLDBLUE << fMyName << ":Main thread ... read-back " << cBrdEvntCntrs[cBoard->getIndex()] << " events from BeBoard#" << +cBoard->getId() << RESET;
            // finished if I've received all events or if someone else has stopped triggers for me
            if(cBrdEvntCntrs[cBoard->getIndex()] >= fNevents || cInterface->GetTriggerState() == 0)
            {
                LOG(DEBUG) << BOLDBLUE << fMyName << ":Main thread ... finished collecting all requested events from BeBoard" << +cBoard->getId() << RESET;
                cNFinished++;
            }
        }
        cWaitCounter++;
        cAllFinished = (cNFinished == fDetectorContainer->size());
    } while(!cAllFinished);

    // make double sure that all triggers have
    // been stopped here
    // stop triggers on all boards
    for(auto cBoard: *fDetectorContainer) { fBeBoardInterface->Stop(cBoard); }

    // wait for all threads to finish
    // this will just do in sequence for all boards
    for(auto cBoard: *fDetectorContainer)
    {
        // launch threads for continuous readout
        cReadoutThreads[cBoard->getIndex()].join(); // pauses until first finishes
    }
}
// continuous readout
// this will continue to read data until the stop triggers command
// has been reached
void OTTool::ContinousReadout(BeBoard* pBoard)
{
    // get D19cFW Interface
    fBeBoardInterface->setBoard(pBoard->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    LOG(INFO) << BOLDBLUE << fMyName << "::ContinousReadout ... until I've received " << fNevents << " events in the readout" << RESET;
    std::vector<uint32_t> cCompleteData(0);
    // stop triggers
    fBeBoardInterface->Start(pBoard);
    fEventCounter                = 0;
    size_t              cCounter = 0;
    bool                cBreak   = false;
    bool                cWait    = false;
    std::vector<size_t> cTriggerCounters(0);
    do
    {
        // check state of triggers FSM
        if(cInterface->GetTriggerState() != 1)
        {
            fBeBoardInterface->Stop(pBoard);
            fBeBoardInterface->Start(pBoard);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(fReadoutPause));
        auto                  cTriggerCounter = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
        std::vector<uint32_t> cData(0);
        fEventCounter += ReadData(pBoard, cData, cWait);
        if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
        cTriggerCounters.push_back(cTriggerCounter);
        if(cCounter % 200 == 0 && cCounter > 0)
        { LOG(INFO) << BOLDMAGENTA << "BeamTestCheck2S continuousReadout loop ... " << +cTriggerCounters[cTriggerCounters.size() - 1] << " triggers received" << RESET; }
        cCounter++;
        cBreak = (fEventCounter >= fNevents);
    } while(!cBreak);
    fBeBoardInterface->Stop(pBoard);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<uint32_t> cData(0);
    fEventCounter += ReadData(pBoard, cData, cWait);
    if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
    DecodeData(pBoard, cCompleteData, fEventCounter, fBeBoardInterface->getBoardType(pBoard));
    LOG(INFO) << BOLDYELLOW << fMyName << " : Mean trigger rate is " << cTriggerCounters[cTriggerCounters.size() - 1] / (cCounter * fReadoutPause * 1e-6) << " Hz"
              << " .... readout " << fEventCounter << RESET;
}
void OTTool::ContinousReadoutTh(uint8_t cBrdId)
{
    LOG(DEBUG) << BOLDBLUE << fMyName << ":Starting continuous readout thread for BeBoard#" << +cBrdId << RESET;
    // get D19cFW Interface
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBrdId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBrdId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    // wait until triggers have started
    size_t cWaitCounter = 0;
    size_t cMaxWait     = 10000;
    do
    {
        std::this_thread::sleep_for(std::chrono::microseconds(fThreadWait));
        if(cWaitCounter % 100 == 0) LOG(DEBUG) << BOLDBLUE << "\t\t" << fMyName << ":Waiting for triggers to start on BeBoard#" << +cBrdId << RESET;
        cWaitCounter++;
    } while(cInterface->GetTriggerState() != 1 && cWaitCounter < cMaxWait);

    if(cWaitCounter == cMaxWait)
    {
        LOG(INFO) << BOLDRED << "Triggers not started on this board.. start them myself!" << RESET;
        cInterface->Start();
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(fThreadWait));
            LOG(INFO) << BOLDRED << " ... waiting  for triggers to start... " << RESET;
        } while(cInterface->GetTriggerState() == 0);
    }

    LOG(DEBUG) << BOLDBLUE << fMyName << ":Triggers started .. now polling readout.." << RESET;
    std::vector<uint32_t> cCompleteData(0);
    bool                  cWait = false;
    std::vector<size_t>   cTriggerCounters(0);
    // repeat until triggers have stopped
    cWaitCounter        = 0;
    size_t cLclEvntCntr = 0;
    do
    {
        std::this_thread::sleep_for(std::chrono::microseconds(fReadoutPause));
        auto                  cTriggerState   = cInterface->GetTriggerState();
        auto                  cTriggerSource  = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.fast_command_block.trigger_source");
        auto                  cTriggerCounter = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_stat.fast_command_block.trigger_in_counter");
        std::vector<uint32_t> cData(0);
        cLclEvntCntr += ReadData((*cBoardIter), cData, cWait);
        if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
        cTriggerCounters.push_back(cTriggerCounter);
        if(cWaitCounter % 100 == 0 && cWaitCounter > 0)
        {
            LOG(DEBUG) << BOLDBLUE << "\t\t" << fMyName << ":Waiting for triggers to be stopped on BeBoard#" << +cBrdId << " continuousReadout loop ... "
                       << +cTriggerCounters[cTriggerCounters.size() - 1] << " triggers received"
                       << " trigger source is " << +cTriggerSource << " trigger state is " << +cTriggerState << " and " << cLclEvntCntr << " events in the readout so far ... " << RESET;
        }
        cWaitCounter++;
    } while(cInterface->GetTriggerState() == 1);
    // now decode data
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<uint32_t> cData(0);
    cLclEvntCntr += ReadData((*cBoardIter), cData, cWait);
    if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
    DecodeData((*cBoardIter), cCompleteData, cLclEvntCntr, fBeBoardInterface->getBoardType((*cBoardIter)));
    LOG(DEBUG) << BOLDYELLOW << fMyName << " : Mean trigger rate is " << cTriggerCounters[cTriggerCounters.size() - 1] / (cWaitCounter * fReadoutPause * 1e-6) << " Hz"
               << " .... readout " << cLclEvntCntr << " events from the FC7" << RESET;
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
    cEvntHeader << "Event#" << +pEvent->GetEventCount(); 
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto              cL1IdCIC  = static_cast<D19cCic2Event*>(pEvent)->L1Id(cHybrid->getId(), 0);
            auto              cL1Status = static_cast<D19cCic2Event*>(pEvent)->L1Status(cHybrid->getId());
            auto              cBxId     = (pEvent)->BxId(cHybrid->getId());
            auto              cStubStat = static_cast<D19cCic2Event*>(pEvent)->Status(cHybrid->getId());
            std::stringstream cOutStubs;
            std::stringstream cOutL1;
            std::stringstream cOutAna;
            std::stringstream cOutEvntHeader; 
            if(cStubStat != 0x00 || cL1Status != 0x00)
                cOutEvntHeader << cEvntHeader.str() << BOLDRED << " L1Id " << +cL1IdCIC << " Stub status is " << std::bitset<8>(cStubStat) << " L1 status [FEs] is " << std::bitset<8>(cL1Status)
                     << " L1 status [CIC] is " << std::bitset<1>(cL1Status & 0x1) << " BxId is " << +cBxId ;
            else
                cOutEvntHeader << cEvntHeader.str() << BOLDGREEN << " L1Id " << +cL1IdCIC << " Stub status is " << std::bitset<8>(cStubStat) << " L1 status [FEs] is " << std::bitset<8>(cL1Status)
                     << " L1 status [CIC] is " << std::bitset<1>(cL1Status & 0x1) << " BxId is " << +cBxId ;
            
            bool cStubFound=false;
            bool cClusterFound=false;
            bool cAna=false;
            for(auto cChip: *cHybrid)
            {
                if(!cSparsified) break;
                if(cChip->getFrontEndType() == FrontEndType::SSA ) continue;

                auto  cStubs   = pEvent->StubVector(cHybrid->getId(), cChip->getId());
                if(cChip->getFrontEndType() == FrontEndType::CBC3 )
                {
                    auto cClusters = (pEvent)->getClusters(cHybrid->getId(), cChip->getId());
                    cOutL1 << BOLDBLUE << "\t..ROC#" << +cChip->getId() << " has " << +cClusters.size() << " clusters." << RESET;
                }
                else 
                {
                    auto cStripClusters = static_cast<D19cCic2Event*>(pEvent)->GetStripClusters(cHybrid->getId(), cChip->getId());
                    auto cPxlClusters = static_cast<D19cCic2Event*>(pEvent)->GetPixelClusters(cHybrid->getId(), cChip->getId());
                    if( cStubs.size() > 0 )
                    {
                        cStubFound=true;
                        cOutStubs << BOLDYELLOW << "\t..ROC#" << +cChip->getId() << " has " 
                            << +cStubs.size() << " stubs" 
                            << "\t : ";
                        uint8_t cIndx=0;
                        for(auto cStub : cStubs )
                        {
                            cOutStubs << " Stub#" << +cIndx << " : Seed " << +cStub.getPosition() 
                                 << " Row " << +cStub.getRow() 
                                 << " Bend " << +cStub.getBend()
                                 << " \t";
                            cIndx++;
                        }
                    }
                    if( cStripClusters.size() > 0 &&  cPxlClusters.size() > 0 )
                    {
                        cClusterFound=true;
                        cOutL1 << BOLDBLUE << "\t..ROC#" << +cChip->getId() << " has " 
                            << +cStripClusters.size() << " S-clusters and "
                            << +cPxlClusters.size() << " P-clusters\t"; 
                        for( auto cPxlCluster : cPxlClusters )
                        {
                            cOutL1 << BOLDMAGENTA << "\tP-Address " << +cPxlCluster.fAddress << " P-Position " << +cPxlCluster.fZpos << " P_Width " << +cPxlCluster.fWidth << ";";
                        }
                    
                        for( auto cStripCluster : cStripClusters )
                        {
                            cOutL1 << BOLDGREEN << "\tS-Address " << +cStripCluster.fAddress << " S_Width " << +cStripCluster.fWidth << ";";
                        }
                    }
                    if( cStripClusters.size() == 1 &&  cPxlClusters.size() == 1 )
                    { 
                        cAna = true;
                        float cCenterOfMassP=0; 
                        for( auto cPxlCluster : cPxlClusters )
                        {
                            cCenterOfMassP += cPxlCluster.fAddress + cPxlCluster.fWidth; 
                        }
                        cCenterOfMassP /= cPxlClusters.size();

                        float cCenterOfMassS=0; 
                        for( auto cStripCluster : cStripClusters )
                        {
                            cCenterOfMassS += cStripCluster.fAddress + cStripCluster.fWidth; 
                        }
                        cCenterOfMassS /= cStripClusters.size();
                    
                        cOutAna << BOLDYELLOW << "\t..ROC#" << +cChip->getId() << " has " 
                            << +cStripClusters.size() << " S-clusters and "
                            << +cPxlClusters.size() << " P-clusters\t"
                            << "..Center of mass P : " << cCenterOfMassP << " Center of mass S : " << cCenterOfMassS 
                            << " # of stubs is " << cStubs.size()
                            << "\n";
                    }
                }
            }
            if( (cClusterFound || cStubFound) && pEvent->GetEventCount() < 10  ) 
                LOG (INFO) << cOutEvntHeader.str() << RESET; 
            if( (cClusterFound && cStubFound) && pEvent->GetEventCount() < 10 )
            {
                LOG(INFO) << BOLDGREEN << " Clusters and stubs in the same event " << RESET;
                LOG(INFO) << cOutStubs.str() << RESET;
                LOG(INFO) << cOutL1.str() << RESET;
            }
            if( (cClusterFound && !cStubFound) && pEvent->GetEventCount() < 10 )
            {
                LOG(INFO) << BOLDRED << " Clusters and no stubs in the same event " << RESET;
                LOG(INFO) << cOutL1.str() << RESET;
            }
            if( (!cClusterFound && cStubFound) && pEvent->GetEventCount() < 10 )
            {
                LOG(INFO) << BOLDRED << " Stubs but no Clusters in the same event " << RESET;
                LOG(INFO) << cOutStubs.str() << RESET;
            }
            if( cAna && pEvent->GetEventCount() < 10 ) 
            {
                LOG(INFO) << BOLDYELLOW << " Event with exactly one cluster in both sensors " << RESET;
                LOG(INFO) << cOutAna.str() << RESET;
            }
        }
    }
}

//
void OTTool::InjectPattern(BeBoard* pBoard, std::vector<Injection> pInjections, int pChipId)
{
    bool cInjectAll=false;
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
                LOG(DEBUG) << BOLDMAGENTA << "Injecting patterns in ROC#" << +cChip->getId() << RESET;
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
                            if( cInjectAll ) continue;
                            auto cPxl = cInjection.fColumn * NSSACHANNELS + (uint32_t)cInjection.fRow;
                            LOG(INFO) << BOLDBLUE << " Injecting in pixel " << +cPxl << " column " << +cInjection.fColumn << " row " << +cInjection.fRow << RESET;
                            std::stringstream cRegName;
                            cRegName << "ENFLAGS_P" << +cPxl;
                            fReadoutChipInterface->WriteChipReg(cChip, cRegName.str(), 0x5F, false);
                        }
                        if( cInjectAll ) fReadoutChipInterface->WriteChipReg(cChip,"ENFLAGS_ALL",0x5F);
                    }
                }
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    // for digi injection .. explicity disable all other strips  
                    if(pInjections.size() > 0 && fInjectionType == 0 ){ 
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
                            fReadoutChipInterface->WriteChipReg(cChip,cRegNameEn.str() ,0x8);
                            std::stringstream cRegNamePattern;
                            cRegNamePattern << "DigCalibPattern_L_S" << +cInjection.fRow;
                            fReadoutChipInterface->WriteChipReg(cChip, cRegNamePattern.str(), 0x01);
                        }
                        else
                        {
                            if( cInjectAll ) continue;
                            LOG (INFO) << BOLDGREEN << " Inection in Strip#" << +cInjection.fRow << RESET;
                            fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x08);
                            fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cInjection.fRow), 0x11);
                        }
                    }
                    if( fInjectionType == 1 && cInjectAll)
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x08);
                        fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL",0x11);
                    }
                }
            } // chip
        }     // hybrid
    }         // optica]l group
}
void OTTool::UpdateFromRegMap(BeBoard* pBoard)
{
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
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    for(uint8_t cDAC = 0; cDAC < 1; cDAC++)
                    {
                        std::stringstream cRegName;
                        cRegName << "ThDAC" << +cDAC;
                        cThreshold = cChip->getReg(cRegName.str());
                    }
                }
                LOG(INFO) << BOLDMAGENTA << "Setting threshold on ROC#" << +cChip->getId() << " to 0x" << std::hex << +cThreshold << std::dec << RESET;
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
                    LOG(INFO) << BOLDMAGENTA << "Setting StubMode regisger on ROC#" << +cChip->getId() << " to " << cMode << RESET;
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
                    LOG(INFO) << BOLDMAGENTA << "Setting HitLogicMode register on ROC#" << +cChip->getId() << " to " << cMode << RESET;
                }
                else if( cChip->getFrontEndType() == FrontEndType::SSA ) 
                {
                    auto cMode = cChip->getReg("SAMPLINGMODE_ALL");
                    fReadoutChipInterface->WriteChipReg(cChip, "SAMPLINGMODE_ALL", cMode);
                    LOG(INFO) << BOLDMAGENTA << "Setting HitLogicMode register on ROC#" << +cChip->getId() << " to " << cMode << RESET;
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
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    cRegName = "CalDAC0";
                }
                else if( cChip->getFrontEndType() == FrontEndType::SSA ) 
                {
                    cRegName = "Bias_CALDAC";
                }
                else if( cChip->getFrontEndType() == FrontEndType::CBC3 ) 
                {
                    cRegName = "MiscTestPulseCtrl&AnalogMux";
                }
                uint8_t cInjectedCharge = cChip->getReg(cRegName);
                if( cChip->getFrontEndType() == FrontEndType::CBC3 ) cInjectedCharge = (cInjectedCharge >> 6 ) & 0x3F;
                fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge" , cInjectedCharge);
                LOG(INFO) << BOLDMAGENTA << "Setting Charge injection register on ROC#" << +cChip->getId() << " to " << +cInjectedCharge << RESET;
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
                uint16_t cLatency=0;
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    cLatency = cChip->getReg("L1Offset_2_ALL") << 8 | cChip->getReg("L1Offset_1_ALL");
                }
                else if( cChip->getFrontEndType() == FrontEndType::SSA ) 
                {
                    cLatency = cChip->getReg("L1-Latency_MSB") << 8 | cChip->getReg("L1-Latency_LSB");
                }
                else if( cChip->getFrontEndType() == FrontEndType::CBC3 ) 
                {
                    //To-Do fill 
                    //cRegName = "MiscTestPulseCtrl&AnalogMux";
                }
                LOG (INFO) << BOLDYELLOW << "Setting latency on ROC#" << +cChip->getId() << " to " << cLatency << RESET;
                fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency" , cLatency);
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
                uint16_t cCut=0;
                std::string cRegName; 
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    cRegName = "HipCut_ALL";
                    cCut = cChip->getReg(cRegName);
                }
                else if( cChip->getFrontEndType() == FrontEndType::SSA ) 
                {
                    cRegName = "HIPCUT_ALL";
                    cCut = cChip->getReg(cRegName);
                }
                else if( cChip->getFrontEndType() == FrontEndType::CBC3 ) 
                {
                    cCut = cChip->getReg("HIP&TestMode");
                }
                LOG (INFO) << BOLDYELLOW << "Setting HIP register on ROC#" << +cChip->getId() << " to " << cCut << RESET;
                fReadoutChipInterface->WriteChipReg(cChip, cRegName , cCut);
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
                if( cChip->getFrontEndType() == FrontEndType::SSA ) 
                {
                    std::vector<std::string> cRegNames{"PhaseShiftClock","ClockDeskewing"}; 
                    for( auto cRegName : cRegNames )
                    {
                        auto cValueInMemory = cChip->getReg(cRegName);
                        fReadoutChipInterface->WriteChipReg(cChip,cRegName, cValueInMemory);
                    }
                }
                else if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    std::vector<std::string> cRegNames{"PhaseShift","ConfDLL"}; 
                    for( auto cRegName : cRegNames )
                    {
                        auto cValueInMemory = cChip->getReg(cRegName);
                        fReadoutChipInterface->WriteChipReg(cChip,cRegName, cValueInMemory);
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
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL" , 0x0F);
                    LOG(INFO) << BOLDMAGENTA << "Setting ENFLAGS_ALL on ROC#" << +cChip->getId() << " to enable both modes.." << RESET;
                }
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    for( size_t cIndx=0; cIndx < NSSACHANNELS ; cIndx++)
                    {
                        std::stringstream cRegName;
                        cRegName << "ENFLAGS_S" << +(cIndx+1);
                        auto cValueInMemory = cChip->getReg(cRegName.str());
                        if( (cValueInMemory & 0x1) == 0 ) // strip is masked 
                        {
                            fReadoutChipInterface->WriteChipReg(cChip,cRegName.str(), cValueInMemory);
                        }
                    }
                }

            }
        }
    }
}