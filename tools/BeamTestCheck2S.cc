#include "BeamTestCheck2S.h"

#include "../HWDescription/Cbc.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

BeamTestCheck2S::BeamTestCheck2S() : OTTool() {}

BeamTestCheck2S::~BeamTestCheck2S() {}

// Initialization function
void BeamTestCheck2S::Initialise()
{
    Prepare();
    SetName("BeamTestCheck2S");

    initializeRecycleBin();

    // create groups for injection
    // set injection group
    fChannelGroupHandler = new CBCChannelGroupHandler();
    fChannelGroupHandler->setChannelGroupParameters(16, 2); // number of cluster per group, number of rows per cluster

    // set TP amplitude and delay
    fTPamplitude = 255 - findValueInSettings("Check2STPamplitude", 255);
    fTPdelay     = findValueInSettings("Check2STPdelay", 0);

    // threshold
    fThreshold = findValueInSettings("Check2Sthreshold", 0);

    // initialize latency scan range based on TP settings
    fStartLatency = findValueInSettings("StartLatency", 0);
    fLatencyRange = findValueInSettings("LatencyRange", 0);

    // initialize containers
    // latency per hybrid
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fLatencyContainer);
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fLatencyContainerS0);
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fLatencyContainerS1);

    // cluster occupancy per chip
    ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, float>>(*fDetectorContainer, fClusterOccupancy);
    ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, float>>(*fDetectorContainer, fClusterOccupancyS0);
    ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, float>>(*fDetectorContainer, fClusterOccupancyS1);

    // TDC per board 
    ContainerFactory::copyAndInitBoard<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fTDCContainer);
    
    // pedestals
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, fPedestalContainer);

#ifdef __USE_ROOT__
    fDQMHistogrammer.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

// State machine control functions
void BeamTestCheck2S::Running()
{
    Initialise();
    fSuccess = true;
    Reset();
}

