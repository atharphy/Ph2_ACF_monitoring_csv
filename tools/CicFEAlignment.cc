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
                LOG(INFO) << BOLDBLUE << "CicFEAlignment::Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;
                for(auto cChip: *cHybrid)
                {
                    if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->UpdateModifiedRegisterMap(cChip);
                    auto cModMap = fReadoutChipInterface->GetModifiedRegisterMap(cChip);
                    LOG(INFO) << BOLDBLUE << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
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

    ContainerFactory::copyAndInitHybrid<PortAlignmentVals>(*fDetectorContainer, fPhaseAlignmentValues);
    ContainerFactory::copyAndInitHybrid<PortAlignmentVals>(*fDetectorContainer, fWordAlignmentValues);
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
                auto& cPhaseAlVals              = cPhaseAlignmentThisHybrid->getSummary<PortAlignmentVals>();
                cPhaseAlVals.clear();
                for(uint8_t cPhyPortChannel = 0; cPhyPortChannel < 4; cPhyPortChannel += 1) // 4 inputs per phy port
                {
                    std::vector<uint8_t> cVals(12, 0); // 12 phy ports
                    cPhaseAlVals.push_back(cVals);
                }
                auto& cWordAlignmentThisHybrid = cWordAlignmentThisOpticalGroup->at(cHybrid->getIndex());
                auto& cWordAlignmentVals       = cWordAlignmentThisHybrid->getSummary<PortAlignmentVals>();
                cWordAlignmentVals.clear();
                for(uint8_t cFeId = 0; cFeId < 8; cFeId += 1)
                {
                    std::vector<uint8_t> cVals(5, 0); // 6 stub lines per FE
                    cWordAlignmentVals.push_back(cVals);
                }

                // configure CBCs
                for(auto cChip: *cHybrid)
                {
                    // check chip type
                    fWithMPA = fWithMPA || (cChip->getFrontEndType() == FrontEndType::MPA);
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
                auto& cPhaseAlignment           = cPhaseAlignmentThisHybrid->getSummary<PortAlignmentVals>(); //[pLine];

                auto&  cCic         = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                auto   cOptimalTaps = fCicInterface->GetOptimalTaps(cCic);
                size_t cPhyPort     = 0;
                size_t cPhyPortChnl = 0;
                size_t cCounter     = 0;
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                    std::string cOutput;
                    char        cBuffer[80];
                    // first all the stub lines
                    for(uint8_t cInput = 0; cInput < 5; cInput += 1)
                    {
                        cPhaseAlignment[cPhyPortChnl][cPhyPort] = cOptimalTaps[cPhyPortChnl][cPhyPort];
                        sprintf(cBuffer, "%.2d ", cOptimalTaps[cPhyPortChnl][cPhyPort]);
                        cOutput += cBuffer;
                        cPhyPort     = ((cCounter + 1) % 4 == 0) ? (cPhyPort + 1) : cPhyPort;
                        cPhyPortChnl = cCounter % 4;
                        cCounter++;
                    }
                    // then the L1 line
                    size_t cPhyPortL1                           = (cChip->getId() % 8 > 3) ? 11 : 10;
                    size_t cPhyPortChnlL1                       = (cChip->getId() % 8 % 4);
                    cPhaseAlignment[cPhyPortChnlL1][cPhyPortL1] = cOptimalTaps[cPhyPortChnlL1][cPhyPortL1];
                    sprintf(cBuffer, "%.2d ", cOptimalTaps[cPhyPortChnlL1][cPhyPortL1]);
                    cOutput += cBuffer;
                    LOG(INFO) << BOLDBLUE << "Optimal tap found on FE" << +cChip->getId() << " : " << cOutput << RESET;
                }
                fCicInterface->SetStaticPhaseAlignment(cCic);
            }
        }
        // for(auto cOpticalGroup: *cBoard)
        // {
        //     for(auto cHybrid: *cOpticalGroup)
        //     {
        //         auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        //         // 4 channels per phyPort ... 12 phyPorts per CIC
        //         // std::vector<std::vector<uint8_t>> cPhaseTaps(4, std::vector<uint8_t>(12, 0));
        //         // 8 FEs per CIC .... 6 SLVS lines per FE
        //         std::vector<std::vector<uint8_t>> cPhaseTapsFEs(8, std::vector<uint8_t>(6, 0));
        //         // read back phase aligner values
        //         LOG(INFO) << BOLDBLUE << "Phase aligner on CIC " << BOLDGREEN << " LOCKED " << BOLDBLUE << " ... storing values and swithcing to static phase " << RESET;
        //         for(auto cChip: *cHybrid)
        //         {
        //             if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
        //             auto cTaps = fCicInterface->GetOptimalTaps(cCic, cChip->getId());
        //             for(size_t cIndx = 0; cIndx < cTaps.size(); cIndx++) cPhaseTapsFEs[cChip->getId()][cIndx] = cTaps[cIndx];
        //         } // loop over FEs

        //         for(auto cChip: *cHybrid)
        //         {
        //             if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

        //             std::string cOutput;
        //             for(uint8_t cInput = 0; cInput < 6; cInput += 1)
        //             {
        //                 char cBuffer[80];
        //                 sprintf(cBuffer, "%.2d ", cPhaseTapsFEs[cChip->getId()][cInput]);
        //                 cOutput += cBuffer;
        //             }
        //             LOG(INFO) << BOLDBLUE << "Optimal tap found on CIC phy-port input connected to FE#" << +cChip->getId() << " : " << cOutput << RESET;
        //         }
        //         // put phase aligner in static mode

        //         fCicInterface->SetStaticPhaseAlignment(cCic);
        //     } // hybrid
        // }     //
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
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, true);
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
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, false);
    }
    return cAligned;
}

