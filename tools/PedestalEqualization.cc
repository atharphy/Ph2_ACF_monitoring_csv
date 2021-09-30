#include "PedestalEqualization.h"
#include "../HWDescription/ReadoutChip.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/DataContainer.h"
#include "../Utils/Occupancy.h"
#include "../Utils/SSAChannelGroupHandler.h"

// initialize the static member

using namespace Ph2_System;
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

PedestalEqualization::PedestalEqualization() : Tool() {}

PedestalEqualization::~PedestalEqualization() {}

void PedestalEqualization::Initialise(bool pAllChan, bool pDisableStubLogic)
{
    fDisableStubLogic = pDisableStubLogic;

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0));

    cWithCBC = (cFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
    cWithSSA = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);

    if(cWithCBC) fChannelGroupHandler = new CBCChannelGroupHandler();
    if(cWithSSA) fChannelGroupHandler = new SSAChannelGroupHandler();
    fChannelGroupHandler->setChannelGroupParameters(16, 2);
    this->fAllChan = pAllChan;

    fSkipMaskedChannels          = findValueInSettings("SkipMaskedChannels", 0);
    fMaskChannelsFromOtherGroups = findValueInSettings("MaskChannelsFromOtherGroups", 1);
    fCheckLoop                   = findValueInSettings("VerificationLoop", 1);
    fTestPulseAmplitude          = findValueInSettings("PedestalEqualizationPulseAmplitude", 0);
    fEventsPerPoint              = findValueInSettings("Nevents", 10);
    fNEventsPerBurst             = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : -1;
    fTargetOffset                = 0x7F;
    fTargetVcth                  = 0x0;
    if(cWithSSA) fTargetOffset = 0xF;
    this->SetSkipMaskedChannels(fSkipMaskedChannels);

    if(fTestPulseAmplitude == 0)
        fTestPulse = 0;
    else
        fTestPulse = 1;
        // LOG (INFO) << BLUE <<  "fTestPulse " <<fTestPulse<< RESET ;

#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualization.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif

    if(fDisableStubLogic)
    {
        ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, fStubLogicCointainer);
        ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, fHIPCountCointainer);

        for(auto board: *fDetectorContainer)
        {
            for(auto opticalGroup: *board)
            {
                for(auto hybrid: *opticalGroup)
                {
                    for(auto chip: *hybrid)
                    {
                        ReadoutChip* theChip = static_cast<ReadoutChip*>(chip);
                        // if it is a CBC3, disable the stub logic for this procedure
                        if(theChip->getFrontEndType() == FrontEndType::CBC3)
                        {
                            LOG(INFO) << BOLDBLUE << "Chip Type = CBC3 - thus disabling Stub logic for offset tuning for CBC " << +chip->getId() << RESET;
                            fStubLogicCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint8_t>() =
                                fReadoutChipInterface->ReadChipReg(theChip, "Pipe&StubInpSel&Ptwidth");

                            uint8_t value = fReadoutChipInterface->ReadChipReg(theChip, "HIP&TestMode");
                            fHIPCountCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint8_t>() = value;
                            static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(theChip, false, true, 0);
                        }
                    }
                }
            }
        }
    }

    LOG(INFO) << "Parsed settings:";
    LOG(INFO) << "	Nevents = " << fEventsPerPoint;
    LOG(INFO) << "	TestPulseAmplitude = " << int(fTestPulseAmplitude);
    LOG(INFO) << "  Target Vcth determined algorithmically for ROC";
    LOG(INFO) << "  Target Offset fixed to half range (0x80) for ROC";
}

