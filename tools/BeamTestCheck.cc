#include "BeamTestCheck.h"

#include "../HWDescription/Cbc.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

BeamTestCheck::BeamTestCheck() : OTTool() {}

BeamTestCheck::~BeamTestCheck() {}

// Initialization function
void BeamTestCheck::Initialise()
{
    Prepare();
    SetName("BeamTestCheck");
    LOG(INFO) << BOLDYELLOW << "Number of events is " << fNevents << RESET;
    if(fReadoutMode == 0)
    {
        // list of chip registers that can be modified by this tool
        SetChipRegstoPerserve(FrontEndType::CBC3, {"TriggerLatency1", "FeCtrl&TrgLat2"});

        // list of board registers that can be modified by this tool
        for(auto cBoard: *fDetectorContainer)
        {
            LOG(INFO) << BOLDYELLOW << "Package delay on BeBoard#" << +cBoard->getId() << " set to "
                      << fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay") << RESET;
        }
        std::vector<std::string> cBrdRegsToKeep{"fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay",
                                                "fc7_daq_cnfg.readout_block.global.common_stubdata_delay",
                                                "fc7_daq_cnfg.fast_command_block.trigger_source",
                                                "fc7_daq_cnfg.tlu_block.handshake_mode"};
        SetBrdRegstoPerserve(cBrdRegsToKeep);
    }
    initializeRecycleBin();

    // create groups for injection
    // set injection group
    CBCChannelGroupHandler theChannelGroupHandler;
    theChannelGroupHandler.setChannelGroupParameters(16, 2);
    setChannelGroupHandler(theChannelGroupHandler);

    // set TP amplitude and delay
    fTPamplitude = findValueInSettings<double>("Check2STPamplitude", 255);
    fTPdelay     = findValueInSettings<double>("Check2STPdelay", 0);

    // threshold
    fThreshold = findValueInSettings<double>("Check2Sthreshold", 0);

    // initialize latency scan range based on TP settings
    fStartLatency = findValueInSettings<double>("StartLatency", 0);
    fLatencyRange = findValueInSettings<double>("LatencyRange", 0);

    auto cInjectionType = findValueInSettings<double>("InjectionType", 0);
    SetInjectionType(cInjectionType);

    // initialize containers
    // latency per hybrid
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fLatencyContainer);
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fStubLatencyContainer);
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fLatencyContainerS0);
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fLatencyContainerS1);
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fLatencyContainerCoincidence);

    // cluster occupancy per chip
    ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, float>>(*fDetectorContainer, fClusterOccupancy);
    ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, float>>(*fDetectorContainer, fClusterOccupancyS0);
    ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, float>>(*fDetectorContainer, fClusterOccupancyS1);

    // hit occupancies per strip
    ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fHitOccupancyS0);
    ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fHitOccupancyS1);
    ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fHitOccupancyCoinc);

    // stub occupancies per chip
    ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fStubOccupancy);
    // hits per TDC phase
    ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fHitContainerTDC);

    // hit+stub maps per chip
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, fHitMap);
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, fStubMap);

    // bend maps per hybrid
    ContainerFactory::copyAndInitHybrid<GenericDataArray<BENDBINS, uint16_t>>(*fDetectorContainer, fBendMap);
    ContainerFactory::copyAndInitHybrid<GenericDataArray<BENDBINS, uint16_t>>(*fDetectorContainer, fEventsWithSingleClusters);
    ContainerFactory::copyAndInitHybrid<GenericDataArray<BENDBINS, uint16_t>>(*fDetectorContainer, fEventsWithStubs);

    //
    ContainerFactory::copyAndInitHybrid<std::vector<std::vector<uint32_t>>>(*fDetectorContainer, fEventSubSet);
    ContainerFactory::copyAndInitHybrid<std::vector<std::vector<uint32_t>>>(*fDetectorContainer, fStubSubSet);

    // optimal latencies
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, fOptimalL1Latency);
    ContainerFactory::copyAndInitBoard<uint32_t>(*fDetectorContainer, fOptimalStubLatency);
    // TDC per board
    ContainerFactory::copyAndInitBoard<GenericDataArray<TDCBINS, uint16_t>>(*fDetectorContainer, fTDCContainer);

    // pedestals
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, fPedestalContainer);

#ifdef __USE_ROOT__
    fDQMHistogrammer.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
    // injection
    defInjection();
}

// State machine control functions
void BeamTestCheck::Running()
{
    Initialise();
    fSuccess = true;
    Reset();
}

void BeamTestCheck::DisableAllFEs()
{
    // disable all FEs
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                // cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
                fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
            }
        }
    }
}
//
void BeamTestCheck::CheckWithTP(uint8_t pContinousReadout)
{
    for(auto cBoard: *fDetectorContainer)
    {
        // prepare injection
        PrepareForTP(cBoard);
    }
    if(fScanL1Latency) ScanL1Latency(pContinousReadout);
    if(fScanStubLatency) ScanStubLatency(pContinousReadout);

#ifdef __USE_ROOT__
    fDQMHistogrammer.fillLatencyPlots(fLatencyContainerS0, fLatencyContainerS1);
    fDQMHistogrammer.fillStubLatencyPlots(fStubLatencyContainer);
    fDQMHistogrammer.fillTriggerTDCPlots(fTDCContainer);
#endif
    // validate
    Validate();

    // for(auto cBoard: *fDetectorContainer) { PrintData(cBoard); }
}
void BeamTestCheck::ValidateTP()
{
    for(auto cBoard: *fDetectorContainer)
    {
        PrepareForExternalTP(cBoard);
        // prepare injection
        // PrepareForTP(cBoard);
    }
    // validate
    Validate();
}
void BeamTestCheck::ValidateRaw()
{
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        const std::vector<Event*>& cEvents      = this->GetEvents();
        BeBoardRegMap              cRegMap      = cBoard->getBeBoardRegMap();
        uint32_t                   cTriggerMult = cRegMap["fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity"];
        for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++) { Count(cEvents, cTriggerId, 1); }
    }
#ifdef __USE_ROOT__
    fDQMHistogrammer.fillHitMaps(fHitMap, fStubMap, fHitContainerTDC);
    // fDQMHistogrammer.fillBendPlots(fBendMap);
    // fDQMHistogrammer.fillCountPlots(fEventSubSet, fStubSubSet);
