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
void PSAlignment::MapMPAOutputs(std::string pSetupType)
{
    LOG(INFO) << BOLDBLUE << "Configuring MPA output register [mapping between output bits and output pads] .... " << RESET;
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
                        fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_0", 1); // 1
                        fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_1", 2); // 2
                        fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_2", 3); // 3
                        fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_3", 4); // 4
                        fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_4", 5); // 5
                        fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_5", 0); // L1 line
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
    LOG(DEBUG) << BOLDMAGENTA << "Expect correct latency to be " << +cLatency << RESET;

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
    for(uint8_t cChipId = 0; cChipId < 8; cChipId++)
    {
        bool nochip = true;
        if(!cCurPhaseFound) cPhaseFound = false; // all chips need to be tuned
        cCurPhaseFound = false;
        for(uint8_t cPhase = 0; cPhase < 8; cPhase++)
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
                                    // std::cout << "getPosition "<<+st.getPosition()<< std::endl;
                                    // std::cout << "getBend "<<+st.getBend()<< std::endl;
                                    // std::cout << "getRow " <<+st.getRow()<<std::endl;
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
            } // retime loop
        }     // phase loop
    }         // chip id loop [up-to 8 chips per hybrid ]

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
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                    uint32_t cGpix = static_cast<MPA*>(cChip)->PNglobal(std::pair<uint32_t, uint32_t>(cRow, cCol));
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency);
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
    for(uint8_t cChipId = 0; cChipId < 8; cChipId++)
    {
        bool nochip = true;
        if(!cCurPhaseFound) cPhaseFound = false;
        cCurPhaseFound = false;
        for(uint8_t cPhase = 3; cPhase < 8; cPhase++)
        {
            if(cCurPhaseFound == true) break;
            for(uint8_t cES = 2; cES < 4; cES++)
            {
                if(cCurPhaseFound == true) break;
                for(uint8_t cWord = 0; cWord < 16; cWord++)
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
                                nochip = false;
                                fReadoutChipInterface->WriteChipReg(cChip, "L1InputPhase", cPhase);
                                fReadoutChipInterface->WriteChipReg(cChip, "LatencyRx40", cWord);
                                fReadoutChipInterface->WriteChipReg(cChip, "EdgeSelT1Raw", cES);
                            } // chip
                        }     // hybrid
                    }         // optical group
                    if(nochip)
                    {
                        cCurPhaseFound = true;
                        continue;
                    }
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
                                        // std::cout << "-------------------------------PIXELS-------------------------------"<< std::endl;
                                        // std::cout << "fAddress "<<+pc.fAddress<<std::endl;
                                        // std::cout << "fWidth "<<+pc.fWidth<< std::endl;
                                        // std::cout << "fZpos "<<+pc.fZpos << std::endl;
                                        if((cCol) == pc.fAddress and (cRow - 1) == pc.fZpos) MatchNPclustot += 1;
                                    }
                                    for(auto& sc: Sclus)
                                    {
                                        // std::cout << "-------------------------------STRIPS-------------------------------"<< std::endl;
                                        // std::cout << "fAddress? "<<+sc.fAddress<<std::endl;
                                        // std::cout << "fWidth "<<+sc.fWidth<< std::endl;
                                        // std::cout << "fMip "<<+sc.fMip << std::endl<< std::endl;
                                        if((cCol) == sc.fAddress and sc.fMip == 1) MatchNSclustot += 1;
                                    }
                                }
                            }
                        }
                    }
                    if((MatchNPclustot == (cNevents - 1)) and (MatchNSclustot == (cNevents - 1)))
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

        // cl1Aligned = cl1Aligned && this->AlignL1Inputs(cBoard);

        cStubAligned = cStubAligned && this->AlignStubInputs(cBoard);

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
