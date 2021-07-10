#include "PSAlignment.h"

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

PSAlignment::PSAlignment() : Tool() { fRegMapContainer.reset(); }

PSAlignment::~PSAlignment() {}
void PSAlignment::Reset()
{
    // set everything back to original values .. like I wasn't here
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap) { cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second)); }
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);
        // comment out for now
        // auto& cRegMapThisBoard = fRegMapContainer.at(cBoard->getIndex());
        // for(auto cOpticalGroup: *cBoard)
        // {
        //     auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());
        //     for(auto cHybrid: *cOpticalGroup)
        //     {
        //         auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
        //         LOG(INFO) << BOLDBLUE << "Resetting all registers on readout chips connected to FEhybrid#" << (cHybrid->getId()) << " back to their original values..." << RESET;
        //         for(auto cChip: *cHybrid)
        //         {
        //             auto&                                         cRegMapThisChip = cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>();
        //             std::vector<std::pair<std::string, uint16_t>> cVecRegisters;
        //             cVecRegisters.clear();
        //             for(auto cReg: cRegMapThisChip)
        //             {
        //                 if(cChip->getFrontEndType() == FrontEndType::MPA)
        //                 {
        //                     if(cReg.first.find("OutSetting") != std::string::npos || cReg.first.find("LatencyRx320") != std::string::npos || cReg.first.find("LatencyRx40") != std::string::npos ||
        //                        cReg.first.find("RetimePix") != std::string::npos)
        //                     { LOG(DEBUG) << BOLDMAGENTA << "\t...Will NOT set " << cReg.first << " back to original value. " << RESET; }
        //                 }
        //                 else
        //                 {
        //                     cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue));
        //                 }
        //             }
        //             fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
        //         }
        //     }
        // }
    }
    resetPointers();
}

