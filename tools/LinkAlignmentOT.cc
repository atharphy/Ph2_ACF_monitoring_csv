#include "tools/LinkAlignmentOT.h"

#include "HWInterface/D19cBackendAlignmentFWInterface.h"
#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "Utils/CBCChannelGroupHandler.h"
#include "Utils/ContainerFactory.h"

//#include "boost/format.hpp"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

LinkAlignmentOT::LinkAlignmentOT() : OTTool() {}
LinkAlignmentOT::~LinkAlignmentOT() {}

// Processing
void LinkAlignmentOT::AlignStubPackage()
{
    for(const auto cBoard: *fDetectorContainer) { AlignStubPackage(cBoard); } // align stubs
}
bool LinkAlignmentOT::Align()
{
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
    LOG(INFO) << BOLDYELLOW << "LinkAlignmentOT::Align ..." << RESET;
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
    for(const auto cBoard: *fDetectorContainer)
    {
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
        // force trigger source to be internal triggers
        LOG(INFO) << BOLDYELLOW << "Forcing trigger source to internal triggers" << RESET;
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source", 3);
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
        for(auto cOpticalGroup: *cBoard)
        {
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
            AlignLpGBTInputs(cOpticalGroup);
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
            WordAlignBEdata(cOpticalGroup);
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
        }
        // check that word alignment of L1 data worked
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
        LOG(INFO) << BOLDYELLOW << "LinkAlignmentOT::Align ... trying to readout L1 data.. " << RESET;
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
        // ReadNEvents(cBoard, 10);
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
    } // align BE

std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
    AlignStubPackage();
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
    fSuccess = true;
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
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

    // no Chip registers to perserve

    // initialize containers that hold values found by this tool
    ContainerFactory::copyAndInitHybrid<std::vector<uint8_t>>(*fDetectorContainer, fBeSamplingDelay);
    ContainerFactory::copyAndInitHybrid<std::vector<uint8_t>>(*fDetectorContainer, fBeBitSlip);
    ContainerFactory::copyAndInitHybrid<uint8_t>(*fDetectorContainer, fLpGBTSamplingDelay);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cBeSamplingDelay = fBeSamplingDelay.getObject(cBoard->getId());
        auto& cBeBitSlip       = fBeBitSlip.getObject(cBoard->getId());
        auto& cLinkSampling    = fLpGBTSamplingDelay.getObject(cBoard->getId());

        for(auto cOpticalGroup: *cBoard)
        {
            auto&  cBeSamplingDelayOG = cBeSamplingDelay->getObject(cOpticalGroup->getId());
            auto&  cBeBitSlipOG       = cBeBitSlip->getObject(cOpticalGroup->getId());
            auto&  cLinkDelayOG       = cLinkSampling->getObject(cOpticalGroup->getId());
            size_t cNlines            = (cOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cBeSamplingDelayHybrd = cBeSamplingDelayOG->getObject(cHybrid->getId());
                auto& cBeBitSlipHybrd       = cBeBitSlipOG->getObject(cHybrid->getId());
                auto& cThisBeSamplingDelay  = cBeSamplingDelayHybrd->getSummary<std::vector<uint8_t>>();
                auto& cThisBeBitSlip        = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();
                auto& cLinkDelay            = cLinkDelayOG->getObject(cHybrid->getId())->getSummary<uint8_t>();
                cLinkDelay                  = 0;
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
    LOG(INFO) << BOLDYELLOW << "LinkAlignmentOT::AlignLpGBTInputs ..." << RESET;
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
    auto cMode = flpGBTInterface->PhaseAlignRx(clpGBT, cEportGroups, cEportChnls);
    cAligned   = cAligned && (cMode != 15);
    // cMode      = ( cMode > 8 ) ? 5 : cMode;
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

        auto& cLinkSampling = fLpGBTSamplingDelay.getObject((*cBoardIter)->getId())->getObject(pOpticalGroup->getId())->getObject(cHybrid->getId())->getSummary<uint8_t>();
        cLinkSampling       = cMode;
    }
    return cAligned;
}
void LinkAlignmentOT::CheckLpgbtOutputs(uint8_t pPattern)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard) { CheckLpgbtOutputs(cOpticalGroup, pPattern); }
    }
}
bool LinkAlignmentOT::CheckLpgbtOutputs(const OpticalGroup* pOpticalGroup, uint8_t pPattern)
{
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto                             cInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    D19cDebugFWInterface*            cDebugInterface   = cInterface->getDebugInterface();
    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();

    LOG(INFO) << BOLDMAGENTA << "Checking lpGBT-out data on OpticalGroup#" << +pOpticalGroup->getId() << RESET;
    auto& clpGBT = pOpticalGroup->flpGBT;
    if(clpGBT == nullptr) return true;

    // configure lpGBT to produce uplink pattern for all rx groups
    D19clpGBTInterface* clpGBTInterface    = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    uint32_t            cPatternToTransmit = pPattern << 24 | pPattern << 16 | pPattern << 8 | pPattern;
    clpGBTInterface->ConfigureDPPattern(clpGBT, cPatternToTransmit);
    clpGBTInterface->ConfigureRxSource(clpGBT, clpGBTInterface->getGroups(), lpGBTconstants::PATTERN_CONST);

    // now check output
    uint8_t cNlines = (pOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;
    for(auto cHybrid: *pOpticalGroup)
    {
        AlignerObject cAlignerObjct;
        cAlignerObjct.fHybrid = cHybrid->getId();
        cAlignerObjct.fChip   = 0;
        LineConfiguration cLineCnfg;
        cLineCnfg.fPattern       = pPattern;
        cLineCnfg.fPatternPeriod = 8;
        for(uint8_t cLineId = 1; cLineId <= cNlines; cLineId++)
        {
            LOG(INFO) << BOLDMAGENTA << "Aligning Stub line#" << +cLineId << " on Hybrid#" << +cHybrid->getId() << RESET;
            cAlignerObjct.fLine = cLineId;
            cAlignerInterface->AlignWord(cAlignerObjct, cLineCnfg, true);
            cAlignerInterface->GetLineStatus(cAlignerObjct);
            if(cAlignerInterface->IsLineWordAligned(cAlignerObjct)) LOG(INFO) << BOLDYELLOW << "\t..Line#" << +cLineId << " aligned." << RESET;
        }
        fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
        fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
        for(size_t cAttempt = 0; cAttempt < 2; cAttempt++)
        {
            LOG(INFO) << BOLDMAGENTA << "Stub debug output - post-alignment -  hybrid#" << +cHybrid->getId() << " - Attempt#" << +cAttempt << RESET;
            cDebugInterface->StubDebug(true, cNlines, (cAttempt > 0));
        }
        // L1A debug
        cAlignerObjct.fLine = 0;
        cLineCnfg.fPattern  = pPattern;
        cAlignerInterface->AlignWord(cAlignerObjct, cLineCnfg, true);
        cAlignerInterface->GetLineStatus(cAlignerObjct);
        if(cAlignerInterface->IsLineWordAligned(cAlignerObjct)) LOG(INFO) << BOLDYELLOW << "\t..Line#" << +cAlignerObjct.fLine << " aligned." << RESET;
        cDebugInterface->L1ADebug();
    }
    // back to normal pattern .. i.e. data from CIC
    clpGBTInterface->ConfigureRxSource(clpGBT, clpGBTInterface->getGroups(), lpGBTconstants::PATTERN_NORMAL);

    return true;
}

// Word align L1 + stub data in the backend
bool LinkAlignmentOT::WordAlignBEdata(const BeBoard* pBoard)
{
    bool cAligned = true;
    LOG(INFO) << BOLDYELLOW << "LinkAlignmentOT::WordAlignBEdata" << RESET;
    for(auto cOpticalGroup: *pBoard)
    {
        cAligned = WordAlignBEdata(cOpticalGroup);
        if(!cAligned)
        {
            LOG(INFO) << BOLDRED << "Could not word align-BE data for BeBoard#" << +pBoard->getId() << " Link#" << +cOpticalGroup->getId() << RESET;
            throw std::runtime_error(std::string("Could not word align-BE data in LinkAlignmentOT..."));
            return cAligned;
        }
    } // optical groups connected to this  board
    return cAligned;
}
bool LinkAlignmentOT::WordAlignBEdata(const OpticalGroup* pOpticalGroup)
{
    fStubDebug      = true;
    bool cAligned   = false;
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    LOG(INFO) << BOLDYELLOW << "LinkAlignmentOT::WordAlignBEdata for an OG " << RESET;
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    D19cDebugFWInterface*            cDebugInterface   = cInterface->getDebugInterface();
    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();
    cAlignerInterface->InitializeConfiguration();
    cAlignerInterface->InitializeAlignerObject();
    LOG(INFO) << BOLDYELLOW << "LinkAlignmentOT::WordAlignBEdata after debug interface " << RESET;

    auto& cBeBitSlip   = fBeBitSlip.getObject((*cBoardIter)->getId());
    auto& cBeBitSlipOG = cBeBitSlip->getObject(pOpticalGroup->getId());

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
    for(size_t cLineId = 1; cLineId <= cNlines; cLineId++)
    {
        for(auto cHybrid: *pOpticalGroup)
        {
            auto& cBeBitSlipHybrd = cBeBitSlipOG->getObject(cHybrid->getId());
            auto& cThisBeBitSlip  = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();

            LOG(INFO) << BOLDMAGENTA << "Aligning Stub line#" << +cLineId << " on Hybrid#" << +cHybrid->getId() << RESET;
            AlignerObject cAlignerObjct;
            cAlignerObjct.fHybrid = cHybrid->getId();
            cAlignerObjct.fChip   = 0;
            cAlignerObjct.fLine   = cLineId;
            LineConfiguration cLineCnfg;
            cLineCnfg.fPattern       = 0xEA;
            cLineCnfg.fPatternPeriod = 8;
            cAlignerInterface->AlignWord(cAlignerObjct, cLineCnfg, true);
            cAlignerInterface->GetLineStatus(cAlignerObjct);
            cAligned                = cAlignerInterface->IsLineWordAligned(cAlignerObjct);
            cThisBeBitSlip[cLineId] = cAlignerInterface->GetLineConfiguration().fBitslip;

            if(!cAligned)
            {
                LOG(INFO) << BOLDRED << "Could not word align-BE data for BeBoard#" << +cBoardId << " Link#" << +pOpticalGroup->getId() << " stub line " << +(cLineId - 1) << RESET;
                throw std::runtime_error(std::string("Could not word align-BE data in LinkAlignmentOT..."));
            }
            if(cThisBeBitSlip[cLineId] == 0 && !fAllowZeroBitslip)
            {
                size_t cMaxAttempts = 10;
                size_t cIter        = 0;
                do
                {
                    cAlignerInterface->AlignWord(cAlignerObjct, cLineCnfg, true);
                    cAlignerInterface->GetLineStatus(cAlignerObjct);
                    cAligned                = cAlignerInterface->IsLineWordAligned(cAlignerObjct);
                    cThisBeBitSlip[cLineId] = cAlignerInterface->GetLineConfiguration().fBitslip;
                    cIter++;
                } while(cIter < cMaxAttempts && cThisBeBitSlip[cLineId] == 0);
                if(cThisBeBitSlip[cLineId] == 0)
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
        auto&                cBeBitSlipHybrd = cBeBitSlipOG->getObject(cHybrid->getId());
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
        //             cDebugInterface->StubDebug( true, cNlines );
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
                cDebugInterface->StubDebug(true, cNlines);
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
    fL1Debug = true;
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::WordAlignBEdata ... word alignment on L1 lines from CIC.." << RESET;
    cAligned = L1WordAlignment(pOpticalGroup, fL1Debug);

    size_t cIndx = 0;
    // re-confiure enabled FEs
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
        cIndx++;
    }
    LOG(INFO) << BOLDYELLOW << "Reached end of WordAlignBEData" << RESET;
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

    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();
    cAlignerInterface->InitializeConfiguration();
    cAlignerInterface->InitializeAlignerObject();

    auto& cBeSamplingDelay   = fBeSamplingDelay.getObject((*cBoardIter)->getId());
    auto& cBeSamplingDelayOG = cBeSamplingDelay->getObject(pOpticalGroup->getId());

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
    for(size_t cLineId = 0; cLineId <= cNlines; cLineId++)
    {
        for(auto cHybrid: *pOpticalGroup)
        {
            auto& cBeSamplingDelayHybrd = cBeSamplingDelayOG->getObject(cHybrid->getId());
            auto& cThisBeSamplingDelay  = cBeSamplingDelayHybrd->getSummary<std::vector<uint8_t>>();

            if(cLineId > 0)
                LOG(INFO) << BOLDMAGENTA << "Setting sampling delay on Stub line#" << +cLineId << " on Hybrid#" << +cHybrid->getId() << RESET;
            else
                LOG(INFO) << BOLDMAGENTA << "Setting sampling delay on L1A line on Hybrid#" << +cHybrid->getId() << RESET;

            AlignerObject cAlignerObjct;
            cAlignerObjct.fHybrid = cHybrid->getId();
            cAlignerObjct.fChip   = 0;
            cAlignerObjct.fLine   = cLineId;
            LineConfiguration cLineCnfg;
            cAlignerInterface->TunePhase(cAlignerObjct, cLineCnfg);
            cAlignerInterface->GetLineStatus(cAlignerObjct);
            cAligned                      = cAlignerInterface->IsLinePhaseAligned(cAlignerObjct);
            cThisBeSamplingDelay[cLineId] = cAlignerInterface->GetLineConfiguration().fDelay;

            // cTuner.TunePhase(cInterface, cHybrid->getId(), 0, cLineId);
            // cTuner.GetLineStatus(cInterface, cHybrid->getId(), 0, cLineId);
            // cThisBeSamplingDelay[cLineId] = cTuner.fDelay;
            // cAligned                      = cTuner.fPhaseAlignmentFSMstate == 14;
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
std::pair<bool, uint8_t> LinkAlignmentOT::PhaseTuneLine(const Chip* pChip, uint8_t pLineId)
{
    std::pair<bool, uint8_t> cLineStatus;
    cLineStatus.first  = false;
    cLineStatus.second = 0;

    auto cBoardId   = pChip->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    LOG(DEBUG) << BOLDYELLOW << "LinkAlignmentOT::PhaseTuneLine#" << +pLineId << " for a Chip#" << +pChip->getId() << RESET;
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();
    cAlignerInterface->InitializeConfiguration();
    cAlignerInterface->InitializeAlignerObject();

    AlignerObject cAlignerObjct;
    cAlignerObjct.fHybrid = pChip->getHybridId();
    cAlignerObjct.fChip   = pChip->getId();
    cAlignerObjct.fLine   = pLineId;
    LineConfiguration cLineCnfg;
    cAlignerInterface->TunePhase(cAlignerObjct, cLineCnfg);
    cAlignerInterface->GetLineStatus(cAlignerObjct);
    cLineStatus.first = cAlignerInterface->IsLinePhaseAligned(cAlignerObjct);
    if(!cLineStatus.first)
    {
        LOG(INFO) << BOLDRED << "Could not phase align-BE data for BeBoard#" << +cBoardId << " Board#" << +pChip->getBeBoardId() << " Hybrid#" << +pChip->getHybridId() << " Chip#" << +pChip->getId()
                  << " line# " << +pLineId << RESET;
        throw std::runtime_error(std::string("Could not phase align-BE data in LinkAlignmentOT..."));
    }

    cLineStatus.second = cAlignerInterface->GetLineConfiguration().fDelay;
    return cLineStatus;
}

std::pair<bool, uint8_t> LinkAlignmentOT::WordAlignLine(const Chip* pChip, uint8_t pLineId, uint8_t pAlignmentPattern, uint8_t pPeriod)
{
    std::pair<bool, uint8_t> cLineStatus;
    cLineStatus.first  = false;
    cLineStatus.second = 0;

    auto cBoardId   = pChip->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();
    cAlignerInterface->InitializeConfiguration();
    cAlignerInterface->InitializeAlignerObject();

    AlignerObject cAlignerObjct;
    cAlignerObjct.fHybrid = pChip->getHybridId();
    cAlignerObjct.fChip   = pChip->getId();
    cAlignerObjct.fLine   = pLineId;
    LineConfiguration cLineCnfg;
    cLineCnfg.fPattern       = pAlignmentPattern;
    cLineCnfg.fPatternPeriod = pPeriod;
    cAlignerInterface->AlignWord(cAlignerObjct, cLineCnfg, true);
    cAlignerInterface->GetLineStatus(cAlignerObjct);
    cLineStatus.first  = cAlignerInterface->IsLineWordAligned(cAlignerObjct);
    cLineStatus.second = cAlignerInterface->GetLineConfiguration().fBitslip;

    if(!cLineStatus.first)
    {
        LOG(INFO) << BOLDRED << "Could not word align-BE data for BeBoard#" << +cBoardId << " Board#" << +pChip->getBeBoardId() << " Hybrid#" << +pChip->getHybridId() << " Chip#" << +pChip->getId()
                  << " line# " << +pLineId << RESET;
        throw std::runtime_error(std::string("Could not word align-BE data in LinkAlignmentOT..."));
    }
    // check if I allow a bit-slip of 0
    if(cLineStatus.second == 0 && !fAllowZeroBitslip)
    {
        size_t cMaxAttempts = 10;
        size_t cIter        = 0;
        do
        {
            cAlignerInterface->AlignWord(cAlignerObjct, cLineCnfg, true);
            cAlignerInterface->GetLineStatus(cAlignerObjct);
            cLineStatus.first  = cAlignerInterface->IsLineWordAligned(cAlignerObjct);
            cLineStatus.second = cAlignerInterface->GetLineConfiguration().fBitslip;
            cIter++;
        } while(cIter < cMaxAttempts && cLineStatus.second == 0);
        if(cLineStatus.second == 0)
        {
            LOG(INFO) << BOLDRED << "Bitslip of 0 found for BE-stub data for BeBoard#" << +cBoardId << " Board#" << +pChip->getBeBoardId() << " Hybrid#" << +pChip->getHybridId() << " Chip#"
                      << +pChip->getId() << " line# " << +pLineId << RESET;
            throw std::runtime_error(std::string("Bitslip of 0 for word-aligned BE data in LinkAlignmentOT..."));
        }
    }
    return cLineStatus;
}
void LinkAlignmentOT::ManuallyConfigureLine(const Chip* pChip, uint8_t pLineId, uint8_t pPhase, uint8_t pBitslip)
{
    auto cBoardId   = pChip->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();
    cAlignerInterface->InitializeConfiguration();
    cAlignerInterface->InitializeAlignerObject();

    AlignerObject cAlignerObjct;
    cAlignerObjct.fHybrid = pChip->getHybridId();
    cAlignerObjct.fChip   = pChip->getId();
    cAlignerObjct.fLine   = pLineId;
    LineConfiguration cLineCnfg;
    cLineCnfg.fMode       = 2;
    cLineCnfg.fDelay      = pPhase;
    cLineCnfg.fBitslip    = pBitslip;
    cLineCnfg.fEnableL1   = 0;
    cLineCnfg.fMasterLine = 0;
    cAlignerInterface->SetLineMode(cAlignerObjct, cLineCnfg);
}
void LinkAlignmentOT::LegacyAlignmentMPA(const Chip* pChip)
{
    auto cBoardId   = pChip->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());

    int cCounter     = 0;
    int cMaxAttempts = 10;

    uint32_t hardware_ready = 0;

    while(hardware_ready < 1)
    {
        if(cCounter++ > cMaxAttempts)
        {
            uint32_t delay5_done_cbc0     = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_stat.physical_interface_block.delay5_done_cbc0");
            uint32_t serializer_done_cbc0 = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_stat.physical_interface_block.serializer_done_cbc0");
            uint32_t bitslip_done_cbc0    = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_stat.physical_interface_block.bitslip_done_cbc0");

            uint32_t delay5_done_cbc1     = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_stat.physical_interface_block.delay5_done_cbc1");
            uint32_t serializer_done_cbc1 = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_stat.physical_interface_block.serializer_done_cbc1");
            uint32_t bitslip_done_cbc1    = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_stat.physical_interface_block.bitslip_done_cbc1");
            LOG(INFO) << "Clock Data Timing tuning failed after " << cMaxAttempts << " attempts with value - aborting!";
            LOG(INFO) << "Debug Info CBC0: delay5 done: " << delay5_done_cbc0 << ", serializer_done: " << serializer_done_cbc0 << ", bitslip_done: " << bitslip_done_cbc0;
            LOG(INFO) << "Debug Info CBC1: delay5 done: " << delay5_done_cbc1 << ", serializer_done: " << serializer_done_cbc1 << ", bitslip_done: " << bitslip_done_cbc1;
            uint32_t tuning_state_cbc0 = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_stat.physical_interface_block.state_tuning_cbc0");
            uint32_t tuning_state_cbc1 = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_stat.physical_interface_block.state_tuning_cbc1");
            LOG(INFO) << "tuning state cbc0: " << tuning_state_cbc0 << ", cbc1: " << tuning_state_cbc1;
            throw std::runtime_error("Clock Data Timing tuning failed");
        }

        fBeBoardInterface->ChipReSync(*cBoardIter);

        usleep(10);
        // reset  the timing tuning
        fBeBoardInterface->WriteBoardReg(*cBoardIter, "fc7_daq_ctrl.physical_interface_block.control.cbc3_tune_again", 0x1);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        hardware_ready = fBeBoardInterface->ReadBoardReg(*cBoardIter, "fc7_daq_stat.physical_interface_block.hardware_ready");
    }
}
bool LinkAlignmentOT::LineTuning(const Chip* pChip, uint8_t pLineId, uint8_t pAlignmentPattern, uint8_t pPeriod)
{
    // For now keep legacy until we can test on MPA SCC
    if(pChip->getFrontEndType() == FrontEndType::MPA or pChip->getFrontEndType() == FrontEndType::MPA2)
    {
        LegacyAlignmentMPA(pChip);
        return true;
    }

    auto cBoardId   = pChip->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();
    cAlignerInterface->InitializeConfiguration();
    cAlignerInterface->InitializeAlignerObject();

    LOG(INFO) << BOLDBLUE << "Phase and word alignement on BeBoard" << +cBoardId << " FE" << +pChip->getHybridId() << " CBC" << +pChip->getId() << " - line " << +pLineId << RESET;
    fBeBoardInterface->ChipReSync(*cBoardIter);

    AlignerObject cAlignerObjct;
    cAlignerObjct.fHybrid = pChip->getHybridId();
    cAlignerObjct.fChip   = pChip->getId();
    cAlignerObjct.fLine   = pLineId;
    LineConfiguration cLineCnfg;
    cLineCnfg.fMode    = 2;
    cLineCnfg.fDelay   = 0;
    cLineCnfg.fBitslip = 0;
    cAlignerInterface->SetLineMode(cAlignerObjct, cLineCnfg);

    bool                     cSuccess  = false;
    unsigned int             cAttempts = 0;
    std::pair<bool, uint8_t> cPhaseAlignmentStatus, cWordAlignmentStatus;
    do
    {
        try
        {
            cPhaseAlignmentStatus = PhaseTuneLine(pChip, pLineId);
            cSuccess              = cPhaseAlignmentStatus.first;
        }
        catch(const std::runtime_error& e)
        {
            LOG(ERROR) << "Failed to phase align line " << +pLineId << " of chip " << +pChip->getId() << RESET;
            cSuccess = false;
        }

        try
        {
            cWordAlignmentStatus = WordAlignLine(pChip, pLineId, pAlignmentPattern, pPeriod);
            cSuccess             = cWordAlignmentStatus.first && cSuccess;
        }
        catch(const std::runtime_error& e)
        {
            LOG(ERROR) << "Failed to word align line " << +pLineId << " of chip " << +pChip->getId() << RESET;
            cSuccess = false;
        }

        LOG(INFO) << BOLDBLUE << "Automated phase tuning attempt" << cAttempts << " : " << ((cSuccess) ? "Worked" : "Failed") << RESET;

        cAttempts++;
    } while(!cSuccess && cAttempts < 10);
    if(cSuccess && pLineId == 1 &&
       (pChip->getFrontEndType() == FrontEndType::CBC3 || pChip->getFrontEndType() == FrontEndType::SSA || pChip->getFrontEndType() == FrontEndType::SSA2 ||
        pChip->getFrontEndType() == FrontEndType::MPA || pChip->getFrontEndType() == FrontEndType::MPA2))
    {
        LOG(INFO) << BOLDBLUE << "Forcing L1A line to match alignment result for first stub line." << RESET;
        uint8_t cBitslip = cWordAlignmentStatus.second + (uint8_t)(pChip->getFrontEndType() == FrontEndType::SSA || pChip->getFrontEndType() == FrontEndType::SSA2 ||
                                                                   pChip->getFrontEndType() == FrontEndType::MPA || pChip->getFrontEndType() == FrontEndType::MPA2);
        ManuallyConfigureLine(pChip, pLineId, cPhaseAlignmentStatus.second, cBitslip);
    }
    return cSuccess;
}
bool LinkAlignmentOT::L1WordAlignment(const OpticalGroup* pOpticalGroup, bool pScope)
{
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    LOG(INFO) << BOLDYELLOW << "LinkAlignmentOT::L1WordAlignment " << RESET;

    auto                             cInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();
    D19cDebugFWInterface*            cDebugInterface   = cInterface->getDebugInterface();
    cAlignerInterface->InitializeConfiguration();
    cAlignerInterface->InitializeAlignerObject();

    bool cSuccess = true;

    // configure triggers
    // make sure you're only sending one trigger at a time
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.clear();
    std::vector<std::string> cFcmdRegs{"misc.trigger_multiplicity", "user_trigger_frequency", "trigger_source", "misc.backpressure_enable", "triggers_to_accept"};
    std::vector<uint16_t>    cFcmdRegVals{0, 100, 3, 0, 0};
    std::vector<uint8_t>     cFcmdRegOrigVals(0);
    for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    {
        std::string cRegName = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
        cVecReg.push_back({cRegName, cFcmdRegVals[cIndx]});
    }
    cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cVecReg.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    cVecReg.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x1});
    fBeBoardInterface->WriteBoardMultReg(*cBoardIter, cVecReg);

    auto& cBeBitSlip   = fBeBitSlip.getObject((*cBoardIter)->getId());
    auto& cBeBitSlipOG = cBeBitSlip->getObject(pOpticalGroup->getId());

    bool cAllowZeroBitslip = true;
    LOG(INFO) << BOLDBLUE << "Aligning the back-end to properly decode L1A data coming from the front-end objects." << RESET;
    fBeBoardInterface->ChipReSync(*cBoardIter);
    fBeBoardInterface->Start(*cBoardIter);
    // back-end tuning on l1 lines
    auto& clpGBT   = pOpticalGroup->flpGBT;
    bool  cOptical = clpGBT != nullptr;

    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        if(cCic == nullptr)
        {
            LOG(INFO) << BOLDYELLOW << " No CIC to use for L1 Word alignment..." << RESET;
            continue;
        }

        auto& cBeBitSlipHybrd = cBeBitSlipOG->getObject(cHybrid->getId());
        auto& cThisBeBitSlip  = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();

        int     cChipId = cCic->getId();
        uint8_t cLineId = 0;
        LOG(INFO) << BOLDBLUE << "Performing word alignment [in the back-end] to prepare for receiving CIC L1A data ...: FE " << +cHybrid->getId() << " Chip" << +cChipId << RESET;
        uint16_t cPattern = 0xFE;

        // configure pattern
        LOG(INFO) << BOLDBLUE << "LinkAlignmentOT::L1WordAlignment for CIC data" << RESET;
        AlignerObject cAlignerObjct;
        cAlignerObjct.fHybrid = cHybrid->getId();
        cAlignerObjct.fChip   = 0;
        cAlignerObjct.fLine   = cLineId;
        LineConfiguration cLineCnfg;
        cLineCnfg.fPattern    = cPattern;
        cLineCnfg.fMode       = 0;
        cLineCnfg.fEnableL1   = 0;
        cLineCnfg.fMasterLine = 0;
        cAlignerInterface->SetLineMode(cAlignerObjct, cLineCnfg);

        uint8_t cPhaseDelay = 0;
        if(!cOptical)
        {
            auto cL1AlignmentStatus = PhaseTuneLine(cCic, cLineId);
            cPhaseDelay             = cL1AlignmentStatus.second;
        }
        for(uint16_t cPatternLength = 40; cPatternLength < 41; cPatternLength++)
        {
            LOG(INFO) << BOLDYELLOW << "Trying to align data with patttern length " << +cPatternLength << RESET;
            std::pair<bool, uint8_t> cLineStatus;
            cLineCnfg.fPatternPeriod = cPatternLength;
            cAlignerInterface->AlignWord(cAlignerObjct, cLineCnfg, true);
            cAlignerInterface->GetLineStatus(cAlignerObjct);
            cLineStatus.first  = cAlignerInterface->IsLineWordAligned(cAlignerObjct);
            cLineStatus.second = cAlignerInterface->GetLineConfiguration().fBitslip;
            cSuccess           = cLineStatus.first;
            if(cSuccess) cThisBeBitSlip.push_back(cLineStatus.second);
        }
        // if the above doesn't work.. try and find the correct bitslip manually in software
        if(!cSuccess)
        {
            cSuccess = false;
            LOG(INFO) << BOLDBLUE << "Going to try and align manually in software..." << RESET;
            const uint8_t cMaxIters  = 10;
            uint8_t       cIterCount = 0;
            do
            {
                LOG(INFO) << BOLDBLUE << "\t\t Alignment attempt#" << +cIterCount << RESET;
                for(uint8_t cBitslip = 0; cBitslip < 8; cBitslip++)
                {
                    if(cSuccess) continue;
                    LOG(INFO) << BOLDMAGENTA << "Manually setting bitslip to " << +cBitslip << RESET;
                    ManuallyConfigureLine(cCic, cLineId, cPhaseDelay, cBitslip); // generic

                    auto        cWords   = fBeBoardInterface->ReadBlockBoardReg(*cBoardIter, "fc7_daq_stat.physical_interface_block.l1a_debug", 50);
                    std::string cBuffer  = "";
                    bool        cAligned = false;
                    std::string cOutput  = "\n";
                    for(auto cWord: cWords)
                    {
                        auto                     cString = std::bitset<32>(cWord).to_string();
                        std::vector<std::string> cOutputWords(0);
                        for(size_t cIndex = 0; cIndex < 4; cIndex++)
                        {
                            auto c8bitWord = cString.substr(cIndex * 8, 8);
                            cOutputWords.push_back(c8bitWord);
                            cAligned = (cAligned | (std::stoi(c8bitWord, nullptr, 2) == cPattern));
                        }
                        for(auto cIt = cOutputWords.end() - 1; cIt >= cOutputWords.begin(); cIt--) { cOutput += *cIt + " "; }
                        cOutput += "\n";
                    }
                    if(cAligned)
                    {
                        // this->ResetReadout();
                        // pTuner.fBitslip = cBitslip;
                        cSuccess = cAllowZeroBitslip ? true : (cBitslip != 0);
                        if(cSuccess)
                        {
                            LOG(INFO) << BOLDGREEN << cOutput << RESET;
                            cThisBeBitSlip.push_back(cBitslip);
                        }
                    }
                    else
                    {
                        LOG(INFO) << BOLDRED << cOutput << RESET;
                        fBeBoardInterface->Start(*cBoardIter);
                    }
                    // this->ResetReadout();
                }
                cIterCount++;
            } while(cIterCount < cMaxIters && !cSuccess);
        }
    }
    fBeBoardInterface->Stop(*cBoardIter);

    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        if(cCic == nullptr || !pScope) continue;

        // select lines for slvs debug
        fBeBoardInterface->WriteBoardReg(*cBoardIter, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
        fBeBoardInterface->WriteBoardReg(*cBoardIter, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
        cDebugInterface->L1ADebug();
    }

    return cSuccess;
}
bool LinkAlignmentOT::AlignStubPackage(BeBoard* pBoard)
{
    size_t cTriggerMult  = 0;
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
    cTriggerSrc = (cTriggerSrc == 6) ? cTriggerSrc : 6;
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
        bool cFirstOnLink = true;
        for(auto cHybrid: *cOpticalGroup)
        {
            if(!cFirstOnLink) continue;
            LOG(INFO) << BOLDMAGENTA << "\t\t..Hybrid#" << +cHybrid->getId() << " on Link#" << +cOpticalGroup->getId() << RESET;
            cNewMask     = cNewMask | (1 << cHybrid->getId());
            cFirstOnLink = false;
        }
    }
    LOG(INFO) << BOLDBLUE << "LinkAlignmentOT::AlignStubPackage setting hybrid enable register to " << std::bitset<32>(cNewMask) << RESET;

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
            bool cFirstOnLink = true;
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
                        LOG(INFO) << BOLDYELLOW << "Event#" << +cEvent->GetEventCount() << "\t.. Hybrid#" << +cId << " BxId is " << cEvent->BxId(cId) << RESET;
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
                                if(cBxId > 8) cGoodBxIds++;
                                if(cCounter > 0)
                                {
                                    auto cPreviousBxId = cBxIds[cIdToCheck][cCounter - 1];
                                    int  cBxDifference = (cNRollOvers)*cMaxBxCounter + (cPreviousBxId % cMaxBxCounter);
                                    cNRollOvers += ((cPreviousBxId >= 2500) && (cPreviousBxId < cMaxBxCounter)) && (cBxId < cPreviousBxId) ? 1 : 0;
                                    cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBxId % cMaxBxCounter) - cBxDifference;
                                    if(cBxId > (int)cDelayAfterTP)
                                    { LOG(INFO) << BOLDGREEN << "\t\t\t\t.. Diff#" << cCounter << " : " << cBxDifference << "[ BxID = " << cBxIds[cIdToCheck][cCounter] << " ]" << RESET; }
                                    else
                                        LOG(INFO) << BOLDRED << "\t\t\t\t.. Diff#" << cCounter << " : " << cBxDifference << "[ BxID = " << cBxIds[cIdToCheck][cCounter] << " ]" << RESET;
                                    cBxDifferences.push_back(cBxDifference);
                                }
                                cCounter++;
                            }
                            if(std::adjacent_find(cBxDifferences.begin(), cBxDifferences.end(), std::not_equal_to<int>()) == cBxDifferences.end())
                            {
                                if(cGoodBxIds == cBxIds[cIdToCheck].size())
                                {
                                    LOG(INFO) << BOLDGREEN << "\t\t\t..Constant BxId difference of " << +cBxDifferences[0] << " 40 MHz clks on Hybrid#" << +cIdToCheck << RESET;
                                    cNFound++;
                                }
                                else
                                    LOG(INFO) << BOLDRED << "\t\t\t..Constant BxId difference of " << +cBxDifferences[0] << " 40 MHz clks on Hybrid#" << +cIdToCheck << RESET;
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
    // set everything back to original values .. except for the trigger source
    // like I wasn't here
    // reset fast command registers
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::FindPackageDelay Resetting BeBoards regs back to their original values" << RESET;
    cRegVec.clear();
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 3});
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
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.global.hybrid_enable", cEnableMask);
    // and check
    // make sure you do this with internal triggers
    ReadNEvents(pBoard, 10);
    const std::vector<Event*>& cEvents = this->GetEvents();
    for(auto& cEvent: cEvents)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto cBx = (int)cEvent->BxId(cHybrid->getId());
                LOG(INFO) << BOLDGREEN << "Link#" << +cOpticalGroup->getId() << " Hybrid#" << +cHybrid->getId() << " BxId " << cBx << RESET;
            }
        }
    }
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source", cOriginalTriggerSrc);
    LOG(INFO) << BOLDMAGENTA << "Found package delay to be " << +cFinalDelay << RESET;

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
            auto cHybrid = pOpticalGroup->getFirstObject();
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