void PedestalEqualization::FindVplus()
{
    if(fTestPulse)
    {
        this->enableTestPulse(true);
        setFWTestPulse();
        for(auto cBoard: *fDetectorContainer)
        {
            if(cWithSSA)
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "InjectedCharge", fTestPulseAmplitude);
            else
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "TestPulsePotNodeSel", fTestPulseAmplitude);
        }
        LOG(INFO) << BLUE << "Enabled test pulse. " << RESET;
    }
    else
        this->enableTestPulse(false);

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    LOG(INFO) << BOLDBLUE << "Identifying optimal Vplus for ROC..." << RESET;

    if(cWithCBC) setSameDac("VCth", fTargetVcth);
    if(cWithSSA) setSameDac("Bias_THDAC", fTargetVcth);

    bool originalAllChannelFlag = this->fAllChan;
    this->SetTestAllChannels(true);

    if(cWithCBC) setSameLocalDac("ChannelOffset", fTargetOffset);
    if(cWithSSA) setSameLocalDac("ThresholdTrim", fTargetOffset);

    if(cWithCBC) this->bitWiseScan("VCth", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(cWithSSA) this->bitWiseScan("Bias_THDAC", fEventsPerPoint, 0.56, fNEventsPerBurst);

    dumpConfigFiles();

    if(cWithCBC) setSameLocalDac("ChannelOffset", 0xFF);
    if(cWithSSA) setSameLocalDac("ThresholdTrim", 0xFF);

    DetectorDataContainer theVcthContainer;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theVcthContainer);

    float    cMeanValue = 0.;
    uint32_t nCbc       = 0;

    for(auto board: theVcthContainer) // for on boards - begin
    {
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            for(auto module: *opticalGroup) // for on module - begin
            {
                nCbc += module->size();
                std::string tmpParameter = "";
                for(auto chip: *module) // for on chip - begin
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(module->getIndex())->at(chip->getIndex()));
                    uint16_t     tmpVthr = 0;
                    if(cWithCBC) tmpVthr = (theChip->getReg("VCth1") + (theChip->getReg("VCth2") << 8));
                    if(cWithSSA) tmpVthr = theChip->getReg("Bias_THDAC");

                    chip->getSummary<uint16_t>() = tmpVthr;

                    chip->getSummary<uint16_t>()=tmpVthr;

                    LOG (INFO) << GREEN << "VCth value for BeBoard " << +board->getId() << " OpticalGroup " << +opticalGroup->getId()  << " Module " << +module->getId() << " ROC " << +chip->getId() << " = " << tmpVthr << RESET;
                    cMeanValue+=tmpVthr;

                    tmpParameter = "";
                    tmpParameter = "VCth" + std::to_string(chip->getId());
                    #ifdef __USE_ROOT__
                        fillSummaryTree(tmpParameter, tmpVthr);
                    #endif
                } // for on chip - end
            }     // for on module - end
        }         // for on opticalGroup - end
    }             // for on board - end

#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualization.fillVplusPlots(theVcthContainer);
#else
    auto theVCthStream = prepareModuleContainerStreamer<EmptyContainer, uint16_t, EmptyContainer>();
    for(auto board: theVcthContainer)
    {
        if(fStreamerEnabled) theVCthStream.streamAndSendBoard(board, fNetworkStreamer);
    }
#endif

    fTargetVcth = uint16_t(cMeanValue / nCbc);

    if(cWithCBC) setSameDac("VCth", fTargetVcth);
    if(cWithSSA) setSameDac("Bias_THDAC", fTargetVcth);

    LOG (INFO) << BOLDBLUE << "Mean VCth value of all chips is " << fTargetVcth << " - using as TargetVcth value for all chips!" << RESET;
    #ifdef __USE_ROOT__
        fillSummaryTree("VCth", fTargetVcth);
    #endif
}
void PedestalEqualization::FindOffsets()
{
    LOG(INFO) << BOLDBLUE << "Finding offsets..." << RESET;
    // just to be sure, configure the correct VCth and VPlus values

    uint32_t NCH = NCHANNELS;
    if(cWithSSA) NCH = NSSACHANNELS;

    if(cWithCBC) setSameDac("VCth", fTargetVcth);
    if(cWithSSA) setSameDac("Bias_THDAC", fTargetVcth);

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    if(cWithCBC) this->bitWiseScan("ChannelOffset", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(cWithSSA) this->bitWiseScan("ThresholdTrim", fEventsPerPoint, 0.56, fNEventsPerBurst);

    dumpConfigFiles();
    DetectorDataContainer theOffsetsCointainer;
    ContainerFactory::copyAndInitChannel<uint8_t>(*fDetectorContainer, theOffsetsCointainer);

    for(auto board: theOffsetsCointainer) // for on boards - begin
    {
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            for(auto module: *opticalGroup) // for on module - begin
            {
                for(auto chip: *module) // for on chip - begin
                {
                    if(fDisableStubLogic and cWithCBC)
                    {
                        ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(module->getIndex())->at(chip->getIndex()));

                        uint8_t stubLogicValue = fStubLogicCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(module->getIndex())->at(chip->getIndex())->getSummary<uint8_t>();
                        fReadoutChipInterface->WriteChipReg(theChip, "Pipe&StubInpSel&Ptwidth", stubLogicValue);

                        uint8_t HIPCountValue = fHIPCountCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(module->getIndex())->at(chip->getIndex())->getSummary<uint8_t>();
                        fReadoutChipInterface->WriteChipReg(theChip, "HIP&TestMode", HIPCountValue);
                    }

                    unsigned int channelNumber = 1;
                    int          cMeanOffset   = 0;

                    for(auto& channel: *chip->getChannelContainer<uint8_t>()) // for on channel - begin
                    {
                        char charRegName[20];
                        if(cWithCBC) sprintf(charRegName, "Channel%03d", channelNumber++);
                        if(cWithSSA) sprintf(charRegName, "THTRIMMING_S%d", channelNumber++);
                        std::string cRegName = charRegName;
                        channel = static_cast<ReadoutChip*>(fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(module->getIndex())->at(chip->getIndex()))->getReg(cRegName);
                        cMeanOffset += channel;
                    }

                    LOG(INFO) << BOLDRED << "Mean offset on ROC" << +chip->getId() << " is : " << (cMeanOffset) / (double)NCH << " Vcth units." << RESET;
                } // for on chip - end
            }     // for on module - end
        }         // for on opticalGroup - end
    }             // for on board - end

