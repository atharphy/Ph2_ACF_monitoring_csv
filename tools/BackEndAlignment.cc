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
            auto& cRegMapThisModule = cRegMapThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cRegMapThisHybrid = cRegMapThisModule->at(cHybrid->getIndex());
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

bool BackEndAlignment::MPAAlignment(BeBoard* pBoard)
{
    bool cTuned = true;
    LOG(INFO) << GREEN << "Trying Phase Tuning for MPA Chip(s)" << RESET;

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                ReadoutChip*             cReadoutChip = static_cast<ReadoutChip*>(cChip);
                std::vector<std::string> cRegNames{"ReadoutMode", "ECM", "LFSR_data"};
                std::vector<uint8_t>     cOriginalValues;
                uint8_t                  cAlignmentPattern = 0xa0;
                std::vector<uint8_t>     cRegValues{0x0, 0x08, cAlignmentPattern};
                for(size_t cIndex = 0; cIndex < 3; cIndex++)
                {
                    cOriginalValues.push_back(fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegNames[cIndex]));
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cRegValues[cIndex]);
                }

                for(uint8_t cLineId = 0; cLineId < 6; cLineId++)
                { cTuned = cTuned && static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getId(), cChip->getId(), cLineId, cAlignmentPattern, 8); }

                for(size_t cIndex = 0; cIndex < 3; cIndex++) { fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cOriginalValues[cIndex]); };
            }
        }
    }

    LOG(INFO) << GREEN << "MPA Phase tuning finished succesfully" << RESET;
    return cTuned;
}

bool BackEndAlignment::SSAAlignment(BeBoard* pBoard)
{
    LOG(INFO) << GREEN << "Trying Phase Tuning for SSA Chip(s)" << RESET;
    // configure patterns
    bool                  cTuned = true;
    std::vector<uint32_t> cVec(0);
    std::vector<uint32_t> cReplies(0); // might be superceding cVecReq?
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                ReadoutChip* cReadoutChip = static_cast<ReadoutChip*>(cChip);
                // configure SLVS drive strength and readout mode
                std::vector<std::string> cRegNames{"SLVS_pad_current", "ReadoutMode"};
                std::vector<uint8_t>     cOriginalValues;
                std::vector<uint8_t>     cRegValues{0x7, 2};
                for(size_t cIndex = 0; cIndex < 2; cIndex++)
                {
                    // auto cRegItem = static_cast<ChipRegItem>(cReadoutChip->getRegItem ( cRegNames[cIndex] ));
                    cOriginalValues.push_back(fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegNames[cIndex]));
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cRegValues[cIndex]);
                } // all that did was set our pad current to max and our readout mode to transmit known patterns

                // configure output pattern on sutb lines
                uint8_t cAlignmentPattern = 0x80;
                for(uint8_t cLineId = 0; cLineId < 6; cLineId++)
                {
                    char cBuffer[11];
                    sprintf(cBuffer, "OutPattern%d", cLineId - 1);
                    std::string cRegName = (cLineId == 8) ? "OutPattern7/FIFOconfig" : std::string(cBuffer, sizeof(cBuffer));
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cAlignmentPattern);
                    cTuned = cTuned && static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getId(), cChip->getId(), cLineId, cAlignmentPattern, 8);
                }

                // self.I2C.strip_write("ENFLAGS", 0, 0b01001)
                // self.I2C.strip_write("DigCalibPattern_L", 0, 0)
                // self.I2C.strip_write("DigCalibPattern_H", 0, 0)
                // self.I2C.peri_write( "CalPulse_duration", 15)
                // self.I2C.strip_write("ENFLAGS",   7, 0b01001)
                // self.I2C.strip_write("ENFLAGS", 120, 0b01001)
                // self.I2C.strip_write("DigCalibPattern_L",   7, 0xff)
                // self.I2C.strip_write("DigCalibPattern_L", 120, 0xff)
                // fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern7/FIFOconfig",7)
                // fc7.write("ctrl_phy_phase_tune_again", 1)

                // back to original modes
                for(size_t cIndex = 0; cIndex < 2; cIndex++) { fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cOriginalValues[cIndex]); }
            }
        }
    }
    LOG(INFO) << GREEN << "SSA Phase tuning finished succesfully" << RESET;
    return cTuned;
}
bool BackEndAlignment::CICAlignment(BeBoard* pBoard)
{
    // make sure you're only sending one trigger at a time here
    bool cSparsified          = (fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable") == 1);
    auto cTriggerMultiplicity = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0);

    // force CIC to output repeating 101010 pattern [by disabling all FEs]
    for(auto cModule: *pBoard)
    {
        for(auto cHybrid: *cModule)
        {
            auto& cCic = static_cast<OuterTrackerModule*>(cHybrid)->fCic;
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
            auto& cCic = static_cast<OuterTrackerModule*>(cHybrid)->fCic;
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
            auto& cCic = static_cast<OuterTrackerModule*>(cHybrid)->fCic;
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
            auto& cCic = static_cast<OuterTrackerModule*>(cHybrid)->fCic;
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
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<Module*>(cHybrid)->getLinkId());
            for(auto cReadoutChip: *cHybrid)
            {
                ReadoutChip* theReadoutChip = static_cast<ReadoutChip*>(cReadoutChip);
                // fBeBoardInterface->WriteBoardReg (pBoard,
                // "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cReadoutChip->getChipId() );
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

        OuterTrackerModule* cFirstHybrid = static_cast<OuterTrackerModule*>(cBoard->at(0)->at(0));
        bool                cWithCIC     = cFirstHybrid->fCic != NULL;
        if(cWithCIC)
            cAligned = this->CICAlignment(theBoard);
        else
        {
            ReadoutChip* theFirstReadoutChip = static_cast<ReadoutChip*>(cBoard->at(0)->at(0)->at(0));
            bool         cWithCBC            = (theFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
            bool         cWithSSA            = (theFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
            bool         cWithMPA            = (theFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);
            if(cWithCBC) { this->CBCAlignment(theBoard); }
            else if(cWithSSA)
            {
                this->SSAAlignment(theBoard);
            }
            else if(cWithMPA)
            {
                this->MPAAlignment(theBoard);
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
void BackEndAlignment::Start(int currentRun)
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

    Destroy();
}

void BackEndAlignment::Pause() {}

void BackEndAlignment::Resume() {}
