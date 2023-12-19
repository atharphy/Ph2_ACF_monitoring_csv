#include "tools/ECVLinkAlignmentOT.h"

#include "HWInterface/D19cBackendAlignmentFWInterface.h"
#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/ExceptionHandler.h"
#include "Utils/CBCChannelGroupHandler.h"
#include "Utils/ContainerFactory.h"
#include "Utils/GenericDataArray.h"
//#include "boost/format.hpp"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

ECVLinkAlignmentOT::ECVLinkAlignmentOT() : LinkAlignmentOT() {}
ECVLinkAlignmentOT::~ECVLinkAlignmentOT() {}

bool ECVLinkAlignmentOT::Scan()
{
    LOG(INFO) << BOLDYELLOW << "ECVLinkAlignmentOT::Align ..." << RESET;
    for(const auto cBoard: *fDetectorContainer)
    {
        // force trigger source to be internal triggers
        LOG(INFO) << BOLDYELLOW << "Forcing trigger source to internal triggers" << RESET;
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source", 3);
        for(auto cOpticalGroup: *cBoard)
        {
            ECV(cOpticalGroup);
        }
    }

    fSuccess = true;
    return fSuccess;
}

// Initialization function
void ECVLinkAlignmentOT::Initialise()
{
    // prepare common OTTool
    Prepare();
    SetName("ECVLinkAlignmentOT");

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

    #ifdef __USE_ROOT__
        fDQMHistogrammer.book(fResultFile, *fDetectorContainer, fSettingsMap);
    #endif
}

void ECVLinkAlignmentOT::SetCICClockPolarityAndStrength(const OpticalGroup* pOpticalGroup, bool pInverted, uint8_t pStrength)
{
    uint8_t                    cHybridClockDrive = pStrength;
    uint8_t                    cPreEmphMode      = 0; // 3
    uint8_t                    cPreEmphStr       = 0; // 7
    auto& clpGBT = pOpticalGroup->flpGBT;

    for(auto cHybrid: *pOpticalGroup)
    {
        uint8_t cSide = cHybrid->getId() % 2;
        // first .. send clock to the CBCs on this hybrid
        lpGBTClockConfig cClkCnfg;
        cClkCnfg.fClkFreq         = 4;
        cClkCnfg.fClkDriveStr     = cHybridClockDrive;
        if (pInverted)
            cClkCnfg.fClkInvert       = (cSide == 0) ? 0 : 1; //1:0
        else
            cClkCnfg.fClkInvert       = (cSide == 0) ? 1 : 0; //1:0

        cClkCnfg.fClkPreEmphWidth = 0; 
        cClkCnfg.fClkPreEmphMode  = cPreEmphMode;
        cClkCnfg.fClkPreEmphStr   = cPreEmphStr;
        LOG(INFO) << BOLDBLUE << "Enabling Hybrid clock [Side == " << +cSide << "]" << RESET;

        if(pOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S)
            static_cast<D19clpGBTInterface*>(flpGBTInterface)->hybridClock(clpGBT, cClkCnfg, cSide);
        else
            static_cast<D19clpGBTInterface*>(flpGBTInterface)->cicClock(clpGBT, cClkCnfg, cSide);
    }
}

