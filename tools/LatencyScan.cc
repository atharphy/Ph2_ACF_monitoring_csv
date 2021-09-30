#include "LatencyScan.h"

#include "../HWDescription/Cbc.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/Occupancy.h"

LatencyScan::LatencyScan() : Tool() {}

LatencyScan::~LatencyScan() {}

void LatencyScan::Initialize()
{
    fStartLatency = findValueInSettings<double>("StartLatency", 1);
    fLatencyRange = findValueInSettings<double>("LatencyRange", 1);
    fHoleMode     = findValueInSettings<double>("HoleMode", 1);
    fNevents      = findValueInSettings<double>("Nevents", 10);

#ifdef __USE_ROOT__
    fDQMHistogramLatencyScan.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif

    LOG(INFO) << "Histograms and Settings initialised.";
}

void LatencyScan::MeasureTriggerTDC()
{
    LOG(INFO) << "Measuring Trigger TDC ... ";

    DetectorDataContainer theTriggerTDCContainer;
    ContainerFactory::copyAndInitHybrid<GenericDataArray<TDCBINS, uint16_t>>(*fDetectorContainer, theTriggerTDCContainer);

    for(auto board: theTriggerTDCContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(fDetectorContainer->at(board->getIndex()));

        ReadNEvents(theBoard, fNevents);
        const std::vector<Event*>& events = GetEvents();
        std::vector<uint32_t>      values(TDCBINS - 1, 0);

        for(auto& cEvent: events)
        {
            uint8_t cTDCVal = cEvent->GetTDC();
            LOG(INFO) << "TDC Val is " << cTDCVal;

            if(theBoard->getBoardType() == BoardType::D19C)
            {
                // Fix from Mykyta, ONLY the value of the cTDCShiftValue variable have to be changed, NEVER change the
                // formula
                uint8_t cTDCShiftValue = 1;
                // don't touch next two lines (!!!)
                if(cTDCVal < cTDCShiftValue)
                    cTDCVal += (fTDCBins - cTDCShiftValue);
                else
                    cTDCVal -= cTDCShiftValue;
            }
            if(cTDCVal >= fTDCBins)
                LOG(INFO) << "ERROR, TDC value not within expected range - normalized value is " << +cTDCVal << " - original Value was " << +cEvent->GetTDC() << "; not considering this Event!"
                          << std::endl;
            else
            {
                // Board level value, just fill the first optical group & hybrid with the value for streaming simplicity
                values[cTDCVal]++;
            }
        }
        LOG(INFO) << "hybrid? " << board->at(0)->at(0)->getIndex();
        for(uint32_t v = 0; v < fTDCBins; v++)
        {
            LOG(INFO) << "filling " << v << " with " << values[v];
            board->at(0)->at(0)->getSummary<GenericDataArray<TDCBINS, uint16_t>>()[v] = values[v];
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramLatencyScan.fillTriggerTDCPlots(theTriggerTDCContainer);
#else
    auto theTriggerTDCStream = prepareHybridContainerStreamer<EmptyContainer, EmptyContainer, GenericDataArray<TDCBINS, uint16_t>>("TriggerTDC");
    for(auto board: theTriggerTDCContainer)
    {
        if(fStreamerEnabled) theTriggerTDCStream.streamAndSendBoard(board, fNetworkStreamer);
    }
#endif
}

void LatencyScan::ScanLatency()
{
    LOG(INFO) << "Scanning Latency ... ";
    uint32_t cIterationCount = 0;

    // Fabio - clean BEGIN
    setFWTestPulse();
    enableTestPulse(true);
    setSameDac("TestPulsePotNodeSel", 150);
    // Fabio - clean END

    LatencyVisitor cVisitor(fReadoutChipInterface, 0);

    DetectorDataContainer theLatencyContainer;
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, theLatencyContainer);

    for(auto board: theLatencyContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(fDetectorContainer->at(board->getIndex()));
        for(uint16_t cLat = fStartLatency; cLat < fStartLatency + fLatencyRange; cLat++)
        {
            //  Set a Latency Value on all FEs
            cVisitor.setLatency(cLat);
            this->accept(cVisitor);
            ReadNEvents(theBoard, fNevents);

            for(auto opticalGroup: *board)
            {
                const std::vector<Event*>& events = GetEvents();
                for(auto hybrid: *opticalGroup)
                {
                    uint32_t cHitSum = 0;
                    for(auto& cEvent: events)
                    {
                        // first, reset the hit counter - I need separate counters for each event
                        int cHitCounter = 0;
                        for(auto chip: *hybrid)
                        {
                            ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex()));
                            if(theChip->getFrontEndType() == FrontEndType::MPA)
                                cHitCounter += static_cast<D19cMPAEvent*>(cEvent)->GetNPixelClusters(hybrid->getId(), chip->getId());
                            else if(theChip->getFrontEndType() == FrontEndType::SSA)
                                cHitCounter += static_cast<D19cMPAEvent*>(cEvent)->GetNStripClusters(hybrid->getId(), static_cast<SSA*>(theChip)->getPartid());
                            else
                                cHitCounter += cEvent->GetNHits(hybrid->getId(), chip->getId());
                        }
                        cHitSum += cHitCounter; // TODO: It would be nice to fill per event so you could have the errors correct, maybe do with occupancy?

                    } // end event loop

                    LOG(INFO) << "FE: " << +hybrid->getId() << "; Latency " << +cLat << " clock cycles; Hits " << cHitSum << "; Events " << fNevents;
                    hybrid->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLat - fStartLatency] = cHitSum;
                } // end hybrid

            } // end optical group

        } // end latency loop

        cIterationCount++;
    } // end board loop