#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualization.fillOccupancyPlots(theOccupancyContainer);
    fDQMHistogramPedestalEqualization.fillOffsetPlots(theOffsetsCointainer);
#else
    auto theOccupancyStream = prepareChannelContainerStreamer<Occupancy>();
    for(auto board: theOccupancyContainer)
    {
        if(fStreamerEnabled) theOccupancyStream.streamAndSendBoard(board, fNetworkStreamer);
    }

    auto theOffsetStream = prepareChannelContainerStreamer<uint8_t>();
    for(auto board: theOffsetsCointainer)
    {
        if(fStreamerEnabled) theOffsetStream.streamAndSendBoard(board, fNetworkStreamer);
    }
#endif

    // a add write original register ;
}

void PedestalEqualization::FindGains() // SSA only
{
    //     uint32_t NCH = NCHANNELS;
    //     if(cWithSSA) NCH = NSSACHANNELS;

    //     if(cWithCBC) setSameDac("VCth", fTargetVcth);
    if(cWithSSA) setSameDac("Bias_THDAC", fTargetVcth);

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    DetectorDataContainer theGainsCointainer;
    ContainerFactory::copyAndInitChannel<uint8_t>(*fDetectorContainer, theGainsCointainer);

    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    // cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 10});
    // cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cal_pulse", 1});
    // cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.antenna", 0});

    for(auto cBeBoard: theGainsCointainer)
    {
        BeBoard* pBoard = static_cast<BeBoard*>(fDetectorContainer->at(cBeBoard->getIndex()));
        fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

        for(auto cOpticalReadout: *cBeBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* cReadoutChip =
                        static_cast<ReadoutChip*>(fDetectorContainer->at(cBeBoard->getIndex())->at(cOpticalReadout->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));

                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "AnalogueAsync", 1);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "InjectedCharge", 20);

                    for(uint32_t channel = 0; channel < cReadoutChip->size(); channel++)
                    {
                        std::string cRegName  = Form("GAINTRIMMING_S%d", channel + 1);
                        uint8_t     cRegValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegName);
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 5);
                    }
                }
            }
        }
        // this->ReadNEvents(pBoard, 1000);
        // const std::vector<Event*>& cEvents2 = this->GetEvents(pBoard);
    }
 
    SetTestAllChannels(true);
    if(cWithSSA) this->bitWiseScan("Bias_THDAC", fEventsPerPoint, 0.91, fNEventsPerBurst);
    dumpConfigFiles();


    BeBoard *pBoard = static_cast<BeBoard*>(fDetectorContainer->at(0));

    // this->ReadNEvents(pBoard, 1000);
    // const std::vector<Event*>& cEvents2 = this->GetEvents(pBoard);

    for(auto cBeBoard: theGainsCointainer)
    {                    
        BeBoard *pBoard = static_cast<BeBoard*>(fDetectorContainer->at(cBeBoard->getIndex()));
        // fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
        for(auto cOpticalReadout: *cBeBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                for(uint32_t channel = 0; channel < 120; channel++)
                {
                    bool cGainCalibrated = false;
                    int  cOccupancy      = 0;

                    std::vector<int> cOccupancyVector(cHybrid->size());
                    std::vector<bool> cGainCalibratedVector(cHybrid->size());
                    for (uint i = 0 ; i < cGainCalibratedVector.size() ; i++ )
                        cGainCalibratedVector[i] = false;
                

                    do
                    {    
                        // std::this_thread::sleep_for(std::chrono::microseconds(50));
                        this->ReadNEvents(pBoard, 1000);
                        const std::vector<Event*>& cEvents = this->GetEvents(pBoard);
                        for(auto cEvent: cEvents)
                        {
                            for(auto cChip: *cHybrid)
                            {
                                ReadoutChip* cReadoutChip =
                                    static_cast<ReadoutChip*>(fDetectorContainer->at(cBeBoard->getIndex())->at(cOpticalReadout->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));

                                auto cNhits     = cEvent->GetNHits(cHybrid->getId(), cReadoutChip->getId());
                                auto cHitVector = cEvent->GetHits(cHybrid->getId(), cReadoutChip->getId());   
                                cOccupancy = cHitVector[channel];     

                                if(!cGainCalibratedVector[cChip->getIndex()])
                                {
                                    LOG(INFO) << "Chip: " << cChip->getId() << " Channel: " << channel << " Occupancy: " << +cOccupancy << RESET ;

                                    std::string cRegName  = Form("GAINTRIMMING_S%d", channel + 1);
                                    uint8_t     cRegValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegName);
                                    // LOG(INFO) << BOLDCYAN << "GainTrim: " << +cRegValue << RESET;

                                    int cThresholdValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, "Bias_THDAC");

                                    if(cOccupancy < 850)
                                    {
                                        if(cRegValue - 1 >= 0)
                                            fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cRegValue - 1);
                                        else
                                        {
                                            LOG(INFO) << BOLDRED << "Reached end of cal" << RESET;
                                            fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 0);
                                            cGainCalibratedVector[cChip->getIndex()] = true;
                                            // cOccupancyVector[cChip->getIndex()] = cHitVector[channel];
                                        }
                                    }
                                    else if(cOccupancy > 950 )
                                    {
                                        if(cRegValue + 1 < 16)
                                            fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cRegValue + 1);
                                        else
                                        {
                                            LOG(INFO) << BOLDRED << "Reached end of cal" << RESET;
                                            fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 15);
                                            cGainCalibratedVector[cChip->getIndex()] = true;
                                            // cOccupancyVector[cChip->getIndex()] = cHitVector[channel];
                                        }
                                    }
                                    else
                                    {
                                        LOG(INFO) << BOLDGREEN << "Value ok" << RESET;
                                        cGainCalibratedVector[cChip->getIndex()] = true;
                                    }
                                }
                                cOccupancyVector[cChip->getIndex()] = cOccupancy;
                            } // chip
                            // LOG(INFO) << "Channel " << channel << RESET;
                            bool aux = true;
                            for (uint i = 0 ; i < cGainCalibratedVector.size() ; i++ ) {
                                // LOG(INFO) << +cGainCalibratedVector[i] << RESET;
                                aux &= cGainCalibratedVector[i];
                                cGainCalibrated = aux;
                            } 
                            LOG(INFO) << BOLDGREEN << cGainCalibrated << RESET;
                        } // event
                    }while(!cGainCalibrated);
                    std::string cRegName = Form("GAINTRIMMING_S%d", channel + 1);
                    for(uint i = 0; i < cHybrid->size(); i++)
                    {
                        ReadoutChip* cReadoutChip    = static_cast<ReadoutChip*>(fDetectorContainer->at(cBeBoard->getIndex())->at(cOpticalReadout->getIndex())->at(cHybrid->getIndex())->at(i));
                        int          cRegValue       = fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegName);
                        int          cThresholdValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, "Bias_THDAC");
                        LOG(INFO) << BOLDMAGENTA << "Chip " << +cReadoutChip->getId() << " channel: " << channel << " Threshold: " << cThresholdValue
                                    << " Occupancy: " << (double)cOccupancyVector[i] << " GainTrim: " << cRegValue << RESET;
                    }
                }
            }
        }
    }
}

