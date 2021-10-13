#include "CicFEAlignment.h"

// #ifdef __USE_ROOT__

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

CicFEAlignment::CicFEAlignment() : Tool() { fRegMapContainer.reset(); }

CicFEAlignment::~CicFEAlignment() {}
void CicFEAlignment::Reset()
{
    // set everything back to original values .. like I wasn't here
    bool cWithPS = false;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap) cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second));
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);

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
                LOG(DEBUG) << BOLDBLUE << "CicFEAlignment::Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;
                for(auto cChip: *cHybrid)
                {
                    if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->UpdateModifiedRegisterMap(cChip);
                    auto cModMap = fReadoutChipInterface->GetModifiedRegisterMap(cChip);
                    LOG(DEBUG) << BOLDBLUE << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    for(auto cMapItem: cModMap)
                    {
                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        LOG(DEBUG) << BOLDBLUE << "CicFEAlignment::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to "
                                   << cMapItem.second.fValue << RESET;
                        fReadoutChipInterface->WriteChipReg(cChip, cMapItem.first, cMapItem.second.fValue);
                    }
                }
            }
        }
    }
    fReadoutChipInterface->ClearModifiedRegisterMap();
    if(cWithPS) static_cast<PSInterface*>(fReadoutChipInterface)->ResetModifiedRegisterMap();
    resetPointers();
}

void CicFEAlignment::Initialise()
{
    fSuccess = false;
    fWithMPA = false;
    // this is needed if you're going to use groups anywhere
    fChannelGroupHandler = new CBCChannelGroupHandler(); // This will be erased in tool.resetPointers()
    fChannelGroupHandler->setChannelGroupParameters(16, 2);

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    ContainerFactory::copyAndInitChip<AlignmentValues>(*fDetectorContainer, fPhaseAlignmentValues);
    ContainerFactory::copyAndInitChip<AlignmentValues>(*fDetectorContainer, fWordAlignmentValues);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cPhaseAlignmentThisBoard = fPhaseAlignmentValues.at(cBoard->getIndex());
        auto& cWordAlignmentThisBoard  = fWordAlignmentValues.at(cBoard->getIndex());

        for(auto cOpticalGroup: *cBoard)
        {
            auto& cPhaseAlignmentThisOpticalGroup = cPhaseAlignmentThisBoard->at(cOpticalGroup->getIndex());
            auto& cWordAlignmentThisOpticalGroup  = cWordAlignmentThisBoard->at(cOpticalGroup->getIndex());

            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cPhaseAlignmentThisHybrid = cPhaseAlignmentThisOpticalGroup->at(cHybrid->getIndex());
                auto& cWordAlignmentThisHybrid = cWordAlignmentThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    // check chip type
                    fWithMPA = fWithMPA || (cChip->getFrontEndType() == FrontEndType::MPA);
                    if( cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2 ) continue;
                    
                    auto& cPhaseAlignmentThisChip = cPhaseAlignmentThisHybrid->at(cChip->getIndex());
                    auto& cWordAlignmentThisChip = cWordAlignmentThisHybrid->at(cChip->getIndex());
                        
                    auto& cPhaseAlVals              = cPhaseAlignmentThisChip->getSummary<AlignmentValues>();
                    cPhaseAlVals.clear(); cPhaseAlVals.resize(6,0);
                    auto& cWordAlignmentVals       = cWordAlignmentThisChip->getSummary<AlignmentValues>();
                    cWordAlignmentVals.clear();cWordAlignmentVals.resize(5,0);
                }
            }
        }
    }

    // retreive original settings for all chips and all back-end boards
    ContainerFactory::copyAndInitChip<ChipRegMap>(*fDetectorContainer, fRegMapContainer);
    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        auto&                cBoardRegNap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        const BeBoardRegMap& cOrigRegMap  = static_cast<const BeBoard*>(cBoard)->getBeBoardRegMap();
        cBoardRegNap.insert(cOrigRegMap.begin(), cOrigRegMap.end());
    }

    // clear map of modified registers
    fReadoutChipInterface->ClearModifiedRegisterMap();
    bool cIsPS = false;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            bool cWithLpGBT = (cOpticalGroup->flpGBT != nullptr);
            for(auto cHybrid: *cOpticalGroup)
            {
                if(cIsPS) continue;
                auto cType    = FrontEndType::SSA;
                bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                cType         = FrontEndType::MPA;
                bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                cIsPS         = (cWithSSA && cWithMPA) && cWithLpGBT;
            }
        }
    }
    if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->ResetModifiedRegisterMap();
}

