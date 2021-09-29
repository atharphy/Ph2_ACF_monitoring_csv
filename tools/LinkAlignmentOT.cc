#include "LinkAlignmentOT.h"

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
//#include "boost/format.hpp"


using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

LinkAlignmentOT::LinkAlignmentOT() : Tool() { fBoardRegContainer.reset(); fSuccess=true;}
LinkAlignmentOT::~LinkAlignmentOT() {}
void LinkAlignmentOT::Reset()
{
    LOG(INFO) << BOLDGREEN << "Resetting registers touched  by LinkAlignmentOT" << RESET;
    // set everything back to original values .. like I wasn't here
    bool cWithPS = false;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap)
        {
            if(cReg.first.find("stub_package_delay") != std::string::npos) continue;
            cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second));
        }
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
                LOG(DEBUG) << BOLDBLUE << "LinkAlignmentOT::Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;
                for(auto cChip: *cHybrid)
                {
                    if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->UpdateModifiedRegisterMap(cChip);
                    auto cModMap = fReadoutChipInterface->GetModifiedRegisterMap(cChip);
                    LOG(DEBUG) << BOLDBLUE << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    for(auto cMapItem: cModMap)
                    {
                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        LOG(DEBUG) << BOLDBLUE << "LinkAlignmentOT::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to "
                                   << cMapItem.second.fValue << RESET;
                        fReadoutChipInterface->WriteChipReg(cChip, cMapItem.first, cMapItem.second.fValue);
                    }
                }
            }
        }
    }
    if(fReadoutChipInterface != nullptr)
    {
        fReadoutChipInterface->ClearModifiedRegisterMap();
        if(cWithPS) static_cast<PSInterface*>(fReadoutChipInterface)->ResetModifiedRegisterMap();
    }
    resetPointers();
}

// Processing 
bool LinkAlignmentOT::Align()
{
    for( auto cBoard: *fDetectorContainer )
    {
        for( auto cOpticalGroup : *cBoard )
        {
            AlignLpGBTInputs(cOpticalGroup);
            try
            {
                WordAlignBEdata(cOpticalGroup);
            }
            catch(const std::exception& e)
            {
                fSuccess=false;
                LOG (INFO) << BOLDRED << "BE word alignment failed" << RESET;
                throw std::runtime_error(std::string("Could not word align BE data in LinkAlignmentOT..."));
            }

            try
            {
                AlignStubPackage(cOpticalGroup);
            }
            catch(const std::exception& e)
            {
                fSuccess=false;
                LOG (INFO) << BOLDRED << "BE stub package alignment failed" << RESET;
                throw std::runtime_error(std::string("Could not find stub package delay in LinkAlignmentOT..."));
            }
        }
    }
    fSuccess=true;
    return fSuccess;
}

