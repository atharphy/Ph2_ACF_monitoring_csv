#include "MemoryCheck2S.h"
//#ifdef __USE_ROOT__

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ThresholdAndNoise.h"
#include "Occupancy.h"
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

#include <random>

MemoryCheck2S::MemoryCheck2S() : Tool() { fRegMapContainer.reset(); }

MemoryCheck2S::~MemoryCheck2S() {}

void MemoryCheck2S::Reset()
{
    LOG(INFO) << BOLDGREEN << "Resetting registers touched  by MemoryCheck2S" << RESET;
    // set everything back to original values .. like I wasn't here
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap)
        {
            cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second));
        }
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);

        auto& cRegMapThisBoard = fRegMapContainer.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
                LOG(INFO) << BOLDBLUE << "Resetting all registers on readout chips connected to FEhybrid#" << (cHybrid->getId()) << " back to their original values..." << RESET;
                for(auto cChip: *cHybrid)
                {
                    auto&                                         cRegMapThisChip = cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>();
                    std::vector<std::pair<std::string, uint16_t>> cVecRegisters;
                    cVecRegisters.clear();
                    for(auto cReg: cRegMapThisChip) cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue));

                    // for now only reconfigure CBCs 
                    if( cChip->getFrontEndType() != FrontEndType::CBC3 ) continue;
                    fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
                }
            }
        }
    }
}
void MemoryCheck2S::ReconfigureOffsets()
{
    LOG (INFO) << BOLDMAGENTA << "\t... [MemoryCheck2S] Resetting all registers on Page1 of CBCs" << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cRegMapThisBoard = fRegMapContainer.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    if( cChip->getFrontEndType() != FrontEndType::CBC3 ) continue;
                    
                    auto&                                         cRegMapThisChip = cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>();
                    std::vector<std::pair<std::string, uint16_t>> cVecRegisters;
                    cVecRegisters.clear();
                    for(auto cReg: cRegMapThisChip)
                    {
                        // mask registers I'll do separately later
                        if( cReg.second.fPage == 1 ) 
                        {
                            cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue));
                        }
                    }
                    // for now only reconfigure CBCs 
                    fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
                }
            }
        }
    }
}

