#include "PedeNoiseTime.h"
#include "../HWDescription/Cbc.h"
#include "../HWDescription/SSA.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/MPAChannelGroupHandler.h"
#include "../Utils/Occupancy.h"
#include "../Utils/SSAChannelGroupHandler.h"
#include "../Utils/ThresholdAndNoise.h"
#include "boost/format.hpp"
#include <math.h>

#ifdef __USE_ROOT__
#include "../DQMUtils/DQMHistogramPedeNoise.h"
#endif

PedeNoiseTime::PedeNoiseTime() : Tool() {}

PedeNoiseTime::~PedeNoiseTime() { clearDataMembers(); }

void PedeNoiseTime::cleanContainerMap()
{
    for(auto container: fSCurveOccupancyMap) fRecycleBin.free(container.second);
    fSCurveOccupancyMap.clear();
}

void PedeNoiseTime::clearDataMembers()
{
    delete fThresholdAndNoiseContainer;
    delete fStubLogicValue;
    delete fHIPCountValue;
    cleanContainerMap();
}

void PedeNoiseTime::Initialise(bool pAllChan, bool pDisableStubLogic)
{
    fDisableStubLogic = pDisableStubLogic;

    ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0));
    cWithCBC                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
    cWithSSA                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
    cWithMPA                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);

    if(cWithCBC)
    {
        CBCChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(16, 2); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler);
    }
    if(cWithSSA)
    {
        SSAChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler, FrontEndType::SSA);
        setChannelGroupHandler(theChannelGroupHandler, FrontEndType::SSA2);
    }
    if(cWithMPA)
    {
        MPAChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler, FrontEndType::MPA);
        setChannelGroupHandler(theChannelGroupHandler, FrontEndType::MPA2);
    }

    initializeRecycleBin();
    fAllChan = pAllChan;

    fSkipMaskedChannels          = findValueInSettings<double>("SkipMaskedChannels", 0);
    fMaskChannelsFromOtherGroups = findValueInSettings<double>("MaskChannelsFromOtherGroups", 1);
    fPlotSCurves                 = findValueInSettings<double>("PlotSCurves", 0);
    fFitSCurves                  = findValueInSettings<double>("FitSCurves", 0);
    fPulseAmplitude              = findValueInSettings<double>("PedeNoisePulseAmplitude", 0);
    fEventsPerPoint              = findValueInSettings<double>("Nevents", 10);
    fNEventsPerBurst             = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : -1;
    LOG(INFO) << "Parsed settings:";
    LOG(INFO) << " Nevents = " << fEventsPerPoint;

    this->SetSkipMaskedChannels(fSkipMaskedChannels);
    if(fFitSCurves) fPlotSCurves = true;

    // for now.. force to use async mode here
    bool cForcePSasync = true;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto cType    = FrontEndType::SSA;
                bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                cType         = FrontEndType::MPA;
                bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                if(!cWithSSA && !cWithMPA) continue;

                if(!cForcePSasync) continue;

                cBoard->setEventType(EventType::PSAS);
                LOG(INFO) << BOLDMAGENTA << "PedeNoiseTime::Initialise Setting event type to PSAS" << RESET;
                // set all SSAs + MPAs to output data in async mode
                for(auto cROC: *cHybrid)
                {
                    // TBC - what about MPA here?
                    fReadoutChipInterface->WriteChipReg(cROC, "AnalogueAsync", 1);
                }
            }
        }
    }
#ifdef __USE_ROOT__
    fDQMHistogramPedeNoise.book(fResultFile, *fDetectorContainer, fSettingsMap);
    for(auto cBoard: *fDetectorContainer)
    {
        TString  cName = Form("DataLog_BeBoard%d", cBoard->getId());
        TObject* cObj  = gROOT->FindObject(cName);
        if(cObj) delete cObj;

        TTree* cTree = new TTree(cName, "DataLog");
        cTree->Branch("EventCnt", &fEvent.fEventCnt);
        cTree->Branch("EvntId", &fEvent.fEventId);
        cTree->Branch("EventLoss", &fEvent.fEventLoss);
        cTree->Branch("Latency", &fEvent.fL1Latency);
        cTree->Branch("L1Id", &fEvent.fL1Id);
        cTree->Branch("L1Mismatch", &fEvent.fL1Mismatch);
        cTree->Branch("HybridId", &fEvent.fHybridId);
        cTree->Branch("ChipId", &fEvent.fChipId);
        cTree->Branch("Threshold", &fEvent.fThreshold);
        cTree->Branch("Hits", &fEvent.fHits);
        this->bookHistogram(cBoard, "DataLog", cTree);

        cName = Form("PedeNoiseSummary_BeBoard%d", cBoard->getId());
        cObj  = gROOT->FindObject(cName);
        if(cObj) delete cObj;

        cTree = new TTree(cName, "PedeNoiseSummary");
        cTree->Branch("Latency", &fEvent.fL1Latency);
        cTree->Branch("HybridId", &fEvent.fHybridId);
        cTree->Branch("ChipId", &fEvent.fChipId);
        cTree->Branch("Pedestal", &fPedestalStats.fMean);
        cTree->Branch("Noise", &fNoiseStats.fMean);
        cTree->Branch("PedestalError", &fPedestalStats.fStdDev);
        cTree->Branch("NoiseError", &fNoiseStats.fStdDev);
        cTree->Branch("PedestalMax", &fPedestalStats.fMax);
        cTree->Branch("NoiseMax", &fNoiseStats.fMax);
        cTree->Branch("PedestalMin", &fPedestalStats.fMin);
        cTree->Branch("NoiseMin", &fNoiseStats.fMin);
        this->bookHistogram(cBoard, "PedeNoiseSummary", cTree);
    }
#endif
    ContainerFactory::copyAndInitBoard<uint32_t>(*fDetectorContainer, fTriggerCounter);
}