#ifdef __USE_ROOT__
    fDQMHistogramLatencyScan.fillLatencyPlots(theLatencyContainer);
#else
    auto theLatencyStream = prepareHybridContainerStreamer<EmptyContainer, EmptyContainer, GenericDataArray<VECSIZE, uint16_t>>();
    for(auto board: theLatencyContainer)
    {
        if(fStreamerEnabled) theLatencyStream.streamAndSendBoard(board, fNetworkStreamer);
    }
#endif
}

void LatencyScan::StubLatencyScan()
{
    // check if TP trigger is being used
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);
        // check trigger source
        uint16_t cTriggerSource = fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
        if(cTriggerSource == 6)
        {
            LOG(INFO) << BOLDBLUE << "Trigger source is ... using TP" << RESET;

            // if TP on .. and CIC
            // make sure stubs are only in one CBC
            // becaause otherwise ..
            uint8_t cTPgroup = 0;
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cCic        = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    bool  cMaskOthers = (cCic != NULL) ? true : false;
                    for(auto cChip: *cHybrid)
                    {
                        if((cChip->getFrontEndType() == FrontEndType::CBC3))
                        {
                            auto cReadoutChipInterface = static_cast<CbcInterface*>(fReadoutChipInterface);
                            if(cMaskOthers && cChip->getId() == 0)
                            {
                                if(cChip->getFrontEndType() == FrontEndType::CBC3)
                                {
                                    uint8_t cFirstSeed = static_cast<uint8_t>(2 * (1 + std::floor((cTPgroup * 2 + 16 * 0) / 2.))); // in half strips
                                    cReadoutChipInterface->injectStubs(cChip, {cFirstSeed}, {0}, false);
                                }
                            }
                            else if(cMaskOthers && cChip->getFrontEndType() == FrontEndType::CBC3)
                            {
                                fReadoutChipInterface->WriteChipReg(cChip, "TestPulse", (int)0);
                            }
                        }

                    } // roc
                }     // hybrid
            }         // hybrid
        }
    }

    DetectorDataContainer theStubContainer;
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, theStubContainer);

    for(uint8_t cLat = fStartLatency; cLat < fStartLatency + fLatencyRange; cLat++)
    {
        // container to hold scan result
        DetectorDataContainer* cMatchedEvents = new DetectorDataContainer();
        ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *cMatchedEvents);

        LOG(INFO) << BOLDBLUE << "Stub Latency " << +cLat << RESET;
        for(auto cBoard: *fDetectorContainer)
        {
            int      cNStubs           = 0;
            BeBoard* cBeBoard          = static_cast<BeBoard*>(cBoard);
            auto&    cMatchesThisBoard = cMatchedEvents->at(cBoard->getIndex());
            // Take Data for all Hybrids
            // here set the stub latency

            for(auto cReg: getStubLatencyName(cBeBoard->getBoardType())) fBeBoardInterface->WriteBoardReg(cBeBoard, cReg, cLat);
            this->ReadNEvents(cBeBoard, this->findValueInSettings<double>("Nevents"));
            const std::vector<Event*>& cEvents = this->GetEvents();
            // Loop over Events from this Acquisition
            for(auto& cEvent: cEvents)
            {
                auto cEventCount = cEvent->GetEventCount();
                LOG(DEBUG) << BOLDBLUE << "\tEvent " << +cEventCount << RESET;
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& cMatchesThisOpticalGroup = cMatchesThisBoard->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cCic               = static_cast<OuterTrackerHybrid*>(fDetectorContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex()))->fCic;
                        auto& cMatchesThisHybrid = cMatchesThisOpticalGroup->at(cHybrid->getIndex());
                        if(cCic != NULL)
                        {
                            auto cBx = cEvent->BxId(cHybrid->getId());
                            LOG(INFO) << BOLDBLUE << "\t\t..Hybrid " << +cHybrid->getId() << " BxID " << +cBx << RESET;
                        }

                        for(auto cChip: *cHybrid)
                        {
                            auto& cMatchesThisROC = cMatchesThisHybrid->at(cChip->getIndex());
                            if(cChip->getFrontEndType() == FrontEndType::CBC3)
                            {
                                auto                 cReadoutChipInterface = static_cast<CbcInterface*>(fReadoutChipInterface);
                                std::vector<uint8_t> cBendLUT              = cReadoutChipInterface->readLUT(cChip);
                                auto                 cHits                 = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                                auto                 cStubs                = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                                int                  cMatchedHits          = 0;
                                for(auto cStub: cStubs)
                                {
                                    // each bend code is stored in this vector - bend encoding start at -7 strips,
                                    // increments by 0.5 strips
                                    // uint8_t cBendCode = cBendLUT[ (cStub.getBend()/2. - (-7.0))/0.5 ];
                                    // find bend code
                                    auto     cIter         = std::find(cBendLUT.begin(), cBendLUT.end(), cStub.getBend());
                                    uint16_t cIndex        = std::distance(cBendLUT.begin(), cIter);
                                    int      cBend         = (0.5 * cIndex + (-7.0)) * 2.0;
                                    auto     cExpectedHits = cReadoutChipInterface->stubInjectionPattern(cChip, cStub.getPosition(), cBend);
                                    LOG(INFO) << BOLDBLUE << "\t\t\t...Stub with seed " << +cStub.getPosition() << " and bendCode " << +cStub.getBend() << " which is bend " << +cBend << " half-strips"
                                              << RESET;
                                    // check that the hits from these stubs
                                    // match the hits in the event
                                    for(auto cHit: cExpectedHits)
                                    {
                                        auto cFound = std::find(cHits.begin(), cHits.end(), cHit);
                                        cMatchesThisROC->getChannel<Occupancy>(cHit).fOccupancy += (cFound != cHits.end()) ? 1 : 0;
                                        cMatchedHits += (cFound != cHits.end()) ? 1 : 0;
                                    }
                                    // only count stubs where the match is perfect
                                    cNStubs += (cMatchedHits == (int)cExpectedHits.size()) ? 1 : 0;
                                }

                                //
                                if(cStubs.size() > 0)
                                    LOG(INFO) << BOLDGREEN << "\t\t\tCBC#" << +cChip->getId() << "...Found " << +cStubs.size() << " stubs in the readout."
                                              << " and " << +cHits.size() << " hits of which .. " << +cMatchedHits << " match the stubs!" << RESET;
                                else
                                    LOG(INFO) << BOLDRED << "\t\t\tCBC#" << +cChip->getId() << "...Found " << +cStubs.size() << " stubs in the readout."
                                              << " and " << +cHits.size() << " hits of which .. " << +cMatchedHits << " match the stubs!" << RESET;
                            }
                            else if(cChip->getFrontEndType() == FrontEndType::SSA)
                            {
                                auto cStubs = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                                cNStubs     = cStubs.size();
                            }
                            else if(cChip->getFrontEndType() == FrontEndType::MPA)
                            {
                                auto cHits  = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                                auto cStubs = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                                cNStubs += cStubs.size();
                            }
                        } // chip
                    }     // optical group
                }         // hybrids
            }             // events
            LOG(INFO) << BOLDBLUE << "\t..." << +cNStubs << " matched stubs in " << +cEvents.size() << " readout events." << RESET;

            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    theStubContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLat - fStartLatency] = cNStubs;

                } // hybrid
            }     // hybrid

        } // board
    }     // latency

