#include "BackEndAlignment.h"

#include "../HWInterface/BackendAlignmentInterface.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

BackEndAlignment::BackEndAlignment() : Tool() { fRegMapContainer.reset(); }

BackEndAlignment::~BackEndAlignment() {}
void BackEndAlignment::Reset()
{
    // set everything back to original values .. like I wasn't here
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap){ 
            if(cReg.first.find("stub_package_delay") != std::string::npos) continue;
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
                    fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
                }
            }
        }
    }
    resetPointers();
}
void BackEndAlignment::Initialise()
{
    fSuccess = false;
    // this is needed if you're going to use groups anywhere
    fChannelGroupHandler = new CBCChannelGroupHandler(); // This will be erased in tool.resetPointers()
    fChannelGroupHandler->setChannelGroupParameters(16, 2);

    // retreive original settings for all chips and all back-end boards
    ContainerFactory::copyAndInitChip<ChipRegMap>(*fDetectorContainer, fRegMapContainer);
    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>() = static_cast<BeBoard*>(cBoard)->getBeBoardRegMap();
        auto& cRegMapThisBoard                                                 = fRegMapContainer.at(cBoard->getIndex());
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                auto& cRegMapThisHybrid = cRegMapThisBoard->at(cOpticalReadout->getIndex())->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid) { cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>() = static_cast<ReadoutChip*>(cChip)->getRegMap(); }
            }
        }
    }
}
bool BackEndAlignment::Bx0Alignment(BeBoard* pBoard)
{
    bool cAligned = true;
    LOG(INFO) << GREEN << "Trying CIC un-packer alignment in the back-end" << RESET;

    uint32_t cNevents = 10; 
    // make sure I'm using the regular triger source
    uint16_t cNtriggers   = 0;
    uint16_t cTriggerRate = 1;
    uint8_t  cSource      = 3;
    uint8_t  cStubsMask   = 0;
    uint8_t  cStubLatency = 100;
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(cNtriggers, cTriggerRate, cSource, cStubsMask, cStubLatency);

    // only inject stub in the first ROC
    // for CBC 
    std::vector<uint8_t> cChipIds{0};
    std::vector<uint8_t> cSeeds{10};
    std::vector<int>     cBends{0};
    // for PS 
    //std::vector<uint32_t> cPixelIds{1, 200};
    // for now comment out
    uint16_t cMaxBxCounter = 3564;

    bool cIsPS=false;
    // check trigger source 
    // and reload 
    uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    LOG (INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    cTriggerSrc = (cTriggerSrc==6) ? cTriggerSrc : 6 ;
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    
    // latency to set on all FEs
    uint16_t cDelay   = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    uint16_t cLatency = cDelay -1;
    // PS specific configuration .. 
    // TO-DO : ADD TO GLOBAL tag in XML for MPAs TO BE ABLE TO CONFIGURE FROM THERE 
    uint8_t  cStubWindow = 1; // stub window in half pixels (1)
    uint8_t  cMode = 2; // (0) pixel-strip, (1) strip-strip, (2) pixel-pixel, (3) strip-pixel 
    //uint8_t  cModeReg = (cMode<<6)|cStubWindow; 
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                auto cReadoutChip          = static_cast<ReadoutChip*>(cChip);
                auto cReadoutChipInterface = static_cast<CbcInterface*>(fReadoutChipInterface);
                // for the moment - only written for CBC3
                if(cChip->getFrontEndType() == FrontEndType::CBC3) 
                {
                    //only inject stubs in the first ROC
                    if(std::find(cChipIds.begin(), cChipIds.end(), cChip->getId()) != cChipIds.end()) { cReadoutChipInterface->injectStubs(cReadoutChip, cSeeds, cBends, true); }
                    else
                    {
                        // make sure all other chips are quiet
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "VCth", 100);
                    }
                }
                else if( cChip->getFrontEndType() == FrontEndType::MPA) 
                {
                    cIsPS=true;
                    // activate stub mode
                    fReadoutChipInterface->WriteChipReg(cChip,"StubMode", cMode);
                    fReadoutChipInterface->WriteChipReg(cChip,"StubWindow", cStubWindow);
                    auto cRegValue = fReadoutChipInterface->ReadChipReg(cChip,"ECM");
                    LOG (INFO) << BOLDBLUE << "Read-back of pixel mode.. ECM register set to 0x" 
                        << std::hex << +cRegValue << std::dec << RESET;

                    // digital sync this pattern on pixel 1 
                    LOG (INFO) << BOLDBLUE << "Controlling injection .." << RESET;
                    // first make sure all pixels output 0x00 
                    fReadoutChipInterface->WriteChipReg(cChip,"DigitalSync", 0x00);
                    // for( auto cPixelId : cPixelIds )
                    // {
                    //     // then .. for pixels I want enable pattern on Pixel1
                    //     fReadoutChipInterface->WriteChipReg(cChip,"DigitalSyncP1", 0xFF);
                    // }
                    // then .. for pixels I want enable pattern on Pixel1
                    fReadoutChipInterface->WriteChipReg(cChip,"DigitalSyncP1", 0xFF);
                    // then .. for pixels I want enable pattern on Pixel20 as well 
                    fReadoutChipInterface->WriteChipReg(cChip,"DigitalSyncP200", 0xFF);

                    fReadoutChipInterface->WriteChipReg(cChip,"TriggerLatency",cLatency);

                    //
                     // mapping for PS module 
                    // mapping for probe station/etc. can be different
                    fReadoutChipInterface->WriteChipReg(cChip,"Out0",1);  
                    fReadoutChipInterface->WriteChipReg(cChip,"Out1",2);  
                    fReadoutChipInterface->WriteChipReg(cChip,"Out2",3);  
                    fReadoutChipInterface->WriteChipReg(cChip,"Out3",4);  
                    fReadoutChipInterface->WriteChipReg(cChip,"Out4",5);  
                    fReadoutChipInterface->WriteChipReg(cChip,"Out5",0);//L1 line 

                }
            } // chip
        }     // hybrid
    }         // module
    LOG (INFO) <<  BOLDBLUE << "Bx0Alignment for : " << ((cIsPS) ? "PS" : "2S") << RESET;
    // resync .. checking what this does to the alignment 
    // seems ok .. I will keep it then 
    // fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(pBoard));
    
    
    // now try and find correct package delay 
    auto cOriginalDelay = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
    LOG(INFO) << BOLDBLUE << "Original package delay is " << +cOriginalDelay << RESET;
    bool    cCorrectDelay = false;
    uint8_t cPackageDelay = 0;
    uint8_t cFinalDelay   = cPackageDelay;
    for(cPackageDelay = 0; cPackageDelay < 8; cPackageDelay++)
    {
        if(cCorrectDelay) continue;

        LOG(INFO) << BOLDMAGENTA << "Package delay set to " << +cPackageDelay << RESET;
        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay", cPackageDelay);
        (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->Bx0Alignment();

        // check stubs
        // 2 events should be enough
        LOG(INFO) << BOLDMAGENTA << "Requesting " << +cNevents << " events from the board " << RESET;
        ReadNEvents(pBoard, cNevents);
        const std::vector<Event*>& cEventsWithStubs = this->GetEvents(pBoard);
        LOG(INFO) << BOLDBLUE << "Read back " << +cEventsWithStubs.size() << " events from the FC7 ..." << RESET;

        // now ... check for incrementing BxIds 
        int     cNRollOvers = 0 ;
        std::vector<int> cBxIds(0);
        std::vector<int> cBxDifferences(0);//I think by injecting this way this number should always be the same ..
        for(auto& cEvent: cEventsWithStubs)
        {
            for(auto cOpticalGroup: *pBoard)
            {
                // only checked for first link 
                if(cOpticalGroup->getIndex() > 0) continue;

                for(auto cHybrid: *cOpticalGroup)
                {
                    if(cHybrid->getIndex() > 0) continue;

                    auto cBx = (int)cEvent->BxId(cHybrid->getId());
                    if( cBxIds.size() > 0 ) 
                    {
                        int cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBxIds[cBxIds.size()-1]%cMaxBxCounter); 
                        cNRollOvers += ((cBxIds[cBxIds.size()-1] >= 2500 ) && (cBxIds[cBxIds.size()-1] < cMaxBxCounter)) && (cBx < cBxIds[cBxIds.size()-1]) ? 1 : 0 ;
                        cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBx%cMaxBxCounter) - cBxDifference;
                        cBxDifferences.push_back(cBxDifference);
                        LOG (DEBUG) << BOLDBLUE << "\t.....BxDifference is " << +cBxDifference << RESET;
                    }
                    cBxIds.push_back(cBx);
                    LOG(INFO) << BOLDBLUE << "Hybrid " << +cHybrid->getId() << " BxID " << +cBx << RESET;
                    
                } // hybrids or CICs
            }// modules or optical links
        }// events 
        // figure out the differences between the bxIds 
        auto cFirstDifference=cBxDifferences[0];
        std::adjacent_difference(cBxDifferences.begin(), cBxDifferences.end(), cBxDifferences.begin()); 
        cBxDifferences.erase (cBxDifferences.begin()); // erase the first element 
        for(auto cDifference : cBxDifferences )
            LOG (DEBUG) << BOLDBLUE << "\t..." << +cDifference << RESET;
        // all elements are equal 
        if( cFirstDifference != 0 && std::equal(cBxDifferences.begin() + 1, cBxDifferences.end(), cBxDifferences.begin()) )
        {
            LOG (INFO) << BOLDGREEN << "Found differences between bxIds to always be the same : " << +cFirstDifference << RESET;
            LOG (INFO) << BOLDGREEN << "Going to fix the manual package delay to " << +cPackageDelay << RESET;
            cCorrectDelay=true;
        }
        else
            LOG (INFO) << BOLDRED << "Found differences between bxIds to be different from one another." << RESET;


        // // // I need to think about this some more ...
        // // // so for now just check the number of stubs
        // // // first I want to check that the BxIds from one CIC is incrementing correctly
        // // // assuming that this is the same for all CICs
        // std::vector<uint16_t> cBxIds(0);
        // bool                  cIncrementing  = true;
        // bool                  cCorrectNStubs = true;
        // for(auto& cEvent: cEventsWithStubs)
        // {
        //     for(auto cOpticalGroup: *pBoard)
        //     {
        //         if(cOpticalGroup->getIndex() > 0) continue;

        //         for(auto cHybrid: *cOpticalGroup)
        //         {
        //             if(cHybrid->getIndex() > 0) continue;

        //             auto cBx = cEvent->BxId(cHybrid->getId());
        //             // cBxIds.push_back( cBx );
        //             // if( ( cBxIds.size() == 1   )
        //             //     cIncrementing =  true;
        //             // else if ( cBxIds.size() > 1 )
        //             //     if ( )
        //             // else
        //             //     cIncrementing = (cIncrementing) && ( cBx > cBxIds[cBxIds.size()-1]);

        //             LOG(DEBUG) << BOLDBLUE << "Hybrid " << +cHybrid->getId() << " BxID "
        //                        << +cBx
        //                        // << " roll-over indicator set to " << ((cRollOver) ? "True" : "False")
        //                        << " and incrementing flag is " << ((cIncrementing) ? "True" : "False") << RESET;
        //             for(auto cChip: *cHybrid)
        //             {
        //                 if(std::find(cChipIds.begin(), cChipIds.end(), cChip->getId()) == cChipIds.end()) continue;

        //                 auto cStubs    = cEvent->StubVector(cHybrid->getId(), cChip->getId());
        //                 cCorrectNStubs = cCorrectNStubs && (cStubs.size() == cSeeds.size());
        //                 auto cHits     = cEvent->GetHits(cHybrid->getId(), cChip->getId());

        //                 LOG(DEBUG) << BOLDBLUE << "\t..ROC#" << +cChip->getId() << " has " << +cStubs.size() << " stubs in the event and " << +cHits.size() << " hits." << RESET;
        //             }
        //         } // hybrids or CICs
        //     }     // modules or optical links
        // }
        // cCorrectDelay = cCorrectNStubs && cIncrementing;
        // if(cCorrectDelay)
        // {
        //     cFinalDelay = cCorrectDelay;
        //     LOG(INFO) << BOLDBLUE << "Stub package delay will be set to " << +cPackageDelay << RESET;

        //     // check again
        //     this->ReadNEvents(pBoard, 10);
        //     const std::vector<Event*>& cEvents = this->GetEvents();
        //     LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 ..." << RESET;
        //     for(auto& cEvent: cEvents)
        //     {
        //         auto cEventCount = cEvent->GetEventCount();
        //         LOG(INFO) << BOLDBLUE << "Event " << +cEventCount << RESET;
        //         for(auto cOpticalGroup: *pBoard)
        //         {
        //             // check number of stubs
        //             for(auto cHybrid: *cOpticalGroup)
        //             {
        //                 auto cStatus = static_cast<D19cCic2Event*>(cEvent)->Status(cHybrid->getId());
        //                 auto cBx     = cEvent->BxId(cHybrid->getId());

        //                 LOG(INFO) << BOLDBLUE << "FE" << +cHybrid->getId() << " Status : " << std::bitset<9>(cStatus) << " BxId : " << +cBx << RESET;
        //             } // hybrids or CICs
        //         }     // modules
        //     }         // event
        // }
        // else
        //     LOG(INFO) << BOLDRED << "Stubs from CIC for a package delay of " << +cPackageDelay << " do not make sense...  continuing the scan ..." << RESET;

    } // pkg delay


    LOG(INFO) << BOLDMAGENTA << "End of Bx0Alignment loop " << RESET;
    cAligned = cCorrectDelay && (cFinalDelay < 8);
    
    // quick and dirty stub latency scan 
    // if I do 
    if( cAligned )
    {
        // I expect the offset to be in this range [at least for this case]
        int cCorrectLatency = 0; 
        for( int cOffset=70; cOffset < 90; cOffset++)
        {
            int cStubLatency = cLatency - cOffset; 
            if( cCorrectLatency != 0 ) continue; 

            if( cStubLatency < 0 ) continue;

            fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
            LOG (INFO) << BOLDBLUE << "Stub latency set to " << +cStubLatency << RESET;

            LOG(INFO) << BOLDMAGENTA << "Requesting " << +cNevents << " events from the board " << RESET;
            ReadNEvents(pBoard, cNevents);
            const std::vector<Event*>& cEventsWithStubs = this->GetEvents(pBoard);
            LOG(INFO) << BOLDBLUE << "Read back " << +cEventsWithStubs.size() << " events from the FC7 ..." << RESET;
            for( auto cEvent : cEventsWithStubs ) 
            {
                for(auto cOpticalGroup: *pBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        // only for the first hybrid 
                        if(cHybrid->getIndex() > 0) continue;

                        for(auto cChip: *cHybrid)
                        {
                            if( cChip->getFrontEndType() == FrontEndType::SSA ) continue;

                            auto cStubs    = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                            LOG (INFO) << BOLDBLUE << "\t... found " << +cStubs.size() << " stubs in this event."  << RESET;
                            if( cStubs.size() == 2 ) 
                            {
                                // auto cPClusters = (static_cast<D19cCic2Event*>(cEvent))->GetPixelClusters(cHybrid->getId(), cChip->getId());
                                // auto cSClusters = (static_cast<D19cCic2Event*>(cEvent))->GetStripClusters(cHybrid->getId(), cChip->getId());
                                LOG (INFO) << BOLDMAGENTA << "\t\t... number of S-clusters from EventClass is " << +(static_cast<D19cCic2Event*>(cEvent))->GetNStripClusters(cHybrid->getId() )
                                    << " number of P-clusters from EventClass is " << +(static_cast<D19cCic2Event*>(cEvent))->GetNPixelClusters(cHybrid->getId() )
                                    << RESET;
                                    cCorrectLatency = cOffset;
                            
                                // LOG (INFO) << BOLDMAGENTA << "\t\t... found " << +cPClusters.size() << " p clusters "
                                //     << " and "<< +cSClusters.size() << " s clusters in Bx0Alignment"
                                //     << " number of S-clusters from EventClass is " << +(static_cast<D19cCic2Event*>(cEvent))->GetNStripClusters(cHybrid->getId() )
                                //     << " number of P-clusters from EventClass is " << +(static_cast<D19cCic2Event*>(cEvent))->GetNPixelClusters(cHybrid->getId() )
                                //     << RESET;
                                //     cCorrectLatency = cOffset;
                            }
                        }// ROCs 
                    } // hybrids or CICs
                }// optical group loop 
            }//event loop 
        }
        cAligned = ( cCorrectLatency != 0 );
        if( cAligned )
        {
            LOG (INFO) << BOLDGREEN << "Setting correct stub offset for this back-end board to " << +cCorrectLatency << RESET;
            // adding this here in preparation for stub decoding 
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->SetStubOffset( cCorrectLatency );
        }
        else
        {
            LOG(INFO) << BOLDRED << "Could not find correct stub offset in back-end.. stop and check!" << RESET;
            throw std::runtime_error(std::string("Could not find correct stub offset in back-end.. stop and check!"));
        }
    }
    //cAligned = true; // for now 
    return cAligned;
}
bool BackEndAlignment::PSAlignment(BeBoard* pBoard)
{
    bool cTuned = true;
    LOG(INFO) << GREEN << "Trying Phase Tuning for PS Chip(s)" << RESET;

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                ReadoutChip*             cReadoutChip = static_cast<ReadoutChip*>(cChip);


                if (cChip->getFrontEndType() == FrontEndType::MPA)
                    {
                    LOG(INFO) << GREEN << "MPA Alignment" << RESET;
                    std::vector<std::string> cRegNames{"ReadoutMode", "ECM", "LFSR_data"};
                    std::vector<uint8_t>     cOriginalValues;
                    uint8_t                  cAlignmentPattern = 0xa0;
                    std::vector<uint8_t>     cRegValues{0x0, 0x08, cAlignmentPattern};

                    for(size_t cIndex = 0; cIndex < 3; cIndex++)
                    {
                        cOriginalValues.push_back(fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegNames[cIndex]));
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cRegValues[cIndex]);
                    }

                    for(uint8_t cLineId = 0; cLineId < 8; cLineId++)
                    { cTuned = cTuned && static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cChip->getIndex(), cLineId, cAlignmentPattern, 8); }

                    for(size_t cIndex = 0; cIndex < 3; cIndex++) { fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cOriginalValues[cIndex]); };

                    }
                if (cChip->getFrontEndType() == FrontEndType::SSA)
                    {
                    LOG(INFO) << GREEN << "SSA Alignment" << RESET;
                    ReadoutChip* cReadoutChip = static_cast<ReadoutChip*>(cChip);
                    std::vector<std::string> cRegNames{"SLVS_pad_current", "ReadoutMode"};
                    std::vector<uint8_t>     cOriginalValues;
                    std::vector<uint8_t>     cRegValues{0x7, 2};
                    for(size_t cIndex = 0; cIndex < 2; cIndex++)
                    {
                        cOriginalValues.push_back(fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegNames[cIndex]));
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cRegValues[cIndex]);
                    }

                    uint8_t cAlignmentPattern = 0x80;

                    for(uint8_t cLineId = 0; cLineId < 8; cLineId++)
                    {
                        char cBuffer[11];
                        sprintf(cBuffer, "OutPattern%d", cLineId );
                        std::string cRegName = (cLineId == 7) ? "OutPattern7/FIFOconfig" : std::string(cBuffer, sizeof(cBuffer));
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cAlignmentPattern);
                        cTuned = cTuned && static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cChip->getIndex(), cLineId, cAlignmentPattern, 8);
                    }

                    for(size_t cIndex = 0; cIndex < 2; cIndex++) { fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cOriginalValues[cIndex]); }
                    }
            }
        }
    }

    LOG(INFO) << GREEN << "PS Phase tuning finished succesfully" << RESET;
    return cTuned;
}

