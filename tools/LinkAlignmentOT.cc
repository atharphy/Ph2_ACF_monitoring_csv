#include "LinkAlignmentOT.h"

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
//#include "boost/format.hpp"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

LinkAlignmentOT::LinkAlignmentOT() : OTTool() {}
LinkAlignmentOT::~LinkAlignmentOT() {}

// Processing
void LinkAlignmentOT::AlignStubPackage()
{
    for(auto cBoard: *fDetectorContainer) { AlignStubPackage(cBoard); } // align stubs
}
bool LinkAlignmentOT::Align()
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            AlignLpGBTInputs(cOpticalGroup);
            try
            {
                WordAlignBEdata(cOpticalGroup);
            }
            catch(const std::exception& e)
            {
                fSuccess = false;
                LOG(INFO) << BOLDRED << "BE word alignment failed" << RESET;
                throw std::runtime_error(std::string("Could not word align BE data in LinkAlignmentOT..."));
            }
        }
    } // align BE

    AlignStubPackage();
    fSuccess = true;
    return fSuccess;
}

// Initialization function
void LinkAlignmentOT::Initialise()
{
    // prepare common OTTool
    Prepare();
    SetName("LinkAlignmentOT");

    // list of board registers that can be modified by this tool
    std::vector<std::string> cBrdRegsToKeep{"fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay"};
    SetBrdRegstoPerserve(cBrdRegsToKeep);

    // no ROC registers to perserve

    // initialize containers that hold values found by this tool
    ContainerFactory::copyAndInitHybrid<std::vector<uint8_t>>(*fDetectorContainer, fBeSamplingDelay);
    ContainerFactory::copyAndInitHybrid<std::vector<uint8_t>>(*fDetectorContainer, fBeBitSlip);
    ContainerFactory::copyAndInitHybrid<uint8_t>(*fDetectorContainer, fLpGBTSamplingDelay);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cBeSamplingDelay = fBeSamplingDelay.at(cBoard->getIndex());
        auto& cBeBitSlip       = fBeBitSlip.at(cBoard->getIndex());
        auto& cLinkSampling       = fLpGBTSamplingDelay.at(cBoard->getIndex());

        for(auto cOpticalGroup: *cBoard)
        {
            auto&  cBeSamplingDelayOG = cBeSamplingDelay->at(cOpticalGroup->getIndex());
            auto&  cBeBitSlipOG       = cBeBitSlip->at(cOpticalGroup->getIndex());
            auto&  cLinkDelayOG       = cLinkSampling->at(cOpticalGroup->getIndex());
            size_t cNlines            = (cOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cBeSamplingDelayHybrd = cBeSamplingDelayOG->at(cHybrid->getIndex());
                auto& cBeBitSlipHybrd       = cBeBitSlipOG->at(cHybrid->getIndex());
                auto& cThisBeSamplingDelay  = cBeSamplingDelayHybrd->getSummary<std::vector<uint8_t>>();
                auto& cThisBeBitSlip        = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();
                auto&  cLinkDelay           = cLinkDelayOG->at(cHybrid->getIndex())->getSummary<uint8_t>(); 
                cLinkDelay=0;
                for(size_t cLineId = 0; cLineId < cNlines; cLineId++)
                {
                    cThisBeSamplingDelay.push_back(0);
                    cThisBeBitSlip.push_back(0);
                }
            }
        }
    }
}

// Align lpGBT inputs
bool LinkAlignmentOT::AlignLpGBTInputs(const OpticalGroup* pOpticalGroup)
{
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    // stop triggers to make sure that there are no L1 packets from the CIC
    fBeBoardInterface->Stop((*cBoardIter));

    LOG(INFO) << BOLDMAGENTA << "Aligning CIC-lpGBT data on OpticalGroup#" << +pOpticalGroup->getId() << RESET;
    auto& clpGBT = pOpticalGroup->flpGBT;
    if(clpGBT == nullptr) return true;

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
    bool                 cAligned = true;
    std::vector<uint8_t> cEportGroups;
    std::vector<uint8_t> cEportChnls;
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
        for(auto cGrp: cGroups) cEportGroups.push_back(cGrp);
        for(auto cChnl: cChannels) cEportChnls.push_back(cChnl);
    }
    auto cMode = flpGBTInterface->AutoPhaseAlignRx(clpGBT, cEportGroups, cEportChnls);
    cAligned   = cAligned && (cMode != 15);
    //cMode      = ( cMode > 8 ) ? 5 : cMode; 
    for(size_t cIndx = 0; cIndx < cEportGroups.size(); cIndx++) { flpGBTInterface->ConfigureRxPhase(clpGBT, cEportGroups[cIndx], cEportChnls[cIndx], cMode); }

    // configure CICs to NOT output alignment pattern on stub lines
    size_t cIndx = 0;
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, false);
        fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
        cIndx++;

        auto& cLinkSampling       = fLpGBTSamplingDelay.at((*cBoardIter)->getIndex())->at(pOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<uint8_t>();
        cLinkSampling = cMode;
    }
    return cAligned;
}