void ECVLinkAlignmentOT::ECV(const OpticalGroup* pOpticalGroup)
{
    uint8_t clockPolarityStart      = 0, clockPolarityEnd       = 1 ;
    uint8_t cicClockStrengthStart   = 7, cicClockStrengthEnd    = 7 ;
    uint8_t cicSLVSStrengthStart    = 5, cicSLVSStrengthEnd     = 5 ;
    uint8_t lpGBTPhaseStart         = 0, lpGBTPhaseEnd          = 14;

    
    InitWordAlignStubs(pOpticalGroup);
    for (uint8_t clockPolarity = clockPolarityStart; clockPolarity <= clockPolarityEnd; clockPolarity++)
    {
        for (uint8_t clockStrength = cicClockStrengthStart; clockStrength <= cicClockStrengthEnd; clockStrength++)
        {
            SetCICClockPolarityAndStrength(pOpticalGroup, clockPolarity == 0, clockStrength);
            for (uint8_t cicStrength = cicSLVSStrengthStart; cicStrength <= cicSLVSStrengthEnd; cicStrength ++)
            {
                for(auto cHybrid: *pOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fCicInterface->ConfigureDriveStrength(cCic, cicStrength); 
                }
                for (uint8_t phase = lpGBTPhaseStart; phase <= lpGBTPhaseEnd; phase++)
                {
                    LOG (INFO) << BOLDRED << "CLOCK POLARITY:\t" << +clockPolarity << RESET;
                    LOG (INFO) << BOLDRED << "CLOCK STRENGTH:\t" << +clockStrength << RESET;
                    LOG (INFO) << BOLDRED << "CIC STRENGTH:\t" << +cicStrength << RESET;
                    LOG (INFO) << BOLDRED << "RX PHASE:\t" << +phase << RESET;

                    SetlpGBTRxPhase(pOpticalGroup,phase);
                    std::vector<std::pair<uint8_t, std::pair<uint8_t,bool>>>  alignedLines = CheckWordAlignBEdataStubs(pOpticalGroup);

                    for (auto a : alignedLines)
                    {
                        StoreWordAlignInHistogram(clockPolarity,clockStrength, cicStrength,phase,a.first,a.second.first, a.second.second);
                    }
                }
            }
        }
    }
    
    StopWordAlignStubs(pOpticalGroup);
    for (uint8_t clockPolarity = clockPolarityStart; clockPolarity <= clockPolarityEnd; clockPolarity++)
    {
        for (uint8_t clockStrength = cicClockStrengthStart; clockStrength <= cicClockStrengthEnd; clockStrength++)
        {
            SetCICClockPolarityAndStrength(pOpticalGroup, clockPolarity == 0, clockStrength);
            for (uint8_t cicStrength = cicSLVSStrengthStart; cicStrength <= cicSLVSStrengthEnd; cicStrength ++)
            {
                for(auto cHybrid: *pOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fCicInterface->ConfigureDriveStrength(cCic, cicStrength); 
                }
                for (uint8_t phase = lpGBTPhaseStart; phase <= lpGBTPhaseEnd; phase++)
                {
                    LOG (INFO) << BOLDRED << "CLOCK POLARITY:\t" << +clockPolarity << RESET;
                    LOG (INFO) << BOLDRED << "CLOCK STRENGTH:\t" << +clockStrength << RESET;
                    LOG (INFO) << BOLDRED << "CIC STRENGTH:\t" << +cicStrength << RESET;
                    LOG (INFO) << BOLDRED << "RX PHASE:\t" << +phase << RESET;

                    SetlpGBTRxPhase(pOpticalGroup,phase);
                    std::vector<std::pair<uint8_t, std::pair<uint8_t,bool>>>  alignedLines = CheckWordAlignBEdataL1(pOpticalGroup);

                    for (auto a : alignedLines)
                    {
                        StoreWordAlignInHistogram(clockPolarity,clockStrength, cicStrength,phase,a.first,a.second.first, a.second.second);
                    }
                }
            }
        }
    }

    for (uint8_t clockPolarity = clockPolarityStart; clockPolarity <= clockPolarityEnd; clockPolarity++)
    {
        for (uint8_t clockStrength = cicClockStrengthStart; clockStrength <= cicClockStrengthEnd; clockStrength++)
        {
            SetCICClockPolarityAndStrength(pOpticalGroup, clockPolarity == 0, clockStrength);
            for (uint8_t cicStrength = cicSLVSStrengthStart; cicStrength <= cicSLVSStrengthEnd; cicStrength ++)
            {
                for(auto cHybrid: *pOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fCicInterface->ConfigureDriveStrength(cCic, cicStrength); 
                }
                for (uint8_t phase = lpGBTPhaseStart; phase <= lpGBTPhaseEnd; phase++)
                {
                    LOG (INFO) << BOLDRED << "CLOCK POLARITY:\t" << +clockPolarity << RESET;
                    LOG (INFO) << BOLDRED << "CLOCK STRENGTH:\t" << +clockStrength << RESET;
                    LOG (INFO) << BOLDRED << "CIC STRENGTH:\t" << +cicStrength << RESET;
                    LOG (INFO) << BOLDRED << "RX PHASE:\t" << +phase << RESET;

                    SetlpGBTRxPhase(pOpticalGroup,phase);

                    std::vector<std::pair<uint8_t, std::pair< uint8_t, float>>> stubBER  = StubBitErrorTest(pOpticalGroup);
                    for (auto ber: stubBER)
                    {
                        StoreBERInHistogram(clockPolarity,clockStrength, cicStrength,phase,ber.first, ber.second.first, ber.second.second);
                    }
                }
            }
        }
    }

    InitL1BitErrorTest(pOpticalGroup);
    for (uint8_t clockPolarity = clockPolarityStart; clockPolarity <= clockPolarityEnd; clockPolarity++)
    {
        for (uint8_t clockStrength = cicClockStrengthStart; clockStrength <= cicClockStrengthEnd; clockStrength++)
        {
            SetCICClockPolarityAndStrength(pOpticalGroup, clockPolarity == 0, clockStrength);
            for (uint8_t cicStrength = cicSLVSStrengthStart; cicStrength <= cicSLVSStrengthEnd; cicStrength ++)
            {
                for(auto cHybrid: *pOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fCicInterface->ConfigureDriveStrength(cCic, cicStrength); 
                }
                for (uint8_t phase = lpGBTPhaseStart; phase <= lpGBTPhaseEnd; phase++)
                {
                    LOG (INFO) << BOLDRED << "CLOCK POLARITY:\t" << +clockPolarity << RESET;
                    LOG (INFO) << BOLDRED << "CLOCK STRENGTH:\t" << +clockStrength << RESET;
                    LOG (INFO) << BOLDRED << "CIC STRENGTH:\t" << +cicStrength << RESET;
                    LOG (INFO) << BOLDRED << "RX PHASE:\t" << +phase << RESET;

                    SetlpGBTRxPhase(pOpticalGroup,phase);

                    std::vector<std::pair<uint8_t, std::pair< uint8_t, float>>>  l1BER  = L1BitErrorTest(pOpticalGroup);
                    for (auto ber: l1BER)
                    {
                        StoreBERInHistogram(clockPolarity,clockStrength, cicStrength,phase,ber.first, ber.second.first, ber.second.second);
                    }
                }
            }
        }
    }
    

    for (uint8_t clockPolarity = clockPolarityStart; clockPolarity <= clockPolarityEnd; clockPolarity++)
    {
        for (uint8_t clockStrength = cicClockStrengthStart; clockStrength <= cicClockStrengthEnd; clockStrength++)
        {
            SetCICClockPolarityAndStrength(pOpticalGroup, clockPolarity == 0, clockStrength);
            for (uint8_t cicStrength = cicSLVSStrengthStart; cicStrength <= cicSLVSStrengthEnd; cicStrength ++)
            {
                for(auto cHybrid: *pOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fCicInterface->ConfigureDriveStrength(cCic, cicStrength); 
                }
                LOG (INFO) << BOLDRED << "CLOCK POLARITY:\t" << +clockPolarity << RESET;
                LOG (INFO) << BOLDRED << "CLOCK STRENGTH:\t" << +clockStrength << RESET;
                LOG (INFO) << BOLDRED << "CIC STRENGTH:\t" << +cicStrength << RESET;

                AlignLpGBTInputs(pOpticalGroup);

                std::vector<std::pair<uint8_t,std::pair<uint8_t,uint8_t>>> trainedPhases = flpGBTInterface->fTrainedPhases;
                uint8_t chosenPhase = flpGBTInterface->fChosenPhase;
                std::vector<uint8_t> hybridIds;
                // = getGroupsAndChannels(pOpticalGroup, false);
                for (auto trainedPhase : trainedPhases)
                {
                    if(pOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S)
                    {
                        if      (trainedPhase.first == 0 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,6, trainedPhase.second.second);
                        else if (trainedPhase.first == 4 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,0, trainedPhase.second.second);
                        else if (trainedPhase.first == 4 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,1, trainedPhase.second.second);
                        else if (trainedPhase.first == 5 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,2, trainedPhase.second.second);
                        else if (trainedPhase.first == 5 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,3, trainedPhase.second.second);
                        else if (trainedPhase.first == 6 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,4, trainedPhase.second.second);
                        
                        else if (trainedPhase.first == 0 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,0, trainedPhase.second.second);
                        else if (trainedPhase.first == 1 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,1, trainedPhase.second.second);
                        else if (trainedPhase.first == 1 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,2, trainedPhase.second.second);
                        else if (trainedPhase.first == 2 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,3, trainedPhase.second.second);
                        else if (trainedPhase.first == 2 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,4, trainedPhase.second.second);
                        else if (trainedPhase.first == 3 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,6, trainedPhase.second.second);
                    }
                    if(pOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS)
                    {
                        if      (trainedPhase.first == 4 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,6, trainedPhase.second.second);
                        else if (trainedPhase.first == 4 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,5, trainedPhase.second.second);
                        else if (trainedPhase.first == 5 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,3, trainedPhase.second.second);
                        else if (trainedPhase.first == 5 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,4, trainedPhase.second.second);
                        else if (trainedPhase.first == 6 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,1, trainedPhase.second.second);
                        else if (trainedPhase.first == 6 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,2, trainedPhase.second.second);
                        else if (trainedPhase.first == 0 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,0, trainedPhase.second.second);
                        
                        else if (trainedPhase.first == 0 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,5, trainedPhase.second.second);
                        else if (trainedPhase.first == 1 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,6, trainedPhase.second.second);
                        else if (trainedPhase.first == 1 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,4, trainedPhase.second.second);
                        else if (trainedPhase.first == 2 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,3, trainedPhase.second.second);
                        else if (trainedPhase.first == 2 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,2, trainedPhase.second.second);
                        else if (trainedPhase.first == 3 && trainedPhase.second.first == 0) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,1,1, trainedPhase.second.second);
                        else if (trainedPhase.first == 3 && trainedPhase.second.first == 2) StoreTrainedPhases(clockPolarity, clockStrength, cicStrength,0,0, trainedPhase.second.second);
                    }
                }
                StoreChosenPhase(clockPolarity, clockStrength, cicStrength, chosenPhase);
            }
        }
    }




}

