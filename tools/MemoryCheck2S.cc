#include "MemoryCheck2S.h"
//#ifdef __USE_ROOT__

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "BackEndAlignment.h"
#include "Occupancy.h"
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

#include <random>

MemoryCheck2S::MemoryCheck2S() : Tool() { fRegMapContainer.reset(); }

MemoryCheck2S::~MemoryCheck2S() {}

void MemoryCheck2S::Initialise()
{
    // this is needed if you're going to use groups anywhere
    fChannelGroupHandler = new CBCChannelGroupHandler(); // This will be erased in tool.resetPointers()
    fChannelGroupHandler->setChannelGroupParameters(16, 2);

    ContainerFactory::copyAndInitStructure<ChannelList>(*fDetectorContainer, fInjections);
    ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fDataMismatches);
    ContainerFactory::copyAndInitStructure<EventsList>(*fDetectorContainer, fBadEvents);
    ContainerFactory::copyAndInitStructure<EventsList>(*fDetectorContainer, fGoodEvents);
    

    ContainerFactory::copyAndInitChip<int>(*fDetectorContainer, fHitCheckContainer);
    ContainerFactory::copyAndInitChip<int>(*fDetectorContainer, fStubCheckContainer);

    // retreive original settings for all chips and all back-end boards
    ContainerFactory::copyAndInitChip<ChipRegMap>(*fDetectorContainer, fRegMapContainer);
    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        // 
        auto& cBoardRegNap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>(); 
        const BeBoardRegMap& cOrigRegMap = static_cast<const BeBoard*>(cBoard)->getBeBoardRegMap();
        cBoardRegNap.insert( cOrigRegMap.begin(), cOrigRegMap.end() );
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ChipRegMap& theChipMap = fRegMapContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<ChipRegMap>();
                    const ChipRegMap& theOriginalMap = static_cast<ReadoutChip*>(cChip)->getRegMap();
                    theChipMap.insert(theOriginalMap.begin(), theOriginalMap.end()); 
                }
            }
        }
    }

    zeroContainers();
}
void MemoryCheck2S::zeroContainers()
{
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cInjections = fInjections.at(cBoard->getIndex());
        auto& cMismatches = fDataMismatches.at(cBoard->getIndex());
        auto& cBadEvents  = fBadEvents.at(cBoard->getIndex());
        auto& cGoodEvents = fGoodEvents.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());
            auto& cMismatchesOpticalGroup = cMismatches->at(cOpticalGroup->getIndex());
            auto& cBadEventsOpticalGroup  = cBadEvents->at(cBoard->getIndex());
            auto& cGoodEventsOpticalGroup = cGoodEvents->at(cBoard->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
                auto& cMismatchesHybrid = cMismatchesOpticalGroup->at(cHybrid->getIndex());
                auto& cBadEventsHybrid  = cBadEventsOpticalGroup->at(cHybrid->getIndex());
                auto& cGoodEventsHybrid = cGoodEventsOpticalGroup->at(cHybrid->getIndex());
                // cBxIdsMatchesHybrid->getSummary<std::vector<uint32_t>().clear();

                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;

                    auto& cInjectionsChip = cInjectionsHybrid->at(cChip->getIndex());
                    auto& cMismatchesChip = cMismatchesHybrid->at(cChip->getIndex());
                    auto& cBadEventsChip  = cBadEventsHybrid->at(cChip->getIndex());
                    auto& cGoodEventsChip = cGoodEventsHybrid->at(cChip->getIndex());
                    //
                    cBadEventsChip->getSummary<EventsList>().clear();
                    cGoodEventsChip->getSummary<EventsList>().clear();
                    cInjectionsChip->getSummary<ChannelList>().clear();
                    cMismatchesChip->getSummary<uint32_t>() = 0;
                    fRegMapContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<ChipRegMap>() =
                    static_cast<ReadoutChip*>(cChip)->getRegMap();
                }
            }
        }
    }
    
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;

                    fHitCheckContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>()  = 0;
                    fStubCheckContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = 0;
                }
            }
        }
    }
}
uint32_t MemoryCheck2S::GenericTriggerConfig(BeBoard* pBoard, int cNrepetitions)
{
    LOG (DEBUG) << BOLDMAGENTA << "MemoryCheck2S setting TriggerConfig " << RESET;

    // n events 
    auto cSetting = fSettingsMap.find ( "Nevents" );
    uint32_t cNevents = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 100;

    // configure trigger blocks
    auto cTriggerMult = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    int cNInjectedTriggers = fNInjectedTriggers*cNrepetitions;
    cNevents = cNrepetitions*cNInjectedTriggers*(1+cTriggerMult);
    // repeat the sequence N times 
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.generic_fcmd.number_of_repetitions", cNrepetitions ) ;
    // make sure fast command duration is 0 
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control.fast_duration", 0x0);

    // make sure I accept all trgigers 
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNevents );
    
    // when using generic fast commands I want to read data 
    // so first stop the trigger sources in the FC7
    uint32_t cNWords = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.readout_block.general.words_cnt");
    uint32_t cNtriggers = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
    LOG (DEBUG) << BOLDMAGENTA << "Before stopping triggers : "
        << +cNWords << " words in the readout and "
        << +cNtriggers << " triggers in the trigger_in counter."
        << RESET;

    // this stops triggers and 
    // re-loads the configuration 
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetTriggerFSM();
    
    cNWords = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.readout_block.general.words_cnt");
    cNtriggers = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
    LOG (DEBUG) << BOLDMAGENTA << "After stopping triggers [no reset]: "
        << +cNWords << " words in the readout and "
        << +cNtriggers << " triggers in the trigger_in counter "
        << RESET;

    // make sure data handshake is disabled
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.data_handshake_enable",0x00);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // set the packet size to be exactly equal to the number we expect 
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.packet_nbr", cNevents -1 );
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // make sure this is set 
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNevents );
    std::this_thread::sleep_for(std::chrono::microseconds(10));

    // re-load configuration 
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetTriggerFSM();
    
    return cNevents;
}
void MemoryCheck2S::GenericTestPulse(int pReSync)
{
    LOG (INFO) << BOLDMAGENTA << "Injecting TP with generic FCMDs " << RESET;
    
    auto cSetting = fSettingsMap.find( "TestPulseSeparation" );
    size_t cTPdelay = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 2; 
    // length of the trigger burst 
    cSetting = fSettingsMap.find( "LengthOfBurst" );
    size_t cBurstLength= ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 1; 
    
    FCMDs cFCMDs;
    size_t fSizeGap=20;
    //std::vector<uint8_t> cFastCommands(0);
    fFastCommands.clear();
    fNInjectedTriggers=0;
    for( size_t cIndx=0; cIndx < 2*fSizeGap; cIndx++)
    {
        fFastCommands.push_back( cFCMDs.fEmpty );
    }
    size_t cReSyncSep = pReSync;
    // send a resync + BC0 
    // need this to clear the L1 counters 
    fFastCommands.push_back( cFCMDs.fClear); 
    fFastCommands.push_back( cFCMDs.fEmpty); 
    // gap until injection
    for( size_t cBx=0; cBx < cReSyncSep; cBx++)
    {
        fFastCommands.push_back( cFCMDs.fEmpty );
    }
    // inject 
    fFastCommands.push_back( cFCMDs.fTestPulse );
    // gap until L1A
    for( size_t cBx=0; cBx < cTPdelay; cBx++)
    {
        fFastCommands.push_back( cFCMDs.fEmpty );
    }
    for( size_t cBx=0; cBx < cBurstLength; cBx++)
    {
        fFastCommands.push_back( cFCMDs.fTrigger );
        fExpectedPipelineAddress.push_back( (cReSyncSep+cBx)%512); 
        fNInjectedTriggers++;
    }
    for( size_t cBx=0; cBx < 5000; cBx++)
    {
        fFastCommands.push_back( cFCMDs.fEmpty );
    }
    fTotalEventsExpected += fNInjectedTriggers;
    // fFastCommands.clear();
    // fFastCommands.insert(fFastCommands.end(), cFastCommands.begin(), cFastCommands.begin() + 0x3FFF );
}
bool MemoryCheck2S::SendGenericTestPulses(int pReSync)
{
    bool cSuccess=true;
    GenericTestPulse(pReSync);
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFCMDBram(fFastCommands);
    for( auto cBoard : *fDetectorContainer )
    {
        auto cNevents = this->GenericTriggerConfig(cBoard);
        LOG (INFO) << BOLDMAGENTA << "Expect " << +cNevents << " triggers." << RESET;
        // start generic  - ctrl signal high 
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x1);
        // stop generic  - ctrl signal low 
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x0);

        // wait until all triggers have been sent 
        uint32_t cCounter=0;
        uint32_t cNtriggers = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
        uint32_t cNWords = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            cNtriggers = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
            cNWords = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
            // print to screen
            LOG(INFO) << BOLDGREEN << "Iter#" << +cCounter << " : " 
                << +cNtriggers << " counted and "
                << +cNWords << " words in the readout"
                << RESET;
            cCounter++;
        }while( cCounter < 100 && cNtriggers < cNevents); 
        cSuccess = cSuccess && (cNtriggers >= cNevents);
    }
    return cSuccess;
}
bool MemoryCheck2S::ReadAfterGenericBlock(int pNExpected)
{
    bool cSucessReadout=true;
    LOG (INFO) << BOLDMAGENTA << "Expect to read-back " << +pNExpected << " events." << RESET;
    for( auto cBoard : *fDetectorContainer )
    {
        // trigger configuration 
        bool cSuccess=false;
        // now wait until number of words have stopped increasing 
        uint32_t cNWordsPrev = 0;
        size_t cCounter=0;
        uint32_t cNtriggers = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
        auto cNWords = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            cNWordsPrev = (cCounter == 0 ) ? 0 : cNWords;
            cNWords = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
            // print to screen
            LOG(DEBUG) << BOLDGREEN << "Iter#" << +cCounter << " : " 
                << +cNWords << " words in the readout and "
                << " previous iteration found "
                << +cNWordsPrev
                << RESET;
            cCounter++;
        }while( cCounter < 100 && (cNWordsPrev != cNWords) );

        LOG(DEBUG) << BOLDGREEN << "After " << +cCounter << " iterations: " 
                << +cNWords << " words in the readout"
                << " and "
                << +cNtriggers 
                << " triggers found by the trigger_in_counter"
                << RESET;
        
        // Read Data 
        size_t cReadBackEvents=0;
        if( cNWords > 0 )
        {
            // wait another 10 ms 
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cReadBackEvents = this->ReadData(cBoard, true);
            cSuccess = cSuccess && ( cReadBackEvents == (size_t)pNExpected ); 
        }

        if(!cSuccess)
            LOG (INFO) << BOLDRED << "Trigger in counter " << +cNtriggers
                << " and number of words in the readout is " 
                << +cReadBackEvents 
                << " .... expected both to be "
                << +pNExpected 
                << RESET;
        cSucessReadout = cSucessReadout && cSuccess;
    }
    return cSucessReadout;
}
void MemoryCheck2S::MemoryCheck2SRaw()
{
    bool cSparisfication = false;
    uint16_t cThreshold = 1000; 
    bool cWithNoise=false;
    bool cUseOffsets=false;
    bool cInjection=false;
    auto cSetting = fSettingsMap.find ( "Ntrials" );
    size_t cNtrials = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 10;
    cSetting = fSettingsMap.find( "TestPulseSeparation" );
    size_t cTPdelay = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 2; 
    cSetting = fSettingsMap.find( "LengthOfBurst" );
    size_t cBurstLength= ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 1; 
    
    size_t cDepthPipeline = 512; 
    int    cNOffsts = std::ceil(cDepthPipeline/(float)cBurstLength); 
        
    // injection 
    fDetectorDataContainer = &fExpectedOccupancy;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        bool cSparsified = cBoard->getSparsification();
        if( cSparsified ) LOG (INFO) << BOLDMAGENTA << "Sparsification on " << RESET;
        else LOG (INFO) << BOLDMAGENTA << "Sparsification off " << RESET;
        
        auto& cExpectedOccThisBoard = cExpectedOccupancy.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cExpectedOccThisOG = cExpectedOccThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cExpectedOccThisHybrid = cExpectedOccThisOG->at(cHybrid->getIndex());
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, cSparsified);
                // only 2S for now 
                // configure injection 
                for(auto cChip: *cHybrid)
                {
                    auto& cExpectedOccThisChip = cExpectedOccThisHybrid->at(cChip->getIndex());
                    if( cChip->getFrontEndType() == FrontEndType::CBC3)
                    {
                        std::vector<uint8_t> cExpectedHits(0);
                        if( !cInjection ) // all channels 
                        {
                            for( size_t cIndx=0; cIndx < cChip->size(); cIndx++)
                            {
                                cExpectedOccThisChip->getChannel<Occupancy>(cIndx).fOccupancy = 1; 
                            }
                            continue;
                        }

                        uint8_t cSeed = 10 + 2*(cChip->getId() + 1 );
                        std::vector<uint8_t> cSeeds{10};cSeeds[0]=cSeed;
                        std::vector<int>     cBends{0};
                        
                        for(size_t cIndx = 0; cIndx < cSeeds.size(); cIndx += 1)
                        {
                            auto cHitList = (static_cast<CbcInterface*>(fReadoutChipInterface))->stubInjectionPattern(cChip, cSeeds[cIndx], cBends[cIndx]);
                            LOG(INFO) << BOLDBLUE << "RoC#" << +cChip->getId() << " expect to see hits in channels : " << RESET;
                            for( auto cHit : cHitList )
                            { 
                                LOG (INFO) << BOLDMAGENTA << "\t\t.." << +cHit << RESET;
                                cExpectedOccThisChip->getChannel<Occupancy>(cHit).fOccupancy = 1; 
                            }
                        }
                        (static_cast<CbcInterface*>(fReadoutChipInterface))->injectStubs(cChip, cSeeds, cBends,  cWithNoise, cUseOffsets);
                        // if using TP injection make sure the latency is set correctly 
                    }
                }//Chip
            }//Hybrid
        }//OG
    }

            
    int cMinLatencyOffset = -1;//-10;
    int cMaxLatencyOffset = 0;//(cWithNoise) ? cMinLatencyOffset + 1 : cMinLatencyOffset + std::fabs(cMinLatencyOffset)*2; 
    for( int cLatencyOffset = cMinLatencyOffset ; cLatencyOffset < cMaxLatencyOffset ; cLatencyOffset++)
    {
        uint16_t cLatency = cTPdelay + cLatencyOffset;
        if( cLatency >= cTPdelay ) continue;

        LOG (INFO) << BOLDMAGENTA << "Latency of " << +cLatency << RESET;
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
                        fReadoutChipInterface->WriteChipReg(cChip,"TriggerLatency", cLatency);
                    }//Chip
                }//Hybrid
            }//OG
            fBeBoardInterface->ChipReSync(cBoard);
        }//Board      

        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();
        fExpectedPipelineAddress.clear();
        // send enough triggers 
        // to cover full pipeline N times 
        for( size_t cAttempt = 0 ; cAttempt < cNtrials; cAttempt++)
        {
            // generate enough fast command sequences 
            //  to cover complete pipeline  
            for( size_t cIndx=0; cIndx < (size_t)cNOffsts ; cIndx++)
            {
                int cResync = cIndx*cBurstLength;
                this->SendGenericTestPulses(cResync);
            }
            LOG (INFO) << BOLDMAGENTA << "Try to readout data from " << +fTotalEventsExpected << " triggers." << RESET;

            // read 
            this->ReadAfterGenericBlock(fTotalEventsExpected);
        }

        //
        Check();
    }//latency scan
            // // now just send triggers 
            // for(auto cBoard: *fDetectorContainer)
            // {
            //     const std::vector<Event*>& cEvents = this->GetEvents();
            //     LOG (INFO) << BOLDMAGENTA << "Read back " << +cEvents.size() << " events from BeBoard#" << +cBoard->getId() << RESET;
            //     size_t cEvntCnt=0;
            //     for(auto& cEvent: cEvents)
            //     {
            //         auto cExpectedPipelineAddress = fExpectedPipelineAddress[cEvntCnt];
            //         LOG (INFO) << BOLDMAGENTA << "Event#" << +cEvent->GetEventCount() << RESET;
            //         for(auto cOpticalGroup: *cBoard)
            //         {
            //             for(auto cHybrid: *cOpticalGroup)
            //             {
            //                 // only 2S for now 
            //                 // configure injection 
            //                 for(auto cChip: *cHybrid)
            //                 {
            //                     if( cChip->getFrontEndType() != FrontEndType::CBC3 ) continue;

            //                     auto cHits = cEvent->GetHits( cHybrid->getId(), cChip->getId()); 
            //                     auto cPipelineAddress = cEvent->PipelineAddress( cHybrid->getId(), cChip->getId());
            //                     auto cL1Id = cEvent->L1Id( cHybrid->getId(), cChip->getId());
            //                     LOG (INFO) << BOLDMAGENTA 
            //                         << "\t.. CBC#" << +cChip->getId()
            //                         << " pipeline address is " << +cPipelineAddress
            //                         << " expected pipeline address is " << +cExpectedPipelineAddress
            //                         << " L1Id is " << +cL1Id
            //                         << " : found " << +cHits.size() << " hits." << RESET;
            //                     //for( auto cHit : cHits ) LOG (INFO) << BOLDMAGENTA << "\t\t... hit in channel " << +cHit << RESET;
            //                 }//chip 
            //             }//hybrid
            //         }//OG
            //         cEvntCnt++;
            //     }// event 
            // }
        
}
// compare read back 
// against injected data 
void MemoryCheck2S::Check()
{
    DetectorDataContainer cMeasuredOccupancy;
    fDetectorDataContainer = &cMeasuredOccupancy;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cMeasuredOcThisBrd = cMeasuredOccupancy.at(cBoard->getIndex());
        auto& cExpectedOcThisBrd = fExpectedOccupancy.at(cBoard->getIndex());
        const std::vector<Event*>& cEvents = this->GetEvents();
        LOG (INFO) << BOLDMAGENTA << "Read back " << +cEvents.size() << " events from BeBoard#" << +cBoard->getId() << RESET;
        size_t cEvntCnt=0;
        for(auto& cEvent: cEvents)
        {
            auto cExpectedPipelineAddress = fExpectedPipelineAddress[cEvntCnt];
            LOG (INFO) << BOLDMAGENTA << "Event#" << +cEvent->GetEventCount() << RESET;
            for(auto cOpticalGroup: *cBoard)
            {
                auto& cMeasuredOcThisOG = cMeasuredOcThisBrd->at(cOpticalGroup->getIndex());
                auto& cExpectedOcThisOG = cExpectedOcThisBrd->at(cOpticalGroup->getIndex());
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cMeasuredOcThisHybrd = cMeasuredOcThisOG->at(cHybrid->getIndex());
                    auto& cExpectedOcThisHybrd = cExpectedOcThisOG->at(cHybrid->getIndex());
                    // only 2S for now 
                    for(auto cChip: *cHybrid)
                    {
                        if( cChip->getFrontEndType() != FrontEndType::CBC3 ) continue;

                        auto& cMeasuredOcThisChip = cMeasuredOcThisHybrd->at(cChip->getIndex());
                        auto& cExpectedOcThisChip = cExpectedOcThisHybrd->at(cChip->getIndex());
                        // 
                        auto cHits = cEvent->GetHits( cHybrid->getId(), cChip->getId()); 
                        auto cPipelineAddress = cEvent->PipelineAddress( cHybrid->getId(), cChip->getId());
                        auto cL1Id = cEvent->L1Id( cHybrid->getId(), cChip->getId());
                        
                        for( auto cHit : cHits ) 
                        {
                            cMeasuredOcThisChip->getChannel<Occupancy>(cHit).fOccupancy++;
                        }


                        LOG (INFO) << BOLDMAGENTA 
                            << "\t.. CBC#" << +cChip->getId()
                            << " pipeline address is " << +cPipelineAddress
                            << " expected pipeline address is " << +cExpectedPipelineAddress
                            << " L1Id is " << +cL1Id
                            << " : found " << +cHits.size() << " hits." << RESET;
                        //for( auto cHit : cHits ) LOG (INFO) << BOLDMAGENTA << "\t\t... hit in channel " << +cHit << RESET;
                    }//chip 
                }//hybrid
            }//OG
            cEvntCnt++;
        }// event 
    }
}
void MemoryCheck2S::MemoryCheck2SSparse()
{
    bool cSparsified = true;
    uint16_t cThreshold = 1000; 
    
    bool cWithNoise=false;
    bool cUseOffsets=false;
    auto cSetting = fSettingsMap.find ( "Ntrials" );
    size_t cNtrials = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 10;
    cSetting = fSettingsMap.find( "TestPulseSeparation" );
    size_t cTPdelay = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 2; 
    cSetting = fSettingsMap.find( "LengthOfBurst" );
    size_t cBurstLength= ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 1; 
    
    size_t cDepthPipeline = 512; 
    int    cNOffsts = std::ceil(cDepthPipeline/(float)cBurstLength); 
    
    // here have to be careful 
    // because I can only look at a maximum of 32 clusters at a time per CBC 
    // but .. you can do it in groups of TPs 
    for( size_t cGroup=0; cGroup < 1; cGroup++)
    {
        // prepare injections 
        // first figure out seeds 
        std::vector<uint8_t> cSeeds{10}; cSeeds.clear(); 
        std::vector<int> cBends{0}; cBends.clear();
        for( size_t cIndx=0; cIndx < 2; cIndx++)
        {
            int cChannel = cGroup*2 + 1 + 16*cIndx; 
            int cStrip = 2*(1 + cChannel/2); 
            LOG(INFO) << BOLDBLUE << "\t.. Injecting in strip#" << cStrip << RESET;
                                
            cSeeds.push_back(cStrip);
            cBends.push_back(0);
        }// will have 16 stubs per CBC .. which is 
        // way too much for stubs but .. ok 

        // now actually prepare channel 
        // masks
        DetectorDataContainer cExpectedOccupancy;
        fDetectorDataContainer = &cExpectedOccupancy;
        ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
        for(auto cBoard: *fDetectorContainer)
        {
            bool cSparsified = cBoard->getSparsification();
            if( cSparsified ) LOG (INFO) << BOLDMAGENTA << "Sparsification on " << RESET;
            else LOG (INFO) << BOLDMAGENTA << "Sparsification off " << RESET;
            
            auto& cExpectedOccThisBoard = cExpectedOccupancy.at(cBoard->getIndex());
            for(auto cOpticalGroup: *cBoard)
            {

                auto& cExpectedOccThisOG = cExpectedOccThisBoard->at(cOpticalGroup->getIndex());
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cExpectedOccThisHybrid = cExpectedOccThisOG->at(cHybrid->getIndex());
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fCicInterface->SetSparsification(cCic, cSparsified);
                    // only 2S for now 
                    // configure injection 
                    for(auto cChip: *cHybrid)
                    {
                        auto& cExpectedOccThisChip = cExpectedOccThisHybrid->at(cChip->getIndex());
                        if( cChip->getFrontEndType() == FrontEndType::CBC3)
                        {
                            std::vector<uint8_t> cExpectedHits(0);
                            for(size_t cIndx = 0; cIndx < cSeeds.size(); cIndx += 1)
                            {
                                auto cHitList = (static_cast<CbcInterface*>(fReadoutChipInterface))->stubInjectionPattern(cChip, cSeeds[cIndx], cBends[cIndx]);
                                LOG(INFO) << BOLDBLUE << "RoC#" << +cChip->getId() << " expect to see hits in channels : " << RESET;
                                for( auto cHit : cHitList )
                                { 
                                    LOG (INFO) << BOLDMAGENTA << "\t\t.." << +cHit << RESET;
                                    cExpectedOccThisChip->getChannel<Occupancy>(cHit).fOccupancy = 1; 
                                }
                            }
                            (static_cast<CbcInterface*>(fReadoutChipInterface))->injectStubs(cChip, cSeeds, cBends,  cWithNoise, cUseOffsets);
                            // if using TP injection make sure the latency is set correctly 
                        }
                    }//Chip
                }//Hybrid
            }//OG
        }//inj over boards

        int cMinLatencyOffset = -1;//-10;
        int cMaxLatencyOffset =  0;//(cWithNoise) ? cMinLatencyOffset + 1 : cMinLatencyOffset + std::fabs(cMinLatencyOffset)*2; 
        for( int cLatencyOffset = cMinLatencyOffset ; cLatencyOffset < cMaxLatencyOffset ; cLatencyOffset++)
        {
            uint16_t cLatency = cTPdelay + cLatencyOffset;
            if( cLatency >= cTPdelay ) continue;

            LOG (INFO) << BOLDMAGENTA << "Latency of " << +cLatency << RESET;
            for(auto cBoard: *fDetectorContainer)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        for(auto cChip: *cHybrid)
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
                            fReadoutChipInterface->WriteChipReg(cChip,"TriggerLatency", cLatency);
                        }//Chip
                    }//Hybrid
                }//OG
                fBeBoardInterface->ChipReSync(cBoard);
            }//Board      

            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();
            fExpectedPipelineAddress.clear();
            // send enough trigger 
            for( size_t cAttempt = 0 ; cAttempt < cNtrials; cAttempt++)
            {
                // inject 
                for( size_t cIndx=0; cIndx < (size_t)cNOffsts ; cIndx++)
                {
                    int cResync = cIndx*cBurstLength;
                    this->SendGenericTestPulses(cResync);
                }
                LOG (INFO) << BOLDMAGENTA << "Try to readout data from " << +fTotalEventsExpected << " triggers." << RESET;
                this->ReadAfterGenericBlock(fTotalEventsExpected);
                // now just send triggers 
                for(auto cBoard: *fDetectorContainer)
                {
                    const std::vector<Event*>& cEvents = this->GetEvents();
                    LOG (INFO) << BOLDMAGENTA << "Read back " << +cEvents.size() << " events from BeBoard#" << +cBoard->getId() << RESET;
                    size_t cEvntCnt=0;
                    for(auto& cEvent: cEvents)
                    {
                        auto cExpectedPipelineAddress = fExpectedPipelineAddress[cEvntCnt];
                        LOG (INFO) << BOLDMAGENTA << "Event#" << +cEvent->GetEventCount() << RESET;
                        for(auto cOpticalGroup: *cBoard)
                        {
                            for(auto cHybrid: *cOpticalGroup)
                            {
                                // only 2S for now 
                                // configure injection 
                                for(auto cChip: *cHybrid)
                                {
                                    if( cChip->getFrontEndType() != FrontEndType::CBC3 ) continue;

                                    auto cHits = cEvent->GetHits( cHybrid->getId(), cChip->getId()); 
                                    auto cPipelineAddress = cEvent->PipelineAddress( cHybrid->getId(), cChip->getId());
                                    auto cL1Id = cEvent->L1Id( cHybrid->getId(), cChip->getId());
                                    LOG (INFO) << BOLDMAGENTA 
                                        << "\t.. CBC#" << +cChip->getId()
                                        << " pipeline address is " << +cPipelineAddress
                                        << " expected pipeline address is " << +cExpectedPipelineAddress
                                        << " L1Id is " << +cL1Id
                                        << " : found " << +cHits.size() << " hits." << RESET;
                                    //for( auto cHit : cHits ) LOG (INFO) << BOLDMAGENTA << "\t\t... hit in channel " << +cHit << RESET;
                                }//chip 
                            }//hybrid
                        }//OG
                        cEvntCnt++;
                    }// event 
                }
            }
        }// scan latency
    }
}
//
void MemoryCheck2S::writeObjects()
{
    this->SaveResults();
    fResultFile->Flush();
}
// State machine control functions
void MemoryCheck2S::Running() { Initialise(); }

void MemoryCheck2S::Stop()
{
    this->SaveResults();
    fResultFile->Flush();
    
    SaveResults();
    CloseResultFile();
    Destroy();
}

void MemoryCheck2S::Pause() {}

void MemoryCheck2S::Resume() {}