#ifdef __USE_ROOT__
    fDQMHistogramLatencyScan.fillStubLatencyPlots(theStubContainer);
#else
    auto theStubStream = prepareHybridContainerStreamer<EmptyContainer, EmptyContainer, GenericDataArray<VECSIZE, uint16_t>>();
    for(auto board: theStubContainer)
    {
        if(fStreamerEnabled) theStubStream.streamAndSendBoard(board, fNetworkStreamer);
    }
#endif
}

void LatencyScan::ScanLatency2D()
{
    DetectorDataContainer theLatencyContainer;
    // 2D array -- hit latency vs stub latency
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, GenericDataArray<VECSIZE, uint16_t>>>(*fDetectorContainer, theLatencyContainer);

    LatencyVisitor cVisitor(fReadoutChipInterface, 0);
    int            cNSteps = 0;
    for(uint16_t cLatency = fStartLatency; cLatency < fStartLatency + fLatencyRange; cLatency++)
    {
        //  Set a Latency Value on all FEs
        cVisitor.setLatency(cLatency);
        this->accept(cVisitor);

        // maximum stub latency can only be L1 latency ...
        for(uint8_t cStubLatency = 0; cStubLatency < cLatency; cStubLatency++)
        {
            // Take Data for all Hybrids
            for(auto pBoard: *fDetectorContainer)
            {
                BeBoard* theBoard = static_cast<BeBoard*>(pBoard);
                // set a stub latency value on all FEs
                for(auto cReg: getStubLatencyName(theBoard->getBoardType())) fBeBoardInterface->WriteBoardReg(theBoard, cReg, cStubLatency);

                // I need this to normalize the TDC values I get from the Strasbourg FW
                uint32_t cNevents       = 0;
                uint32_t cNEvents_wHit  = 0;
                uint32_t cNEvents_wStub = 0;
                uint32_t cNEvents_wBoth = 0;
                fBeBoardInterface->Start(theBoard);
                do
                {
                    uint32_t cNeventsReadBack = ReadData(theBoard);
                    if(cNeventsReadBack == 0)
                    {
                        LOG(INFO) << BOLDRED << "..... Read back " << +cNeventsReadBack << " events!! Why?!" << RESET;
                        continue;
                    }

                    const std::vector<Event*>& events = GetEvents();
                    cNevents += events.size();
                    for(auto cOpticalGroup: *pBoard)
                    {
                        for(auto cFe: *cOpticalGroup)
                        {
                            for(auto cEvent: events)
                            {
                                bool cHitFound  = false;
                                bool cStubFound = false;
                                // now loop the channels for this particular event and increment a counter
                                for(auto cCbc: *cFe)
                                {
                                    int               cHitCounter  = cEvent->GetNHits(cFe->getId(), cCbc->getId());
                                    std::vector<Stub> cStubs       = cEvent->StubVector(cFe->getId(), cCbc->getId());
                                    int               cStubCounter = cStubs.size();

                                    if(cHitCounter == 0) {}

                                    if(cHitCounter > 0) cHitFound = true;

                                    if(cStubCounter > 0) cStubFound = true;
                                }
                                cNEvents_wHit += cHitFound ? 1 : 0;
                                cNEvents_wStub += cStubFound ? 1 : 0;
                                cNEvents_wBoth += (cHitFound && cStubFound) ? 1 : 0;
                            }

                            theLatencyContainer.at(pBoard->getIndex())
                                ->at(cOpticalGroup->getIndex())
                                ->at(cFe->getIndex())
                                ->getSummary<GenericDataArray<VECSIZE, GenericDataArray<VECSIZE, uint16_t>>>()[cStubLatency][(cLatency - fStartLatency)] += cNEvents_wBoth;
                        }
                    }

                } while(cNevents < fNevents);
                fBeBoardInterface->Stop(theBoard);

                if(cNSteps % 10 == 0)
                {
                    LOG(INFO) << BOLDBLUE << "For an L1 latency of " << +cLatency << " and a stub latency of " << +cStubLatency << " - found : " << RESET;
                    LOG(INFO) << BOLDBLUE << "\t\t " << cNEvents_wHit << "/" << cNevents << " events with a hit. " << RESET;
                    LOG(INFO) << BOLDBLUE << "\t\t " << cNEvents_wStub << "/" << cNevents << " events with a stub. " << RESET;
                    LOG(INFO) << BOLDBLUE << "\t\t " << cNEvents_wBoth << "/" << cNevents << " events with both a hit and a stub. " << RESET;
                }
            }
            cNSteps++;
        }
    }

    // now display a message to the user to let them know what the optimal latencies are for each FE
    for(auto pBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                std::pair<uint8_t, uint16_t> cOptimalLatencies;
                cOptimalLatencies.first  = 0;
                cOptimalLatencies.second = 0;
                int cMaxNEvents_wBoth    = 0;

                // run same loop as before
                for(uint16_t cLatency = fStartLatency; cLatency < fStartLatency + fLatencyRange; cLatency++)
                {
                    // maximum stub latency can only be L1 latency ...
                    for(uint8_t cStubLatency = 0; cStubLatency < cLatency; cStubLatency++)
                    {
                        uint16_t val = theLatencyContainer.at(pBoard->getIndex())
                                           ->at(cOpticalGroup->getIndex())
                                           ->at(cFe->getIndex())
                                           ->getSummary<GenericDataArray<VECSIZE, GenericDataArray<VECSIZE, uint16_t>>>()[cStubLatency][(cLatency - fStartLatency)];

                        if(val >= cMaxNEvents_wBoth)
                        {
                            cOptimalLatencies.first  = cStubLatency;
                            cOptimalLatencies.second = cLatency;
                            cMaxNEvents_wBoth        = val;
                        }
                    }

                    LOG(INFO) << BOLDRED << "************************************************************************************" << RESET;
                    LOG(INFO) << BOLDRED << "For FE" << +cFe->getId() << " found optimal latencies to be : " << RESET;
                    LOG(INFO) << BOLDRED << "........ Stub Latency of " << +cOptimalLatencies.first << " and a Trigger Latency of " << +cOptimalLatencies.second << RESET;
                    LOG(INFO) << BOLDRED << "************************************************************************************" << RESET;
                }
            }
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramLatencyScan.fill2DLatencyPlots(theLatencyContainer);
#else
    auto theLatencyStream = prepareHybridContainerStreamer<EmptyContainer, EmptyContainer, GenericDataArray<VECSIZE, GenericDataArray<VECSIZE, uint16_t>>>("2D");
    for(auto board: theLatencyContainer)
    {
        if(fStreamerEnabled) theLatencyStream.streamAndSendBoard(board, fNetworkStreamer);
    }
#endif
}