void ECVLinkAlignmentOT::SetlpGBTRxPhase(const OpticalGroup* pOpticalGroup, uint8_t pPhase)
{
    auto& clpGBT = pOpticalGroup->flpGBT;

    std::vector<uint8_t> cEportGroups = getGroupsAndChannels(pOpticalGroup, true);
    std::vector<uint8_t> cEportChnls = getGroupsAndChannels(pOpticalGroup, false);

    for(size_t cIndx = 0; cIndx < cEportGroups.size(); cIndx++) { flpGBTInterface->ConfigureRxPhase(clpGBT, cEportGroups[cIndx], cEportChnls[cIndx], pPhase); }

}

void ECVLinkAlignmentOT::InitWordAlignStubs(const OpticalGroup* pOpticalGroup)
{
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    LOG(INFO) << BOLDYELLOW << "ECVLinkAlignmentOT::WordAlignBEdata for an OG " << RESET;
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();
    cAlignerInterface->InitializeConfiguration();
    cAlignerInterface->InitializeAlignerObject();
    LOG(INFO) << BOLDYELLOW << "ECVLinkAlignmentOT::WordAlignBEdata after debug interface " << RESET;

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
}

std::vector<std::pair<uint8_t, std::pair<uint8_t,bool>>> ECVLinkAlignmentOT::CheckWordAlignBEdataStubs(const OpticalGroup* pOpticalGroup)
{
    std::vector<std::pair<uint8_t, std::pair<uint8_t,bool>>> alignedLines;

    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });

    auto& cBeBitSlip   = fBeBitSlip.getObject((*cBoardIter)->getId());
    auto& cBeBitSlipOG = cBeBitSlip->getObject(pOpticalGroup->getId());

    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();
    cAlignerInterface->InitializeConfiguration();
    cAlignerInterface->InitializeAlignerObject();

    size_t cNlines = (pOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;

    for(auto cHybrid: *pOpticalGroup)
    {
        for(size_t cLineId = 1; cLineId <= cNlines; cLineId++)
        {
            auto& cBeBitSlipHybrd = cBeBitSlipOG->getObject(cHybrid->getId());
            auto& cThisBeBitSlip  = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();

            LOG(INFO) << BOLDMAGENTA << "Aligning Stub line#" << +cLineId << " on Hybrid#" << +cHybrid->getId() << RESET;
            AlignerObject cAlignerObjct;
            cAlignerObjct.fHybrid = cHybrid->getId();
            cAlignerObjct.fChip   = 0;
            cAlignerObjct.fLine   = cLineId;
            cAlignerObjct.fOptical = 1;
            LineConfiguration cLineCnfg;
            cLineCnfg.fPattern       = 0xEA;
            cLineCnfg.fPatternPeriod = 8;
            cAlignerInterface->AlignWord(cAlignerObjct, cLineCnfg, true);
            bool cAligned                = cAlignerInterface->IsLineWordAligned();
            LOG (INFO) << "Line " << +cLineId << " " << cAligned << RESET;

            cThisBeBitSlip[cLineId] = cAlignerInterface->GetLineConfiguration().fBitslip;

            if(cThisBeBitSlip[cLineId] == 0 && !fAllowZeroBitslip)
            {
                size_t cMaxAttempts = 10;
                size_t cIter        = 0;
                do
                {
                    cAlignerInterface->AlignWord(cAlignerObjct, cLineCnfg, true);
                    cAligned                = cAlignerInterface->IsLineWordAligned();
                    cThisBeBitSlip[cLineId] = cAlignerInterface->GetLineConfiguration().fBitslip;
                    cIter++;
                } while(cIter < cMaxAttempts && cThisBeBitSlip[cLineId] == 0);
            }
            alignedLines.push_back(std::make_pair( cHybrid->getId(), std::make_pair(cLineId-1 ,cAligned )) );
            //LOG (INFO) << cHybrid->getId() << RESET;
            //LOG (INFO) << cLineId-1  << RESET;
            //LOG (INFO) << cAligned << RESET;

        }
    }
    return alignedLines;
}

