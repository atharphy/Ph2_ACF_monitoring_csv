#include "LatencyScan.h"

#ifdef __USE_ROOT__
#include "../HWDescription/Cbc.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/Occupancy.h"

LatencyScan::LatencyScan() : Tool() {}

LatencyScan::~LatencyScan() {}

void LatencyScan::Initialize(uint32_t pStartLatency, uint32_t pLatencyRange)
{
#ifdef __USE_ROOT__
    fDQMHistogramLatencyScan.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif

    for(auto cBoard: *fDetectorContainer)
    {
        uint32_t cBoardId = cBoard->getId();

        TH1F* cTriggerTDC = new TH1F(Form("h_BeBoard_triggerTDC_Be%d", cBoardId), Form("Trigger TDC BE%d; Trigger TDC; # of Hits", cBoardId), fTDCBins, -0.5, fTDCBins - 0.5);

        cTriggerTDC->SetFillColor(4);
        cTriggerTDC->SetFillStyle(3001);
        bookHistogram(cBoard, "triggerTDC", cTriggerTDC);

        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                uint32_t cFeId = cFe->getId();

                TCanvas* ctmpCanvas = new TCanvas(Form("c_online_canvas_fe%d", cFeId), Form("FE%d  Online Canvas", cFeId));
                // ctmpCanvas->Divide( 2, 2 );
                fCanvasMap[cFe] = ctmpCanvas;

                fNCbc = cFe->size();

                // 1D Hist forlatency scan
                TString  cName = Form("h_module_latency_Fe%d", cFeId);
                TObject* cObj  = gROOT->FindObject(cName);

                if(cObj) delete cObj;

                TH1F* cLatHist = nullptr;

                cLatHist = new TH1F(cName, Form("Latency FE%d; Latency; # of Hits", cFeId), (pLatencyRange), pStartLatency - 0.5, pStartLatency + (pLatencyRange)-0.5);

                cLatHist->GetXaxis()->SetTitle("Trigger Latency");
                cLatHist->SetFillColor(4);
                cLatHist->SetFillStyle(3001);
                bookHistogram(cFe, "module_latency", cLatHist);

                cName = Form("h_module_stub_latency_Fe%d", cFeId);
                cObj  = gROOT->FindObject(cName);

                if(cObj) delete cObj;

                TH1F* cStubHist = new TH1F(cName, Form("Stub Lateny FE%d; Stub Lateny; # of Stubs", cFeId), pLatencyRange, pStartLatency, pStartLatency + pLatencyRange);
                cStubHist->SetMarkerStyle(2);
                bookHistogram(cFe, "module_stub_latency", cStubHist);

                cName                = Form("h_module_latency_2D_Fe%d", cFeId);
                TH2D* cLatencyScan2D = new TH2D(cName,
                                                Form("Latency FE%d; Stub Latency; L1 Latency; # of Events w/ no Hits and no Stubs", cFeId),
                                                pLatencyRange,
                                                pStartLatency - 0.5,
                                                pStartLatency + (pLatencyRange)-0.5,
                                                pLatencyRange,
                                                pStartLatency - 0.5,
                                                pStartLatency + (pLatencyRange)-0.5);
                bookHistogram(cFe, "module_latency_2D", cLatencyScan2D);
            }
        }
    }

    parseSettings();
    LOG(INFO) << "Histograms and Settings initialised.";
}