void BeamTestCheck2S::DisableAllFEs()
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
void BeamTestCheck2S::CheckWithTP()
{
    for(auto cBoard: *fDetectorContainer)
    {
        // prepare injection
        PrepareForTP(cBoard);
        // scan the latency - find best hit latency
        ScanLatency(cBoard,1);
        // scan the threshold, record number of hits; cluster occupancy
        // ScanThreshold(cBoard);
    }
#ifdef __USE_ROOT__
    fDQMHistogrammer.fillLatencyPlots(fLatencyContainerS0, fLatencyContainerS1);
    fDQMHistogrammer.fillClusterOccupancyPlots(fClusterOccupancy);
#endif
}
//
void BeamTestCheck2S::CheckWithInternal(uint8_t pContinousReadout)
{
    for(auto cBoard: *fDetectorContainer)
    {
        // prepare injection
        PrepareForInternal(cBoard);
        // retreive events from FC7
        if(pContinousReadout == 1)
            ContinousReadout(cBoard);
        else
            ReadNEvents(cBoard, fNevents);

        // process events
        ProcessEvents(cBoard);
    }
}
void BeamTestCheck2S::CheckWithExternal(uint8_t pContinousReadout)
{
    LOG(INFO) << BOLDBLUE << "Checking with external triggers - will readout " << fNevents << RESET;

    for(auto cBoard: *fDetectorContainer)
    {
        // prepare injection
        PrepareForExternal(cBoard);
        LOG (INFO) << "External check with " << fNevents << " -- continuous readout set to " << +pContinousReadout << RESET;
    }

    if( pContinousReadout ) ContinousReadout();

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
    //     fDQMHistogrammer.fillTriggerTDCPlots(fTDCContainer);    
    // #endif
}
void BeamTestCheck2S::UpdateClusterContainers(BeBoard* pBoard, const std::vector<Event*> pEvents, size_t pIndx)
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
void BeamTestCheck2S::ScanThreshold(BeBoard* pBoard)
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
void BeamTestCheck2S::ScanLatency(BeBoard* pBoard, uint8_t pContinousReadout)
{
    // bool cUseReadNevents = false;
    LOG(INFO) << "Scanning Latency ... ContinousReadout set to " << +pContinousReadout << RESET;;
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
    for(uint16_t cIndx = 0; cIndx < TDCBINS; cIndx++)
    {
       cTDCContainer->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cIndx] = 0;
    }
    
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
    auto cRefSensor = findValueInSettings("Check2SRefSensor", 0);
    auto cRefSide   = findValueInSettings("Check2SRefSide", 0);
    auto cRefChip   = findValueInSettings("Check2SRefChip", 0);

    fUseReadNEvents   = findValueInSettings("Check2SUseReadNEvents", 0);
    fWait_ms          = findValueInSettings("Check2Swait", 0);
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
                            LOG(DEBUG) << BOLDBLUE << "Event#" << (*cEventIter)->GetEventCount() << "ROC#" << +cChip->getId() % 8 << " " << +cHits.size() << " hits." << RESET;
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
            // cOccBrd->normalizeAndAverageContainers(fDetectorContainer->at(cBrdIndx), fChannelGroupHandler->allChannelGroup(), fNReadbackEvents);
            // float cOccGlbl = cOccBrd->getSummary<Occupancy, Occupancy>().fOccupancy;
            cTotalHits = cTotalHitsS0 + cTotalHitsS1;

            if(cTotalHits > 0)
            {
                if(cRefHits >= cMaxHits)
                {
                    fOptimalLatency = cLat;
                    LOG(INFO) << BOLDYELLOW << "[!!!! new max !!!!]Latency of " << (cLat + cTriggerId) << " - trigger#" << +cTriggerId << " in a burst of " << (1 + cTriggerMult)
                              << "... on average have found " << std::setprecision(2) << cTotalHits / cNormalizationFactor << " hit(s) per event."
                              << "In S0 " << cTotalHitsS0 << " hit(s); in S1 = " << cTotalHitsS1  << " hit(s)."
                              << "... optimal latency will be set to " << cLat << ". Normalization done with " << fNReadbackEvents << " events." << RESET;
                    cMaxHits = cRefHits;
                }
                else
                    LOG(INFO) << BOLDBLUE << "Latency of " << (cLat + cTriggerId) << " - trigger#" << +cTriggerId << " in a burst of " << (1 + cTriggerMult) << "... on average have found "
                              << std::setprecision(2) << cTotalHits / cNormalizationFactor << " hit(s) per event."
                              << "In S0 " << cTotalHitsS0  << " hit(s); in S1 = " << cTotalHitsS1  << " hit(s)."
                              << "Normalization done with " << fNReadbackEvents << " events." << RESET;
            }
            else
                LOG(INFO) << BOLDBLUE << "Latency of " << (cLat + cTriggerId) << " - trigger#" << +cTriggerId << " in a burst of " << (1 + cTriggerMult) << "... on average have found "
                          << std::setprecision(2) << cTotalHits / cNormalizationFactor << " hit(s) per event."
                          << "In S0 " << cTotalHitsS0  << " hit(s); in S1 = " << cTotalHitsS1  << " hit(s) "
                          << " normalization done with " << fNReadbackEvents << " events." << RESET;
            #ifdef __USE_ROOT__
                fDQMHistogrammer.fillLatencyPlots(cLat + cTriggerId, *theOccupancyContainer, cHitContainer);
            #endif
        }
        if(cOffset < (1 + cTriggerMult)) cOffset = (1 + cTriggerMult);
        cLat += cOffset;
    } while(cLat < fStartLatency + fLatencyRange);

    LOG(INFO) << BOLDYELLOW << "Optimal latency found to be : " << fOptimalLatency << " 40 MHz clock cycles [L1 data]" << RESET;
}