void PedeNoiseTime::disableStubLogic()
{
    fStubLogicValue = new DetectorDataContainer();
    fHIPCountValue  = new DetectorDataContainer();
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *fStubLogicValue);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *fHIPCountValue);

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cROC: *cHybrid)
                {
                    if(cROC->getFrontEndType() == FrontEndType::CBC3)
                    {
                        LOG(INFO) << BOLDBLUE << "Chip Type = CBC3 - thus disabling Stub logic for pedestal and noise measurement." << RESET;
                        fStubLogicValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cROC->getIndex())->getSummary<uint16_t>() =
                            fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(cROC), "Pipe&StubInpSel&Ptwidth");
                        fHIPCountValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cROC->getIndex())->getSummary<uint16_t>() =
                            fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(cROC), "HIP&TestMode");
                        fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cROC), "Pipe&StubInpSel&Ptwidth", 0x23);
                        fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cROC), "HIP&TestMode", 0x00);
                    }
                }
            }
        }
    }
}

void PedeNoiseTime::reloadStubLogic()
{
    // re-enable stub logic

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cROC: *cHybrid)
                {
                    RegisterVector cRegVec;
                    if(cROC->getFrontEndType() == FrontEndType::CBC3)
                    {
                        LOG(INFO) << BOLDBLUE << "Chip Type = CBC3 - re-enabling stub logic to original value!" << RESET;
                        cRegVec.push_back(
                            {"Pipe&StubInpSel&Ptwidth", fStubLogicValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cROC->getIndex())->getSummary<uint16_t>()});
                        cRegVec.push_back(
                            {"HIP&TestMode", fHIPCountValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cROC->getIndex())->getSummary<uint16_t>()});
                        fReadoutChipInterface->WriteChipMultReg(cROC, cRegVec);
                    }
                }
            }
        }
    }
}

void PedeNoiseTime::sweepSCurves()
{
    uint16_t cStartValue = 0;
    if(cWithSSA) cStartValue = 40;
    if(cWithMPA) cStartValue = 50;
    bool originalAllChannelFlag = this->fAllChan;

    if(fPulseAmplitude != 0 && originalAllChannelFlag && cWithCBC)
    {
        this->SetTestAllChannels(false);
        LOG(INFO) << RED << "Cannot inject pulse for all channels, test in groups enabled. " << RESET;
    }

    // configure TP amplitude
    for(auto cBoard: *fDetectorContainer)
    {
        if(cWithSSA || cWithMPA)
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "InjectedCharge", fPulseAmplitude);
        else
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "TestPulsePotNodeSel", fPulseAmplitude);
    }
    if(fPulseAmplitude != 0)
    {
        this->enableTestPulse(true);
        setFWTestPulse();
        LOG(INFO) << BLUE << "Enabled test pulse. " << RESET;
        cStartValue = this->findPedestal();
    }
    else
    {
        this->enableTestPulse(false);
        cStartValue = this->findPedestal(true);
    }

    if(fDisableStubLogic) disableStubLogic();
    // LOG (INFO) << BLUE <<  "SV " <<cStartValue<< RESET ;

    measureSCurves(cStartValue);

    if(fDisableStubLogic) reloadStubLogic();

    this->SetTestAllChannels(originalAllChannelFlag);
    if(fPulseAmplitude != 0)
    {
        this->enableTestPulse(false);
        if(cWithSSA)
            setSameGlobalDac("InjectedCharge", 0);
        else if(cWithMPA)
        {
            setSameGlobalDac("CalDAC0", 0);
            setSameGlobalDac("CalDAC1", 0);
            setSameGlobalDac("CalDAC2", 0);
            setSameGlobalDac("CalDAC3", 0);
            setSameGlobalDac("CalDAC4", 0);
            setSameGlobalDac("CalDAC5", 0);
            setSameGlobalDac("CalDAC6", 0);
        }
        else
            setSameGlobalDac("TestPulsePotNodeSel", 0);

        LOG(INFO) << BLUE << "Disabled test pulse. " << RESET;
    }

    LOG(INFO) << BOLDBLUE << "Finished sweeping SCurves..." << RESET;
    return;
}

void PedeNoiseTime::measureNoise()
{
    findPedestal(true);
    // LOG(INFO) << BOLDBLUE << "sweepSCurves" << RESET;
    // sweepSCurves();
    // LOG(INFO) << BOLDBLUE << "extractPedeNoiseTime" << RESET;
    // extractPedeNoiseTime();
    // LOG(INFO) << BOLDBLUE << "producePedeNoiseTimePlots" << RESET;
    // producePedeNoiseTimePlots();
    // LOG(INFO) << BOLDBLUE << "Done" << RESET;
}

