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

    // // read original thresholds from chips ...
    // ContainerFactory::copyAndInitStructure<uint16_t>(*fDetectorContainer, fThresholds);
    // // read original logic configuration from chips .. [Pipe&StubInpSel&Ptwidth , HIP&TestMode]
    // ContainerFactory::copyAndInitStructure<uint16_t>(*fDetectorContainer, fLogic);
    // ContainerFactory::copyAndInitStructure<uint16_t>(*fDetectorContainer, fHIPs);
    // ContainerFactory::copyAndInitStructure<uint16_t>(*fDetectorContainer, fPtCuts);
    ContainerFactory::copyAndInitHybrid<PortAlignmentVals>(*fDetectorContainer, fPhaseAlignmentValues);
    ContainerFactory::copyAndInitHybrid<PortAlignmentVals>(*fDetectorContainer, fWordAlignmentValues);

    for(auto cBoard: *fDetectorContainer)
    {
        // auto& cThresholdsThisBoard     = fThresholds.at(cBoard->getIndex());
        // auto& cLogicThisBoard          = fLogic.at(cBoard->getIndex());
        // auto& cHIPsThisBoard           = fHIPs.at(cBoard->getIndex());
        // auto& cPtCutThisBoard          = fPtCuts.at(cBoard->getIndex());
        auto& cPhaseAlignmentThisBoard = fPhaseAlignmentValues.at(cBoard->getIndex());
        auto& cWordAlignmentThisBoard  = fWordAlignmentValues.at(cBoard->getIndex());

        for(auto cOpticalGroup: *cBoard)
        {
            // auto& cThresholdsThisOpticalGroup     = cThresholdsThisBoard->at(cOpticalGroup->getIndex());
            // auto& cLogicThisOpticalGroup          = cLogicThisBoard->at(cOpticalGroup->getIndex());
            // auto& cHIPsThisOpticalGroup           = cHIPsThisBoard->at(cOpticalGroup->getIndex());
            // auto& cPtCutThisOpticalGroup          = cPtCutThisBoard->at(cOpticalGroup->getIndex());
            auto& cPhaseAlignmentThisOpticalGroup = cPhaseAlignmentThisBoard->at(cOpticalGroup->getIndex());
            auto& cWordAlignmentThisOpticalGroup  = cWordAlignmentThisBoard->at(cOpticalGroup->getIndex());

            for(auto cHybrid: *cOpticalGroup)
            {
                // auto& cThresholdsThisHybrid     = cThresholdsThisOpticalGroup->at(cHybrid->getIndex());
                // auto& cLogicThisHybrid          = cLogicThisOpticalGroup->at(cHybrid->getIndex());
                // auto& cHIPsThisHybrid           = cHIPsThisOpticalGroup->at(cHybrid->getIndex());
                // auto& cPtCutThisHybrid          = cPtCutThisOpticalGroup->at(cHybrid->getIndex());
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
        //
        auto&                cBoardRegNap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        const BeBoardRegMap& cOrigRegMap  = static_cast<const BeBoard*>(cBoard)->getBeBoardRegMap();
        cBoardRegNap.insert(cOrigRegMap.begin(), cOrigRegMap.end());
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

    // read back original masks
    // read back original masks
    ContainerFactory::copyAndInitChip<const ChannelGroup<NCHANNELS>*>(*fDetectorContainer, fChipMasks);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cMasksThisBrd = fChipMasks.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cMasksThisOG = cMasksThisBrd->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cMasksThisHybrid = cMasksThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    const ChannelGroup<NCHANNELS>* cMsk           = static_cast<const ChannelGroup<NCHANNELS>*>(cChip->getChipOriginalMask());
                    auto&                          cMasksThisChip = cMasksThisHybrid->at(cChip->getIndex());
                    if(cChip->getFrontEndType() == FrontEndType::CBC3)
                    {
                        auto& cOriginalMask = cMasksThisChip->getSummary<ChannelGroup<NCHANNELS>*>();
                        cOriginalMask       = new ChannelGroup<NCHANNELS, 1>;
                        for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                        {
                            bool cEnabled = cMsk->isChannelEnabled(cChnl);
                            if(cEnabled)
                                cOriginalMask->enableChannel(cChnl);
                            else
                                cOriginalMask->disableChannel(cChnl);
                        }
                    }
                    // if( cChip->getFrontEndType() == FrontEndType::SSA )  cOriginalMask = new ChannelGroup<NSSACHANNELS, 1>;
                    // if( cChip->getFrontEndType() == FrontEndType::MPA )  cOriginalMask = new ChannelGroup<NSSACHANNELS, NMPACOLS>;
                    // to -do .. same for MPA where have to look over cols
                }
            } // hybrids
        }     // OG
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
void CicFEAlignment::Running()
{
    Initialise();
    bool cPhaseAligned = true;
    if(fWithMPA)
        cPhaseAligned = this->PhaseAlignmentMPA();
    else
        cPhaseAligned = this->PhaseAlignment();
    if(!cPhaseAligned)
    {
        LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " phase alignment step on CIC input .. " << RESET;
        exit(FAILED_PHASE_ALIGNMENT);
    }
    LOG(INFO) << BOLDGREEN << "SUCCESSFUL " << BOLDBLUE << " phase alignment on CIC inputs... " << RESET;
    fSuccess = cPhaseAligned;

    bool cWordAligned = this->WordAlignment();
    if(!cWordAligned)
    {
        LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << "word alignment step on CIC input .. " << RESET;
        exit(FAILED_WORD_ALIGNMENT);
    }

    LOG(INFO) << BOLDGREEN << "SUCCESSFUL " << BOLDBLUE << " word alignment on CIC inputs... " << RESET;
    // automatic alignment
    // TO-DO ADD alignment for PS
    bool cBxAligned = (fWithMPA) ? this->SetBx0Delay(fStubBxDelayPS) : this->SetBx0Delay(fStubBxDelay2S);
    if(!cBxAligned)
    {
        LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " bx0 alignment step in CIC ... " << RESET;
        exit(FAILED_BX_ALIGNMENT);
    }
    LOG(INFO) << BOLDGREEN << "SUCCESSFUL " << BOLDBLUE << " bx0 alignment step in CIC ... " << RESET;
    fSuccess = (cPhaseAligned && cWordAligned && cBxAligned);
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
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
        // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Bx0Alignment();
    }
    return true;
}

