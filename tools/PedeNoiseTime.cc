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

// #ifdef __USE_ROOT__
// #include "../DQMUtils/DQMHistogramPedeNoiseTime.h"
// #endif

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

    if(cWithCBC) fChannelGroupHandler = new CBCChannelGroupHandler();
    if(cWithSSA) fChannelGroupHandler = new SSAChannelGroupHandler();
    if(cWithMPA) fChannelGroupHandler = new MPAChannelGroupHandler();

    initializeRecycleBin();
    fChannelGroupHandler->setChannelGroupParameters(16, 2);
    // For async only -- to fix
    if(cWithMPA or cWithSSA) fChannelGroupHandler->setChannelGroupParameters(120, 16);
    fAllChan = pAllChan;

    fSkipMaskedChannels          = findValueInSettings("SkipMaskedChannels", 0);
    fMaskChannelsFromOtherGroups = findValueInSettings("MaskChannelsFromOtherGroups", 1);
    fPlotSCurves                 = findValueInSettings("PlotSCurves", 0);
    fFitSCurves                  = findValueInSettings("FitSCurves", 0);
    fPulseAmplitude              = findValueInSettings("PedeNoisePulseAmplitude", 0);
    fEventsPerPoint              = findValueInSettings("Nevents", 10);
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
    // #ifdef __USE_ROOT__
    //     fDQMHistogramPedeNoiseTime.book(fResultFile, *fDetectorContainer, fSettingsMap);
    // #endif
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
    LOG(INFO) << BOLDBLUE << "sweepSCurves" << RESET;
    sweepSCurves();
    LOG(INFO) << BOLDBLUE << "extractPedeNoiseTime" << RESET;
    extractPedeNoiseTime();
    LOG(INFO) << BOLDBLUE << "producePedeNoiseTimePlots" << RESET;
    producePedeNoiseTimePlots();
    LOG(INFO) << BOLDBLUE << "Done" << RESET;
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
    // #ifdef __USE_ROOT__
    //     fDQMHistogramPedeNoiseTime.fillValidationPlots(theOccupancyContainer);
    //     // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fStreamerEnabled << std::endl;
    //     // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fStreamerEnabled << std::endl;
    //     // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fStreamerEnabled << std::endl;
    // #else
    //     std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fStreamerEnabled << std::endl;
    //     std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fStreamerEnabled << std::endl;
    //     std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fStreamerEnabled << std::endl;
    //     auto theOccupancyStream = prepareHybridContainerStreamer<Occupancy, Occupancy, Occupancy>();
    //     // auto theOccupancyStream = prepareChannelContainerStreamer<Occupancy>();

    //     LOG(INFO) << "6 ";
    //     for(auto board: theOccupancyContainer)
    //     {
    //         if(fStreamerEnabled) theOccupancyStream.streamAndSendBoard(board, fNetworkStreamer);
    //     }
    // #endif
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
    } while((size_t)fNInjectedTriggers < pNtriggersToSend);
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
    // set the packet size to be exactly equal to the number we expect
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.packet_nbr", (fNInjectedTriggers - 1) * cNrepetitions);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // make sure this is set
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", fNInjectedTriggers * cNrepetitions);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // re-load configuration
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetTriggerFSM();
    return fNInjectedTriggers * cNrepetitions;
}
bool PedeNoiseTime::SendGenericTriggers(size_t pNtriggersToSend, int pTriggerSeparation)
{
    DetectorDataContainer cNtriggerContainer;
    ContainerFactory::copyAndInitBoard<uint32_t>(*fDetectorContainer, cNtriggerContainer);
    size_t cNtriggersToSendPerAttempt = 50;
    size_t cNrepetitions              = 10;
    // this means that I should send 500 triggers at a time
    bool   cContinue    = true;
    size_t cTriggerIter = 0;
    do
    {
        bool cSuccess = true;
        GenericTriggers(cNtriggersToSendPerAttempt, pTriggerSeparation);
        // LOG (INFO) << BOLDMAGENTA << "Using fast command bram to inject " << fNInjectedTriggers << " into system..." << RESET;
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFCMDBram(fFastCommands);
        // LOG (INFO) << BOLDMAGENTA << "TriggerIter#" << +cTriggerIter << RESET;
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
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                cTriggerCounter = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
                // auto cNWords = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
                // LOG(INFO) << BOLDGREEN << "\t\t\t\t.. Trigger wait loop Iter#" << +cCounter << " : "
                //     << +cTriggerCounter << " counted and "
                //     << +cNWords << " words in the readout"
                //     << RESET;
                cCounter++;
            } while(cCounter < 100 && cTriggerCounter < cNevents);
            cTriggerCounter = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
            cSuccess        = cSuccess && (cTriggerCounter >= cNevents);
            if(cSuccess)
            {
                auto& cNtriggers = cNtriggerContainer.at(cBoard->getIndex())->getSummary<uint32_t>();
                cNtriggers += cTriggerCounter;
                // LOG (INFO) << BOLDGREEN << "\t\t\t...BeBoard#" << +cBoard->getIndex() << " .. received " << +cNtriggers << " triggers" << RESET;
                cContinue = cContinue && (cNtriggers < pNtriggersToSend);
            }
        } // board loop
        cTriggerIter++;
    } while(cContinue); // use generic block to send pNtriggersToSend ... generate 50 at a time
    bool cSuccess = true;
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cNtriggers = cNtriggerContainer.at(cBoard->getIndex())->getSummary<uint32_t>();
        cSuccess         = cSuccess && (cNtriggers >= pNtriggersToSend);

    } // check that all the board have received the correct number of triggers
    if(cSuccess)
    {
        DetectorDataContainer theOccupancyContainer;
        fDetectorDataContainer = &theOccupancyContainer;
        ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
        // now all triggers have been sent. . look at the data in the readout
        for(auto cBoard: *fDetectorContainer)
        {
            auto                  cNWords = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
            std::vector<uint32_t> cData(0);
            auto                  cNeventsReadBack = ReadData(cBoard, cData, false);
            DecodeData(cBoard, cData, cNeventsReadBack, fBeBoardInterface->getBoardType(cBoard));
            LOG(INFO) << BOLDMAGENTA << "BeBoard#" << +cBoard->getIndex() << " found " << +cNWords << " words in the readout"
                      << " - Read-back " << +cData.size() << " 32 bit words "
                      << " containing .." << +cNeventsReadBack << " events." << RESET;
        }
    }
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
void PedeNoiseTime::measureSCurves(uint16_t pStartValue)
{
    int cMeanTriggerSeparation = 500;
    // adding limit to define what all one and all zero actually mean.. avoid waiting forever during scan!
    float    cLimit         = 0.05;
    int      cMinBreakCount = 10;
    uint16_t cValue         = pStartValue;
    uint16_t cMaxValue      = (1 << 10) - 1;
    // uint16_t cMinValue      = 0;
    if(cWithSSA) cMaxValue = (1 << 8) - 1;
    if(cWithMPA) cMaxValue = (1 << 8) - 1;
    float              cFirstLimit = (cWithCBC) ? 0 : 1;
    std::vector<int>   cSigns{-1, 1};
    std::vector<float> cLimits{cFirstLimit, 1 - cFirstLimit};
    //(fDetectorContainer[0]->getBoardType() == BoardType::D19C)

    int cCounter = 0;
    for(auto cSign: cSigns)
    {
        bool firstlim      = false;
        bool cLimitFound   = false;
        int  cLimitCounter = 0;
        do
        {
            DetectorDataContainer* theOccupancyContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
            fDetectorDataContainer                       = theOccupancyContainer;
            fSCurveOccupancyMap[cValue]                  = theOccupancyContainer;
            std::string cRegName                         = "VCth";
            if(cWithSSA) cRegName = "Bias_THDAC";
            if(cWithMPA) cRegName = "ThDAC_ALL";
            this->setSameGlobalDac(cRegName, cValue);
            bool cSuccess = this->DataFromRandomTriggers(cMeanTriggerSeparation);
            if(!cSuccess) continue;

            // now retreive events
            const std::vector<Event*>& cPh2Events = GetEvents();
            // LOG(INFO) << BOLDMAGENTA << "Have " << +cPh2Events.size() << " events to look at." << RESET;
            for(auto& cEvent: cPh2Events)
            {
                // auto cEventId   = cEvent->GetEventCount();
                // auto cTriggerId = cEvent->GetExternalTriggerId();
                // LOG(INFO) << BOLDBLUE << "Event#" << +cEventId << " trigger Id " << +cTriggerId << RESET;
                for(auto cBoard: *fDetectorContainer)
                {
                    auto& cOccThisBoard = theOccupancyContainer->at(cBoard->getIndex());
                    LOG(DEBUG) << "Board#" << +cOccThisBoard->getIndex() << RESET;
                    for(auto cOpticalGroup: *cBoard)
                    {
                        auto& cOccThisOG = cOccThisBoard->at(cOpticalGroup->getIndex());
                        LOG(DEBUG) << "OG#" << +cOccThisOG->getIndex() << RESET;
                        for(auto cHybrid: *cOpticalGroup)
                        {
                            auto& cOccThisHybrid = cOccThisOG->at(cHybrid->getIndex());
                            LOG(DEBUG) << "Hybrid#" << +cHybrid->getIndex() << RESET;
                            // LOG(INFO) << BOLDBLUE << "Filling data container for hybrid " << +hybrid->getId() << RESET;
                            for(auto cChip: *cHybrid)
                            {
                                auto& cOccThischip = cOccThisHybrid->at(cChip->getIndex());
                                LOG(DEBUG) << "Chip#" << +cOccThischip->getIndex() << RESET;

                                std::vector<uint32_t> cHits = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                                LOG(DEBUG) << BOLDBLUE << "Filling data container for chip " << +cChip->getId() << " at index " << +cChip->getIndex() << "\t.... " << +cHits.size() << " hits in chip."
                                           << RESET;
                                for(auto cHit: cHits)
                                {
                                    if(fChannelGroupHandler->allChannelGroup()->isChannelEnabled(cHit))
                                    {
                                        // LOG (INFO) << BOLDMAGENTA << "\t\t..found a hit in channel " << +cHit << RESET;
                                        cOccThischip->getChannelContainer<Occupancy>()->at(cHit).fOccupancy += 1.;
                                    }
                                }
                            } // CHIP
                        }     // hybrid
                    }         // OG
                }             // BOARD

            } // events
            // auto cNevents = cPh2Events.size();
            float globalOccupancy = theOccupancyContainer->getSummary<Occupancy, Occupancy>().fOccupancy;

            // #ifdef __USE_ROOT__
            //             if(fPlotSCurves) fDQMHistogramPedeNoiseTime.fillSCurvePlots(cValue, *theOccupancyContainer);
            // #else
            //             if(fPlotSCurves)
            //             {
            //                 auto theSCurveStreamer = prepareChannelContainerStreamer<Occupancy, uint16_t>("SCurve");
            //                 theSCurveStreamer.setHeaderElement(cValue);
            //                 for(auto board: *theOccupancyContainer)
            //                 {
            //                     if(fStreamerEnabled) theSCurveStreamer.streamAndSendBoard(board, fNetworkStreamer);
            //                 }
            //             }
            // #endif

            auto cDistanceFromTarget = std::fabs(globalOccupancy - (cLimits[cCounter]));
            LOG(INFO) << BOLDMAGENTA << "Current value of threshold is  " << cValue << " Occupancy: " << std::setprecision(2) << std::fixed << globalOccupancy << "\t.. distance from target is "
                      << cDistanceFromTarget * 100 << "\t..Incrementing limit found counter "
                      << " -- current value is " << +cLimitCounter << RESET;
            if(cDistanceFromTarget <= cLimit || firstlim) // || globalOccupancy>1.0)
            {
                firstlim = true;
                // LOG(DEBUG) << BOLDMAGENTA << "\t\t....Incrementing limit found counter "
                //            << " -- current value is " << +cLimitCounter << RESET;
                cLimitCounter++;
            }

            cValue += cSign;
            cLimitFound = (cValue == 0 || cValue >= cMaxValue) || (cLimitCounter >= cMinBreakCount);
            if(cLimitFound) { LOG(INFO) << BOLDYELLOW << "Switching sign.." << RESET; }

        } while(!cLimitFound);
        cCounter++;
        cValue = pStartValue + cSigns[cCounter];
    }
    // this->HttpServerProcess();
    LOG(DEBUG) << YELLOW << "Found minimal and maximal occupancy " << cMinBreakCount << " times, SCurves finished! " << RESET;
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
                            if(!fChannelGroupHandler->allChannelGroup()->isChannelEnabled(iChannel)) continue;
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
                        if(!fChannelGroupHandler->allChannelGroup()->isChannelEnabled(iChannel)) continue;
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
        board->normalizeAndAverageContainers(fDetectorContainer->at(board->getIndex()), fChannelGroupHandler->allChannelGroup(), 0);
    }
}

void PedeNoiseTime::producePedeNoiseTimePlots()
{
    // #ifdef __USE_ROOT__
    //     if(!fFitSCurves) fDQMHistogramPedeNoiseTime.fillPedestalAndNoisePlots(*fThresholdAndNoiseContainer);
    // #else
    //     auto theThresholdAndNoiseStream = prepareChannelContainerStreamer<ThresholdAndNoise>();
    //     for(auto board: *fThresholdAndNoiseContainer)
    //     {
    //         if(fStreamerEnabled) { theThresholdAndNoiseStream.streamAndSendBoard(board, fNetworkStreamer); }
    //     }
    // #endif
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
    // #ifdef __USE_ROOT__
    //     fDQMHistogramPedeNoiseTime.process();
    // #endif
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
