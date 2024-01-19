#include "tools/OTalignBoardDataWord.h"
#include "System/RegisterHelper.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTalignBoardDataWord::fCalibrationDescription = "Insert brief calibration description here";

OTalignBoardDataWord::OTalignBoardDataWord() : Tool() {}

OTalignBoardDataWord::~OTalignBoardDataWord() {}

void OTalignBoardDataWord::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTalignBoardDataWord.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTalignBoardDataWord::ConfigureCalibration()
{

}

void OTalignBoardDataWord::Running()
{
    LOG(INFO) << "Starting OTalignBoardDataWord measurement.";
    Initialise();
    LOG(INFO) << "Done with OTalignBoardDataWord.";
    Reset();
}

void OTalignBoardDataWord::Stop(void)
{
    LOG(INFO) << "Stopping OTalignBoardDataWord measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTalignBoardDataWord.process();
    #endif
    dumpConfigFiles();
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTalignBoardDataWord stopped.";
}

void OTalignBoardDataWord::Pause()
{

}


void OTalignBoardDataWord::Resume()
{

}


void OTalignBoardDataWord::Reset()
{
    fRegisterHelper->restoreSnapshot();
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
            cAlignerObjct.fHybrid  = cHybrid->getId();
            cAlignerObjct.fChip    = 0;
            cAlignerObjct.fLine    = cLineId;
            cAlignerObjct.fOptical = 1;
            LineConfiguration cLineCnfg;
            cLineCnfg.fPattern       = 0xEA;
            cLineCnfg.fPatternPeriod = 8;
            cAlignerInterface->AlignWord(cAlignerObjct, cLineCnfg, true);
            cAligned                = cAlignerInterface->IsLineWordAligned();
            cThisBeBitSlip[cLineId] = cAlignerInterface->GetLineConfiguration().fBitslip;

            if(!cAligned)
            {
                if(((cHybrid->getId() % 2) == 0) & ((cLineId - 1) == 4) & (pOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S))
                { continue; } // CIC_OUT_4_R will always fail for kick-off SEH, ignore here to keep allowing noise measurements
                LOG(INFO) << BOLDRED << "Could not word align-BE data in LinkAlignmentOT on Board id " << +cBoardId << " OpticalGroup id" << +pOpticalGroup->getId() << " Hybrid id"
                          << +cHybrid->getId() << " stub line " << +(cLineId - 1) << " --- Hybrid will be disabled" << RESET;
                ExceptionHandler::getInstance()->disableHybrid(cBoardId, pOpticalGroup->getId(), cHybrid->getId());
                continue;
            }
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
                if(cThisBeBitSlip[cLineId] == 0)
                {
                    LOG(INFO) << BOLDRED << "Bitslip of 0 found for BE-stub data on Board id " << +cBoardId << " OpticalGroup id" << +pOpticalGroup->getId() << " Hybrid id" << +cHybrid->getId()
                              << " stub line " << +(cLineId - 1) << " --- Hybrid will be disabled" << RESET;
                    ExceptionHandler::getInstance()->disableHybrid(cBoardId, pOpticalGroup->getId(), cHybrid->getId());
                    continue;
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