//////////////////////////////////////          PRIVATE METHODS             //////////////////////////////////////

void LatencyScan::writeObjects()
{
#ifdef __USE_ROOT__
    fDQMHistogramLatencyScan.process();
#endif
}

// State machine control functions

void LatencyScan::ConfigureCalibration() { CreateResultDirectory("Results/Run_Latency"); }

void LatencyScan::Running()
{
    LOG(INFO) << "Starting Latency Scan";

    Initialize();
    ScanLatency();
    // StubLatencyScan();
    // MeasureTriggerTDC();
    LOG(INFO) << "Done with Latency Scan";
}

void LatencyScan::Stop()
{
    LOG(INFO) << "Stopping Latency Scan.";
    writeObjects();
    dumpConfigFiles();
    closeFileHandler();
    LOG(INFO) << "Latency Scan stopped.";
}

void LatencyScan::Pause() {}

void LatencyScan::Resume() {}

// these functions are used by MPA latency that relies on their output histograms
// they should be replaced to avoid duplication
#ifdef __USE_ROOT__
std::map<HybridContainer*, uint8_t> LatencyScan::ScanStubLatency(uint8_t pStartLatency, uint8_t pLatencyRange)
{
    // Now the actual scan
    LOG(INFO) << BOLDBLUE << "Scanning Stub Latency ... ";
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);
        // check trigger source
        uint16_t cTriggerSource = fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
        if(cTriggerSource == 6)
        {
            LOG(INFO) << BOLDBLUE << "Trigger source is ... using TP" << RESET;

            // if TP on .. and CIC
            // make sure stubs are only in one CBC
            // becaause otherwise ..
            uint8_t cTPgroup = 0;
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cCic        = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    bool  cMaskOthers = (cCic != NULL) ? true : false;
                    for(auto cChip: *cHybrid)
                    {
                        auto cReadoutChipInterface = static_cast<CbcInterface*>(fReadoutChipInterface);
                        if(cMaskOthers && cChip->getId() == 0)
                        {
                            uint8_t cFirstSeed = static_cast<uint8_t>(2 * (1 + std::floor((cTPgroup * 2 + 16 * 0) / 2.))); // in half strips
                            cReadoutChipInterface->injectStubs(cChip, {cFirstSeed}, {0}, false);
                        }
                        else if(cMaskOthers)
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "TestPulse", (int)0);
                        }
                    } // roc
                }     // hybrid
            }         // hybrid
        }

        for(uint8_t cLat = pStartLatency; cLat < pStartLatency + pLatencyRange; cLat++)
        {
            int cNStubs = 0;
            // Take Data for all Hybrids
            // here set the stub latency
            for(auto cReg: getStubLatencyName(cBeBoard->getBoardType())) fBeBoardInterface->WriteBoardReg(cBeBoard, cReg, cLat);
            this->ReadNEvents(cBeBoard, this->findValueInSettings<double>("Nevents"));
            const std::vector<Event*>& cEvents = this->GetEvents();
            // Loop over Events from this Acquisition
            for(auto& cEvent: cEvents)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup) { cNStubs += countStubs(cHybrid, cEvent, "hybrid_stub_latency", cLat); }
                }
            }
            LOG(INFO) << "Stub Latency " << +cLat << " Stubs " << cNStubs << " Events " << cEvents.size();
        }
        // done counting hits for all FE's, now update the Histograms
        updateHists("hybrid_stub_latency", false);
    }

    // // analyze the Histograms
    std::map<HybridContainer*, uint8_t> cStubLatencyMap;

    LOG(INFO) << "Identified the Latency with the maximum number of Stubs at: ";

    for(auto cFe: fHybridHistMap)
    {
        TH1F*   cTmpHist           = dynamic_cast<TH1F*>(getHist(cFe.first, "hybrid_stub_latency"));
        uint8_t cStubLatency       = static_cast<uint8_t>(cTmpHist->GetMaximumBin() - 1);
        cStubLatencyMap[cFe.first] = cStubLatency;

        // BeBoardRegWriter cLatWriter ( fBeBoardInterface, "", 0 );

        // if ( cFe.first->getHybridId() == 0 ) cLatWriter.setRegister ( "cbc_stubdata_latency_adjust_fe1", cStubLatency );
        // else if ( cFe.first->getHybridId() == 1 ) cLatWriter.setRegister ( "cbc_stubdata_latency_adjust_fe2",
        // cStubLatency );

        // this->accept ( cLatWriter );

        LOG(INFO) << "Stub Latency FE " << +cFe.first->getId() << ": " << +cStubLatency << " clock cycles!";
    }

    return cStubLatencyMap;
}

