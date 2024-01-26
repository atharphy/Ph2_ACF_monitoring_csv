#include "tools/OTalignBoardDataWord.h"
#include "HWDescription/BeBoard.h"
#include "HWDescription/Hybrid.h"
#include "HWDescription/OpticalGroup.h"
#include "HWInterface/D19cBackendAlignmentFWInterface.h"
#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/ExceptionHandler.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerFactory.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTalignBoardDataWord::fCalibrationDescription = "Align triggered data on to decode events";

OTalignBoardDataWord::OTalignBoardDataWord() : Tool() {}

OTalignBoardDataWord::~OTalignBoardDataWord() {}

void OTalignBoardDataWord::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeBoardRegister("fc7_daq_stat\\.physical_interface_block\\.phase_tuning_reply");
    // free the registers in case any

    size_t               numberOfLines = (fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
    std::vector<uint8_t> initialBitSlipVector(numberOfLines, 0);
    ContainerFactory::copyAndInitHybrid<std::vector<uint8_t>>(*fDetectorContainer, fBeBitSlip, initialBitSlipVector);

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTalignBoardDataWord.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTalignBoardDataWord::ConfigureCalibration() {}

void OTalignBoardDataWord::Running()
{
    LOG(INFO) << "Starting OTalignBoardDataWord measurement.";
    Initialise();
    WordAlignBEdata();
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

void OTalignBoardDataWord::Pause() {}

void OTalignBoardDataWord::Resume() {}

void OTalignBoardDataWord::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTalignBoardDataWord::WordAlignBEdata()
{
    LOG(INFO) << BOLDYELLOW << "OTalignBoardDataWord::WordAlignBEdata" << RESET;

    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            bool cAligned = this->WordAlignBEdata(theOpticalGroup);
            if(!cAligned)
            {
                LOG(INFO) << BOLDRED << "Could not word align-BE data in OTalignBoardDataWord on Board id " << +theBoard->getId() << " OpticalGroup id" << +theOpticalGroup->getId()
                          << " --- OpticalGroup will be disabled" << RESET;
                ExceptionHandler::getInstance()->disableOpticalGroup(theBoard->getId(), theOpticalGroup->getId());
                continue;
            }
        } // optical groups connected to this  board
    }
}

bool OTalignBoardDataWord::WordAlignBEdata(const OpticalGroup* theOpticalGroup)
{
    bool isStubDebug = true;
    bool cAligned    = false;
    auto theBoardId  = theOpticalGroup->getBeBoardId();
    auto theBoard    = fDetectorContainer->getObject(theBoardId);
    LOG(INFO) << BOLDYELLOW << "LinkAlignmentOT::WordAlignBEdata for an OG " << RESET;
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    fBeBoardInterface->setBoard(theBoardId);
    D19cDebugFWInterface*            cDebugInterface   = cInterface->getDebugInterface();
    D19cBackendAlignmentFWInterface* cAlignerInterface = cInterface->getBackendAlignmentInterface();
    cAlignerInterface->InitializeConfiguration();
    cAlignerInterface->InitializeAlignerObject();
    LOG(INFO) << BOLDYELLOW << "LinkAlignmentOT::WordAlignBEdata after debug interface " << RESET;

    auto& cBeBitSlip   = fBeBitSlip.getObject(theBoardId);
    auto& cBeBitSlipOG = cBeBitSlip->getObject(theOpticalGroup->getId());

    // configure CICs to output alignment pattern on L1 lines
    std::vector<uint8_t> cFeEnableRegs(0);
    for(auto cHybrid: *theOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, true);
        cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
    }
    // stop triggers to make sure that there are no L1 packets from the CIC
    fBeBoardInterface->Stop(theBoard);

    // align stub lines in the BE
    size_t cNlines = (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::WordAlignBEdata ... word alignment on " << +cNlines << "/6 lines stub from CIC.." << RESET;
    for(size_t cLineId = 1; cLineId <= cNlines; cLineId++)
    {
        for(auto cHybrid: *theOpticalGroup)
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
            cAligned = cAlignerInterface->IsLineWordAligned();
            cThisBeBitSlip[cLineId] = cAlignerInterface->GetLineConfiguration().fBitslip;
            if(!cAligned)
            {
                if(((cHybrid->getId() % 2) == 0) & ((cLineId - 1) == 4) & (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S))
                {
                    LOG(INFO) << BOLDYELLOW << "Attention! ignoring alignment failure on right hybrid CIC line 4 due to bug in kickoff SEH!" << RESET;
                    continue;
                } // CIC_OUT_4_R will always fail for kick-off SEH, ignore here to keep allowing noise measurements
                LOG(INFO) << BOLDRED << "Could not word align-BE data in LinkAlignmentOT on Board id " << +theBoardId << " OpticalGroup id" << +theOpticalGroup->getId() << " Hybrid id"
                          << +cHybrid->getId() << " stub line " << +(cLineId - 1) << " --- Hybrid will be disabled" << RESET;
                ExceptionHandler::getInstance()->disableHybrid(theBoardId, theOpticalGroup->getId(), cHybrid->getId());
                continue;
            }
        }
    }
    // check for 0 bit slips
    for(auto cHybrid: *theOpticalGroup)
    {
        auto&                cBeBitSlipHybrd = cBeBitSlipOG->getObject(cHybrid->getId());
        auto&                cThisBeBitSlip  = cBeBitSlipHybrd->getSummary<std::vector<uint8_t>>();
        std::vector<uint8_t> cBitSlipHist(15, 0);
        for(auto cItem: cThisBeBitSlip) cBitSlipHist[cItem]++;
        auto cMode = std::max_element(cBitSlipHist.begin(), cBitSlipHist.end()) - cBitSlipHist.begin();
        LOG(INFO) << BOLDMAGENTA << "Hybrid#" << +cHybrid->getId() << " most frequent bitslip is " << +cMode << RESET;
    }

    if(isStubDebug)
    {
        for(auto cHybrid: *theOpticalGroup)
        {
            LOG(INFO) << BOLDMAGENTA << "Stub debug output - hybrid#" << +cHybrid->getId() << RESET;
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
            for(size_t cIter = 0; cIter < 1; cIter++)
            {
                LOG(INFO) << BOLDYELLOW << "Debug capture Iteration#" << cIter << RESET;
                cDebugInterface->StubDebug(true, cNlines);
            }
        }
    }

    // disable stub output
    for(auto cHybrid: *theOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, false);
    }
    // align L1 data in the BE
    bool fL1Debug = true;
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::WordAlignBEdata ... word alignment on L1 lines from CIC.." << RESET;
    cAligned = L1WordAlignment(theOpticalGroup, fL1Debug);

    size_t cIndx = 0;
    // re-confiure enabled FEs
    for(auto cHybrid: *theOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
        cIndx++;
    }
    LOG(INFO) << BOLDYELLOW << "Reached end of WordAlignBEData" << RESET;
    return cAligned;
}