void CicFEAlignment::writeObjects()
{
    this->SaveResults();
#ifdef __USE_ROOT__
    // fDQMHistogramHybridTest.process();
    fResultFile->Flush();
#endif
}
// State machine control functions
void CicFEAlignment::AlignInputs()
{
    // align CIC inputs - first phase 
    bool cPhaseAligned = this->PhaseAlignment();
    if(!cPhaseAligned)
    {
        LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " phase alignment step on CIC input .. " << RESET;
        exit(FAILED_PHASE_ALIGNMENT);
    }
    LOG(INFO) << BOLDGREEN << "SUCCESSFUL " << BOLDBLUE << " phase alignment on CIC inputs... " << RESET;
    fSuccess = cPhaseAligned;

    // then word 
    bool cWordAligned = this->WordAlignment();
    if(!cWordAligned)
    {
        LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << "word alignment step on CIC input .. " << RESET;
        exit(FAILED_WORD_ALIGNMENT);
    }
    LOG(INFO) << BOLDGREEN << "SUCCESSFUL " << BOLDBLUE << " word alignment on CIC inputs... " << RESET;
    
    // bool cBxAligned = (fWithMPA) ? this->SetBx0Delay(fStubBxDelayPS) : this->SetBx0Delay(fStubBxDelay2S);
    // if(!cBxAligned)
    // {
    //     LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " to set Bx0 delay in CIC ... " << RESET;
    //     exit(FAILED_BX_ALIGNMENT);
    // }
    // LOG(INFO) << BOLDGREEN << "SUCCESSFUL " << BOLDBLUE << " setting of Bx0 delay in CIC ... " << RESET;
    fSuccess = (cPhaseAligned && cWordAligned );
}
void CicFEAlignment::Running()
{
    Initialise();
    AlignInputs();
    Reset();
}
void CicFEAlignment::SetStubWindowOffsets(uint8_t pBendCode, int pBend)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    // read bend LUT
                    ReadoutChip*         theChip   = static_cast<ReadoutChip*>(cChip);
                    std::vector<uint8_t> cBendLUT  = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theChip);
                    auto                 cIterator = std::find(cBendLUT.begin(), cBendLUT.end(), pBendCode);
                    if(cIterator != cBendLUT.end())
                    {
                        int     cPosition    = std::distance(cBendLUT.begin(), cIterator);
                        double  cBend_strips = -7. + 0.5 * cPosition;
                        uint8_t cOffsetCode  = static_cast<uint8_t>(std::abs(cBend_strips * 2)) | (std::signbit(-1 * cBend_strips) << 3);
                        fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset12", (cOffsetCode << 4) | (cOffsetCode << 0));
                        fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset34", (cOffsetCode << 4) | (cOffsetCode << 0));
                        LOG(DEBUG) << BOLDBLUE << "Bend code of " << std::bitset<4>(pBendCode) << " found for bend reg " << +cPosition << " which means " << cBend_strips << " strips [offset code "
                                   << std::bitset<4>(cOffsetCode) << "]." << RESET;
                    }
                }
            }
        }
    }
}
bool CicFEAlignment::SetBx0Delay(uint8_t pDelay, uint8_t pStubPackageDelay)
{
    // configure Bx0 alignment patterns in CIC
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                OuterTrackerHybrid* theHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
                if(theHybrid->fCic != NULL)
                {
                    bool cConfigured = fCicInterface->ManualBx0Alignment(theHybrid->fCic, pDelay);
                    if(!cConfigured)
                    {
                        LOG(INFO) << BOLDRED << "Failed to manually set Bx0 delay in CIC..." << RESET;
                        exit(0);
                    }
                }
            }
        }
    }
    return true;
}