int LatencyScan::countStubs(Hybrid* pFe, const Event* pEvent, std::string pHistName, uint8_t pParameter)
{
    // loop over Hybrids & Cbcs and count hits separately
    int cStubCounter = 0;

    //  get histogram to fill
    TH1F* cTmpHist = dynamic_cast<TH1F*>(getHist(pFe, pHistName));

    for(auto cCbc: *pFe)
    {
        if(pEvent->StubBit(pFe->getId(), cCbc->getId())) cStubCounter += pEvent->StubVector(pFe->getId(), cCbc->getId()).size();
    }
    int   cBin        = cTmpHist->FindBin(pParameter);
    float cBinContent = cTmpHist->GetBinContent(cBin);
    cBinContent += cStubCounter;
    cTmpHist->SetBinContent(cBin, cBinContent);

    // if (cStubCounter != 0) std::cout << "Found " << cStubCounter << " Stubs in this event" << std::endl;

    // GA, old, potentially buggy code)
    // cTmpHist->Fill ( pParameter );

    return cStubCounter;
}

std::map<HybridContainer*, uint8_t> LatencyScan::ScanLatency_root(uint16_t pStartLatency, uint16_t pLatencyRange)
{
    LOG(INFO) << "Scanning Latency ... ";
    uint32_t cIterationCount = 0;

    // //Fabio - clean BEGIN
    // setFWTestPulse();
    // setSystemTestPulse ( 200, 0, true, false );
    // //Fabio - clean END

    LatencyVisitor cVisitor(fReadoutChipInterface, 0);
    for(auto pBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(pBoard);
        for(uint16_t cLat = pStartLatency; cLat < pStartLatency + pLatencyRange; cLat++)
        {
            //  Set a Latency Value on all FEs
            cVisitor.setLatency(cLat);
            this->accept(cVisitor);
            ReadNEvents(theBoard, fNevents);
            const std::vector<Event*>& events = GetEvents();
            countHitsLat(theBoard, events, "hybrid_latency", cLat, pStartLatency);
            // done counting hits for all FE's, now update the Histograms
            updateHists("hybrid_latency", false);
        }

        // done counting hits for all FE's, now update the Histograms
        updateHists("hybrid_latency", false);
        cIterationCount++;
    }

    // analyze the Histograms
    std::map<HybridContainer*, uint8_t> cLatencyMap;

    for(auto cFe: fHybridHistMap)
    {
        TH1F*   cTmpHist       = dynamic_cast<TH1F*>(getHist(cFe.first, "hybrid_latency"));
        uint8_t cHitLatency    = static_cast<uint8_t>(cTmpHist->GetXaxis()->GetBinUpEdge(cTmpHist->GetMaximumBin()));
        cLatencyMap[cFe.first] = cHitLatency;

        LOG(INFO) << "Hit Latency FE " << +cFe.first->getId() << ": " << +cHitLatency << " clock cycles!";
    }

    return cLatencyMap;
}