bool CicFEAlignment::ManualPhaseAlignment(uint16_t pPhase)
{
    bool cConfigured = true;
    // for(auto cBoard: *fDetectorContainer)
    // {
    //     for(auto cOpticalGroup: *cBoard)
    //     {
    //         for(auto cHybrid: *cOpticalGroup)
    //         {
    //             auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
    //             if(cCic != NULL)
    //             {
    //                 fCicInterface->SetAutomaticPhaseAlignment(cCic, false);
    //                 for(auto cChip: *cHybrid)
    //                 {
    //                     for(int cLineId = 0; cLineId < 6; cLineId++) { cConfigured = cConfigured && fCicInterface->SetStaticPhaseAlignment(cCic, cChip->getId(), cLineId, pPhase); }
    //                 }
    //             }
    //         }
    //     }
    //     fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    // }
    return cConfigured;
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
                    size_t cPhyPortL1                           = (cChip->getId() > 3) ? 11 : 10;
                    size_t cPhyPortChnlL1                       = (cChip->getId() % 4);
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
bool CicFEAlignment::PhaseAlignmentMPA(uint16_t pWait_ms)
{
    // MPA phase alignment
    bool    cAligned   = true;
    auto    cSetting   = fSettingsMap.find("SLVSDrive");
    uint8_t cSLVSDrive = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 7;
    LOG(INFO) << BOLDBLUE << "Starting CIC automated phase alignment procedure for MPAs .... " << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                // enable MPA alignment pattern
                LOG(INFO) << GREEN << "Enabling MPA Alignment pattern" << RESET;
                std::vector<uint8_t>     cOriginalValues;
                std::vector<std::string> cRegs;
                uint8_t                  cAlignmentPattern = 0xAA;
                std::vector<uint8_t>     cRegValues{0x2, cAlignmentPattern};
                std::vector<std::string> cRegNames{"ReadoutMode", "LFSR_data"};
                for(size_t cIndex = 0; cIndex < cRegValues.size(); cIndex++)
                {
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                        cOriginalValues.push_back(fReadoutChipInterface->ReadChipReg(cChip, cRegNames[cIndex]));
                        cRegs.push_back(cRegNames[cIndex]);
                        fReadoutChipInterface->WriteChipReg(cChip, cRegNames[cIndex], cRegValues[cIndex]);
                    } // loop over MPAs
                }     // loop over registers

                // configure SLVS drive
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
                    fReadoutChipInterface->WriteChipReg(cChip, "SLVSDrive", cSLVSDrive);
                } // loop over MPAs

                
                // send a resync
                fBeBoardInterface->ChipReSync(cBoard);

                // enable automatic phase aligner
                fCicInterface->SetAutomaticPhaseAlignment(static_cast<OuterTrackerHybrid*>(cHybrid)->fCic, true);
                bool cLocked = fCicInterface->CheckPhaseAlignerLock(cCic);
                // if locked .. switch to automatic phase aligner mode with best values
                if(cLocked)
                {
                    LOG(INFO) << BOLDBLUE << "Phase aligner on CIC " << BOLDGREEN << " LOCKED " << BOLDBLUE << " ... storing values and switching to static phase " << RESET;
                    // this->SetStaticPhaseAlignment();
                }
                cAligned = cAligned && cLocked;

                // reset original values
                for(size_t cIndex = 0; cIndex < cRegs.size(); cIndex++)
                {
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                        fReadoutChipInterface->WriteChipReg(cChip, cRegs[cIndex], cOriginalValues[cIndex]);
                    } // loop over MPAs
                }
            } // hybrid
        }     // optical group
    }         // board
    //(static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, 4);
    // check alignment and use static phase from now on
    return cAligned;
}
void CicFEAlignment::InjectAlignmentPattern(uint8_t pChipId, uint8_t pPhyPort)
{
    std::vector<uint8_t> cSeeds_ph3{42, 0x52, 0xAA};
    uint8_t              cBendCode_phAlign = 0xa;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            LOG(INFO) << BOLDBLUE << "OpticalGroup " << +cOpticalGroup->getIndex() << RESET;
            for(auto cHybrid: *cOpticalGroup)
            {
                LOG(INFO) << BOLDBLUE << "Hybrid " << +cHybrid->getIndex() << RESET;
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                // generate alignment pattern on all stub lines
                LOG(INFO) << BOLDBLUE << "Generating STUB patterns needed for phase alignment on FE" << +cHybrid->getId() << RESET;
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getId() != pChipId) continue;

                    LOG(INFO) << BOLDBLUE << "Injecting CIC phase alignment pattern for CBC .... " << +cChip->getId() << RESET;
                    ReadoutChip* theReadoutChip = static_cast<ReadoutChip*>(cChip);
                    // enable stub logic
                    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(theReadoutChip, "Sampled", true, true);
                    // switch on HitOr
                    fReadoutChipInterface->WriteChipReg(theReadoutChip, "HitOr", 1);
                    // set PtCut to maximum
                    fReadoutChipInterface->WriteChipReg(theReadoutChip, "PtCut", 14);

                    // read bend LUT
                    std::vector<uint8_t> cBendLUT  = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theReadoutChip);
                    auto                 cIterator = std::find(cBendLUT.begin(), cBendLUT.end(), cBendCode_phAlign);
                    if(cIterator != cBendLUT.end())
                    {
                        int    cPosition    = std::distance(cBendLUT.begin(), cIterator);
                        double cBend_strips = -7. + 0.5 * cPosition;
                        LOG(DEBUG) << BOLDBLUE << "Bend code of " << std::bitset<4>(cBendCode_phAlign) << " found for bend reg " << +cPosition << " which means " << cBend_strips << " strips."
                                   << RESET;

                        std::vector<uint8_t> cSeeds{0x11, 0x55, 0x99};
                        std::vector<int>     cBends(cSeeds.size(), static_cast<int>(cBend_strips * 2));
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theReadoutChip, cSeeds, cBends, true);
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
                fCicInterface->SelectMux(cCic, pPhyPort);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // std::this_thread::sleep_for (std::chrono::milliseconds(500));
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(cBoard, cHybrid->getId(), 0, 1, 0x11, 8);
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(cBoard, cHybrid->getId(), 0, 2, 0x55, 8);
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(cBoard, cHybrid->getId(), 0, 3, 0x99, 8);
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(cBoard, cHybrid->getId(), 0, 4, 0xAA, 8);

                // //align
                // for( int cLine=1; cLine < 5; cLine++)
                // {
                //     if( cLine < 3 )
                //         static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning( cBoard, cHybrid->getId() , 0 , cLine , cSeeds_ph3[cLine-1] , 8);
                //     else
                //         static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning( cBoard, cHybrid->getId() , 0 , cLine , ( cBendCode_phAlign << 4 ) | cBendCode_phAlign
                //         , 8);
                // }

                if(pPhyPort < 10) // stub line
                    (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, 4);
            }
        }
    }
}
bool CicFEAlignment::PhaseAlignment(uint16_t pWait_ms, uint32_t pNTriggers)
{
    bool cAligned = true;
    LOG(INFO) << BOLDBLUE << "Starting CIC automated phase alignment procedure for CBCs .... " << RESET;

    // read back original masks
    DetectorDataContainer cChipMasks;
    ContainerFactory::copyAndInitChip<const ChannelGroup<NCHANNELS>*>(*fDetectorContainer, cChipMasks);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cMasksThisBrd = cChipMasks.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cMasksThisOG = cMasksThisBrd->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cMasksThisHybrid = cMasksThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cMasksThisChip = cMasksThisHybrid->at(cChip->getIndex());
                    auto& cOriginalMask  = cMasksThisChip->getSummary<const ChannelGroup<NCHANNELS>*>();
                    cOriginalMask        = static_cast<const ChannelGroup<NCHANNELS>*>(cChip->getChipOriginalMask());
                }
            } // hybrids
        }     // OG
    }

    for(auto cBoard: *fDetectorContainer)
    {
        // original threshold + logic values
        // auto& cThresholdsThisBoard     = fThresholds.at(cBoard->getIndex());
        // auto& cLogicThisBoard          = fLogic.at(cBoard->getIndex());
        // auto& cHIPsThisBoard           = fHIPs.at(cBoard->getIndex());
        // auto& cPtCutThisBoard          = fPtCuts.at(cBoard->getIndex());

        ChannelGroup<NCHANNELS, 1> cChannelMask;
        cChannelMask.disableAllChannels();
        for(uint8_t cChannel = 0; cChannel < NCHANNELS; cChannel += 2) cChannelMask.enableChannel(cChannel); // generate a hit in every Nth channel

        // original masks for channels
        auto& cMasksThisBrd = cChipMasks.at(cBoard->getIndex());

        // stub lines
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cMasksThisOG = cMasksThisBrd->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cMasksThisHybrid = cMasksThisOG->at(cHybrid->getIndex());
                // enable automatic phase aligner
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetAutomaticPhaseAlignment(cCic, true);

                // generate alignment pattern on all stub lines
                LOG(INFO) << BOLDBLUE << "Generating STUB patterns needed for phase alignment on FE" << +cHybrid->getId() << RESET;
                for(auto cChip: *cHybrid)
                {
                    auto& cMasksThisChip = cMasksThisHybrid->at(cChip->getIndex());
                    auto& cOriginalMask  = cMasksThisChip->getSummary<const ChannelGroup<NCHANNELS>*>();

                    ReadoutChip* theReadoutChip = static_cast<ReadoutChip*>(cChip);
                    // original mask
                    // const ChannelGroup<NCHANNELS>* cOriginalMask = static_cast<const ChannelGroup<NCHANNELS>*>(cChip->getChipOriginalMask());
                    // enable stub logic
                    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(theReadoutChip, "Sampled", true, true);
                    // switch on HitOr
                    fReadoutChipInterface->WriteChipReg(theReadoutChip, "HitOr", 1);
                    // set PtCut to maximum
                    fReadoutChipInterface->WriteChipReg(theReadoutChip, "PtCut", 14);
                    // if I set this it doesn't work..
                    // no cluster cut
                    fReadoutChipInterface->WriteChipReg(theReadoutChip, "ClusterCut", 4);

                    // read bend LUT
                    uint8_t              cBendCode_phAlign = 0xa;
                    std::vector<uint8_t> cBendLUT          = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theReadoutChip);
                    auto                 cIterator         = std::find(cBendLUT.begin(), cBendLUT.end(), cBendCode_phAlign);
                    if(cIterator != cBendLUT.end())
                    {
                        int    cPosition    = std::distance(cBendLUT.begin(), cIterator);
                        double cBend_strips = -7. + 0.5 * cPosition;
                        // LOG(INFO) << BOLDBLUE << "Bend code of " << std::bitset<4>(cBendCode_phAlign) << " found for bend reg " << +cPosition << " which means " << cBend_strips << " strips." <<
                        // RESET;

                        // first pattern - stubs lines 0, 1 , 3
                        // seeds on stub line 0 , stub line 1
                        // bends on stub line 2
                        std::vector<uint8_t> cSeeds_ph1{0x55, 0xAA};
                        std::vector<int>     cBends_ph1(cSeeds_ph1.size(), static_cast<int>(cBend_strips * 2));
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theReadoutChip, cSeeds_ph1, cBends_ph1, true);
                        std::this_thread::sleep_for(std::chrono::milliseconds(pWait_ms));

                        // second pattern - 1, 2, 3 , 4
                        // whatever on stub line 0
                        // then alignment pattern on stub lines 1 + 2
                        std::vector<uint8_t> cSeeds_ph3{42, 0x55, 0xAA};
                        std::vector<int>     cBends_ph3(cSeeds_ph3.size(), static_cast<int>(cBend_strips * 2));
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theReadoutChip, cSeeds_ph3, cBends_ph3, true);
                        std::this_thread::sleep_for(std::chrono::milliseconds(pWait_ms));
                    }
                    fReadoutChipInterface->maskChannelsGroup(theReadoutChip, cOriginalMask);
                }

                LOG(INFO) << BOLDBLUE << "Generating HIT patterns needed for phase alignment on FE" << +cHybrid->getId() << RESET;
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theReadoutChip = static_cast<ReadoutChip*>(cChip);
                    // original mask
                    fReadoutChipInterface->maskChannelsGroup(theReadoutChip, &cChannelMask);
                }
            }
        }
        // l1 lines
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.stub_debug.enable", 0x00);
        // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(pNTriggers, 100, 3);
        uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
        uint16_t cSrc        = 3;
        if(cTriggerSrc != cSrc)
        {
            LOG(INFO) << BOLDBLUE << "\t.. Changing trigger source is set to " << +cSrc << RESET;
            std::vector<std::pair<std::string, uint32_t>> cRegVec;
            cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cSrc});
            cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
            fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
        }

        // count triggers sent to the CIC
        bool   cAllTriggersSent = false;
        size_t cAttempt         = 0;
        size_t cMaxAttempts     = 10;
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetTriggerFSM();
        auto cNTriggersSent = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
        do {
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Start();
            do {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                cNTriggersSent = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
                // LOG(INFO) << BOLDBLUE << "\t... during CIC phase alignment of L1 lines from CBC " << +cNTriggersSent << " triggers sent." << RESET;
                cAllTriggersSent = (cNTriggersSent >= pNTriggers);
            } while(!cAllTriggersSent);
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Stop();
            if(cNTriggersSent == 0) static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetTriggerFSM();
            cAttempt++;
        } while(cNTriggersSent == 0 && cAttempt < cMaxAttempts);

        // set trigger source back
        if(cTriggerSrc != cSrc)
        {
            LOG(INFO) << BOLDBLUE << "\t.. Changing trigger source back to " << +cTriggerSrc << RESET;
            std::vector<std::pair<std::string, uint32_t>> cRegVec;
            cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
            cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
            fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
        }
        // send a resync
        fBeBoardInterface->ChipReSync(cBoard);
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
        }     // OG
    }
    return cAligned;
}
void CicFEAlignment::WordAlignmentPattern(ReadoutChip* pChip, std::vector<uint8_t> pAlignmentPatterns)
{
    // enable stub logic
    if(pChip->getFrontEndType() == FrontEndType::CBC3)
    {
        LOG(INFO) << GREEN << "Configuring CBC3 Alignment pattern" << RESET;

        static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(pChip, "Sampled", true, true);
        // switch on HitOr
        fReadoutChipInterface->WriteChipReg(pChip, "HitOr", 0);
        // set PtCut to maxmim
        fReadoutChipInterface->WriteChipReg(pChip, "PtCut", 14);
        // no cluster cut
        fReadoutChipInterface->WriteChipReg(pChip, "ClusterCut", 4);

        std::vector<uint8_t> cStubs{pAlignmentPatterns[0], pAlignmentPatterns[1], pAlignmentPatterns[2]};
        std::vector<uint8_t> cBendLUT = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(pChip);
        std::vector<uint8_t> cBendCodes{
            static_cast<uint8_t>(pAlignmentPatterns[3] & 0x0F), static_cast<uint8_t>((pAlignmentPatterns[3] & 0xF0) >> 4), static_cast<uint8_t>(pAlignmentPatterns[4] & 0x0F)};
        std::vector<int> cBends(3, 0);
        for(size_t cIndex = 0; cIndex < cBendCodes.size(); cIndex += 1)
        {
            auto cIterator = std::find(cBendLUT.begin(), cBendLUT.end(), cBendCodes[cIndex]);
            if(cIterator != cBendLUT.end())
            {
                int    cPosition    = std::distance(cBendLUT.begin(), cIterator);
                double cBend_strips = -7. + 0.5 * cPosition;
                cBends[cIndex]      = cBend_strips * 2;
                LOG(DEBUG) << BOLDBLUE << "Bend code of " << std::bitset<4>(cBendCodes[cIndex]) << " found for bend reg " << +cPosition << " which means " << cBend_strips << " strips." << RESET;
            }
        }
        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(pChip, cStubs, cBends);
    }
    else
    {
        // enable MPA alignment pattern
        LOG(INFO) << GREEN << "Configuring MPA Alignment pattern :"
                  << " MPA will output 0x" << std::hex << +pAlignmentPatterns[0] << std::dec << RESET;
        std::vector<uint8_t>     cOriginalValues;
        std::vector<uint8_t>     cRegValues{0x2, pAlignmentPatterns[0]};
        std::vector<std::string> cRegNames{"ReadoutMode", "LFSR_data"};
        for(size_t cIndx = 0; cIndx < cRegNames.size(); cIndx++) { fReadoutChipInterface->WriteChipReg(pChip, cRegNames[cIndx], cRegValues[cIndx]); }
    }
}
bool CicFEAlignment::WordAlignment(uint16_t pWait_ms)
{
    LOG(INFO) << BOLDBLUE << "Starting CIC automated word alignment procedure .... " << RESET;

    // phase alignment step - first 85 [] , 170 []
    bool                 cAligned = true;
    std::vector<uint8_t> cAlignmentPatterns_CBC{0x7A, 0xBC, 0xD4, 0x31, 0x81};
    std::vector<uint8_t> cAlignmentPatterns_MPA{0x81, 0x81, 0x81, 0x81, 0x81};

    // read back original masks
    DetectorDataContainer cChipMasks;
    ContainerFactory::copyAndInitChip<const ChannelGroup<NCHANNELS>*>(*fDetectorContainer, cChipMasks);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cMasksThisBrd = cChipMasks.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cMasksThisOG = cMasksThisBrd->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cMasksThisHybrid = cMasksThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cMasksThisChip = cMasksThisHybrid->at(cChip->getIndex());
                    auto& cOriginalMask  = cMasksThisChip->getSummary<const ChannelGroup<NCHANNELS>*>();
                    cOriginalMask        = static_cast<const ChannelGroup<NCHANNELS>*>(cChip->getChipOriginalMask());
                }
            } // hybrids
        }     // OG
    }

    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard                = static_cast<BeBoard*>(cBoard);
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

                // now inject stubs that can generate word alignment pattern
                std::vector<std::string> cRegNames{"ReadoutMode", "LFSR_data"};
                std::vector<uint8_t>     cOriginalValues;
                std::vector<std::string> cRegs;
                for(auto cRegName: cRegNames)
                {
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() == FrontEndType::MPA)
                        {
                            cOriginalValues.push_back(fReadoutChipInterface->ReadChipReg(cChip, cRegName));
                            cRegs.push_back(cRegName);
                        }
                    }
                }
                std::vector<uint8_t> cAlignmentPatterns;
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                    cAlignmentPatterns = (cChip->getFrontEndType() == FrontEndType::MPA) ? cAlignmentPatterns_MPA : cAlignmentPatterns_CBC;

                    this->WordAlignmentPattern(static_cast<ReadoutChip*>(cChip), cAlignmentPatterns);
                }
                // now send a fast reset
                // fBeBoardInterface->ChipReSync(theBoard);

                // run automated word alignment
                cAligned = cAligned && fCicInterface->AutomatedWordAlignment(cCic, cAlignmentPatterns, pWait_ms);
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
                            cWordAlignmentVals[cChip->getId()][cLine] = cWordAlignmentValues[cChip->getId()][cLine];
                            char cBuffer[80];
                            sprintf(cBuffer, "%.2d ", cWordAlignmentVals[cChip->getId()][cLine]);
                            cOutput += cBuffer;
                        }
                        LOG(INFO) << BOLDBLUE << "Word alignment values for FE#" << +cChip->getId() << " : " << cOutput << RESET;
                    }
                }
                else
                    LOG(INFO) << BOLDBLUE << "Automated word alignment procedure " << BOLDRED << " FAILED!" << RESET;

                // reset original register values
                for(size_t cIndx = 0; cIndx < cRegs.size(); cIndx++)
                {
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                        fReadoutChipInterface->WriteChipReg(cChip, cRegs[cIndx], cOriginalValues[cIndx]);
                    }
                }
            }
        }

        // original masks for channels of all ROCs
        auto& cMasksThisBrd = cChipMasks.at(cBoard->getIndex());
        // re-configure original mask
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cMasksThisOG = cMasksThisBrd->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cMasksThisHybrid = cMasksThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cMasksThisChip = cMasksThisHybrid->at(cChip->getIndex());
                    auto& cOriginalMask  = cMasksThisChip->getSummary<const ChannelGroup<NCHANNELS>*>();
                    fReadoutChipInterface->maskChannelsGroup(cChip, cOriginalMask);
                }
            } // hybrids
        }     // OG
        // now send a fast reset
        fBeBoardInterface->ChipReSync(theBoard);
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