bool OTalignBoardDataWord::L1WordAlignment(const OpticalGroup* pOpticalGroup, bool pScope)
{
    auto theBoardId = pOpticalGroup->getBeBoardId();
    auto theBoard   = fDetectorContainer->getObject(theBoardId);
    fBeBoardInterface->setBoard(theBoardId);
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
    fBeBoardInterface->WriteBoardMultReg(theBoard, cVecReg);

    auto& cBeBitSlip   = fBeBitSlip.getObject(theBoardId);
    auto& cBeBitSlipOG = cBeBitSlip->getObject(pOpticalGroup->getId());

    bool cAllowZeroBitslip = true;
    LOG(INFO) << BOLDBLUE << "Aligning the back-end to properly decode L1A data coming from the front-end objects." << RESET;
    fBeBoardInterface->ChipReSync(theBoard);
    fBeBoardInterface->Start(theBoard);
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
        cAlignerObjct.fHybrid  = cHybrid->getId();
        cAlignerObjct.fChip    = 0;
        cAlignerObjct.fLine    = cLineId;
        cAlignerObjct.fOptical = cOptical ? 1 : 0;
        LineConfiguration cLineCnfg;
        cLineCnfg.fPattern    = cPattern;
        cLineCnfg.fMode       = 0;
        cLineCnfg.fEnableL1   = 0;
        cLineCnfg.fMasterLine = 0;
        cAlignerInterface->ManuallyConfigureLine(cAlignerObjct, cLineCnfg);

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
            cLineStatus.first  = cAlignerInterface->IsLineWordAligned();
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

                    auto        cWords   = fBeBoardInterface->ReadBlockBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.l1a_debug", 50);
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
                        fBeBoardInterface->Start(theBoard);
                    }
                    // this->ResetReadout();
                }
                cIterCount++;
            } while(cIterCount < cMaxIters && !cSuccess);
        }
    }
    fBeBoardInterface->Stop(theBoard);

    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        if(cCic == nullptr || !pScope) continue;

        // select lines for slvs debug
        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
        cDebugInterface->L1ADebug();
    }

    return cSuccess;
}

std::pair<bool, uint8_t> OTalignBoardDataWord::PhaseTuneLine(const Chip* pChip, uint8_t pLineId)
{
    std::pair<bool, uint8_t> cLineStatus;
    cLineStatus.first  = false;
    cLineStatus.second = 0;

    auto cBoardId   = pChip->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    LOG(DEBUG) << BOLDYELLOW << "OTalignBoardDataWord::PhaseTuneLine#" << +pLineId << " for a Chip#" << +pChip->getId() << RESET;
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
    cLineStatus.first = cAlignerInterface->IsLinePhaseAligned();
    if(!cLineStatus.first)
    {
        LOG(INFO) << BOLDRED << "Could not phase align-BE data in OTalignBoardDataWord on Board id " << +cBoardId << " OpticalGroup id" << +pChip->getOpticalGroupId() << " Hybrid id "
                  << +pChip->getHybridId() << " Chip id " << +pChip->getId() << " line# " << +pLineId << " --- Chip will be disabled" << RESET;
        ExceptionHandler::getInstance()->disableChip(+cBoardId, pChip->getOpticalGroupId(), pChip->getHybridId(), pChip->getId());
    }

    cLineStatus.second = cAlignerInterface->GetLineConfiguration().fDelay;
    return cLineStatus;
}

void OTalignBoardDataWord::ManuallyConfigureLine(const Chip* pChip, uint8_t pLineId, uint8_t pPhase, uint8_t pBitslip)
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
    cAlignerInterface->ManuallyConfigureLine(cAlignerObjct, cLineCnfg);
}