// Word align L1 + stub data in the backend
bool LinkAlignmentOT::WordAlignBEdata(const BeBoard* pBoard)
{
    bool cAligned = true;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            cAligned = WordAlignBEdata(cOpticalGroup);
            if(!cAligned)
            {
                LOG(INFO) << BOLDRED << "Could not word align-BE data for BeBoard#" << +cBoard->getId() << " Link#" << +cOpticalGroup->getId() << RESET;
                throw std::runtime_error(std::string("Could not word align-BE data in LinkAlignmentOT..."));
                return cAligned;
            }
        }
    }
    return cAligned;
}
bool LinkAlignmentOT::WordAlignBEdata(const OpticalGroup* pOpticalGroup)
{
    fStubDebug      = true;
    bool cAligned   = false;
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    auto& cBeBitSlip   = fBeBitSlip.at((*cBoardIter)->getIndex());
    auto& cBeBitSlipOG = cBeBitSlip->at(pOpticalGroup->getIndex());

    // configure CICs to output alignment pattern on L1 lines
    std::vector<uint8_t> cFeEnableRegs(0);
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, true);
        cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
    }
    // stop triggers to make sure that there are no L1 packets from the CIC
    fBeBoardInterface->Stop((*cBoardIter));

    // align stub lines in the BE
    size_t cNlines = (pOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::WordAlignBEdata ... word alignment on " << +cNlines << "/6 lines stub from CIC.." << RESET;
    D19cFWInterface::PhaseTuner cTuner;
    for(size_t cLineId = 1; cLineId <= cNlines; cLineId++)
    {
        for(auto cHybrid: *pOpticalGroup)
        {
            auto& cBeBitSlipHybrd = cBeBitSlipOG->at(cHybrid->getIndex());
            auto& cThisBeBitSlip  = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();

            LOG(INFO) << BOLDMAGENTA << "Aligning Stub line#" << +cLineId << " on Hybrid#" << +cHybrid->getId() << RESET;
            cTuner.AlignWord(cInterface, cHybrid->getId(), 0, cLineId, 0xEA, 8, true);
            cTuner.GetLineStatus(cInterface, cHybrid->getId(), 0, cLineId);
            cAligned                = (cTuner.fWordAlignmentFSMstate == 14);
            cThisBeBitSlip[cLineId] = cTuner.fBitslip;
            if(!cAligned)
            {
                LOG(INFO) << BOLDRED << "Could not word align-BE data for BeBoard#" << +cBoardId << " Link#" << +pOpticalGroup->getId() << " stub line " << +(cLineId - 1) << RESET;
                throw std::runtime_error(std::string("Could not word align-BE data in LinkAlignmentOT..."));
            }
            // check if I allow a bit-slip of 0
            if(cTuner.fBitslip == 0)
            {
                size_t cMaxAttempts = 10;
                size_t cIter        = 0;
                do
                {
                    cTuner.AlignWord(cInterface, cHybrid->getId(), 0, cLineId, 0xEA, 8, true);
                    cTuner.GetLineStatus(cInterface, cHybrid->getId(), 0, cLineId);
                    cAligned                = (cTuner.fWordAlignmentFSMstate == 14);
                    cThisBeBitSlip[cLineId] = cTuner.fBitslip;
                    cIter++;
                } while(cIter < cMaxAttempts && cTuner.fBitslip == 0);
                if(cTuner.fBitslip == 0)
                {
                    LOG(INFO) << BOLDRED << "Bitslip of 0 found for BE-stub data for BeBoard#" << +cBoardId << " Link#" << +pOpticalGroup->getId() << " stub line " << +(cLineId - 1) << RESET;
                    throw std::runtime_error(std::string("Bitslip of 0 for word-aligned BE data in LinkAlignmentOT..."));
                }
            }
        }
    }
    // check for 0 bit slips
    for(auto cHybrid: *pOpticalGroup)
    {
        auto&                cBeBitSlipHybrd = cBeBitSlipOG->at(cHybrid->getIndex());
        auto&                cThisBeBitSlip  = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();
        std::vector<uint8_t> cBitSlipHist(15, 0);
        for(auto cItem: cThisBeBitSlip) cBitSlipHist[cItem]++;
        auto cMode = std::max_element(cBitSlipHist.begin(), cBitSlipHist.end()) - cBitSlipHist.begin();
        LOG(INFO) << BOLDMAGENTA << "Hybrid#" << +cHybrid->getId() << " most frequent bitslip is " << +cMode << RESET;
        // now if any line has a bit-slip that isn't the mode.. set it to the mode
        // for( size_t cLineId =1 ; cLineId <= cNlines; cLineId++)
        // {
        //     if(cThisBeBitSlip[cLineId] != cMode ){
        //         fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
        //         fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
        //         for( uint8_t cBitSlip=0; cBitSlip < 8; cBitSlip++)
        //         {
        //             LOG (INFO) << BOLDMAGENTA << "Manually setting BitSlip on Line#" << +cLineId << " to " << +cBitSlip << RESET;
        //             cTuner.SetLineMode(cInterface, cHybrid->getId(), 0, cLineId, 0);
        //             cTuner.SetLineMode(cInterface, cHybrid->getId(), 0, cLineId, 2, 0, cMode, 0, 0);
        //             cInterface->StubDebug( true, cNlines );
        //         }
        //     }
        // }
    }
    if(fStubDebug)
    {
        for(auto cHybrid: *pOpticalGroup)
        {
            LOG(INFO) << BOLDMAGENTA << "Stub debug output - hybrid#" << +cHybrid->getId() << RESET;
            fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
            fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
            for(size_t cIter = 0; cIter < 1; cIter++)
            {
                LOG(INFO) << BOLDYELLOW << "Debug capture Iteration#" << cIter << RESET;
                cInterface->StubDebug(true, cNlines);
            }
        }
    }

    // disable stub output
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, false);
    }
    // align L1 data in the BE
    fL1Debug = false;
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::WordAlignBEdata ... word alignment on L1 lines from CIC.." << RESET;
    cAligned           = cInterface->L1WordAlignment(pOpticalGroup, fL1Debug);
    auto   cL1Bitslips = cInterface->getL1Bitslips();
    size_t cIndx       = 0;
    // configure CICs to NOT output alignment pattern on stub lines
    // and also re-confiure enabled FEs
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;

        auto& cBeBitSlipHybrd = cBeBitSlipOG->at(cHybrid->getIndex());
        auto& cThisBeBitSlip  = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();

        cThisBeBitSlip[0] = cL1Bitslips[cIndx];
        // disable alignment output
        fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
        cIndx++;
    }

    return cAligned;
}
// Phase align L1 + stub data in the backend
bool LinkAlignmentOT::PhaseAlignBEdata(const BeBoard* pBoard)
{
    bool cAligned = true;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            cAligned = PhaseAlignBEdata(cOpticalGroup);
            if(!cAligned) { throw std::runtime_error(std::string("Could not phase align-BE data in LinkAlignmentOT...")); }
        }
    }
    return cAligned;
}
bool LinkAlignmentOT::PhaseAlignBEdata(const OpticalGroup* pOpticalGroup)
{
    bool cAligned   = true;
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    auto& cBeSamplingDelay   = fBeSamplingDelay.at((*cBoardIter)->getIndex());
    auto& cBeSamplingDelayOG = cBeSamplingDelay->at(pOpticalGroup->getIndex());

    // configure CICs to output alignment pattern on L1 lines
    std::vector<uint8_t> cFeEnableRegs(0);
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, true);
        cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
    }
    // stop triggers to make sure that there are no L1 packets from the CIC
    fBeBoardInterface->Stop((*cBoardIter));

    // align stub lines in the BE
    size_t cNlines = (pOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::WordAlignBEdata ... word alignment on " << +cNlines << "/6 lines stub from CIC.." << RESET;
    D19cFWInterface::PhaseTuner cTuner;
    for(size_t cLineId = 0; cLineId <= cNlines; cLineId++)
    {
        for(auto cHybrid: *pOpticalGroup)
        {
            auto& cBeSamplingDelayHybrd = cBeSamplingDelayOG->at(cHybrid->getIndex());
            auto& cThisBeSamplingDelay  = cBeSamplingDelayHybrd->getSummary<std::vector<uint8_t>>();

            if(cLineId > 0)
                LOG(INFO) << BOLDMAGENTA << "Setting sampling delay on Stub line#" << +cLineId << " on Hybrid#" << +cHybrid->getId() << RESET;
            else
                LOG(INFO) << BOLDMAGENTA << "Setting sampling delay on L1A line on Hybrid#" << +cHybrid->getId() << RESET;
            cTuner.TunePhase(cInterface, cHybrid->getId(), 0, cLineId);
            cTuner.GetLineStatus(cInterface, cHybrid->getId(), 0, cLineId);
            cThisBeSamplingDelay[cLineId] = cTuner.fDelay;
            cAligned                      = cTuner.fPhaseAlignmentFSMstate == 14;
            if(!cAligned) return cAligned;
        }
    }

    // configure CICs to NOT output alignment pattern on stub lines
    // and re-configure FE enable register
    size_t cIndx = 0;
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
// bool LinkAlignmentOT::AlignStubPackage(BeBoard* pBoard)
// {
//     // set board and get interface
//     fBeBoardInterface->setBoard(pBoard->getId());
//     auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
//     // make sure you're only sending one trigger at a time here
//     LOG(INFO) << GREEN << "Trying to align CIC stub decoder in the back-end" << RESET;
//     // sparsification of
//     bool                 cSparsified = pBoard->getSparsification();
//     std::vector<uint8_t> cFeEnableRegs(0);
//     // disable FEs for all hybrids
//     if(cSparsified)
//         LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::AlignStubPackage Sparsification on " << RESET;
//     else
//         LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::AlignStubPackage Sparsification off " << RESET;

//     for(auto cOpticalGroup: *pBoard)
//     {
//         for(auto cHybrid: *cOpticalGroup)
//         {
//             auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
//             cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
//             // disable all FEs. . not needed here
//             fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
//         }
//     }
//     // check trigger source
//     // and reload
//     uint16_t cTriggerSrc         = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
//     uint16_t cOriginalTPdelay    = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
//     uint16_t cOriginalStubDelay  = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
//     uint16_t cOriginalTriggerSrc = cTriggerSrc;
//     uint16_t cOrignalTriggerMult = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
//     uint8_t  cOriginalTLUconfig  = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled");
//     cTriggerSrc                  = (cTriggerSrc == 6) ? cTriggerSrc : 6;
//     LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
//     std::vector<std::pair<std::string, uint32_t>> cRegVec;
//     cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
//     cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
//     cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0x0});
//     cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", 400});
//     cRegVec.push_back({"fc7_daq_cnfg.readout_block.global.common_stubdata_delay", 200});
//     cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
//     fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

//     bool    cSkip         = false;
//     uint8_t cPackageDelay = 7;
//     uint8_t cFinalDelay   = cPackageDelay;
//     if(!cSkip)
//     {
//         // gethybrid IDs
//         std::vector<uint8_t>                    cHybridIds(0);
//         std::map<uint8_t, std::vector<uint8_t>> cHybridIdsMap;
//         for(auto cOpticalGroup: *pBoard)
//         {
//             auto cIter = cHybridIdsMap.find(cOpticalGroup->getId());
//             if(cIter == cHybridIdsMap.end())
//             {
//                 std::vector<uint8_t> cDummy;
//                 cDummy.clear();
//                 cHybridIdsMap[cOpticalGroup->getId()] = cDummy;
//                 cIter                                 = cHybridIdsMap.find(cOpticalGroup->getId());
//             }
//             for(auto cHybrid: *cOpticalGroup)
//             {
//                 cHybridIds.push_back(cHybrid->getId());
//                 cIter->second.push_back(cHybrid->getId());
//             }
//         }
//         // unique ids for each hybrid
//         bool cCorrectDelay = false;
//         // now try and find correct package delay
//         uint16_t cMaxBxCounter  = 3564;
//         uint32_t cNevents       = 10;
//         auto     cOriginalDelay = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay");
//         LOG(INFO) << BOLDBLUE << "Original package delay is " << +cOriginalDelay << RESET;
//         LOG(DEBUG) << cMaxBxCounter << RESET;
//         size_t cAttempt = 0;
//         do
//         {
//             LOG(INFO) << BOLDMAGENTA << "Package delay alignment attempt#" << +cAttempt << RESET;
//             for(cPackageDelay = 0; cPackageDelay < 8; cPackageDelay++)
//             {
//                 if(cCorrectDelay) continue;

//                 LOG(INFO) << BOLDMAGENTA << "Trying a stub package delay set to " << +cPackageDelay << ".. check BxIds in SW" << RESET;
//                 fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay", cPackageDelay);
//                 cInterface->Bx0Alignment();

//                 ReadNEvents(pBoard, cNevents);
//                 const std::vector<Event*>& cEvents = this->GetEvents();
//                 LOG(DEBUG) << BOLDBLUE << "Read back " << +cEvents.size() << " events from the FC7 ..." << RESET;

//                 // fill map of BxIds for this hybrid
//                 std::map<uint8_t, std::vector<int>> cBxIds;
//                 for(auto& cEvent: cEvents)
//                 {
//                     for(auto cId: cHybridIds)
//                     {
//                         auto cIter = cBxIds.find(cId);
//                         if(cIter == cBxIds.end())
//                         {
//                             std::vector<int> cDummy;
//                             cDummy.clear();
//                             cBxIds[cId] = cDummy;
//                             cIter       = cBxIds.find(cId);
//                         }
//                         cIter->second.push_back(cEvent->BxId(cId));
//                     }
//                 }

//                 // check that BxIds ae synchronous across single links
//                 std::vector<uint8_t> cIdsToCompare(0);
//                 for(auto cIter: cHybridIdsMap)
//                 {
//                     LOG(INFO) << BOLDBLUE << "\t..Checking Sync for hybrids on Link#" << +cIter.first << RESET;
//                     bool cSyncThisLink = true;  // if there's only one hybrid by definition you are in sync
//                     if(cIter.second.size() > 1) // either 1 or 2 hybrids per link
//                     {
//                         // check if the two hybrids are synchronous
//                         LOG(DEBUG) << BOLDYELLOW << "\t.. checking sync between " << +cIter.second[0] << " and " << +cIter.second[1] << RESET;
//                         auto& cBxIdsFirst  = cBxIds[cIter.second[0]];
//                         auto& cBxIdsSecond = cBxIds[cIter.second[1]];
//                         cSyncThisLink      = (cBxIdsFirst == cBxIdsSecond);
//                         if(cSyncThisLink) LOG(DEBUG) << BOLDGREEN << "Sync on Link#" << +cIter.first << " between Hybrid#" << +cIter.second[0] << " and Hybrid#" << +cIter.second[1] << RESET;
//                     }
//                     // if in sync.. add first hybrid id to list
//                     if(cSyncThisLink) { cIdsToCompare.push_back(cIter.second[0]); }
//                     else
//                         LOG(INFO) << BOLDRED << "\t..FAILED sync on Link#" << +cIter.first << " between Hybrid#" << +cIter.second[0] << " and Hybrid#" << +cIter.second[1] << RESET;
//                 }
//                 // if all the links are synchronous then.. check if we are
//                 // in sync across the multiple links
//                 if(cIdsToCompare.size() == cHybridIdsMap.size())
//                 {
//                     std::vector<uint16_t> cPairsCompared;
//                     std::vector<uint8_t>  cMatchesFound;
//                     // compare ids from all links
//                     for(auto cIdFirst: cIdsToCompare)
//                     {
//                         for(auto cIdSecond: cIdsToCompare)
//                         {
//                             if(cIdFirst == cIdSecond) continue;
//                             uint16_t cPairId = (std::max(cIdFirst, cIdSecond) << 8) | std::min(cIdFirst, cIdSecond);
//                             if(std::find(cPairsCompared.begin(), cPairsCompared.end(), cPairId) != cPairsCompared.end()) continue;

//                             uint8_t cMatchFound = (cBxIds[cIdFirst] == cBxIds[cIdSecond]);
//                             if(cMatchFound)
//                             { LOG(INFO) << BOLDGREEN << "\t\t..BxIds from Hybrid#" << +cIdFirst << " and " << +cIdSecond << " are identical.. next will check the difference" << RESET; }
//                             else
//                                 LOG(INFO) << BOLDRED << "\t\t..BxIds from Hybrid#" << +cIdFirst << " and " << +cIdSecond << " DO NOT match.. " << RESET;
//                             cMatchesFound.push_back(cMatchFound);
//                             cPairsCompared.push_back(cPairId);
//                         }
//                     }
//                     if(cIdsToCompare.size() == 1)
//                     {
//                         uint16_t cPairId = 0xFF << 8 | cIdsToCompare[0];
//                         cPairsCompared.push_back(cPairId);
//                         cMatchesFound.push_back(1);
//                     }
//                     // for those that match.. check BxId difference
//                     std::vector<uint8_t> cFoundDelays(0);
//                     for(size_t cIndx = 0; cIndx < cMatchesFound.size(); cIndx++)
//                     {
//                         if(cMatchesFound[cIndx] == 0) continue;
//                         uint8_t              cFirst = cPairsCompared[cIndx] & 0xFF;
//                         uint8_t              cScnd  = (cPairsCompared[cIndx] >> 8) & 0xFF;
//                         std::vector<uint8_t> cIdsToCheck(0);
//                         cIdsToCheck.push_back(cFirst);
//                         // 0xFF marks the case where there is no second hybrid to c
//                         // compare against
//                         if(cScnd != 0xFF) cIdsToCheck.push_back(cScnd);
//                         // std::vector<uint8_t> cIdsToCheck{ cFirst, cScnd};
//                         size_t cNFound = 0;
//                         for(auto cIdToCheck: cIdsToCheck)
//                         {
//                             std::vector<int> cBxDifferences(0);
//                             size_t           cNRollOvers = 0;
//                             size_t           cCounter    = 0;
//                             for(auto cBxId: cBxIds[cIdToCheck])
//                             {
//                                 if(cCounter > 0)
//                                 {
//                                     auto cPreviousBxId = cBxIds[cIdToCheck][cCounter - 1];
//                                     int  cBxDifference = (cNRollOvers)*cMaxBxCounter + (cPreviousBxId % cMaxBxCounter);
//                                     cNRollOvers += ((cPreviousBxId >= 2500) && (cPreviousBxId < cMaxBxCounter)) && (cBxId < cPreviousBxId) ? 1 : 0;
//                                     cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBxId % cMaxBxCounter) - cBxDifference;
//                                     LOG(INFO) << BOLDYELLOW << "\t\t\t\t.. Diff#" << cCounter << " : " << cBxDifference << "[ BxID = " << cBxIds[cIdToCheck][cCounter] << " ]" << RESET;
//                                     cBxDifferences.push_back(cBxDifference);
//                                 }
//                                 cCounter++;
//                             }
//                             if(std::adjacent_find(cBxDifferences.begin(), cBxDifferences.end(), std::not_equal_to<int>()) == cBxDifferences.end())
//                             {
//                                 LOG(INFO) << BOLDGREEN << "\t\t\t..Constant BxId difference of " << +cBxDifferences[0] << " 40 MHz clks on Hybrid#" << +cIdToCheck << RESET;
//                                 cNFound++;
//                             }
//                         }
//                         cFoundDelays.push_back((cNFound == cIdsToCheck.size()) ? 1 : 0);
//                     }
//                     auto cNFound = std::accumulate(cFoundDelays.begin(), cFoundDelays.end(), 0);
//                     if((size_t)cNFound == cMatchesFound.size() && cNFound != 0)
//                     {
//                         LOG(INFO) << BOLDGREEN << "All hybrids match for a package delay of " << +cPackageDelay << RESET;
//                         cCorrectDelay = true;
//                     }
//                     else
//                         LOG(DEBUG) << BOLDRED << "For a package delay of " << +cPackageDelay << " found " << +cNFound << "/" << cMatchesFound.size()
//                                    << " pairs of hybrids with a constant difference in BxIds" << RESET;
//                 } // Ids are synchronous across each link
//                 else
//                     LOG(INFO) << BOLDRED << "For a pakcage delay of " << +cPackageDelay << " DE-SYNC in one of the links..." << RESET;
//             } // pkg delay
//             cAttempt++;
//         } while(cAttempt < 1 && !cCorrectDelay);
//     }
//     // set everything back to original values .. like I wasn't here
//     // reset fast command registers
//     LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::FindPackageDelay Resetting BeBoards regs back to their original values" << RESET;
//     cRegVec.clear();
//     cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cOriginalTriggerSrc});
//     cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
//     cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cOrignalTriggerMult});
//     cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", cOriginalTPdelay});
//     cRegVec.push_back({"fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cOriginalStubDelay});
//     cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", cOriginalTLUconfig});
//     fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

//     // reconfigure sparsification + FEs enabled in this CIC
//     LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::FindPackageDelay Resetting Sparsification" << RESET;
//     fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", (int)cSparsified);
//     size_t cIndx = 0;
//     for(auto cOpticalGroup: *pBoard)
//     {
//         for(auto cHybrid: *cOpticalGroup)
//         {
//             auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
//             fCicInterface->SetSparsification(cCic, cSparsified);
//             fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
//             cIndx++;
//         }
//     }
//     LOG(INFO) << BOLDMAGENTA << "Found package delay to be " << +cFinalDelay << RESET;
//     return cFinalDelay;
// }
bool LinkAlignmentOT::AlignStubPackage(BeBoard* pBoard)
{
    size_t cTriggerMult=0; 
    size_t cDelayAfterTP = 300; 
    // set board and get interface
    fBeBoardInterface->setBoard(pBoard->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    // make sure you're only sending one trigger at a time here
    LOG(INFO) << GREEN << "Trying to align CIC stub decoder in the back-end" << RESET;
    // sparsification of
    bool                 cSparsified = pBoard->getSparsification();
    std::vector<uint8_t> cFeEnableRegs(0);
    // disable FEs for all hybrids
    if(cSparsified)
        LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::AlignStubPackage Sparsification on " << RESET;
    else
        LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::AlignStubPackage Sparsification off " << RESET;

    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
            // disable all FEs. . not needed here
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        }
    }
    // check trigger source
    // and reload
    uint16_t cTriggerSrc         = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    uint16_t cOriginalTPdelay    = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    uint16_t cOriginalResetEn    = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset");
    uint16_t cOriginalStubDelay  = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
    uint16_t cOriginalTriggerSrc = cTriggerSrc;
    uint16_t cOrignalTriggerMult = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint8_t  cOriginalTLUconfig  = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled");
    
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cTriggerSrc                  = (cTriggerSrc == 6) ? cTriggerSrc : 6;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cTriggerMult});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", cDelayAfterTP});
    cRegVec.push_back({"fc7_daq_cnfg.readout_block.global.common_stubdata_delay", 200});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", 1});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    // first lets figure out how many hybrids are enabled 
    auto cEnableMask = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.global.hybrid_enable");
    // and select one hybrid from each link 
    uint32_t cNewMask = 0x00; 
    for(auto cOpticalGroup: *pBoard)
    {
        bool cFirstOnLink=true;
        for(auto cHybrid: *cOpticalGroup)
        {
            if(!cFirstOnLink) continue;
            LOG (INFO) << BOLDMAGENTA << "\t\t..Hybrid#" << +cHybrid->getId() << " on Link#" << +cOpticalGroup->getId() << RESET;
            cNewMask = cNewMask | ( 1 << cHybrid->getId() ); 
            cFirstOnLink = false;
        }
    }
    LOG (INFO) << BOLDBLUE << "LinkAlignmentOT::AlignStubPackage setting hybrid enable register to " << std::bitset<32>(cNewMask) << RESET;
    
    bool    cSkip         = false;
    uint8_t cPackageDelay = 7;
    uint8_t cFinalDelay   = cPackageDelay;
    if(!cSkip)
    {
        // gethybrid IDs
        std::vector<uint8_t>                    cHybridIds(0);
        std::map<uint8_t, std::vector<uint8_t>> cHybridIdsMap;
        for(auto cOpticalGroup: *pBoard)
        {
            auto cIter = cHybridIdsMap.find(cOpticalGroup->getId());
            if(cIter == cHybridIdsMap.end())
            {
                std::vector<uint8_t> cDummy;
                cDummy.clear();
                cHybridIdsMap[cOpticalGroup->getId()] = cDummy;
                cIter                                 = cHybridIdsMap.find(cOpticalGroup->getId());
            }
            bool cFirstOnLink=true;
            for(auto cHybrid: *cOpticalGroup)
            {
                if(!cFirstOnLink) continue;
                cHybridIds.push_back(cHybrid->getId());
                cIter->second.push_back(cHybrid->getId());
                cFirstOnLink = false;
            }
        }
        // unique ids for each hybrid
        bool cCorrectDelay = false;
        // now try and find correct package delay
        uint16_t cMaxBxCounter  = 3564;
        uint32_t cNevents       = 10;
        auto     cOriginalDelay = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay");
        LOG(INFO) << BOLDBLUE << "Original package delay is " << +cOriginalDelay << RESET;
        LOG(DEBUG) << cMaxBxCounter << RESET;
        size_t cAttempt = 0;
        do
        {
            LOG(INFO) << BOLDMAGENTA << "Package delay alignment attempt#" << +cAttempt << RESET;
            for(cPackageDelay = 0; cPackageDelay < 8; cPackageDelay++)
            {
                if(cCorrectDelay) continue;

                LOG(INFO) << BOLDMAGENTA << "Trying a stub package delay set to " << +cPackageDelay << ".. check BxIds in SW" << RESET;
                fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay", cPackageDelay);
                cInterface->Bx0Alignment();

                ReadNEvents(pBoard, cNevents);
                const std::vector<Event*>& cEvents = this->GetEvents();
                LOG(DEBUG) << BOLDBLUE << "Read back " << +cEvents.size() << " events from the FC7 ..." << RESET;

                // fill map of BxIds for this hybrid
                std::map<uint8_t, std::vector<int>> cBxIds;
                for(auto& cEvent: cEvents)
                {
                    for(auto cId: cHybridIds)
                    {
                        auto cIter = cBxIds.find(cId);
                        if(cIter == cBxIds.end())
                        {
                            std::vector<int> cDummy;
                            cDummy.clear();
                            cBxIds[cId] = cDummy;
                            cIter       = cBxIds.find(cId);
                        }
                        cIter->second.push_back(cEvent->BxId(cId));
                        LOG (INFO) << BOLDYELLOW << "Event#" << +cEvent->GetEventCount() << "\t.. Hybrid#" << +cId << " BxId is " << cEvent->BxId(cId) << RESET;
                    }
                }

                // check that BxIds ae synchronous across single links
                std::vector<uint8_t> cIdsToCompare(0);
                for(auto cIter: cHybridIdsMap)
                {
                    LOG(INFO) << BOLDBLUE << "\t..Checking Sync for hybrids on Link#" << +cIter.first << RESET;
                    bool cSyncThisLink = true;  // if there's only one hybrid by definition you are in sync
                    if(cIter.second.size() > 1) // either 1 or 2 hybrids per link
                    {
                        // check if the two hybrids are synchronous
                        LOG(DEBUG) << BOLDYELLOW << "\t.. checking sync between " << +cIter.second[0] << " and " << +cIter.second[1] << RESET;
                        auto& cBxIdsFirst  = cBxIds[cIter.second[0]];
                        auto& cBxIdsSecond = cBxIds[cIter.second[1]];
                        cSyncThisLink      = (cBxIdsFirst == cBxIdsSecond);
                        if(cSyncThisLink) LOG(DEBUG) << BOLDGREEN << "Sync on Link#" << +cIter.first << " between Hybrid#" << +cIter.second[0] << " and Hybrid#" << +cIter.second[1] << RESET;
                    }
                    // if in sync.. add first hybrid id to list
                    if(cSyncThisLink) { cIdsToCompare.push_back(cIter.second[0]); }
                    else
                        LOG(INFO) << BOLDRED << "\t..FAILED sync on Link#" << +cIter.first << " between Hybrid#" << +cIter.second[0] << " and Hybrid#" << +cIter.second[1] << RESET;
                }
                // if all the links are synchronous then.. check if we are
                // in sync across the multiple links
                if(cIdsToCompare.size() == cHybridIdsMap.size())
                {
                    std::vector<uint16_t> cPairsCompared;
                    std::vector<uint8_t>  cMatchesFound;
                    // compare ids from all links
                    for(auto cIdFirst: cIdsToCompare)
                    {
                        for(auto cIdSecond: cIdsToCompare)
                        {
                            if(cIdFirst == cIdSecond) continue;
                            uint16_t cPairId = (std::max(cIdFirst, cIdSecond) << 8) | std::min(cIdFirst, cIdSecond);
                            if(std::find(cPairsCompared.begin(), cPairsCompared.end(), cPairId) != cPairsCompared.end()) continue;

                            uint8_t cMatchFound = (cBxIds[cIdFirst] == cBxIds[cIdSecond]);
                            if(cMatchFound)
                            { LOG(INFO) << BOLDGREEN << "\t\t..BxIds from Hybrid#" << +cIdFirst << " and " << +cIdSecond << " are identical.. next will check the difference" << RESET; }
                            else
                                LOG(INFO) << BOLDRED << "\t\t..BxIds from Hybrid#" << +cIdFirst << " and " << +cIdSecond << " DO NOT match.. " << RESET;
                            cMatchesFound.push_back(cMatchFound);
                            cPairsCompared.push_back(cPairId);
                        }
                    }
                    if(cIdsToCompare.size() == 1)
                    {
                        uint16_t cPairId = 0xFF << 8 | cIdsToCompare[0];
                        cPairsCompared.push_back(cPairId);
                        cMatchesFound.push_back(1);
                    }
                    // for those that match.. check BxId difference
                    std::vector<uint8_t> cFoundDelays(0);
                    // std::vector<uint8_t> cGoodBxId(0);
                    for(size_t cIndx = 0; cIndx < cMatchesFound.size(); cIndx++)
                    {
                        if(cMatchesFound[cIndx] == 0) continue;
                        uint8_t              cFirst = cPairsCompared[cIndx] & 0xFF;
                        uint8_t              cScnd  = (cPairsCompared[cIndx] >> 8) & 0xFF;
                        std::vector<uint8_t> cIdsToCheck(0);
                        cIdsToCheck.push_back(cFirst);
                        // 0xFF marks the case where there is no second hybrid to c
                        // compare against
                        if(cScnd != 0xFF) cIdsToCheck.push_back(cScnd);
                        // std::vector<uint8_t> cIdsToCheck{ cFirst, cScnd};
                        size_t cNFound = 0;
                        for(auto cIdToCheck: cIdsToCheck)
                        {
                            std::vector<int> cBxDifferences(0);
                            size_t           cNRollOvers = 0;
                            size_t           cCounter    = 0;
                            uint8_t          cGoodBxIds  = 0; 
                            for(auto cBxId: cBxIds[cIdToCheck])
                            {
                                if( cBxId > 8) cGoodBxIds++;
                                if(cCounter > 0)
                                {
                                    auto cPreviousBxId = cBxIds[cIdToCheck][cCounter - 1];
                                    int  cBxDifference = (cNRollOvers)*cMaxBxCounter + (cPreviousBxId % cMaxBxCounter);
                                    cNRollOvers += ((cPreviousBxId >= 2500) && (cPreviousBxId < cMaxBxCounter)) && (cBxId < cPreviousBxId) ? 1 : 0;
                                    cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBxId % cMaxBxCounter) - cBxDifference;
                                    if( cBxId > (int)cDelayAfterTP ){ 
                                        LOG(INFO) << BOLDGREEN << "\t\t\t\t.. Diff#" << cCounter << " : " << cBxDifference << "[ BxID = " << cBxIds[cIdToCheck][cCounter] << " ]" << RESET;
                                    }
                                    else LOG(INFO) << BOLDRED << "\t\t\t\t.. Diff#" << cCounter << " : " << cBxDifference << "[ BxID = " << cBxIds[cIdToCheck][cCounter] << " ]" << RESET;
                                    cBxDifferences.push_back(cBxDifference);
                                }
                                cCounter++;
                            }
                            if(std::adjacent_find(cBxDifferences.begin(), cBxDifferences.end(), std::not_equal_to<int>()) == cBxDifferences.end())
                            {
                                if( cGoodBxIds == cBxIds[cIdToCheck].size() )
                                {
                                    LOG(INFO) << BOLDGREEN << "\t\t\t..Constant BxId difference of " << +cBxDifferences[0] << " 40 MHz clks on Hybrid#" << +cIdToCheck << RESET;
                                    cNFound++;
                                }
                                else LOG(INFO) << BOLDRED << "\t\t\t..Constant BxId difference of " << +cBxDifferences[0] << " 40 MHz clks on Hybrid#" << +cIdToCheck << RESET;
                                     
                            }
                        }
                        cFoundDelays.push_back((cNFound == cIdsToCheck.size()) ? 1 : 0);

                    }
                    auto cNFound = std::accumulate(cFoundDelays.begin(), cFoundDelays.end(), 0);
                    if((size_t)cNFound == cMatchesFound.size() && cNFound != 0)
                    {
                        LOG(INFO) << BOLDGREEN << "All hybrids match for a package delay of " << +cPackageDelay << RESET;
                        cCorrectDelay = true;
                    }
                    else
                        LOG(DEBUG) << BOLDRED << "For a package delay of " << +cPackageDelay << " found " << +cNFound << "/" << cMatchesFound.size()
                                   << " pairs of hybrids with a constant difference in BxIds" << RESET;
                } // Ids are synchronous across each link
                else
                    LOG(INFO) << BOLDRED << "For a pakcage delay of " << +cPackageDelay << " DE-SYNC in one of the links..." << RESET;
            } // pkg delay
            cAttempt++;
        } while(cAttempt < 1 && !cCorrectDelay);
    }
    // set everything back to original values .. like I wasn't here
    // reset fast command registers
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::FindPackageDelay Resetting BeBoards regs back to their original values" << RESET;
    cRegVec.clear();
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cOriginalTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cOrignalTriggerMult});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", cOriginalTPdelay});
    cRegVec.push_back({"fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cOriginalStubDelay});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", cOriginalResetEn});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", cOriginalTLUconfig});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    // reconfigure sparsification + FEs enabled in this CIC
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::FindPackageDelay Resetting Sparsification" << RESET;
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", (int)cSparsified);
    size_t cIndx = 0;
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SetSparsification(cCic, cSparsified);
            fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
            cIndx++;
        }
    }
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.global.hybrid_enable" , cEnableMask);
    // and check 
    ReadNEvents(pBoard, 10);
    const std::vector<Event*>& cEvents = this->GetEvents();
    for(auto& cEvent: cEvents)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto cBx = (int)cEvent->BxId(cHybrid->getId());
                LOG (INFO) << BOLDGREEN << "Link#" << +cOpticalGroup->getId() << " Hybrid#" << +cHybrid->getId() << " BxId " << cBx << RESET;
            }
        }
    }
    LOG(INFO) << BOLDMAGENTA << "Found package delay to be " << +cFinalDelay << RESET;
    return cFinalDelay;
}
bool LinkAlignmentOT::AlignStubPackage(const OpticalGroup* pOpticalGroup)
{
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    // make sure you're only sending one trigger at a time here
    LOG(INFO) << GREEN << "Trying to align CIC stub decoder in the back-end" << RESET;
    // sparsification of
    bool                 cSparsified   = (*cBoardIter)->getSparsification();
    uint32_t             cNevents      = 10;
    uint16_t             cMaxBxCounter = 3564;
    bool                 cCorrectDelay = false;
    std::vector<uint8_t> cFeEnableRegs(0);
    // disable FEs for all hybrids
    if(cSparsified)
        LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::AlignStubPackage Sparsification on " << RESET;
    else
        LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::AlignStubPackage Sparsification off " << RESET;

    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
        // disable all FEs. . not needed here
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
    }

    // check trigger source
    // and reload
    uint16_t cTriggerSrc         = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.fast_command_block.trigger_source");
    uint16_t cOriginalTriggerSrc = cTriggerSrc;
    uint16_t cOrignalTriggerMult = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint8_t  cOriginalTLUconfig  = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.tlu_block.tlu_enabled");
    cTriggerSrc                  = (cTriggerSrc == 6) ? cTriggerSrc : 6;
    LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0x0});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    fBeBoardInterface->WriteBoardMultReg((*cBoardIter), cRegVec);

    // now try and find correct package delay
    auto cOriginalDelay = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay");
    LOG(INFO) << BOLDBLUE << "Original package delay is " << +cOriginalDelay << RESET;
    uint8_t cPackageDelay = 7;
    uint8_t cFinalDelay   = cPackageDelay;
    for(cPackageDelay = 0; cPackageDelay < 8; cPackageDelay++)
    {
        if(cCorrectDelay) continue;

        LOG(INFO) << BOLDMAGENTA << "Trying a stub package delay set to " << +cPackageDelay << ".. check BxIds in SW" << RESET;
        fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay", cPackageDelay);
        cInterface->Bx0Alignment();

        // check stubs
        // 2 events should be enough
        // LOG(DEBUG) << BOLDMAGENTA << "Requesting " << +cNevents << " events from the board " << RESET;
        ReadNEvents((*cBoardIter), cNevents);
        const std::vector<Event*>& cEventsWithStubs = this->GetEvents();
        LOG(DEBUG) << BOLDBLUE << "Read back " << +cEventsWithStubs.size() << " events from the FC7 ..." << RESET;

        // now ... check for incrementing BxIds
        int              cNRollOvers = 0;
        std::vector<int> cBxIds(0);
        std::vector<int> cBxDifferences(0); // I think by injecting this way this number should always be the same ..
        for(auto& cEvent: cEventsWithStubs)
        {
            for(auto cHybrid: *pOpticalGroup)
            {
                if(cHybrid->getIndex() > 0) continue;

                auto cBx = (int)cEvent->BxId(cHybrid->getId());
                if(cBxIds.size() > 0)
                {
                    int cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBxIds[cBxIds.size() - 1] % cMaxBxCounter);
                    cNRollOvers += ((cBxIds[cBxIds.size() - 1] >= 2500) && (cBxIds[cBxIds.size() - 1] < cMaxBxCounter)) && (cBx < cBxIds[cBxIds.size() - 1]) ? 1 : 0;
                    cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBx % cMaxBxCounter) - cBxDifference;
                    cBxDifferences.push_back(cBxDifference);
                    // LOG(INFO) << BOLDBLUE << "\t.....BxDifference is " << +cBxDifference << RESET;
                }
                cBxIds.push_back(cBx);
                LOG(DEBUG) << BOLDBLUE << "Hybrid " << +cHybrid->getId() << " BxID " << +cBx << RESET;

            } // hybrids or CICs
        }     // events
        // figure out the differences between the bxIds
        auto cFirstDifference = cBxDifferences[0];
        std::adjacent_difference(cBxDifferences.begin(), cBxDifferences.end(), cBxDifferences.begin());
        cBxDifferences.erase(cBxDifferences.begin()); // erase the first element
        for(auto cDifference: cBxDifferences) LOG(DEBUG) << BOLDBLUE << "\t..." << +cDifference << RESET;
        // all elements are equal
        if(cFirstDifference != 0 && std::equal(cBxDifferences.begin() + 1, cBxDifferences.end(), cBxDifferences.begin()))
        {
            LOG(DEBUG) << BOLDGREEN << "Found differences between bxIds to always be the same : " << +cFirstDifference << RESET;
            LOG(INFO) << BOLDGREEN << "Going to fix the manual package delay to " << +cPackageDelay << RESET;
            cFinalDelay   = cPackageDelay;
            cCorrectDelay = true;
        }
        else
            LOG(DEBUG) << BOLDRED << "Found differences between bxIds to be different from one another." << RESET;

    } // pkg delay

    // set everything back to original values .. like I wasn't here
    // reset fast command registers
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::FindPackageDelay Resetting BeBoards regs back to their original values" << RESET;
    cRegVec.clear();
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cOriginalTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cOrignalTriggerMult});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", cOriginalTLUconfig});
    fBeBoardInterface->WriteBoardMultReg((*cBoardIter), cRegVec);

    // reconfigure sparsification + FEs enabled in this CIC
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::FindPackageDelay Resetting Sparsification" << RESET;
    fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", (int)cSparsified);
    size_t cIndx = 0;

    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        fCicInterface->SetSparsification(cCic, cSparsified);
        fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
        cIndx++;
    }
    LOG(INFO) << BOLDMAGENTA << "Found package delay to be " << +cFinalDelay << RESET;
    return cCorrectDelay;
}
// State machine control functions
void LinkAlignmentOT::Running()
{
    Initialise();
    try
    {
        this->Align();
    }
    catch(const std::exception& e)
    {
        fSuccess = false;
        LOG(INFO) << BOLDRED << "LinkAlignmentOT failed" << RESET;
        throw std::runtime_error(std::string("Could not align link in the BE"));
    }
    fSuccess = true;
    Reset();
}

void LinkAlignmentOT::Stop() {}

void LinkAlignmentOT::Pause() {}

void LinkAlignmentOT::Resume() {}