void PSAlignment::Initialise()
{
    fSuccess = false;
    // retreive original settings for all chips and all back-end boards
    ContainerFactory::copyAndInitChip<ChipRegMap>(*fDetectorContainer, fRegMapContainer);
    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ChipRegMap&       theChipMap     = fRegMapContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<ChipRegMap>();
                    const ChipRegMap& theOriginalMap = static_cast<ReadoutChip*>(cChip)->getRegMap();
                    theChipMap.insert(theOriginalMap.begin(), theOriginalMap.end());
                }
            }
        }
    }
}
void PSAlignment::MapMPAOutputs(std::string pSetupType)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                // map MPA outputs
                for(auto cChip: *cHybrid) // for each chip (makes sense)
                {
                    if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                    // mapping for PS module
                    // mapping for probe station/etc. can be different
                    if(pSetupType.find("PSModule") != std::string::npos)
                    {
                        std::vector<int> cMappedTo{1, 2, 3, 4, 5, 0};
                        for(size_t cIndx = 0; cIndx < cMappedTo.size(); cIndx++)
                        {
                            LOG(INFO) << BOLDBLUE << "Configuring MPA output register [mapping between output bits and output pads] .... Output# " << +cIndx << RESET;
                            std::ostringstream cRegName;
                            cRegName << "OutSetting_" << cIndx;
                            fReadoutChipInterface->WriteChipReg(cChip, cRegName.str(), cMappedTo[cIndx]);
                        }
                    }
                } // chip
            }     // hybrid
        }         // optical group
    }
}
bool PSAlignment::AlignStubInputs(BeBoard* pBoard)
{
    bool     cPhaseFound = true;
    uint32_t cNevents    = 10;
    LOG(INFO) << BOLDBLUE << "Aligning MPA stub inputs.." << RESET;
    uint8_t cRow        = 9; // random pixel to activate -- could be configurable
    uint8_t cCol        = 45;
    auto    cStubOffset = pBoard->getStubOffset();
    // check trigger source
    // and reload
    uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    cTriggerSrc = (cTriggerSrc == 6) ? cTriggerSrc : 6;
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    uint16_t cDelay   = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    uint16_t cLatency = cDelay - 1;
    LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    LOG(INFO) << BOLDMAGENTA << "Expect correct latency to be " << +cLatency << RESET;
    // configure SSA to inject digitally
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // configure SSA to inject digitally
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                    uint32_t cGpix = static_cast<MPA*>(cChip)->PNglobal(std::pair<uint32_t, uint32_t>(cRow, cCol));
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency);
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_P" + std::to_string(cGpix), 0x37);
                    fReadoutChipInterface->WriteChipReg(cChip, "DigitalSync_P" + std::to_string(cGpix), 0xAA); // enable 1 pix
                }
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency - 1);
                    fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_L_ALL", 0xAA);
                    fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_H_ALL", 0xAA);
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cCol), 0x9);
                }
            } // chip
        }     // hybrid
    }         // optica]l group

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                // uint8_t  cMode = 3;
                // uint8_t  cStubWindow=8;

                static_cast<PSInterface*>(fReadoutChipInterface)->Activate_ps(cChip, 8);
                // text parsed reimplementation
                // fReadoutChipInterface->WriteChipReg(cChip,"StubMode", cMode);
                // fReadoutChipInterface->WriteChipReg(cChip,"StubWindow", cStubWindow);
            } // chip
        }     // hybrid
    }         // optica]l group

    // scan phase and check stubs
    // for each chip on a hybrid

    bool cCurPhaseFound = true;
    // not sure why cChipId loop, why not just all on hybrid
    for(uint8_t cChipId = 0+8; cChipId < 8+8; cChipId++)
    {
        bool nochip = true;
        if(!cCurPhaseFound) cPhaseFound = false; // all chips need to be tuned
        cCurPhaseFound = false;
        for(uint8_t cPhase = 0; cPhase < 8; cPhase++)
        {
            if(cCurPhaseFound == true) break; // break loops if chip phase already found
            for(uint8_t cRetime = 6; cRetime < 8; cRetime++)
            {
                if(cCurPhaseFound == true) break;
                for(auto cOpticalReadout: *pBoard)
                {
                    for(auto cHybrid: *cOpticalReadout)
                    {
                        // configure SSA to inject digitally
                        for(auto cChip: *cHybrid) // for each chip (makes sense)
                        {
                            if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
                            if(cChip->getId() != cChipId) continue;
                            nochip = false;
                            fReadoutChipInterface->WriteChipReg(cChip, "StubInputPhase", cPhase);
                            fReadoutChipInterface->WriteChipReg(cChip, "RetimePix", cRetime);
                        } // chip
                    }     // hybrid
                }         // optical group
                if(nochip)
                {
                    cCurPhaseFound = true;
                    continue;
                }
                size_t writeslat = cLatency - cStubOffset - cRetime; // stub latency
                std::cout << "Writing common_stubdata_delay " << writeslat << std::endl;
                fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", writeslat);
                // now read data
                LOG(INFO) << BOLDBLUE << "Setting stub input sampling phase, and pixel retime for MPA#" << +cChipId << " on hybrid to " << +cPhase << " and " << +cRetime << RESET;
                ReadNEvents(pBoard, cNevents);
                const std::vector<Event*>& cEvents = this->GetEvents();
                LOG(INFO) << BOLDBLUE << "Checking phase by reading back " << +cEvents.size() << " events from the FC7 ..." << RESET;
                uint32_t MatchNStubtot = 0;
                for(auto& ev: cEvents)
                {
                    for(auto cOpticalReadout: *pBoard)
                    {
                        for(auto cHybrid: *cOpticalReadout)
                        {
                            for(auto cChip: *cHybrid)
                            {
                                if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
                                if(cChip->getId() != cChipId) continue;
                                std::vector<Stub> stubs = static_cast<D19cCic2Event*>(ev)->StubVector(cHybrid->getId(), cChip->getId());
                                for(auto& st: stubs)
                                {
                                    if((2 * cCol) == st.getPosition() and (cRow - 1) == st.getRow()) MatchNStubtot += 1; // Match row and column
                                    std::cout << "getPosition " << +st.getPosition() << std::endl;
                                    std::cout << "getBend " << +st.getBend() << std::endl;
                                    std::cout << "getRow " << +st.getRow() << std::endl;
                                }
                            }
                        }
                    }
                }
                if(MatchNStubtot == (cNevents))
                {
                    LOG(INFO) << BOLDGREEN << "-----" << RESET;
                    LOG(INFO) << BOLDGREEN << "Stub input sampling phase and pixel retime for MPA#" << +cChipId << " completed." << RESET;
                    LOG(INFO) << BOLDGREEN << "Phase:" << +cPhase << " Retime:" << +cRetime << RESET;
                    LOG(INFO) << BOLDGREEN << "-----" << RESET;
                    cCurPhaseFound = true;
                }
                else
                {
                    LOG(INFO) << BOLDRED << "-----" << RESET;
                    LOG(INFO) << BOLDRED << "Stub input sampling phase and pixel retime for MPA#" << +cChipId << " completed." << RESET;
                    LOG(INFO) << BOLDRED << "Phase:" << +cPhase << " Retime:" << +cRetime << RESET;
                    LOG(INFO) << BOLDRED << "-----" << RESET;
                }
            } // retime loop
        }     // phase loop
    }         // chip id loop [up-to 8 chips per hybrid ]

    return cPhaseFound;
}
std::vector<std::pair<uint8_t, uint8_t>> PSAlignment::AlignChip(ReadoutChip* pChip, std::vector<Injection> pInjections, uint16_t pLatency)
{
    std::vector<std::pair<uint8_t, uint8_t>> cGoodCombinations; 
    cGoodCombinations.clear();
    LOG (INFO) << BOLDMAGENTA << "PSAlignment::AlignChip  - aligning L1 and stub data for SSA-MPA pair#" << +pChip->getId()%8 << RESET;
    auto cBoardId   = pChip->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    auto cTriggerMult = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint32_t cNevents = 10;

    for( uint8_t cPhase = 2; cPhase < 5; cPhase++) 
    {
        if( cGoodCombinations.size() > 0 ) continue; //for now .. only the first one 
        for(uint8_t cWord = 0; cWord < 16; cWord++)
        {
            if( cGoodCombinations.size() > 0 ) continue; //for now .. only the first one
            fReadoutChipInterface->WriteChipReg(pChip, "L1InputPhase", cPhase);
            fReadoutChipInterface->WriteChipReg(pChip, "LatencyRx40", cWord); 

            ReadNEvents(*cBoardIter, cNevents);
            const std::vector<Event*>& cEvents = this->GetEvents();
            if( cEvents.size() == 0 ) continue;

            LOG(INFO) << BOLDBLUE << "Setting L1 input sampling phase MPAs to " << +cPhase << " and Rx40 delay to " << +cWord << RESET;
            // start at the beginning + trigger id in burst 
            for( size_t cTriggerId=0; cTriggerId < cTriggerMult+1 ; cTriggerId++)
            {
                size_t cMatchedEvents=0;
                auto cEventIter = cEvents.begin() + cTriggerId ;
                do
                {   
                    if( cEventIter >= cEvents.end() ) break; 
                    bool cNmatch=true;
                    bool cFmatch=true;
                    auto cPclus = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(pChip->getHybridId(), pChip->getId());
                    auto cSclus = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(pChip->getHybridId(), pChip->getId());
                    cNmatch = cNmatch && ( cSclus.size() == pInjections.size() && cPclus.size() == pInjections.size() ) ;
                    cFmatch = cNmatch; 
                    if( cNmatch ) 
                    {
                        LOG (DEBUG) << BOLDBLUE << "Trigger#" << +cTriggerId << " in a burst of " << (1+ cTriggerMult) 
                                    << " MPA" << +pChip->getId() << " found " << cSclus.size()
                                    << " S clusters and " 
                                    << cPclus.size() 
                                    << " P clusters in L1 data from MPA#" << +pChip->getId()
                                    << RESET;
                        for( size_t cIndx=0; cIndx < pInjections.size(); cIndx++)
                        {
                            cFmatch = cFmatch && ( cPclus[cIndx].fAddress == cSclus[cIndx].fAddress); 
                            if( ( cPclus[cIndx].fAddress == cSclus[cIndx].fAddress) )
                                LOG (DEBUG) << BOLDGREEN << "Exact match found " 
                                        << BOLDYELLOW << " P-cluster in row " << +cPclus[cIndx].fAddress
                                        << " column " << +cPclus[cIndx].fZpos << " width is " << +cPclus[cIndx].fWidth
                                        << BOLDCYAN << " S-cluster in row " << +cSclus[cIndx].fAddress
                                        << " column " << (0) << " width is " << +cSclus[cIndx].fWidth
                                        << RESET;
                            else
                                LOG (DEBUG) << BOLDRED << "Exact match not found " 
                                        << BOLDYELLOW << " P-cluster in row " << +cPclus[cIndx].fAddress
                                        << " column " << +cPclus[cIndx].fZpos << " width is " << +cPclus[cIndx].fWidth
                                        << BOLDCYAN << " S-cluster in row " << +cSclus[cIndx].fAddress
                                        << " column " << (0) << " width is " << +cSclus[cIndx].fWidth
                                        << RESET;
                        }
                    }
                    // if( cNmatch && cFmatch )
                    //     LOG (INFO) << BOLDGREEN << "Event#" << (*cEventIter)->GetEventCount() << " Trigger#" << +cTriggerId 
                    //         << " in a burst of " << (1+ cTriggerMult) 
                    //         << " number of S-clusters match is " << +cNmatch
                    //         << " full S-cluster match is " << +cFmatch 
                    //         << RESET;
                    cMatchedEvents += (cFmatch&&cNmatch) ? 1 : 0;
                    cEventIter += (1+cTriggerMult);
                }while(cEventIter < cEvents.end());
                if( cMatchedEvents == cNevents ){ 
                    std::pair<uint8_t, uint8_t> cComb; 
                    cComb.first = cPhase;
                    cComb.second = cWord;
                    cGoodCombinations.push_back( cComb );
                    LOG (INFO) << BOLDGREEN << "All events match for, LatencyRx320 of " 
                        << +cComb.first << " , LatencyRx40 " 
                        << +cComb.second << " full matching of S-clusters in MPA data" 
                        << RESET;
                }
            }
        }
    }

    size_t cL1CombIndx=0;
    for( auto cComb : cGoodCombinations )
    {
        if( cL1CombIndx > 0 ) continue;
        // now run stub alignment procedure 
        LOG (INFO) << BOLDYELLOW << "Scanning Stub alignment parameters for L1 alignmnet parameters.." << RESET;
        fReadoutChipInterface->WriteChipReg(pChip, "L1InputPhase", cComb.first);
        fReadoutChipInterface->WriteChipReg(pChip, "LatencyRx40", cComb.second); 
        fReadoutChipInterface->WriteChipReg(pChip, "StubMode", 0 );
        fReadoutChipInterface->WriteChipReg(pChip, "StubWindow", 1   );
        
        // I think that the sampling phase or the stub line should be about the same 
        // as the L1 line 
        // if the lines between SSAs and MPAs on the hybrid are matched
        // and I think they are
        uint8_t cStartPhase = (cComb.first == 0 ) ? cComb.first : cComb.first-1; 
        uint8_t cEndPhase = cComb.first+2;
        std::vector<std::pair<uint8_t, uint8_t>> cGoodCombinationsStubs; 
        cGoodCombinationsStubs.clear();
        int cGoodStubDelay=0;
        for(int cStubAddDelay = 0; cStubAddDelay <= 5 ; cStubAddDelay++)
        { 
            if( cGoodCombinationsStubs.size() > 0 ) continue;
            LOG (INFO) << BOLDMAGENTA << "Additional stub data delay of " << cStubAddDelay << RESET;
            for(uint8_t cPhase = cStartPhase; cPhase < cEndPhase; cPhase++)
            {

                if( cGoodCombinationsStubs.size() > 0 ) continue;
                for(uint8_t cRetime = 0; cRetime < 8; cRetime++)
                {

                    if( cGoodCombinationsStubs.size() > 0 ) continue;
                    auto    cStubOffset = (*cBoardIter)->getStubOffset() + cStubAddDelay ;
                    size_t cStubDelay = pLatency - cStubOffset - cRetime; // stub latency
                    fReadoutChipInterface->WriteChipReg(pChip, "RetimePix", cRetime);
                    fReadoutChipInterface->WriteChipReg(pChip, "StubInputPhase", cPhase);

                    //LOG (INFO) << BOLDYELLOW  << "Writing common_stubdata_delay " << cStubDelay << " [ReTime pix is set to " << +cRetime << " ]" << std::endl;
                    fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubDelay);
                    ReadNEvents((*cBoardIter), cNevents);
                    const std::vector<Event*>& cEvents = this->GetEvents();

                    LOG (INFO) << BOLDMAGENTA << "LatencyRx320 for stubs of " << +cPhase <<  " ReTime of " << +cRetime << RESET;
                    for( size_t cTriggerId=0; cTriggerId < (1+cTriggerMult) ; cTriggerId++)
                    {
                        size_t cMatchedEvents=0;
                        auto cEventIter = cEvents.begin() + cTriggerId ;
                        do
                        {   
                            if( cEventIter >= cEvents.end() ) break; 
                            bool cNmatch=true;
                            auto cPclus = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(pChip->getHybridId(), pChip->getId());
                            auto cSclus = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(pChip->getHybridId(), pChip->getId());
                            auto cStubs = static_cast<D19cCic2Event*>(*cEventIter)->StubVector(pChip->getHybridId(), pChip->getId());
                            cNmatch = cNmatch && (cStubs.size() == pInjections.size() && cPclus.size() == pInjections.size() && cSclus.size() == pInjections.size() );
                            if( cStubs.size() != 0 ) 
                                LOG (INFO) << BOLDBLUE << "Trigger#" << +cTriggerId << " in a burst of " << (1+ cTriggerMult) 
                                                    << " MPA" << +pChip->getId() << " found " << cSclus.size()
                                                    << " S clusters and " 
                                                    << cPclus.size() 
                                                    << " P clusters in L1 data from MPA#" << +pChip->getId()
                                                    << " also have " << +cStubs.size() << " stbs."
                                                    << RESET;
                            if( cStubs.size() == pInjections.size() )
                            {
                                size_t cStubCntr=0;
                                for( auto cStub : cStubs)
                                {
                                    LOG (INFO) << BOLDCYAN << "\t\tStub#" << +cStubCntr 
                                        << " Position " << +cStub.getPosition() << " - Row " << +cStub.getRow() << " - Bend " << +cStub.getBend() << RESET;
                                    cStubCntr++;
                                }
                            }
                            cEventIter += (1+cTriggerMult);
                            cMatchedEvents += (cNmatch) ? 1 : 0; 
                        }while(cEventIter < cEvents.end());
                        if( cMatchedEvents == cNevents ) 
                        {
                            cGoodStubDelay = cStubAddDelay; 
                            std::pair<uint8_t, uint8_t> cComb; 
                            cComb.first = cPhase;
                            cComb.second = cRetime;
                            cGoodCombinationsStubs.push_back( cComb );
                            LOG (INFO) << BOLDGREEN << "All events stubs match for, LatencyRx320 of " 
                                << +cComb.first << " , ReTimePix " 
                                << +cComb.second << " full matching of S-clusters in MPA data" 
                                << RESET;
                        }
                    }
                } 
            }
        }
        (*cBoardIter)->setStubOffset((*cBoardIter)->getStubOffset() + cGoodStubDelay);

        LOG (INFO) << BOLDMAGENTA << "Summary of SSA-MPA data alignment" << RESET;
        LOG (INFO) << BOLDMAGENTA << "LatencyRx320 of " << +cComb.first << " , LatencyRx40 " << +cComb.second << " full matching of S-clusters in MPA data" << RESET;
        for(auto cCombStbs : cGoodCombinationsStubs)
        {
            LOG (INFO) << BOLDMAGENTA << "\t.. Stub data : LatencyRx320 of " << +cCombStbs.first << " , ReTimePix " << +cCombStbs.second << " full matching of stub data in MPA" << RESET;
            fReadoutChipInterface->WriteChipReg(pChip, "StubInputPhase", cCombStbs.first);
            fReadoutChipInterface->WriteChipReg(pChip, "RetimePix", cCombStbs.second);
        }
        cL1CombIndx++;
    }
    return cGoodCombinations;
}
bool PSAlignment::AlignL1Inputs(BeBoard* pBoard)
{
    bool cPhaseFound = true;
    LOG(INFO) << BOLDBLUE << "Aligning MPA L1 inputs.." << RESET;
    //uint32_t cNevents = 10;
    //float cFraction = 1.0;
    
    // check trigger source
    // and reload
    Injection cInjection;
    std::vector<Injection> cInjections;
    cInjection.fRow = 45;
    cInjection.fColumn = 9;
    cInjections.push_back( cInjection );
    cInjection.fRow = 20;
    cInjection.fColumn = 7;
    cInjections.push_back( cInjection );
    
    // check trigger source
    // and reload
    uint16_t cTriggerSrc         = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    uint16_t cOriginalTriggerSrc = cTriggerSrc;
    uint8_t  cOriginalTLUconfig  = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled");
    cTriggerSrc                  = (cTriggerSrc == 6) ? cTriggerSrc : 6;
    LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    
    // uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    // cTriggerSrc = (cTriggerSrc == 6) ? cTriggerSrc : 6;
    // std::vector<std::pair<std::string, uint32_t>> cRegVec;
    // cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    // cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    // fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    
    LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    auto cTriggerMult = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint16_t cDelay   = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    int     cOptimalOffset     = -1 + (cTriggerMult > 1);
    uint16_t cLatency = cDelay + cOptimalOffset;
    LOG(DEBUG) << BOLDMAGENTA << "Expect correct latency to be " << +cLatency << RESET;


    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                // make sure L1 latency is configured
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency);
                    (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, cInjections);
                }
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency - 1);
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                    fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x01);
                    for( auto cInjection : cInjections ) 
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cInjection.fRow), 0x9);
                    }
                }
            } // chip
        } // hybrid
    } // optica]l group

     for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                // make sure L1 latency is configured
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    auto cStbOffset = pBoard->getStubOffset();
                    auto cCombinations = AlignChip(cChip, cInjections, cLatency);
                    pBoard->setStubOffset(cStbOffset);
                }
            } // chip
        } // hybrid
    } // optica]l group

    // // scan phase and check L1
    // std::vector<std::pair<uint8_t, uint8_t>> cGoodCombinations; 
    // cGoodCombinations.clear();
    // for( uint8_t cPhase = 2; cPhase < 4; cPhase++) 
    // {
    //     for(uint8_t cWord = 0; cWord < 16; cWord++)
    //     {
    //         for(auto cOpticalReadout: *pBoard)
    //         {
    //             for(auto cHybrid: *cOpticalReadout)
    //             {
    //                 for(auto cChip: *cHybrid)
    //                 {
    //                     if( cChip->getFrontEndType() != FrontEndType::MPA ) continue;
    //                     fReadoutChipInterface->WriteChipReg(cChip, "L1InputPhase", cPhase);
    //                     fReadoutChipInterface->WriteChipReg(cChip, "LatencyRx40", cWord); 
    //                 }
    //             }
    //         }
    //         ReadNEvents(pBoard, cNevents);
    //         const std::vector<Event*>& cEvents = this->GetEvents();
    //         if( cEvents.size() == 0 ) continue;

    //         LOG(INFO) << BOLDBLUE << "Setting L1 input sampling phase MPAs to " << +cPhase << " and Rx40 delay to " << +cWord << RESET;
    //         // start at the beginning + trigger id in burst 
    //         for( size_t cTriggerId=0; cTriggerId < cTriggerMult+1 ; cTriggerId++)
    //         {
    //             size_t cMatchedEvents=0;
    //             auto cEventIter = cEvents.begin() + cTriggerId ;
    //             do
    //             {   
    //                 if( cEventIter >= cEvents.end() ) break; 
    //                 bool cNmatch=true;
    //                 bool cFmatch=true;
    //                 for(auto cOpticalGroup: *pBoard)
    //                 {
    //                     for(auto cHybrid: *cOpticalGroup)
    //                     {
    //                         for(auto cChip: *cHybrid)
    //                         {
    //                             if (cChip->getFrontEndType() == FrontEndType::SSA ) continue; 
    //                             auto cPclus = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(cHybrid->getId(), cChip->getId());
    //                             auto cSclus = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(cHybrid->getId(), cChip->getId());
    //                             cNmatch = cNmatch && ( cSclus.size() == cInjections.size() && cPclus.size() == cInjections.size() ) ;
    //                             cFmatch = cNmatch; 
    //                             if( cNmatch ) 
    //                             {
    //                                 LOG (DEBUG) << BOLDBLUE << "Trigger#" << +cTriggerId << " in a burst of " << (1+ cTriggerMult) 
    //                                             << " MPA" << +cChip->getId() << " found " << cSclus.size()
    //                                             << " S clusters and " 
    //                                             << cPclus.size() 
    //                                             << " P clusters in L1 data from MPA#" << +cChip->getId()
    //                                             << RESET;
    //                                 for( size_t cIndx=0; cIndx < cInjections.size(); cIndx++)
    //                                 {
    //                                     cFmatch = cFmatch && ( cPclus[cIndx].fAddress == cSclus[cIndx].fAddress); 
    //                                     if( ( cPclus[cIndx].fAddress == cSclus[cIndx].fAddress) )
    //                                         LOG (DEBUG) << BOLDGREEN << "Exact match found " 
    //                                                 << BOLDYELLOW << " P-cluster in row " << +cPclus[cIndx].fAddress
    //                                                 << " column " << +cPclus[cIndx].fZpos << " width is " << +cPclus[cIndx].fWidth
    //                                                 << BOLDCYAN << " S-cluster in row " << +cSclus[cIndx].fAddress
    //                                                 << " column " << (0) << " width is " << +cSclus[cIndx].fWidth
    //                                                 << RESET;
    //                                     else
    //                                         LOG (DEBUG) << BOLDRED << "Exact match not found " 
    //                                                 << BOLDYELLOW << " P-cluster in row " << +cPclus[cIndx].fAddress
    //                                                 << " column " << +cPclus[cIndx].fZpos << " width is " << +cPclus[cIndx].fWidth
    //                                                 << BOLDCYAN << " S-cluster in row " << +cSclus[cIndx].fAddress
    //                                                 << " column " << (0) << " width is " << +cSclus[cIndx].fWidth
    //                                                 << RESET;
    //                                 }
    //                             }
    //                         }// chip vector 
    //                     }// hybrid vector 
    //                 }// optical group vector 
    //                 // if( cNmatch && cFmatch )
    //                 //     LOG (INFO) << BOLDGREEN << "Event#" << (*cEventIter)->GetEventCount() << " Trigger#" << +cTriggerId 
    //                 //         << " in a burst of " << (1+ cTriggerMult) 
    //                 //         << " number of S-clusters match is " << +cNmatch
    //                 //         << " full S-cluster match is " << +cFmatch 
    //                 //         << RESET;
    //                 cMatchedEvents += (cFmatch&&cNmatch) ? 1 : 0;
    //                 cEventIter += (1+cTriggerMult);
    //             }while(cEventIter < cEvents.end());
    //             if( cMatchedEvents == cNevents*cFraction ){ 
    //                 std::pair<uint8_t, uint8_t> cComb; 
    //                 cComb.first = cPhase;
    //                 cComb.second = cWord;
    //                 cGoodCombinations.push_back( cComb );
    //                 LOG (INFO) << BOLDGREEN << "All events match for, LatencyRx320 of " 
    //                     << +cComb.first << " , LatencyRx40 " 
    //                     << +cComb.second << " full matching of S-clusters in MPA data" 
    //                     << RESET;
    //             }
    //         }
    //     }
    // }

    // size_t cL1CombIndx=0;
    // for(auto cComb : cGoodCombinations)
    // {
    //     if( cL1CombIndx > 0 ) continue;
    //     // now run stub alignment procedure 
    //     LOG (INFO) << BOLDYELLOW << "Scanning Stub alignment parameters for L1 alignmnet parameters.." << RESET;
    //     for(auto cOpticalReadout: *pBoard)
    //     {
    //         for(auto cHybrid: *cOpticalReadout)
    //         {
    //             for(auto cChip: *cHybrid)
    //             {
    //                 if( cChip->getFrontEndType() != FrontEndType::MPA ) continue;

    //                 fReadoutChipInterface->WriteChipReg(cChip, "L1InputPhase", cComb.first);
    //                 fReadoutChipInterface->WriteChipReg(cChip, "LatencyRx40", cComb.second); 
    //                 fReadoutChipInterface->WriteChipReg(cChip, "StubMode", 0 );
    //                 fReadoutChipInterface->WriteChipReg(cChip, "StubWindow", 1   );
    //             }
    //         }
    //     }
    //     // I think that the sampling phase or the stub line should be about the same 
    //     // as the L1 line 
    //     // if the lines between SSAs and MPAs on the hybrid are matched
    //     // and I think they are
    //     uint8_t cStartPhase = (cComb.first == 0 ) ? cComb.first : cComb.first-1; 
    //     uint8_t cEndPhase = cComb.first+2;
    //     std::vector<std::pair<uint8_t, uint8_t>> cGoodCombinationsStubs; 
    //     cGoodCombinationsStubs.clear();
    //     int cGoodStubDelay=0;
    //     for(int cStubAddDelay = 0; cStubAddDelay <= 5 ; cStubAddDelay++)
    //     { 
    //         LOG (INFO) << BOLDMAGENTA << "Additional stub data delay of " << cStubAddDelay << RESET;
    //         for(uint8_t cPhase = cStartPhase; cPhase < cEndPhase; cPhase++)
    //         {
    //             for(uint8_t cRetime = 4; cRetime < 6; cRetime++)
    //             {
    //                 auto    cStubOffset = pBoard->getStubOffset() + cStubAddDelay ;
    //                 size_t cStubDelay = cLatency - cStubOffset - cRetime; // stub latency
    //                 for(auto cOpticalReadout: *pBoard)
    //                 {
    //                     for(auto cHybrid: *cOpticalReadout)
    //                     {
    //                         for(auto cChip: *cHybrid)
    //                         {
    //                             if( cChip->getFrontEndType() != FrontEndType::MPA ) continue;

    //                             fReadoutChipInterface->WriteChipReg(cChip, "RetimePix", cRetime);
    //                             fReadoutChipInterface->WriteChipReg(cChip, "StubInputPhase", cPhase);
    //                         }
    //                     }
    //                 }

    //                 //LOG (INFO) << BOLDYELLOW  << "Writing common_stubdata_delay " << cStubDelay << " [ReTime pix is set to " << +cRetime << " ]" << std::endl;
    //                 fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubDelay);
    //                 ReadNEvents(pBoard, cNevents);
    //                 const std::vector<Event*>& cEvents = this->GetEvents();

    //                 LOG (INFO) << BOLDMAGENTA << "LatencyRx320 for stubs of " << +cPhase <<  " ReTime of " << +cRetime << RESET;
    //                 for( size_t cTriggerId=0; cTriggerId < (1+cTriggerMult) ; cTriggerId++)
    //                 {
    //                     size_t cMatchedEvents=0;
    //                     auto cEventIter = cEvents.begin() + cTriggerId ;
    //                     do
    //                     {   
    //                         if( cEventIter >= cEvents.end() ) break; 
    //                         bool cNmatch=true;
    //                         for(auto cOpticalGroup: *pBoard)
    //                         {
    //                             for(auto cHybrid: *cOpticalGroup)
    //                             {
    //                                 for(auto cChip: *cHybrid)
    //                                 {
    //                                     if (cChip->getFrontEndType() == FrontEndType::SSA ) continue; 
    //                                     auto cPclus = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(cHybrid->getId(), cChip->getId());
    //                                     auto cSclus = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(cHybrid->getId(), cChip->getId());
    //                                     auto cStubs = static_cast<D19cCic2Event*>(*cEventIter)->StubVector(cHybrid->getId(), cChip->getId());
    //                                     cNmatch = cNmatch && (cStubs.size() == cInjections.size() && cPclus.size() == cInjections.size() && cSclus.size() == cInjections.size() );
    //                                     if( cStubs.size() != 0 ) 
    //                                         LOG (INFO) << BOLDBLUE << "Trigger#" << +cTriggerId << " in a burst of " << (1+ cTriggerMult) 
    //                                                             << " MPA" << +cChip->getId() << " found " << cSclus.size()
    //                                                             << " S clusters and " 
    //                                                             << cPclus.size() 
    //                                                             << " P clusters in L1 data from MPA#" << +cChip->getId()
    //                                                             << " also have " << +cStubs.size() << " stbs."
    //                                                             << RESET;
    //                                     if( cStubs.size() == cInjections.size() )
    //                                     {
    //                                         size_t cStubCntr=0;
    //                                         for( auto cStub : cStubs)
    //                                         {
    //                                             LOG (INFO) << BOLDCYAN << "\t\tStub#" << +cStubCntr 
    //                                                 << " Position " << +cStub.getPosition() << " - Row " << +cStub.getRow() << " - Bend " << +cStub.getBend() << RESET;
    //                                             cStubCntr++;
    //                                         }
    //                                     }
    //                                 }// chip vector 
    //                             }// hybrid vector 
    //                         }// optical group vector 
    //                         cEventIter += (1+cTriggerMult);
    //                         cMatchedEvents += (cNmatch) ? 1 : 0; 
    //                     }while(cEventIter < cEvents.end());
    //                     if( cMatchedEvents == cNevents*cFraction ) 
    //                     {
    //                         cGoodStubDelay = cStubAddDelay; 
    //                         std::pair<uint8_t, uint8_t> cComb; 
    //                         cComb.first = cPhase;
    //                         cComb.second = cRetime;
    //                         cGoodCombinationsStubs.push_back( cComb );
    //                         LOG (INFO) << BOLDGREEN << "All events stubs match for, LatencyRx320 of " 
    //                             << +cComb.first << " , ReTimePix " 
    //                             << +cComb.second << " full matching of S-clusters in MPA data" 
    //                             << RESET;
    //                     }
    //                 }
    //             } 
    //         }
    //     }
    //     pBoard->setStubOffset(pBoard->getStubOffset() + cGoodStubDelay);

    //     LOG (INFO) << BOLDMAGENTA << "Summary of SSA-MPA data alignment" << RESET;
    //     LOG (INFO) << BOLDMAGENTA << "LatencyRx320 of " << +cComb.first << " , LatencyRx40 " << +cComb.second << " full matching of S-clusters in MPA data" << RESET;
    //     for(auto cCombStbs : cGoodCombinationsStubs)
    //     {
    //         LOG (INFO) << BOLDMAGENTA << "\t.. Stub data : LatencyRx320 of " << +cCombStbs.first << " , ReTimePix " << +cCombStbs.second << " full matching of stub data in MPA" << RESET;
    //         for(auto cOpticalReadout: *pBoard)
    //         {
    //             for(auto cHybrid: *cOpticalReadout)
    //             {
    //                 for(auto cChip: *cHybrid)
    //                 {
    //                     if( cChip->getFrontEndType() != FrontEndType::MPA ) continue;

    //                     fReadoutChipInterface->WriteChipReg(cChip, "StubInputPhase", cCombStbs.first);
    //                     fReadoutChipInterface->WriteChipReg(cChip, "RetimePix", cCombStbs.second);
    //                 }
    //             }
    //         }
    //     }
    //     cL1CombIndx++;    
    // }
    
    // set everything back to original values .. like I wasn't here
    // reset fast command registers
    LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindPackageDelay Resetting BeBoards regs back to their original values" << RESET;
    cRegVec.clear();
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cOriginalTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", cOriginalTLUconfig});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);


    // for(uint8_t cChipId = 0+8; cChipId < 8+8; cChipId++)
    // {
    //     bool nochip = true;
    //     if(!cCurPhaseFound) cPhaseFound = false;
    //     cCurPhaseFound = false;
    //     for(uint8_t cPhase = 0; cPhase < 8; cPhase++)
    //     {
    //         if(cCurPhaseFound == true) break;
    //         for(uint8_t cES = 2; cES < 4; cES++)
    //         {
    //             if(cCurPhaseFound == true) break;
    //             for(uint8_t cWord = 0; cWord < 16; cWord++)
    //             {
    //                 if(cCurPhaseFound == true) break;
    //                 for(auto cOpticalReadout: *pBoard)
    //                 {
    //                     for(auto cHybrid: *cOpticalReadout)
    //                     {
    //                         for(auto cChip: *cHybrid)
    //                         {
    //                             if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
    //                             if(cChip->getId() != cChipId) continue;
    //                             nochip = false;
    //                             fReadoutChipInterface->WriteChipReg(cChip, "L1InputPhase", cPhase);
    //                             fReadoutChipInterface->WriteChipReg(cChip, "LatencyRx40", cWord);
    //                             fReadoutChipInterface->WriteChipReg(cChip, "EdgeSelT1Raw", cES);
    //                         } // chip
    //                     }     // hybrid
    //                 }         // optical group
    //                 if(nochip)
    //                 {
    //                     cCurPhaseFound = true;
    //                     continue;
    //                 }
    //                 // now read data
    //                 LOG(INFO) << BOLDBLUE << "Setting L1 input sampling phase and word for MPA#" << +cChipId << " on hybrid to " << +cPhase << " and " << +cWord << RESET;
    //                 ReadNEvents(pBoard, cNevents);
    //                 const std::vector<Event*>& cEvents = this->GetEvents();
    //                 LOG(INFO) << BOLDBLUE << "Checking phase by reading back " << +cEvents.size() << " events from the FC7 ..." << RESET;
    //                 uint32_t MatchNPclustot = 0;
    //                 uint32_t MatchNSclustot = 0;
    //                 for(auto& ev: cEvents)
    //                 {
    //                     for(auto cOpticalReadout: *pBoard)
    //                     {
    //                         for(auto cHybrid: *cOpticalReadout)
    //                         {
    //                             for(auto cChip: *cHybrid)
    //                             {
    //                                 if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
    //                                 if(cChip->getId() != cChipId) continue;

    //                                 std::vector<PCluster> Pclus = static_cast<D19cCic2Event*>(ev)->GetPixelClusters(cHybrid->getId(), cChip->getId());
    //                                 std::vector<SCluster> Sclus = static_cast<D19cCic2Event*>(ev)->GetStripClusters(cHybrid->getId(), cChip->getId());

    //                                 for(auto& pc: Pclus)
    //                                 {
    //                                     std::cout << "-------------------------------PIXELS-------------------------------"<< std::endl;
    //                                     std::cout << "fAddress "<<+pc.fAddress<<std::endl;
    //                                     std::cout << "fWidth "<<+pc.fWidth<< std::endl;
    //                                     std::cout << "fZpos "<<+pc.fZpos << std::endl;
    //                                     if((cInjection.fRow) == pc.fAddress and (cInjection.fColumn - 1) == pc.fZpos) MatchNPclustot += 1;
    //                                 }
    //                                 for(auto& sc: Sclus)
    //                                 {
    //                                     std::cout << "-------------------------------STRIPS-------------------------------"<< std::endl;
    //                                     std::cout << "fAddress? "<<+sc.fAddress<<std::endl;
    //                                     std::cout << "fWidth "<<+sc.fWidth<< std::endl;
    //                                     std::cout << "fMip "<<+sc.fMip << std::endl<< std::endl;
    //                                     if((cInjection.fRow) == sc.fAddress ) MatchNSclustot += 1;
    //                                 }
    //                             }
    //                         }
    //                     }
    //                 }
    //                 if((MatchNPclustot == (cNevents)*cFraction) and (MatchNSclustot == (cNevents)*cFraction ))
    //                 {
    //                     LOG(INFO) << BOLDGREEN << "-----" << RESET;
    //                     LOG(INFO) << BOLDGREEN << "Phase and Word alignment for MPA#" << +cChipId << " completed." << RESET;
    //                     LOG(INFO) << BOLDGREEN << "Phase:" << +cPhase << " Word:" << +cWord << " Edge Sel:" << +cES << RESET;
    //                     LOG(INFO) << BOLDGREEN << "-----" << RESET;
    //                     cCurPhaseFound = true;
    //                 }
    //                 else
    //                 {
    //                     LOG(INFO) << BOLDRED << "-----" << RESET;
    //                     LOG(INFO) << BOLDRED << "Phase and Word alignment for MPA#" << +cChipId << " completed." << RESET;
    //                     LOG(INFO) << BOLDRED << "Phase:" << +cPhase << " Word:" << +cWord << " Edge Sel:" << +cES << RESET;
    //                     LOG(INFO) << BOLDRED << "-----" << RESET;
    //                 }
    //             } // word loop
    //         }     // ES loop
    //     }         // phase loop
    // }             // chip id loop [up-to 8 chips per hybrid ]
    return cPhaseFound;
}
bool PSAlignment::Align()
{
    LOG(INFO) << BOLDBLUE << "Starting MPA-SSA alignment procedure .... " << RESET;

    bool cl1Aligned   = true;
    bool cStubAligned = true;
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->ChipReSync(cBoard);
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();

        cl1Aligned = cl1Aligned && this->AlignL1Inputs(cBoard);

        //cStubAligned = cStubAligned && this->AlignStubInputs(cBoard);

        LOG(INFO) << BOLDBLUE << "L1 alignemnt " << RESET;
        cl1Aligned ? LOG(INFO) << BOLDGREEN << "Succeeded" << RESET : LOG(INFO) << BOLDRED << "Failed" << RESET;

        // LOG(INFO) << BOLDBLUE << "Stub alignemnt " << RESET;
        // cStubAligned ? LOG(INFO) << BOLDGREEN << "Succeeded" << RESET : LOG(INFO) << BOLDRED << "Failed" << RESET;
    }
    return cStubAligned && cl1Aligned;
}
void PSAlignment::writeObjects() {}
// State machine control functions
void PSAlignment::Running()
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
    }
}

void PSAlignment::Stop()
{
    dumpConfigFiles();

    // Destroy();
}

void PSAlignment::Pause() {}

void PSAlignment::Resume() {}