void LatencyScan::MeasureTriggerTDC()
{
    LOG(INFO) << "Measuring Trigger TDC ... ";

    std::map<uint16_t, std::vector<uint16_t>> BeBoardTriggerTDCMap;
    // Take Data for all Modules
    for(auto pBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(pBoard);
        // I need this to normalize the TDC values I get from the Strasbourg FW
        BeBoardTriggerTDCMap[pBoard->getId()] = std::vector<uint16_t>(fTDCBins, 0);

        ReadNEvents(theBoard, fNevents);

        const std::vector<Event*>& events = GetEvents(theBoard);

        for(auto& cEvent: events)
        {
            uint8_t cTDCVal = cEvent->GetTDC();

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
                ++BeBoardTriggerTDCMap[pBoard->getId()][cTDCVal];
            }
        }
    }

    for(auto pBoard: *fDetectorContainer)
    {
        TH1F* cTmpHist = dynamic_cast<TH1F*>(getHist(pBoard, "triggerTDC"));
        for(size_t tdcValue = 0; tdcValue < fTDCBins; ++tdcValue) { cTmpHist->SetBinContent(tdcValue + 1, BeBoardTriggerTDCMap[pBoard->getId()][tdcValue]); }
    }

    return;
}

std::map<ModuleContainer*, uint8_t> LatencyScan::ScanLatency(uint8_t pStartLatency, uint8_t pLatencyRange)
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

            const std::vector<Event*>& events = GetEvents(theBoard);
            countHitsLat(theBoard, events, "module_latency", cLat, pStartLatency);
            // done counting hits for all FE's, now update the Histograms
            updateHists("module_latency", false);
        }

        // done counting hits for all FE's, now update the Histograms
        updateHists("module_latency", false);
        cIterationCount++;
    }

    // analyze the Histograms
    std::map<ModuleContainer*, uint8_t> cLatencyMap;

    updateHists("module_latency", true);

    return cLatencyMap;
}

void LatencyScan::StubLatencyScan(uint8_t pStartLatency, uint8_t pLatencyRange)
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
                    auto& cCic        = static_cast<OuterTrackerModule*>(cHybrid)->fCic;
                    bool  cMaskOthers = (cCic != NULL) ? true : false;
                    for(auto cChip: *cHybrid)
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
                    } // roc
                }     // hybrid
            }         // module
        }
    }

    // scan stub latency
    for(uint8_t cLat = pStartLatency; cLat < pStartLatency + pLatencyRange; cLat++)
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
            // Take Data for all Modules
            // here set the stub latency
            for(auto cReg: getStubLatencyName(cBeBoard->getBoardType())) fBeBoardInterface->WriteBoardReg(cBeBoard, cReg, cLat);

            this->ReadNEvents(cBeBoard, this->findValueInSettings("Nevents"));
            const std::vector<Event*>& cEvents = this->GetEvents(cBeBoard);
            // Loop over Events from this Acquisition
            for(auto& cEvent: cEvents)
            {
                auto cEventCount = cEvent->GetEventCount();
                LOG(DEBUG) << BOLDBLUE << "\tEvent " << +cEventCount << RESET;
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& cMatchesThisModule = cMatchesThisBoard->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cCic               = static_cast<OuterTrackerModule*>(cHybrid)->fCic;
                        auto& cMatchesThisHybrid = cMatchesThisModule->at(cHybrid->getIndex());
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
                            else
                            {
                                // TO_BE IMPLEMENTED
                            }
                        } // chip
                    }     // hybrid
                }         // modules
            }             // events
            LOG(INFO) << BOLDBLUE << "\t..." << +cNStubs << " matched stubs in " << +cEvents.size() << " readout events." << RESET;
#ifdef __USE_ROOT__
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    TH1F* cTmpHist    = dynamic_cast<TH1F*>(getHist(cHybrid, "module_stub_latency"));
                    int   cBin        = cTmpHist->FindBin(cLat);
                    float cBinContent = cTmpHist->GetBinContent(cBin);
                    cBinContent += cNStubs;
                    cTmpHist->SetBinContent(cBin, cBinContent);
                } // hybrid
            }     // module