#endif
}
void BeamTestCheck::Validate()
{
    // validate
    // read events
    if(fReadoutMode == 0) ContinousReadout();

    LOG(INFO) << BOLDYELLOW << "Creating root file [hit map] from raw file" << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        const std::vector<Event*>& cEvents              = this->GetEvents();
        float                      cNormalizationFactor = fNevents; // cEvents.size() / (1 + cTriggerMult);
        LOG(INFO) << BOLDMAGENTA << "Read-back " << +cEvents.size() << " events from BeBoard#" << +cBoard->getId() << " - normalization factor for occupancy is " << +cNormalizationFactor << RESET;
        BeBoardRegMap              cRegMap              = cBoard->getBeBoardRegMap();
        std::string                cMultRegName         = "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity";
        size_t                     cTriggerMult         = (fReadoutMode == 0) ? fBeBoardInterface->ReadBoardReg(cBoard, cMultRegName) : cRegMap[cMultRegName];
        for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++) { Count(cEvents, cTriggerId, 1); }
    }
    #ifdef __USE_ROOT__
        fDQMHistogrammer.fillHitMaps(fHitMap, fStubMap, fHitContainerTDC);
        fDQMHistogrammer.fillBendPlots(fBendMap);
        fDQMHistogrammer.fillCountPlots(fEventSubSet, fStubSubSet);
    #endif
}
//
void BeamTestCheck::CheckWithInternal(uint8_t pContinousReadout)
{
    for(auto cBoard: *fDetectorContainer)
    {
        // prepare injection
        PrepareForInternal(cBoard);
        LOG(INFO) << "Check with internal triggers " << RESET;
    }

    if(fScanL1Latency) ScanL1Latency(pContinousReadout);
    if(fScanStubLatency) ScanStubLatency(pContinousReadout);

#ifdef __USE_ROOT__
    fDQMHistogrammer.fillLatencyPlots(fLatencyContainerS0, fLatencyContainerS1);
    fDQMHistogrammer.fillStubLatencyPlots(fStubLatencyContainer);
    fDQMHistogrammer.fillTriggerTDCPlots(fTDCContainer);
#endif

    // validate
    Validate();

    for(auto cBoard: *fDetectorContainer) { PrintData(cBoard); }
}
void BeamTestCheck::CheckWithTLU(uint8_t pContinousReadout)
{
    LOG(INFO) << BOLDBLUE << "Checking with external triggers - will readout " << fNevents << RESET;

    for(auto cBoard: *fDetectorContainer)
    {
        // prepare injection
        PrepareForTLU(cBoard);
        LOG(INFO) << "External check with " << fNevents << " -- continuous readout set to " << +pContinousReadout << RESET;
    }

    if(fScanL1Latency) ScanL1Latency(pContinousReadout);
    if(fScanStubLatency) ScanStubLatency(pContinousReadout);

#ifdef __USE_ROOT__
    fDQMHistogrammer.fillLatencyPlots(fLatencyContainerS0, fLatencyContainerS1);
    fDQMHistogrammer.fillStubLatencyPlots(fStubLatencyContainer);
    fDQMHistogrammer.fillTriggerTDCPlots(fTDCContainer);
#endif

    // validate
    Validate();
}
void BeamTestCheck::CheckWithExternal(uint8_t pContinousReadout)
{
    LOG(INFO) << BOLDBLUE << "Checking with external triggers - will readout " << fNevents << RESET;

    for(auto cBoard: *fDetectorContainer)
    {
        // prepare injection
        PrepareForExternal(cBoard);
        LOG(INFO) << "External check with " << fNevents << " -- continuous readout set to " << +pContinousReadout << RESET;
    }

    if(fScanL1Latency) ScanL1Latency(pContinousReadout);
    if(fScanStubLatency) ScanStubLatency(pContinousReadout);
        /*// print out optimal L1 + stub latencies
        for(auto cBoard: *fDetectorContainer)
        {
            LOG(INFO) << BOLDYELLOW << "BeBoard#" << +cBoard->getId() << " optimal stub latency found to be " << fOptimalStubLatency.at(cBoard->getIndex())->getSummary<uint32_t>() << RESET;
            for(auto cOpticalGroup: *cBoard) // for on opticalGroup - begin
            {
                for(auto cHybrid: *cOpticalGroup) // for on hybrid - begin
                {
                    LOG(INFO) << BOLDYELLOW << "\tHybrid#" << +cHybrid->getId() << RESET;
                    for(auto cChip: *cHybrid) // for on chip - begin
                    {
                        auto& cLat =
        fOptimalL1Latency.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>(); LOG(INFO) << BOLDYELLOW
        << "\t\tChip#" << +cChip->getId() << " optimal L1 latency found to be " << cLat << RESET;
                    }
                }
            }
        }*/

#ifdef __USE_ROOT__
    fDQMHistogrammer.fillLatencyPlots(fLatencyContainerS0, fLatencyContainerS1);
    fDQMHistogrammer.fillStubLatencyPlots(fStubLatencyContainer);
    fDQMHistogrammer.fillTriggerTDCPlots(fTDCContainer);
#endif

    // validate
    Validate();

    // for(auto cBoard: *fDetectorContainer)
    // {
    //     // prepare injection
    //     PrepareForExternal(cBoard);
    //     LOG (INFO) << "External check with " << fNevents << " -- continuous readout set to " << +pContinousReadout << RESET;

    //     // if(pContinousReadout == 1) ContinousReadout(cBoard);
    //     // else ReadNEvents(cBoard, fNevents);

    //     // // process events
    //     // //ProcessEvents(cBoard);
    //     // // scan the latency - find best hit latency
    //     ScanLatency(cBoard, pContinousReadout);
    //     // scan the threshold, record number of hits; cluster occupancy
    //     // ScanThreshold(cBoard);
    // }
    // #ifdef __USE_ROOT__
    //     fDQMHistogrammer.fillLatencyPlots(fLatencyContainerS0, fLatencyContainerS1);
    //     fDQMHistogrammer.fillStubLatencyPlots(fStubLatencyContainer);
    //     fDQMHistogrammer.fillTriggerTDCPlots(fTDCContainer);
    // #endif
}
void BeamTestCheck::ValidateExternal()
{
    for(auto cBoard: *fDetectorContainer)
    {
        // prepare injection
        PrepareForExternal(cBoard);
    }
    // validate
    Validate();
}
void BeamTestCheck::ValidateTLU()
{
    for(auto cBoard: *fDetectorContainer)
    {
        // prepare injection
        PrepareForTLU(cBoard);
    }
    // validate
    Validate();
}
void BeamTestCheck::UpdateClusterContainers(BeBoard* pBoard, const std::vector<Event*> pEvents, size_t pIndx)
{
    // not sure if I can just retrieve the events..
    // this->ReadNEvents(pBoard, this->findValueInSettings("Nevents"));
    // const std::vector<Event*>& pEvents = this->GetEvents();
    if(fThStep % 10 == 0) LOG(INFO) << BOLDMAGENTA << "Calculating cluster occupancy for " << +pEvents.size() << " events read-back from BeBoard#" << +pBoard->getIndex() << RESET;
    auto   cClusterOccupancy    = fClusterOccupancy.at(pBoard->getIndex());
    auto   cClusterOccupancyS0  = fClusterOccupancyS0.at(pBoard->getIndex());
    auto   cClusterOccupancyS1  = fClusterOccupancyS1.at(pBoard->getIndex());
    size_t cTriggerMult         = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    float  cNormalizationFactor = pEvents.size() / (1 + cTriggerMult);
    for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++)
    {
        auto cEventIter = pEvents.begin() + cTriggerId;
        do
        {
            if(cEventIter >= pEvents.end()) break;
            for(auto cOpticalGroup: *pBoard)
            {
                auto& cClusterOccupancyOG   = cClusterOccupancy->at(cOpticalGroup->getIndex());
                auto& cClusterOccupancyOGS0 = cClusterOccupancyS0->at(cOpticalGroup->getIndex());
                auto& cClusterOccupancyOGS1 = cClusterOccupancyS1->at(cOpticalGroup->getIndex());
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cClusterOccupancyH   = cClusterOccupancyOG->at(cHybrid->getIndex());
                    auto& cClusterOccupancyHS0 = cClusterOccupancyOGS0->at(cHybrid->getIndex());
                    auto& cClusterOccupancyHS1 = cClusterOccupancyOGS1->at(cHybrid->getIndex());

                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;

                        // auto cPedestal = fPedestalContainer.at(pBoard->getIndex())
                        //                     ->at(cOpticalGroup->getIndex())
                        //                     ->at(cHybrid->getIndex())
                        //                     ->at(cChip->getIndex())
                        //                     ->getSummary<uint16_t>();

                        auto& cClusterOccupancyC   = cClusterOccupancyH->at(cChip->getIndex());
                        auto& cClusterOccupancyCS0 = cClusterOccupancyHS0->at(cChip->getIndex());
                        auto& cClusterOccupancyCS1 = cClusterOccupancyHS1->at(cChip->getIndex());
                        // auto  cThreshold = fReadoutChipInterface->ReadChipReg(cChip,"Threshold");

                        // zero
                        if((*cEventIter)->GetEventCount() == cTriggerId)
                        {
                            cClusterOccupancyC->getSummary<GenericDataArray<VECSIZE, float>>()[pIndx]   = 0;
                            cClusterOccupancyCS0->getSummary<GenericDataArray<VECSIZE, float>>()[pIndx] = 0;
                            cClusterOccupancyCS1->getSummary<GenericDataArray<VECSIZE, float>>()[pIndx] = 0;
                        }

                        auto   cClusters    = (*cEventIter)->getClusters(cHybrid->getId(), cChip->getId());
                        size_t cNClustersS0 = 0;
                        size_t cNClustersS1 = 0;
                        for(auto cCluster: cClusters)
                        {
                            cNClustersS0 += (cCluster.fSensor == 0) ? 1 : 0;
                            cNClustersS1 += (cCluster.fSensor == 1) ? 1 : 0;
                        }

                        // adjust
                        cClusterOccupancyC->getSummary<GenericDataArray<VECSIZE, float>>()[pIndx] += (cNClustersS0 + cNClustersS1) / cNormalizationFactor;
                        cClusterOccupancyCS0->getSummary<GenericDataArray<VECSIZE, float>>()[pIndx] += cNClustersS0 / cNormalizationFactor;
                        cClusterOccupancyCS1->getSummary<GenericDataArray<VECSIZE, float>>()[pIndx] += cNClustersS1 / cNormalizationFactor;

                    } // chip vector
                }     // hybrid vector
            }         // optical group vector
            cEventIter += (1 + cTriggerMult);
        } while(cEventIter < pEvents.end());
    }
    for(auto cOpticalGroup: *pBoard)
    {
        auto& cClusterOccupancyOG   = cClusterOccupancy->at(cOpticalGroup->getIndex());
        auto& cClusterOccupancyOGS0 = cClusterOccupancyS0->at(cOpticalGroup->getIndex());
        auto& cClusterOccupancyOGS1 = cClusterOccupancyS1->at(cOpticalGroup->getIndex());
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cClusterOccupancyH   = cClusterOccupancyOG->at(cHybrid->getIndex());
            auto& cClusterOccupancyHS0 = cClusterOccupancyOGS0->at(cHybrid->getIndex());
            auto& cClusterOccupancyHS1 = cClusterOccupancyOGS1->at(cHybrid->getIndex());

            for(auto cChip: *cHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;

                auto& cClusterOccupancyC   = cClusterOccupancyH->at(cChip->getIndex());
                auto& cClusterOccupancyCS0 = cClusterOccupancyHS0->at(cChip->getIndex());
                auto& cClusterOccupancyCS1 = cClusterOccupancyHS1->at(cChip->getIndex());
                auto  cThreshold           = fReadoutChipInterface->ReadChipReg(cChip, "Threshold");
                if(fThStep % 10 == 0)
                    LOG(INFO) << BOLDMAGENTA << "Cluster occupancy at a threshold of " << cThreshold << " for Chip#" << +cChip->getId() << " on FE#" << +cHybrid->getId()
                              << " is : " << cClusterOccupancyC->getSummary<GenericDataArray<VECSIZE, float>>()[pIndx] << " overall "
                              << cClusterOccupancyCS0->getSummary<GenericDataArray<VECSIZE, float>>()[pIndx] << " on S0 " << cClusterOccupancyCS1->getSummary<GenericDataArray<VECSIZE, float>>()[pIndx]
                              << " on S1 " << RESET;

            } // chip vector
        }     // hybrid vector
    }         // optical group vector
}
void BeamTestCheck::ScanThreshold(BeBoard* pBoard)
{
    bool cSparsified = pBoard->getSparsification();
    // make sure I am in un-sparsified mode
    LOG(INFO) << BOLDGREEN << "Setting sparsification OFF" << RESET;
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", 0);
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SetSparsification(cCic, 0);
        }
    }

    float cLimit      = 0.075;
    float cBreakCount = 5;
    // first set latency - off
    uint16_t cOffLatency = fOptimalLatency - 20;
    LOG(INFO) << BOLDGREEN << "Setting trigger latency to " << cOffLatency << " [off latency]" << RESET;
    setSameDacBeBoard(pBoard, "TriggerLatency", cOffLatency);
    fBeBoardInterface->ChipReSync(pBoard);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // find pedestal
    DetectorDataContainer cContainerOffLatency;
    fDetectorDataContainer = &cContainerOffLatency;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    bitWiseScan("Threshold", fNevents, 0.75);
    for(auto cOpticalGroup: *pBoard) // for on opticalGroup - begin
    {
        for(auto cHybrid: *cOpticalGroup) // for on hybrid - begin
        {
            for(auto cChip: *cHybrid) // for on chip - begin
            {
                uint16_t cThreshold = fReadoutChipInterface->ReadChipReg(cChip, "Threshold");
                fPedestalContainer.at(pBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = cThreshold;

                LOG(INFO) << BOLDMAGENTA << "Off-latency... 50 percent occupancy level on chip#" << +cChip->getId() << "FE#" << cHybrid->getId() << " found for a threshold of " << cThreshold << RESET;
            } // for on chip - end
        }     // for on hybrid - end
    }         // for on opticalGroup - end

    // make sure in sparisified mode for this
    LOG(INFO) << BOLDGREEN << "Setting sparsification ON" << RESET;
    pBoard->setSparsification(true);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", 1);
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SetSparsification(cCic, 1);
        }
    }

    // back to on latency - scan till all zero
    LOG(INFO) << BOLDGREEN << "Setting trigger latency to " << fOptimalLatency << " [on latency]" << RESET;
    setSameDacBeBoard(pBoard, "TriggerLatency", fOptimalLatency);
    fBeBoardInterface->ChipReSync(pBoard);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int    cOffset          = 0;
    size_t cLimitReached    = 0;
    float  cTargetOccupancy = 0.0;
    LOG(INFO) << BOLDGREEN << "Scanning threshold until all zeros reached" << RESET;
    std::vector<uint16_t> cThresholdOffsets(0);
    fThStep = 0;
    do
    {
        // update threshold
        for(auto cOpticalGroup: *pBoard) // for on opticalGroup - begin
        {
            for(auto cHybrid: *cOpticalGroup) // for on hybrid - begin
            {
                for(auto cChip: *cHybrid) // for on chip - begin
                {
                    uint16_t cThreshold = fPedestalContainer.at(pBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                    fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold + cOffset);
                    if(fThStep % 10 == 0) LOG(INFO) << BOLDBLUE << "Threshold on Chip" << +cChip->getId() << " on Hybrid" << +cHybrid->getId() << " Vcth is " << (cThreshold + cOffset) << RESET;
                } // for on chip - end
            }     // for on hybrid - end
        }         // for on opticalGroup - end

        // measure BeBoard occupancy
        DetectorDataContainer* cOccContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
        fDetectorDataContainer               = cOccContainer;
        measureBeBoardData(pBoard->getIndex(), fNevents);
        // figure out if zero was reached on all chips
        /*for(auto cOpticalGroup: *fDetectorDataContainer->at(pBoard->getIndex()))
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    // update counters
                    auto& cOccThisChip = cChip->getSummary<Occupancy, Occupancy>().fOccupancy;
                    LOG (INFO) << BOLDBLUE << "\t.. Occupancy on Chip" << +cChip->getId() << " on Hybrid" << +cHybrid->getId()
                        << " is " << cOccThisChip
                        << RESET;
                }//chip
            }// hybrid
        }//OG
        */
        float cGlbOcc = cOccContainer->getSummary<Occupancy, Occupancy>().fOccupancy;
        if(fThStep % 10 == 0) LOG(INFO) << BOLDBLUE << "ThScan [Step#" << +cOffset << "] global occupancy is " << cGlbOcc << RESET;
        bool cLimitFound = std::fabs(cGlbOcc - cTargetOccupancy) <= cLimit;

        // update cluster occupancy for this threshold
        const std::vector<Event*>& pEvents = this->GetEvents();
        UpdateClusterContainers(pBoard, pEvents, std::fabs(cOffset));

        cLimitReached += (cLimitFound) ? 1 : 0;
        cThresholdOffsets.push_back(cOffset);
        cOffset -= 1;
        fThStep++;
    } while(cLimitReached < cBreakCount);

    // make sure sparsification is reset
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", cSparsified);
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SetSparsification(cCic, cSparsified);
        }
    }
}
void BeamTestCheck::ScanL1Latency(uint8_t pContinousReadout)
{
    bool cValidate = false;
    LOG(INFO) << "Scanning Latency ... ContinousReadout set to " << +pContinousReadout << RESET;

    size_t cTotalNChnls = 0;
    size_t cNHybrids    = 0;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto opticalGroup: *cBoard)
        {
            cNHybrids += opticalGroup->size();
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid) { cTotalNChnls += chip->size(); } // chip
            }                                                             // hybrid
        }                                                                 // OG
    }

    // zero container that hold TDC information per board
    for(auto cBoard: *fDetectorContainer)
    {
        auto cTDCContainer = fTDCContainer.at(cBoard->getIndex());
        for(uint16_t cIndx = 0; cIndx < TDCBINS; cIndx++) { cTDCContainer->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx] = 0; }
    }

    // zero container
    // that hold latency per hybrid
    for(auto cBoard: *fDetectorContainer)
    {
        auto  cLatencyContainer      = fLatencyContainer.at(cBoard->getIndex());
        auto  cLatencyContainerS0    = fLatencyContainerS0.at(cBoard->getIndex());
        auto  cLatencyContainerS1    = fLatencyContainerS1.at(cBoard->getIndex());
        auto& cLatencyContainerCoinc = fLatencyContainerCoincidence.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(uint16_t cIndx = 0; cIndx < fLatencyRange; cIndx++)
                {
                    cLatencyContainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx]      = 0;
                    cLatencyContainerS0->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx]    = 0;
                    cLatencyContainerS1->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx]    = 0;
                    cLatencyContainerCoinc->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx] = 0;
                }
            } // hybrid
        }     // optical group
    }

    // container to hold trigger multiplicity per board
    DetectorDataContainer cBrdTriggerMult;
    ContainerFactory::copyAndInitBoard<uint32_t>(*fDetectorContainer, cBrdTriggerMult);
    for(auto cBoard: *fDetectorContainer)
    { cBrdTriggerMult.at(cBoard->getIndex())->getSummary<uint32_t>() = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity"); }

    for(auto cBoard: *fDetectorContainer)
    {
        setSameDacBeBoard(cBoard, "TriggerLatency", fStartLatency);
        fBeBoardInterface->ChipReSync(cBoard);
    }

    size_t cLatStep = 0;
    // need a container to hold maximum hit count per chip
    // and one to hold best latency per chip
    DetectorDataContainer cHitCount, cMaximumHitCount;
    ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, cMaximumHitCount);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cLat      = fOptimalL1Latency.at(cBoard->getIndex());
        auto& cMaxCount = cMaximumHitCount.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cLatThisOG      = cLat->at(cOpticalGroup->getIndex());
            auto& cMaxCountThisOG = cMaxCount->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cLatThisHybrid      = cLatThisOG->at(cHybrid->getIndex());
                auto& cMaxCountThisHybrid = cMaxCountThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cLatThisChip                        = cLatThisHybrid->at(cChip->getIndex());
                    auto& cMaxCountThisChip                   = cMaxCountThisHybrid->at(cChip->getIndex());
                    cLatThisChip->getSummary<uint16_t>()      = 0;
                    cMaxCountThisChip->getSummary<uint32_t>() = 0;
                }
            } // hybrid
        }     // optical group
    }

    // prepare container to hold hit information per chip
    DetectorDataContainer cHitContainer;
    ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, cHitContainer);
    bool cBreak = false;
    do
    {
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        uint16_t cLatency = fReadoutChipInterface->ReadChipReg(cChip, "TriggerLatency");
                        LOG(DEBUG) << BOLDMAGENTA << "L1 Latency for Chip#" << +cChip->getId() << " set to " << cLatency << RESET;
                    } // chip
                }     // hybrid
            }         // optical group
        }
        LOG(INFO) << BOLDBLUE << "Latency Step#" << +cLatStep << RESET;
        ContinousReadout();

        // check read-back events
        for(auto cBoard: *fDetectorContainer)
        {
            auto cBrdIndx = cBoard->getIndex();
            fBeBoardInterface->setBoard(cBoard->getId());
            const std::vector<Event*>& cEvents              = this->GetEvents();
            auto&                      cTriggerMult         = cBrdTriggerMult.at(cBoard->getIndex())->getSummary<uint32_t>();
            float                      cNormalizationFactor = fNevents;
            LOG(DEBUG) << BOLDMAGENTA << "Read-back " << +cEvents.size() << " from BeBoard#" << +cBoard->getId() << " - normalization factor for occupancy is " << +cNormalizationFactor << RESET;

            // data containers for this board
            auto& cLatencyContainer      = fLatencyContainer.at(cBrdIndx);
            auto& cLatencyContainerS0    = fLatencyContainerS0.at(cBrdIndx);
            auto& cLatencyContainerS1    = fLatencyContainerS1.at(cBrdIndx);
            auto& cLatencyContainerCoinc = fLatencyContainerCoincidence.at(cBrdIndx);

            for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++)
            {
                Count(cEvents, cTriggerId);
                // check if maximum hit count has been exceeded
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cHitContainerS0 = fHitOccupancyS0.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex());
                        auto& cHitContainerS1 = fHitOccupancyS1.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex());
                        for(auto cChip: *cHybrid)
                        {
                            auto  cLatency = fReadoutChipInterface->ReadChipReg(cChip, "TriggerLatency");
                            auto  cCrntCnt = cHitContainerS0->at(cChip->getIndex())->getSummary<uint32_t>() + cHitContainerS1->at(cChip->getIndex())->getSummary<uint32_t>();
                            auto& cMaxCnt  = cMaximumHitCount.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint32_t>();
                            auto& cLat     = fOptimalL1Latency.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                            if(cCrntCnt >= cMaxCnt && cCrntCnt > 0)
                            {
                                cLat = cLatency;
                                LOG(DEBUG) << BOLDYELLOW << "\t\t..New maximum found for Chip#" << +cChip->getId() << " on Hybrid#" << +cHybrid->getId() << " for an L1 latency of " << cLatency
                                           << " -- previous maximum was " << cMaxCnt << " -- now is " << cCrntCnt << RESET;
                                cMaxCnt = cCrntCnt;
                            }
                            else
                                LOG(DEBUG) << BOLDBLUE << "Chip#" << +cChip->getId() << " -- previous maximum was " << cMaxCnt << " -- current hit count is " << cCrntCnt << RESET;
                        } // chip
                    }     // hybrid
                }         // optical group
                // fill containers for DQMUtils
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cHitContainerS0  = fHitOccupancyS0.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex());
                        auto& cHitContainerS1  = fHitOccupancyS1.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex());
                        auto& cCoHitCointainer = fHitOccupancyCoinc.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex());

                        // get indices of SSAs
                        std::vector<uint8_t> cIndices(0);
                        std::vector<uint8_t> cSSAIds(0);
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::MPA2 || cChip->getFrontEndType() == FrontEndType ::CBC3)
                                cIndices.push_back(cChip->getIndex());
                            else
                                cSSAIds.push_back(cChip->getId());
                        }

                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::CBC3 || cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::MPA2)
                            {
                                auto& cHitsS0 = cHitContainerS0->at(cChip->getIndex())->getSummary<uint32_t>();
                                auto& cCoHits = cCoHitCointainer->at(cChip->getIndex())->getSummary<uint32_t>();
                                cLatencyContainerS0->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLatStep * (1 + cTriggerMult) - cTriggerId] += cHitsS0;
                                cLatencyContainer->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLatStep * (1 + cTriggerMult) - cTriggerId] += cHitsS0;
                                cLatencyContainerCoinc->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLatStep * (1 + cTriggerMult) - cTriggerId] += cCoHits;
                            }
                            if(cChip->getFrontEndType() == FrontEndType::CBC3 || cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2)
                            {
                                uint8_t cS1Index  = cChip->getIndex();
                                bool    cUpdateS1 = (cChip->getFrontEndType() == FrontEndType::CBC3) ? true : (std::find(cSSAIds.begin(), cSSAIds.end(), cChip->getId() % 8) != cSSAIds.end());
                                if(cUpdateS1 && cChip->getFrontEndType() == FrontEndType::CBC3)
                                    cS1Index = std::distance(cSSAIds.begin(), std::find(cSSAIds.begin(), cSSAIds.end(), cChip->getId() % 8));

                                auto& cHitsS1 = cHitContainerS1->at(cS1Index)->getSummary<uint32_t>();
                                cLatencyContainerS1->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLatStep * (1 + cTriggerMult) - cTriggerId] += cHitsS1;
                                cLatencyContainer->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLatStep * (1 + cTriggerMult) - cTriggerId] += cHitsS1;
                            }
                        }
                    }
                }
                // print-out
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cS0 =
                            cLatencyContainerS0->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLatStep * (1 + cTriggerMult) - cTriggerId];
                        auto& cS1 =
                            cLatencyContainerS1->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLatStep * (1 + cTriggerMult) - cTriggerId];
                        auto& cM =
                            cLatencyContainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLatStep * (1 + cTriggerMult) - cTriggerId];
                        if(cM > 0)
                        {
                            LOG(INFO) << BOLDYELLOW << "Hybrid" << +cHybrid->getId() << " Latency of " << (fStartLatency + cLatStep * (1 + cTriggerMult)) << " - trigger#" << +cTriggerId
                                      << " in a burst of " << (1 + cTriggerMult) << "... on average have found " << std::setprecision(2) << cM / cNormalizationFactor << " hit(s) per event."
                                      << BOLDMAGENTA << "In S0 " << cS0 << " hit(s); " << BOLDGREEN << "in S1 = " << cS1 << " hit(s)."
                                      << "Normalization done with " << cNormalizationFactor << " events." << RESET;
                        }
                        else
                            LOG(INFO) << BOLDBLUE << "Hybrid" << +cHybrid->getId() << " Latency of " << (fStartLatency + cLatStep * (1 + cTriggerMult)) << " - trigger#" << +cTriggerId
                                      << " in a burst of " << (1 + cTriggerMult) << "... on average have found " << std::setprecision(2) << cM / cNormalizationFactor << " hit(s) per event."
                                      << BOLDMAGENTA << "In S0 " << cS0 << " hit(s); " << BOLDGREEN << "in S1 = " << cS1 << " hit(s)."
                                      << "Normalization done with " << cNormalizationFactor << " events." << RESET;
                    }
                }