// void PedestalEqualization::FindGains() {
//     LOG(INFO) << BOLDBLUE << "Finding gains..." << RESET;
//     // just to be sure, configure the correct VCth and VPlus values

//     uint32_t NCH = NCHANNELS;
//     if(cWithSSA) NCH = NSSACHANNELS;

//     if(cWithCBC) setSameDac("VCth", fTargetVcth);
//     if(cWithSSA) setSameDac("Bias_THDAC", fTargetVcth);

//     DetectorDataContainer theOccupancyContainer;
//     fDetectorDataContainer = &theOccupancyContainer;
//     ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

//     // if(cWithCBC) this->bitWiseScan("ChannelOffset", fEventsPerPoint, 0.56, fNEventsPerBurst);
//     if(cWithSSA) this->bitWiseScan("GainTrim", fEventsPerPoint, 0.56, fNEventsPerBurst);
    
//     dumpConfigFiles();
//     DetectorDataContainer theOffsetsCointainer;
//     ContainerFactory::copyAndInitChannel<uint8_t>(*fDetectorContainer, theOffsetsCointainer);

//     for(auto board: theOffsetsCointainer) // for on boards - begin
//     {
//         for(auto opticalGroup: *board) // for on opticalGroup - begin
//         {
//             for(auto module: *opticalGroup) // for on module - begin
//             {
//                 for(auto chip: *module) // for on chip - begin
//                 {
//                     unsigned int channelNumber = 1;
//                     int          cMeanGain   = 0;

