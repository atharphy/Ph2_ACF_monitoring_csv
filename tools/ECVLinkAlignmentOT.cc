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
            AlignLpGBTInputs(cOpticalGroup);
            WordAlignBEdata(cOpticalGroup);
        }
        // check that word alignment of L1 data worked
        //LOG(INFO) << BOLDYELLOW << "ECVLinkAlignmentOT::Align ... trying to readout L1 data.. " << RESET;
        //ReadNEvents(cBoard, 10);
    } // align BE

    //AlignStubPackage();
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

void ECVLinkAlignmentOT::SetHybridClockPolarityAndStrength(const OpticalGroup* pOpticalGroup, bool pInverted, uint8_t pStrength)
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
        static_cast<D19clpGBTInterface*>(flpGBTInterface)->hybridClock(clpGBT, cClkCnfg, cSide);
    }
}

void ECVLinkAlignmentOT::ECV(const OpticalGroup* pOpticalGroup)
{
    std::stringstream tables;
    for (uint8_t clockPolarity = 0; clockPolarity <=1; clockPolarity++)
    {
        for (uint8_t clockStrength = 1; clockStrength <=7; clockStrength++)
        {

            SetHybridClockPolarityAndStrength(pOpticalGroup, clockPolarity == 0, clockStrength);


            for (uint8_t cicStrength = 1; cicStrength <= 5; cicStrength ++)
            {

                tables << "Clock Polarity:\t" << +clockPolarity << "\n";
                tables << "Clock Strength:\t" << +clockStrength << "\n";
                tables << "CIC Strength:\t" << +cicStrength << "\n";
                for(auto cHybrid: *pOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fCicInterface->ConfigureDriveStrength(cCic, cicStrength); 
                    
                }
                std::vector<std::vector<bool>> wordAlignment;
                std::vector<std::vector<float>> bitErrors;

                for (uint8_t phase = 0; phase <= 14; phase++)
                {
                    LOG (INFO) << BOLDRED << "CLOCK POLARITY:\t" << +clockPolarity << RESET;
                    LOG (INFO) << BOLDRED << "CLOCK STRENGTH:\t" << +clockStrength << RESET;
                    LOG (INFO) << BOLDRED << "CIC STRENGTH:\t" << +cicStrength << RESET;
                    LOG (INFO) << BOLDRED << "RX PHASE:\t" << +phase << RESET;

                    SetlpGBTRxPhase(pOpticalGroup,phase);
                    std::vector <bool> alignedLines = CheckWordAlignBEdata(pOpticalGroup);
                    wordAlignment.push_back(alignedLines);
                    std::stringstream ss;
                    for (auto line : alignedLines) ss << line << "\t";
                    LOG (INFO) << ss.str() << RESET;
                    std::vector<float> bers = BitErrorTest(pOpticalGroup); 
                    bitErrors.push_back(bers);
                    StoreValuesInHistogram(clockPolarity,clockStrength, cicStrength,phase,bers);

                }
                AlignLpGBTInputs(pOpticalGroup);
                WordAlignBEdata(pOpticalGroup, false);


                std::stringstream table = PrintECVResultTable(pOpticalGroup, wordAlignment, bitErrors);
                tables << table.str() <<"\n";
            }
        }
    }
    LOG (INFO) << "\n" << tables.str() << "\n"<<RESET;;
}

void ECVLinkAlignmentOT::SetlpGBTRxPhase(const OpticalGroup* pOpticalGroup, uint8_t pPhase)
{
    auto& clpGBT = pOpticalGroup->flpGBT;

    std::vector<uint8_t> cEportGroups = getGroupsAndChannels(pOpticalGroup, true);
    std::vector<uint8_t> cEportChnls = getGroupsAndChannels(pOpticalGroup, false);

    for(size_t cIndx = 0; cIndx < cEportGroups.size(); cIndx++) { flpGBTInterface->ConfigureRxPhase(clpGBT, cEportGroups[cIndx], cEportChnls[cIndx], pPhase); }

}

void ECVLinkAlignmentOT::StoreValuesInHistogram(uint8_t pClockPolarity, uint8_t pClockStrength, uint8_t pCicStrength, uint8_t pPhase, std::vector<float> pBers)
{

    auto cBoard = fDetectorContainer->getFirstObject();

    DetectorDataContainer cBERContainer;
    ContainerFactory::copyAndInitHybrid<std::vector<float>>(*fDetectorContainer, cBERContainer);

    for(auto cOpticalGroup: *cBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            cBERContainer.getObject(cBoard->getId())
                          ->getObject(cOpticalGroup->getId())
                          ->getObject(cHybrid->getId())
                          ->getSummary<std::vector<float>>() = pBers;            
        }
    } // optical group

#ifdef __USE_ROOT__
    fDQMHistogrammer.filllpGBTCICPlot(pClockPolarity, pClockStrength, pCicStrength, pPhase,cBERContainer);
#endif
}

std::vector<bool> ECVLinkAlignmentOT::CheckWordAlignBEdata(const OpticalGroup* pOpticalGroup)
{
    std::vector<bool> ret;
    fStubDebug      = true;
    bool cAligned   = false;
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    LOG(INFO) << BOLDYELLOW << "ECVLinkAlignmentOT::WordAlignBEdata for an OG " << RESET;
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();
    cAlignerInterface->InitializeConfiguration();
    cAlignerInterface->InitializeAlignerObject();
    LOG(INFO) << BOLDYELLOW << "ECVLinkAlignmentOT::WordAlignBEdata after debug interface " << RESET;

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
    LOG(INFO) << BOLDMAGENTA << "ECVLinkAlignmentOT::WordAlignBEdata ... word alignment on " << +cNlines << "/6 lines stub from CIC.." << RESET;
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
            cAligned                = cAlignerInterface->IsLineWordAligned();
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
            ret.push_back(cAligned);
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
    LOG(INFO) << BOLDMAGENTA << "ECVLinkAlignmentOT::WordAlignBEdata ... word alignment on L1 lines from CIC.." << RESET;
    cAligned = L1WordAlignment(pOpticalGroup, fL1Debug);
    ret.insert(ret.begin(),cAligned); //to be corrected
    ret.push_back(cAligned);
    size_t cIndx = 0;
    // re-confiure enabled FEs
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
        cIndx++;
    }
    LOG(INFO) << BOLDYELLOW << "Reached end of WordAlignBEData" << RESET;
    return ret;
}