#ifdef __USE_ROOT__
                fDQMHistogrammer.fillLatencyPlots(fStartLatency + cLatStep * (1 + cTriggerMult), cTriggerId, *fDetectorDataContainer, fHitContainerTDC);
#endif
            }
        }
        // update trigger latency for next step
        bool cAllCompleted = true;
        for(auto cBoard: *fDetectorContainer)
        {
            auto& cTriggerMult = cBrdTriggerMult.at(cBoard->getIndex())->getSummary<uint32_t>();
            setSameDacBeBoard(cBoard, "TriggerLatency", fStartLatency + (1 + cLatStep) * (1 + cTriggerMult));
            cAllCompleted = cAllCompleted && ((1 + cLatStep) * (1 + cTriggerMult) >= fLatencyRange);
            fBeBoardInterface->ChipReSync(cBoard);
        }
        cBreak = cAllCompleted;
        cLatStep++;
    } while(!cBreak); // cLatStep < fLatencyRange);

    // set latency per FE ASIC
    float  cMeanLat = 0;
    size_t cNItems  = 0;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // std::map<uint8_t,uint16_t> cLatencies;
                for(auto cChip: *cHybrid)
                {
                    // if( cChip->getFrontEndType() == FrontEndType::SSA  ) continue;

                    auto& cLat = fOptimalL1Latency.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                    LOG(INFO) << BOLDMAGENTA << "Found optimal L1 Latency for Chip#" << +cChip->getId() << " to be " << cLat << RESET;
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLat);
                    // cLatencies[cChip->getId()%8]=cLat;
                    cMeanLat += cLat;
                    cNItems++;
                } // chip
                // for( auto cChip : *cHybrid )
                // {
                //     if( cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::CBC3  ) continue;
                //     LOG(INFO) << BOLDMAGENTA << "Will set SSA L1 Latency for Chip#" << +cChip->getId() << " to be " << cLatencies[cChip->getId()%8] << " - 1 " <<  RESET;
                //     fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatencies[cChip->getId()%8]-1);
                // }
            } // hybrid
        }     // optical group
        fBeBoardInterface->ChipReSync(cBoard);
    }
    //
    uint16_t cLatency = (uint16_t)(cMeanLat / cNItems);
    LOG(INFO) << BOLDMAGENTA << "Scan L1 Latency : average latency across all items is " << cLatency << RESET;
    if(cValidate)
    {
        // int  cValidateRange=3;
        ContinousReadout();
        // check read-back events
        for(auto cBoard: *fDetectorContainer)
        {
            fBeBoardInterface->setBoard(cBoard->getId());
            const std::vector<Event*>& cEvents      = this->GetEvents();
            size_t                     cTriggerMult = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
            for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++) { Count(cEvents, cTriggerId); }
        }
    }
}
void BeamTestCheck::Count(const std::vector<Event*> pEvents, size_t pTriggerId, uint8_t pFillCorrelations, uint8_t pPrint)
{
    // layer swaps [S0/S1] are stub seeds
    // read back from chips
    DetectorDataContainer cLyrSwp;
    ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, cLyrSwp);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2) continue;

                    auto& cLyrSwap = cLyrSwp.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint8_t>();
                    // 0 -- bottom sensor seed; 1 -- top sensor seed
                    if(fReadoutMode == 0)
                        cLyrSwap = fReadoutChipInterface->ReadChipReg(cChip, "LayerSwap");
                    else
                    {
                        if(cChip->getFrontEndType() == FrontEndType::CBC3)
                        {
                            uint8_t cRegValue = cChip->getReg("LayerSwap&CluWidth");
                            cLyrSwap          = (cRegValue & 0x08) >> 3;
                        }
                        else
                        {
                            uint8_t cBitShift = ECM_TABLE.find("StubMode")->second;
                            auto    cReg      = cChip->getReg("ECM");
                            uint8_t cRegMask  = (0x3 << cBitShift); //
                            uint8_t cValue    = (cReg & cRegMask) >> cBitShift;
                            cLyrSwap          = cValue & 0x1; // 0 -- pixels as seed; 1 --> strip as seed
                        }
                    }
                    LOG(DEBUG) << BOLDYELLOW << " Layer Swap on Chip#" << +cChip->getId() << " is " << +cLyrSwap << RESET;
                }
            }
        }
    }

    // Bend LUT
    // encoded bend
    DetectorDataContainer cBendLUT;
    ContainerFactory::copyAndInitChip<std::vector<uint8_t>>(*fDetectorContainer, cBendLUT);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2) continue;

                    auto& cLUT = cBendLUT.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<std::vector<uint8_t>>();
                    cLUT.clear();
                    if(cChip->getFrontEndType() == FrontEndType::CBC3)
                        cLUT = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(cChip, fReadoutMode);
                    else
                        cLUT = static_cast<PSInterface*>(fReadoutChipInterface)->readLUT(cChip, fReadoutMode);
                    // to-do.. check readLUT for MPAs
                }
            }
        }
    }

    // containers to hold occ [L1,Stubs] per event
    DetectorDataContainer cEventL1OccS0, cEventL1OccS1, cEventStubOcc;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, cEventL1OccS0);
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, cEventL1OccS1);
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, cEventStubOcc);

    // Cluster container
    DetectorDataContainer cClusterS0, cClusterS1, cStubsSingles;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, cClusterS0);
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, cClusterS1);
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, cStubsSingles);

    // zero count interesting events per hybrid
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cEvntSmry = fEventSubSet.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<std::vector<std::vector<uint32_t>>>();
                auto& cStbsSmry = fStubSubSet.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<std::vector<std::vector<uint32_t>>>();
                LOG(DEBUG) << cEvntSmry.size() << " \t " << cStbsSmry.size() << RESET;
                for(size_t cTDCbin = 0; cTDCbin < TDCBINS; cTDCbin++)
                {
                    std::vector<uint32_t> cPrepEvnts(BENDBINS, 0);
                    cEvntSmry.push_back(cPrepEvnts);
                    std::vector<uint32_t> cPrepStbs(BENDBINS, 0);
                    cStbsSmry.push_back(cPrepStbs);
                }
            }
        }
    }

    // check read-back events
    for(auto cBoard: *fDetectorContainer)
    {
        auto          cBrdIndx     = cBoard->getIndex();
        BeBoardRegMap cRegMap      = cBoard->getBeBoardRegMap();
        std::string   cMultRegName = "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity";
        size_t        cTriggerMult = (fReadoutMode == 0) ? fBeBoardInterface->ReadBoardReg(cBoard, cMultRegName) : cRegMap[cMultRegName];

        // zero hit containers for each sensor
        // for each TDC phase
        // and hit maps
        auto& cHitContainerS0      = fHitOccupancyS0.at(cBrdIndx);
        auto& cHitContainerS1      = fHitOccupancyS1.at(cBrdIndx);
        auto& cStubContainer       = fStubOccupancy.at(cBrdIndx);
        auto& cCoHitCointainer     = fHitOccupancyCoinc.at(cBrdIndx);
        auto& cClusterContainerS0  = cClusterS0.at(cBrdIndx);
        auto& cClusterContainerS1  = cClusterS1.at(cBrdIndx);
        auto& cSingleStubContainer = cStubsSingles.at(cBrdIndx);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(size_t cIndx = 0; cIndx < 30; cIndx++)
                {
                    fBendMap.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<BENDBINS, uint16_t>>()[cIndx]                  = 0;
                    fEventsWithSingleClusters.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<BENDBINS, uint16_t>>()[cIndx] = 0;
                    fEventsWithStubs.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<BENDBINS, uint16_t>>()[cIndx]          = 0;
                }

                for(auto cChip: *cHybrid)
                {
                    cHitContainerS0->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint32_t>()  = 0;
                    cHitContainerS1->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint32_t>()  = 0;
                    cCoHitCointainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint32_t>() = 0;
                    cStubContainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint32_t>()   = 0;
                    for(uint16_t cIndx = 0; cIndx < TDCBINS; cIndx++)
                    {
                        fHitContainerTDC.at(cBoard->getIndex())
                            ->at(cOpticalGroup->getIndex())
                            ->at(cHybrid->getIndex())
                            ->at(cChip->getIndex())
                            ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx] = 0;
                    }

                    for(uint32_t cIndx = 0; cIndx < cChip->size(); cIndx++)
                    {
                        fHitMap.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cIndx).fOccupancy  = 0;
                        fStubMap.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cIndx).fOccupancy = 0;
                        cClusterContainerS0->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cIndx).fOccupancy             = 0;
                        cClusterContainerS1->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cIndx).fOccupancy             = 0;
                        cSingleStubContainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cIndx).fOccupancy            = 0;
                    }
                } // chip
            }     // hybrid
        }         // optical group

        // start at the beginning + trigger id in burst
        auto                   cEventIter            = pEvents.begin() + pTriggerId;
        size_t                 cEventCount           = 0;
        DetectorDataContainer* theOccupancyContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
        fDetectorDataContainer                       = theOccupancyContainer;

        auto cMaxEventsToProc = (fNevents >= 10000) ? 10000 : fNevents;
        do
        {
            if(cEventIter >= pEvents.end()) break;

            uint8_t cTDCVal = (*cEventIter)->GetTDC();
            fTDCContainer.at(cBrdIndx)->getSummary<GenericDataArray<TDCBINS, uint16_t>>()[cTDCVal]++;

            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    // auto              cL1IdCIC  = static_cast<D19cCic2Event*>(*cEventIter)->L1Id(cHybrid->getId(), 0);
                    auto cL1Status = static_cast<D19cCic2Event*>(*cEventIter)->L1Status(cHybrid->getId());
                    auto cBxId     = (*cEventIter)->BxId(cHybrid->getId());
                    auto cStubStat = static_cast<D19cCic2Event*>(*cEventIter)->Status(cHybrid->getId());
                    LOG(DEBUG) << BOLDYELLOW << "Event#" << (*cEventIter)->GetEventCount() << " BxId " << +cBxId << " L1 Status " << std::bitset<9>(cL1Status) << " Stub Status "
                               << std::bitset<8>(cStubStat) << RESET;
                    auto&                cOccHybrid = fDetectorDataContainer->at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex());
                    std::vector<uint8_t> cIndices(0);
                    std::vector<uint8_t> cSSAIds(0);
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::MPA2 || cChip->getFrontEndType() == FrontEndType ::CBC3)
                            cIndices.push_back(cChip->getIndex());
                        else
                            cSSAIds.push_back(cChip->getId());
                    }
                    size_t cIndx = 0;

                    // make sure occupancy for this event
                    // this hybrid
                    // is set to 0
                    for(auto cChip: *cHybrid)
                    {
                        // make sure stub map for this event is set to 0
                        for(uint32_t cCh = 0; cCh < cChip->size(); cCh++)
                        {
                            cEventL1OccS0.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cCh).fOccupancy = 0;
                            cEventL1OccS1.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cCh).fOccupancy = 0;
                            cEventStubOcc.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cCh).fOccupancy = 0;
                        }
                    } // chips

                    auto& cEvntSmry = fEventSubSet.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<std::vector<std::vector<uint32_t>>>();
                    auto& cStbsSmry = fStubSubSet.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<std::vector<std::vector<uint32_t>>>();
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2) continue;

                        auto  cStubs   = (*cEventIter)->StubVector(cHybrid->getId(), cChip->getId());
                        auto& cLyrSwap = cLyrSwp.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint8_t>();
                        // if its a CBC .. look for events with exactly 2 clusters
                        if(cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::MPA2)
                        {
                            auto cStripClusters = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(cHybrid->getId(), cChip->getId());
                            auto cPxlClusters   = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(cHybrid->getId(), cChip->getId());
                            if(cStripClusters.size() == cPxlClusters.size() && cPxlClusters.size() == 1) // events with exactly one cluster in each sensor
                            {
                                double cCenterOfMassP = 0;
                                bool   cSingles       = true;
                                for(auto cPxlCluster: cPxlClusters)
                                {
                                    uint32_t cRow = cPxlCluster.fAddress - 1;
                                    uint32_t cCol = cPxlCluster.fZpos;
                                    if(cPxlCluster.fWidth != 0)
                                    {
                                        cSingles = false;
                                        continue;
                                    }
                                    cClusterContainerS0->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cRow, cCol).fOccupancy++;
                                    for(uint8_t cOff = 0; cOff <= cPxlCluster.fWidth; cOff++) cCenterOfMassP += cPxlCluster.fAddress + cOff;
                                    cCenterOfMassP /= (1 + cPxlCluster.fWidth);
                                }
                                double cCenterOfMassS = 0;
                                for(auto cStrpCluster: cStripClusters)
                                {
                                    uint32_t cRow = cStrpCluster.fAddress - 1;
                                    uint32_t cCol = 0;
                                    if(cStrpCluster.fWidth != 0)
                                    {
                                        cSingles = false;
                                        continue;
                                    }
                                    cClusterContainerS1->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cRow, cCol).fOccupancy++;
                                    for(uint8_t cOff = 0; cOff <= cStrpCluster.fWidth; cOff++) cCenterOfMassS += cStrpCluster.fAddress + cOff;
                                    cCenterOfMassS /= (1 + cStrpCluster.fWidth);
                                }
                                // cSingles = cSingles && (cTDCVal == 3 );
                                // cSingles = cSingles && (cTDCVal < 2 || cTDCVal > 4 );
                                for(auto cStub: cStubs)
                                {
                                    if(!cSingles) continue;
                                    uint32_t cRow = std::floor(cStub.getPosition() / 2.0);
                                    uint32_t cCol = cStub.getRow();
                                    cSingleStubContainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cRow, cCol).fOccupancy++;
                                }
                                double cBend                = 0;
                                double cDifference          = (cLyrSwap == 0) ? (cCenterOfMassS - cCenterOfMassP) : (cCenterOfMassP - cCenterOfMassS);
                                cBend                       = (std::fabs(cDifference) <= 3.5) ? cDifference : 7.0;
                                double            cDiffIndx = (cBend - (-7.5)) / 0.5;
                                std::stringstream cOut;
                                cOut << "Event#" << (*cEventIter)->GetEventCount() << " BxId " << +cBxId << " L1 Status " << std::bitset<9>(cL1Status) << " Stub Status " << std::bitset<8>(cStubStat)
                                     << " Hybrid#" << +cHybrid->getId() << " Chip# " << +cChip->getId() << " found " << +cStubs.size() << " stubs"
                                     << " and exactly 1 cluster in each of S0 + S1 "
                                     << " center of mass P-Cluster " << cCenterOfMassP << " center of mass S-Cluster " << cCenterOfMassS << " bend from clusters is " << cBend << " [ index is "
                                     << cDiffIndx << " ]\n";
                                for(auto cPxlCluster: cPxlClusters)
                                {
                                    uint32_t cRow = cPxlCluster.fAddress - 1;
                                    uint32_t cCol = cPxlCluster.fZpos;
                                    if(cPxlCluster.fWidth != 0) continue;
                                    cOut << "\t\t\t\t\t.. P cluster  - row " << cRow << " column " << cCol << "\n";
                                }
                                bool cStubFound = (cStubs.size() > 0);
                                for(auto cStub: cStubs)
                                {
                                    if(!cSingles) continue;
                                    uint32_t cRow = std::floor(cStub.getPosition() / 2.0) - 1;
                                    uint32_t cCol = cStub.getRow();
                                    cOut << "\t\t\t\t\t.. stub in row " << +cRow << " and column " << +cCol << RESET;
                                }

                                if(cSingles)
                                {
                                    fEventsWithSingleClusters.at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->getSummary<GenericDataArray<BENDBINS, uint16_t>>()[cDiffIndx]++;
                                    fEventsWithStubs.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<BENDBINS, uint16_t>>()[cDiffIndx] +=
                                        cStubs.size();
                                    cEvntSmry[cTDCVal][cDiffIndx]++;
                                    cStbsSmry[cTDCVal][cDiffIndx] += cStubs.size();
                                }
                                if(cSingles && cStubFound && pPrint)
                                    LOG(INFO) << BOLDGREEN << "!!!" << cOut.str() << RESET;
                                else if(cSingles && !cStubFound && pPrint)
                                    LOG(INFO) << BOLDRED << "!!!" << cOut.str() << RESET;
                            }
                        }
                        if(pPrint)
                            LOG(DEBUG) << BOLDYELLOW << "Event#" << (*cEventIter)->GetEventCount() << " BxId " << +cBxId << " L1 Status " << std::bitset<9>(cL1Status) << " Stub Status "
                                       << std::bitset<8>(cStubStat) << " Hybrid#" << +cHybrid->getId() << " Chip# " << +cChip->getId() << " found " << +cStubs.size() << " stubs." << RESET;
                        auto& cLUT = cBendLUT.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<std::vector<uint8_t>>();
                        // loop over stubs and count
                        for(auto cStub: cStubs)
                        {
                            // update hit map
                            uint32_t cSeedChnl = 0;
                            uint16_t cRow      = 0;
                            uint16_t cCol      = 0;
                            if(cChip->getFrontEndType() == FrontEndType::CBC3)
                            {
                                uint32_t cSeedStrip = std::floor(cStub.getPosition() / 2.0); // counting from 1
                                // The default setting uses the even number channels (252:0) for the Seed Layer
                                cSeedChnl = (cLyrSwap == 0) ? 2 * (cSeedStrip - 1) : +2 * (cSeedStrip - 1) + 1;
                                cRow      = cSeedChnl;
                            }
                            else
                            {
                                cRow = std::floor(cStub.getPosition() / 2.0);
                                cCol = cStub.getRow();
                            }
                            // update bend map
                            // each bend code is stored in this vector - bend encoding start at -7 strips, increments by 0.5 strips
                            int   cBendIndx = std::distance(cLUT.begin(), std::find(cLUT.begin(), cLUT.end(), cStub.getBend()));
                            float cBend     = -7.0 + cBendIndx * 0.5;
                            fBendMap.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<BENDBINS, uint16_t>>()[cBendIndx]++;
                            // if(pPrint)

                            uint16_t cMaxRows     = (cChip->getFrontEndType() == FrontEndType::CBC3) ? cChip->size() : NSSACHANNELS;
                            uint16_t cMaxCols     = (cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::MPA2) ? NMPACOLS : 1;
                            bool     cValidCoords = (cRow < cMaxRows && cCol < cMaxCols);

                            if(!cValidCoords)
                                LOG(INFO) << BOLDRED << "\t\t.. Stub in strip# " << +cStub.getPosition() << " Row# " << +cStub.getRow() << " Bend " << +cStub.getBend()
                                          << " i.e. seed in Chip channel# [R" << +cRow << ",C" << +cCol << " bend in strips is " << cBend << " bend index is " << cBendIndx << RESET;
                            else
                                LOG(DEBUG) << BOLDBLUE << "\t\t.. Hybrid#" << +cHybrid->getId() << " Chip#" << +cChip->getId() << " Stub in strip# " << +cStub.getPosition() << " Row# "
                                           << +cStub.getRow() << " Bend " << +cStub.getBend() << " i.e. seed in Chip channel# [R" << +cRow << ",C" << +cCol << " bend in strips is " << cBend
                                           << " bend index is " << cBendIndx << RESET;
                            // update stub map
                            if(cValidCoords)
                            {
                                fStubMap.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cRow, cCol).fOccupancy++;
                                cEventStubOcc.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cRow, cCol).fOccupancy = 1;
                            }
                        }
                        cStubContainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint32_t>() += cStubs.size();

                        std::vector<uint16_t> cTmpS0;
                        std::vector<uint16_t> cTmpS1;
                        uint16_t              cMaxRows = (cChip->getFrontEndType() == FrontEndType::CBC3) ? cChip->size() : NSSACHANNELS;
                        cTmpS0.assign(cMaxRows, 0);
                        cTmpS1.assign(cMaxRows, 0);
                        bool    cSSAExists = std::find(cSSAIds.begin(), cSSAIds.end(), cChip->getId() % 8) != cSSAIds.end();
                        uint8_t cSSAIndex  = (cSSAExists) ? std::distance(cSSAIds.begin(), std::find(cSSAIds.begin(), cSSAIds.end(), cChip->getId() % 8)) : 0;
                        if(cSSAExists) LOG(DEBUG) << BOLDYELLOW << "MPA#" << +cChip->getId() << " SSA Index " << +cSSAIndex << RESET;

                        // loop over hits and count
                        auto cHits = (*cEventIter)->GetHits(cHybrid->getId(), cChip->getId());
                        if(cHits.size() > 0) LOG(DEBUG) << BOLDBLUE << "Event#" << (*cEventIter)->GetEventCount() << " Chip#" << +(cChip->getId() % 8) << "   " << +cHits.size() << " hits." << RESET;
                        for(auto cHit: cHits)
                        {
                            if(pPrint) LOG(DEBUG) << BOLDYELLOW << "Hybrid#" << +cHybrid->getId() << " Chip# " << +cChip->getId() << " Channel " << cHit << RESET;
                            auto& cOccChip = cOccHybrid->at(cChip->getIndex());
                            LOG(DEBUG) << cOccChip->getChannel<Occupancy>(0).fOccupancy << RESET;
                            uint16_t cRow = (cChip->getFrontEndType() == FrontEndType::CBC3) ? cHit : 0;
                            uint16_t cCol = 0;
                            // sensor iD - 0 -- bottoml; 1 -- top
                            uint8_t cSensorID = (cChip->getFrontEndType() == FrontEndType::CBC3) ? (cHit % 2 != 0) : (cCol != 0);
                            // uint16_t cMaxRows  = (cChip->getFrontEndType() == FrontEndType::CBC3) ? cChip->size() : 0;
                            // if(cChip->getFrontEndType() == FrontEndType::MPA) cMaxRows = NSSACHANNELS;
                            uint16_t cMaxCols = 1;
                            if((cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::MPA2) && cSensorID == 0) cMaxCols = NMPACOLS;

                            if(cChip->getFrontEndType() != FrontEndType::CBC3)
                            {
                                uint8_t cZPos    = (cHit >> 24) & 0xFF;
                                cSensorID        = (cZPos == 0) ? 1 : 0;
                                uint8_t cAddress = (cHit >> 8) & 0x7F;
                                uint8_t cId      = cHit & 0xFF;
                                cRow             = cAddress + cId;
                                cCol             = (cZPos == 0) ? cZPos : cZPos - 1;
                                if(cRow == cMaxRows || cCol == cMaxCols)
                                    LOG(INFO) << BOLDRED << "Event#" << (*cEventIter)->GetEventCount() << " Hybrid#" << +cHybrid->getId() << " Chip# " << +cChip->getId() << " S" << +cSensorID
                                              << " Address " << +cAddress << " , Zpos " << +cZPos << " id " << +cId << " Row " << +cRow << " Column " << +cCol << RESET;
                                else if(pPrint)
                                    LOG(INFO) << BOLDBLUE << "Event#" << (*cEventIter)->GetEventCount() << " Hybrid#" << +cHybrid->getId() << " Chip# " << +cChip->getId() << " S" << +cSensorID
                                              << " Address " << +cAddress << " , Zpos " << +cZPos << " id " << +cId << " Row " << +cRow << " Column " << +cCol << RESET;
                            }
                            else
                            {
                                LOG(DEBUG) << BOLDBLUE << "Event#" << (*cEventIter)->GetEventCount() << " Hybrid#" << +cHybrid->getId() << " Chip# " << +cChip->getId() << " S" << +cSensorID << " Row "
                                           << +cRow << " Column " << +cCol << RESET;
                            }
                            bool cValidCoords = (cRow < cMaxRows && cCol < cMaxCols);
                            if(cSensorID == 0)
                            {
                                cHitContainerS0->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint32_t>()++;
                                // update hit container for each TDC phase
                                fHitContainerTDC.at(cBoard->getIndex())
                                    ->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->at(cChip->getIndex())
                                    ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cTDCVal]++;
                            }
                            else if(cChip->getFrontEndType() != FrontEndType::CBC3 && cSSAExists)
                            {
                                cHitContainerS1->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cSSAIndex)->getSummary<uint32_t>()++;
                                // cHitContainerS1->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cSSAIndices[cIndx])->getSummary<uint32_t>()++;
                                // update hit container for each TDC phase
                                fHitContainerTDC.at(cBoard->getIndex())
                                    ->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->at(cSSAIndex)
                                    ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cTDCVal]++;
                            }
                            else
                            {
                                cHitContainerS1->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint32_t>()++;
                                // update hit container for each TDC phase
                                fHitContainerTDC.at(cBoard->getIndex())
                                    ->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->at(cChip->getIndex())
                                    ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cTDCVal]++;
                            }

                            if(cEventCount == 0 && cValidCoords) { cOccChip->getChannel<Occupancy>(cRow, cCol).fOccupancy = 1; }
                            else if(cValidCoords)
                            {
                                cOccChip->getChannel<Occupancy>(cRow, cCol).fOccupancy++;
                            }

                            if(cSensorID == 0 && cValidCoords)
                                cEventL1OccS0.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cRow, cCol).fOccupancy = 1;
                            else if(cValidCoords)
                                cEventL1OccS1.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cRow, cCol).fOccupancy = 1;

                            if(cValidCoords && cSensorID == 0 && pPrint)
                                LOG(INFO) << BOLDYELLOW << " R" << +cRow << " C" << +cCol << " S0 "
                                          << cEventL1OccS0.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cRow, cCol).fOccupancy
                                          << RESET;
                            if(cValidCoords && cSensorID == 1 && pPrint)
                                LOG(INFO) << BOLDYELLOW << " R" << +cRow << " C" << +cCol << " S1 "
                                          << cEventL1OccS1.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cRow, cCol).fOccupancy
                                          << RESET;
                            // update hit map
                            if(cChip->getFrontEndType() == FrontEndType::CBC3 && cValidCoords)
                            {
                                fHitMap.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cIndices[cIndx])->getChannel<Occupancy>(cRow, cCol).fOccupancy++;
                                cTmpS0[cRow]++;
                            }
                            else if(cSensorID == 0 && cValidCoords)
                            {
                                fHitMap.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cIndices[cIndx])->getChannel<Occupancy>(cRow, cCol).fOccupancy++;
                                cTmpS0[cRow]++;
                            }
                            else if(cValidCoords && cSSAExists)
                            {
                                // find correct SSA Index
                                fHitMap.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cSSAIndex)->getChannel<Occupancy>(cRow, cCol).fOccupancy++;
                                // fHitMap.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cSSAIndices[cIndx])->getChannel<Occupancy>(cRow,
                                // cCol).fOccupancy++;
                                cTmpS1[cRow]++;
                            }
                            if(pPrint && cValidCoords)
                            {
                                if(cSensorID == 0)
                                    LOG(INFO)
                                        << BOLDGREEN << " Hybrid#" << +cHybrid->getId() << " Chip# " << +cChip->getId() << " Row " << +cRow << " , Col " << +cCol << " occ. "
                                        << fHitMap.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cRow, cCol).fOccupancy
                                        << RESET;
                                else
                                    LOG(INFO)
                                        << BOLDMAGENTA << " Hybrid#" << +cHybrid->getId() << " Chip# " << +cChip->getId() << " Row " << +cRow << " , Col " << +cCol << " occ. "
                                        << fHitMap.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(cRow, cCol).fOccupancy
                                        << RESET;
                            }
                        }

                        // now check for coincidences
                        size_t cNCoincidences = 0;
                        for(auto cRow = 0; cRow < cMaxRows; cRow++)
                        {
                            bool cCoincident = (cTmpS0[cRow] == cTmpS1[cRow] && cTmpS0[cRow] > 0);
                            cNCoincidences += (cCoincident) ? 1 : 0;
                            if(pPrint && cCoincident) LOG(INFO) << BOLDYELLOW << "\t\t\t\t... found  a coincidence in row " << +cRow << RESET;
                        }
                        cCoHitCointainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint32_t>() += cNCoincidences;
                        cIndx++;
                    } // chip vector
                }     // hybrid vector
            }         // optical group vector

            // fill correlation plot
            if(pFillCorrelations)
            {
#ifdef __USE_ROOT__
                fDQMHistogrammer.fillCorrelations(cEventL1OccS0, cEventL1OccS1, cEventStubOcc);
#endif
            }
            cEventIter += (1 + cTriggerMult);
            cEventCount++;
        } while(cEventIter < pEvents.end() && cEventCount < cMaxEventsToProc); // I've only asked to look at fNEvents
    }

    for(auto cBoard: *fDetectorContainer)
    {
        auto cBrdIndx = cBoard->getIndex();
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cHitContainerS0  = fHitOccupancyS0.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex());
                auto& cHitContainerS1  = fHitOccupancyS1.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex());
                auto& cStubContainer   = fStubOccupancy.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex());
                auto& cCoHitCointainer = fHitOccupancyCoinc.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex());

                auto& cEvntSmry = fEventSubSet.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<std::vector<std::vector<uint32_t>>>();
                auto& cStbsSmry = fStubSubSet.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<std::vector<std::vector<uint32_t>>>();

                for(size_t cTDCbin = 0; cTDCbin < TDCBINS; cTDCbin++)
                {
                    for(size_t cBn = 0; cBn < cEvntSmry[cTDCbin].size(); cBn++)
                    {
                        if(cEvntSmry[cTDCbin][cBn] > 0)
                            LOG(INFO) << BOLDBLUE << "Hybrid#" << +cHybrid->getId() << " TDC bin " << +cTDCbin << " BendBin " << +cBn << " -- event summary : " << cEvntSmry[cTDCbin][cBn]
                                      << " -- stub summary : " << cStbsSmry[cTDCbin][cBn] << RESET;
                    }
                } // print-out count per TDC

                for(auto cChip: *cHybrid)
                {
                    // if(cChip->getFrontEndType() == FrontEndType::MPA )
                    // {
                    //     auto& cEventsWithSingles =
                    //     fEventsWithSingleClusters.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<BENDBINS,
                    //     uint16_t>>(); auto& cEventsWithStubs  =
                    //     fEventsWithStubs.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<BENDBINS, uint16_t>>(); float
                    //     cNevents=0; float cNstubs=0; for( size_t cIndx=0; cIndx < BENDBINS ; cIndx++) cNevents+= cEventsWithSingles[cIndx]; for( size_t cIndx=0; cIndx < BENDBINS ; cIndx++)
                    //     cNstubs+= cEventsWithStubs[cIndx];

                    //     LOG(INFO) << BOLDMAGENTA << "Counting step... Trigger#" << +pTriggerId << " Hybrid#" << +cHybrid->getId() << " Chip#" << +(cChip->getId())
                    //         << " " << cNevents
                    //         << " events with exactly one cluster of size 1 in each layer "
                    //         << " and " << +cNstubs
                    //         << " stubs found "
                    //         << " stub matching efficiecny is " << (float)cNstubs/cNevents
                    //         << RESET;
                    // }

                    if(cChip->getFrontEndType() == FrontEndType::CBC3 && cHitContainerS0->at(cChip->getIndex())->getSummary<uint32_t>() > 0)
                        LOG(INFO) << BOLDBLUE << "Counting step...Trigger#" << +pTriggerId << " Hybrid#" << +cHybrid->getId() << " Chip#" << +(cChip->getId())
                                  << " Stubs   : " << cStubContainer->at(cChip->getIndex())->getSummary<uint32_t>() << " stubs."
                                  << " Hits S0 : " << cHitContainerS0->at(cChip->getIndex())->getSummary<uint32_t>() << " hits."
                                  << " Hits S1 : " << cHitContainerS1->at(cChip->getIndex())->getSummary<uint32_t>() << " hits." << RESET;
                    else if((cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::MPA2) && cHitContainerS0->at(cChip->getIndex())->getSummary<uint32_t>() > 0)
                        LOG(INFO) << BOLDMAGENTA << "Counting step... Trigger#" << +pTriggerId << " Hybrid#" << +cHybrid->getId() << " Chip#" << +(cChip->getId())
                                  << " Stubs   : " << cStubContainer->at(cChip->getIndex())->getSummary<uint32_t>() << " stubs."
                                  << " Hits S0 : " << cHitContainerS0->at(cChip->getIndex())->getSummary<uint32_t>() << " hits."
                                  << " and found " << cCoHitCointainer->at(cChip->getIndex())->getSummary<uint32_t>() << " hits in the same row as S1 " << RESET;
                    else if((cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2) && cHitContainerS1->at(cChip->getIndex())->getSummary<uint32_t>() > 0)
                    {
                        LOG(INFO) << BOLDGREEN << "Counting step... Trigger#" << +pTriggerId << " Hybrid#" << +cHybrid->getId() << " Chip#" << +(cChip->getId())
                                  << " Hits S1 : " << cHitContainerS1->at(cChip->getIndex())->getSummary<uint32_t>() << " hits." << RESET;
                    }
                }
            }
        }
    }
}
void BeamTestCheck::ScanLatency(BeBoard* pBoard, uint8_t pContinousReadout)
{
    // bool cUseReadNevents = false;
    LOG(INFO) << "Scanning Latency ... ContinousReadout set to " << +pContinousReadout << RESET;
    ;
    size_t cTotalNChnls = 0;
    size_t cNHybrids    = 0;
    for(auto board: *fDetectorContainer)
    {
        for(auto opticalGroup: *board)
        {
            cNHybrids += opticalGroup->size();
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid) { cTotalNChnls += chip->size(); } // chip
            }                                                             // hybrid
        }                                                                 // OG
    }                                                                     // board

    // zero container that hold TDC information per board
    auto cTDCContainer = fTDCContainer.at(pBoard->getIndex());
    for(uint16_t cIndx = 0; cIndx < TDCBINS; cIndx++) { cTDCContainer->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx] = 0; }

    // zero container
    // latency per hybrid
    auto cLatencyContainer   = fLatencyContainer.at(pBoard->getIndex());
    auto cLatencyContainerS0 = fLatencyContainerS0.at(pBoard->getIndex());
    auto cLatencyContainerS1 = fLatencyContainerS1.at(pBoard->getIndex());
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(uint16_t cIndx = 0; cIndx < fLatencyRange; cIndx++)
            {
                cLatencyContainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx]   = 0;
                cLatencyContainerS0->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx] = 0;
                cLatencyContainerS1->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx] = 0;
            }
        } // hybrid
    }     // optical group

    // use ReadDataRather than ReadNEvents
    auto cRefSensor = findValueInSettings<double>("Check2SRefSensor", 0);
    auto cRefSide   = findValueInSettings<double>("Check2SRefSide", 0);
    auto cRefChip   = findValueInSettings<double>("Check2SRefChip", 0);

    fUseReadNEvents   = findValueInSettings<double>("Check2SUseReadNEvents", 0);
    fWait_ms          = findValueInSettings<double>("Check2Swait", 0);
    uint16_t cLat     = fStartLatency;
    float    cMaxHits = 0;
    fOptimalLatency   = cLat;
    do
    {
        setSameDacBeBoard(pBoard, "TriggerLatency", cLat);
        fBeBoardInterface->ChipReSync(pBoard);

        uint16_t cOffset      = 0;
        auto     cBrdIndx     = pBoard->getIndex();
        size_t   cTriggerMult = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");

        if(pContinousReadout == 1)
            ContinousReadout(pBoard);
        else
            ReadNEvents(pBoard, fNevents);

        const std::vector<Event*>& cEvents              = this->GetEvents();
        float                      cNormalizationFactor = cEvents.size() / (1 + cTriggerMult);
        // loop over triggers in the burst
        for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++)
        {
            if((cLat + cTriggerId) >= (fStartLatency + fLatencyRange)) continue;

            // prepare container to hold hit information per chip
            DetectorDataContainer cHitContainer;
            ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, cHitContainer);
            // zero hit container
            for(auto cOpticalGroup: *pBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        for(uint16_t cIndx = 0; cIndx < TDCBINS; cIndx++)
                        {
                            cHitContainer.at(pBoard->getIndex())
                                ->at(cOpticalGroup->getIndex())
                                ->at(cHybrid->getIndex())
                                ->at(cChip->getIndex())
                                ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx] = 0;
                        }
                    } // chip
                }     // hybrid
            }         // optical group

            // start at the beginning + trigger id in burst
            auto cEventIter  = cEvents.begin() + cTriggerId;
            fNReadbackEvents = cEvents.size();
            // calculate occupancy for each
            DetectorDataContainer* theOccupancyContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
            fDetectorDataContainer                       = theOccupancyContainer;
            fSCurveOccupancyMap[cLat + cTriggerId]       = theOccupancyContainer;
            auto& cOccBrd                                = theOccupancyContainer->at(cBrdIndx);
            int   cTotalHits                             = 0;
            int   cTotalHitsS0                           = 0;
            int   cTotalHitsS1                           = 0;

            int cRefHits = 0;
            do
            {
                if(cEventIter >= cEvents.end()) break;
                uint8_t cTDCVal = (*cEventIter)->GetTDC();
                cTDCContainer->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cTDCVal]++;

                //(*cEventIter)->fillDataContainer(cOccBrd, fChannelGroupHandler->allChannelGroup());
                for(auto cOpticalGroup: *pBoard)
                {
                    auto& cOccOG = cOccBrd->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cOccHybrid = cOccOG->at(cHybrid->getIndex());

                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;

                            auto cHits = (*cEventIter)->GetHits(cHybrid->getId(), cChip->getId());
                            LOG(DEBUG) << BOLDBLUE << "Event#" << (*cEventIter)->GetEventCount() << "Chip#" << +cChip->getId() % 8 << " " << +cHits.size() << " hits." << RESET;
                            cTotalHits += cHits.size();
                            for(auto cHit: cHits)
                            {
                                if(cHit % 2 == cRefSensor)
                                {
                                    if(cHybrid->getId() % 2 == cRefSide)
                                    {
                                        if(cRefChip == cChip->getId()) { cRefHits++; }
                                    }
                                }
                                if(cHit % 2 == 0)
                                {
                                    cLatencyContainerS0->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLat + cTriggerId - fStartLatency]++;
                                    cTotalHitsS0++;
                                }
                                else
                                {
                                    cLatencyContainerS1->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLat + cTriggerId - fStartLatency]++;
                                    cTotalHitsS1++;
                                }
                                cLatencyContainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLat + cTriggerId - fStartLatency]++;
                                cHitContainer.at(pBoard->getIndex())
                                    ->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->at(cChip->getIndex())
                                    ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cTDCVal] += 1;
                                auto& cOccChip = cOccHybrid->at(cChip->getIndex());
                                cOccChip->getChannel<Occupancy>(cHit).fOccupancy++;
                            }
                        } // chip vector
                    }     // hybrid vector
                }         // optical group vector
                cEventIter += (1 + cTriggerMult);
            } while(cEventIter < cEvents.end());
            cOccBrd->normalizeAndAverageContainers(fDetectorContainer->at(cBrdIndx), getChannelGroupHandlerContainer()->getObject(cOccBrd->getId()), fNReadbackEvents);
            // float cOccGlbl = cOccBrd->getSummary<Occupancy, Occupancy>().fOccupancy;
            cTotalHits = cTotalHitsS0 + cTotalHitsS1;

            if(cTotalHits > 0)
            {
                if(cRefHits >= cMaxHits)
                {
                    fOptimalLatency = cLat;
                    LOG(INFO) << BOLDYELLOW << "[!!!! new max !!!!]Latency of " << (cLat + cTriggerId) << " - trigger#" << +cTriggerId << " in a burst of " << (1 + cTriggerMult)
                              << "... on average have found " << std::setprecision(2) << cTotalHits / cNormalizationFactor << " hit(s) per event."
                              << "In S0 " << cTotalHitsS0 << " hit(s); in S1 = " << cTotalHitsS1 << " hit(s)."
                              << "... optimal latency will be set to " << cLat << ". Normalization done with " << fNReadbackEvents << " events." << RESET;
                    cMaxHits = cRefHits;
                }
                else
                    LOG(INFO) << BOLDBLUE << "Latency of " << (cLat + cTriggerId) << " - trigger#" << +cTriggerId << " in a burst of " << (1 + cTriggerMult) << "... on average have found "
                              << std::setprecision(2) << cTotalHits / cNormalizationFactor << " hit(s) per event."
                              << "In S0 " << cTotalHitsS0 << " hit(s); in S1 = " << cTotalHitsS1 << " hit(s)."
                              << "Normalization done with " << fNReadbackEvents << " events." << RESET;
            }
            else
                LOG(INFO) << BOLDBLUE << "Latency of " << (cLat + cTriggerId) << " - trigger#" << +cTriggerId << " in a burst of " << (1 + cTriggerMult) << "... on average have found "
                          << std::setprecision(2) << cTotalHits / cNormalizationFactor << " hit(s) per event."
                          << "In S0 " << cTotalHitsS0 << " hit(s); in S1 = " << cTotalHitsS1 << " hit(s) "
                          << " normalization done with " << fNReadbackEvents << " events." << RESET;
