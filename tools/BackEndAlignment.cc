#include "BackEndAlignment.h"

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "D19cDebugFWInterface.h"
#include "boost/format.hpp"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

BackEndAlignment::BackEndAlignment() : LinkAlignmentOT() {}

BackEndAlignment::~BackEndAlignment() {}

void BackEndAlignment::Initialise()
{
    fSuccess = false;
    // this is needed if you're going to use groups anywhere
    CBCChannelGroupHandler theChannelGroupHandler;
    theChannelGroupHandler.setChannelGroupParameters(16, 2);
    setChannelGroupHandler(theChannelGroupHandler);
    // prepare common OTTool
    Prepare();
    SetName("BackEndAlignment");

    // list of board registers that can be modified by this tool
    std::vector<std::string> cBrdRegsToKeep{"fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay"};
    SetBrdRegstoPerserve(cBrdRegsToKeep);

    // pair select for PS-FEHs
    fPairSelect = std::to_string(findValueInSettings<double>("EnablePairSelect", 0));

    // retreive original settings for all chips and all back-end boards
    ContainerFactory::copyAndInitHybrid<uint8_t>(*fDetectorContainer, fEnabledFEs);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cEnabledFEs = fEnabledFEs.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cEnabledFEsOG = cEnabledFEs->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cEnabledFEsHybrid = cEnabledFEsOG->at(cHybrid->getIndex());
                auto& cEnabled          = cEnabledFEsHybrid->getSummary<uint8_t>();
                cEnabled                = 0;
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::CBC3) cEnabled = cEnabled | (1 << cChip->getId());
                }
            }
        }
    }
}