#endif
        } // board
    }     // latency
}
std::map<ModuleContainer*, uint8_t> LatencyScan::ScanStubLatency(uint8_t pStartLatency, uint8_t pLatencyRange)
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
                    auto& cCic        = static_cast<OuterTrackerModule*>(cHybrid)->fCic;
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
            }         // module
        }

        for(uint8_t cLat = pStartLatency; cLat < pStartLatency + pLatencyRange; cLat++)
        {
            int cNStubs = 0;
            // Take Data for all Modules
            // here set the stub latency
            for(auto cReg: getStubLatencyName(cBeBoard->getBoardType())) fBeBoardInterface->WriteBoardReg(cBeBoard, cReg, cLat);

            this->ReadNEvents(cBeBoard, this->findValueInSettings("Nevents"));
            const std::vector<Event*>& cEvents = this->GetEvents(cBeBoard);
            // Loop over Events from this Acquisition
            for(auto& cEvent: cEvents)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup) { cNStubs += countStubs(cHybrid, cEvent, "module_stub_latency", cLat); }
                }
            }
            LOG(INFO) << "Stub Latency " << +cLat << " Stubs " << cNStubs << " Events " << cEvents.size();
        }
        // done counting hits for all FE's, now update the Histograms
        updateHists("module_stub_latency", false);
    }

    // // analyze the Histograms
    std::map<ModuleContainer*, uint8_t> cStubLatencyMap;

    LOG(INFO) << "Identified the Latency with the maximum number of Stubs at: ";

    for(auto cFe: fModuleHistMap)
    {
        TH1F*   cTmpHist           = dynamic_cast<TH1F*>(getHist(cFe.first, "module_stub_latency"));
        uint8_t cStubLatency       = static_cast<uint8_t>(cTmpHist->GetMaximumBin() - 1);
        cStubLatencyMap[cFe.first] = cStubLatency;

        // BeBoardRegWriter cLatWriter ( fBeBoardInterface, "", 0 );

        // if ( cFe.first->getFeId() == 0 ) cLatWriter.setRegister ( "cbc_stubdata_latency_adjust_fe1", cStubLatency );
        // else if ( cFe.first->getFeId() == 1 ) cLatWriter.setRegister ( "cbc_stubdata_latency_adjust_fe2",
        // cStubLatency );

        // this->accept ( cLatWriter );

        LOG(INFO) << "Stub Latency FE " << +cFe.first->getId() << ": " << +cStubLatency << " clock cycles!";
    }

    return cStubLatencyMap;
}