void CicFEAlignment::SetStaticPhaseAlignment()
{
    LOG(INFO) << BOLDBLUE << "Setting CIC phase to static mode.." << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cPhaseAlignmentThisBoard = fPhaseAlignmentValues.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cPhaseAlignmentThisOpticalGroup = cPhaseAlignmentThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cPhaseAlignmentThisHybrid = cPhaseAlignmentThisOpticalGroup->at(cHybrid->getIndex());
                
                auto&  cCic         = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->GetOptimalTaps(cCic);
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2 ) continue;
                    auto& cPhaseAlignmentThisChip = cPhaseAlignmentThisHybrid->at(cChip->getIndex());
                    auto& cPhaseAlignmentVals     = cPhaseAlignmentThisChip->getSummary<AlignmentValues>(); 
                    
                    auto cPhaseTapsThisFE = fCicInterface->GetOptimalTaps(cCic, cChip->getId()%8);
                    std::stringstream cOutput;
                    for( uint8_t cLineId =0 ; cLineId < 6;  cLineId++)
                    {
                        cPhaseAlignmentVals[cLineId] = cPhaseTapsThisFE[cLineId];
                        cOutput << +cPhaseAlignmentVals[cLineId] << " "; 
                    }
                    LOG(INFO) << BOLDBLUE << "Optimal tap found on CIC#" << +cChip->getHybridId() << " FE" << +cChip->getId() << " : " << cOutput.str() << RESET;
                }
                fCicInterface->SetStaticPhaseAlignment(cCic);
            }
        }
    }
}
bool CicFEAlignment::CicLpGbtAlignment()
{
    // align CIC-lpGBT
    bool cOutAligned=true;
    for(auto cBoard : *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT = cOpticalGroup->flpGBT;
            if(clpGBT == nullptr) continue;
            bool cWithCIC = false;
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                if(cCic == NULL) continue;
                cWithCIC = true;
            }
            if(!cWithCIC) continue;
            cOutAligned = cOutAligned && CicLpGbtAlignment(cOpticalGroup);
        }
    }
    if(!cOutAligned)
    {
        LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " phase alignment step on CIC output [in lpGBT] .. " << RESET;
        exit(FAILED_PHASE_ALIGNMENT);
    }
    LOG(INFO) << BOLDGREEN << "SUCCESSFUL " << BOLDBLUE << " phase alignment on CIC outputs [in lpGBT]... " << RESET;
    return cOutAligned;
}
bool CicFEAlignment::CicLpGbtAlignment(const OpticalGroup* pOpticalGroup)
{
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    // stop triggers to make sure that there are no L1 packets from the CIC
    fBeBoardInterface->Stop((*cBoardIter));
    
    LOG(INFO) << BOLDMAGENTA << "Aligning CIC-lpGBT data on OpticalGroup#" << +pOpticalGroup->getId() << RESET;
    auto& clpGBT = pOpticalGroup->flpGBT;
    // configure CICs to output alignment pattern on stub lines
    std::vector<uint8_t> cFeEnableRegs(0);
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, true);
        cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
    }
    bool cAligned=true;
    for(auto cHybrid: *pOpticalGroup)
    {
        std::vector<uint8_t> cGroups;
        std::vector<uint8_t> cChannels;
        if(pOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S)
        {
            if(cHybrid->getId() % 2 == 0)
            {
                cGroups   = {0, 4, 4, 5, 5, 6};
                cChannels = {0, 0, 2, 0, 2, 0};
            }
            else
            {
                cGroups   = {0, 1, 1, 2, 2, 3};
                cChannels = {2, 0, 2, 0, 2, 2};
            }
        }
        else
        {
            if(cHybrid->getId() % 2 == 0)
            {
                cGroups   = {4, 4, 5, 5, 6, 6, 0};
                cChannels = {2, 0, 2, 0, 2, 0, 0};
            }
            else
            {
                cGroups   = {0, 1, 1, 2, 2, 3, 3};
                cChannels = {2, 0, 2, 0, 2, 0, 2};
            }
        }
        cAligned = cAligned && flpGBTInterface->AutoPhaseAlignRx(clpGBT, cGroups, cChannels);
    }
    // configure CICs to NOT output alignment pattern on stub lines
    size_t cIndx=0;
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, false);
        fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
        cIndx++;
    }
    return cAligned;
}

