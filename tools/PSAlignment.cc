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
        for(auto cReg: cBeRegMap){ 
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
                    for(auto cReg: cRegMapThisChip){ 
                        if( cChip->getFrontEndType() == FrontEndType::MPA ) 
                        {
                            if( cReg.first.find("OutSetting") != std::string::npos || cReg.first.find("LatencyRx320") != std::string::npos ) 
                            {
                                LOG (INFO) << BOLDMAGENTA << "\t...Will NOT set " << cReg.first << " back to original value. " << RESET;
                            }
                        }
                        else
                        {
                            cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue));
                        }
                    }
                    fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
                }
            }
        }
    }
    resetPointers();
}

void PSAlignment::Initialise()
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
                    if( cChip->getFrontEndType( ) != FrontEndType::MPA ) continue;

                    // mapping for PS module 
                    // mapping for probe station/etc. can be different
                    if( pSetupType.find("PSModule") != std::string::npos) 
                    {
                        fReadoutChipInterface->WriteChipReg(cChip,"OutSetting_0",1);//1  
                        fReadoutChipInterface->WriteChipReg(cChip,"OutSetting_1",2);//2  
                        fReadoutChipInterface->WriteChipReg(cChip,"OutSetting_2",3);//3  
                        fReadoutChipInterface->WriteChipReg(cChip,"OutSetting_3",4);//4  
                        fReadoutChipInterface->WriteChipReg(cChip,"OutSetting_4",5);//5  
                        fReadoutChipInterface->WriteChipReg(cChip,"OutSetting_5",0);//L1 line 
                    }
                }//chip
            }//hybrid 
        }//optical group 
    }
}
bool PSAlignment::AlignStubInputs(BeBoard* pBoard)
{
    bool cPhaseFound=true;
    uint32_t cNevents = 10; 
    LOG (INFO) << BOLDBLUE << "Aligning MPA stub inputs.." << RESET;
    std::vector<uint32_t> cRowIds{10}; // these will be used to generate stubs 

    // check trigger source 
    // and reload 
    uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    LOG (INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    cTriggerSrc = (cTriggerSrc==6) ? cTriggerSrc : 6 ;
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    
    uint16_t cDelay   = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    uint16_t cLatency = cDelay -1;
    LOG (DEBUG) << BOLDMAGENTA << "Expect correct latency to be " << +cLatency << RESET;

    // configure SSA to inject digitally 
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // configure SSA to inject digitally 
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                if( cChip->getFrontEndType( ) != FrontEndType::SSA ) continue;

                // kevin's code for injection goes here 

            }//chip
        }//hybrid 
    }//optica]l group 

    // configure MPA2 to be in strip-strip mode
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // configure SSA to inject digitally 
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                if( cChip->getFrontEndType( ) != FrontEndType::MPA ) continue;
                
                // make sure MPA is in s-s mode 
                uint8_t  cMode = 2; // (0) pixel-strip, (1) strip-strip, (2) pixel-pixel, (3) strip-pixel 
                uint8_t  cStubWindow=1; 
                fReadoutChipInterface->WriteChipReg(cChip,"StubMode", cMode);
                fReadoutChipInterface->WriteChipReg(cChip,"StubWindow", cStubWindow);
            }//chip
        }//hybrid 
    }//optica]l group 


    // scan phase and check stubs 
    // for each chip on a hybrid 
    for( uint8_t cChipId=0; cChipId < 8 ; cChipId++)
    {
        for( uint8_t cPhase=0; cPhase < 7 ; cPhase++) 
        {
            for(auto cOpticalReadout: *pBoard)
            {
                for(auto cHybrid: *cOpticalReadout)
                {
                    // configure SSA to inject digitally 
                    for(auto cChip: *cHybrid) // for each chip (makes sense)
                    {
                        if( cChip->getFrontEndType( ) != FrontEndType::MPA ) continue;
                        if( cChip->getId() != cChipId ) continue; 

                        fReadoutChipInterface->WriteChipReg(cChip,"StubInputPhase", cPhase);
                    }//chip
                }//hybrid 
            }//optical group

            //now read data 
            LOG (INFO) << BOLDBLUE << "Setting stub input sampling phase for MPA#" << +cChipId << " on hybrid to " << +cPhase << RESET;
            ReadNEvents(pBoard, cNevents);
            const std::vector<Event*>& cEvents = this->GetEvents(pBoard);
            LOG(INFO) << BOLDBLUE << "Checking phase by reading back " << +cEvents.size() << " events from the FC7 ..." << RESET;
        }// phase loop 
    }// chip id loop [up-to 8 chips per hybrid ]

    return cPhaseFound;
}
bool PSAlignment::AlignL1Inputs(BeBoard* pBoard)
{
    bool cPhaseFound=true;
    LOG (INFO) << BOLDBLUE << "Aligning MPA L1 inputs.." << RESET;
    std::vector<uint32_t> cRowIds{10}; // these will be used to generate stubs 
    uint32_t cNevents = 10; 

    // check trigger source 
    // and reload 
    uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    LOG (INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    cTriggerSrc = (cTriggerSrc==6) ? cTriggerSrc : 6 ;
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    
    uint16_t cDelay   = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    uint16_t cLatency = cDelay -1;
    LOG (DEBUG) << BOLDMAGENTA << "Expect correct latency to be " << +cLatency << RESET;

    // configure SSA to inject digitally 
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // configure SSA to inject digitally 
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                // make sure L1 latency is configured 
                fReadoutChipInterface->WriteChipReg(cChip,"TriggerLatency", cLatency);
                
                if( cChip->getFrontEndType( ) != FrontEndType::SSA ) continue;

                // kevin's code for injection goes here 
                
            }//chip
        }//hybrid 
    }//optica]l group 

    // scan phase and check stubs 
    // for each chip on a hybrid 
    for( uint8_t cChipId=0; cChipId < 8 ; cChipId++)
    {
        for( uint8_t cPhase=0; cPhase < 7 ; cPhase++) 
        {
            for(auto cOpticalReadout: *pBoard)
            {
                for(auto cHybrid: *cOpticalReadout)
                {
                    // configure SSA to inject digitally 
                    for(auto cChip: *cHybrid) // for each chip (makes sense)
                    {
                        if( cChip->getFrontEndType( ) != FrontEndType::MPA ) continue;
                        if( cChip->getId() != cChipId ) continue; 

                        fReadoutChipInterface->WriteChipReg(cChip,"L1InputPhase", cPhase);
                    }//chip
                }//hybrid 
            }//optical group

            //now read data 
            LOG (INFO) << BOLDBLUE << "Setting L1 input sampling phase for MPA#" << +cChipId << " on hybrid to " << +cPhase << RESET;
            ReadNEvents(pBoard, cNevents);
            const std::vector<Event*>& cEvents = this->GetEvents(pBoard);
            LOG(INFO) << BOLDBLUE << "Checking phase by reading back " << +cEvents.size() << " events from the FC7 ..." << RESET;
     
        }// phase loop 
    }// chip id loop [up-to 8 chips per hybrid ]
    return cPhaseFound;
}
bool PSAlignment::Align()
{
    LOG(INFO) << BOLDBLUE << "Starting MPA alignment procedure .... " << RESET;
    bool cAligned = true;
    for(auto cBoard: *fDetectorContainer)
    {
        // BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        // // read back register map before you've done anything
        // auto cBoardRegisterMap = theBoard->getBeBoardRegMap();

        cAligned = cAligned && this->AlignL1Inputs(cBoard);

        // now send a fast reset
        //fBeBoardInterface->ChipReSync(theBoard);
    }
    return cAligned;
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