void BeamTestCheck2S::PrepareForTP(BeBoard* pBoard)
{
    // configure trigger
    uint8_t                                       cTriggerSource   = 6;
    uint32_t                                      cDelayAfterReset = 100;
    uint32_t                                      cDelayTillNext   = 5000;
    std::vector<std::string>                      cFcmdRegs{"trigger_source", "test_pulse.delay_after_fast_reset", "test_pulse.delay_before_next_pulse", "triggers_to_accept"};
    std::vector<uint32_t>                         cFcmdRegVals{cTriggerSource, cDelayAfterReset, cDelayTillNext, fNevents };
    std::vector<uint32_t>                         cFcmdRegOrigVals(cFcmdRegs.size(), 0);
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
    // inject in one of each CBCs
    for(auto cGroup: *fChannelGroupHandler)
    {
        if(cNgroups > 0) continue;
        for(auto cOpticalGroup: *pBoard)
        {
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
    // set thresholds from xml
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                uint32_t cThreshold = 0;
                if(cChip->getFrontEndType() == FrontEndType::CBC3) cThreshold = (cChip->getReg("VCth1") + (cChip->getReg("VCth2") << 8));
                if(cChip->getFrontEndType() == FrontEndType::SSA) cThreshold = cChip->getReg("Bias_THDAC");
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    for(uint8_t cDAC = 0; cDAC < 1; cDAC++)
                    {
                        std::stringstream cRegName;
                        cRegName << "ThDAC" << +cDAC;
                        cThreshold = cChip->getReg(cRegName.str());
                    }
                }
                fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
                LOG(INFO) << BOLDMAGENTA << "Setting threshold on ROC#" << +cChip->getId() << " to " << cThreshold << RESET;
            }
        }
    }

    setSameDacBeBoard(pBoard, "InjectedCharge", fTPamplitude);
    setSameDacBeBoard(pBoard, "TestPulseDelay", fTPdelay);
}
void BeamTestCheck2S::PrepareForInternal(BeBoard* pBoard, uint8_t pLimitTriggers)
{
    // configure trigger
    uint8_t                                       cTriggerSource     = 3;
    uint32_t                                      cNtriggersToAccept = (pLimitTriggers == 0) ? 0 : (uint32_t)fNevents;
    std::vector<std::string>                      cFcmdRegs{"trigger_source", "triggers_to_accept"};
    std::vector<uint32_t>                         cFcmdRegVals{cTriggerSource, cNtriggersToAccept};
    std::vector<uint32_t>                         cFcmdRegOrigVals(cFcmdRegs.size(), 0);
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

    // stop triggers
    fBeBoardInterface->Stop(pBoard);
    // send a ReSync
    fBeBoardInterface->ChipReSync(pBoard);
    // set thresholds
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                uint32_t cThreshold = 0;
                if(cChip->getFrontEndType() == FrontEndType::CBC3) cThreshold = (cChip->getReg("VCth1") + (cChip->getReg("VCth2") << 8));
                if(cChip->getFrontEndType() == FrontEndType::SSA) cThreshold = cChip->getReg("Bias_THDAC");
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    for(uint8_t cDAC = 0; cDAC < 1; cDAC++)
                    {
                        std::stringstream cRegName;
                        cRegName << "ThDAC" << +cDAC;
                        cThreshold = cChip->getReg(cRegName.str());
                    }
                }
                fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
                LOG(INFO) << BOLDMAGENTA << "Setting threshold on ROC#" << +cChip->getId() << " to " << cThreshold << RESET;
            }
        }
    }
}
void BeamTestCheck2S::ProcessEvents(BeBoard* pBoard)
{
    PrintData(pBoard);
}

void BeamTestCheck2S::PrepareForExternal(BeBoard* pBoard)
{
    // configure trigger
    // make sure I am accepting all triggers 
    uint8_t                                       cTriggerSource = 5;
    std::vector<std::string>                      cFcmdRegs{"trigger_source","triggers_to_accept"};
    std::vector<uint32_t>                         cFcmdRegVals{cTriggerSource,0};//fNevents};
    std::vector<uint32_t>                         cFcmdRegOrigVals(cFcmdRegs.size(), 0);
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.clear();
    for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    {
        std::string cRegName    = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
        cFcmdRegOrigVals[cIndx] = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        cRegVec.push_back({cRegName, cFcmdRegVals[cIndx]});
    }
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    // enable DIO5
    cRegVec.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x0});
    cRegVec.push_back({"fc7_daq_cnfg.dio5_block.dio5_en", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.dio5_block.ch2.threshold", 50});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled", 0);

    // stop triggers
    fBeBoardInterface->Stop(pBoard);
    // send a ReSync
    fBeBoardInterface->ChipReSync(pBoard);
    // set thresholds
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                uint32_t cThreshold = 0;
                if(cChip->getFrontEndType() == FrontEndType::CBC3) cThreshold = (cChip->getReg("VCth1") + (cChip->getReg("VCth2") << 8));
                if(cChip->getFrontEndType() == FrontEndType::SSA) cThreshold = cChip->getReg("Bias_THDAC");
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    for(uint8_t cDAC = 0; cDAC < 1; cDAC++)
                    {
                        std::stringstream cRegName;
                        cRegName << "ThDAC" << +cDAC;
                        cThreshold = cChip->getReg(cRegName.str());
                    }
                }
                LOG(INFO) << BOLDMAGENTA << "Setting threshold on ROC#" << +cChip->getId() << " to " << cThreshold << RESET;
                fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
            }
        }
    }
}
void BeamTestCheck2S::Stop() {}

void BeamTestCheck2S::Pause() {}

void BeamTestCheck2S::Resume() {}

void BeamTestCheck2S::writeObjects()
{
#ifdef __USE_ROOT__
    this->SaveResults();
    fDQMHistogrammer.process();
#endif
}