#ifdef __USE_ROOT__
            fDQMHistogrammer.fillLatencyPlots(cLat + cTriggerId, cTriggerId, *theOccupancyContainer, cHitContainer);
#endif
        }
        if(cOffset < (1 + cTriggerMult)) cOffset = (1 + cTriggerMult);
        cLat += cOffset;
    } while(cLat < fStartLatency + fLatencyRange);

    LOG(INFO) << BOLDYELLOW << "Optimal latency found to be : " << fOptimalLatency << " 40 MHz clock cycles [L1 data]" << RESET;
}
void BeamTestCheck::ScanStubLatency(uint8_t pContinousReadout)
{
    // bool cUseReadNevents = false;
    LOG(INFO) << "Scanning Stub Latency ... ContinousReadout set to " << +pContinousReadout << RESET;
    // stub offset already set for this board
    // LOG(INFO) << BOLDBLUE << "Stub offset for BeBoard#" << +pBoard->getId() << " set to " << +pBoard->getStubOffset() << RESET;
    // figure out latency scan range
    DetectorDataContainer cBrdLatency;
    ContainerFactory::copyAndInitBoard<uint16_t>(*fDetectorContainer, cBrdLatency);
    for(auto cBoard: *fDetectorContainer)
    {
        cBrdLatency.at(cBoard->getIndex())->getSummary<uint16_t>() = 0;
        uint16_t cMinLatency                                       = 0;
        uint16_t cMaxLatency                                       = 0;
        // get mode for stub latency
        std::vector<uint16_t> cLatencyBins(512, 0);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2) continue;

                    auto cLat = fReadoutChipInterface->ReadChipReg(cChip, "TriggerLatency");
                    if(cLat > 0) cLatencyBins[cLat]++;
                    if(cMinLatency == 0) cMinLatency = cLat;
                    if(cMaxLatency == 0) cMaxLatency = cLat;

                    if(cLat > cMaxLatency) cMaxLatency = cLat;
                    if(cLat < cMaxLatency) cMinLatency = cLat;
                }
            }
        }
        auto cModeLatency = std::max_element(cLatencyBins.begin(), cLatencyBins.end()) - cLatencyBins.begin();
        // set latency for the board to the minium
        LOG(INFO) << BOLDMAGENTA << "Min L1 latency on this board is " << cMinLatency << " 40 MHz clock cycles" << RESET;
        LOG(INFO) << BOLDMAGENTA << "Max L1 latency on this board is " << cMaxLatency << " 40 MHz clock cycles" << RESET;
        LOG(INFO) << BOLDBLUE << "Mode L1 latency on this board is " << cModeLatency << " 40 MHz clock cycles" << RESET;
        cBrdLatency.at(cBoard->getIndex())->getSummary<uint16_t>() = cModeLatency;
        // setSameDacBeBoard(cBoard, "TriggerLatency", cModeLatency);
        // fBeBoardInterface->ChipReSync(cBoard);
    }

    // container to hold trigger multiplicity per board
    DetectorDataContainer cBrdTriggerMult;
    ContainerFactory::copyAndInitBoard<uint32_t>(*fDetectorContainer, cBrdTriggerMult);
    for(auto cBoard: *fDetectorContainer)
    { cBrdTriggerMult.at(cBoard->getIndex())->getSummary<uint32_t>() = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity"); }

    // zero container
    // that hold latency per hybrid
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cLatencyContainer = fStubLatencyContainer.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(uint16_t cIndx = 0; cIndx < fLatencyRange; cIndx++)
                { cLatencyContainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx] = 0; }
            } // hybrid
        }     // optical group
    }

    // need a container to hold maximum hit count per chip
    // and one to hold best latency per chip [already defined in tool : fOptimalStubLatency]
    DetectorDataContainer cMaximumStubCount;
    ContainerFactory::copyAndInitBoard<uint32_t>(*fDetectorContainer, cMaximumStubCount);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cMaxCount                                                    = cMaximumStubCount.at(cBoard->getIndex());
        cMaxCount->getSummary<uint32_t>()                                  = 0;
        fOptimalStubLatency.at(cBoard->getIndex())->getSummary<uint32_t>() = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
    }

    // check if stub alignment has already been run
    bool cAlignmentRun = true;
    for(auto cBoard: *fDetectorContainer) { cAlignmentRun = cAlignmentRun && (cBoard->getStubOffset() != 0); }
    int cOffset = 0;
    // if no alignment has been run .. use scan range
    if(!cAlignmentRun)
    {
        auto     cSetting   = fSettingsMap.find("StubAlignmentScanStart");
        uint32_t cScanStart = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<uint32_t>(cSetting->second) : 100;
        cOffset             = cScanStart;
    }
    else
        cOffset = (int)(fLatencyRange / 2.);

    fOptimalLatency = 0;
    size_t cLatStep = 0;
    do
    {
        // set stub latency on all BE boards
        std::vector<uint8_t> cSet(0);
        for(auto cBoard: *fDetectorContainer)
        {
            auto  cBrdIndx     = cBoard->getIndex();
            auto& cTriggerMult = cBrdTriggerMult.at(cBrdIndx)->getSummary<uint32_t>();
            int   cStubLatency = cBrdLatency.at(cBrdIndx)->getSummary<uint16_t>() - (cOffset - cLatStep * (1 + cTriggerMult));
            if(cStubLatency < 0)
            {
                LOG(INFO) << BOLDYELLOW << +cTriggerMult << " " << +cStubLatency << " " << cStubLatency << RESET;
                cSet.push_back(0);
                continue;
            }
            fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
            LOG(INFO) << BOLDBLUE << "Setting stub latency on BeBoard#" << +cBoard->getId() << " to " << cStubLatency << " [stub offset is "
                      << (cBrdLatency.at(cBrdIndx)->getSummary<uint16_t>() - cStubLatency) << " ]" << RESET;
            cSet.push_back(1);
        }
        // read events
        ContinousReadout();

        for(auto cBoard: *fDetectorContainer)
        {
            if(cSet[cBoard->getIndex()] == 0) continue;
            fBeBoardInterface->setBoard(cBoard->getId());
            const std::vector<Event*>& cEvents              = this->GetEvents();
            auto&                      cTriggerMult         = cBrdTriggerMult.at(cBoard->getIndex())->getSummary<uint32_t>();
            float                      cNormalizationFactor = cEvents.size() / (1 + cTriggerMult);
            LOG(DEBUG) << BOLDMAGENTA << "Read-back " << +cEvents.size() << " from BeBoard#" << +cBoard->getId() << " - normalization factor for occupancy is " << +cNormalizationFactor << RESET;

            for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++)
            {
                Count(cEvents, cTriggerId);
                // fill containers for DQMUtils
                auto   cBrdIndx          = cBoard->getIndex();
                auto&  cLatencyContainer = fStubLatencyContainer.at(cBrdIndx);
                auto&  cMaxCount         = cMaximumStubCount.at(cBrdIndx)->getSummary<uint32_t>();
                size_t cStubCount        = 0;
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cStubContainer = fStubOccupancy.at(cBrdIndx)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex());
                        for(auto cChip: *cHybrid)
                        {
                            auto& cStubs = cStubContainer->at(cChip->getIndex())->getSummary<uint32_t>();
                            cLatencyContainer->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLatStep + cTriggerId] += cStubs;
                            cStubCount += cStubs;
                        }
                    }
                }
                if(cStubCount > cMaxCount)
                {
                    cMaxCount                                                = cStubCount;
                    fOptimalStubLatency.at(cBrdIndx)->getSummary<uint32_t>() = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
                    LOG(INFO) << BOLDYELLOW << "Brd#" << +cBoard->getId() << " Trigger# " << +cTriggerId << " found new optimal stub latency of "
                              << fOptimalStubLatency.at(cBrdIndx)->getSummary<uint32_t>() << RESET;
                }
            }
        }
        cLatStep++;
    } while(cLatStep < fLatencyRange);

    // configure optimal stub latency
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cLat = fOptimalStubLatency.at(cBoard->getIndex())->getSummary<uint32_t>();
        LOG(INFO) << BOLDYELLOW << "Setting common_stubdata_delay on BeBoard#" << +cBoard->getId() << " to " << cLat << RESET;
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cLat);
    }
}
void BeamTestCheck::PrepareForExternalTP(BeBoard* pBoard)
{
    // configure trigger
    uint8_t                                       cTriggerSource = 13;
    std::vector<std::string>                      cTPRegs{"test_pulse.delay_after_fast_reset", "test_pulse.delay_after_test_pulse", "test_pulse.delay_before_next_pulse", "test_pulse.en_fast_reset"};
    BeBoardRegMap                                 cRegMap = pBoard->getBeBoardRegMap();
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    for(auto cReg: cTPRegs)
    {
        std::string cRegName = "fc7_daq_cnfg.fast_command_block." + cReg;
        cRegVec.push_back({cRegName, cRegMap[cRegName]});
    }
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSource});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled", 0);

    bool cMaskChannelsFromOtherGroups = false;
    bool cInject                      = true;
    bool cWith2S                      = false;
    // inject in one of each CBCs
    auto boardIndex = pBoard->getIndex();
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                for(uint16_t groupNumber = 0; groupNumber < 1; ++groupNumber)
                {
                    if(groupNumber > getChannelGroupHandlerContainer()
                                         ->getObject(fDetectorContainer->getObject(boardIndex)->getId())
                                         ->getObject(cOpticalGroup->getId())
                                         ->getObject(cHybrid->getId())
                                         ->getObject(cChip->getId())
                                         ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                         ->getNumberOfGroups())
                        continue;
                    fReadoutChipInterface->maskChannelsAndSetInjectionSchema(cChip,
                                                                             getChannelGroupHandlerContainer()
                                                                                 ->getObject(fDetectorContainer->at(boardIndex)->getId())
                                                                                 ->getObject(cOpticalGroup->getId())
                                                                                 ->getObject(cHybrid->getId())
                                                                                 ->getObject(cChip->getId())
                                                                                 ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                                                                 ->getTestGroup(groupNumber),
                                                                             cMaskChannelsFromOtherGroups,
                                                                             cInject);
                }
            }
        }
    }

    // set TP amplitude and delay
    LOG(INFO) << BOLDYELLOW << "Enabling TP with : " << +fTPamplitude << " injected charge "
              << " delay of " << +fTPdelay << " ns " << RESET;

    // stop triggers
    fBeBoardInterface->Stop(pBoard);
    // send a ReSync
    fBeBoardInterface->ChipReSync(pBoard);

    UpdateFromRegMap(pBoard);
    if(cWith2S)
    {
        // setSameDacBeBoard(pBoard, "InjectedCharge", fTPamplitude);
        setSameDacBeBoard(pBoard, "TestPulseDelay", fTPdelay);
    }

    // inject PS
    if(!cWith2S) InjectPattern(pBoard, fInjections, -1);
}
void BeamTestCheck::PrepareForTP(BeBoard* pBoard)
{
    // configure trigger
    uint8_t                  cTriggerSource   = 6;
    uint32_t                 cDelayAfterReset = 300;
    uint32_t                 cDelayAfterTP    = 300;
    uint32_t                 cDelayTillNext   = 5000;
    std::vector<std::string> cFcmdRegs{"trigger_source", "test_pulse.delay_after_fast_reset", "test_pulse.delay_after_test_pulse", "test_pulse.delay_before_next_pulse", "triggers_to_accept"};
    std::vector<uint32_t>    cFcmdRegVals{cTriggerSource, cDelayAfterReset, cDelayAfterTP, cDelayTillNext, 0};
    std::vector<uint32_t>    cFcmdRegOrigVals(cFcmdRegs.size(), 0);
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.clear();
    for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    {
        std::string cRegName    = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
        cFcmdRegOrigVals[cIndx] = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        cRegVec.push_back({cRegName, cFcmdRegVals[cIndx]});
    }
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled", 0);

    size_t cNgroups                     = 0;
    bool   cMaskChannelsFromOtherGroups = false;
    bool   cInject                      = true;
    bool   cWith2S                      = false;
    // inject in one of each CBCs
    for(auto cGroup: *getChannelGroupHandlerContainer()->at(0)->at(0)->at(0)->at(0)->getSummary<std::shared_ptr<ChannelGroupHandler>>().get())
    {
        if(cNgroups > 0) continue;
        for(auto cOpticalGroup: *pBoard)
        {
            if(cOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) continue;
            cWith2S = true;
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid) { fReadoutChipInterface->maskChannelsAndSetInjectionSchema(cChip, cGroup, cMaskChannelsFromOtherGroups, cInject); }
            }
        }
        cNgroups++;
    }

    // set TP amplitude and delay
    LOG(INFO) << BOLDYELLOW << "Enabling TP with : " << +fTPamplitude << " injected charge "
              << " delay of " << +fTPdelay << " ns " << RESET;

    // stop triggers
    fBeBoardInterface->Stop(pBoard);
    // send a ReSync
    fBeBoardInterface->ChipReSync(pBoard);

    UpdateFromRegMap(pBoard);
    if(cWith2S)
    {
        // setSameDacBeBoard(pBoard, "InjectedCharge", fTPamplitude);
        setSameDacBeBoard(pBoard, "TestPulseDelay", fTPdelay);
    }

    // inject PS
    if(!cWith2S) InjectPattern(pBoard, fInjections, -1);
}
void BeamTestCheck::PrepareForTLU(BeBoard* pBoard)
{
    BeBoardRegMap cRegMap         = pBoard->getBeBoardRegMap();
    uint32_t      cTriggerMult    = cRegMap["fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity"];
    uint32_t      cStubDataDaelay = cRegMap["fc7_daq_cnfg.readout_block.global.common_stubdata_delay"];

    // configure trigger
    // make sure I am accepting all triggers
    uint8_t                                       cTriggerSource = 4;
    std::vector<std::string>                      cFcmdRegs{"trigger_source", "triggers_to_accept"};
    std::vector<uint32_t>                         cFcmdRegVals{cTriggerSource, 0}; // fNevents};
    std::vector<uint32_t>                         cFcmdRegOrigVals(cFcmdRegs.size(), 0);
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.clear();
    for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    {
        std::string cRegName    = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
        cFcmdRegOrigVals[cIndx] = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        cRegVec.push_back({cRegName, cFcmdRegVals[cIndx]});
    }
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cTriggerMult});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    // enable DIO5
    cRegVec.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x0});
    cRegVec.push_back({"fc7_daq_cnfg.dio5_block.dio5_en", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.dio5_block.ch2.threshold", 0});
    cRegVec.push_back({"fc7_daq_cnfg.dio5_block.ch2.term_enable", 1});
    cRegVec.push_back({"fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubDataDaelay});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled", 1);
    std::string cRegName   = "fc7_daq_cnfg.tlu_block.handshake_mode";
    size_t      cHandshake = cRegMap[cRegName];
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.handshake_mode", cHandshake);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.phase_select", 255);

    // stop triggers
    fBeBoardInterface->Stop(pBoard);
    // send a ReSync
    fBeBoardInterface->ChipReSync(pBoard);
    UpdateFromRegMap(pBoard);
}
void BeamTestCheck::PrepareForInternal(BeBoard* pBoard, uint8_t pLimitTriggers)
{
    BeBoardRegMap cRegMap = pBoard->getBeBoardRegMap();
    // triggers
    uint32_t cTriggerFreq = cRegMap["fc7_daq_cnfg.fast_command_block.user_trigger_frequency"];

    // configure trigger
    uint8_t                                       cTriggerSource     = 3;
    uint32_t                                      cNtriggersToAccept = (pLimitTriggers == 0) ? 0 : (uint32_t)fNevents;
    uint32_t                                      cHandshakeMode     = (pLimitTriggers == 0) ? 0 : 1;
    std::vector<std::string>                      cFcmdRegs{"trigger_source", "triggers_to_accept", "user_trigger_frequency"};
    std::vector<uint32_t>                         cFcmdRegVals{cTriggerSource, cNtriggersToAccept, cTriggerFreq};
    std::vector<uint32_t>                         cFcmdRegOrigVals(cFcmdRegs.size(), 0);
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.clear();
    for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    {
        std::string cRegName    = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
        cFcmdRegOrigVals[cIndx] = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        cRegVec.push_back({cRegName, cFcmdRegVals[cIndx]});
    }
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0});
    cRegVec.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", cHandshakeMode});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    // stop triggers
    fBeBoardInterface->Stop(pBoard);
    // send a ReSync
    fBeBoardInterface->ChipReSync(pBoard);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    UpdateFromRegMap(pBoard);

    LOG(INFO) << BOLDYELLOW << "Handshake Mode " << +cHandshakeMode << RESET;
}
void BeamTestCheck::ProcessEvents(BeBoard* pBoard) { PrintData(pBoard); }