int LatencyScan::countHitsLat(BeBoard* pBoard, const std::vector<Event*> pEventVec, std::string pHistName, uint16_t pParameter, uint32_t pStartLatency)
{
    uint32_t cTotalHits = 0;
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cFe: *cOpticalGroup)
        {
            uint32_t cHitSum = 0;
            //  get histogram to fill
            TH1F* cTmpHist = dynamic_cast<TH1F*>(getHist(cFe, pHistName));
            for(auto& cEvent: pEventVec)
            {
                // first, reset the hit counter - I need separate counters for each event
                int cHitCounter = 0;

                for(auto cCbc: *cFe)
                {
                    // now loop the channels for this particular event and increment a counter
                    if(cCbc->getFrontEndType() == FrontEndType::MPA)
                        cHitCounter += static_cast<D19cMPAEvent*>(cEvent)->GetNPixelClusters(cFe->getId(), cCbc->getId());
                    else if(cCbc->getFrontEndType() == FrontEndType::SSA)
                        cHitCounter += static_cast<D19cMPAEvent*>(cEvent)->GetNStripClusters(cFe->getId(), static_cast<SSA*>(cCbc)->getPartid());
                    else
                        cHitCounter += cEvent->GetNHits(cFe->getId(), cCbc->getId());
                }

                // now I have the number of hits in this particular event for all CBCs and the TDC value

                cTmpHist->Fill(pParameter, cHitCounter);

                // if (cHitCounter != 0 ) std::cout << "Found " << cHitCounter << " Hits in this event!" << std::endl;

                // GA: old, potentially buggy code
                // cTmpHist->Fill (cBin, cHitCounter);

                cHitSum += cHitCounter;
            }

            LOG(INFO) << "FE: " << +cFe->getId() << "; Latency " << +pParameter << " clock cycles; Hits " << cHitSum << "; Events " << fNevents;
            cTotalHits += cHitSum;
        }
    }

    return cTotalHits;
}