bool BackEndAlignment::CICAlignment(BeBoard* pBoard)
{
    // make sure you're only sending one trigger at a time here
    auto cTriggerMultiplicity = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0);

    // force CIC to output repeating 101010 pattern on L1 line 
    // needed for phase alignment in back-end 
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SelectOutput(cCic, true);
        }
    }
    bool cAligned = true;
    if(!pBoard->ifOptical()) cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->L1PhaseTuning(pBoard, fL1Debug);
    if(!cAligned)
    {
        LOG(INFO) << BOLDBLUE << "L1A phase alignment in the back-end " << BOLDRED << " FAILED ..." << RESET;
        return false;
    }

    // force CIC to output empty L1A frames [by disabling all FEs]
    // needed for word alingment in the back-end
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            // disable alignment output 
            fCicInterface->SelectOutput(cCic, false);
            // 
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        }
    }
    fL1Debug = true;
    cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->L1WordAlignment(pBoard, fL1Debug);
    if(!cAligned)
    {
        LOG(INFO) << BOLDBLUE << "L1A word alignment in the back-end " << BOLDRED << " FAILED ..." << RESET;
        return false;
    }

    // enable CIC output of alignmnent pattern on stub lines 
    // .. and enable all FEs again
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            // enable alignment output for stubs 
            fCicInterface->SelectOutput(cCic, true);
        }
    }
    cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->StubTuning(pBoard, true);

    // disable CIC output of pattern on stub + l1 lines
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SelectOutput(cCic, false);
            
            // figure out which FEs are active 
            std::vector<uint8_t> cEnabledFEs(0);             
            for(auto cReadoutChip: *cHybrid)
            {
                if( cReadoutChip->getFrontEndType() == FrontEndType::SSA) continue;
                cEnabledFEs.push_back( cReadoutChip->getId() );
            }
            fCicInterface->EnableFEs(cCic, cEnabledFEs, true);
        }//hybrids [CICs]
    }//optical groups [modules]

    // re-load configuration of fast command block from register map loaded from xml file
    LOG(INFO) << BOLDBLUE << "Re-loading original coonfiguration of fast command block from hardware description file [.xml] " << RESET;
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFastCommandBlock(pBoard);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cTriggerMultiplicity);
    return cAligned;
}