void LatencyScan::ScanLatency2D(uint8_t pStartLatency, uint8_t pLatencyRange)
{
    LatencyVisitor cVisitor(fReadoutChipInterface, 0);
    int            cNSteps = 0;
    for(uint16_t cLatency = pStartLatency; cLatency < pStartLatency + pLatencyRange; cLatency++)
    {
        //  Set a Latency Value on all FEs
        cVisitor.setLatency(cLatency);
        this->accept(cVisitor);

        // maximum stub latency can only be L1 latency ...
        for(uint8_t cStubLatency = 0; cStubLatency < cLatency; cStubLatency++)
        {
            // Take Data for all Modules
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

                    const std::vector<Event*>& events = GetEvents(theBoard);
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
                            TH2D* cTmpHist2D = dynamic_cast<TH2D*>(getHist(cFe, "module_latency_2D"));
                            cTmpHist2D->Fill(cStubLatency, cLatency, cNEvents_wBoth);
                            cTmpHist2D->GetZaxis()->SetRangeUser(1, cNevents);
                            updateHists("module_latency_2D", false);
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

                TH2D* cTmpHist2D = dynamic_cast<TH2D*>(getHist(cFe, "module_latency_2D"));

                for(int cBinX = 1; cBinX < cTmpHist2D->GetXaxis()->GetNbins(); cBinX++)
                {
                    for(int cBinY = 1; cBinY < cTmpHist2D->GetYaxis()->GetNbins(); cBinY++)
                    {
                        int cBinContent = cTmpHist2D->GetBinContent(cBinX, cBinY);
                        if(cBinContent >= cMaxNEvents_wBoth)
                        {
                            cOptimalLatencies.first  = (uint8_t)cTmpHist2D->GetXaxis()->GetBinCenter(cBinX);
                            cOptimalLatencies.second = (uint16_t)cTmpHist2D->GetYaxis()->GetBinCenter(cBinY);
                            cMaxNEvents_wBoth        = cBinContent;
                        }
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

//////////////////////////////////////          PRIVATE METHODS             //////////////////////////////////////

// int LatencyScan::countHits ( BeBoard* pBoard,  const std::vector<Event*> pEventVec, std::map<uint8_t,
// std::map<uint8_t,uint32_t> > numberOfHits, std::map<uint8_t,uint32_t> > numberOfStubs)
// {

// }

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

int LatencyScan::countStubs(Module* pFe, const Event* pEvent, std::string pHistName, uint8_t pParameter)
{
    // loop over Modules & Cbcs and count hits separately
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

void LatencyScan::updateHists(std::string pHistName, bool pFinal)
{
    for(auto& cCanvas: fCanvasMap)
    {
        // maybe need to declare temporary pointers outside the if condition?
        if(pHistName == "module_latency")
        {
            cCanvas.second->cd();
            TH1F* cTmpHist = dynamic_cast<TH1F*>(getHist(static_cast<Ph2_HwDescription::Module*>(cCanvas.first), pHistName));
            cTmpHist->DrawCopy();
            cCanvas.second->Update();
        }
        else if(pHistName == "module_stub_latency")
        {
            cCanvas.second->cd();
            TH1F* cTmpHist = dynamic_cast<TH1F*>(getHist(static_cast<Ph2_HwDescription::Module*>(cCanvas.first), pHistName));
            cTmpHist->DrawCopy();
            cCanvas.second->Update();
        }
        else if(pHistName == "module_latency_2D")
        {
            cCanvas.second->cd();
            TH2D* cTmpHist = dynamic_cast<TH2D*>(getHist(static_cast<Ph2_HwDescription::Module*>(cCanvas.first), pHistName));
            cTmpHist->DrawCopy("colz");
            cTmpHist->SetStats(0);
            cCanvas.second->Update();
        }
    }

    this->HttpServerProcess();
}

void LatencyScan::parseSettings()
{
    // now read the settings from the map
    auto cSetting = fSettingsMap.find("Nevents");

    if(cSetting != std::end(fSettingsMap))
        fNevents = cSetting->second;
    else
        fNevents = 2000;

    cSetting = fSettingsMap.find("HoleMode");

    if(cSetting != std::end(fSettingsMap))
        fHoleMode = cSetting->second;
    else
        fHoleMode = 1;

    cSetting = fSettingsMap.find("TriggerSource");
    if(cSetting != std::end(fSettingsMap)) trigSource = cSetting->second;
    LOG(INFO) << int(trigSource);

    // cSetting = fSettingsMap.find ( "TestPulsePotentiometer" );
    // fTestPulseAmplitude = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 0x7F;

    LOG(INFO) << "Parsed the following settings:";
    LOG(INFO) << "	Nevents = " << fNevents;
    LOG(INFO) << "	HoleMode = " << int(fHoleMode);
    // LOG (INFO) << "   TestPulseAmplitude = " << int ( fTestPulseAmplitude ) ;
}
void LatencyScan::writeObjects()
{
    this->SaveResults();
    // just use auto iterators to write everything to disk
    // this is the old method before Tool class was cool
    fResultFile->cd();

    // Save canvasses too
    // fNoiseCanvas->Write ( fNoiseCanvas->GetName(), TObject::kOverwrite );
    // fPedestalCanvas->Write ( fPedestalCanvas->GetName(), TObject::kOverwrite );
    // fFeSummaryCanvas->Write ( fFeSummaryCanvas->GetName(), TObject::kOverwrite );
    fResultFile->Flush();
}

// State machine control functions

void LatencyScan::ConfigureCalibration()
{
    CreateResultDirectory("Results/Run_LatencyScan");
    InitResultFile("LatencyScanResults");
}

void LatencyScan::Start(int currentRun) {}

void LatencyScan::Stop() {}

void LatencyScan::Pause() {}

void LatencyScan::Resume() {}

#endif