bool CicFEAlignment::PhaseAlignment(uint16_t pWait_us, uint32_t pNTriggers)
{
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

                for(auto cChip: *cHybrid)
                {
                    cWithCBC = cWithCBC || ( cChip->getFrontEndType() == FrontEndType::CBC3 );
                    if( cWithCBC ) static_cast<CbcInterface*>(fReadoutChipInterface)->producePhaseAlignmentPattern(cChip,pWait_us*1000);
                    else static_cast<MPAInterface*>(fReadoutChipInterface)->producePhaseAlignmentPattern(cChip,pWait_us*1000);
                }    
            }
        }
        // send N triggers on L1 lines 
        if( cWithCBC )
        {
            LOG(INFO) << BOLDBLUE << "Sending triggers with to FEs.." << RESET;
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
                    LOG(INFO) << BOLDBLUE << "Phase aligner on CIC " << BOLDGREEN << " LOCKED " << BOLDBLUE << " ... storing values and switching to static phase " << RESET;
                    this->SetStaticPhaseAlignment();
                }
                cAligned = cAligned && cLocked;
            } // CICs
        }// OG
    }
    return cAligned;
}
bool CicFEAlignment::WordAlignment(uint16_t pWait_us)
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
                auto& cWordAlignmentVals       = cWordAlignmentThisHybrid->getSummary<PortAlignmentVals>();

                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                if(cCic == NULL) continue;
                
                // configure word alignment pattern on CBCs 
                std::vector<uint8_t> cAlignmentPatterns;
                for(auto cChip: *cHybrid)
                {
                    if( cChip->getFrontEndType() == FrontEndType::CBC3 ){
                        static_cast<CbcInterface*>(fReadoutChipInterface)->produceWordAlignmentPattern(cChip);
                        cAlignmentPatterns = static_cast<CbcInterface*>(fReadoutChipInterface)->getWordAlignmentPatterns();
                    }
                    else{ 
                        static_cast<MPAInterface*>(fReadoutChipInterface)->produceWordAlignmentPattern(cChip);
                        cAlignmentPatterns = static_cast<MPAInterface*>(fReadoutChipInterface)->getWordAlignmentPatterns();
                    }
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
                        if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                        std::string cOutput;
                        for(size_t cLine = 0; cLine < 5; cLine++)
                        {
                            cWordAlignmentVals[cChip->getId() % 8][cLine] = cWordAlignmentValues[cChip->getId() % 8][cLine];
                            char cBuffer[80];
                            sprintf(cBuffer, "%.2d ", cWordAlignmentVals[cChip->getId() % 8][cLine]);
                            cOutput += cBuffer;
                        }
                        LOG(INFO) << BOLDBLUE << "Word alignment values for FE#" << +cChip->getId() << " : " << cOutput << RESET;
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