void LatencyScan::updateHists(std::string pHistName, bool pFinal)
{
    for(auto& cCanvas: fCanvasMap)
    {
        // maybe need to declare temporary pointers outside the if condition?
        if(pHistName == "hybrid_latency")
        {
            cCanvas.second->cd();
            TH1F* cTmpHist = dynamic_cast<TH1F*>(getHist(static_cast<Ph2_HwDescription::Hybrid*>(cCanvas.first), pHistName));
            cTmpHist->DrawCopy();
            cCanvas.second->Update();
        }
        else if(pHistName == "hybrid_stub_latency")
        {
            cCanvas.second->cd();
            TH1F* cTmpHist = dynamic_cast<TH1F*>(getHist(static_cast<Ph2_HwDescription::Hybrid*>(cCanvas.first), pHistName));
            cTmpHist->DrawCopy();
            cCanvas.second->Update();
        }
        else if(pHistName == "hybrid_latency_2D")
        {
            cCanvas.second->cd();
            TH2D* cTmpHist = dynamic_cast<TH2D*>(getHist(static_cast<Ph2_HwDescription::Hybrid*>(cCanvas.first), pHistName));
            cTmpHist->DrawCopy("colz");
            cTmpHist->SetStats(0);
            cCanvas.second->Update();
        }
    }

    this->HttpServerProcess();
}

#endif