std::vector<float> ECVLinkAlignmentOT::StubBitErrorTest(const OpticalGroup* pOpticalGroup)
{
    std::vector<float> ret;
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

    std::vector<int> lineBitErrors(cNlines*hybridCount, 0);


    LOG (INFO) << "Start readout of stubs" << RESET;
    for (int i = 0; i<1000; i++)
    {
        std::vector<std::string> stubData;
        for(auto cHybrid: *pOpticalGroup)
        {
            fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
            fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
            for (auto line : cDebugInterface->StubDebug(true, cNlines,false) )
            {
                stubData.push_back(line);
            }
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
                    lineBitErrors[ ( lineCount % (cNlines*hybridCount) ) ] += 32;

                else
                    lineBitErrors[ ( lineCount % (cNlines*hybridCount) ) ] += bitErrors.count();
            }
            else
            {
                lineBitErrors[ ( lineCount % (cNlines*hybridCount) ) ] += 32;
            }
            lineCount++;
        }
    }
    for (int i = 0; i< (cNlines*hybridCount); i++)
    {
        LOG (INFO) << i << " : " << (float)(lineBitErrors[i])/ (float)(1000*32)  << RESET;
        ret.push_back((float)(lineBitErrors[i])/ (float)(1000*32));
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

std::vector<float>  ECVLinkAlignmentOT::L1BitErrorTest(const OpticalGroup* pOpticalGroup)
{
    std::vector<float> ret;

    int cHybridCount = 0;
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
        ret.push_back((float)(lineBitErrors[+cHybrid->getId()])/ (float)(n_triggers*32));
    }
    return ret;
}

std::vector<float> ECVLinkAlignmentOT::BitErrorTest(const OpticalGroup* pOpticalGroup)
{
    std::vector<float> stubBER  = StubBitErrorTest(pOpticalGroup);
    std::vector<float> l1BER    = L1BitErrorTest(pOpticalGroup);

    std::vector<float> ret;

    if ( pOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S)
    {
        ret.push_back(l1BER[0]);
        for(auto ber : stubBER)
            ret.push_back(ber);
        ret.push_back(l1BER[1]);
    }
    if ( pOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS)
    {
        ret.push_back(stubBER[0]);
        ret.push_back(stubBER[1]);
        ret.push_back(stubBER[2]);
        ret.push_back(stubBER[3]);
        ret.push_back(stubBER[4]);
        ret.push_back(stubBER[5]);
        ret.push_back(l1BER[0]);
        ret.push_back(stubBER[6]);
        ret.push_back(stubBER[7]);
        ret.push_back(stubBER[8]);
        ret.push_back(stubBER[9]);
        ret.push_back(stubBER[10]);
        ret.push_back(stubBER[11]);
        ret.push_back(l1BER[1]);
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

std::stringstream ECVLinkAlignmentOT::PrintECVResultTable(const OpticalGroup* pOpticalGroup, std::vector<std::vector<bool>> pWordAlignment, std::vector<std::vector<float>> pBitErrors)
{
    std::stringstream ret;
    std::vector<uint8_t> cEportGroups = getGroupsAndChannels(pOpticalGroup, true);
    std::vector<uint8_t> cEportChnls = getGroupsAndChannels(pOpticalGroup, false);

    ret << BOLDGREEN << "ECV Result Table\n" << RESET;
    //D19clpGBTInterface* clpGBTInterface    = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    //ret << BOLDYELLOW << "Trained lpGBT Phases: " << RESET;
    //for (auto a : flpGBTInterface->fTrainedPhases) ret << +a << "\t";
    //ret << "\n";
    ret << "Type\tL1_R\tStub1_R\tStub2_R\tStub3_R\tStub4_R\tStub5_R\tStub1_L\tStub2_L\tStub3_L\tStub4_L\tStub5_L\tL1_L\n";
    ret << "Phase\t";
    for(size_t cIndx = 0; cIndx < cEportGroups.size(); cIndx++) ret << +cEportGroups[cIndx] << ":" << +cEportChnls[cIndx] << "\t";
    ret << "\n";
    for (int phase = 0; phase < (int) pWordAlignment.size(); phase ++)
    {
        std::stringstream line;
        if (flpGBTInterface->fTrainedPhases.back() == phase)
            line << BOLDBLUE << phase << RESET << "\t";
        else
            line << phase << "\t";

        //for (auto entry : pWordAlignment[phase]) line << entry << "\t";
        int chn = 0;
        for (auto error : pBitErrors[phase])
        {
            if (pWordAlignment[phase][chn] && error == 0)
                line <<BOLDGREEN;
            if (pWordAlignment[phase][chn] && error > 0)
                line <<BOLDYELLOW;
            if (!pWordAlignment[phase][chn])
                line <<BOLDRED;
            if (flpGBTInterface->fTrainedPhases[chn] ==phase)
                line << BOLDBLUE;
            line << (int)(error*1000) /1000.0 << RESET <<"\t";


            chn++;
        }
        ret<< line.str() << "\n" << RESET;
    }
    return ret;
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