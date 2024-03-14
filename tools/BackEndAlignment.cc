#include "tools/BackEndAlignment.h"

#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "Utils/CBCChannelGroupHandler.h"
#include "Utils/ContainerFactory.h"
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
    fPairSelect = (uint8_t)(findValueInSettings<double>("EnablePairSelect", 0));

    // retreive original settings for all chips and all back-end boards
    ContainerFactory::copyAndInitHybrid<uint8_t>(*fDetectorContainer, fEnabledFEs);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cEnabledFEs = fEnabledFEs.getObject(cBoard->getId());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cEnabledFEsOG = cEnabledFEs->getObject(cOpticalGroup->getId());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cEnabledFEsHybrid = cEnabledFEsOG->getObject(cHybrid->getId());
                auto& cEnabled          = cEnabledFEsHybrid->getSummary<uint8_t>();
                cEnabled                = 0;
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::MPA2 || cChip->getFrontEndType() == FrontEndType::CBC3)
                        cEnabled = cEnabled | (1 << cChip->getId());
                }
            }
        }
    }
}

void BackEndAlignment::SetEnabledChips(std::string pSSAPair)
{
    fPairName = pSSAPair;
    fEnabledChips.clear();
    for(uint8_t cId = 0; cId < 8; cId++)
    {
        if(cId != (int)(fPairName[0] - '0') && cId != (int)(fPairName[1] - '0')) continue;
        LOG(INFO) << BOLDYELLOW << "Enabling Chip#" << +cId << RESET;
        fEnabledChips.push_back(cId);
    }
}
bool BackEndAlignment::PSAlignment(BeBoard* pBoard)
{
    bool cTuned = true;
    LOG(INFO) << GREEN << "BackEndAlignment for PS Chip(s)" << RESET;
    fBeBoardInterface->setBoard(pBoard->getId());
    auto                  cInterface             = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    D19cDebugFWInterface* cDebugInterface        = cInterface->getDebugInterface();
    uint8_t               cPhaseAlignmentPattern = 0xAA;
    uint8_t               cWordAlignmentPattern  = 0xEA;
    auto                  cFrontEndTypes         = pBoard->connectedFrontEndTypes();

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                fReadoutChipInterface->WriteChipReg(cChip, "EnableSLVSTestOutput", 0x1);
                for(uint8_t cLineId = 0; cLineId < 8; cLineId++) // stub lines - 1 to 8
                {
                    std::stringstream cRegName;
                    cRegName << "OutPatternStubLine" << +(cLineId);
                    fReadoutChipInterface->WriteChipReg(cChip, cRegName.str(), cPhaseAlignmentPattern);
                }
                fReadoutChipInterface->WriteChipReg(cChip, "OutPatternL1Line", cPhaseAlignmentPattern);
            }
        }
    } // configure PA pattern on all SLVS lines

    uint8_t cFirstLine = (std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA2) != cFrontEndTypes.end())
                             ? 1
                             : 0;
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            uint16_t chipCounter = 0;
            for(auto cChip: *cHybrid)
            {
                if(fEnabledChips.size() != cHybrid->size() && chipCounter > 1)
                {
                    LOG(INFO) << BOLDYELLOW << "Skipping Phase tuning on Chip#" << +cChip->getId() << RESET;
                    continue;
                }
                for(uint8_t cLineId = cFirstLine; cLineId <= 8; cLineId++) // stub lines - 1 to 8
                {
                    PhaseTuneLine(cChip, cLineId);
                }

                ++chipCounter;
            }
        }
    } // run phase aligner on all lines

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                fReadoutChipInterface->WriteChipReg(cChip, "EnableSLVSTestOutput", 0x1);
                for(uint8_t cLineId = 0; cLineId < 8; cLineId++) // stub lines - 1 to 8
                {
                    std::stringstream cRegName;
                    cRegName << "OutPatternStubLine" << +(cLineId);
                    fReadoutChipInterface->WriteChipReg(cChip, cRegName.str(), cWordAlignmentPattern);
                }
                fReadoutChipInterface->WriteChipReg(cChip, "OutPatternL1Line", cWordAlignmentPattern);
            }
        }
    } // configure WA pattern on all SLVS lines

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            uint16_t chipCounter = 0;
            for(auto cChip: *cHybrid)
            {
                if(fEnabledChips.size() != cHybrid->size() && chipCounter > 1)
                {
                    LOG(INFO) << BOLDYELLOW << "Skipping word alignment on Chip#" << +cChip->getId() << RESET;
                    continue;
                }
                for(uint8_t cLineId = cFirstLine; cLineId <= 8; cLineId++) // stub lines - 1 to 8
                {
                    WordAlignLine(cChip, cLineId, cWordAlignmentPattern, 8);
                }

                // replace this with something that gets the value
                // from one of the stub lines
                // ManuallyConfigureLine(cChip,0, 15,0);
                ++chipCounter;
            }
        }
    } // run word aligner on all lines

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                fReadoutChipInterface->WriteChipReg(cChip, "EnableSLVSTestOutput", 0x0);
                for(uint8_t cLineId = 0; cLineId < 8; cLineId++) // stub lines - 1 to 8
                {
                    std::stringstream cRegName;
                    cRegName << "OutPatternStubLine" << +(cLineId);
                    fReadoutChipInterface->WriteChipReg(cChip, cRegName.str(), 0x00);
                }
                fReadoutChipInterface->WriteChipReg(cChip, "OutPatternL1Line", 0x00);
            }
        }
    } // disable SLVS output on all chips - this makes sure we now have L1 data back on L1 line

    // for(auto cOpticalReadout: *pBoard)
    // {
    //     for(auto cHybrid: *cOpticalReadout)
    //     {
    //         for(auto cChip: *cHybrid)
    //         {
    //             if(fEnabledChips.size() != cHybrid->size() && cChip->getId() <= 1)
    //             {
    //                 fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
    //                 fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cChip->getId());
    //                 cDebugInterface->L1ADebug();
    //             }
    //         }
    //     }
    // } // check that L1 data is there

    if(cTuned)
    {
        LOG(INFO) << BOLDGREEN << "PS Phase+Word Alignment succesful" << RESET;
        uint16_t cTriggerSrc      = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
        uint16_t cOriginalTPdelay = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
        LOG(INFO) << BOLDYELLOW << "Trigger source : " << +cTriggerSrc << "\t TP delay " << +cOriginalTPdelay << RESET;

        // just checking digital injection
        for(auto cOpticalReadout: *pBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                for(auto cChip: *cHybrid)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "DigitalSync", 0x00);
                    fReadoutChipInterface->WriteChipReg(cChip, "EdgeSel", 0);
                    fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_H", 0x1);
                    for(int cStrip = 0; cStrip < 20; cStrip += 2)
                    {
                        std::stringstream cRegName;
                        cRegName << "DigitalSync_S" << cStrip;
                        fReadoutChipInterface->WriteChipReg(cChip, cRegName.str(), 0x1);
                    }
                }
            }
        } // enable digital sync on all strips
        for(int cLatencyOffset = -2; cLatencyOffset <= -2; cLatencyOffset++)
        {
            LOG(INFO) << BOLDYELLOW << "Latency will be set to " << (cOriginalTPdelay + cLatencyOffset) << RESET;
            for(auto cOpticalReadout: *pBoard)
            {
                for(auto cHybrid: *cOpticalReadout)
                {
                    for(auto cChip: *cHybrid)
                    {
                        // fReadoutChipInterface->WriteChipReg(cChip,"DigitalSync", 0x01);
                        fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cOriginalTPdelay + cLatencyOffset);
                    }
                }
            } // enable digital sync on all strips

            for(auto cOpticalReadout: *pBoard)
            {
                for(auto cHybrid: *cOpticalReadout)
                {
                    for(uint8_t cChipId = 0; cChipId < 2; cChipId++)
                    {
                        LOG(INFO) << BOLDYELLOW << "Chip#" << +cChipId << RESET;
                        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.auto_l1_capture", 1);
                        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
                        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cChipId);
                        cDebugInterface->L1ADebug();
                    }
                }
            }
        }
    }
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
                auto cDelay   = getBeSamplingDelay(pBoard->getId(), cOpticalGroup->getId(), cHybrid->getId(), cLineId);
                auto cBitslip = getBeBitSlip(pBoard->getId(), cOpticalGroup->getId(), cHybrid->getId(), cLineId);
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
    fBeBoardInterface->setBoard(pBoard->getId());
    auto                  cInterface      = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    D19cDebugFWInterface* cDebugInterface = cInterface->getDebugInterface();

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cReadoutChip: *cHybrid)
            {
                ReadoutChip* theReadoutChip = static_cast<ReadoutChip*>(cReadoutChip);
                fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cReadoutChip->getId());
                // original mask
                auto cOriginalMask = std::static_pointer_cast<ChannelGroup<1, NCHANNELS>>(cReadoutChip->getChipOriginalMask());
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

                uint8_t                              cSuccess     = 0x00;
                int                                  theBendValue = static_cast<int>(cBend_strips * 2);
                std::vector<std::pair<uint8_t, int>> cSeedAndBendList{{0x82, theBendValue}, {0x8E, theBendValue}, {0x9E, theBendValue}};
                static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theReadoutChip, cSeedAndBendList);
                // first align lines with stub seeds
                uint8_t cLineId = 1;
                for(size_t cIndex = 0; cIndex < 3; cIndex++)
                {
                    cSuccess = cSuccess | (LineTuning(cReadoutChip, cLineId, cSeedAndBendList[cIndex].first, 8) << cIndex);
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
                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>(cSeedAndBendList[0].first) << ", " << std::bitset<8>(cSeedAndBendList[1].first) << ", "
                          << std::bitset<8>(cSeedAndBendList[2].first) << " on stub lines  0, 1 and 2." << RESET;
                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>((cBendCode_phAlign << 4) | cBendCode_phAlign) << " on stub line  4." << RESET;
                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>((1 << 7) | cBendCode_phAlign) << " on stub line  5." << RESET;
                LOG(INFO) << BOLDMAGENTA << "After alignment of last stub line ... stub lines 0-5: " << RESET;
                cDebugInterface->StubDebug(true, 5);

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
        bool cWithCIC  = false;
        bool cWithCBC  = false;
        bool cWithSSA2 = false;
        bool cWithMPA2 = false;

        auto cHybrid = cBoard->getFirstObject()->getFirstObject();
        cWithCIC     = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic != NULL;
        for(auto cReadoutChip: *cHybrid)
        {
            cWithCBC  = cWithCBC || cReadoutChip->getFrontEndType() == FrontEndType::CBC3;
            cWithSSA2 = cWithSSA2 || cReadoutChip->getFrontEndType() == FrontEndType::SSA2;
            cWithMPA2 = cWithMPA2 || cReadoutChip->getFrontEndType() == FrontEndType::MPA2;
        } // ROcs
        if(cWithCIC) { cAligned = this->CICAlignment(theBoard); }
        else if(cWithCBC) { cAligned = this->CBCAlignment(theBoard); }
        else if(cWithMPA2 || cWithSSA2)
            cAligned = this->PSAlignment(theBoard);

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