// Initialization function 
void LinkAlignmentOT::Initialise()
{
    fSuccess = false;
    // retreive original settings for all chips and all back-end boards
    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    ContainerFactory::copyAndInitHybrid<std::vector<uint8_t>>(*fDetectorContainer, fBeSamplingDelay);
    ContainerFactory::copyAndInitHybrid<std::vector<uint8_t>>(*fDetectorContainer, fBeBitSlip);
    for(auto cBoard: *fDetectorContainer)
    {
        auto&                cBoardRegNap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        const BeBoardRegMap& cOrigRegMap  = static_cast<const BeBoard*>(cBoard)->getBeBoardRegMap();
        cBoardRegNap.insert(cOrigRegMap.begin(), cOrigRegMap.end());

        auto& cBeSamplingDelay = fBeSamplingDelay.at(cBoard->getIndex());
        auto& cBeBitSlip  = fBeBitSlip.at(cBoard->getIndex());

        for(auto cOpticalGroup: *cBoard)
        {
            auto& cBeSamplingDelayOG = cBeSamplingDelay->at(cOpticalGroup->getIndex());
            auto& cBeBitSlipOG  = cBeBitSlip->at(cOpticalGroup->getIndex());
            size_t cNlines = (cOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cBeSamplingDelayHybrd = cBeSamplingDelayOG->at(cHybrid->getIndex());
                auto& cBeBitSlipHybrd = cBeBitSlipOG->at(cHybrid->getIndex());
                auto& cThisBeSamplingDelay        = cBeSamplingDelayHybrd->getSummary<std::vector<uint8_t>>();
                auto& cThisBeBitSlip              = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();
                for( size_t cLineId=0; cLineId < cNlines; cLineId++)
                {
                    cThisBeSamplingDelay.push_back(0);
                    cThisBeBitSlip.push_back(0);
                }
            }
        }
    }

    // clear map of modified registers
    fWithCIC = false;
    if(fReadoutChipInterface != nullptr)
    {
        fReadoutChipInterface->ClearModifiedRegisterMap();
        bool cIsPS = false;
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                bool cWithLpGBT = (cOpticalGroup->flpGBT != nullptr); 
                fWithCIC = cWithLpGBT; 
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fWithCIC = fWithCIC || (cCic != nullptr );
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
    if( clpGBT == nullptr ) return true;
    
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

// Word align L1 + stub data in the backend 
bool LinkAlignmentOT::WordAlignBEdata(const BeBoard* pBoard)
{
    bool cAligned=true;
    for( auto cBoard: *fDetectorContainer )
    {
        for( auto cOpticalGroup : *cBoard )
        {
            cAligned = WordAlignBEdata(cOpticalGroup);
            if( !cAligned ){
                LOG (INFO) << BOLDRED << "Could not word align-BE data for BeBoard#" << +cBoard->getId() 
                    << " Link#" << +cOpticalGroup->getId() << RESET;
                throw std::runtime_error(std::string("Could not word align-BE data in LinkAlignmentOT..."));
                return cAligned;
            }
        }
    }
    return cAligned;
}
bool LinkAlignmentOT::WordAlignBEdata(const OpticalGroup* pOpticalGroup)
{
    fStubDebug = true;
    bool cAligned=false;
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    
    auto& cBeBitSlip  = fBeBitSlip.at((*cBoardIter)->getIndex());
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
    for( size_t cLineId =1 ; cLineId <= cNlines; cLineId++)
    {
        for(auto cHybrid: *pOpticalGroup)
        {
            auto& cBeBitSlipHybrd = cBeBitSlipOG->at(cHybrid->getIndex());
            auto& cThisBeBitSlip        = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();
            
            LOG (INFO) << BOLDMAGENTA << "Aligning Stub line#" << +cLineId << " on Hybrid#" << +cHybrid->getId() << RESET;
            cTuner.AlignWord(cInterface, cHybrid->getId(),0, cLineId, 0xEA, 8, true);
            cTuner.GetLineStatus(cInterface, cHybrid->getId(), 0, cLineId);
            cAligned = ( cTuner.fWordAlignmentFSMstate == 14 ); 
            cThisBeBitSlip[cLineId] = cTuner.fBitslip;
            if(!cAligned){
                LOG (INFO) << BOLDRED << "Could not word align-BE data for BeBoard#" << +cBoardId
                    << " Link#" << +pOpticalGroup->getId() << " stub line " << +(cLineId-1) <<  RESET;
                throw std::runtime_error(std::string("Could not word align-BE data in LinkAlignmentOT..."));
            }
            // check if I allow a bit-slip of 0
            if( cTuner.fBitslip == 0 ){
                size_t cMaxAttempts=10;
                size_t cIter=0;
                do
                {
                    cTuner.AlignWord(cInterface, cHybrid->getId(),0, cLineId, 0xEA, 8, true);
                    cTuner.GetLineStatus(cInterface, cHybrid->getId(), 0, cLineId);
                    cAligned = ( cTuner.fWordAlignmentFSMstate == 14 ); 
                    cThisBeBitSlip[cLineId] = cTuner.fBitslip;
                    cIter++;
                }while( cIter < cMaxAttempts && cTuner.fBitslip == 0 );
                if(  cTuner.fBitslip == 0 ) 
                {
                    LOG (INFO) << BOLDRED << "Bitslip of 0 found for BE-stub data for BeBoard#" << +cBoardId
                        << " Link#" << +pOpticalGroup->getId() << " stub line " << +(cLineId-1) <<  RESET;
                    throw std::runtime_error(std::string("Bitslip of 0 for word-aligned BE data in LinkAlignmentOT..."));
                }
            }
        }
    }
    // check for 0 bit slips 
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cBeBitSlipHybrd = cBeBitSlipOG->at(cHybrid->getIndex());
        auto& cThisBeBitSlip        = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();
        std::vector<uint8_t> cBitSlipHist(15,0);
        for( auto cItem : cThisBeBitSlip) cBitSlipHist[cItem]++;
        auto cMode = std::max_element( cBitSlipHist.begin(), cBitSlipHist.end() ) - cBitSlipHist.begin() ;
        LOG (INFO) << BOLDMAGENTA << "Most frequent bitslip is " << +cMode ;
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
            LOG (INFO) << BOLDMAGENTA << "Stub debug output - hybrid#" << +cHybrid->getId() << RESET;
            fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
            fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
            for( size_t cIter=0; cIter < 1; cIter++)
            {
                LOG (INFO) << BOLDYELLOW << "Debug capture Iteration#" << cIter << RESET;
                cInterface->StubDebug( true, cNlines );
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
    fL1Debug=false;
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::WordAlignBEdata ... word alignment on L1 lines from CIC.." << RESET;
    cAligned = cInterface->L1WordAlignment(pOpticalGroup, fL1Debug);
    auto cL1Bitslips = cInterface->getL1Bitslips();
    size_t cIndx=0;
    // configure CICs to NOT output alignment pattern on stub lines
    // and also re-confiure enabled FEs 
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        
        auto& cBeBitSlipHybrd = cBeBitSlipOG->at(cHybrid->getIndex());
        auto& cThisBeBitSlip        = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();
        
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
    bool cAligned=true;
    for( auto cBoard: *fDetectorContainer )
    {
        for( auto cOpticalGroup : *cBoard )
        {
            cAligned = PhaseAlignBEdata(cOpticalGroup);
            if( !cAligned ){
                throw std::runtime_error(std::string("Could not phase align-BE data in LinkAlignmentOT..."));
            }
        }
    }
    return cAligned;
}
bool LinkAlignmentOT::PhaseAlignBEdata(const OpticalGroup* pOpticalGroup)
{
    bool cAligned=true;
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    
    auto& cBeSamplingDelay = fBeSamplingDelay.at((*cBoardIter)->getIndex());
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
    for( size_t cLineId =0 ; cLineId <= cNlines; cLineId++)
    {
        for(auto cHybrid: *pOpticalGroup)
        {
            auto& cBeSamplingDelayHybrd = cBeSamplingDelayOG->at(cHybrid->getIndex());
            auto& cThisBeSamplingDelay        = cBeSamplingDelayHybrd->getSummary<std::vector<uint8_t>>();
            
            if( cLineId > 0 ) LOG (INFO) << BOLDMAGENTA << "Setting sampling delay on Stub line#" << +cLineId << " on Hybrid#" << +cHybrid->getId() << RESET;
            else LOG (INFO) << BOLDMAGENTA << "Setting sampling delay on L1A line on Hybrid#" << +cHybrid->getId() << RESET;
            cTuner.TunePhase(cInterface, cHybrid->getId(), 0, cLineId);
            cTuner.GetLineStatus(cInterface, cHybrid->getId(), 0, cLineId);
            cThisBeSamplingDelay[cLineId] = cTuner.fDelay;
            cAligned = cTuner.fPhaseAlignmentFSMstate == 14 ;
            if( !cAligned ) return cAligned; 
        }
    }

    // configure CICs to NOT output alignment pattern on stub lines
    // and re-configure FE enable register 
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
bool LinkAlignmentOT::AlignStubPackage(const OpticalGroup* pOpticalGroup)
{
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    

    // make sure you're only sending one trigger at a time here
    LOG(INFO) << GREEN << "Trying to align CIC stub decoder in the back-end" << RESET;
    // sparsification of
    bool cSparsified = (*cBoardIter)->getSparsification();
    uint32_t cNevents      = 10;
    uint16_t cMaxBxCounter = 3564;
    bool    cCorrectDelay = false;
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
    auto cOriginalDelay = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
    LOG(INFO) << BOLDBLUE << "Original package delay is " << +cOriginalDelay << RESET;
    uint8_t cPackageDelay = 7;
    uint8_t cFinalDelay   = cPackageDelay;
    for(cPackageDelay = 0; cPackageDelay < 8; cPackageDelay++)
    {
        if(cCorrectDelay) continue;

        LOG(INFO) << BOLDMAGENTA << "Package delay set to " << +cPackageDelay << RESET;
        fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay", cPackageDelay);
        cInterface->Bx0Alignment();

        // check stubs
        // 2 events should be enough
        // LOG(DEBUG) << BOLDMAGENTA << "Requesting " << +cNevents << " events from the board " << RESET;
        ReadNEvents((*cBoardIter), cNevents);
        const std::vector<Event*>& cEventsWithStubs = this->GetEvents();
        LOG(INFO) << BOLDBLUE << "Read back " << +cEventsWithStubs.size() << " events from the FC7 ..." << RESET;

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
                LOG(INFO) << BOLDBLUE << "Hybrid " << +cHybrid->getId() << " BxID " << +cBx << RESET;

            } // hybrids or CICs
        } // events
        // figure out the differences between the bxIds
        auto cFirstDifference = cBxDifferences[0];
        std::adjacent_difference(cBxDifferences.begin(), cBxDifferences.end(), cBxDifferences.begin());
        cBxDifferences.erase(cBxDifferences.begin()); // erase the first element
        for(auto cDifference: cBxDifferences) LOG(DEBUG) << BOLDBLUE << "\t..." << +cDifference << RESET;
        // all elements are equal
        if(cFirstDifference != 0 && std::equal(cBxDifferences.begin() + 1, cBxDifferences.end(), cBxDifferences.begin()))
        {
            LOG(INFO) << BOLDGREEN << "Found differences between bxIds to always be the same : " << +cFirstDifference << RESET;
            LOG(INFO) << BOLDGREEN << "Going to fix the manual package delay to " << +cPackageDelay << RESET;
            cFinalDelay   = cPackageDelay;
            cCorrectDelay = true;
        }
        else
            LOG(INFO) << BOLDRED << "Found differences between bxIds to be different from one another." << RESET;

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
        fSuccess=false;
        LOG (INFO) << BOLDRED << "LinkAlignmentOT failed" << RESET;
        throw std::runtime_error(std::string("Could not align link in the BE"));
    }
    fSuccess=true;
    Reset();
}

void LinkAlignmentOT::Stop() {}

void LinkAlignmentOT::Pause() {}

void LinkAlignmentOT::Resume() {}