bool CicFEAlignment::PhaseAlignment(uint16_t pWait_us, uint32_t pNTriggers)
{
    bool cDebug=false;
    bool cAligned = true;
    LOG(INFO) << BOLDBLUE << "Starting CIC automated phase alignment procedure for CBCs .... " << RESET;

    for(auto cBoard: *fDetectorContainer)
    {
        bool cWithCBC=false;
        // generate alignment pattern on all stub lines
        LOG(INFO) << BOLDBLUE << "Generating Patterns needed for phase alignment of CIC inputs." << RESET;

        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetAutomaticPhaseAlignment(cCic, true);
                // configure ROCs to produce phase alignment patterns 
                for(auto cChip: *cHybrid)
                {
                    if( cChip->getFrontEndType() == FrontEndType::CBC3) cWithCBC = true;
                    fReadoutChipInterface->producePhaseAlignmentPattern(cChip,10);
                }
            }
        }
        // send N triggers on L1 lines 
        if( cWithCBC )
        {
            LOG(INFO) << BOLDBLUE << "Sending triggers to FEs to align L1 output from CBCs.." << RESET;
            uint16_t                                      cTriggerSrc = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
            // if external or async triggers are used then revert to internal here 
            bool cReconfigureTrigger = (cTriggerSrc == 4 || cTriggerSrc || 5 || cTriggerSrc == 10 );
            std::vector<std::pair<std::string, uint32_t>> cRegVec;
            if( cReconfigureTrigger )
            {
                uint16_t                                      cSrc        = 3;
                if(cTriggerSrc != cSrc)
                {
                    LOG(INFO) << BOLDBLUE << "\t.. Changing trigger source is set to " << +cSrc << RESET;
                    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cSrc});
                }
                cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNTriggers});
                cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
                fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
            }
            fBeBoardInterface->SendNTriggers(cBoard,pNTriggers);

            // set trigger source back
            if(cReconfigureTrigger)
            {
                LOG(INFO) << BOLDBLUE << "\t.. Changing trigger source back to " << +cTriggerSrc << RESET;
                std::vector<std::pair<std::string, uint32_t>> cRegVec;
                cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
                cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
                fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
            }
        }// in the CBC case you need to send triggers to get alignment data on L1 line 
        // check alignment 
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // enable automatic phase aligner
                auto& cCic    = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                bool  cLocked = fCicInterface->CheckPhaseAlignerLock(cCic);
                // if locked .. switch to automatic phase aligner mode with best values
                if(cLocked)
                {
                    LOG(INFO) << BOLDBLUE << "Phase aligner on CIC" << +cHybrid->getId() <<  BOLDGREEN << " LOCKED " << BOLDBLUE << " ... storing values and switching to static phase " << RESET;
                }
                else
                    LOG(INFO) << BOLDBLUE << "Phase aligner on CIC" << +cHybrid->getId() <<  BOLDRED << " FAILED to LOCK " << BOLDBLUE << " ... storing values and switching to static phase " << RESET;
                cAligned = cAligned && cLocked;
            } // CICs
        }// OG
    }
    if( cAligned ) this->SetStaticPhaseAlignment();

    // check
    for(auto cBoard: *fDetectorContainer)
    {
        if(!cDebug) continue;
        
        fBeBoardInterface->setBoard(cBoard->getId());
        auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
         for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                for( uint8_t cPhyPort=0; cPhyPort<12; cPhyPort++)
                {
                    fCicInterface->SelectMux(cCic,cPhyPort);
                    cInterface->StubDebug(true, 4);
                }
                fCicInterface->ControlMux(cCic,0);
            }
        }
    }
    
    return cAligned;
}
bool CicFEAlignment::WordAlignment(uint32_t pWait_us)
{
    LOG(INFO) << BOLDBLUE << "Starting CIC automated word alignment procedure .... " << RESET;

    bool                 cAligned = true;
    for(auto cBoard: *fDetectorContainer)
    {
        auto&    cWordAlignmentThisBoard = fWordAlignmentValues.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cWordAlignmentThisOpticalGroup = cWordAlignmentThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cWordAlignmentThisHybrid = cWordAlignmentThisOpticalGroup->at(cHybrid->getIndex());
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                if(cCic == NULL) continue;
                
                // configure word alignment pattern on CBCs 
                std::vector<uint8_t> cAlignmentPatterns=fReadoutChipInterface->getWordAlignmentPatterns();
                for(auto cChip: *cHybrid)
                {
                    fReadoutChipInterface->produceWordAlignmentPattern(cChip);
                } 
                
                // run automated word alignment
                cAligned = cAligned && fCicInterface->AutomatedWordAlignment(cCic, cAlignmentPatterns, pWait_us*1000);
                // check status 
                if(cAligned)
                {
                    fCicInterface->SetStaticWordAlignment(cCic, 1);
                    std::vector<std::vector<uint8_t>> cWordAlignmentValues = fCicInterface->GetWordAlignmentValues(cCic);
                    LOG(INFO) << BOLDBLUE << "Automated word alignment procedure " << BOLDGREEN << " SUCCEEDED!" << RESET;
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2 ) continue;

                        auto& cWordAlignmentThisChip = cWordAlignmentThisHybrid->at(cChip->getIndex());
                        auto& cWordAlignmentVals       = cWordAlignmentThisChip->getSummary<AlignmentValues>();

                        std::stringstream cOutput;
                        for(size_t cLine = 0; cLine < 5; cLine++)
                        {
                            cWordAlignmentVals[cLine] = cWordAlignmentValues[cChip->getId() % 8][cLine];
                            cOutput << +cWordAlignmentVals[cLine] << " ";
                        }
                        LOG(INFO) << BOLDBLUE << "Word alignment values for FE#" << +cChip->getId() << " : " << cOutput.str() << RESET;
                    }
                }
                else
                    LOG(INFO) << BOLDBLUE << "Automated word alignment procedure " << BOLDRED << " FAILED!" << RESET;
            }
        }
    }

    return cAligned;
}
void CicFEAlignment::Stop()
{
    dumpConfigFiles();
    // Destroy();
}

void CicFEAlignment::Pause() {}

void CicFEAlignment::Resume() {}

// #endif
