#include "LatencyScan.h"

#include "../HWDescription/Cbc.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/MPAChannelGroupHandler.h"
#include "../Utils/Occupancy.h"
#include "../Utils/SSAChannelGroupHandler.h"

LatencyScan::LatencyScan() : Tool() {}

LatencyScan::~LatencyScan() {}

void LatencyScan::Initialize()
{
    ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0));
    bool         cWithCBC          = (cFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
    bool         cWithSSA          = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
    bool         cWithMPA          = (cFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);

    if(cWithCBC) fChannelGroupHandler = new CBCChannelGroupHandler();
    if(cWithSSA) fChannelGroupHandler = new SSAChannelGroupHandler();
    if(cWithMPA) fChannelGroupHandler = new MPAChannelGroupHandler();

    initializeRecycleBin();
    fChannelGroupHandler->setChannelGroupParameters(16, 2);

    fStartLatency = findValueInSettings("StartLatency", 1);
    fLatencyRange = findValueInSettings("LatencyRange", 1);
    fHoleMode     = findValueInSettings("HoleMode", 1);
    fNevents      = findValueInSettings("Nevents", 10);
    std::cout << "Going to read " << fNevents << " events" << std::endl;

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
void LatencyScan::cleanContainerMap()
{
    for(auto container: fSCurveOccupancyMap) fRecycleBin.free(container.second);
    fSCurveOccupancyMap.clear();
}

void LatencyScan::ScanLatency()
{
    // bool cUseReadNevents = false;
    LOG(INFO) << "Scanning Latency ... ";
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

    uint16_t cLat = fStartLatency; 
    //for(uint16_t cLat = fStartLatency; cLat < fStartLatency + fLatencyRange; cLat++)
    do
    {
        setSameDac("TriggerLatency", cLat);
        uint16_t cOffset=0; 
        for(auto cBoard: *fDetectorContainer)
        {
            auto cBrdIndx = cBoard->getIndex();
            size_t cTriggerMult = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
            LOG (INFO) << BOLDRED << "Reading events in scan latency.." << RESET;
            this->ReadNEvents(cBoard, fNevents);
            const std::vector<Event*>& cEvents = this->GetEvents();
            // loop over triggers in the burst 
            for( size_t cTriggerId=0; cTriggerId < cTriggerMult+1 ; cTriggerId++)
            {
                // LOG (INFO) << BOLDMAGENTA << "Latency of " << cLat+cTriggerId << RESET;
                if( (cLat+cTriggerId) >= (fStartLatency + fLatencyRange) ) continue;
                // start at the beginning + trigger id in burst 
                auto cEventIter = cEvents.begin() + cTriggerId ;
                // calculate occupancy for each 
                DetectorDataContainer* theOccupancyContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
                fDetectorDataContainer                       = theOccupancyContainer;
                fSCurveOccupancyMap[cLat+cTriggerId]         = theOccupancyContainer;
                auto& cOccBrd = theOccupancyContainer->at(cBrdIndx);
                do
                {   
                    if( cEventIter >= cEvents.end() ) break; 
                    (*cEventIter)->fillDataContainer(cOccBrd, fChannelGroupHandler->allChannelGroup()); 
                    cEventIter += (1+cTriggerMult);
                }while(cEventIter < cEvents.end());
                cOccBrd->normalizeAndAverageContainers(fDetectorContainer->at(cBrdIndx), fChannelGroupHandler->allChannelGroup(), fNevents);
                float cOccGlbl = cOccBrd->getSummary<Occupancy, Occupancy>().fOccupancy;
                LOG (INFO) << BOLDMAGENTA << "Latency of " << (cLat+cTriggerId) << " - trigger#" << +cTriggerId << " in a burst of " << (1+cTriggerMult) 
                    << " - on average have found " << cOccGlbl * cTotalNChnls << " channels with a hit [per board per event]." << RESET;
                #ifdef __USE_ROOT__
                    fDQMHistogramLatencyScan.fillLatencyPlots(cLat+cTriggerId, *theOccupancyContainer);
                #endif
            }
            if( cOffset < (1+cTriggerMult) ) cOffset = (1+cTriggerMult); 
        }//board
        cLat += cOffset; 
        // DetectorDataContainer* theOccupancyContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
        // fDetectorDataContainer                       = theOccupancyContainer;
        // fSCurveOccupancyMap[cLat]                    = theOccupancyContainer;
        // set trigger latency 
        //this->setDacAndMeasureData("TriggerLatency", cLat, fNevents);
//         float cOccGlbl = theOccupancyContainer->getSummary<Occupancy, Occupancy>().fOccupancy;
//         LOG(INFO) << BOLDMAGENTA << "Latency of " << cLat << " .. on average have found " << cOccGlbl * cTotalNChnls << " hits per event" << RESET;
// #ifdef __USE_ROOT__
//         fDQMHistogramLatencyScan.fillLatencyPlots(cLat, *theOccupancyContainer);
// #endif
    }while( cLat < fStartLatency + fLatencyRange );
    /*#ifdef __USE_ROOT__
        fDQMHistogramLatencyScan.fillLatencyPlots(*theLatencyContainer);
    #else
        auto theLatencyStream = prepareHybridContainerStreamer<EmptyContainer, EmptyContainer, GenericDataArray<VECSIZE, uint16_t>>();
        for(auto board: theLatencyContainer)
        {
            if(fStreamerEnabled) theLatencyStream.streamAndSendBoard(board, fNetworkStreamer);
        }
    #endif*/
}

void LatencyScan::StubLatencyScan()
{
    DetectorDataContainer theStubContainer;
    ContainerFactory::copyAndInitHybrid<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, theStubContainer);

    auto cStubOffset = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->getStubOffset();
    // means that at some point the stub latency was scanned and the correct value was identified
    uint16_t cLowerLimit = fStartLatency;
    uint16_t cUpperLimit = fStartLatency + fLatencyRange;
    if(cStubOffset != 0xFFFF)
    {
        LOG(INFO) << BOLDMAGENTA << "Since stub latency offset was already found to be " << +cStubOffset << " clock cycles modifying range of scan .. to start  at "
                  << " a value close to the hit latency " << RESET;
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                if(cOpticalGroup->getIndex() > 0) break;

                for(auto cHybrid: *cOpticalGroup)
                {
                    if(cHybrid->getIndex() > 0) break;

                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getIndex() > 0) break;

                        auto     cTriggerLatency = fReadoutChipInterface->ReadChipReg(cChip, "TriggerLatency");
                        uint16_t cRange          = 8;
                        cLowerLimit              = cTriggerLatency - cStubOffset - cRange / 2;
                        cUpperLimit              = cTriggerLatency - cStubOffset + cRange / 2;
                        LOG(INFO) << BOLDMAGENTA << "Using latency value programmed in Chp#" << +cChip->getId() << " : modifying range of scan .. to start looking for stubs at " << cLowerLimit
                                  << " clock cycles - trigger latency is set to " << cTriggerLatency << " clock cycles." << RESET;
                    } // chips
                }     // hybrids
            }         // OGs
        }             // brds
    }

    // check for TP
    for(auto cBoard: *fDetectorContainer)
    {
        uint16_t cTriggerSource = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
        if(cTriggerSource == 6)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getIndex() > 0)
                        {
                            LOG(INFO) << BOLDMAGENTA << "Since I am use the TP .. want to make sure I see stubs from only one chip "
                                      << " by disabling injection on Chip#" << +cChip->getId() << RESET;
                            fReadoutChipInterface->enableInjection(cChip, false);

                        }
                    }
                }
            } //
        }     //
    }         //

    int cDebugOut = 5;
    for(uint16_t cLat = fStartLatency; cLat < fStartLatency + fLatencyRange; cLat++)
    {
        // container to hold scan result
        // DetectorDataContainer* cMatchedEvents = new DetectorDataContainer();
        // ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *cMatchedEvents);
        for(auto cBoard: *fDetectorContainer)
        {
            // zero stub container
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    theStubContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLat - fStartLatency] = 0;
                } // hybrid
            }     //

            if(!(cLat >= cLowerLimit && cLat < cUpperLimit)) continue;

            // auto&    cMatchesThisBoard = cMatchedEvents->at(cBoard->getIndex());
            // Take Data for all Hybrids
            // here set the stub latency

            for(auto cReg: getStubLatencyName(cBoard->getBoardType())) fBeBoardInterface->WriteBoardReg(cBoard, cReg, cLat);
            this->ReadNEvents(cBoard, fNevents);
            const std::vector<Event*>& cEvents = this->GetEvents();
            // Loop over Events from this Acquisition
            LOG(INFO) << BOLDMAGENTA << "BeBoard#" << +cBoard->getIndex() << " ..searching for a match between stub and hit data for a stub latency of  " << +cLat << RESET;
            for(auto& cEvent: cEvents)
            {
                auto cEventCount = cEvent->GetEventCount();
                LOG(DEBUG) << BOLDBLUE << "\tEvent " << +cEventCount << RESET;
                for(auto cOpticalGroup: *cBoard)
                {
                    // auto& cMatchesThisOpticalGroup = cMatchesThisBoard->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        // auto& cMatchesThisHybrid = cMatchesThisOpticalGroup->at(cHybrid->getIndex());
                        auto& cCic = static_cast<OuterTrackerHybrid*>(fDetectorContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex()))->fCic;
                        if(cCic != NULL)
                        {
                            auto cBx = cEvent->BxId(cHybrid->getId());
                            if(cEventCount % cDebugOut == 0) LOG(INFO) << BOLDBLUE << "\t\t..Hybrid " << +cHybrid->getId() << " BxID " << +cBx << RESET;
                        }

                        size_t cNStubs = 0;
                        for(auto cChip: *cHybrid)
                        {
                            // auto& cMatchesThisROC = cMatchesThisHybrid->at(cChip->getIndex());
                            if(cChip->getFrontEndType() == FrontEndType::CBC3)
                            {
                                // first check for hits
                                auto cHits = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                                if(cHits.size() == 0) continue;

                                // for(auto cHit : cHits )
                                // {
                                //     if(cEventCount % cDebugOut == 0) LOG(INFO) << BOLDYELLOW << "\t\t\t...Hybrid#" << +cHybrid->getId() 
                                //             << " chip#" << +cChip->getId() 
                                //             << " hit found in channel " 
                                //             << +cHit
                                //             << RESET;
                                // }
                                auto                 cReadoutChipInterface = static_cast<CbcInterface*>(fReadoutChipInterface);
                                std::vector<uint8_t> cBendLUT              = cReadoutChipInterface->readLUT(cChip);
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
                                    // if(cEventCount % cDebugOut == 0) LOG(INFO) << BOLDYELLOW << "\t\t\t...Hybrid#" << +cHybrid->getId() 
                                    //         << " chip#" << +cChip->getId() 
                                    //         << " stub with seed " << +cStub.getPosition() << " and bendCode " << +cStub.getBend() << " which is bend " << +cBend << " half-strips"
                                    //         << RESET;
                                    // check that the hits from these stubs
                                    // match the hits in the event
                                    for(auto cHit: cExpectedHits)
                                    {
                                        auto cFound = std::find(cHits.begin(), cHits.end(), cHit);
                                        // cMatchesThisROC->getChannel<Occupancy>(cHit).fOccupancy += (cFound != cHits.end()) ? 1 : 0;
                                        cMatchedHits += (cFound != cHits.end()) ? 1 : 0;
                                    }
                                    // only count stubs where the match is perfect
                                    cNStubs += (cMatchedHits == (int)cExpectedHits.size()) ? 1 : 0;
                                }

                                //
                                // if(cStubs.size() > 0)
                                //     LOG(INFO) << BOLDGREEN << "\t\t\tCBC#" << +cChip->getId() << "...Found " << +cStubs.size() << " stubs in the readout."
                                //               << " and " << +cHits.size() << " hits of which .. " << +cMatchedHits << " match the stubs!" << RESET;
                                // else
                                //     LOG(INFO) << BOLDRED << "\t\t\tCBC#" << +cChip->getId() << "...Found " << +cStubs.size() << " stubs in the readout."
                                //               << " and " << +cHits.size() << " hits of which .. " << +cMatchedHits << " match the stubs!" << RESET;
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
                        theStubContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLat - fStartLatency] +=
                            cNStubs;
                        if(cEventCount % cDebugOut == 0)
                            LOG(INFO) << BOLDBLUE << "Event#" << +cEventCount << "\t\t.. found "
                                      << theStubContainer.at(cBoard->getIndex())
                                             ->at(cOpticalGroup->getIndex())
                                             ->at(cHybrid->getIndex())
                                             ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLat - fStartLatency]
                                      << " stubs on hybrid" << +cHybrid->getId() 
                                      << " that match hit information in the readout.." << RESET;
                    } // hybrids
                }     // optical group
            }         // events
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    LOG(INFO)
                        << BOLDMAGENTA << "Hybrid#" << +cHybrid->getId() << " found "
                        << theStubContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLat - fStartLatency]
                        << " matched stubs in " << +cEvents.size() << " readout events." << RESET;
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
                do {
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
            this->ReadNEvents(cBeBoard, this->findValueInSettings("Nevents"));
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