void ECVLinkAlignmentOT::StopWordAlignStubs(const OpticalGroup* pOpticalGroup)
{
    // disable alignment output
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, false);
    }
}

std::vector<std::pair<uint8_t, std::pair<uint8_t,bool>>> ECVLinkAlignmentOT::CheckWordAlignBEdataL1(const OpticalGroup* pOpticalGroup)
{
    std::vector<std::pair<uint8_t, std::pair<uint8_t,bool>>> ret ;

    // align L1 data in the BE
    fL1Debug = false;
    LOG(INFO) << BOLDMAGENTA << "ECVLinkAlignmentOT::WordAlignBEdata ... word alignment on L1 lines from CIC.." << RESET;
    bool cAligned0 = L1WordAlignment(pOpticalGroup, fL1Debug, 2); //If one line is not aligned it is false for both lines
    bool cAligned1 = L1WordAlignment(pOpticalGroup, fL1Debug, 1); //If one line is not aligned it is false for both lines
    ret.push_back(std::make_pair (0 ,std::make_pair( 6 ,cAligned0 ) ) );
    ret.push_back(std::make_pair (1 ,std::make_pair( 6,cAligned1 ) ) );
    return ret;
}

std::vector<std::pair<uint8_t, std::pair< uint8_t, float>>> ECVLinkAlignmentOT::StubBitErrorTest(const OpticalGroup* pOpticalGroup)
{
    std::vector<std::pair<uint8_t, std::pair< uint8_t, float>>> ret;
    LOG (INFO) << "ECV StubBitErrorTest" << RESET;
    std::vector<uint8_t> cFeEnableRegs(0);
    int hybridCount = 0;

    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, true);
        cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        hybridCount++;
    }
   
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto                             cInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    D19cDebugFWInterface*            cDebugInterface   = cInterface->getDebugInterface();
    
    uint8_t cNlines = (pOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;



    LOG (INFO) << "Start readout of stubs" << RESET;
    for(auto cHybrid: *pOpticalGroup)
    {
        std::vector<int> lineBitErrors(cNlines, 0);

        fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
        fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
        int n_events = 1000;
        for (int i = 0; i<n_events; i++)
        {
            std::vector<std::string> stubData;

            for (auto line : cDebugInterface->StubDebug(true, cNlines,false) )
            {
                stubData.push_back(line);
            }
            if (i%200 ==0)
            {
                LOG (INFO) << "Check output #Readout "<< i << RESET;
            }
            int lineCount = 0;

            for (auto line : stubData){
                if (i%200 ==0)
                {
                    LOG (INFO) << line << RESET;
                }
                line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
                //LOG (INFO) << "Line "<<(lineCount % (cNlines*hybridCount)) << " : "<<  line << RESET;
                std::bitset<32> pattern = 0xEAAAAAAA;

                std::size_t found = line.find("111");
                if (found != std::string::npos)
                {
                    std::bitset<32> toCheck(line.substr(found,32));
                    std::bitset<32> bitErrors = (pattern ^ toCheck);
                    if (toCheck.count() == 32)
                        lineBitErrors[ ( lineCount % (cNlines) ) ] += 32;

                    else
                        lineBitErrors[ ( lineCount % (cNlines) ) ] += bitErrors.count();
                }
                else
                {
                    lineBitErrors[ ( lineCount % (cNlines) ) ] += 32;
                }
                lineCount++;
            }
        }
        uint8_t line = 0;
        for (auto bitErrors : lineBitErrors)
        {
            ret.push_back(std::make_pair(cHybrid->getId(), std::make_pair(line,(float)(bitErrors)/ (float)(n_events*32)  )));
            line++;
        }
    }
    size_t cIndx = 0;
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, false);
        fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
        cIndx++;
    }

    return ret;
}

