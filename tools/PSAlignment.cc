#include "PSAlignment.h"

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

PSAlignment::PSAlignment() : Tool() { 
    fRegMapContainer.reset(); 
}

PSAlignment::~PSAlignment() {}
void PSAlignment::Reset()
{
    // set everything back to original values .. like I wasn't here
    bool cWithPS = false;
    LOG (INFO) << BOLDYELLOW << "PSAlignment::Reset - Resetting BE board and chip registers I've touched/modified" << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(DEBUG) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap) { cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second)); }
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);

        std::vector<std::string> cRegsMod{"OutSetting","LatencyRx320","LatencyRx40","RetimePix","EdgeSelTrig","EdgeSelT1Raw"};
        for(auto cOpticalGroup: *cBoard)
        {
            bool cWithLpGBT = (cOpticalGroup->flpGBT != nullptr);
            for(auto cHybrid: *cOpticalGroup)
            {
                auto cType    = FrontEndType::SSA;
                bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                cType         = FrontEndType::MPA;
                bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                bool cIsPS    = (cWithSSA && cWithMPA) && cWithLpGBT;
                cWithPS       = cWithPS || cIsPS;
                LOG(DEBUG) << BOLDBLUE << "BackEndAlignment::Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;
                for(auto cChip: *cHybrid)
                {
                    if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->UpdateModifiedRegisterMap(cChip);
                    auto cModMap = fReadoutChipInterface->GetModifiedRegisterMap(cChip);
                    LOG(DEBUG) << BOLDYELLOW << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    for(auto cMapItem: cModMap)
                    {
                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        bool cLeaveReg=false;
                        for(auto cReg : cRegsMod)
                        {
                            cLeaveReg = cLeaveReg || (cMapItem.first.find(cReg) != std::string::npos );
                        }
                        if( !cLeaveReg ) 
                        {
                            LOG(DEBUG) << BOLDYELLOW << "BackEndAlignment::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to "
                                       << cMapItem.second.fValue << RESET;
                            fReadoutChipInterface->WriteChipReg(cChip, cMapItem.first, cMapItem.second.fValue);
                        }
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
                            LOG(DEBUG) << BOLDBLUE << "Configuring MPA output register [mapping between output bits and output pads] .... Output# " << +cIndx << RESET;
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
    auto    cStubOffset = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->getStubOffset();
    // check trigger source
    // and reload
    uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    cTriggerSrc = (cTriggerSrc == 6) ? cTriggerSrc : 6;
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    uint16_t cDelay   = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    uint16_t cLatency = cDelay - 1;
    LOG(INFO) << BOLDMAGENTA << "Expect correct L1 latency to be " << +cLatency << RESET;

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
                    fReadoutChipInterface->WriteChipReg(cChip, "DigitalSync_P" + std::to_string(cGpix), 0x01); // enable 1 pix
                }
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency - 1);
                    fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_L_ALL", 0x01);
                    fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_H_ALL", 0x01);
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

                static_cast<PSInterface*>(fReadoutChipInterface)->Activate_ps(cChip, 2);
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
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // configure SSA to inject digitally
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                uint8_t cChipId=cChip->getId();
                if(!cCurPhaseFound) cPhaseFound = false; // all chips need to be tuned
                cCurPhaseFound = false;
                for(uint8_t cPhase = 3; cPhase < 4; cPhase++)
                {

                    if(cCurPhaseFound == true) break; // break loops if chip phase already found
                    for(uint8_t cRetime = 5; cRetime < 8; cRetime++)
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
                                    fReadoutChipInterface->WriteChipReg(cChip, "StubInputPhase", cPhase);
                                    fReadoutChipInterface->WriteChipReg(cChip, "RetimePix", cRetime);
                                } // chip
                            }     // hybrid
                        }         // optical group
                        std::cout << "Writing common_stubdata_delay " << +cLatency<< ","<<+cStubOffset<<","<<+cRetime<< std::endl;

                        size_t writeslat = cLatency - (cStubOffset + cRetime); // stub latency
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
                                            //std::cout << "getPosition " << +st.getPosition() << std::endl;
                                            //std::cout << "getBend " << +st.getBend() << std::endl;
                                            //std::cout << "getRow " << +st.getRow() << std::endl;
                                        }
                                    }
                                }
                            }
                        }
                        std::cout << "MatchNStubtot " << MatchNStubtot << std::endl;
                        if(MatchNStubtot > (cNevents-1))
                        {
                            LOG(INFO) << BOLDGREEN << "-----" << RESET;
                            LOG(INFO) << BOLDGREEN << "Stub input sampling phase and pixel retime for MPA#" << +cChipId << " completed." << RESET;
                            LOG(INFO) << BOLDGREEN << "Phase:" << +cPhase << " Retime:" << +cRetime << RESET;
                            LOG(INFO) << BOLDGREEN << "-----" << RESET;
                            cCurPhaseFound = true;
                        }

                    } // retime loop
                }     // phase loop
            }
        }
    }

    return cPhaseFound;
}
bool PSAlignment::AlignL1Inputs(BeBoard* pBoard)
{
    bool cPhaseFound = true;
    LOG(INFO) << BOLDBLUE << "Aligning MPA L1 inputs.." << RESET;
    uint32_t cNevents = 10;

    // check trigger source
    // and reload
    uint8_t  cRow        = 9; // random pixel to activate -- could be configurable
    uint8_t  cCol        = 45;
    uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    cTriggerSrc = (cTriggerSrc == 6) ? cTriggerSrc : 6;
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    uint16_t cDelay   = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    uint16_t cLatency = cDelay - 1;
    LOG(INFO) << BOLDMAGENTA << "Expect correct L1 latency to be " << +cLatency << RESET;

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                // make sure L1 latency is configured
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                    uint32_t cGpix = static_cast<MPA*>(cChip)->PNglobal(std::pair<uint32_t, uint32_t>(cRow, cCol));
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency);
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_P" + std::to_string(cGpix), 0x37);
                    fReadoutChipInterface->WriteChipReg(cChip, "DigitalSync_P" + std::to_string(cGpix), 0x01); // enable 1 pix
                }
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency - 1);
                    fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_L_ALL", 0x01);
                    fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_H_ALL", 0x01);
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cCol), 0x9);
                }
            } // chip
        }     // hybrid
    }         // optica]l group

    // scan phase and check L1
    bool cCurPhaseFound = true;
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // configure SSA to inject digitally
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                uint8_t cChipId=cChip->getId();

                if(!cCurPhaseFound) cPhaseFound = false;
                cCurPhaseFound = false;
                for(uint8_t cPhase = 3; cPhase < 8; cPhase++)
                {
                    if(cCurPhaseFound == true) break;
                    for(uint8_t cES = 3; cES < 4; cES++)
                    {
                        if(cCurPhaseFound == true) break;
                        for(uint8_t cWord = 0; cWord < 5; cWord++)
                        {
                            if(cCurPhaseFound == true) break;
                            for(auto cOpticalReadout: *pBoard)
                            {
                                for(auto cHybrid: *cOpticalReadout)
                                {
                                    for(auto cChip: *cHybrid)
                                    {
                                        if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
                                        if(cChip->getId() != cChipId) continue;
                                        fReadoutChipInterface->WriteChipReg(cChip, "L1InputPhase", cPhase);
                                        fReadoutChipInterface->WriteChipReg(cChip, "LatencyRx40", cWord);
                                        fReadoutChipInterface->WriteChipReg(cChip, "EdgeSelT1Raw", cES);
                                    } // chip
                                }     // hybrid
                            }         // optical group

                            // now read data
                            LOG(INFO) << BOLDBLUE << "Setting L1 input sampling phase and word for MPA#" << +cChipId << " on hybrid to " << +cPhase << " and " << +cWord << RESET;
                            ReadNEvents(pBoard, cNevents);
                            const std::vector<Event*>& cEvents = this->GetEvents();
                            LOG(INFO) << BOLDBLUE << "Checking phase by reading back " << +cEvents.size() << " events from the FC7 ..." << RESET;
                            uint32_t MatchNPclustot = 0;
                            uint32_t MatchNSclustot = 0;
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

                                            std::vector<PCluster> Pclus = static_cast<D19cCic2Event*>(ev)->GetPixelClusters(cHybrid->getId(), cChip->getId());
                                            std::vector<SCluster> Sclus = static_cast<D19cCic2Event*>(ev)->GetStripClusters(cHybrid->getId(), cChip->getId());

                                            for(auto& pc: Pclus)
                                            {
                                                 //std::cout << "-------------------------------PIXELS-------------------------------"<< std::endl;
                                                 //std::cout << "fAddress "<<+pc.fAddress<<std::endl;
                                                // std::cout << "fWidth "<<+pc.fWidth<< std::endl;
                                                 //std::cout << "fZpos "<<+pc.fZpos << std::endl;
                                                if((cCol) == pc.fAddress and (cRow - 1) == pc.fZpos)
                                                  {
                                                        //std::cout << "PIXPASS"<< std::endl;

                                                        MatchNPclustot += 1;
                                                  }
                                            }
                                            for(auto& sc: Sclus)
                                            {
                                                 //std::cout << "-------------------------------STRIPS-------------------------------"<< std::endl;
                                                 //std::cout << "fAddress? "<<+sc.fAddress<<std::endl;
                                                // std::cout << "fWidth "<<+sc.fWidth<< std::endl;
                                                 //std::cout << "fMip "<<+sc.fMip << std::endl<< std::endl;
                                                if((cCol) == sc.fAddress and sc.fMip == 1)
                                                {
                                                      //std::cout << "STRIPPASS"<< std::endl;
                                                      MatchNSclustot += 1;
                                                }

                                            }
                                        }
                                    }
                                }
                            }
                            std::cout << "MatchNPclustot "<<MatchNPclustot<< " MatchNSclustot "<<MatchNSclustot<<std::endl;
                            if((MatchNPclustot > (cNevents - 2)) and (MatchNSclustot > (cNevents - 2)))
                            {
                                LOG(INFO) << BOLDGREEN << "-----" << RESET;
                                LOG(INFO) << BOLDGREEN << "Phase and Word alignment for MPA#" << +cChipId << " completed." << RESET;
                                LOG(INFO) << BOLDGREEN << "Phase:" << +cPhase << " Word:" << +cWord << " Edge Sel:" << +cES << RESET;
                                LOG(INFO) << BOLDGREEN << "-----" << RESET;
                                cCurPhaseFound = true;
                            }
                        } // word loop
                    }     // ES loop
                }         // phase loop
            }             // chip id loop [up-to 8 chips per hybrid ]
        }
    }
    return cPhaseFound;
}
std::vector<std::pair<uint8_t, uint8_t>> PSAlignment::AlignL1(ReadoutChip* pChip, std::vector<Injection> pInjections )
{
    struct
    {
        bool operator()(PCluster a, PCluster b) const { return a.fAddress < b.fAddress; }
    } customSortPclus;
    struct
    {
        bool operator()(SCluster a, SCluster b) const { return a.fAddress < b.fAddress; }
    } customSortSclus;

    std::vector<std::pair<uint8_t, uint8_t>> cGoodCombinations;
    cGoodCombinations.clear();
    LOG(INFO) << BOLDMAGENTA << "PSAlignment::AlignL1  - aligning L1 and stub data for SSA-MPA pair#" << +pChip->getId() % 8 << RESET;
    auto     cBoardId     = pChip->getBeBoardId();
    auto     cBoardIter   = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    auto     cTriggerMult = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint32_t cNevents     = 10;

    uint8_t cStartPhaseL1 = 3; //to-do - set from xml 
    uint8_t cStopPhaseL1  = 5; //to-do - set from xml 
    bool    cOnlyFirst    = false;
    for(uint8_t cPhase = cStartPhaseL1; cPhase < cStopPhaseL1; cPhase++)
    {
        if(cOnlyFirst && cGoodCombinations.size() > 0) continue; // for now .. only the first one
        for(uint8_t cWord = 0; cWord < 8; cWord++)// goes to 16... for now scan to 8 
        {
            if(cOnlyFirst && cGoodCombinations.size() > 0) continue; // for now .. only the first one
            fReadoutChipInterface->WriteChipReg(pChip, "L1InputPhase", cPhase);
            fReadoutChipInterface->WriteChipReg(pChip, "LatencyRx40", cWord);

            ReadNEvents(*cBoardIter, cNevents);
            const std::vector<Event*>& cEvents = this->GetEvents();
            if(cEvents.size() == 0) continue;

            LOG(DEBUG) << BOLDBLUE << "\tSetting L1 input sampling phase MPAs to " << +cPhase << " and Rx40 delay to " << +cWord << " -- reading back " << cNevents << RESET;
            // start at the beginning + trigger id in burst
            for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++)
            {
                size_t cMatchedEvents = 0;
                auto   cEventIter     = cEvents.begin() + cTriggerId;
                do {
                    if(cEventIter >= cEvents.end()) break;
                    bool cNmatch = true;
                    bool cFmatch = true;
                    auto cPclus  = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(pChip->getHybridId(), pChip->getId());
                    auto cSclus  = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(pChip->getHybridId(), pChip->getId());
                    // sort P clusters by row 
                    std::sort(cPclus.begin(), cPclus.end(), customSortPclus);
                    // sort S clusters by row
                    std::sort(cSclus.begin(), cSclus.end(), customSortSclus);
                    cNmatch      = cNmatch && (cSclus.size() == pInjections.size() && cPclus.size() == pInjections.size());
                    cFmatch      = cNmatch;
                    if(cNmatch)
                    {
                        std::vector<SCluster> cMtchdSclstrs; 
                        std::vector<PCluster> cMtchdPclstrs;
                        // Check P-clusters  
                        for(auto cInjection : pInjections ) 
                        {
                            bool cMatchFound=false;
                            for( auto cPcluster : cPclus )
                            {
                                if( cMatchFound ) continue;
                                cMatchFound  = ( cPcluster.fAddress == cInjection.fRow ) && ( cPcluster.fZpos == cInjection.fColumn ) ;
                                if( cMatchFound )
                                {
                                    PCluster cMtchdPclstr;
                                    cMtchdPclstr.fAddress = cPcluster.fAddress;
                                    cMtchdPclstr.fZpos    = cPcluster.fZpos; 
                                    cMtchdPclstrs.push_back( cMtchdPclstr );
                                }
                            }
                            cFmatch = cFmatch && cMatchFound;
                        }

                        // check S-clusters 
                        for(auto cInjection : pInjections ) 
                        {
                            bool cMatchFound=false;
                            for( auto cScluster : cSclus )
                            {
                                if( cMatchFound ) continue;
                                cMatchFound  = ( cScluster.fAddress == cInjection.fRow ) ;
                                if( cMatchFound )
                                {
                                    SCluster cMtchdSclstr;
                                    cMtchdSclstr.fAddress = cInjection.fRow ; 
                                    cMtchdSclstrs.push_back( cMtchdSclstr );
                                }
                            }
                            cFmatch = cFmatch && cMatchFound;
                        }

                        if( cFmatch ) 
                        {
                            LOG(DEBUG) << BOLDGREEN << "\t\t Trigger#" << +cTriggerId << " Event#" << (*cEventIter)->GetEventCount() 
                                    << " in a burst of " << (1 + cTriggerMult) << " MPA" << +pChip->getId() << " found " << cSclus.size() << " matched S clusters and "
                                    << cPclus.size() << " matched P clusters in L1 data.  L1 input sampling phase is  " 
                                    << +cPhase << " Rx40 delay is " << +cWord << RESET;
                            for( uint8_t  cMatchId=0; cMatchId < cMtchdSclstrs.size() ; cMatchId++)
                            {
                                LOG(DEBUG) << BOLDGREEN << "\t\t\t Exact match found " << BOLDYELLOW << " S-cluster in row " << +cMtchdSclstrs[cMatchId].fAddress 
                                    << " P-cluster in row " << +cMtchdPclstrs[cMatchId].fAddress << " column " << +cMtchdPclstrs[cMatchId].fZpos 
                                    << RESET; 
                            }
                        }
                    }
                    // if( cNmatch && cFmatch )
                    //     LOG (INFO) << BOLDGREEN << "Event#" << (*cEventIter)->GetEventCount() << " Trigger#" << +cTriggerId
                    //         << " in a burst of " << (1+ cTriggerMult)
                    //         << " number of S-clusters match is " << +cNmatch
                    //         << " full S-cluster match is " << +cFmatch
                    //         << RESET;
                    cMatchedEvents += (cFmatch && cNmatch) ? 1 : 0;
                    cEventIter += (1 + cTriggerMult);
                } while(cEventIter < cEvents.end());
                if(cMatchedEvents >= (cNevents-1))//to allow for single triggers 
                {
                    std::pair<uint8_t, uint8_t> cComb;
                    cComb.first  = cPhase;
                    cComb.second = cWord;
                    cGoodCombinations.push_back(cComb);
                    LOG(INFO) << BOLDGREEN << "All events match for, LatencyRx320 of " << +cComb.first << " , LatencyRx40 [first]" << +(cComb.second & 0x3) << " LatencyRx40 [re-start] "
                              << +(cComb.second >> 2) << " full matching of S-clusters in MPA data" << RESET;
                }
            }
        }
    }
    return cGoodCombinations;
}
std::vector<std::pair<uint8_t, uint8_t>> PSAlignment::AlignStubs( ReadoutChip* pChip, std::vector<Injection> pInjections, uint16_t pLatency)
{
    struct
    {
        bool operator()(PCluster a, PCluster b) const { return a.fAddress < b.fAddress; }
    } customSortPclus;
    struct
    {
        bool operator()(SCluster a, SCluster b) const { return a.fAddress < b.fAddress; }
    } customSortSclus;
    struct
    {
        bool operator()(Stub a, Stub b) const { return a.fPosition < b.fPosition; }
    } customSortStubs;

    std::vector<std::pair<uint8_t, uint8_t>> cGoodCombinationsStubs;
    cGoodCombinationsStubs.clear();
    LOG(INFO) << BOLDMAGENTA << "PSAlignment::AlignChip  - aligning L1 and stub data for SSA-MPA pair#" << +pChip->getId() % 8 << RESET;
    auto     cBoardId     = pChip->getBeBoardId();
    auto     cBoardIter   = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    auto     cTriggerMult = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint32_t cNevents     = 10;

    int cGoodStubDelay = 0;
    uint8_t cStartPhase = 2;  //to-do - set from xml 
    uint8_t cEndPhase  = 7; //to-do - set from xml 
    bool    cOnlyFirst    = true;
    bool    cCheckL1      = true;
    for(int cStubAddDelay = (cTriggerMult==0)?3:0 ; cStubAddDelay <= 5; cStubAddDelay++)
    {
        if(cOnlyFirst && cGoodCombinationsStubs.size() > 0) continue;
        auto   cStubOffset = (*cBoardIter)->getStubOffset() + cStubAddDelay;
        LOG(INFO) << BOLDMAGENTA << "Additional stub data delay of " << cStubAddDelay << RESET;
        for(uint8_t cPhase = cStartPhase; cPhase < cEndPhase; cPhase++)
        {
            if(cOnlyFirst && cGoodCombinationsStubs.size() > 0) continue;
            for(uint8_t cRetime = 3; cRetime < 8; cRetime++)//to-do - add range to xml
            {
                if(cOnlyFirst && cGoodCombinationsStubs.size() > 0) continue;
                size_t cStubDelay  = pLatency - cStubOffset - cRetime; // stub latency
                fReadoutChipInterface->WriteChipReg(pChip, "RetimePix", cRetime);
                fReadoutChipInterface->WriteChipReg(pChip, "StubInputPhase", cPhase);

                fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubDelay);
                ReadNEvents((*cBoardIter), cNevents);
                const std::vector<Event*>& cEvents = this->GetEvents();

                LOG (INFO) << BOLDMAGENTA  << "Writing common_stubdata_delay " << cStubDelay << " [ReTime pix is set to " << +cRetime 
                    << " ]\t\t LatencyRx320 for stubs of " << +cPhase << " ReTime of " << +cRetime << RESET;
                for(size_t cTriggerId = 0; cTriggerId < (1 + cTriggerMult); cTriggerId++)
                {
                    size_t cMatchedEvents = 0;
                    auto   cEventIter     = cEvents.begin() + cTriggerId;
                    size_t cMatchedEventsL1 = 0 ;
                    size_t cNchecked=0;
                    do {
                        if(cEventIter >= cEvents.end()) break;
                        auto cPclus  = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(pChip->getHybridId(), pChip->getId());
                        auto cSclus  = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(pChip->getHybridId(), pChip->getId());
                        // sort P clusters by row 
                        std::sort(cPclus.begin(), cPclus.end(), customSortPclus);
                        // sort S clusters by row
                        std::sort(cSclus.begin(), cSclus.end(), customSortSclus);
                        auto cStubs  = static_cast<D19cCic2Event*>(*cEventIter)->StubVector(pChip->getHybridId(), pChip->getId());
                        cMatchedEventsL1 += (cPclus.size() == pInjections.size() && cSclus.size() == pInjections.size()) ? 1 : 0; 
                        bool cNmatch      = true;
                        if( cCheckL1 ){ 
                            cNmatch = (cPclus.size() == pInjections.size() && cSclus.size() == pInjections.size());
                            LOG(DEBUG) << BOLDGREEN << "Trigger#" << +cTriggerId << " in a burst of " << (1 + cTriggerMult) << " MPA" << +pChip->getId() << " found " <<
                                " correct numnber of S and P clusters." << RESET;
                        }
                        if(cStubs.size() != 0 && cNmatch)
                            LOG(DEBUG) << BOLDGREEN << "Trigger#" << +cTriggerId << " Event#" << (*cEventIter)->GetEventCount() 
                                << " in a burst of " << (1 + cTriggerMult) << " MPA" << +pChip->getId() << " found " << cSclus.size()
                                << " S clusters and " << cPclus.size() << " P clusters in L1 data from MPA#" << +pChip->getId() << " also have " << +cStubs.size() << " stbs." << RESET;
                        // print stubs if they are there
                        if( cNmatch )
                        { 
                            size_t cStubCntr = 0;
                            // sort stubs by position 
                            std::sort(cStubs.begin(), cStubs.end(), customSortStubs);
                            for(auto cStub: cStubs)
                            {
                                LOG(DEBUG) << BOLDCYAN << "\t\tStub#" << +cStubCntr << " Position " << +cStub.getPosition() << " - Row " << +cStub.getRow() << " - Bend " << +cStub.getBend()
                                          << RESET;
                                cStubCntr++;
                            }
                            cNmatch = cNmatch && (cStubs.size() == pInjections.size());
                        }
                        // update counters 
                        cEventIter += (1 + cTriggerMult);
                        cMatchedEvents += (cNmatch) ? 1 : 0;
                        cNchecked++; 
                    } while(cEventIter < cEvents.end());
                    if( cMatchedEvents > 0 ) LOG (DEBUG) << BOLDMAGENTA << cMatchedEvents << " out of " << cNevents 
                            << " matched for stubs " 
                            << cMatchedEventsL1 << " events matched for L1A data "
                            << ". Found " << cEvents.size() 
                            << " events in the readout ]. In total " 
                            << cNchecked << " events were checked"
                            << RESET;
                    if(cMatchedEvents >= (cNevents-1)) // to allow for a trigger multiplicity of 1
                    {
                        cGoodStubDelay = cStubAddDelay;
                        std::pair<uint8_t, uint8_t> cComb;
                        cComb.first  = cPhase;
                        cComb.second = cRetime;
                        cGoodCombinationsStubs.push_back(cComb);
                        LOG(INFO) << BOLDGREEN << "All events stubs match for, LatencyRx320 of " << +cComb.first << " , ReTimePix " << +cComb.second << " full matching of S-clusters in MPA data"
                                  << RESET;
                    }
                }
            }
        }
    }
    (*cBoardIter)->setStubOffset((*cBoardIter)->getStubOffset() + cGoodStubDelay);
    for(auto cCombStbs: cGoodCombinationsStubs)
    {
        LOG(INFO) << BOLDMAGENTA << "\t.. Stub data : LatencyRx320 of " << +cCombStbs.first << " , ReTimePix " << +cCombStbs.second << " full matching of stub data in MPA" << RESET;
        fReadoutChipInterface->WriteChipReg(pChip, "StubInputPhase", cCombStbs.first);
        fReadoutChipInterface->WriteChipReg(pChip, "RetimePix", cCombStbs.second);
    }
    return cGoodCombinationsStubs;

}
std::vector<std::pair<uint8_t, uint8_t>> PSAlignment::AlignChip(ReadoutChip* pChip, std::vector<Injection> pInjections, uint16_t pLatency)
{
    std::vector<std::pair<uint8_t, uint8_t>> cGoodCombinations;
    cGoodCombinations.clear();
    LOG(INFO) << BOLDMAGENTA << "PSAlignment::AlignChip  - aligning L1 and stub data for SSA-MPA pair#" << +pChip->getId() % 8 << RESET;
    auto     cBoardId     = pChip->getBeBoardId();
    auto     cBoardIter   = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    auto     cTriggerMult = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint32_t cNevents     = 10;

    uint8_t cStartPhaseL1 = 2; // to-do : set from xml 
    uint8_t cStopPhaseL1  = 5; // to-do : set from xml 
    bool    cOnlyFirst    = true;
    for(uint8_t cPhase = cStartPhaseL1; cPhase < cStopPhaseL1; cPhase++)
    {
        if(cOnlyFirst && cGoodCombinations.size() > 0) continue; // for now .. only the first one
        for(uint8_t cWord = 0; cWord < 16; cWord++)
        {
            if(cOnlyFirst && cGoodCombinations.size() > 0) continue; // for now .. only the first one
            fReadoutChipInterface->WriteChipReg(pChip, "L1InputPhase", cPhase);
            fReadoutChipInterface->WriteChipReg(pChip, "LatencyRx40", cWord);

            ReadNEvents(*cBoardIter, cNevents);
            const std::vector<Event*>& cEvents = this->GetEvents();
            if(cEvents.size() == 0) continue;

            LOG(INFO) << BOLDBLUE << "Setting L1 input sampling phase MPAs to " << +cPhase << " and Rx40 delay to " << +cWord << " - going to read " << +cNevents << RESET;
            // start at the beginning + trigger id in burst
            for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++)
            {
                size_t cMatchedEvents = 0;
                auto   cEventIter     = cEvents.begin() + cTriggerId;
                do {
                    if(cEventIter >= cEvents.end()) break;
                    bool cNmatch = true;
                    bool cFmatch = true;
                    auto cPclus  = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(pChip->getHybridId(), pChip->getId());
                    auto cSclus  = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(pChip->getHybridId(), pChip->getId());
                    cNmatch      = cNmatch && (cSclus.size() == pInjections.size() && cPclus.size() == pInjections.size());
                    cFmatch      = cNmatch;
                    if(cNmatch)
                    {
                        LOG(DEBUG) << BOLDBLUE << "Trigger#" << +cTriggerId << " in a burst of " << (1 + cTriggerMult) << " MPA" << +pChip->getId() << " found " << cSclus.size() << " S clusters and "
                                   << cPclus.size() << " P clusters in L1 data from MPA#" << +pChip->getId() << RESET;
                        for(size_t cIndx = 0; cIndx < pInjections.size(); cIndx++)
                        {
                            cFmatch = cFmatch && (cPclus[cIndx].fAddress == cSclus[cIndx].fAddress);
                            if((cPclus[cIndx].fAddress == cSclus[cIndx].fAddress))
                                LOG(DEBUG) << BOLDGREEN << "Exact match found " << BOLDYELLOW << " P-cluster in row " << +cPclus[cIndx].fAddress << " column " << +cPclus[cIndx].fZpos << " width is "
                                           << +cPclus[cIndx].fWidth << BOLDCYAN << " S-cluster in row " << +cSclus[cIndx].fAddress << " column " << (0) << " width is " << +cSclus[cIndx].fWidth
                                           << RESET;
                            else
                                LOG(DEBUG) << BOLDRED << "Exact match not found " << BOLDYELLOW << " P-cluster in row " << +cPclus[cIndx].fAddress << " column " << +cPclus[cIndx].fZpos << " width is "
                                           << +cPclus[cIndx].fWidth << BOLDCYAN << " S-cluster in row " << +cSclus[cIndx].fAddress << " column " << (0) << " width is " << +cSclus[cIndx].fWidth
                                           << RESET;
                        }
                    }
                    // if( cNmatch && cFmatch )
                    //     LOG (INFO) << BOLDGREEN << "Event#" << (*cEventIter)->GetEventCount() << " Trigger#" << +cTriggerId
                    //         << " in a burst of " << (1+ cTriggerMult)
                    //         << " number of S-clusters match is " << +cNmatch
                    //         << " full S-cluster match is " << +cFmatch
                    //         << RESET;
                    cMatchedEvents += (cFmatch && cNmatch) ? 1 : 0;
                    cEventIter += (1 + cTriggerMult);
                } while(cEventIter < cEvents.end());
                if(cMatchedEvents == cNevents)
                {
                    std::pair<uint8_t, uint8_t> cComb;
                    cComb.first  = cPhase;
                    cComb.second = cWord;
                    cGoodCombinations.push_back(cComb);
                    LOG(INFO) << BOLDGREEN << "All events match for, LatencyRx320 of " << +cComb.first << " , LatencyRx40 [first]" << +(cComb.second & 0x3) << " LatencyRx40 [re-start] "
                              << +(cComb.second >> 2) << " full matching of S-clusters in MPA data" << RESET;
                }
            }
        }
    }

    size_t cL1CombIndx = 0;
    for(auto cComb: cGoodCombinations)
    {
        if(cL1CombIndx > 0 && cOnlyFirst) continue;
        // now run stub alignment procedure
        LOG(INFO) << BOLDYELLOW << "Scanning Stub alignment parameters for L1 alignmnet parameters.." << RESET;
        fReadoutChipInterface->WriteChipReg(pChip, "L1InputPhase", cComb.first);
        fReadoutChipInterface->WriteChipReg(pChip, "LatencyRx40", cComb.second);
        fReadoutChipInterface->WriteChipReg(pChip, "StubMode", 0);
        fReadoutChipInterface->WriteChipReg(pChip, "StubWindow", 1);

        // I think that the sampling phase or the stub line should be about the same
        // as the L1 line
        // if the lines between SSAs and MPAs on the hybrid are matched
        // and I think they are
        uint8_t                                  cStartPhase = (cComb.first == 0) ? cComb.first : cComb.first - 1;
        uint8_t                                  cEndPhase   = cComb.first + 2;
        std::vector<std::pair<uint8_t, uint8_t>> cGoodCombinationsStubs;
        cGoodCombinationsStubs.clear();
        int cGoodStubDelay = 0;
        for(int cStubAddDelay = 0; cStubAddDelay <= 5; cStubAddDelay++)
        {
            if(cGoodCombinationsStubs.size() > 0) continue;
            LOG(INFO) << BOLDMAGENTA << "Additional stub data delay of " << cStubAddDelay << RESET;
            for(uint8_t cPhase = cStartPhase; cPhase < cEndPhase; cPhase++)
            {
                if(cGoodCombinationsStubs.size() > 0) continue;
                for(uint8_t cRetime = 0; cRetime < 8; cRetime++)
                {
                    if(cGoodCombinationsStubs.size() > 0) continue;
                    auto   cStubOffset = (*cBoardIter)->getStubOffset() + cStubAddDelay;
                    size_t cStubDelay  = pLatency - cStubOffset - cRetime; // stub latency
                    fReadoutChipInterface->WriteChipReg(pChip, "RetimePix", cRetime);
                    fReadoutChipInterface->WriteChipReg(pChip, "StubInputPhase", cPhase);

                    // LOG (INFO) << BOLDYELLOW  << "Writing common_stubdata_delay " << cStubDelay << " [ReTime pix is set to " << +cRetime << " ]" << std::endl;
                    fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubDelay);
                    ReadNEvents((*cBoardIter), cNevents);
                    const std::vector<Event*>& cEvents = this->GetEvents();

                    LOG(INFO) << BOLDMAGENTA << "LatencyRx320 for stubs of " << +cPhase << " ReTime of " << +cRetime << RESET;
                    for(size_t cTriggerId = 0; cTriggerId < (1 + cTriggerMult); cTriggerId++)
                    {
                        size_t cMatchedEvents = 0;
                        auto   cEventIter     = cEvents.begin() + cTriggerId;
                        do {
                            if(cEventIter >= cEvents.end()) break;
                            bool cNmatch = true;
                            auto cPclus  = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(pChip->getHybridId(), pChip->getId());
                            auto cSclus  = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(pChip->getHybridId(), pChip->getId());
                            auto cStubs  = static_cast<D19cCic2Event*>(*cEventIter)->StubVector(pChip->getHybridId(), pChip->getId());
                            cNmatch      = cNmatch && (cStubs.size() == pInjections.size() && cPclus.size() == pInjections.size() && cSclus.size() == pInjections.size());
                            if(cStubs.size() != 0 && cNmatch)
                                LOG(INFO) << BOLDBLUE << "Trigger#" << +cTriggerId << " in a burst of " << (1 + cTriggerMult) << " MPA" << +pChip->getId() << " found " << cSclus.size()
                                          << " S clusters and " << cPclus.size() << " P clusters in L1 data from MPA#" << +pChip->getId() << " also have " << +cStubs.size() << " stbs." << RESET;
                            if(cStubs.size() == pInjections.size())
                            {
                                size_t cStubCntr = 0;
                                for(auto cStub: cStubs)
                                {
                                    LOG(INFO) << BOLDCYAN << "\t\tStub#" << +cStubCntr << " Position " << +cStub.getPosition() << " - Row " << +cStub.getRow() << " - Bend " << +cStub.getBend()
                                              << RESET;
                                    cStubCntr++;
                                }
                            }
                            cEventIter += (1 + cTriggerMult);
                            cMatchedEvents += (cNmatch) ? 1 : 0;
                        } while(cEventIter < cEvents.end());
                        if(cMatchedEvents == cNevents)
                        {
                            cGoodStubDelay = cStubAddDelay;
                            std::pair<uint8_t, uint8_t> cComb;
                            cComb.first  = cPhase;
                            cComb.second = cRetime;
                            cGoodCombinationsStubs.push_back(cComb);
                            LOG(INFO) << BOLDGREEN << "All events stubs match for, LatencyRx320 of " << +cComb.first << " , ReTimePix " << +cComb.second << " full matching of S-clusters in MPA data"
                                      << RESET;
                        }
                    }
                }
            }
        }
        (*cBoardIter)->setStubOffset((*cBoardIter)->getStubOffset() + cGoodStubDelay);

        LOG(INFO) << BOLDMAGENTA << "Summary of SSA-MPA data alignment" << RESET;
        LOG(INFO) << BOLDMAGENTA << "LatencyRx320 of " << +cComb.first << " , LatencyRx40 " << +cComb.second << " full matching of S-clusters in MPA data" << RESET;
        for(auto cCombStbs: cGoodCombinationsStubs)
        {
            LOG(INFO) << BOLDMAGENTA << "\t.. Stub data : LatencyRx320 of " << +cCombStbs.first << " , ReTimePix " << +cCombStbs.second << " full matching of stub data in MPA" << RESET;
            fReadoutChipInterface->WriteChipReg(pChip, "StubInputPhase", cCombStbs.first);
            fReadoutChipInterface->WriteChipReg(pChip, "RetimePix", cCombStbs.second);
        }
        cL1CombIndx++;
    }
    return cGoodCombinations;
}
// want 
bool PSAlignment::AlignInputs(BeBoard* pBoard, uint8_t pChipId) 
{
    bool cPhaseFound = true;
    LOG(INFO) << BOLDBLUE << "Aligning MPA inputs for both L1 and Stub data for SSA-MPA pair #" << +pChipId << RESET;
    
    // check trigger source
    // and reload
    Injection              cInjection;
    std::vector<Injection> cInjections;
    // in principle here I would like to make sure all lines are aligned 
    // for now I will do just four 
    cInjection.fRow    = 10;
    cInjection.fColumn = 2;
    cInjections.push_back(cInjection);//0
    cInjection.fRow    = 20;
    cInjection.fColumn = 3;
    cInjections.push_back(cInjection);//1
    // cInjection.fRow    = 30;
    // cInjection.fColumn = 4;
    // cInjections.push_back(cInjection);//2
    // cInjection.fRow    = 40;
    // cInjection.fColumn = 5;
    // cInjections.push_back(cInjection);//3
    // cInjection.fRow    = 50;
    // cInjection.fColumn = 6;
    // cInjections.push_back(cInjection);//4
    // cInjection.fRow    = 45;
    // cInjection.fColumn = 9;
    // cInjections.push_back(cInjection);//1
    // cInjection.fRow    = 20;
    // cInjection.fColumn = 7;
    // cInjections.push_back(cInjection);//2
    // cInjection.fRow    = 10;
    // cInjection.fColumn = 2;
    // cInjections.push_back(cInjection);//3
    
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

    LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    auto     cTriggerMult   = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint16_t cDelay         = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    int      cOptimalOffset = -1 + (2*(cTriggerMult > 1)); // want triggered event to be in trigger#2 of the burst 
    //int      cOptimalOffset = 1 + (cTriggerMult > 1); // want triggered event to be in trigger#2 of the burst 
    uint16_t cLatency       = cDelay + cOptimalOffset;
    LOG(DEBUG) << BOLDMAGENTA << "Expect correct latency to be " << +cLatency << RESET;

    // inject 
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                if( cChip->getId()%8 != pChipId ) continue;

                // make sure L1 latency is configured
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency);
                    (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, cInjections,0x01);
                }
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency - 1);
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                    fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x01); 
                    fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_L_ALL", 0x01);
                    for(auto cInjection: cInjections) { fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cInjection.fRow), 0x9); }
                }
            } // chip
        }     // hybrid
    }// optica]l group

    // scan alignment parameters 
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                if( cChip->getId()%8 != pChipId ) continue;
                
                // make sure L1 latency is configured
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    // testing if selecting edge per line works
                    std::vector<uint8_t> cEdgeSelsT1{1};
                    std::vector<uint8_t> cEdgeSelsRaw{0}; 
                    std::vector<uint8_t> cEdgeSelsInputs{1};
                    bool cAllFound=false;
                    //do
                    //{
                        for( auto cEdgeSelT1 : cEdgeSelsT1)
                        {
                            //if( cAllFound ) continue;
                            // L1 lines 
                            fReadoutChipInterface->WriteChipReg(cChip, "SelectEdgeT1", cEdgeSelT1);
                            std::vector<uint8_t> cRawEdge(0);
                            std::vector<std::pair<uint8_t, uint8_t>> cAlParsL1; 
                            for(auto cEdgeSelRaw: cEdgeSelsRaw)
                            {
                                //if( cAllFound ) continue;
                                uint8_t           cLineId = 8;
                                std::stringstream cRegName;
                                cRegName << "SelectEdgeL" << +cLineId;
                                LOG (INFO) << BOLDMAGENTA << "Edge-select for T1 input is " << +cEdgeSelT1 << RESET;
                                LOG(INFO) << BOLDMAGENTA << "Edge-select for Raw strip input L" << +cLineId << " will be set to " << +cEdgeSelRaw << RESET;
                                fReadoutChipInterface->WriteChipReg(cChip, cRegName.str(), cEdgeSelRaw);
                                auto cL1AlignmentPars = this->AlignL1( cChip, cInjections);
                                LOG (INFO) << BOLDBLUE << "Found " << +cL1AlignmentPars.size() << " combinations of alignment parameters for L1 data from SSA" << RESET;
                                for( auto cPar : cL1AlignmentPars )
                                {
                                    cAlParsL1.push_back( cPar ); 
                                    cRawEdge.push_back( cEdgeSelRaw );
                                }
                            }
                            
                            // Stub lines + final parameters 
                            std::vector<std::pair<uint8_t, uint8_t>> cAlParsStubData; 
                            std::vector<std::pair<uint8_t, uint8_t>> cAlParsHitData; 
                            std::vector<uint8_t> cRawEdgeL1(0);
                            std::vector<uint8_t> cRawEdgeStubs(0);
                            for( size_t cIndx=0; cIndx < cRawEdge.size() ; cIndx++)
                            {
                                //if( cAllFound ) continue;
                                // make sure L1 line is aligned 
                                uint8_t           cLineId = 8;
                                std::stringstream cRegName;
                                cRegName << "SelectEdgeL" << +cLineId;
                                // LOG(INFO) << BOLDGREEN << "Edge-select for Raw strip input L" << +cLineId << " will be set to " << +cRawEdge[cIndx] << RESET;
                                // LOG(INFO) << BOLDGREEN << "L1InputPhase for Raw input L" << +cLineId << " will be set to " << +cAlParsL1[cIndx].first << RESET;
                                // LOG(INFO) << BOLDGREEN << "Edge-LatencyRx40 for Raw strip input L" << +cLineId << " will be set to " << +cAlParsL1[cIndx].second << RESET;
                                fReadoutChipInterface->WriteChipReg(cChip, cRegName.str(), cRawEdge[cIndx]);
                                fReadoutChipInterface->WriteChipReg(cChip, "L1InputPhase", cAlParsL1[cIndx].first);
                                fReadoutChipInterface->WriteChipReg(cChip, "LatencyRx40", cAlParsL1[cIndx].second);
                                // stub lines 
                                for( auto cEdgeSelsInput : cEdgeSelsInputs )
                                {
                                    //if( cAllFound ) continue;
                                    LOG (INFO) << BOLDGREEN << "Edge select for stub input is " << +cEdgeSelsInput << RESET;
                                    auto cStbOffset    = pBoard->getStubOffset();
                                    //each stub will appear on one of the lines 
                                    for( uint8_t cLine = 0; cLine < cInjections.size(); cLine++)
                                    {
                                        std::stringstream cRegName;
                                        cRegName << "SelectEdgeL" << +cLine;
                                        fReadoutChipInterface->WriteChipReg(cChip, cRegName.str(), cEdgeSelsInput);
                                    }
                                    auto cStubAlignmentPars = this->AlignStubs( cChip, cInjections, cLatency);
                                    cAllFound = cStubAlignmentPars.size() > 0; 
                                    LOG (DEBUG) << BOLDMAGENTA << (int)cAllFound << RESET;
                                    for( auto cPar : cStubAlignmentPars )
                                    {
                                        cAlParsStubData.push_back( cPar ); 
                                        cAlParsHitData.push_back( cAlParsL1[cIndx] );
                                        cRawEdgeL1.push_back( cRawEdge[cIndx] );
                                        cRawEdgeStubs.push_back( cEdgeSelsInput );
                                        LOG (INFO) << BOLDGREEN << "Edge-select T1 is " << +cEdgeSelT1 << "\tEdge-select for Raw strip input L1A will be set to " << +cRawEdge[cIndx] << RESET;
                                        LOG (INFO) << BOLDGREEN << "Alignment parameters for L1 hit data : [" << +cAlParsL1[cIndx].first << "," << +cAlParsL1[cIndx].first << "]" << RESET;
                                        LOG (INFO) << BOLDGREEN << "Alignment parameters for Stub data : [" << +cPar.first << "," << +cPar.second << "]" << RESET;
                                    }
                                    pBoard->setStubOffset(cStbOffset);
                                }
                            }
                            // // print out summary 
                            // for( size_t cIndx=0 ; cIndx < cAlParsStubData.size(); cIndx++)
                            // {
                            //     LOG (INFO) << BOLDBLUE << "Alignment parameters for L1 hit data : [" << +cAlParsHitData[cIndx].first << "," << +cAlParsStubData[cIndx].first << "]" << RESET;
                            //     LOG (INFO) << BOLDBLUE << "Alignment parameters for Stub data : [" << +cAlParsStubData[cIndx].first << "," << +cAlParsStubData[cIndx].first << "]" << RESET;
                            //     LOG (INFO) << BOLDBLUE << "Edge select L1 data is " << +cRawEdgeL1[cIndx] << RESET;
                            //     LOG (INFO) << BOLDBLUE << "Edge select Stub data is " << +cRawEdgeStubs[cIndx] << RESET;
                            // }
                        }
                    //}while(!cAllFound);

                }
            } // chip
        }     // hybrid
    }// optica]l group

    // set everything back to original values .. like I wasn't here
    // reset fast command registers
    cRegVec.clear();
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cOriginalTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", cOriginalTLUconfig});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    
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
        bool cWithMPA=false;
        bool cWithSSA=false;
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                for(auto cChip: *cHybrid)
                {
                    cWithMPA = cWithMPA || cChip->getFrontEndType() == FrontEndType::MPA;
                    cWithSSA = cWithMPA || (cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2) ; 
                }
            }
        }
        if( !(cWithSSA && cWithMPA) ){ 
            LOG(INFO) << BOLDBLUE << "Not performing SSA-MPA L1 alignment... no PS chips!" << RESET;
            continue;
        }
        // // potentially we have 8 chips possible 
        // cl1Aligned = cl1Aligned && this->AlignL1Inputs(cBoard);
        // cStubAligned = cStubAligned && this->AlignStubInputs(cBoard);
        // LOG(INFO) << BOLDBLUE << "L1 alignemnt " << RESET;
        // cl1Aligned ? LOG(INFO) << BOLDGREEN << "Succeeded" << RESET : LOG(INFO) << BOLDRED << "Failed" << RESET;
        
        uint8_t cMaxChips=8;
        for( uint8_t cChipId=0; cChipId < cMaxChips; cChipId++)
        {
            // check if chip id is there 
            bool cChipFound=false; 
            for(auto cOpticalReadout: *cBoard)
            {
                if(cChipFound) break;
                for(auto cHybrid: *cOpticalReadout)
                {
                    if(cChipFound) break;
                    for(auto cChip: *cHybrid)
                    {
                        if(cChipFound) break;
                        if( cChip->getFrontEndType() != FrontEndType::MPA) continue;
                        if( cChip->getId()%cMaxChips == cChipId ) cChipFound=true;
                    }
                }
            }
            if( !cChipFound ) continue;

            cl1Aligned = cl1Aligned && this->AlignInputs(cBoard, cChipId);
            cl1Aligned ? LOG(INFO) << BOLDGREEN << "Succeeded" << RESET : LOG(INFO) << BOLDRED << "Failed" << RESET;
            this->Reset();
        }
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
