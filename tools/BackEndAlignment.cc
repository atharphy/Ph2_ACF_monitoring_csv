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
        for(auto cReg: cBeRegMap) cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second));
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

    // pair select for PS-FEHs
    fPairSelect         = (uint8_t)findValueInSettings<double>("EnablePairSelect", 0);
    
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

bool BackEndAlignment::PSAlignment(BeBoard* pBoard, uint8_t pSSAPair)
{
    bool cTuned = true;
    LOG(INFO) << GREEN << "BackEndAlignment for PS Chip(s)" << RESET;
    fBeBoardInterface->setBoard(pBoard->getId());
    auto                        cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    D19cFWInterface::PhaseTuner cTuner;
    uint8_t                     cPhaseAlignmentPattern = 0xAA;
    uint8_t                     cWordAlignmentPattern  = 0xEA;
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                ReadoutChip* cReadoutChip = static_cast<ReadoutChip*>(cChip);
                if(cChip->getFrontEndType() == FrontEndType::SSA2 || cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    auto cDriveStrength = fReadoutChipInterface->ReadChipReg(cChip, "SLVS_pad_current_L1");
                    LOG(INFO) << BOLDBLUE << "SSA[#" << +cChip->getId() << " Alignment for L1 and stub lines.. L1 drive set to " << +cDriveStrength << RESET;

                    std::vector<uint8_t> cAlVals(8, 0);
                    uint8_t              cPairId = (cChip->getId() % 2 == 0) ? 1 : 0;
                    uint8_t              cChipId = (pSSAPair) ? cPairId : cChip->getId();
                    cPairId                      = (pSSAPair) ? cPairId : 0;

                    // select SSA pair
                    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.multiplexing_bp.ssa_pair_select", 0x4);
                    cWordAlignmentPattern = (cPairId == 0) ? 0xCA : 0xF0;
                    if(pSSAPair)
                        LOG(INFO) << BOLDBLUE << "Backend alignment for SSA pair#" << +cPairId << " [ChipId in BE is  " << +cChipId << " ]" << RESET;
                    else
                        LOG(INFO) << BOLDBLUE << "Backend alignment for SSA#" << +cChipId << RESET;
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "EnableSLVSTestOutput", 0x1);
                    std::vector<uint8_t> cPhaseTaps(0);
                    for(uint8_t cLineId = 1; cLineId <= 8; cLineId++) // stub lines - 1 to 8
                    {
                        std::stringstream cRegName;
                        cRegName << "OutPatternStubLine" << +(cLineId - 1);
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), cPhaseAlignmentPattern);
                        cTuner.TunePhase(cInterface, cChip->getHybridId(), cChipId, cLineId);
                        cPhaseTaps.push_back(cTuner.fDelay);
                        // if( cReadoutChip->getFrontEndType() == FrontEndType::SSA ) continue;
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), cWordAlignmentPattern);
                        cTuner.AlignWord(cInterface, cChip->getHybridId(), cChipId, cLineId, cWordAlignmentPattern, 8, true);
                        cAlVals[cLineId] = cTuner.GetLineStatus(cInterface, cHybrid->getHybridId(), cChipId, cLineId);
                        LOG(DEBUG) << +cAlVals[cLineId] << RESET;
                    }
                    for(uint8_t cLineId = 0; cLineId < 1; cLineId++) // L1 line - line 0
                    {
                        if(cReadoutChip->getFrontEndType() == FrontEndType::SSA)
                        {
                            cAlVals[cLineId] = 1;
                            uint8_t cMode    = 2;
                            cTuner.SetLineMode(cInterface, cHybrid->getHybridId(), cChipId, cLineId, cMode, cPhaseTaps[cPhaseTaps.size() - 1], 0, 0, 0);
                            continue;
                        }
                        std::stringstream cRegName;
                        cRegName << "OutPatternL1Line";
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), cPhaseAlignmentPattern);
                        cTuner.TunePhase(cInterface, cChip->getHybridId(), cChipId, cLineId);
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), cWordAlignmentPattern);
                        cTuner.AlignWord(cInterface, cChip->getHybridId(), cChipId, cLineId, cWordAlignmentPattern, 8, true);
                        cAlVals[cLineId] = cTuner.GetLineStatus(cInterface, cHybrid->getHybridId(), cChipId, cLineId);
                        LOG(DEBUG) << +cAlVals[cLineId] << RESET;
                    }

                    // back to readout mode 0 to look at the L1 data
                    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
                    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cChipId);
                    cInterface->StubDebug(true, 8);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "EnableSLVSTestOutput", 0x0, false);
                    cInterface->L1ADebug();

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
                    cTuned = cTuned && (std::accumulate(cAlVals.begin(), cAlVals.end(), 0) == 8);
                }
                else if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    LOG(INFO) << GREEN << "SSA Alignment" << RESET;
                    ReadoutChip*             cReadoutChip = static_cast<ReadoutChip*>(cChip);
                    std::vector<std::string> cRegNames{"SLVS_pad_current", "ReadoutMode"};
                    std::vector<uint8_t>     cOriginalValues;
                    std::vector<uint8_t>     cRegValues{0x2, 2};
                    for(size_t cIndex = 0; cIndex < 2; cIndex++) { fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cRegValues[cIndex]); }

                    uint8_t cPairId                = (cChip->getId() % 2 == 0) ? 1 : 0;
                    uint8_t cPhaseAlignmentPattern = 0xAA;
                    // uint8_t                     cWordAlignmentPattern  = 0xEA;
                    for(uint8_t cLineId = 1; cLineId <= 8; cLineId++)
                    {
                        std::stringstream cRegName;
                        if(cLineId < 8)
                            cRegName << "OutPattern" << +(cLineId - 1);
                        else
                            cRegName << "OutPattern" << +(cLineId - 1) << "/FIFOconfig";
                        LOG(INFO) << BOLDBLUE << cRegName.str() << RESET;
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), cPhaseAlignmentPattern);
                        cTuner.TunePhase(cInterface, cChip->getHybridId(), cPairId, cLineId);
                        uint8_t cLineTuned = cTuner.GetLineStatus(cInterface, cHybrid->getHybridId(), cPairId, cLineId);
                        if(cLineTuned == 1 && cLineId == 1)
                        {
                            uint8_t cMode = 2;
                            cTuner.SetLineMode(cInterface, cHybrid->getHybridId(), cPairId, 0, cMode, cTuner.fDelay, 0, 0, 0);
                        }
                        // why can't I word align?
                        // fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName.str(), cWordAlignmentPattern);
                        // cTuner.AlignWord(cInterface, cChip->getHybridId(), cChip->getId(), cLineId, cWordAlignmentPattern, 8, true);
                        // cTuner.GetLineStatus(cInterface, cHybrid->getHybridId(), cChip->getId(), cLineId);
                    }
                    cInterface->StubDebug(true, 8);
                    cTuned = true;
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "ReadoutMode", 0x0, false);
                    cInterface->L1ADebug();

                    // fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source",10);
                    // fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);
                    // fReadoutChipInterface->WriteChipReg(cReadoutChip, "Threshold", 0, false);
                    // fReadoutChipInterface->WriteChipReg(cReadoutChip, "InjectedCharge", 40, false);
                    // fReadoutChipInterface->WriteChipReg(cReadoutChip, "AnalogueAsync", 0x1, false);
                    // fReadoutChipInterface->WriteChipReg(cReadoutChip, "AsyncDelay",8, false);
                    // cInterface->SetPSCounterDelay(8+21);
                    // cInterface->ReadSSACountersFast(pBoard, cChip->getId() , 1);

                    // cTuned = cTuned && static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getId(), cChip->getId(), cLineId, cAlignmentPattern,
                    // 8); for(size_t cIndex = 0; cIndex < 2; cIndex++) { fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cOriginalValues[cIndex]); }
                }
                else if(cChip->getFrontEndType() == FrontEndType::MPA)
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
                        cTuned =
                            cTuned && static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getId(), cChip->getId(), cLineId, cAlignmentPattern, 8);
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