void MemoryCheck2S::Reconfigure()
{
    for( auto cBoard : *fDetectorContainer )
    {
        // reconfigure ROC registers
        // only those that I've touched 
        LOG (INFO) << BOLDMAGENTA << "\t... [MemoryCheck2S] Resetting ROC regs back to their original values" << RESET;
        auto& cRegMapThisBoard = fRegMapContainer.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    const ChipRegMap& cCurrentMap = static_cast<ReadoutChip*>(cChip)->getRegMap();
                    auto&                                         cRegMapThisChip = cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>();
                    std::vector<std::pair<std::string, uint16_t>> cVecRegisters;
                    cVecRegisters.clear();
                    //LOG (INFO) << BOLDMAGENTA << "\t.. CBC#" << +cChip->getId() << RESET;
                    for(auto cReg: cRegMapThisChip)
                    {
                        auto cIter =  cCurrentMap.find(cReg.first);
                        if( cIter->second.fValue != cReg.second.fValue ) 
                        {
                            // mask registers I'll do separately later
                            if( cIter->first.find("MaskChannel") == std::string::npos )
                            {
                                // LOG (INFO) << BOLDMAGENTA << "\t\t\t.. Register " << cReg.first 
                                //     << " was changed .. from 0x" 
                                //     << std::hex << +cIter->second.fValue << std::dec 
                                //     << " to " 
                                //     << std::hex << +cReg.second.fValue << std::dec 
                                //     << " re-writing" 
                                //     << RESET;
                                cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue));
                            }
                        }
                    }
                    // for now only reconfigure CBCs 
                    if( cChip->getFrontEndType() != FrontEndType::CBC3 ) continue;
                    fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
                }
            }
        }
        //fBeBoardInterface->ChipReSync(pBoard);
        

        // // reconfigure masks 
        auto& cMasksThisBrd = fChipMasks.at(cBoard->getIndex());
        LOG (INFO) << BOLDMAGENTA << "\t... [MemoryCheck2S] Resetting ROC masks back to their original values" << RESET;
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cMasksThisOG = cMasksThisBrd->at( cOpticalGroup->getIndex() );
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cMasksThisHybrid = cMasksThisOG->at( cHybrid->getIndex() );
                for(auto cChip: *cHybrid)
                {
                    auto& cMasksThisChip = cMasksThisHybrid->at( cChip->getIndex() );
                    auto& cOriginalMask = cMasksThisChip->getSummary<const ChannelGroup<NCHANNELS>*>();
                    // for( uint16_t cChnl=0; cChnl < cChip->size(); cChnl++)
                    // {
                    //     bool cEnabled = cOriginalMask->isChannelEnabled(cChnl);
                    //     if( cEnabled )
                    //     {  
                    //         if( cChip->getId()  == 0 ) LOG (INFO) << BOLDBLUE << "Reconfig Chnl#" << +cChnl << " enabled." << RESET;
                    //     }
                    //     else
                    //     { 
                    //         if( cChip->getId()  == 0 ) LOG (INFO) << BOLDBLUE << "Reconfig Chnl#" << +cChnl << " disabled." << RESET;
                    //     }
                    // }
                    fReadoutChipInterface->maskChannelsGroup(cChip, cOriginalMask);
                }
            }// hybrids
        }//OG

        // last thing to do 
        // is to reconfigure registers on page 1 
        // of the CBCs 
        LOG (INFO) << BOLDMAGENTA << "\t... [MemoryCheck2S] Resetting all registers on Page1 of CBCs" << RESET;
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    if( cChip->getFrontEndType() != FrontEndType::CBC3 ) continue;
                    
                    auto&                                         cRegMapThisChip = cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>();
                    std::vector<std::pair<std::string, uint16_t>> cVecRegisters;
                    cVecRegisters.clear();
                    for(auto cReg: cRegMapThisChip)
                    {
                        // mask registers I'll do separately later
                        if( cReg.second.fPage == 1 ) 
                        {
                            cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue));
                        }
                    }
                    // for now only reconfigure CBCs 
                    fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
                }
            }
        }
        //fBeBoardInterface->ChipReSync(pBoard);
    }
}
void MemoryCheck2S::Initialise()
{
    // this is needed if you're going to use groups anywhere
    initializeRecycleBin();
    fChannelGroupHandler = new CBCChannelGroupHandler(); // This will be erased in tool.resetPointers()
    fChannelGroupHandler->setChannelGroupParameters(16, 2);

    ContainerFactory::copyAndInitStructure<ChannelList>(*fDetectorContainer, fInjections);
    ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fDataMismatches);
    ContainerFactory::copyAndInitStructure<EventsList>(*fDetectorContainer, fBadEvents);
    ContainerFactory::copyAndInitStructure<EventsList>(*fDetectorContainer, fGoodEvents);
    

    ContainerFactory::copyAndInitChip<int>(*fDetectorContainer, fHitCheckContainer);
    ContainerFactory::copyAndInitChip<int>(*fDetectorContainer, fStubCheckContainer);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, fThresholds);
    
    // fDetectorDataContainer = &fExpectedOccupancy;
    // ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    

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

    // read back original masks 
    ContainerFactory::copyAndInitChip<const ChannelGroup<NCHANNELS>*>(*fDetectorContainer, fChipMasks);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cMasksThisBrd = fChipMasks.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cMasksThisOG = cMasksThisBrd->at( cOpticalGroup->getIndex() );
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cMasksThisHybrid = cMasksThisOG->at( cHybrid->getIndex() );
                for(auto cChip: *cHybrid)
                {

                    auto& cMasksThisChip = cMasksThisHybrid->at( cChip->getIndex() );
                    cMasksThisChip->getSummary<const ChannelGroup<NCHANNELS>*>() = static_cast<const ChannelGroup<NCHANNELS>*>(cChip->getChipOriginalMask());
                    // cOriginalMask = new ChannelGroup<NCHANNELS, 1>;
                    // for( uint16_t cChnl=0; cChnl < cChip->size(); cChnl++)
                    // {
                    //     bool cEnabled = cMsk->isChannelEnabled(cChnl);
                    //     if( cEnabled ){ cOriginalMask->enableChannel( cChnl );
                    //         if( cChip->getId()  == 0 ) LOG (INFO) << BOLDMAGENTA << "Chnl#" << +cChnl << " enabled." << RESET;
                    //     }
                    //     else{ 
                    //         cOriginalMask->disableChannel( cChnl );
                    //         if( cChip->getId()  == 0 ) LOG (INFO) << BOLDMAGENTA << "Chnl#" << +cChnl << " disabled." << RESET;
                    //     }
                    // }
                    //if( cChip->getFrontEndType() == FrontEndType::SSA )  cOriginalMask = new ChannelGroup<NSSACHANNELS, 1>;
                    //if( cChip->getFrontEndType() == FrontEndType::MPA )  cOriginalMask = new ChannelGroup<NSSACHANNELS, NMPACOLS>;
                    // to -do .. same for MPA where have to look over cols 
                }
            }// hybrids
        }//OG
    }
    // to be replaced with a plotting tool 
    #ifdef __USE_ROOT__
        // prepare Tree 
        for(auto cBoard: *fDetectorContainer)
        {
            TString cName = Form("ADC_Monitoring_BeBoard%d", cBoard->getId() );
            TObject* cObj  = gROOT->FindObject(cName); 
            if(cObj) delete cObj;

            TTree* cTree = new TTree(cName, "AnalogueMonitoring");
            cTree->Branch("ADC", &fADCmeasurement.fADC);
            cTree->Branch("StartTime", &fADCmeasurement.fStartTime);
            cTree->Branch("StopTime", &fADCmeasurement.fStopTime);
            cTree->Branch("LinkId", &fADCmeasurement.fLinkId);
            cTree->Branch("HybridId", &fADCmeasurement.fHybridId);
            cTree->Branch("ChipId", &fADCmeasurement.fChipId);
            cTree->Branch("VrefCorr", &fADCmeasurement.fVrefCorr);
            cTree->Branch("Scaling", &fADCmeasurement.fScaling);
            cTree->Branch("Mean", &fADCmeasurement.fMean);
            cTree->Branch("StdDev", &fADCmeasurement.fStdDev);
            cTree->Branch("Description", &fADCmeasurement.fDescription);
            this->bookHistogram(cBoard, "AnalogueTree", cTree);
            
            cName = Form("PhyPort_Monitoring_BeBoard%d", cBoard->getId() );
            cTree = new TTree(cName, "PhyPortMonitoring");
            cTree->Branch("StartTime", &fPhyPort.fStartTime);
            cTree->Branch("StopTime", &fPhyPort.fStopTime);
            cTree->Branch("HybridId", &fPhyPort.fHybridId);
            cTree->Branch("InputPort", &fPhyPort.fPort);
            cTree->Branch("InputChannel", &fPhyPort.fChannel);
            cTree->Branch("TapValue", &fPhyPort.fTap);
            this->bookHistogram(cBoard, "PhyPortTree", cTree);
            
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    cName = Form("MemTest_Cic%d", cHybrid->getId() );
                    cObj  = gROOT->FindObject(cName); 
                    if(cObj) delete cObj;

                    cTree = new TTree(cName, "MemTest");
                    cTree->Branch("Type", &fMemEvent.fType);
                    cTree->Branch("ChipId", &fMemEvent.fChipId);
                    cTree->Branch("HybridId", &fMemEvent.fHybridId);
                    cTree->Branch("TrialId",&fMemEvent.fTrial);
                    cTree->Branch("ReadoutSuccess",&fMemEvent.fReadoutSuccess);
                    //
                    cTree->Branch("StartTime", &fMemEvent.fStartTime);
                    cTree->Branch("StopTime", &fMemEvent.fStopTime);
                    //
                    cTree->Branch("EventId", &fMemEvent.fEventId);
                    cTree->Branch("L1Id", &fMemEvent.fL1Id);
                    //
                    cTree->Branch("MemoryRow", &fMemEvent.fMemoryRow);
                    cTree->Branch("MemoryColumn", &fMemEvent.fMemoryColumnExp);
                    cTree->Branch("MemoryColumnReported", &fMemEvent.fMemoryColumnRep);
                    //
                    cTree->Branch("PackageDelay", &fMemEvent.fPkgDelay);
                    //
                    cTree->Branch("TriggeredBx", &fMemEvent.fTriggeredBx);
                    cTree->Branch("TriggerNumberInBurst", &fMemEvent.fTriggerNumberInBurst);
                    //
                    cTree->Branch("Threshold", &fMemEvent.fThreshold);
                    cTree->Branch("Noise", &fMemEvent.fNoise);
                    cTree->Branch("Pedestal", &fMemEvent.fPedestal);
                    //
                    cTree->Branch("Pass", &fMemEvent.fCorrectValue);
                    this->bookHistogram(cHybrid, "MemoryCheck2STree", cTree);

                    cName = Form("Stubs_Cic%d", cHybrid->getId() );
                    cObj  = gROOT->FindObject(cName); 
                    if(cObj) delete cObj;

                    cTree = new TTree(cName, "Stubs");
                    cTree->Branch("Type", &fStubEvent.fType);
                    cTree->Branch("ChipId", &fStubEvent.fChipId);
                    cTree->Branch("HybridId", &fStubEvent.fHybridId);
                    cTree->Branch("TrialId",&fStubEvent.fTrial);
                    cTree->Branch("ReadoutSuccess",&fStubEvent.fReadoutSuccess);
                    //
                    cTree->Branch("StartTime", &fStubEvent.fStartTime);
                    cTree->Branch("StopTime", &fStubEvent.fStopTime);
                    //
                    cTree->Branch("EventId", &fStubEvent.fEventId);
                    cTree->Branch("L1Id", &fStubEvent.fL1Id);
                    //
                    cTree->Branch("MemoryRow", &fStubEvent.fMemoryRow);
                    cTree->Branch("MemoryColumn", &fStubEvent.fMemoryColumnExp);
                    cTree->Branch("MemoryColumnReported", &fStubEvent.fMemoryColumnRep);
                    //
                    cTree->Branch("TriggeredBx", &fStubEvent.fTriggeredBx);
                    cTree->Branch("TriggerNumberInBurst", &fStubEvent.fTriggerNumberInBurst);
                    //
                    cTree->Branch("Threshold", &fStubEvent.fThreshold);
                    cTree->Branch("Noise", &fStubEvent.fNoise);
                    cTree->Branch("Pedestal", &fStubEvent.fPedestal);
                    //
                    cTree->Branch("PackageDelay", &fStubEvent.fPkgDelay);
                    //
                    cTree->Branch("ExpSeed", &fStubEvent.fSeedExp);
                    cTree->Branch("ExpBend", &fStubEvent.fBendExp);
                    cTree->Branch("RepSeed", &fStubEvent.fSeedRep);
                    cTree->Branch("RepBend", &fStubEvent.fBendRep);
                    cTree->Branch("StubMatch", &fStubEvent.fMatchedStub);
                    //
                    cTree->Branch("Pass", &fStubEvent.fCorrectValue);
                    this->bookHistogram(cHybrid, "Stub2STree", cTree);

                    cName = Form("StubTest_Cic%d", cHybrid->getId() );
                    cObj  = gROOT->FindObject(cName); 
                    if(cObj) delete cObj;


                    cTree = new TTree(cName, "StubTest");
                    cTree->Branch("Type", &fStubEvent.fType);
                    cTree->Branch("ChipId", &fStubEvent.fChipId);
                    cTree->Branch("HybridId", &fStubEvent.fHybridId);
                    cTree->Branch("TrialId",&fStubEvent.fTrial);
                    cTree->Branch("ReadoutSuccess",&fStubEvent.fReadoutSuccess);
                    //
                    cTree->Branch("StartTime", &fStubEvent.fStartTime);
                    cTree->Branch("StopTime", &fStubEvent.fStopTime);
                    //
                    cTree->Branch("EventId", &fStubEvent.fEventId);
                    cTree->Branch("L1Id", &fStubEvent.fL1Id);
                    //
                    cTree->Branch("PackageDelay", &fStubEvent.fPkgDelay);
                    //
                    cTree->Branch("MemoryRow", &fStubEvent.fMemoryRow);
                    cTree->Branch("MemoryColumn", &fStubEvent.fMemoryColumnExp);
                    cTree->Branch("MemoryColumnReported", &fStubEvent.fMemoryColumnRep);
                    //
                    cTree->Branch("TriggeredBx", &fStubEvent.fTriggeredBx);
                    cTree->Branch("TriggerNumberInBurst", &fStubEvent.fTriggerNumberInBurst);
                    //
                    cTree->Branch("Threshold", &fStubEvent.fThreshold);
                    cTree->Branch("Noise", &fStubEvent.fNoise);
                    cTree->Branch("Pedestal", &fStubEvent.fPedestal);
                    //
                    cTree->Branch("ExpSeed", &fStubEvent.fSeedExp);
                    cTree->Branch("ExpBend", &fStubEvent.fBendExp);
                    cTree->Branch("RepSeed", &fStubEvent.fSeedRep);
                    cTree->Branch("RepBend", &fStubEvent.fBendRep);
                    cTree->Branch("StubMatch", &fStubEvent.fMatchedStub);
                    //
                    cTree->Branch("Pass", &fStubEvent.fCorrectValue);
                    this->bookHistogram(cHybrid, "StubCheck2STree", cTree);
                    // for(auto cChip: *cHybrid)
                    // {
                    //     if( cChip->getFrontEndType() != FrontEndType::CBC3) continue;
                    // }
                }
            }
        }
    #endif

    fPackageDelays = new DetectorDataContainer();
    ContainerFactory::copyAndInitBoard<uint8_t>(*fDetectorContainer, *fPackageDelays);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cPkgDelayThisBrd = fPackageDelays->at(cBoard->getIndex());
        auto& cPkgDelay = cPkgDelayThisBrd->getSummary<uint8_t>();
        cPkgDelay = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
    }
    fThresholdAndNoiseContainer = new DetectorDataContainer();
    ContainerFactory::copyAndInitStructure<ThresholdAndNoise>(*fDetectorContainer, *fThresholdAndNoiseContainer);
    
    const auto cTimeStart = std::chrono::system_clock::now();
    fStartTime = std::chrono::duration_cast<std::chrono::seconds>( cTimeStart.time_since_epoch()).count();
    LOG (INFO) << BOLDMAGENTA << "MemoryCheck2S::Initialise at " << fStartTime << " s from epoch." << RESET;
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
// generic analogue injections - mean trigger rate set by
// pTriggerSeparation
void MemoryCheck2S::GenericTriggers(int pTriggerSeparation, int pMaxBurstLength )
{
    // length of the trigger burst 
    // time between the resync and 
    // the first injection 
    size_t cReSyncSep=1;
    auto cSetting = fSettingsMap.find( "TestPulseSeparation" );
    size_t cTPdelay = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 2; 
    
    // random c++
    std::srand (std::time(NULL));
    std::random_device cRndm{};
    std::mt19937 cGen{cRndm()};
    std::poisson_distribution<> cDistTrigSep(pTriggerSeparation);
    std::uniform_int_distribution<int> cBurstDist(1, pMaxBurstLength);//maximum length of TP

    // LOG (INFO) << BOLDMAGENTA << "Injecting with generic FCMDs .." 
    //     << " time between ReSync + TP is  " << +cReSyncSep
    //     << " burst length is " << +cBurstLength
    //     << " mean separation between triggers is "
    //     << +pTriggerSeparation
    //     << " [ all in 40 MHz clock cycles ]"
    //     <<  RESET;
    
    FCMDs cFCMDs;
    size_t fSizeGap=20;
    fFastCommands.clear();
    fNInjectedTriggers=0;
    for( size_t cIndx=0; cIndx < 2*fSizeGap; cIndx++)
    {
        fFastCommands.push_back( cFCMDs.fEmpty );
    }
    // send a resync + BC0 
    // need this to clear the L1 counters 
    fFastCommands.push_back( cFCMDs.fClear); 
    fFastCommands.push_back( cFCMDs.fEmpty); 
    // gap until injection
    size_t cBxId=0;
    for( size_t cBx=0; cBx < cReSyncSep; cBx++)
    {
        fFastCommands.push_back( cFCMDs.fEmpty );
        cBxId++;
    }
    size_t cNtriggersToSend = 30; 
    do
    {
        // first injection 
        fFastCommands.push_back( cFCMDs.fTestPulse );
        size_t cBxWithTP = cBxId; 
        cBxId++;
        // gap until L1A
        for( size_t cBx=0; cBx < cTPdelay; cBx++)
        {
            fFastCommands.push_back( cFCMDs.fEmpty );
            cBxId++;
        }
        size_t cBurstLength= cBurstDist(cGen); // 
        for( size_t cBx=0; cBx < cBurstLength; cBx++)
        {
            fTrialCount.push_back(fTrial);
            fTriggerNumberInBurst.push_back(cBx);
            fTriggeredBxs.push_back( cBxId );
            fFastCommands.push_back( cFCMDs.fTrigger );
            fExpectedPipelineAddress.push_back( (cBxWithTP+cBx)%512); 
            fNInjectedTriggers++;
            cBxId++;
        }
        // how many clocks to wait until 
        // next trigger 
        size_t cTriggerGap = std::round(cDistTrigSep(cGen)); 
        // take the time betweent the TP
        // and the L1A into account 
        // where possible 
        if( cTriggerGap < cTPdelay ) cTriggerGap = 0 ; 
        else cTriggerGap = cTriggerGap - cTPdelay; 

        for( size_t cBx=0; cBx < cTriggerGap; cBx++)
        {
            fFastCommands.push_back( cFCMDs.fEmpty );
            cBxId++;
        }
    }while( (size_t)fNInjectedTriggers < cNtriggersToSend); 
    for( size_t cBx=0; cBx < 10; cBx++)
    {
        fFastCommands.push_back( cFCMDs.fEmpty );
        cBxId++;
    }
    fTotalEventsExpected += fNInjectedTriggers;
}
bool MemoryCheck2S::SendGenericTriggers(int pTriggerSeparation)
{
    bool cSuccess=true;
    GenericTriggers(pTriggerSeparation);
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFCMDBram(fFastCommands);
    for( auto cBoard : *fDetectorContainer )
    {
        auto cNevents = this->GenericTriggerConfig(cBoard);
        //LOG (INFO) << BOLDMAGENTA << "Expect " << +cNevents << " triggers." << RESET;
        // start generic  - ctrl signal high 
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x1);
        // stop generic  - ctrl signal low 
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x0);

        // wait until all triggers have been sent 
        uint32_t cCounter=0;
        uint32_t cNtriggers = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            cNtriggers = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
            // auto cNWords = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
            // LOG(INFO) << BOLDGREEN << "Iter#" << +cCounter << " : " 
            //     << +cNtriggers << " counted and "
            //     << +cNWords << " words in the readout"
            //     << RESET;
            cCounter++;
        }while( cCounter < 100 && cNtriggers < cNevents); 
        cSuccess = cSuccess && (cNtriggers >= cNevents);
    }
    return cSuccess;
}
// gneneric TP injection - distance between resync
// and TP set by pReSync
void MemoryCheck2S::GenericTestPulse(int pReSync)
{
    auto cSetting = fSettingsMap.find( "TestPulseSeparation" );
    size_t cTPdelay = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 2; 
    // length of the trigger burst 
    cSetting = fSettingsMap.find( "LengthOfBurst" );
    size_t cBurstLength= ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 1; 
    
    // LOG (INFO) << BOLDMAGENTA << "Injecting TP with generic FCMDs .." 
    //     << " time between ReSync + TP is  " << +pReSync
    //     << " burst length is " << +cBurstLength
    //     <<  RESET;
    
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
    size_t cBxId=0;
    for( size_t cBx=0; cBx < cReSyncSep; cBx++)
    {
        fFastCommands.push_back( cFCMDs.fEmpty );
        cBxId++;
    }
    // inject 
    fFastCommands.push_back( cFCMDs.fTestPulse );
    size_t cBxWithTP = cBxId; 
    // gap until L1A
    for( size_t cBx=0; cBx < cTPdelay; cBx++)
    {
        fFastCommands.push_back( cFCMDs.fEmpty );
        cBxId++;
    }
    for( size_t cBx=0; cBx < cBurstLength; cBx++)
    {
        fTrialCount.push_back(fTrial);
        fTriggerNumberInBurst.push_back(cBx);
        fFastCommands.push_back( cFCMDs.fTrigger );
        fTriggeredBxs.push_back( cBxId );
        fExpectedPipelineAddress.push_back( (cBxWithTP+cBx)%512); 
        fNInjectedTriggers++;
        cBxId++;
    }
    for( size_t cBx=0; cBx < 5000; cBx++)
    {
        fFastCommands.push_back( cFCMDs.fEmpty );
        cBxId++;
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
        //LOG (INFO) << BOLDMAGENTA << "Expect " << +cNevents << " triggers." << RESET;
        // start generic  - ctrl signal high 
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x1);
        // stop generic  - ctrl signal low 
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x0);

        // wait until all triggers have been sent 
        uint32_t cCounter=0;
        uint32_t cNtriggers = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            cNtriggers = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
            // auto cNWords = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
            // print to screen
            // LOG(INFO) << BOLDGREEN << "Iter#" << +cCounter << " : " 
            //     << +cNtriggers << " counted and "
            //     << +cNWords << " words in the readout"
            //     << RESET;
            cCounter++;
        }while( cCounter < 100 && cNtriggers < cNevents); 
        cSuccess = cSuccess && (cNtriggers >= cNevents);
    }
    return cSuccess;
}
// read after generic block 
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
        }while( cCounter < 1000 && (cNWordsPrev != cNWords) );

        LOG(INFO) << BOLDGREEN << "After " << +cCounter << " iterations: " 
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
            cSuccess = ( cReadBackEvents == (size_t)pNExpected ); 
        }

        if(!cSuccess)
            LOG (INFO) << BOLDRED << "Trigger in counter " << +cNtriggers
                << " and number of events in the readout is " 
                << +cReadBackEvents 
                << " .... expected both to be "
                << +pNExpected 
                << RESET;
        cSucessReadout = cSucessReadout && cSuccess;
    }
    return cSucessReadout;
}
// evaluate pedestal and noise 
void MemoryCheck2S::EvaluatePedeNoise(int pNevents, int pScanRange)
{
    bool cSparisfication=false;
    DetectorDataContainer cSparsBoards; 
    ContainerFactory::copyAndInitBoard<uint8_t>(*fDetectorContainer, cSparsBoards);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cBeBoardSpars   = cSparsBoards.at(cBoard->getIndex());
        auto& cSparsified = cBeBoardSpars->getSummary<uint8_t>();
        cSparsified = (uint8_t)( cBoard->getSparsification() );
        if( cBeBoardSpars ) LOG (INFO) << BOLDMAGENTA << "Sparsification on " << RESET;
        else LOG (INFO) << BOLDMAGENTA << "Sparsification off " << RESET;
        
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", cSparisfication);
        cBoard->setSparsification( (cSparisfication != 0 ) ); 
        if( cSparisfication ) LOG (INFO) << BOLDMAGENTA << "\t... Sparsification now on " << RESET;
        else LOG (INFO) << BOLDMAGENTA << "\t... Sparsification now off " << RESET;
        
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, cSparisfication);
            }
        }
    }
    float cOccupancyAtPedestal = 0.56; 
    LOG (INFO) << BOLDMAGENTA << "MemoryCheck2S - Measure pedestal and noise" << RESET;
    DetectorDataContainer cOccupancyContainer;
    fDetectorDataContainer = &cOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    this->bitWiseScan("VCth", pNevents, cOccupancyAtPedestal);
    // save thresholds to container 
    std::vector<float> cPedestals(0);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                LOG (INFO) << BOLDMAGENTA << "Hybrid#" << +cFe->getId() << RESET;
                for(auto cROC: *cFe)
                {
                    if( cROC->getFrontEndType() != FrontEndType::CBC3) continue;

                    auto cThreshold = fReadoutChipInterface->ReadChipReg(cROC,"Threshold");
                    LOG (INFO) << BOLDMAGENTA << "\t... CBC#" << +cROC->getId() 
                        << " threshold after bit-wise scan is "
                        << cThreshold 
                        << " DAC units"
                        << RESET;
                    cPedestals.push_back( cThreshold );
                }
            }
        }
    }
    auto cPedestalStats = SummarizeStats<float>( cPedestals );
    LOG (INFO) << BOLDMAGENTA << "Mean pedestal on all chips is " << cPedestalStats.fMean 
        << " - RMS is " << cPedestalStats.fStdDev
        << " - minimum value is " << cPedestalStats.fMin 
        << " - maximum value is " << cPedestalStats.fMax 
        << RESET;

    // now scan around threshold and calculate pedestal and noise 
    std::vector<DetectorDataContainer*>      cScanData;
    ContainerRecycleBin<Occupancy>       cRecyclingBin;
    cRecyclingBin.setDetectorContainer(fDetectorContainer);
    for(auto cContainer: cScanData) cRecyclingBin.free(cContainer);
    cScanData.clear();
    
    std::vector<uint16_t> cThresholds(0);
    int cStartValue  = cPedestalStats.fMin; 
    int   cMinOffset  = -1*pScanRange;
    int   cMaxOffset  =  1*pScanRange;
    LOG (INFO) << BOLDMAGENTA << "Scan threshold between " 
        << cStartValue + cMinOffset
        << " and "
        << cStartValue + cMaxOffset
        << " VCth units to identify "
        << " pedestal and noise on this FE object."
        << RESET;
    
    for( int cOffset=cMinOffset ; cOffset<=cMaxOffset; cOffset++)
    {
        cScanData.push_back(cRecyclingBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy()));
        cThresholds.push_back( (uint16_t)(cStartValue + cOffset) );
    }
    
    // scan and record occupancy 
    this->enableTestPulse(false);
    this->SetTestAllChannels(true);
    this->fMaskChannelsFromOtherGroups = true;
    LOG (INFO) << BOLDMAGENTA << "Scanning threshold ... " 
        << (2*pScanRange)
        << " points around mean pedestal value "
        << " across all chips on this FE object"
        << RESET;
    this->scanDac("Threshold", cThresholds, pNevents, cScanData);

    for(auto cBoard: *fDetectorContainer)
    {
        auto &cThNoiseThisBrd = fThresholdAndNoiseContainer->at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto &cThNoiseThisOG = cThNoiseThisBrd->at(cOpticalGroup->getIndex());
            for(auto cFe: *cOpticalGroup)
            {
                auto &cThNoiseThisFE = cThNoiseThisOG->at(cFe->getIndex());
                for(auto cROC: *cFe)
                {
                    if( cROC->getFrontEndType() != FrontEndType::CBC3) continue;

                    auto &cThNoiseThisChip = cThNoiseThisFE->at(cROC->getIndex());
                    std::vector<float> cPedestalsThisROC(0);
                    std::vector<float> cNoiseThisROC(0);
                    for(size_t cChnl=0; cChnl < cROC->size(); cChnl++)
                    {
                        // S-curve for this channel 
                        std::vector<float> cW(cThresholds.size(),0);
                        std::vector<float> cV(cThresholds.size(),0);
                        for(size_t cIndx=0; cIndx < cThresholds.size(); cIndx++)
                        {
                            auto& cDataThisBrd = cScanData[cIndx]->at(cBoard->getIndex());
                            auto& cDataThisOG = cDataThisBrd->at(cOpticalGroup->getIndex());
                            auto& cDataThisHybrd = cDataThisOG->at(cFe->getIndex());
                            auto& cDataThisChip = cDataThisHybrd->at(cROC->getIndex());
                            cW[cIndx] = cDataThisChip->getChannel<Occupancy>(cChnl).fOccupancy;
                            cV[cIndx]= cThresholds[cIndx];
                        }
                        auto cPedeNoise = evalNoise(cW, cV,true);
                        cThNoiseThisChip->getChannel<ThresholdAndNoise>(cChnl).fThreshold = cPedeNoise.first;
                        cThNoiseThisChip->getChannel<ThresholdAndNoise>(cChnl).fNoise = cPedeNoise.second;
                        cPedestalsThisROC.push_back( cThNoiseThisChip->getChannel<ThresholdAndNoise>(cChnl).fThreshold );
                        cNoiseThisROC.push_back( cThNoiseThisChip->getChannel<ThresholdAndNoise>(cChnl).fNoise );
                        // if( cChnl%25 == 0 )
                        //     LOG (INFO) << BOLDMAGENTA << "\t\t... channel#" << +cChnl
                        //         << " pedestal is " << cThNoiseThisChip->getChannel<ThresholdAndNoise>(cChnl).fThreshold
                        //         << " noise is " << cThNoiseThisChip->getChannel<ThresholdAndNoise>(cChnl).fNoise
                        //         << RESET;
                    }//chnl loop 
                }
            }
        }
    }

    // reset sparse 
    LOG (INFO) << BOLDMAGENTA << "Re-setting sparisfication after evaluating pedestal and noise ." << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cBeBoardSpars = cSparsBoards.at(cBoard->getIndex())->getSummary<uint8_t>();
        cBoard->setSparsification((cBeBoardSpars!=0));
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", cBeBoardSpars);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, cBeBoardSpars);
            }
        }
    }
}
// set threshold to something sensible away from the pedestal 
// distance from pedestal is pSigma*square sum ( NOISE , RMS NOISE )
void MemoryCheck2S::SetThreshold( float pSigma )
{
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cThThisBoard = fThresholds.at(cBoard->getIndex());
        auto &cThNoiseThisBrd = fThresholdAndNoiseContainer->at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cThThisOG = cThThisBoard->at(cOpticalGroup->getIndex());
            auto &cThNoiseThisOG = cThNoiseThisBrd->at(cOpticalGroup->getIndex());
            for(auto cFe: *cOpticalGroup)
            {
                auto& cThThisHybrd = cThThisOG->at(cFe->getIndex());
                auto &cThNoiseThisFE = cThNoiseThisOG->at(cFe->getIndex());
                LOG (INFO) << BOLDMAGENTA << "FE#" << +cFe->getId() << RESET;
                    
                for(auto cROC: *cFe)
                {
                    if( cROC->getFrontEndType() != FrontEndType::CBC3) continue;

                    // LOG (INFO) << BOLDMAGENTA << "\t... CBC#" << +cROC->getId() << RESET;
                    auto &cThNoiseThisChip = cThNoiseThisFE->at(cROC->getIndex());
                    std::vector<float> cPedestalsThisROC(0);
                    std::vector<float> cNoiseThisROC(0);
                    for(size_t cChnl=0; cChnl < cROC->size(); cChnl++)
                    {
                        cPedestalsThisROC.push_back( cThNoiseThisChip->getChannel<ThresholdAndNoise>(cChnl).fThreshold );
                        cNoiseThisROC.push_back( cThNoiseThisChip->getChannel<ThresholdAndNoise>(cChnl).fNoise );
                    }//chnl loop 
                    auto cPedStats = SummarizeStats<float>( cPedestalsThisROC );
                    // LOG (INFO) << BOLDMAGENTA << "\t\t... Mean pedestal on this chip is " 
                    //     << std::setprecision(2) << std::fixed 
                    //     << cPedStats.fMean 
                    //     << " - RMS is " << cPedStats.fStdDev
                    //     << " - minimum value is " << cPedStats.fMin 
                    //     << " - maximum value is " << cPedStats.fMax 
                    //     << RESET;
                    auto cNoiseStats = SummarizeStats<float>( cNoiseThisROC );
                    // LOG (INFO) << BOLDMAGENTA << "\t\t... Mean noise on this chip is " 
                    //     << std::setprecision(2) << std::fixed 
                    //     << cNoiseStats.fMean 
                    //     << " - RMS is " << cNoiseStats.fStdDev
                    //     << " - minimum value is " << cNoiseStats.fMin 
                    //     << " - maximum value is " << cNoiseStats.fMax 
                    //     << RESET;
                    float cNoise = std::sqrt( cNoiseStats.fMean*cNoiseStats.fMean + cNoiseStats.fStdDev*cNoiseStats.fStdDev ); 
                    auto& cThThisROC = cThThisHybrd->at(cROC->getIndex());
                    auto& cThresholdToSet = cThThisROC->getSummary<uint16_t>();
                    cThresholdToSet = (uint16_t)(cPedStats.fMean +  pSigma*cNoise);
                    LOG (INFO) << BOLDMAGENTA << "\t Setting threshold on CBC#" << +cROC->getId()
                        << " to "
                        << cThresholdToSet
                        << " DAC units - i.e. "
                        << std::setprecision(2) << std::fixed 
                        << cNoise
                        << " DAC units away from the pedestal"
                        << RESET;
                    fReadoutChipInterface->WriteChipReg(cROC,"Threshold", cThresholdToSet ); 
                }//ROC
            }//hybrid
        }//OG
    }//board

    for( auto cBoard : *fDetectorContainer )
    {
        // only those that I've touched 
        LOG (INFO) << BOLDMAGENTA << "\t... [MemoryCheck2S] Resetting CBCs regs on page1 back to their original values" << RESET;
        auto& cRegMapThisBoard = fRegMapContainer.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    if( cChip->getFrontEndType() != FrontEndType::CBC3 ) continue;
                    
                    auto&                                         cRegMapThisChip = cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>();
                    std::vector<std::pair<std::string, uint16_t>> cVecRegisters;
                    cVecRegisters.clear();
                    for(auto cReg: cRegMapThisChip)
                    {
                        // mask registers I'll do separately later
                        if( cReg.second.fPage == 1 ) 
                        {
                            cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue));
                        }
                    }
                    // for now only reconfigure CBCs 
                    fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
                }
            }
        }
        //fBeBoardInterface->ChipReSync(pBoard);
    }//make sure offsets are set correctly
}
void MemoryCheck2S::DataCheck(std::vector<uint8_t> pActiveCbcs, int pMeanTriggerSeparation, bool pAllOnes ) 
{
    fTypeOfTest=3; 
    bool cInjection=true;
    bool cAllOnes=pAllOnes; 
    fTypeOfTest += (cAllOnes) ? 0 : 1 ;
    bool cSparisfication = true;
    bool cWithNoise=!cInjection;
    bool cUseOffsets=true;
    auto cSetting = fSettingsMap.find ( "Ntrials" );
    size_t cNtrials = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 10;
    cSetting = fSettingsMap.find( "TestPulseSeparation" );
    size_t cTPdelay = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 2; 
    cSetting = fSettingsMap.find( "TriggerSeparation" );
    size_t cTriggerGap = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 500; 
    
    // sparisfication
    DetectorDataContainer cSparsBoards; 
    ContainerFactory::copyAndInitBoard<uint8_t>(*fDetectorContainer, cSparsBoards);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cBeBoardSpars   = cSparsBoards.at(cBoard->getIndex());
        auto& cSparsified = cBeBoardSpars->getSummary<uint8_t>();
        cSparsified = (uint8_t)( cBoard->getSparsification() );
        if( cBeBoardSpars ) LOG (INFO) << BOLDMAGENTA << "Sparsification on " << RESET;
        else LOG (INFO) << BOLDMAGENTA << "Sparsification off " << RESET;
        
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", cSparisfication);
        cBoard->setSparsification( (cSparisfication != 0 ) ); 
        if( cSparisfication ) LOG (INFO) << BOLDMAGENTA << "\t... Sparsification now on " << RESET;
        else LOG (INFO) << BOLDMAGENTA << "\t... Sparsification now off " << RESET;
        
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, cSparisfication);
            }
        }
    }

    // injection - hits 
    fDetectorDataContainer = &fExpectedOccupancy;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    // stubs 
    fDetectorDataContainer = &fExpectedStubs;
    ContainerFactory::copyAndInitChip<std::vector<Stub>>(*fDetectorContainer, *fDetectorDataContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cExpectdStubsThisBoard = fExpectedStubs.at(cBoard->getIndex());
        auto& cExpectedOccThisBoard = fExpectedOccupancy.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cExpectedStubsThisOG = cExpectdStubsThisBoard->at(cOpticalGroup->getIndex());
            auto& cExpectedOccThisOG = cExpectedOccThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cExpectedStubsThisHybrid = cExpectedStubsThisOG->at(cHybrid->getIndex());
                auto& cExpectedOccThisHybrid = cExpectedOccThisOG->at(cHybrid->getIndex());
                // only 2S for now 
                // configure injection 
                for(auto cChip: *cHybrid)
                {
                    auto& cExpectedStubsThisChip = cExpectedStubsThisHybrid->at(cChip->getIndex());
                    auto& cExpectedStubs = cExpectedStubsThisChip->getSummary<std::vector<Stub>>();

                    auto& cExpectedOccThisChip = cExpectedOccThisHybrid->at(cChip->getIndex());
                    if( cChip->getFrontEndType() != FrontEndType::CBC3) continue;

                    // make sure we're in OR  
                    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(cChip, "OR", true, true);

                    std::vector<uint8_t> cExpectedHits(0);
                    if( !cInjection ) // all channels 
                    {
                        for( size_t cHit=0; cHit < cChip->size(); cHit++)
                        {
                            cExpectedOccThisChip->getChannel<Occupancy>(cHit).fOccupancy = (cAllOnes) ? 1 : 0 ; 
                        }
                        continue;
                    }// with noise .. all would be on/off 

                    uint8_t cSeed = 2*(cChip->getId() + 1 );
                    std::vector<uint8_t> cSeeds{cSeed};
                    std::vector<int>     cBends{0};

                    //if( cChip->getId()%2 == 0 ) 
                    if( std::find( pActiveCbcs.begin(), pActiveCbcs.end(), cChip->getId() ) == pActiveCbcs.end() ) 
                    { cSeeds.clear(); cBends.clear(); }

                    // retrieve hit list from stubs 
                    std::vector<uint8_t> cCompleteHitList; cCompleteHitList.clear();
                    for(size_t cIndx = 0; cIndx < cSeeds.size(); cIndx += 1)
                    {
                        std::vector<uint8_t> cBendLUT = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT( cChip );
                        // each bend code is stored in this vector - bend encoding start at -7 strips,
                        // increments by 0.5 strips 
                        uint8_t              cBendCode     = cBendLUT[(cBends[cIndx] / 2. - (-7.0)) / 0.5];
                        //uint8_t cBendCode = cBendLUT[ cBends[cIndx]/2. ];
                        // bend code 
                        Stub cStub(cSeeds[cIndx], cBendCode);
                        cExpectedStubs.push_back( cStub );
                        auto cHitList = (static_cast<CbcInterface*>(fReadoutChipInterface))->stubInjectionPattern(cChip, cSeeds[cIndx], cBends[cIndx]);
                        cCompleteHitList.insert(cCompleteHitList.end(), cHitList.begin(), cHitList.end());
                    }
                    // configure occupancy
                    for( size_t cHit=0; cHit < cChip->size(); cHit++)
                    {
                        bool cHitFound = std::find(cCompleteHitList.begin(), cCompleteHitList.end(), cHit) != cCompleteHitList.end(); 
                        if( cHitFound )
                            cExpectedOccThisChip->getChannel<Occupancy>(cHit).fOccupancy = (cAllOnes ) ? 1 : 0 ; 
                        else
                            cExpectedOccThisChip->getChannel<Occupancy>(cHit).fOccupancy = 0;
                    }
                    (static_cast<CbcInterface*>(fReadoutChipInterface))->injectStubs(cChip, cSeeds, cBends,  cWithNoise, cUseOffsets);
                }//Chip
            }//Hybrid
        }//OG
    }

    int cMinLatencyOffset = -1;
    int cMaxLatencyOffset = 0;
    for( int cLatencyOffset = cMinLatencyOffset ; cLatencyOffset < cMaxLatencyOffset ; cLatencyOffset++)
    {
        uint16_t cLatency = cTPdelay + cLatencyOffset;
        if( cLatency >= cTPdelay ) continue;

        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        //fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
                        fReadoutChipInterface->WriteChipReg(cChip,"TriggerLatency", cLatency);
                    }//Chip
                }//Hybrid
            }//OG
            auto cStubOffset  = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->getStubOffset();
            int  cStubLatency = cLatency - cStubOffset;
            LOG(INFO) << BOLDBLUE << "Setting L1 latency to " << +cLatency << " and stub latency to " << +cStubLatency << RESET;
            fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
            fBeBoardInterface->ChipReSync(cBoard);
        }//Board      

        // make sure you reset offsets 
        // last thing to do 
        // is to reconfigure registers on page 1 
        // of the CBCs 
        ReconfigureOffsets();

        //const auto cStartTime = std::chrono::system_clock::now();
        // send enough triggers 
        // to cover full pipeline N times 
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();
        fExpectedPipelineAddress.clear();
        fTriggeredBxs.clear();
        fTriggerNumberInBurst.clear();
        fTrialCount.clear();
        fTotalEventsExpected=0;
        const auto cTimeStart = std::chrono::system_clock::now();
        fStartTime = std::chrono::duration_cast<std::chrono::seconds>( cTimeStart.time_since_epoch()).count();
        for( size_t cAttempt = 0 ; cAttempt < 500*cNtrials; cAttempt++)
        {
            fTrial=cAttempt;
            if( cAttempt%500 == 0 )
                LOG (INFO) << BOLDMAGENTA << "Attempt#" << +cAttempt 
                    << " of sending Iter#" << +cAttempt 
                    << " of generic triggers"
                    << RESET;
            // generate enough fast command sequences 
            //  to cover complete pipeline  
            this->SendGenericTriggers(cTriggerGap);
        }
        LOG (INFO) << BOLDMAGENTA << "Try to readout data from " << +fTotalEventsExpected << " triggers." << RESET;
        // read 
        this->ReadAfterGenericBlock(fTotalEventsExpected);
        const auto cTimeStop = std::chrono::system_clock::now();
        fStopTime = std::chrono::duration_cast<std::chrono::seconds>( cTimeStop.time_since_epoch()).count();
        Check();
    }//latency scan

    // reset sparse 
    LOG (INFO) << BOLDMAGENTA << "Re-setting sparisfication after data checker ." << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cBeBoardSpars = cSparsBoards.at(cBoard->getIndex())->getSummary<uint8_t>();
        cBoard->setSparsification((cBeBoardSpars==0)? false : true );
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", cBeBoardSpars);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, cBeBoardSpars);
            }
        }
    }
    Reconfigure();
}
void MemoryCheck2S::MemoryCheck2SRaw()
{
    // I still don't understand what is happening with the masking 
    // but ok .. 
    // this forces it to work 
    // make sure all channels are enabled 
    // for this test 
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(cChip,false); 
                }
            }
        }
    }

    fTypeOfTest=0; 
    bool cInjection=false;
    bool cSparisfication = false;
    bool cAllOnes = true;
    fTypeOfTest += (cAllOnes) ? 0 : 1; 
    uint16_t cThreshold = cAllOnes ? 1000 : 100; 
    bool cWithNoise=!cInjection;
    bool cUseOffsets=false;
    
    DetectorDataContainer cSparsBoards; 
    ContainerFactory::copyAndInitBoard<uint8_t>(*fDetectorContainer, cSparsBoards);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cBeBoardSpars   = cSparsBoards.at(cBoard->getIndex());
        auto& cSparsified = cBeBoardSpars->getSummary<uint8_t>();
        cSparsified = (uint8_t)( cBoard->getSparsification() );
        if( cBeBoardSpars ) LOG (INFO) << BOLDMAGENTA << "Sparsification on " << RESET;
        else LOG (INFO) << BOLDMAGENTA << "Sparsification off " << RESET;
        
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", cSparisfication);
        cBoard->setSparsification( (cSparisfication != 0 ) ); 
        if( cSparisfication ) LOG (INFO) << BOLDMAGENTA << "\t... Sparsification now on " << RESET;
        else LOG (INFO) << BOLDMAGENTA << "\t... Sparsification now off " << RESET;
        
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, cSparisfication);
            }
        }
    }

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
        auto& cExpectedOccThisBoard = fExpectedOccupancy.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cExpectedOccThisOG = cExpectedOccThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cExpectedOccThisHybrid = cExpectedOccThisOG->at(cHybrid->getIndex());
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
                            for( size_t cHit=0; cHit < cChip->size(); cHit++)
                            {
                                cExpectedOccThisChip->getChannel<Occupancy>(cHit).fOccupancy = cAllOnes ? 1 : 0 ; 
                            }
                            continue;
                        }

                        uint8_t cSeed = 10 + 2*(cChip->getId() + 1 );
                        std::vector<uint8_t> cSeeds{10};cSeeds[0]=cSeed;
                        std::vector<int>     cBends{0};
                        
                        for(size_t cIndx = 0; cIndx < cSeeds.size(); cIndx += 1)
                        {
                            auto cHitList = (static_cast<CbcInterface*>(fReadoutChipInterface))->stubInjectionPattern(cChip, cSeeds[cIndx], cBends[cIndx]);
                            //LOG(INFO) << BOLDBLUE << "RoC#" << +cChip->getId() << " expect to see hits in channels : " << RESET;
                            for( auto cHit : cHitList )
                            { 
                                cExpectedOccThisChip->getChannel<Occupancy>(cHit).fOccupancy = cAllOnes ? 1 : 0 ; 
                            }
                        }
                        // make sure sampled mode is used  
                        static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(cChip, "Sampled", true, true); 
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

        // make sure you reset offsets 
        // last thing to do 
        // is to reconfigure registers on page 1 
        // of the CBCs 
        ReconfigureOffsets();

        // send enough triggers 
        // to cover full pipeline N times 
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();
        fExpectedPipelineAddress.clear();
        fTotalEventsExpected=0;
        fTriggeredBxs.clear();
        fTriggerNumberInBurst.clear();
        fTrialCount.clear();
        const auto cTimeStart = std::chrono::system_clock::now();
        fStartTime = std::chrono::duration_cast<std::chrono::seconds>( cTimeStart.time_since_epoch()).count();
        // repeat full scan of memort 
        // cNtrials times 
        for( size_t cAttempt = 0 ; cAttempt < cNtrials; cAttempt++)
        {
            fTrial=cAttempt;
            LOG (INFO) << BOLDMAGENTA << "MemoryCheck2SRaw - Attempt#" << +cAttempt << RESET;
            // generate enough fast command sequences 
            //  to cover complete pipeline  
            for( size_t cIndx=0; cIndx < (size_t)cNOffsts ; cIndx++)
            {
                int cResync = cIndx*cBurstLength;
                if( cIndx%16 == 0 )
                    LOG (INFO) << BOLDMAGENTA << "\t.. Sending triggers to scan addresses from : " << +cResync
                        << " to " << cResync+cBurstLength << RESET;
                this->SendGenericTestPulses(cResync);
            }
        }
        LOG (INFO) << BOLDMAGENTA << "Try to readout data from " << +fTotalEventsExpected << " triggers." << RESET;
        // read 
        this->ReadAfterGenericBlock(fTotalEventsExpected);
        const auto cTimeStop = std::chrono::system_clock::now();
        fStopTime = std::chrono::duration_cast<std::chrono::seconds>( cTimeStop.time_since_epoch()).count();
        Check();
    }//latency scan

   
    // reset sparse 
    LOG (INFO) << BOLDMAGENTA << "Re-setting sparisfication after data checker ." << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cBeBoardSpars = cSparsBoards.at(cBoard->getIndex())->getSummary<uint8_t>();
        cBoard->setSparsification((cBeBoardSpars==0)? false : true );
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", cBeBoardSpars);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, cBeBoardSpars);
            }
        }
    }
    Reconfigure();
}
// not used 
void MemoryCheck2S::MemoryCheck2SSparse()
{
    fTypeOfTest=2; 
    bool cSparisfication = false;
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
    
    // prepare data container holding occpancy 
    // and configure sparisfication 
    // should end up with a vector where the occupancy 
    // matches what is expected for this threshold
    fDetectorDataContainer = &fExpectedOccupancy;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        bool cSparsified = cBoard->getSparsification();
        if( cSparsified ) LOG (INFO) << BOLDMAGENTA << "Sparsification on " << RESET;
        else LOG (INFO) << BOLDMAGENTA << "Sparsification off " << RESET;
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", (int)cSparisfication);
        if( cSparisfication ) LOG (INFO) << BOLDMAGENTA << "\t... Sparsification now on " << RESET;
        else LOG (INFO) << BOLDMAGENTA << "\t... Sparsification now off " << RESET;
        
        auto& cExpectedOccThisBoard = fExpectedOccupancy.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {

            auto& cExpectedOccThisOG = cExpectedOccThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cExpectedOccThisHybrid = cExpectedOccThisOG->at(cHybrid->getIndex());
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, cSparisfication);
                // only 2S for now 
                // configure injection 
                for(auto cChip: *cHybrid)
                {
                    if( cChip->getFrontEndType() != FrontEndType::CBC3) continue;

                    auto& cExpectedOccThisChip = cExpectedOccThisHybrid->at(cChip->getIndex());
                    for( size_t cHit=0; cHit < cChip->size(); cHit++)
                    { 
                        cExpectedOccThisChip->getChannel<Occupancy>(cHit).fOccupancy = (cThreshold == 1000 ) ? 1 : 0 ; 
                    }
                }//Chip
            }//Hybrid
        }//OG
    }//inj over boards

    // here have to be careful 
    // because I can only look at a maximum of 32 clusters at a time per CBC 
    // but .. you can do it in groups of TPs 
    // reset  readout before starting 
    int cMinLatencyOffset = -1;//-10;
    int cMaxLatencyOffset =  0;//(cWithNoise) ? cMinLatencyOffset + 1 : cMinLatencyOffset + std::fabs(cMinLatencyOffset)*2; 
    for( int cLatencyOffset = cMinLatencyOffset ; cLatencyOffset < cMaxLatencyOffset ; cLatencyOffset++)
    {
        uint16_t cLatency = cTPdelay + cLatencyOffset;
        if( cLatency >= cTPdelay ) continue;
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
        }//Board  - configure threshold and latency   
        
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();
        // clear vector holding expected pipeline addresses 
        fExpectedPipelineAddress.clear();
        fTotalEventsExpected=0;
        for( size_t cAttempt = 0 ; cAttempt < cNtrials; cAttempt++)
        {
            LOG (INFO) << BOLDMAGENTA << "Attempt#" << +cAttempt << RESET;    
            //const auto cStartTime = std::chrono::system_clock::now();
            for( size_t cGroup=0; cGroup < 8; cGroup++)
            {
                LOG (INFO) << BOLDMAGENTA << "Testing memory cells that belong to TP Group#" << +cGroup << RESET;
                for( size_t cChnlGroup=0; cChnlGroup < 8 ; cChnlGroup++)
                {
                    LOG (INFO) << BOLDMAGENTA << "\t..channel group" << +cChnlGroup << " [ to ensure that CIC FE cut doesn't affect the test]" << RESET;
                    size_t cStart=cChnlGroup*2;
                    // prepare injections 
                    // first figure out seeds 
                    std::vector<uint8_t> cSeeds{10}; cSeeds.clear(); 
                    std::vector<int> cBends{0}; cBends.clear();
                    for( size_t cIndx=cStart; cIndx < cStart+2; cIndx++)
                    {
                        int cChannel = cGroup*2 + 1 + 16*cIndx; 
                        int cStrip = 2*(1 + cChannel/2); 
                        //LOG(INFO) << BOLDBLUE << "\t.. Injecting in strip#" << cStrip << RESET;
                                            
                        cSeeds.push_back(cStrip);
                        cBends.push_back(0);
                    }// will have 16 stubs per CBC .. which is 
                    // way too much for stubs but .. ok 

                    for(auto cBoard: *fDetectorContainer)
                    {
                        for(auto cOpticalGroup: *cBoard)
                        {

                            for(auto cHybrid: *cOpticalGroup)
                            {
                                // only 2S for now 
                                // configure injection 
                                for(auto cChip: *cHybrid)
                                {
                                    if( cChip->getFrontEndType() == FrontEndType::CBC3)
                                    {
                                        // std::vector<uint8_t> cExpectedHits(0);
                                        // for(size_t cIndx = 0; cIndx < cSeeds.size(); cIndx += 1)
                                        // {
                                        //     auto cHitList = (static_cast<CbcInterface*>(fReadoutChipInterface))->stubInjectionPattern(cChip, cSeeds[cIndx], cBends[cIndx]);
                                        //     LOG(INFO) << BOLDBLUE << "RoC#" << +cChip->getId() << " expect to see hits in channels : " << RESET;
                                        //     for( auto cHit : cHitList )
                                        //     { 
                                        //         LOG (INFO) << BOLDMAGENTA << "\t\t.." << +cHit << RESET;
                                        //     }
                                        // }
                                        (static_cast<CbcInterface*>(fReadoutChipInterface))->injectStubs(cChip, cSeeds, cBends,  cWithNoise, cUseOffsets);
                                    }
                                }//Chip
                            }//Hybrid
                        }//OG
                    }// Board - configure masks to make hits appear in thse channels 

                    // inject 
                    for( size_t cIndx=0; cIndx < (size_t)cNOffsts ; cIndx++)
                    {
                        int cResync = cIndx*cBurstLength;
                        this->SendGenericTestPulses(cResync);
                    }
                }
            }//inj over boards 
        }
        LOG (INFO) << BOLDMAGENTA << "Try to readout data from " << +fTotalEventsExpected << " triggers." << RESET;
        // read 
        this->ReadAfterGenericBlock(fTotalEventsExpected);

        // const auto cStopTime = std::chrono::system_clock::now();
        // fStartTimeMemTest = std::chrono::duration_cast<std::chrono::seconds>( cStartTime.time_since_epoch()).count();
        // fStopTimeMemTest = std::chrono::duration_cast<std::chrono::seconds>( cStopTime.time_since_epoch()).count();
        Check();
    }    
}
void MemoryCheck2S::SaveOptimalTaps()
{
    const auto cTimeStart = std::chrono::system_clock::now();
    fPhyPort.fStartTime = (int)std::chrono::duration_cast<std::chrono::seconds>( cTimeStart.time_since_epoch()).count();
    fPhyPort.fStopTime  = fPhyPort.fStartTime;
    for(auto cBoard: *fDetectorContainer)
    {
        //auto& cVrefCorrThisBoard = fVrefCorrections.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                fPhyPort.fHybridId = cHybrid->getId();
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                auto cOptimalTaps = fCicInterface->GetOptimalTaps(cCic);
                for( size_t cPhyPortChnl = 0 ; cPhyPortChnl < 4 ; cPhyPortChnl++)
                {
                    for( size_t cPhyPort= 0 ; cPhyPort < 12 ; cPhyPort ++ )
                    {
                        fPhyPort.fPort = cPhyPort; 
                        fPhyPort.fChannel=cPhyPortChnl; 
                        fPhyPort.fTap = cOptimalTaps[cPhyPortChnl][cPhyPort];
                        #ifdef __USE_ROOT__
                            TTree*      cTree  = static_cast<TTree*>(getHist(cBoard, "PhyPortTree"));
                            cTree->Fill();
                        #endif
                    }
                }  
            }
        }
    }
}
void MemoryCheck2S::ConfigureVref()
{
    LOG (INFO) << BOLDBLUE << "Setting up Vref of lpGBT-ADC.." << RESET;
    std::vector<std::string> cADCs_VoltageMonitors{"ADC2"};
    std::vector<float>       cADCs_Refs{10.4*0.49/10.0};
    // reference volage for lpgBT 
    float cVrefLPGBT = 1.0; 
    float cFactor = cVrefLPGBT/ 1024.;

    //DetectorDataContainer* cTmp = new DetectorDataContainer();
    //ContainerFactory::copyAndInitOpticalGroup<uint8_t>(*fDetectorContainer, *cTmp);
    fVrefCorrections.clear();
    for(auto cBoard: *fDetectorContainer)
    {
        //auto& cVrefCorrThisBoard = fVrefCorrections.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT =  cOpticalGroup->flpGBT ;
            if(clpGBT == nullptr) continue;
            auto clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
            
            // use Vin as the reference 
            // this I know does not change with anything 
            size_t cIndx=cADCs_VoltageMonitors.size()-1;
            // find correction 
            std::vector<float> cVals(10,0);
            uint8_t cEnableVref=1;
            std::string cADCsel = cADCs_VoltageMonitors[cIndx];
            std::vector<uint8_t> cRefPoints{0, 0x05, 0x10, 0x20, 0x3F };
            std::vector<float> cMeasurements(0);
            std::vector<float> cSlopes(0);
            for( auto cRef : cRefPoints) 
            {
                clpGBTInterface->ConfigureVref(clpGBT, cEnableVref, cRef);
                // wait until Vref is stable
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                for(size_t cM=0; cM < cVals.size(); cM++)
                {
                    cVals[cM] = clpGBTInterface->ReadADC(clpGBT, cADCsel)*cFactor;
                }
                float cMean = std::accumulate(cVals.begin(),cVals.end(),0.)/cVals.size();
                float cDifference_V = (cADCs_Refs[cIndx] - cMean );
                // LOG (DEBUG) << BOLDBLUE << "ADC_" << cADCsel << " reading from lpGBT "
                //         << " correction applied is " << +cRef 
                //         << " reading [mean] is "
                //         << +cMean*1e3 
                //         << " milli-volts."
                //         << "\t...Difference between expected and measured "
                //         << " values is "
                //         << cDifference_V*1e3 
                //         << " milli-volts." << RESET;
                cMeasurements.push_back(cDifference_V);
                if( cMeasurements.size() > 1 ) 
                {
                    for(int cI=cMeasurements.size()-2; cI>=0; cI--)
                    {
                        float cSlope = (cMeasurements[cMeasurements.size()-1] - cMeasurements[cI])/(cRefPoints[cMeasurements.size()-1 ]-cRefPoints[cI]);
                        LOG (DEBUG) << BOLDBLUE << "Index " << +(cMeasurements.size()-1 )
                            << " -- index " << cI
                            << " slope is " << cSlope 
                            << RESET;
                        cSlopes.push_back(cSlope);
                    }
                }
            }
            float cMeanSlope = std::accumulate(cSlopes.begin(),cSlopes.end(),0.)/cSlopes.size();
            float cIntcpt = cMeasurements[0]; 
            int cCorr = std::min( std::floor(-1.0*cIntcpt/cMeanSlope), 63. );
            // LOG (INFO) << BOLDMAGENTA << "Mean slope is " << cMeanSlope 
            //     << " , intercept is " << cIntcpt 
            //     << " correction is " << cCorr
            //     << RESET;
            // apply correction and check
            //auto& cVrefCorrThisOG = cVrefCorrThisBoard->at(cOpticalGroup->getIndex());
            //auto& cVrefCorr = cVrefCorrThisOG->getSummary<uint8_t>();
            //cVrefCorr = (uint8_t)cCorr;
            fVrefCorrections.push_back( (uint8_t)cCorr ) ;
            clpGBTInterface->ConfigureVref(clpGBT, cEnableVref, (uint8_t)cCorr);
            // // wait until Vref is stable
            // std::this_thread::sleep_for(std::chrono::milliseconds(10));
            // for(size_t cM=0; cM < cVals.size(); cM++)
            // {
            //     cVals[cM] = clpGBTInterface->ReadADC(clpGBT, cADCsel)*cFactor;
            // }
            // auto cStats = SummarizeStats<float>(cVals);
            // LOG (INFO) << BOLDMAGENTA << "Measured V_min after correction is " 
            //     << std::setprecision(2) << std::fixed 
            //     << cStats.fMean*1e3
            //     << " mV , expected value is "
            //     << cADCs_Refs[cIndx]*1e3 
            //     << " difference is "
            //     << std::fabs(cStats.fMean-cADCs_Refs[cIndx])*1e3
            //     << " mV, correction needed to acheive this was  "
            //     << +cCorr
            //     << RESET;
            
            // turn off ADC mon
            //static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->WriteChipReg(clpGBT,"ADCMon", 0x00 );
        }// configure lpGBT 
    }
}
void MemoryCheck2S::MonitorInputVoltage()
{
    float cVref = 1.0; 
    float cFactor = cVref/ 1024.;
    float cVhybrid = 1.25; 
    float cScalingHybrid = (0.806/cVhybrid);
    uint16_t cTriggerRate = 100; 
    
    // make sure you do this while sending triggers
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(0, cTriggerRate, 3);
    // now measure hybrid voltages
    // this is monitored with one of the ADCs 
    // in the lpGBT  
    size_t cCounter=0;
    for(const auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->Start(cBoard);
        // this is for the hybrid voltage 
        for(auto cOpticalGroup: *cBoard)
        {
            uint8_t cVrefCorr = fVrefCorrections[cCounter];
            cCounter++;
            auto& clpGBT =  cOpticalGroup->flpGBT ;
            if(clpGBT == nullptr) continue;
            auto clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
            uint8_t cADCsel = 1;
            char cADC[4]; sprintf( cADC, "ADC%.1d", cADCsel);
            
            const auto cTimeStart = std::chrono::system_clock::now();
            fStartTime = std::chrono::duration_cast<std::chrono::seconds>( cTimeStart.time_since_epoch()).count();
            std::vector<float> cVals(10);
            for(size_t cM=0; cM < cVals.size(); cM++){ 
                cVals[cM] = clpGBTInterface->ReadADC(clpGBT, cADC)*cFactor;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            const auto cTimeStop = std::chrono::system_clock::now();
            fStopTime = std::chrono::duration_cast<std::chrono::seconds>( cTimeStop.time_since_epoch()).count();
            //
            auto cStats = SummarizeStats<float>(cVals);
            //float cMeasured = cStats.fMean/cScalingHybrid; 
            fADCmeasurement.fADC = cADCsel;
            fADCmeasurement.fStartTime = (int)fStartTime ; 
            fADCmeasurement.fStopTime = (int)fStopTime ; 
            fADCmeasurement.fHybridId = 0xFF;
            fADCmeasurement.fChipId  = 0xFF; 
            fADCmeasurement.fLinkId  = clpGBT->getId();
            fADCmeasurement.fMean = cStats.fMean; 
            fADCmeasurement.fStdDev = cStats.fStdDev;  
            fADCmeasurement.fScaling = cScalingHybrid; 
            fADCmeasurement.fVrefCorr = cVrefCorr ; 
            fADCmeasurement.fDescription = "Hybrid1v25";
            PrintADCMeasurement( fADCmeasurement );
            #ifdef __USE_ROOT__
                TTree*      cTree  = static_cast<TTree*>(getHist(cBoard, "AnalogueTree"));
                cTree->Fill();
            #endif
        }//OG
        fBeBoardInterface->Stop(cBoard);
    }//board

}
void MemoryCheck2S::MonitorTemperature()
{
    float cVref = 1.0; 
    float cFactor = cVref/ 1024.;
    
    std::vector<uint8_t> cADCsels{6,7,14};
    std::vector<std::string> cADCLbls{"TempBpol2v5","TempBpol12V","TempLpGBT"};
    for( size_t cIndx=0; cIndx < cADCsels.size(); cIndx++)
    {
        size_t cCounter=0;
        for(const auto cBoard: *fDetectorContainer)
        {
            fBeBoardInterface->Start(cBoard);
            // this is for the hybrid voltage 
            for(auto cOpticalGroup: *cBoard)
            {
                uint8_t cVrefCorr = fVrefCorrections[cCounter];
                cCounter++;
                auto& clpGBT =  cOpticalGroup->flpGBT ;
                if(clpGBT == nullptr) continue;
                auto clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
                uint8_t cADCsel = cADCsels[cIndx];
                char cADC[4]; 
                if( cADCsel == 14 ) sprintf( cADC, "TEMP");
                else sprintf( cADC, "ADC%.1d", cADCsel);

                const auto cTimeStart = std::chrono::system_clock::now();
                fStartTime = std::chrono::duration_cast<std::chrono::seconds>( cTimeStart.time_since_epoch()).count();
                std::vector<float> cVals(10);
                for(size_t cM=0; cM < cVals.size(); cM++){ 
                    cVals[cM] = clpGBTInterface->ReadADC(clpGBT, cADC)*cFactor;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                const auto cTimeStop = std::chrono::system_clock::now();
                fStopTime = std::chrono::duration_cast<std::chrono::seconds>( cTimeStop.time_since_epoch()).count();
                //
                auto cStats = SummarizeStats<float>(cVals);
                //float cMeasured = cStats.fMean/cScalingHybrid; 
                fADCmeasurement.fADC = cADCsel;
                fADCmeasurement.fStartTime = (int)fStartTime ; 
                fADCmeasurement.fStopTime = (int)fStopTime ; 
                fADCmeasurement.fHybridId = 0xFF;
                fADCmeasurement.fChipId  = 0xFF; 
                fADCmeasurement.fLinkId  = clpGBT->getId();
                fADCmeasurement.fMean = cStats.fMean; 
                fADCmeasurement.fStdDev = cStats.fStdDev;  
                fADCmeasurement.fScaling = 1; 
                fADCmeasurement.fVrefCorr = cVrefCorr ; 
                fADCmeasurement.fDescription = cADCLbls[cIndx];
                PrintADCMeasurement( fADCmeasurement );
                #ifdef __USE_ROOT__
                    TTree*      cTree  = static_cast<TTree*>(getHist(cBoard, "AnalogueTree"));
                    cTree->Fill();
                #endif
            }//OG
        }//board
    }

}
// check bandgap and voltage 
// for different distances from threshold 
void MemoryCheck2S::MonitorAnalogue()
{   
    float cVref = 1.0; 
    float cFactor = cVref/ 1024.;
    // float cVhybrid = 1.25; 
    // float cScalingHybrid = (0.806/cVhybrid);
    // uint16_t cTriggerRate = 100; 
    std::vector<uint8_t> cAmuxSels{5,6,7};
    std::vector<std::string> cAmuxLbls{"VCth","VbgBias", "VbgLDO"};
   
    size_t cLengthLoop=50; 
    DetectorDataContainer cAnBiases;
    ContainerFactory::copyAndInitChip<AdcMeasurements>(*fDetectorContainer, cAnBiases);
    for( size_t cMuxIndx=0; cMuxIndx < cAmuxSels.size(); cMuxIndx++)
    {
        LOG (INFO) << BOLDBLUE << "Monitoring ADC values when AMUXs "
                    << cAmuxLbls[cMuxIndx] 
                    << " is selected" 
                    << RESET;
        
        DetectorDataContainer cAdcMeasurements;
        ContainerFactory::copyAndInitChip<std::vector<float>>(*fDetectorContainer, cAdcMeasurements);
        DetectorDataContainer cFloatingMonitor;
        ContainerFactory::copyAndInitHybrid<std::vector<float>>(*fDetectorContainer, cFloatingMonitor);
        size_t cCounter=0;
        for(const auto cBoard: *fDetectorContainer)
        {
            auto& cBiasThisBrd = cAnBiases.at(cBoard->getIndex());
            auto& cMeasThisBrd = cAdcMeasurements.at(cBoard->getIndex());
            auto& cFloatingThisBrd = cFloatingMonitor.at(cBoard->getIndex());
            // now for the analogue monitoring 
            for(auto cOpticalGroup: *cBoard)
            {
                uint8_t cVrefCorr = fVrefCorrections[cCounter];
                cCounter++;
                auto& cBiasThisOG = cBiasThisBrd->at(cOpticalGroup->getIndex());
                auto& cMeasThisOG = cMeasThisBrd->at(cOpticalGroup->getIndex());
                auto& cFloatingThisOG = cFloatingThisBrd->at(cOpticalGroup->getIndex());
                auto& clpGBT =  cOpticalGroup->flpGBT ;
                if(clpGBT == nullptr) continue;
                auto clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
                // make sure all chips AMUX 
                // is set to floating 
                // on all hybrids 
                for( size_t cChipIndx=0; cChipIndx < 8; cChipIndx++)
                {

                    // LOG (INFO) << BOLDBLUE << "\t.. Selecting chip#"
                    //     << +cChipIndx << RESET;
                    const auto cTimeStart = std::chrono::system_clock::now();
                    fStartTime = std::chrono::duration_cast<std::chrono::seconds>( cTimeStart.time_since_epoch()).count();
    
                    // LOG (INFO) << BOLDBLUE << "\t\t..."
                    //     << " .. first make sure we are at a stable point.."
                    //     << RESET;
                    // make sure all chips are set to floating 
                    for(auto cHybrid: *cOpticalGroup)
                    {   
                        for( auto cChip : *cHybrid )
                        {
                            fReadoutChipInterface->WriteChipReg(cChip,"AmuxOutput", 0); //set to floating 
                        }
                    }
                    // allow to stabilize
                    for( size_t cMeasIndx=0; cMeasIndx < cLengthLoop*2 ; cMeasIndx++)
                    {
                        // now measure 
                        for(auto cHybrid: *cOpticalGroup)
                        {
                            auto& cFltThisHybrid = cFloatingThisOG->at(cHybrid->getIndex());
                            auto& cMeas = cFltThisHybrid->getSummary<std::vector<float>>(); 
                            if( cMeasIndx == 0 ) cMeas.clear();

                            uint8_t cADCsel = (cHybrid->getId()%2 == 0 ) ? 3 : 0 ; 
                            char cADC[4]; sprintf( cADC, "ADC%.1d", cADCsel);
                            // now wait until the output is stable 
                            float cVal  = clpGBTInterface->ReadADC(clpGBT, cADC)*cFactor;
                            cMeas.push_back( cVal );
                            // if( cMeasIndx == 0 || cMeasIndx == cLengthLoop*5 - 1 )
                            //     LOG (INFO) << BOLDYELLOW << "\t\t\t...[ all floating ] measurement#" << +cMeasIndx
                            //         << " on hybrid " << +cHybrid->getId() 
                            //         << " reading from lpGBT "
                            //         << " is "
                            //         << +cVal
                            //         << " Volts. "
                            //         << RESET;
                        }//hybrids
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    // select amux on Nth chip  
                    // LOG (INFO) << BOLDBLUE << "\t\t..."
                    //     << " .. selecting " << cAmuxLbls[cMuxIndx] 
                    //     << " on Chip#" << +cChipIndx
                    //     << RESET;
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        if( cChipIndx >= cHybrid->size() ) continue;
                        fReadoutChipInterface->WriteChipReg(cHybrid->at(cChipIndx),"AmuxOutput", cAmuxSels[cMuxIndx]); //set to floating 
                    }
                    // allow to stabilize
                    for( size_t cMeasIndx=0; cMeasIndx < cLengthLoop/10 ; cMeasIndx++)
                    {
                        // now measure 
                        for(auto cHybrid: *cOpticalGroup)
                        {
                            auto& cMeasThisHybrid = cMeasThisOG->at(cHybrid->getIndex());
                            auto& cMeasThisChip   = cMeasThisHybrid->at(cChipIndx);
                            auto& cMeas = cMeasThisChip->getSummary<std::vector<float>>(); 
                            if( cMeasIndx == 0 ) cMeas.clear(); 

                            uint8_t cADCsel = (cHybrid->getId()%2 == 0 ) ? 3 : 0 ; 
                            char cADC[4]; sprintf( cADC, "ADC%.1d", cADCsel);
                            // now wait until the output is stable 
                            cMeas.push_back( clpGBTInterface->ReadADC(clpGBT, cADC)*cFactor ) ;
                            std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }//hybrids
                    }//measurement loop 
                    // print out loop 
                    const auto cTimeStop = std::chrono::system_clock::now();
                    fStopTime = std::chrono::duration_cast<std::chrono::seconds>( cTimeStop.time_since_epoch()).count();
    
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cMeasThisHybrid = cMeasThisOG->at(cHybrid->getIndex());
                        auto& cMeasThisChip   = cMeasThisHybrid->at(cChipIndx);
                        auto& cMeas = cMeasThisChip->getSummary<std::vector<float>>(); 
                        auto cStats = SummarizeStats<float>( cMeas );
                        //
                        auto& cBiasThisHybrid = cBiasThisOG->at(cHybrid->getIndex());
                        auto& cBiasThisChip   = cBiasThisHybrid->at(cChipIndx);
                        auto& cBias = cBiasThisChip->getSummary<AdcMeasurements>(); 
                        uint8_t cADCsel = (cHybrid->getId()%2 == 0 ) ? 3 : 0 ; 
                        fADCmeasurement.fADC = cADCsel;
                        fADCmeasurement.fStartTime = (int)fStartTime ; 
                        fADCmeasurement.fStopTime = (int)fStopTime ; 
                        fADCmeasurement.fLinkId  = clpGBT->getId();
                        fADCmeasurement.fHybridId = cHybrid->getId();
                        fADCmeasurement.fChipId  = (cHybrid->at(cChipIndx))->getId(); 
                        fADCmeasurement.fMean = cStats.fMean; 
                        fADCmeasurement.fStdDev = cStats.fStdDev;  
                        fADCmeasurement.fScaling = 1; 
                        fADCmeasurement.fVrefCorr = cVrefCorr ; 
                        fADCmeasurement.fDescription = cAmuxLbls[cMuxIndx];
                        PrintADCMeasurement( fADCmeasurement );
                        cBias.push_back( fADCmeasurement ); 
                        #ifdef __USE_ROOT__
                            TTree*      cTree  = static_cast<TTree*>(getHist(cBoard, "AnalogueTree"));
                            cTree->Fill();
                        #endif
                    }
                    // return mux to floating on Nth chip   
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        if( cChipIndx >= cHybrid->size() ) continue;
                        fReadoutChipInterface->WriteChipReg(cHybrid->at(cChipIndx),"AmuxOutput", 0); //set to floating 
                    }
                }//at most 8 chips per hybrid
            }//OG 
        }//board  
    }//mux loop
}
// compare read back 
// against injected data 
void MemoryCheck2S::Check()
{
    int cDuration = fStopTime-fStartTime;
    LOG (INFO) << BOLDMAGENTA << "Running memory check .. " 
        << " test started at " << fStartTime << " s from epoch." 
        << " and finished at " << fStopTime << " s from epoch "
        << " .... so it took " << cDuration << " s."
        << RESET;
    
    DetectorDataContainer cMemEvents;
    fDetectorDataContainer = &cMemEvents;
    ContainerFactory::copyAndInitStructure<MemEvents>(*fDetectorContainer, *fDetectorDataContainer);

    DetectorDataContainer cCorruptedEvents;
    fDetectorDataContainer = &cCorruptedEvents;
    ContainerFactory::copyAndInitStructure<MemEvents>(*fDetectorContainer, *fDetectorDataContainer);
    
    size_t cNCorruptedCells=0; 
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cPkgDelayThisBrd = fPackageDelays->at(cBoard->getIndex());
        auto& cPkgDelay = cPkgDelayThisBrd->getSummary<uint8_t>();
        auto& cThThisBoard = fThresholds.at(cBoard->getIndex());
        auto& cThNoiseThisBrd = fThresholdAndNoiseContainer->at(cBoard->getIndex());
        auto& cMemEventsThisBrd = cMemEvents.at(cBoard->getIndex());
        auto& cBadEventsThisBrd = cCorruptedEvents.at(cBoard->getIndex());
        auto& cExpectedOcThisBrd = fExpectedOccupancy.at(cBoard->getIndex());
        const std::vector<Event*>& cEvents = this->GetEvents();
        LOG (INFO) << BOLDMAGENTA << "Running check on " << +cEvents.size() << " events from BeBoard#" << +cBoard->getId() << RESET;
        fReadoutSuccess = (  cEvents.size() == fTotalEventsExpected ) ? 1 : 0; 
        if( cEvents.size() != fTotalEventsExpected )
            LOG (INFO) << BOLDRED << " ... mismatch in event count .. " << RESET;

        size_t cEvntCnt=0;
        for(auto& cEvent: cEvents)
        {
            if( cEvntCnt%2500 == 0 )
                LOG (INFO) << BOLDMAGENTA << "Checking Event#" << +cEvntCnt << RESET;
            auto cExpectedPipelineAddress = fExpectedPipelineAddress[cEvntCnt];
            auto cTriggeredBx = fTriggeredBxs[cEvntCnt];
            auto cTriggerNumberInBurst = fTriggerNumberInBurst[cEvntCnt]; 
            auto cTrialNumber = fTrialCount[cEvntCnt];
            for(auto cOpticalGroup: *cBoard)
            {
                auto& cThThisOG = cThThisBoard->at(cOpticalGroup->getIndex());
                auto& cThNoiseThisOG = cThNoiseThisBrd->at(cOpticalGroup->getIndex());
                auto& cMemEventsThisOG = cMemEventsThisBrd->at(cOpticalGroup->getIndex());
                auto& cBadEventsThisOG = cBadEventsThisBrd->at(cOpticalGroup->getIndex());
                auto& cExpectedOcThisOG = cExpectedOcThisBrd->at(cOpticalGroup->getIndex());
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cThThisHybrd = cThThisOG->at(cHybrid->getIndex());
                    auto& cThNoiseThisHybrd = cThNoiseThisOG->at(cHybrid->getIndex());
                    auto& cMemEventsThisHybrd = cMemEventsThisOG->at(cHybrid->getIndex());
                    auto& cBadEventsThisHybrid = cBadEventsThisOG->at(cHybrid->getIndex());
                    auto& cExpectedOcThisHybrd = cExpectedOcThisOG->at(cHybrid->getIndex());
                    // only 2S for now 
                    for(auto cChip: *cHybrid)
                    {
                        if( cChip->getFrontEndType() != FrontEndType::CBC3 ) continue;

                        auto& cThThisChip = cThThisHybrd->at(cChip->getIndex());
                        auto& cThresholdSet = cThThisChip->getSummary<uint16_t>();
                    
                        auto& cThNoiseThisChip = cThNoiseThisHybrd->at(cChip->getIndex());
                        auto& cMemEventsThisChip = cMemEventsThisHybrd->at(cChip->getIndex());
                        auto& cMemEventsSummary  = cMemEventsThisChip->getSummary<MemEvents>();

                        auto& cBadEventsThisChip = cBadEventsThisHybrid->at(cChip->getIndex());
                        auto& cBadEventsSummary  = cBadEventsThisChip->getSummary<MemEvents>();

                        if( cEvntCnt == 0 ) cMemEventsSummary.clear(); 

                        auto& cExpectedOcThisChip = cExpectedOcThisHybrd->at(cChip->getIndex());
                        // 
                        auto cStubs = cEvent->StubVector( cHybrid->getId(), cChip->getId()); 
                        auto cHits = cEvent->GetHits( cHybrid->getId(), cChip->getId()); 
                        auto cPipelineAddress = cEvent->PipelineAddress( cHybrid->getId(), cChip->getId());
                        auto cL1Id = cEvent->L1Id( cHybrid->getId(), cChip->getId());
                        
                        // now compare all channels for this address
                        // type of test 
                        fMemEvent.fType = fTypeOfTest; 
                        uint16_t cThresholdDuringTest =  cThresholdSet; 
                        if( fMemEvent.fType == 0 ) cThresholdDuringTest= 1000; 
                        if( fMemEvent.fType == 1 ) cThresholdDuringTest=  100; 
                        
                        // timing information 
                        fMemEvent.fStartTime = (int)fStartTime; 
                        fMemEvent.fStopTime = (int)fStopTime; 
                        // 
                        fMemEvent.fReadoutSuccess = fReadoutSuccess;
                        // event 
                        fMemEvent.fEventId = cEvntCnt;
                        fMemEvent.fL1Id    = cL1Id;
                        fMemEvent.fTrial   = cTrialNumber;
                        //
                        fMemEvent.fChipId  = cChip->getId();
                        // expected location in memory 
                        fMemEvent.fMemoryColumnExp = cExpectedPipelineAddress;
                        fMemEvent.fMemoryColumnRep = cPipelineAddress; 
                        // triggered bx 
                        fMemEvent.fTriggeredBx = cTriggeredBx;
                        fMemEvent.fTriggerNumberInBurst = cTriggerNumberInBurst; 
                        fMemEvent.fThreshold = cThresholdDuringTest; 
                        // 
                        fMemEvent.fPkgDelay = cPkgDelay;
                        // check stubs 
                        if( fMemEvent.fType == 3 )
                        {
                            CopyEvent(fStubEvent, fMemEvent);
                            auto& cExpectdStubsThisBoard = fExpectedStubs.at(cBoard->getIndex());
                            auto& cExpectdStubsThisOG = cExpectdStubsThisBoard->at(cOpticalGroup->getIndex());
                            auto& cExpectdStubsThisHybrd = cExpectdStubsThisOG->at(cHybrid->getIndex());
                            auto& cExpectdStubsThisROC = cExpectdStubsThisHybrd->at(cChip->getIndex());
                            auto& cExpectedStubs = cExpectdStubsThisROC->getSummary<std::vector<Stub>>();
                            // 
                            #ifdef __USE_ROOT__
                                TTree*      cRawStubTree  = static_cast<TTree*>(getHist(cHybrid, "Stub2STree"));
                                for(auto cReadoutStub : cStubs) 
                                {
                                    fStubEvent.fSeedExp = 0xFF;
                                    fStubEvent.fBendExp = 0xFF;
                                    fStubEvent.fSeedRep = cReadoutStub.getPosition();
                                    fStubEvent.fBendRep = cReadoutStub.getBend();
                                }
                                cRawStubTree->Fill();
                            #endif
                            for(auto cStub : cExpectedStubs)
                            {
                                fStubEvent.fSeedExp = cStub.getPosition();
                                fStubEvent.fBendExp = cStub.getBend();

                                bool cMatchFound=false;
                                size_t cMatchIndx=0; 
                                size_t cReadoutStubCounter=0;
                                for(auto cReadoutStub : cStubs) 
                                {
                                    if( cMatchFound ) continue; 
                                    cMatchFound = (cReadoutStub.getPosition() == cStub.getPosition());
                                    cMatchFound = cMatchFound && (cReadoutStub.getBend() == cStub.getBend());
                                    if( cMatchFound ) cMatchIndx = cReadoutStubCounter;  
                                    cReadoutStubCounter++;
                                }
                                fStubEvent.fMatchedStub = ( cMatchFound ) ? 1 : 0 ; 
                                if( cMatchFound )
                                {
                                    fStubEvent.fSeedRep = cStubs[cMatchIndx].getPosition();
                                    fStubEvent.fBendRep = cStubs[cMatchIndx].getBend();
                                }
                                else
                                    fStubEvent.fSeedRep = 0xFF;
                                    fStubEvent.fBendRep = 0xFF;
                                    
                                if( !cMatchFound )
                                {   
                                    cBadEventsSummary.push_back( fStubEvent );
                                }
                                #ifdef __USE_ROOT__
                                    TTree*      cStubTree  = static_cast<TTree*>(getHist(cHybrid, "StubCheck2STree"));
                                    cStubTree->Fill();
                                #endif
                            }
                        }

                        for( size_t cChnl=0; cChnl < cChip->size(); cChnl++)
                        {
                            // information about threshold + noise 
                            fMemEvent.fPedestal =cThNoiseThisChip->getChannel<ThresholdAndNoise>(cChnl).fThreshold; 
                            fMemEvent.fNoise =cThNoiseThisChip->getChannel<ThresholdAndNoise>(cChnl).fNoise; 
                            // memory row 
                            fMemEvent.fMemoryRow = cChnl; 
                            float cExpectedOcc = cExpectedOcThisChip->getChannel<Occupancy>(cChnl).fOccupancy;
                            int   cOcc = (int)(std::find( cHits.begin(), cHits.end() , cChnl) != cHits.end());
                            fMemEvent.fCorrectValue = (uint8_t)(cExpectedOcc == cOcc);
                            if(fMemEvent.fCorrectValue==0)
                            {
                                cBadEventsSummary.push_back( fMemEvent );
                                //PrintMemEvent(fMemEvent);
                            }
                            cNCorruptedCells += (fMemEvent.fCorrectValue==0) ? 1 : 0; 
                            // if ROOT is enabled fill tree here
                            #ifdef __USE_ROOT__
                                TTree*      cTree  = static_cast<TTree*>(getHist(cHybrid, "MemoryCheck2STree"));
                                cTree->Fill();
                            #endif
                            cMemEventsSummary.push_back( fMemEvent );
                        }
                    }//chip 
                }//hybrid
            }//OG
            cEvntCnt++;
        }// event 
    }
    
    if( cNCorruptedCells == 0 )
        LOG (INFO) << BOLDGREEN << "Perfect match between injected and readout data" << RESET;
    else
        LOG (INFO) << BOLDRED << +cNCorruptedCells << " mismatches found between injected and readout data" << RESET;
        
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