bool BackEndAlignment::PSAlignment(BeBoard* pBoard, std::string pSSAPair)
{
    bool cTuned = true;
    LOG(INFO) << GREEN << "BackEndAlignment for PS Chip(s)" << RESET;
    fBeBoardInterface->setBoard(pBoard->getId());
    D19cDebugFWInterface* cDebugInterface        = static_cast<D19cDebugFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    uint8_t               cPhaseAlignmentPattern = 0xAA;
    uint8_t               cWordAlignmentPattern  = 0xEA;
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                ReadoutChip* cReadoutChip = static_cast<ReadoutChip*>(cChip);
                if(cChip->getFrontEndType() == FrontEndType::SSA2 || cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    if( cReadoutChip->getId()!=(int)(pSSAPair[0]-'0') && cReadoutChip->getId()!=(int)(pSSAPair[1]-'0') )
                        continue;
                    auto cDriveStrength = fReadoutChipInterface->ReadChipReg(cChip, "SLVS_pad_current_L1");
                    LOG(INFO) << BOLDBLUE << "SSA#" << +cChip->getId() << " Alignment for L1 and stub lines.. L1 drive set to " << +cDriveStrength << RESET;

                    std::vector<uint8_t> cAlVals(9, 0);
                    uint8_t              cPairId = (cChip->getId() % 2 == 0) ? 1 : 0;
                    uint8_t              cChipId = (pSSAPair != "") ? cPairId : cChip->getId();
                    cPairId                      = (pSSAPair != "") ? cPairId : 0;

                    // select SSA pair
                    if(pSSAPair == "")
                        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.multiplexing_bp.ssa_pair_select", 0x4);
                    cWordAlignmentPattern = (cPairId == 0) ? 0xCA : 0xF0;
                    if(pSSAPair != "")
                        LOG(INFO) << BOLDBLUE << "Backend alignment for SSA " << +cPairId << " in pair [ChipId in BE is  " << +cChip->getId() << " ]" << RESET;
                    else
                        LOG(INFO) << BOLDBLUE << "Backend alignment for SSA#" << +cChipId << RESET;
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "EnableSLVSTestOutput", 0x1);
                    std::vector<uint8_t> cPhaseTaps(9,0);
                    for(uint8_t cLineId = 1; cLineId <= 8; cLineId++) // stub lines - 1 to 8
                    {
                        std::stringstream cRegName;
                        cRegName << "OutPatternStubLine" << +(cLineId - 1);
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), cPhaseAlignmentPattern);
                        auto cLineAlignmentStatus = PhaseTuneLine(cReadoutChip, cLineId);
                        if(cLineAlignmentStatus.first) cPhaseTaps.push_back(cLineAlignmentStatus.second);

                        // cTuner.TunePhase(cInterface, cChip->getHybridId(), cChipId, cLineId);
                        // cPhaseTaps.push_back(cTuner.fDelay);
                        // if( cReadoutChip->getFrontEndType() == FrontEndType::SSA ) continue;

                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), cWordAlignmentPattern);
                        cLineAlignmentStatus = WordAlignLine(cReadoutChip, cLineId, cWordAlignmentPattern, 8);
                        cAlVals[cLineId]     = (cLineAlignmentStatus.first) ? 1 : 0;
                        // cTuner.AlignWord(cInterface, cChip->getHybridId(), cChipId, cLineId, cWordAlignmentPattern, 8, true);
                        // cAlVals[cLineId] = cTuner.GetLineStatus(cInterface, cHybrid->getHybridId(), cChipId, cLineId);
                        LOG(DEBUG) << +cAlVals[cLineId] << RESET;
                    }
                    for(uint8_t cLineId = 0; cLineId < 1; cLineId++) // L1 line - line 0
                    {
                        if(cReadoutChip->getFrontEndType() == FrontEndType::SSA)
                        {
                            cAlVals[cLineId] = 1;
                            ManuallyConfigureLine(cReadoutChip, cLineId, cPhaseTaps[cPhaseTaps.size() - 1], 0);
                            // uint8_t cMode    = 2;
                            // cTuner.SetLineMode(cInterface, cHybrid->getHybridId(), cChipId, cLineId, cMode, cPhaseTaps[cPhaseTaps.size() - 1], 0, 0, 0);
                            continue;
                        }
                        std::stringstream cRegName;
                        cRegName << "OutPatternL1Line";
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), cPhaseAlignmentPattern);
                        auto cLineAlignmentStatus = PhaseTuneLine(cReadoutChip, cLineId);
                        if(cLineAlignmentStatus.first) cPhaseTaps.push_back(cLineAlignmentStatus.second);
                        // cTuner.TunePhase(cInterface, cChip->getHybridId(), cChipId, cLineId);
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), cWordAlignmentPattern);
                        cLineAlignmentStatus = WordAlignLine(cReadoutChip, cLineId, cWordAlignmentPattern, 8);
                        cAlVals[cLineId]     = (cLineAlignmentStatus.first) ? 1 : 0;

                        // cTuner.AlignWord(cInterface, cChip->getHybridId(), cChipId, cLineId, cWordAlignmentPattern, 8, true);
                        // cAlVals[cLineId] = cTuner.GetLineStatus(cInterface, cHybrid->getHybridId(), cChipId, cLineId);

                        LOG(DEBUG) << +cAlVals[cLineId] << RESET;
                    }

                    // back to readout mode 0 to look at the L1 data
                    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
                    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cChipId);
                    cDebugInterface->StubDebug(true, 8);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "EnableSLVSTestOutput", 0x0, false);
                    cDebugInterface->L1ADebug();

                    // Reset to original values
                    for(uint8_t cLineId = 0; cLineId < 8; cLineId++) // stub lines
                    {
                        std::stringstream cRegName;
                        cRegName << "OutPatternStubLine" << +cLineId;
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), 0x00);
                    }
                    for(uint8_t cLineId = 0; cLineId < 1; cLineId++) // L1 line
                    {
                        if(cReadoutChip->getFrontEndType() == FrontEndType::SSA) continue;
                        std::stringstream cRegName;
                        cRegName << "OutPatternL1Line";
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), 0x00);
                    }
                    cTuned = cTuned && (std::accumulate(cAlVals.begin(), cAlVals.end(), 0) == 9);
                }
                else
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
                    {
                        cTuned = cTuned && LineTuning(cReadoutChip, cLineId, cAlignmentPattern, 8);
                        // cTuned =
                        //     cTuned && static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getId(), cChip->getId(), cLineId, cAlignmentPattern, 8);
                    }

                    for(size_t cIndex = 0; cIndex < 3; cIndex++) { fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cOriginalValues[cIndex]); };
                }
            }
        }
    }

    if(cTuned)
        LOG(INFO) << BOLDGREEN << "PS Phase+Word Alignment succesful" << RESET;
    else
        LOG(INFO) << BOLDRED << "FAILED PS BE-Alignment" << RESET;
    return cTuned;
}
// re-use function from link alignment
bool BackEndAlignment::CICAlignment(BeBoard* pBoard)
{
    LinkAlignmentOT::Inherit(this);
    LinkAlignmentOT::Initialise();

    if(!PhaseAlignBEdata(pBoard)) return false;
    if(!WordAlignBEdata(pBoard)) return false;

    LinkAlignmentOT::Reset();

    for(auto cOpticalGroup: *pBoard)
    {
        size_t cNlines = (cOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
        for(auto cHybrid: *cOpticalGroup)
        {
            for(size_t cLineId = 0; cLineId < cNlines; cLineId++)
            {
                auto cDelay   = getBeSamplingDelay(pBoard->getIndex(), cOpticalGroup->getIndex(), cHybrid->getIndex(), cLineId);
                auto cBitslip = getBeBitSlip(pBoard->getIndex(), cOpticalGroup->getIndex(), cHybrid->getIndex(), cLineId);
                if(cLineId == 0)
                    LOG(INFO) << BOLDMAGENTA << "Delay on L1A line is " << +cDelay << "\t\t..Bitslip on Line#" << +cLineId << " is " << +cBitslip << RESET;
                else
                    LOG(INFO) << BOLDMAGENTA << "Delay on Stub line#" << +cLineId << " is " << +cDelay << "\t\t..Bitslip on Line#" << +cLineId << " is " << +cBitslip << RESET;
            }
        } // hybrids
    }     // OGs
    return true;
}

bool BackEndAlignment::CBCAlignment(BeBoard* pBoard)
{
    bool cAligned = true;

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cReadoutChip: *cHybrid)
            {
                ReadoutChip* theReadoutChip = static_cast<ReadoutChip*>(cReadoutChip);
                fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cReadoutChip->getId());
                // original mask
                auto cOriginalMask = std::static_pointer_cast<ChannelGroup<NCHANNELS>>(cReadoutChip->getChipOriginalMask());
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
                // LOG(DEBUG) << BOLDBLUE << "Bend code of " << +cBendCode_phAlign << " found in register " << cPosition << " so a bend of " << cBend_strips << RESET;

                uint8_t              cSuccess = 0x00;
                std::vector<uint8_t> cSeeds{0x82, 0x8E, 0x9E};
                std::vector<int>     cBends(cSeeds.size(), static_cast<int>(cBend_strips * 2));
                static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theReadoutChip, cSeeds, cBends);
                // first align lines with stub seeds
                uint8_t cLineId = 1;
                for(size_t cIndex = 0; cIndex < 3; cIndex++)
                {
                    cSuccess = cSuccess | (LineTuning(cReadoutChip, cLineId, cSeeds[cIndex], 8) << cIndex);
                    cLineId++;
                }
                // then align lines with stub bends
                uint8_t cAlignmentPattern = (cBendCode_phAlign << 4) | cBendCode_phAlign;
                cSuccess                  = cSuccess | (LineTuning(cReadoutChip, cLineId, cAlignmentPattern, 8) << (cLineId - 1));
                cLineId++;
                // finally sync bit + last bend
                cAlignmentPattern = (1 << 7) | cBendCode_phAlign;
                bool cTuned       = LineTuning(cReadoutChip, cLineId, cAlignmentPattern, 8);
                if(!cTuned)
                {
                    LOG(INFO) << BOLDMAGENTA << "Checking if error bit is set ..." << RESET;
                    // check if error bit is set
                    cAlignmentPattern = (1 << 7) | (1 << 6) | cBendCode_phAlign;
                    cSuccess          = cSuccess | (LineTuning(cReadoutChip, cLineId, cAlignmentPattern, 8) << (cLineId - 1));
                }
                else
                    cSuccess = cSuccess | (static_cast<uint8_t>(cTuned) << (cLineId - 1));

                cAligned = (cAligned && cSuccess == 0x1F);
                LOG(INFO) << BOLDBLUE << "Success register for this chip is " << std::bitset<8>(cSuccess) << RESET;
                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>(cSeeds[0]) << ", " << std::bitset<8>(cSeeds[1]) << ", " << std::bitset<8>(cSeeds[2]) << " on stub lines  0, 1 and 2."
                          << RESET;
                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>((cBendCode_phAlign << 4) | cBendCode_phAlign) << " on stub line  4." << RESET;
                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>((1 << 7) | cBendCode_phAlign) << " on stub line  5." << RESET;
                LOG(INFO) << BOLDMAGENTA << "After alignment of last stub line ... stub lines 0-5: " << RESET;
                (static_cast<D19cDebugFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, 5);

                // now unmask all channels and set threshold and hit or logic back to their original values
                fReadoutChipInterface->maskChannelGroup(theReadoutChip, cOriginalMask);
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
        bool cWithCIC          = false;
        bool cWithCBC          = false;
        bool cWithSSA          = false;
        bool cWithSSA2         = false;
        bool cWithMPA          = false;
        for(auto cOpticalReadout: *cBoard)
        {
            if(cOpticalReadout->getIndex() > 0) break;
            for(auto cHybrid: *cOpticalReadout)
            {
                if(cHybrid->getIndex() > 0) break;
                cWithCIC = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic != NULL;
                for(auto cReadoutChip: *cHybrid)
                {
                    cWithCBC  = cWithCBC || cReadoutChip->getFrontEndType() == FrontEndType::CBC3;
                    cWithSSA  = cWithSSA || cReadoutChip->getFrontEndType() == FrontEndType::SSA;
                    cWithSSA2 = cWithSSA2 || cReadoutChip->getFrontEndType() == FrontEndType::SSA2;
                    cWithMPA  = cWithMPA || cReadoutChip->getFrontEndType() == FrontEndType::MPA;
                } // ROcs
            }     // Hybrids
        }         // OGs
        if(cWithCIC) { cAligned = this->CICAlignment(theBoard); }
        else if(cWithCBC)
        {
            cAligned = this->CBCAlignment(theBoard);
        }
        else if(cWithMPA || cWithSSA)
            cAligned = this->PSAlignment(theBoard, fPairSelect);

        // check alignment
        if(cAligned)
            LOG(INFO) << BOLDGREEN << "Back-end alignment worked..." << RESET;
        else
            LOG(INFO) << BOLDRED << "Back-end alignment FAILED..." << RESET;
        // re-load configuration of fast command block from register map loaded from xml file
        LOG(INFO) << BOLDBLUE << "Re-loading original coonfiguration of fast command block from hardware description file [.xml] " << RESET;
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFastCommandBlock(theBoard);
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
    Reset();
}

void BackEndAlignment::Stop()
{
    dumpConfigFiles();
    // Destroy();
}

void BackEndAlignment::Pause() {}

void BackEndAlignment::Resume() {}