bool BackEndAlignment::CICAlignment(BeBoard* pBoard)
{
    // make sure you're only sending one trigger at a time here
    bool cSparsified          = (fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable") == 1);
    auto cTriggerMultiplicity = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0);

    // force CIC to output repeating 101010 pattern [by disabling all FEs]
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            // only produce L1A header .. so disable all FEs .. for CIC2 only
            if(!cSparsified && cCic->getFrontEndType() == FrontEndType::CIC2) fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", 1);

            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
            if(cCic->getFrontEndType() == FrontEndType::CIC)
            {
                // to get a 1010 pattern on the L1 line .. have to do something
                fCicInterface->SelectOutput(cCic, true);
            }
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
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SelectOutput(cCic, false);
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        }
    }
    cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->L1WordAlignment(pBoard, fL1Debug);
    if(!cAligned)
    {
        LOG(INFO) << BOLDBLUE << "L1A word alignment in the back-end " << BOLDRED << " FAILED ..." << RESET;
        return false;
    }

    // enable CIC output of pattern .. and enable all FEs again
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, true);
            fCicInterface->SelectOutput(cCic, true);
        }
    }
    cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->StubTuning(pBoard, true);

    // disable CIC output of pattern
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SelectOutput(cCic, false);
            if(!cSparsified && cCic->getFrontEndType() == FrontEndType::CIC2) fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", 0);
        }
    }

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
            cAligned = this->CICAlignment(theBoard);
        else
        {
            ReadoutChip* theFirstReadoutChip = static_cast<ReadoutChip*>(cBoard->at(0)->at(0)->at(0));
            bool         cWithCBC            = (theFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
            bool         cWithSSA            = (theFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
            bool         cWithSSA2           = (theFirstReadoutChip->getFrontEndType() == FrontEndType::SSA2);
            bool         cWithMPA            = (theFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);
            if(cWithCBC) { this->CBCAlignment(theBoard); }
            else if(cWithMPA or cWithSSA or cWithSSA2)
            {
                this->PSAlignment(theBoard, fPairSelect);
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