void PedeNoiseTime::Validate(uint32_t pNoiseStripThreshold, uint32_t pMultiple)
{
    LOG(INFO) << "Validation: Taking Data with " << fEventsPerPoint * pMultiple << " random triggers!";

    for(auto cBoard: *fDetectorContainer)
    {
        // increase threshold to supress noise
        setThresholdtoNSigma(cBoard, 5);
    }
    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    bool originalAllChannelFlag = this->fAllChan;

    this->SetTestAllChannels(true);
    this->measureData(fEventsPerPoint * pMultiple);
    this->SetTestAllChannels(originalAllChannelFlag);
#ifdef __USE_ROOT__
    fDQMHistogramPedeNoise.fillValidationPlots(theOccupancyContainer);
    // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fDQMStreamerEnabled << std::endl;
#else
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    auto theOccupancyStream = prepareHybridContainerStreamer<Occupancy, Occupancy, Occupancy>();
    // auto theOccupancyStream = prepareChannelContainerStreamer<Occupancy>();

    LOG(INFO) << "6 ";
    for(auto board: theOccupancyContainer)
    {
        if(fDQMStreamerEnabled) theOccupancyStream->streamAndSendBoard(board, fDQMStreamer);
    }
#endif
    LOG(INFO) << "7 ";
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                // std::cout << __PRETTY_FUNCTION__ << " The Hybrid Occupancy = " <<
                // theOccupancyContainer.at(cBoard->getIndex())->at(cFe->getIndex())->getSummary<Occupancy,Occupancy>().fOccupancy
                // << std::endl;

                for(auto cROC: *cFe)
                {
                    RegisterVector cRegVec;
                    uint32_t       NCH = NCHANNELS;
                    if(cWithSSA) NCH = NSSACHANNELS;
                    if(cWithMPA) NCH = NMPACHANNELS;
                    for(uint32_t iChan = 0; iChan < NCH; iChan++)
                    {
                        // LOG (INFO) << RED << "Ch " << iChan << RESET ;
                        float occupancy =
                            theOccupancyContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cFe->getIndex())->at(cROC->getIndex())->getChannel<Occupancy>(iChan).fOccupancy;
                        if(occupancy > float(pNoiseStripThreshold * 0.001))
                        {
                            if(cWithCBC)
                            {
                                // char cRegName[11];
                                // sprintf(cRegName, "Channel%03d", iChan + 1);
                                std::string cRegName = "Channel" + (boost::format("%|03|") % (iChan + 1)).str();
                                cRegVec.push_back({cRegName, 0xFF});
                            }
                            if(cROC->getFrontEndType() == FrontEndType::SSA)
                            {
                                // char cRegName[17];
                                // sprintf(cRegName, "THTRIMMING_S%03d", iChan + 1);
                                std::string cRegName = "THTRIMMING_S" + (boost::format("%|03|") % (iChan + 1)).str();
                                cRegVec.push_back({cRegName, 0x1F});
                            }
                            if((cROC->getFrontEndType() == FrontEndType::MPA))
                            {
                                // char cRegName[12];
                                // sprintf(cRegName, "TrimDAC_P%04d", iChan + 1);
                                std::string cRegName = "TrimDAC_P" + (boost::format("%|04|") % (iChan + 1)).str();
                                cRegVec.push_back({cRegName, 0x1F});
                            }
                            LOG(INFO) << RED << "Found a noisy channel on ROC " << +cROC->getId() << " Channel " << iChan << " with an occupancy of " << occupancy << "; setting offset to " << +0xFF
                                      << RESET;
                        }
                    }

                    fReadoutChipInterface->WriteChipMultReg(cROC, cRegVec);
                }
            }
        }
        setThresholdtoNSigma(cBoard, 0);
    }
}

uint16_t PedeNoiseTime::findPedestal(bool forceAllChannels)
{
    bool originalAllChannelFlag = this->fAllChan;
    if(forceAllChannels) this->SetTestAllChannels(true);

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    if(cWithCBC) this->bitWiseScan("VCth", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(cWithSSA) this->bitWiseScan("Bias_THDAC", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(cWithMPA) this->bitWiseScan("ThDAC_ALL", fEventsPerPoint, 0.56, fNEventsPerBurst);

    if(forceAllChannels) this->SetTestAllChannels(originalAllChannelFlag);

    float    cMean = 0.;
    uint32_t nCbc  = 0;

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cROC: *cFe)
                {
                    uint16_t tmpVthr = 0;
                    if(cWithCBC) tmpVthr = (static_cast<ReadoutChip*>(cROC)->getReg("VCth1") + (static_cast<ReadoutChip*>(cROC)->getReg("VCth2") << 8));
                    if(cWithSSA) tmpVthr = static_cast<ReadoutChip*>(cROC)->getReg("Bias_THDAC");
                    if(cWithMPA) tmpVthr = static_cast<ReadoutChip*>(cROC)->getReg("ThDAC0");

                    cMean += tmpVthr;
                    ++nCbc;
                }
            }
        }
    }

    cMean /= nCbc;

    LOG(INFO) << BOLDBLUE << "Found Pedestals to be around " << BOLDRED << cMean << RESET;

    return cMean;
}