bool BackEndAlignment::CBCAlignment(BeBoard* pBoard)
{
    bool cAligned = false;

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<Hybrid*>(cHybrid)->getLinkId());
            for(auto cReadoutChip: *cHybrid)
            {
                ReadoutChip* theReadoutChip = static_cast<ReadoutChip*>(cReadoutChip);
                // fBeBoardInterface->WriteBoardReg (pBoard,
                // "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cReadoutChip->getId() );
                // original mask
                const ChannelGroup<NCHANNELS>* cOriginalMask = static_cast<const ChannelGroup<NCHANNELS>*>(cReadoutChip->getChipOriginalMask());
                // original threshold
                uint16_t cThreshold = static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(theReadoutChip, "VCth");
                // original HIT OR setting
                uint16_t cHitOR = static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(theReadoutChip, "HitOr");

                // make sure hit OR is turned off
                static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theReadoutChip, "HitOr", 0);
                // make sure pT cut is set to maximum
                // make sure hit OR is turned off
                auto cPtCut = static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(theReadoutChip, "PtCut");
                static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theReadoutChip, "PtCut", 14);

                LOG(INFO) << BOLDBLUE << "Running phase tuning and word alignment on FE" << +cHybrid->getId() << " CBC" << +cReadoutChip->getId() << "..." << RESET;
                uint8_t              cBendCode_phAlign = 2;
                std::vector<uint8_t> cBendLUT          = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theReadoutChip);
                auto                 cIterator         = std::find(cBendLUT.begin(), cBendLUT.end(), cBendCode_phAlign);
                // if bend code isn't there ... quit
                if(cIterator == cBendLUT.end()) continue;

                int    cPosition    = std::distance(cBendLUT.begin(), cIterator);
                double cBend_strips = -7. + 0.5 * cPosition;
                LOG(DEBUG) << BOLDBLUE << "Bend code of " << +cBendCode_phAlign << " found in register " << cPosition << " so a bend of " << cBend_strips << RESET;

                uint8_t              cSuccess = 0x00;
                std::vector<uint8_t> cSeeds{0x82, 0x8E, 0x9E};
                std::vector<int>     cBends(cSeeds.size(), static_cast<int>(cBend_strips * 2));
                static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theReadoutChip, cSeeds, cBends);
                // first align lines with stub seeds
                uint8_t cLineId = 1;
                for(size_t cIndex = 0; cIndex < 3; cIndex++)
                {
                    cSuccess = cSuccess |
                               (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cReadoutChip->getIndex(), cLineId, cSeeds[cIndex], 8)
                                << cIndex);
                    cLineId++;
                }
                // then align lines with stub bends
                uint8_t cAlignmentPattern = (cBendCode_phAlign << 4) | cBendCode_phAlign;
                cSuccess                  = cSuccess |
                           (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cReadoutChip->getIndex(), cLineId, cAlignmentPattern, 8)
                            << (cLineId - 1));
                cLineId++;
                // finally sync bit + last bend
                cAlignmentPattern = (1 << 7) | cBendCode_phAlign;
                bool cTuned =
                    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cReadoutChip->getIndex(), cLineId, cAlignmentPattern, 8);
                if(!cTuned)
                {
                    LOG(INFO) << BOLDMAGENTA << "Checking if error bit is set ..." << RESET;
                    // check if error bit is set
                    cAlignmentPattern = (1 << 7) | (1 << 6) | cBendCode_phAlign;
                    cSuccess =
                        cSuccess |
                        (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cReadoutChip->getIndex(), cLineId, cAlignmentPattern, 8)
                         << (cLineId - 1));
                }
                else
                    cSuccess = cSuccess | (static_cast<uint8_t>(cTuned) << (cLineId - 1));

                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>(cSeeds[0]) << ", " << std::bitset<8>(cSeeds[1]) << ", " << std::bitset<8>(cSeeds[2]) << " on stub lines  0, 1 and 2."
                          << RESET;
                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>((cBendCode_phAlign << 4) | cBendCode_phAlign) << " on stub line  4." << RESET;
                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>((1 << 7) | cBendCode_phAlign) << " on stub line  5." << RESET;
                LOG(INFO) << BOLDMAGENTA << "After alignment of last stub line ... stub lines 0-5: " << RESET;
                (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, 5);

                // now unmask all channels and set threshold and hit or logic back to their original values
                fReadoutChipInterface->maskChannelsGroup(theReadoutChip, cOriginalMask);
                LOG(INFO) << BOLDBLUE << "Setting threshold and HitOR back to orginal value [ " << +cThreshold << " ] DAC units." << RESET;
                fReadoutChipInterface->WriteChipReg(theReadoutChip, "VCth", cThreshold);
                fReadoutChipInterface->WriteChipReg(theReadoutChip, "HitOr", cHitOR);
                fReadoutChipInterface->WriteChipReg(theReadoutChip, "PtCut", cPtCut);
            }
        }
    }

    return cAligned;
}
bool BackEndAlignment::Align()
{
    LOG(INFO) << BOLDBLUE << "Starting back-end alignment procedure .... " << RESET;
    bool cAligned = true;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        // read back register map before you've done anything
        auto cBoardRegisterMap = theBoard->getBeBoardRegMap();

        OuterTrackerHybrid* cFirstHybrid = static_cast<OuterTrackerHybrid*>(cBoard->at(0)->at(0));
        bool                cWithCIC     = cFirstHybrid->fCic != NULL;
        if(cWithCIC)
        {
            cAligned = this->CICAlignment(theBoard);
            cAligned = cAligned && this->Bx0Alignment(theBoard);
        }
        else
        {
            ReadoutChip* theFirstReadoutChip = static_cast<ReadoutChip*>(cBoard->at(0)->at(0)->at(0));
            bool         cWithCBC            = (theFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
            bool         cWithSSA            = (theFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
            bool         cWithMPA            = (theFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);
            if(cWithCBC) { this->CBCAlignment(theBoard); }
            else if(cWithMPA or cWithSSA)
            {
                this->PSAlignment(theBoard);
            }
            else
            {
            }
        }
        // re-load configuration of fast command block from register map loaded from xml file
        LOG(INFO) << BOLDBLUE << "Re-loading original coonfiguration of fast command block from hardware description file [.xml] " << RESET;
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFastCommandBlock(theBoard);

        // now send a fast reset
        fBeBoardInterface->ChipReSync(theBoard);
    }
    return cAligned;
}
void BackEndAlignment::writeObjects() {}
// State machine control functions
void BackEndAlignment::Running()
{
    Initialise();
    fSuccess = this->Align();
    if(!fSuccess)
    {
        LOG(ERROR) << BOLDRED << "Failed to align back-end" << RESET;
// gui::message("Backend alignment failed"); //How
#ifdef __USE_ROOT__
        SaveResults();
        WriteRootFile();
        CloseResultFile();
#endif
        Destroy();
        exit(FAILED_BACKEND_ALIGNMENT);
    }
}

void BackEndAlignment::Stop()
{
    dumpConfigFiles();

    // Destroy();
}

void BackEndAlignment::Pause() {}

void BackEndAlignment::Resume() {}