//                     for(auto& channel: *chip->getChannelContainer<uint8_t>()) // for on channel - begin
//                     {
//                         char charRegName[20];
//                         if(cWithCBC) sprintf(charRegName, "Channel%03d", channelNumber++);
//                         if(cWithSSA) sprintf(charRegName, "GAINTRIMMING_S%d", channelNumber++);
//                         std::string cRegName = charRegName;
//                         channel = static_cast<ReadoutChip*>(fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(module->getIndex())->at(chip->getIndex()))->getReg(cRegName);
//                         LOG(DEBUG) << charRegName << " " << +channel << RESET;
//                         cMeanGain += channel;
//                     }

//                     LOG(INFO) << BOLDRED << "Mean gain on ROC" << +chip->getId() << " is : " << (cMeanGain) / (double)NCH << " Vcth units." << RESET;
//                 } // for on chip - end
//             }     // for on module - end
//         }         // for on opticalGroup - end
//     }             // for on board - end

// #ifdef __USE_ROOT__
//     fDQMHistogramPedestalEqualization.fillOccupancyPlots(theOccupancyContainer);
//     fDQMHistogramPedestalEqualization.fillOffsetPlots(theOffsetsCointainer);
// #else
//     auto theOccupancyStream = prepareChannelContainerStreamer<Occupancy>();
//     for(auto board: theOccupancyContainer)
//     {
//         if(fStreamerEnabled) theOccupancyStream.streamAndSendBoard(board, fNetworkStreamer);
//     }

//     auto theOffsetStream = prepareChannelContainerStreamer<uint8_t>();
//     for(auto board: theOffsetsCointainer)
//     {
//         if(fStreamerEnabled) theOffsetStream.streamAndSendBoard(board, fNetworkStreamer);
//     }
// #endif
// }

void PedestalEqualization::writeObjects()
{
    this->SaveResults();

#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualization.process();
#endif
}

// State machine control functions

void PedestalEqualization::ConfigureCalibration() { CreateResultDirectory("Results/Run_PedestalEqualization"); }

void PedestalEqualization::Start(int currentRun)
{
    LOG(INFO) << "Starting Pedestal Equalization";
    Initialise(true, true);
    FindVplus();
    FindOffsets();
    LOG(INFO) << "Done with Pedestal Equalization";
}

void PedestalEqualization::Stop()
{
    LOG(INFO) << "Stopping Pedestal Equalization.";
    writeObjects();
    dumpConfigFiles();
    closeFileHandler();
    LOG(INFO) << "Pedestal Equalization stopped.";
}

void PedestalEqualization::Pause() {}

void PedestalEqualization::Resume() {}