void BeamTestCheck::PrepareForExternal(BeBoard* pBoard)
{
    // configure trigger
    // make sure I am accepting all triggers
    BeBoardRegMap cRegMap = pBoard->getBeBoardRegMap();
    // trigger config
    uint32_t cStubDataDaelay = cRegMap["fc7_daq_cnfg.readout_block.global.common_stubdata_delay"];
    uint32_t cTriggerMult    = cRegMap["fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity"];

    LOG(INFO) << BOLDYELLOW << "Common stub data delay set to " << +cStubDataDaelay << RESET;
    uint8_t                                       cTriggerSource = 5;
    std::vector<std::string>                      cFcmdRegs{"trigger_source", "triggers_to_accept"};
    std::vector<uint32_t>                         cFcmdRegVals{cTriggerSource, 0}; // fNevents};
    std::vector<uint32_t>                         cFcmdRegOrigVals(cFcmdRegs.size(), 0);
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.clear();
    for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    {
        std::string cRegName    = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
        cFcmdRegOrigVals[cIndx] = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        cRegVec.push_back({cRegName, cFcmdRegVals[cIndx]});
    }
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cTriggerMult});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    // enable DIO5
    cRegVec.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x0});
    cRegVec.push_back({"fc7_daq_cnfg.dio5_block.dio5_en", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.dio5_block.ch2.threshold", 0});
    cRegVec.push_back({"fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubDataDaelay});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled", 0);

    // stop triggers
    fBeBoardInterface->Stop(pBoard);
    // update registers
    UpdateFromRegMap(pBoard);
    // send a ReSync
    fBeBoardInterface->ChipReSync(pBoard);
}
void BeamTestCheck::Stop() {}

void BeamTestCheck::Pause() {}

void BeamTestCheck::Resume() {}

void BeamTestCheck::writeObjects()
{
#ifdef __USE_ROOT__
    this->SaveResults();
    fDQMHistogrammer.process();
#endif
}