// generic analogue injections - mean trigger rate set by
// pTriggerSeparation
void PedeNoiseTime::GenericTriggers(size_t pNtriggersToSend, int pTriggerSeparation, int pMaxBurstLength)
{
    size_t cReSyncSep = 100;
    // random c++
    std::srand(std::time(NULL));
    std::random_device                 cRndm{};
    std::mt19937                       cGen{cRndm()};
    std::poisson_distribution<>        cDistTrigSep(pTriggerSeparation);
    std::uniform_int_distribution<int> cBurstDist(1, pMaxBurstLength); // maximum length of trigger train

    size_t fSizeGap = 20;
    fFastCommands.clear();
    fNInjectedTriggers = 0;
    for(size_t cIndx = 0; cIndx < 2 * fSizeGap; cIndx++) { fFastCommands.push_back(fFCMDs.fEmpty); }
    // send a resync + BC0
    // need this to clear the L1 counters
    fFastCommands.push_back(fFCMDs.fBC0);
    fFastCommands.push_back(fFCMDs.fEmpty);
    // gap until start of triggers
    size_t cBxId     = 0;
    size_t cMaxDepth = 16382;
    for(size_t cBx = 0; cBx < cReSyncSep; cBx++)
    {
        if(fFastCommands.size() == cMaxDepth) continue;
        fFastCommands.push_back(fFCMDs.fEmpty);
        cBxId++;
    }
    do
    {
        size_t cBurstLength = cBurstDist(cGen); //
        for(size_t cBx = 0; cBx < cBurstLength; cBx++)
        {
            if(fNInjectedTriggers >= pNtriggersToSend) continue;
            if(fFastCommands.size() == cMaxDepth) continue;

            fFastCommands.push_back(fFCMDs.fTrigger);
            fTriggeredBxs.push_back(cBxId);
            fIters.push_back(fEvent.fIter);
            fNInjectedTriggers++;
            cBxId++;
        }
        // how many clocks to wait until
        // next trigger
        size_t cTriggerGap = std::round(cDistTrigSep(cGen));
        for(size_t cBx = 0; cBx < cTriggerGap; cBx++)
        {
            if(fFastCommands.size() == cMaxDepth) continue;

            fFastCommands.push_back(fFCMDs.fEmpty);
            cBxId++;
        }
    } while((size_t)fNInjectedTriggers < pNtriggersToSend && fFastCommands.size() != cMaxDepth);
    for(size_t cBx = 0; cBx < 10; cBx++)
    {
        if(fFastCommands.size() == cMaxDepth) continue;
        fFastCommands.push_back(fFCMDs.fEmpty);
        cBxId++;
    }
}
uint32_t PedeNoiseTime::GenericTriggerConfig(BeBoard* pBoard, int cNrepetitions)
{
    LOG(DEBUG) << BOLDMAGENTA << "PedeNoiseTime setting TriggerConfig " << RESET;

    // repeat the sequence N times
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.generic_fcmd.number_of_repetitions", cNrepetitions);
    // make sure fast command duration is 0
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control.fast_duration", 0x0);

    // make sure I accept all trgigers
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", fNInjectedTriggers * cNrepetitions);

    // when using generic fast commands I want to read data
    // so first stop the trigger sources in the FC7
    uint32_t cNWords    = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.readout_block.general.words_cnt");
    uint32_t cNtriggers = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
    LOG(DEBUG) << BOLDMAGENTA << "Before stopping triggers : " << +cNWords << " words in the readout and " << +cNtriggers << " triggers in the trigger_in counter." << RESET;

    // this stops triggers and
    // re-loads the configuration
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetTriggerFSM();

    cNWords    = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.readout_block.general.words_cnt");
    cNtriggers = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
    LOG(DEBUG) << BOLDMAGENTA << "After stopping triggers [no reset]: " << +cNWords << " words in the readout and " << +cNtriggers << " triggers in the trigger_in counter " << RESET;

    // make sure data handshake is disabled
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x00);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // make sure this is set - then I am sure I only accept exactly as many triggers as I have sent
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", fNInjectedTriggers * cNrepetitions);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // re-load configuration
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetTriggerFSM();
    return fNInjectedTriggers * cNrepetitions;
}
bool PedeNoiseTime::SendGenericTriggers(size_t pNtriggersToSend, int pTriggerSeparation)
{
    auto                  cMaxTriggersInBurst        = findValueInSettings<double>("MaxNtriggersPerBurst", 1);
    auto                  cNtriggersToSendPerAttempt = findValueInSettings<double>("Ntriggers", 10);
    DetectorDataContainer cNwordsContainer;
    ContainerFactory::copyAndInitBoard<uint32_t>(*fDetectorContainer, cNwordsContainer);

    for(auto cBoard: *fDetectorContainer)
    {
        auto& cNtriggers = fTriggerCounter.at(cBoard->getIndex())->getSummary<uint32_t>();
        cNtriggers       = 0;
        auto& cNwords    = cNwordsContainer.at(cBoard->getIndex())->getSummary<uint32_t>();
        cNwords          = 0;
    } // make sure all start at 0

    fPerAttempt          = cNtriggersToSendPerAttempt;
    size_t cNrepetitions = 1 + std::floor(pNtriggersToSend / fPerAttempt);
    fReps                = cNrepetitions;
    // LOG (DEBUG) << BOLDMAGENTA << "Generic block used to send " << +pNtriggersToSend << " triggers..."
    //     << " by sending " << cNrepetitions << " blocks of fast command sequences containing " << fPerAttempt
    //     << " triggers each."
    //     << RESET;

    // this means that I should send 500 triggers at a time
    bool cContinue = true;
    fEvent.fIter   = 0;
    fIters.clear();
    fTriggeredBxs.clear();
    fTriggersSent = 0;
    do
    {
        bool cSuccess = true;
        GenericTriggers(cNtriggersToSendPerAttempt, pTriggerSeparation, cMaxTriggersInBurst);
        // LOG (INFO) << BOLDMAGENTA << "Using fast command bram to inject " << fNInjectedTriggers << " into system..." << RESET;
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFCMDBram(fFastCommands);
        fTriggersSent += fNInjectedTriggers * cNrepetitions;
        // LOG (INFO) << BOLDMAGENTA << "TriggerIter#" << +fEvent.fIter << RESET;
        // if(fEvent.fIter % 50 == 0) LOG(INFO) << BOLDMAGENTA << "PedeNoiseTime::SendGenericTriggers TriggerIter#" << fEvent.fIter << RESET;
        for(auto cBoard: *fDetectorContainer)
        {
            auto cNevents = this->GenericTriggerConfig(cBoard, cNrepetitions);
            // LOG (INFO) << BOLDMAGENTA << "BeBoard#" << +cBoard->getIndex() << " will start sending the contents of the FMCD BRAM " << +cNrepetitions << " (times)." << RESET;
            // LOG (INFO) << BOLDMAGENTA << "\t\t.. expect to see " << +cNevents << " triggers in the trigger_in_counter"<< RESET;
            // LOG (INFO) << BOLDMAGENTA << "Expect " << +cNevents << " triggers." << RESET;
            // start generic  - ctrl signal high
            fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x1);
            // stop generic  - ctrl signal low
            fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x0);
            // wait until all triggers have been seen by the FC7
            uint32_t cCounter        = 0;
            uint32_t cTriggerCounter = 0;
            do
            {
                // std::this_thread::sleep_for(std::chrono::microseconds(10));
                cTriggerCounter = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
                auto cNWords    = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
                if(cCounter % 25 == 0 && fEvent.fIter % 25 == 0)
                    LOG(DEBUG) << BOLDGREEN << "\t\t.. trigger wait loop Iter#" << +cCounter << " : " << +cTriggerCounter << " counted and " << +cNWords << " words in the readout" << RESET;
                cCounter++;
            } while(cCounter < 10000 && cTriggerCounter < cNevents);
            cTriggerCounter = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
            cSuccess        = cSuccess && (cTriggerCounter >= cNevents);
            if(cSuccess) // check if
            {
                auto& cNtriggers = fTriggerCounter.at(cBoard->getIndex())->getSummary<uint32_t>();
                cNtriggers += cTriggerCounter;
                // LOG (INFO) << BOLDGREEN << "\t\t\t...BeBoard#" << +cBoard->getIndex() << " .. so far I have received " << +cNtriggers << " triggers" << RESET;
                cContinue = cContinue && (cNtriggers < pNtriggersToSend);
                if(cNtriggers >= pNtriggersToSend) LOG(DEBUG) << BOLDMAGENTA << "\t\t.. found all the triggers" << RESET;
            }
            auto& cNwords        = cNwordsContainer.at(cBoard->getIndex())->getSummary<uint32_t>();
            auto  cCurrentNwords = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
            if(cContinue) // if I'm still waiting .. just check the number of words in the readout
            {
                cContinue = (cNwords != cCurrentNwords);
                if(cNwords == cCurrentNwords) LOG(INFO) << BOLDRED << "\t\t.. word counter has stopped incrementing.. will stop..." << RESET;
            }
            cNwords = cCurrentNwords;
        } // board loop
        fEvent.fIter++;
    } while(cContinue); // use generic block to send pNtriggersToSend ... generate 50 at a time
    bool cSuccess = true;
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cNtriggers = fTriggerCounter.at(cBoard->getIndex())->getSummary<uint32_t>();
        cSuccess         = cSuccess && (cNtriggers >= pNtriggersToSend);

    } // check that all the board have received the correct number of triggers
    return cSuccess;
}
bool PedeNoiseTime::DataFromRandomTriggers(int pTriggerSeparation)
{
    for(auto cBoard: *fDetectorContainer)
    {
        cBoard->setEventType(EventType::VR); // temp for PS tests
        // fReadoutChipInterface->setBoard(cBoard->getId());
        fBeBoardInterface->Stop(cBoard);
        // make sure readout has been reset
        (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->ResetReadout();
    } // make sure triggers have been stopped on all boards
    bool cSuccess = SendGenericTriggers(fEventsPerPoint, pTriggerSeparation);
    if(!cSuccess) return cSuccess;
    return cSuccess;
}
bool PedeNoiseTime::DataFromExternalTriggers()
{
    bool cSuccess = true;
    for(auto cBoard: *fDetectorContainer)
    {
        cBoard->setEventType(EventType::VR); // temp for PS tests
        auto& cNtriggers = fTriggerCounter.at(cBoard->getIndex())->getSummary<uint32_t>();
        fBeBoardInterface->Stop(cBoard);
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.fast_duration", 0x0);
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", fEventsPerPoint);
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x00);
        // make sure readout has been reset
        (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->ResetReadout();
        // start trigggers
        fBeBoardInterface->Start(cBoard);
        // check if all triggers have been received
        // wait until all triggers have been seen by the FC7
        uint32_t cCounter = 0;
        cNtriggers        = 0;
        do
        {
            cNtriggers = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
            /*auto cNWords = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
            LOG(INFO) << BOLDGREEN << "\t\t.. trigger wait loop Iter#" << +cCounter << " : "
                    << +cNtriggers << " counted and "
            << +cNWords << " words in the readout"
            << RESET;*/
            cCounter++;
        } while(cCounter < 10000 && cNtriggers < fEventsPerPoint);
        cSuccess = cSuccess && (cNtriggers >= fEventsPerPoint);
    } // make sure triggers have been stopped on all boards
    if(!cSuccess) return cSuccess;
    // now all triggers have been sent. . look at the data in thei readout
    return cSuccess;
}
bool PedeNoiseTime::GetDataFromFC7()
{
    fEventsPerPoint             = findValueInSettings<double>("NeventsScan", 10);
    int  cMeanTriggerSeparation = findValueInSettings<double>("MeanTriggerSeparation", 500);
    int  cUseFcmdBram           = findValueInSettings<double>("UseFcmdBram", 1);
    bool cSuccess               = (cUseFcmdBram) ? this->DataFromRandomTriggers(cMeanTriggerSeparation) : this->DataFromExternalTriggers();
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    if(cSuccess)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        DetectorDataContainer theOccupancyContainer;
        fDetectorDataContainer = &theOccupancyContainer;
        ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
        // now all triggers have been sent. . look at the data in the readout
        for(auto cBoard: *fDetectorContainer)
        {
            auto                  cNWords    = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
            auto&                 cNtriggers = fTriggerCounter.at(cBoard->getIndex())->getSummary<uint32_t>();
            std::vector<uint32_t> cData(0);
            auto                  cNeventsReadBack = ReadData(cBoard, cData, false);
            DecodeData(cBoard, cData, cNeventsReadBack, fBeBoardInterface->getBoardType(cBoard));
            cSuccess = cSuccess && (cNeventsReadBack >= cNtriggers); //
            if(cNtriggers != cNeventsReadBack)
                LOG(INFO) << BOLDRED << "BeBoard#" << +cBoard->getIndex() << " found " << +cNWords << " words in the readout"
                          << " when " << +cNtriggers << " triggers were sent by the fast command block "
                          << " - Read-back " << +cData.size() << " 32 bit words "
                          << " containing .." << +cNeventsReadBack << " events." << RESET;
        }
    } // get events
    return cSuccess;
}
void PedeNoiseTime::CalculateOccupancy(DetectorDataContainer* pOccupancyContainer)
{
    int    cUseFcmdBram     = findValueInSettings<double>("UseFcmdBram", 1);
    size_t cNeventsExpected = fEventsPerPoint;
    if(cUseFcmdBram) cNeventsExpected = fTriggersSent;
    // now retreive events
    const std::vector<Event*>& cPh2Events = GetEvents();
    fEvent.fEventLoss                     = cNeventsExpected - cPh2Events.size();
    if(fEvent.fEventLoss != 0) LOG(INFO) << BOLDMAGENTA << "Have " << +cPh2Events.size() << " events to look at... and I've asked for " << fEventsPerPoint << RESET;
    fEvent.fEventCnt = 0;
    for(auto& cEvent: cPh2Events)
    {
        auto cTriggerId = cEvent->GetExternalTriggerId();
        fEvent.fEventId = cTriggerId;
        // LOG(INFO) << BOLDBLUE << "Event#" << +fEvent.fEventCnt << " trigger Id " << +cTriggerId << RESET;
        for(auto cBoard: *fDetectorContainer)
        {
            auto& cOccThisBoard = pOccupancyContainer->at(cBoard->getIndex());
            for(auto cOpticalGroup: *cBoard)
            {
                auto& cOccThisOG = cOccThisBoard->at(cOpticalGroup->getIndex());
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cOccThisHybrid = cOccThisOG->at(cHybrid->getIndex());
                    fEvent.fHybridId     = cHybrid->getId();
                    for(auto cChip: *cHybrid)
                    {
                        fEvent.fL1Id = cEvent->L1Id(cHybrid->getId(), cChip->getId());
                        // L1 Id starts counting from 1 ..
                        if(cUseFcmdBram)
                        {
                            fEvent.fL1Mismatch = ((int)(fEvent.fL1Id - 1) != (int)(fEvent.fEventCnt % fPerAttempt));
                            // fEvent.fTriggeredBx = fTriggeredBxs[fEvent.fEventCnt];
                            // fEvent.fIter = fIters[fEvent.fEventCnt];
                        }
                        fEvent.fChipId = cChip->getId();
                        // now for the occupancy
                        auto& cOccThischip = cOccThisHybrid->at(cChip->getIndex());
                        fEvent.fHits.clear();
                        auto cHits = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                        for(auto cHit: cHits)
                        {
                            fEvent.fHits.push_back((uint8_t)cHit);
                            if(getChannelGroupHandlerContainer()
                                   ->getObject(cBoard->getId())
                                   ->getObject(cOpticalGroup->getId())
                                   ->getObject(cHybrid->getId())
                                   ->getObject(cChip->getId())
                                   ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                   ->allChannelGroup()
                                   ->isChannelEnabled(cHit))
                            {
                                // LOG (INFO) << BOLDMAGENTA << "\t\t..found a hit in channel " << +cHit << RESET;
                                cOccThischip->getChannelContainer<Occupancy>()->at(cHit).fOccupancy += 1.;
                            }
                        }
#ifdef __USE_ROOT__
                        TTree* cTree = static_cast<TTree*>(getHist(cBoard, "DataLog"));
                        cTree->Fill();
#endif
                    } // CHIP
                }     // hybrid
            }         // OG
        }             // BOARD
        fEvent.fEventCnt++;
    } // event loop - I want to keep this because I want to look at what happens in an event/per event basis
    auto cNevents = cPh2Events.size();
    pOccupancyContainer->normalizeAndAverageContainers(fDetectorContainer, getChannelGroupHandlerContainer(), cNevents);
}
void PedeNoiseTime::measureSCurves(uint16_t pStartValue)
{
    if(fThresholdAndNoiseContainer == nullptr)
    {
        fThresholdAndNoiseContainer = new DetectorDataContainer();
        ContainerFactory::copyAndInitStructure<ThresholdAndNoise>(*fDetectorContainer, *fThresholdAndNoiseContainer);
    }
    fEventsPerPoint   = findValueInSettings<double>("NeventsScan", 10);
    int cStartLatency = findValueInSettings<double>("StartLatency", 1);
    int cLatencyRange = findValueInSettings<double>("LatencyRange", 1);
    LOG(INFO) << BOLDMAGENTA << "PedeNoiseTime::measureSCurves .. asking for " << fEventsPerPoint << " events per point on the threshold scan" << RESET;
    // adding limit to define what all one and all zero actually mean.. avoid waiting forever during scan!
    float    cLimit    = 0.05;
    uint16_t cMaxValue = (1 << 10) - 1;
    // uint16_t cMinValue      = 0;
    if(cWithSSA) cMaxValue = (1 << 8) - 1;
    if(cWithMPA) cMaxValue = (1 << 8) - 1;
    float              cFirstLimit = (cWithCBC) ? 0 : 1;
    std::vector<int>   cSigns{+1, -1}; // want to scan up then down
    std::vector<float> cLimits{1 - cFirstLimit, cFirstLimit};
    std::vector<int>   cBreakCounts{20, 40};
    int                cMinBreakCount = cBreakCounts[0];
    for(uint16_t cTriggerLatency = cStartLatency; cTriggerLatency < cStartLatency + cLatencyRange; cTriggerLatency++)
    {
        uint16_t cValue = pStartValue;
        // set latency
        fEvent.fL1Latency = cTriggerLatency;
        this->setSameGlobalDac("TriggerLatency", cTriggerLatency);
        LOG(INFO) << BOLDMAGENTA << "Threshold scan for a latency value of " << +cTriggerLatency << RESET;
        int cCounter = 0;
        // containers to hold scan data
        std::vector<DetectorDataContainer*> cScanData;
        ContainerRecycleBin<Occupancy>      cRecyclingBin;
        cRecyclingBin.setDetectorContainer(fDetectorContainer);
        for(auto cContainer: cScanData) cRecyclingBin.free(cContainer);
        cScanData.clear();
        // and a vector to hold the values of the thresholds scanned
        std::vector<uint16_t> cThresholds(0);

        for(auto cSign: cSigns)
        {
            bool   firstlim      = false;
            bool   cLimitFound   = false;
            int    cLimitCounter = 0;
            size_t cStep         = 0;
            do
            {
                // push back new entry into scan data container
                cScanData.push_back(cRecyclingBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy()));
                cThresholds.push_back(cValue);
                // DetectorDataContainer* theOccupancyContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
                fDetectorDataContainer      = cScanData[cScanData.size() - 1];
                fSCurveOccupancyMap[cValue] = cScanData[cScanData.size() - 1];
                std::string cRegName        = "VCth";
                if(cWithSSA) cRegName = "Bias_THDAC";
                if(cWithMPA) cRegName = "ThDAC_ALL";
                fEvent.fThreshold = cValue;
                // now set threshold
                this->setSameGlobalDac(cRegName, fEvent.fThreshold);
                bool cSuccess = GetDataFromFC7();
                if(!cSuccess) continue;
                // now retreive events and calculate occupancy
                CalculateOccupancy(cScanData[cScanData.size() - 1]);
                float globalOccupancy     = cScanData[cScanData.size() - 1]->getSummary<Occupancy, Occupancy>().fOccupancy;
                auto  cDistanceFromTarget = std::fabs(globalOccupancy - (cLimits[cCounter]));
                if(cStep % 5 == 0) LOG(INFO) << BOLDMAGENTA << "Current value of threshold is  " << cValue << " Occupancy: " << std::setprecision(2) << std::fixed << globalOccupancy << RESET;
                // if( cStep%5 == 0 ) LOG(DEBUG) << BOLDMAGENTA << "\t.. distance from target is "
                //           << cDistanceFromTarget * 100 << "\t..Incrementing limit found counter "
                //           << " -- current value is " << +cLimitCounter << RESET;
                if(cDistanceFromTarget <= cLimit || firstlim) // || globalOccupancy>1.0)
                {
                    firstlim = true;
                    cLimitCounter++;
                }
                cValue += cSign;
                cLimitFound = (cValue == 0 || cValue >= cMaxValue) || (cLimitCounter >= cMinBreakCount);
                if(cLimitFound && cSign != cSigns[1])
                {
                    LOG(INFO) << BOLDYELLOW << "Switching sign.." << RESET;
                    cMinBreakCount = cBreakCounts[1];
                }
                cStep++;
            } while(!cLimitFound);
            cCounter++;
            cValue = pStartValue + cSigns[cCounter];
        } // threshold loop

        LOG(INFO) << BOLDMAGENTA << "Extracting pedestal and noise for a latency value of " << +cTriggerLatency << RESET;
        // calculate noise
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cFe: *cOpticalGroup)
                {
                    fEvent.fHybridId = cFe->getId();
                    for(auto cROC: *cFe)
                    {
                        if(cROC->getFrontEndType() != FrontEndType::CBC3) continue;

                        fEvent.fChipId = cROC->getId();
                        std::vector<float> cPedestalsThisROC(0);
                        std::vector<float> cNoiseThisROC(0);
                        for(size_t cChnl = 0; cChnl < cROC->size(); cChnl++)
                        {
                            // S-curve for this channel
                            std::vector<float> cW(cThresholds.size(), 0);
                            std::vector<float> cV(cThresholds.size(), 0);
                            for(size_t cIndx = 0; cIndx < cThresholds.size(); cIndx++)
                            {
                                auto& cDataThisBrd   = cScanData[cIndx]->at(cBoard->getIndex());
                                auto& cDataThisOG    = cDataThisBrd->at(cOpticalGroup->getIndex());
                                auto& cDataThisHybrd = cDataThisOG->at(cFe->getIndex());
                                auto& cDataThisChip  = cDataThisHybrd->at(cROC->getIndex());
                                cW[cIndx]            = cDataThisChip->getChannel<Occupancy>(cChnl).fOccupancy;
                                cV[cIndx]            = cThresholds[cIndx];
                            }
                            auto cPedeNoise = evalNoise(cW, cV, true);
                            cPedestalsThisROC.push_back(cPedeNoise.first);
                            cNoiseThisROC.push_back(cPedeNoise.second);
                        } // chnl loop
                        fPedestalStats = SummarizeStats<float>(cPedestalsThisROC);
                        fNoiseStats    = SummarizeStats<float>(cNoiseThisROC);
                        LOG(INFO) << BOLDGREEN << "Chip " << +cROC->getId() << " -- pedestal + noise summary " << RESET;
                        LOG(INFO) << BOLDMAGENTA << "\t\t... Pedestal Stats : " << std::setprecision(2) << std::fixed << " - Mean is " << fPedestalStats.fMean << " - RMS is " << fPedestalStats.fStdDev
                                  << " - minimum value is " << fPedestalStats.fMin << " - maximum value is " << fPedestalStats.fMax << RESET;
                        LOG(INFO) << BOLDMAGENTA << "\t\t... Noise Stats : "
                                  << " - Mean is " << fNoiseStats.fMean << " - RMS is " << fNoiseStats.fStdDev << " - minimum value is " << fNoiseStats.fMin << " - maximum value is "
                                  << fNoiseStats.fMax << RESET;
#ifdef __USE_ROOT__
                        TTree* cTree = static_cast<TTree*>(getHist(cBoard, "PedeNoiseSummary"));
                        cTree->Fill();
#endif
                    } // chip
                }     // hybrid
            }         // OG
        }             // board

    } // latency loop
}
void PedeNoiseTime::extractPedeNoiseTime()
{
    fThresholdAndNoiseContainer = new DetectorDataContainer();
    ContainerFactory::copyAndInitStructure<ThresholdAndNoise>(*fDetectorContainer, *fThresholdAndNoiseContainer);
    uint16_t                                                     counter          = 0;
    std::map<uint16_t, DetectorDataContainer*>::reverse_iterator previousIterator = fSCurveOccupancyMap.rend();
    for(std::map<uint16_t, DetectorDataContainer*>::reverse_iterator mIt = fSCurveOccupancyMap.rbegin(); mIt != fSCurveOccupancyMap.rend(); ++mIt)
    {
        if(previousIterator == fSCurveOccupancyMap.rend())
        {
            previousIterator = mIt;
            continue;
        }
        if(fSCurveOccupancyMap.size() - 1 == counter) break;

        for(auto board: *fDetectorContainer)
        {
            for(auto opticalGroup: *board)
            {
                for(auto hybrid: *opticalGroup)
                {
                    for(auto chip: *hybrid)
                    {
                        for(uint16_t iChannel = 0; iChannel < chip->size(); ++iChannel)
                        {
                            if(!getChannelGroupHandlerContainer()
                                    ->getObject(board->getId())
                                    ->getObject(opticalGroup->getId())
                                    ->getObject(hybrid->getId())
                                    ->getObject(chip->getId())
                                    ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                    ->allChannelGroup()
                                    ->isChannelEnabled(iChannel))
                                continue;
                            float previousOccupancy = (previousIterator)
                                                          ->second->at(board->getIndex())
                                                          ->at(opticalGroup->getIndex())
                                                          ->at(hybrid->getIndex())
                                                          ->at(chip->getIndex())
                                                          ->getChannel<Occupancy>(iChannel)
                                                          .fOccupancy;
                            float currentOccupancy =
                                mIt->second->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getChannel<Occupancy>(iChannel).fOccupancy;
                            float binCenter = (mIt->first + (previousIterator)->first) / 2.;

                            fThresholdAndNoiseContainer->at(board->getIndex())
                                ->at(opticalGroup->getIndex())
                                ->at(hybrid->getIndex())
                                ->at(chip->getIndex())
                                ->getChannel<ThresholdAndNoise>(iChannel)
                                .fThreshold += binCenter * (previousOccupancy - currentOccupancy);

                            fThresholdAndNoiseContainer->at(board->getIndex())
                                ->at(opticalGroup->getIndex())
                                ->at(hybrid->getIndex())
                                ->at(chip->getIndex())
                                ->getChannel<ThresholdAndNoise>(iChannel)
                                .fNoise += binCenter * binCenter * (previousOccupancy - currentOccupancy);

                            fThresholdAndNoiseContainer->at(board->getIndex())
                                ->at(opticalGroup->getIndex())
                                ->at(hybrid->getIndex())
                                ->at(chip->getIndex())
                                ->getChannel<ThresholdAndNoise>(iChannel)
                                .fThresholdError += previousOccupancy - currentOccupancy;
                        }
                    }
                }
            }
        }

        previousIterator = mIt;
        ++counter;
    }

    // calculate the averages and ship

    for(auto board: *fThresholdAndNoiseContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    for(uint16_t iChannel = 0; iChannel < chip->size(); ++iChannel)
                    {
                        if(!getChannelGroupHandlerContainer()
                                ->getObject(board->getId())
                                ->getObject(opticalGroup->getId())
                                ->getObject(hybrid->getId())
                                ->getObject(chip->getId())
                                ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                ->allChannelGroup()
                                ->isChannelEnabled(iChannel))
                            continue;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold /= chip->getChannel<ThresholdAndNoise>(iChannel).fThresholdError;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fNoise /= chip->getChannel<ThresholdAndNoise>(iChannel).fThresholdError;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fNoise = sqrt(chip->getChannel<ThresholdAndNoise>(iChannel).fNoise - (chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold *
                                                                                                                                            chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold));
                        chip->getChannel<ThresholdAndNoise>(iChannel).fThresholdError = 1;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fNoiseError     = 1;
                    }
                }
            }
        }
        board->normalizeAndAverageContainers(fDetectorContainer->at(board->getIndex()), getChannelGroupHandlerContainer()->getObject(board->getId()), 0);
    }
}