void ECVLinkAlignmentOT::InitL1BitErrorTest(const OpticalGroup* pOpticalGroup)
{
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, true);
        //cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, true);
    }

    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        fCicInterface->SetAutomaticPhaseAlignment(cCic, true);
        // configure Chips to produce phase alignment patterns
        for(auto cChip: *cHybrid)
        {
            //if(cChip->getFrontEndType() == FrontEndType::CBC3) cWithCBC = true;
            fReadoutChipInterface->producePhaseAlignmentPattern(cChip, 10);
        }
    } 
}

std::vector<std::pair<uint8_t, std::pair< uint8_t, float>>>  ECVLinkAlignmentOT::L1BitErrorTest(const OpticalGroup* pOpticalGroup)
{
    std::vector<std::pair<uint8_t, std::pair< uint8_t, float>>> ret;

    int cHybridCount = 0;
    for(auto cHybrid: *pOpticalGroup)
    {   
        LOG (INFO) << cHybrid->getId();
        cHybridCount++;
    } 

    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto                             cInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    D19cDebugFWInterface*            cDebugInterface   = cInterface->getDebugInterface();

    std::vector<int> lineBitErrors(cHybridCount, 0);


    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        if(cCic == nullptr) continue;

        // select lines for slvs debug
        fBeBoardInterface->WriteBoardReg(*cBoardIter, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
        fBeBoardInterface->WriteBoardReg(*cBoardIter, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
    
        int n_triggers = 1000;

        int last_fail_print = 0;
        for (int i = 1; i < n_triggers; i++)
        {
            std::string l1adata = cDebugInterface->L1ADebug(1, false);
            if (i%200==0)
            {
                LOG (INFO) << "L1A debug Hybrid "<< +cHybrid->getId() <<" iteration " << i << RESET;
                LOG (INFO) << l1adata << RESET;
            }
            std::size_t found = l1adata.find("111111111111111111111111111");
            std::bitset<32> pattern = ( pOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S) ? 0xAD55AAB5 : 0xAAAAAAAA;
            if (found != std::string::npos && (1600-found)>(250+32))
            {
                std::bitset<32> toCheck(l1adata.substr(found + 250,32));
                std::bitset<32> bitErrors = (pattern ^ toCheck);
                if (toCheck.count() == 32) //Line always high
                    lineBitErrors[ +cHybrid->getId() ] += 32;
                else
                    lineBitErrors[ +cHybrid->getId()  ] += bitErrors.count();
                if (bitErrors.count() > 0 && (i - last_fail_print) > 100)
                {
                    LOG (INFO) << BOLDRED<< l1adata << RESET;
                    LOG (INFO) << BOLDRED << "L1A ID " << l1adata.substr(found + 28 +9,9) << "\t" << std::stoi(l1adata.substr(found + 28 +9,9),0,2)<<RESET;
                    LOG (INFO) << i << RESET;
                    last_fail_print = i;
                }
            }
            else
            {
                lineBitErrors[ +cHybrid->getId()  ] += 32;
                if ( (1600-found)>(250+32) && (i - last_fail_print ) > 100 )
                {
                    LOG (INFO) << BOLDRED<< l1adata << RESET;
                    LOG (INFO) << BOLDRED << "L1A ID " << l1adata.substr(found + 28 +9,9) << "\t" << std::stoi(l1adata.substr(found + 28 +9,9),0,2)<<RESET;
                    LOG (INFO) << i << RESET;
                    last_fail_print = i;
                }
            }
        }
        LOG (INFO) << +cHybrid->getId() << " : " << (float)(lineBitErrors[+cHybrid->getId()])/ (float)(n_triggers*32)  << RESET;
        ret.push_back(std::make_pair(cHybrid->getId(), std::make_pair(6,(float)(lineBitErrors[+cHybrid->getId()])/ (float)(n_triggers*32))) ) ;
    }
    return ret;
}

std::vector <uint8_t> ECVLinkAlignmentOT::getGroupsAndChannels(const OpticalGroup* pOpticalGroup, bool pGroups)
{
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
        if(pOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS)
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
    if (pGroups)
        return cEportGroups;
    else
        return cEportChnls;
}


void ECVLinkAlignmentOT::StoreBERInHistogram(uint8_t pClockPolarity, uint8_t pClockStrength, uint8_t pCicStrength, uint8_t pPhase, uint8_t pHybridId, uint8_t pLine, float pBer)
{

    auto cBoard = fDetectorContainer->getFirstObject();

    DetectorDataContainer cBERContainer;
    ContainerFactory::copyAndInitHybrid<float>(*fDetectorContainer, cBERContainer);

    for(auto cOpticalGroup: *cBoard)
    {
        cBERContainer.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getObject(pHybridId)
                        ->getSummary<float>() = pBer;
    } // optical group

#ifdef __USE_ROOT__
    fDQMHistogrammer.filllBER(pClockPolarity, pClockStrength, pCicStrength, pPhase,pHybridId, pLine, cBERContainer);
#endif
}

void ECVLinkAlignmentOT::StoreWordAlignInHistogram(uint8_t pClockPolarity, uint8_t pClockStrength, uint8_t pCicStrength, uint8_t pPhase, uint8_t pHybridId, uint8_t pLine, bool pAligned)
{

    auto cBoard = fDetectorContainer->getFirstObject();

    DetectorDataContainer cAlignedWordsContainer;
    ContainerFactory::copyAndInitHybrid<bool>(*fDetectorContainer, cAlignedWordsContainer);

    for(auto cOpticalGroup: *cBoard)
    {
        //LOG (INFO) << "Store\t" << "hybrid: " << +pHybridId << " Line: " << +pLine << " aligned: " << pAligned << RESET;
        cAlignedWordsContainer.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getObject(pHybridId)
                        ->getSummary<bool>() = pAligned;
    } // optical group

#ifdef __USE_ROOT__
    fDQMHistogrammer.filllWordAlign(pClockPolarity, pClockStrength, pCicStrength, pPhase,pHybridId, pLine,cAlignedWordsContainer);
#endif
}

void ECVLinkAlignmentOT::StoreTrainedPhases(uint8_t pClockPolarity, uint8_t pClockStrength, uint8_t pCicStrength, uint8_t pHybridId, uint8_t pLine, uint8_t pPhase)
{
    auto cBoard = fDetectorContainer->getFirstObject();

    DetectorDataContainer cPhasesContainer;
    ContainerFactory::copyAndInitHybrid<uint8_t>(*fDetectorContainer, cPhasesContainer);

    for(auto cOpticalGroup: *cBoard)
    {
        cPhasesContainer.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getObject(pHybridId)
                        ->getSummary<uint8_t>() = pPhase;
        
    } // optical group
    LOG (INFO) << +pLine << RESET;
#ifdef __USE_ROOT__
    fDQMHistogrammer.filllPhases(pClockPolarity, pClockStrength, pCicStrength, pHybridId, pLine, cPhasesContainer);
#endif
}

void ECVLinkAlignmentOT::StoreChosenPhase(uint8_t pClockPolarity, uint8_t pClockStrength, uint8_t pCicStrength, uint8_t pPhase)
{
    auto cBoard = fDetectorContainer->getFirstObject();

    DetectorDataContainer cPhasesContainer;
    ContainerFactory::copyAndInitHybrid<uint8_t>(*fDetectorContainer, cPhasesContainer);

    for(auto cOpticalGroup: *cBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            cPhasesContainer.getObject(cBoard->getId())
                          ->getObject(cOpticalGroup->getId())
                          ->getObject(cHybrid->getId())
                          ->getSummary<uint8_t>() = pPhase;
        }
    } // optical group

#ifdef __USE_ROOT__
    fDQMHistogrammer.filllChosenPhase(pClockPolarity, pClockStrength, pCicStrength, cPhasesContainer);
#endif
}

// State machine control functions
void ECVLinkAlignmentOT::Running()
{
    Initialise();
    Scan();
    fSuccess = true;
    Reset();
}

void ECVLinkAlignmentOT::Stop() {}

void ECVLinkAlignmentOT::Pause() {}

void ECVLinkAlignmentOT::Resume() {}

void ECVLinkAlignmentOT::writeObjects()
{
#ifdef __USE_ROOT__
    this->SaveResults();
    fDQMHistogrammer.process();
#endif
}