void PedeNoiseTime::producePedeNoiseTimePlots()
{
#ifdef __USE_ROOT__
    if(!fFitSCurves) fDQMHistogramPedeNoise.fillPedestalAndNoisePlots(*fThresholdAndNoiseContainer);
#else
    auto theThresholdAndNoiseStream = prepareChannelContainerStreamer<ThresholdAndNoise>();
    for(auto board: *fThresholdAndNoiseContainer)
    {
        if(fDQMStreamerEnabled) { theThresholdAndNoiseStream->streamAndSendBoard(board, fDQMStreamer); }
    }
#endif
}

void PedeNoiseTime::setThresholdtoNSigma(BoardContainer* board, uint32_t pNSigma)
{
    for(auto opticalGroup: *board)
    {
        for(auto hybrid: *opticalGroup)
        {
            for(auto chip: *hybrid)
            {
                uint32_t cROCId = chip->getId();

                uint16_t cPedestal = round(fThresholdAndNoiseContainer->at(board->getIndex())
                                               ->at(opticalGroup->getIndex())
                                               ->at(hybrid->getIndex())
                                               ->at(chip->getIndex())
                                               ->getSummary<ThresholdAndNoise, ThresholdAndNoise>()
                                               .fThreshold);
                uint16_t cNoise    = round(fThresholdAndNoiseContainer->at(board->getIndex())
                                            ->at(opticalGroup->getIndex())
                                            ->at(hybrid->getIndex())
                                            ->at(chip->getIndex())
                                            ->getSummary<ThresholdAndNoise, ThresholdAndNoise>()
                                            .fNoise);
                int      cDiff     = -pNSigma * cNoise;
                uint16_t cValue    = cPedestal + cDiff;

                if(pNSigma > 0)
                    LOG(INFO) << "Changing Threshold on ROC " << +cROCId << " by " << cDiff << " to " << cPedestal + cDiff << " VCth units to supress noise!";
                else
                    LOG(INFO) << "Changing Threshold on ROC " << +cROCId << " back to the pedestal at " << +cPedestal;
                ThresholdVisitor cThresholdVisitor(fReadoutChipInterface, cValue);
                static_cast<ReadoutChip*>(chip)->accept(cThresholdVisitor);
            }
        }
    }
}

void PedeNoiseTime::writeObjects()
{
#ifdef __USE_ROOT__
    fDQMHistogramPedeNoise.process();
#endif
}

void PedeNoiseTime::ConfigureCalibration() { CreateResultDirectory("Results/Run_PedeNoiseTime"); }

void PedeNoiseTime::Running()
{
    LOG(INFO) << "Starting noise measurement";
    Initialise(true, true);
    // auto myFunction = [](const Ph2_HwDescription::ReadoutChip *theChip){
    //     std::cout<<"Using it"<<std::endl;
    //     return (theChip->getId()==0);
    //     };
    // HybridContainer::SetQueryFunction(myFunction);
    measureNoise();
    // HybridContainer::ResetQueryFunction();
    // Validate();
    LOG(INFO) << "Done with noise";
}

void PedeNoiseTime::Stop()
{
    LOG(INFO) << "Stopping noise measurement";
    writeObjects();
    dumpConfigFiles();
    SaveResults();
    closeFileHandler();
    clearDataMembers();
    LOG(INFO) << "Noise measurement stopped.";
}

void PedeNoiseTime::Pause() {}

void PedeNoiseTime::Resume() {}
