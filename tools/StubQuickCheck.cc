#include "StubQuickCheck.h"
#ifdef __USE_ROOT__
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/Occupancy.h"
// temporary fix until we address event which is compatible for IT  + OT
#include "../Utils/CommonVisitors.h"
#include "../Utils/D19cCicEvent.h"
#include "../Utils/Visitor.h"
#include "Channel.h"

#include <map>

#include "TCanvas.h"
#include "TEfficiency.h"
#include "TGraphErrors.h"
#include "TH2.h"
#include "TProfile.h"
#include "TProfile2D.h"
#include "TString.h"
#include "TText.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

StubQuickCheck::StubQuickCheck() : PedeNoise()
{
    fPedestalContainer.reset();
    fNoiseContainer.reset();
    fOccupancyContainer.reset();
}

StubQuickCheck::~StubQuickCheck()
{
    // delete fOffsetCanvas;
    // delete fOccupancyCanvas;
}

void StubQuickCheck::Initialise()
{
    // this is needed if you're going to use groups anywhere
    fChannelGroupHandler = new CBCChannelGroupHandler(); // This will be erased in tool.resetPointers()
    fChannelGroupHandler->setChannelGroupParameters(16, 2);
#ifdef __USE_ROOT__
//    fDQMHistogram.book(fResultFile,*fDetectorContainer);
#endif

    // default LUT for CBCs
    std::vector<double> cBinsBend_Default(0);
    for(int cBin = 0; cBin < 7; cBin++) cBinsBend_Default.push_back(-7.0 + cBin);

    cBinsBend_Default.push_back(0);
    for(int cBin = 7; cBin < 15; cBin++) { cBinsBend_Default.push_back(-7.0 + cBin + 0.5); }

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                TString  cName = Form("h_StubBend");
                TObject* cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                TH1D* cBend = new TH1D(cName, Form("Stub Bend - CIC%d; Bend", (int)cFe->getId()), cBinsBend_Default.size() - 1, cBinsBend_Default.data());
                bookHistogram(cFe, "StubBend", cBend);

                cName = Form("h_StubInformation");
                cObj  = gROOT->FindObject(cName);
                TH2D* cStubInformation =
                    new TH2D(cName, Form("Stub Information - CIC%d ; Stub Bend; Stub Seed", (int)cFe->getId()), cBinsBend_Default.size() - 1, cBinsBend_Default.data(), 127 * 8 / 0.5, 0, 127 * 8);
                bookHistogram(cFe, "StubInformation", cStubInformation);

                cName = Form("h_StubHitCorrelation");
                cObj  = gROOT->FindObject(cName);
                TH2D* cStubHitCorrelation =
                    new TH2D(cName, Form("Stub & Hit correlation - CIC%d; Hit in Bottom Sensor ; Stub Seed", (int)cFe->getId()), 127 * 8, 0, 127 * 8, 127 * 8 / 0.5, 0, 127 * 8);
                bookHistogram(cFe, "StubHitCorrelation", cStubHitCorrelation);

                // now want to loop over all other FEs
                for(auto cOtherOpticalGroup: *cBoard)
                {
                    if(cOpticalGroup->getId() == cOtherOpticalGroup->getId()) continue;

                    for(auto cOtherFe: *cOtherOpticalGroup)
                    {
                        if(cFe->getId() == cOtherFe->getId()) continue;

                        cName = Form("h_BxId_Cic%d_Cic%d", cFe->getId(), cOtherFe->getId());
                        cObj  = gROOT->FindObject(cName);
                        if(cObj) delete cObj;
                        TString cTitle  = Form("BxId from 2 CICs [%d and %d] on links [%d and %d]; Bx Id [CIC %d]; BxId [CIC%d]",
                                              cFe->getId(),
                                              cOtherFe->getId(),
                                              static_cast<OuterTrackerHybrid*>(cFe)->getLinkId(),
                                              static_cast<OuterTrackerHybrid*>(cOtherFe)->getLinkId(),
                                              cFe->getId(),
                                              cOtherFe->getId());
                        TH2D*   cHist2D = new TH2D(cName, cTitle, 3565, 0, 3565, 3565, 0, 3565);
                        bookHistogram(cFe, Form("BxId_CIC%d", cOtherFe->getId()), cHist2D);
                    }
                }
            }
        }

        // matched stubs
        TString  cName = Form("h_NcandidateEvents");
        TObject* cObj  = gROOT->FindObject(cName);
        if(cObj) delete cObj;
        TH1D* cEvents = new TH1D(cName, Form("Candidate events; TDC"), 8, 0 - 0.5, 8 - 0.5);
        bookHistogram(cBoard, "CandidateEvents", cEvents);

        cName = Form("h_MatchedEvents");
        cObj  = gROOT->FindObject(cName);
        if(cObj) delete cObj;
        TH1D* cMatchedEvents = new TH1D(cName, Form("Matched events; TDC"), 8, 0 - 0.5, 8 - 0.5);
        bookHistogram(cBoard, "MatchedEvents", cMatchedEvents);

        cName             = Form("h_MatchingEfficiency");
        TEfficiency* cEff = new TEfficiency(cName, "Stub Matching Efficiency;TDC;#epsilon", 8, 0, 8);
        bookHistogram(cBoard, "MatchingEfficiency", cEff);

        // matched stubs
        cName = Form("h_BxId_Difference");
        cObj  = gROOT->FindObject(cName);
        if(cObj) delete cObj;
        TH1D* cProfile = new TH1D(cName, Form("#Delta{BxId}; Difference in BxId from all links"), 20, -10 - 0.5, 10 - 0.5);
        bookHistogram(cBoard, "BxIdDifference", cProfile);
    }
    //
    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
}

void StubQuickCheck::writeObjects()
{
    this->SaveResults();
    /*#ifdef __USE_ROOT__
        fDQMHistogramHybridTest.process();
    #endif*/
    fResultFile->Flush();
}
void StubQuickCheck::StubCheck(BeBoard* pBoard, const std::vector<Event*> pEvents)
{
    int          cNstubs             = 0;
    int          cNevents            = 0;
    TH1D*        cBxHisto            = static_cast<TH1D*>(getHist(pBoard, "BxIdDifference"));
    TH1D*        cCandidatesHist     = static_cast<TH1D*>(getHist(pBoard, "CandidateEvents"));
    TH1D*        cMatchesHist        = static_cast<TH1D*>(getHist(pBoard, "MatchedEvents"));
    TEfficiency* cMatchingEfficiency = static_cast<TEfficiency*>(getHist(pBoard, "MatchingEfficiency"));
    int          cSyncLoss           = 0;
    for(auto cEvent: pEvents)
    {
        auto                  cEventCount = cEvent->GetEventCount();
        auto                  cTDC        = cEvent->GetTDC();
        std::vector<uint32_t> cBxIds(0);

        LOG(DEBUG) << BOLDBLUE << "Event " << +cEventCount << " --- TDC  " << +cTDC << RESET;
        std::vector<double> cSeeds(0);

        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                TH1D* cBendHistogram      = static_cast<TH1D*>(getHist(cFe, Form("StubBend")));
                TH2D* cStubInformation    = static_cast<TH2D*>(getHist(cFe, Form("StubInformation")));
                TH2D* cStubHitCorrelation = static_cast<TH2D*>(getHist(cFe, Form("StubHitCorrelation")));

                auto cBxId = cEvent->BxId(cFe->getId());
                LOG(DEBUG) << BOLDBLUE << "FE" << +cFe->getId() << " BxId " << +cBxId << RESET;
                if(std::find(cBxIds.begin(), cBxIds.end(), cBxId) == cBxIds.end()) cBxIds.push_back(cBxId);

                // correlation plot for BxIds
                for(auto cOtherOpticalGroup: *pBoard)
                {
                    if(cOpticalGroup->getId() == cOtherOpticalGroup->getId()) continue;

                    for(auto cOtherFe: *cOtherOpticalGroup)
                    {
                        if(cFe->getId() == cOtherFe->getId()) continue;

                        TH2D* cBxCorrelation = static_cast<TH2D*>(getHist(cFe, Form("BxId_CIC%d", cOtherFe->getId())));
                        cBxCorrelation->Fill(cBxId, cEvent->BxId(cOtherFe->getId()));
                    }
                }

                for(auto cChip: *cFe)
                {
                    auto cHits  = cEvent->GetHits(cFe->getId(), cChip->getId());
                    auto cStubs = cEvent->StubVector(cFe->getId(), cChip->getId());

                    // quick cut on exactly one hit in each layer
                    if(cHits.size() > 2) continue;

                    bool cBottomSensor = false;
                    bool cTopSensor    = false;
                    for(auto cHit: cHits)
                    {
                        if(cHit % 2 == 0) cBottomSensor = true;

                        if(cHit % 2 != 0) cTopSensor = true;
                    }
                    if(!(cBottomSensor && cTopSensor)) continue;

                    for(auto cHit: cHits)
                    {
                        if(cHit % 2 == 0)
                        {
                            auto cStripHit    = cChip->getId() * 127 + std::floor(cHit / 2.0);
                            auto cHybridStrip = (cFe->getId() % 2 == 0) ? cStripHit : (8 * 127 - 1 - cStripHit);
                            for(auto cStub: cStubs)
                            {
                                auto cStripSeed       = cChip->getId() * 127 + cStub.getPosition() * 0.5;
                                auto cSeedHybridStrip = (cFe->getId() % 2 == 0) ? cStripSeed : (8 * 127 - 1 - cStripSeed);
                                cStubHitCorrelation->Fill(cHybridStrip, cSeedHybridStrip);
                            }
                            if(cStubs.size() == 0) cStubHitCorrelation->Fill(cHybridStrip, -1); // fill underflow bin if no stubs are present in the event
                        }
                    }

                    cCandidatesHist->Fill(cTDC);
                    cNevents += 1;
                    for(auto cStub: cStubs)
                    {
                        // cHist->Fill(cStub.getBend());
                        std::vector<uint8_t> cExpectedHits =
                            static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern(static_cast<ReadoutChip*>(cChip), cStub.getPosition(), cStub.getBend());
                        bool cMatchFound = false;
                        for(auto cExpectedHit: cExpectedHits)
                        {
                            auto cLookForMatch = std::find(cHits.begin(), cHits.end(), cExpectedHit);
                            if(cLookForMatch != cHits.end()) { cMatchFound = true; }
                        }
                        if(cMatchFound)
                        {
                            auto cBendValue   = (cStub.getBend() & 0x7);
                            auto cBendSign    = std::pow(-1, (cStub.getBend() & 0x8) >> 3);
                            auto cBendDefLUT  = (cBendSign < 0) ? cBendValue * cBendSign : cBendValue * cBendSign * 0.5;
                            auto cStrip       = cChip->getId() * 127 + cStub.getPosition() * 0.5;
                            auto cHybridStrip = (cFe->getId() % 2 == 0) ? cStrip : (8 * 127 - 1 - cStrip);
                            // LOG(DEBUG) << BOLDGREEN << "Stub seed " << +cStub.getPosition() << " - bend code " <<
                            // std::bitset<4>(cStub.getBend()) << " -- bend value " << +cBendValue << " sign is " <<
                            // +cBendSign << " --- " << +cBendDefLUT << RESET; LOG(DEBUG) << BOLDGREEN << ">>>event " <<
                            // +cEventCount << "\t..FE" << +cFe->getId() << "CBC" << +cChip->getId() << "\t.. MATCH
                            // FOUND! Stub seed " << +cStub.getPosition() << "-- TDC phase " << +cTDC << RESET;
                            cBendHistogram->Fill(cBendDefLUT);
                            cStubInformation->Fill(cBendDefLUT, cHybridStrip);
                            cNstubs += 1;
                            cMatchesHist->Fill(cTDC);
                        }
                        cMatchingEfficiency->Fill((float)cMatchFound, cTDC);
                    }
                }
            }
        }
        if(cBxIds.size() > 1)
        {
            for(size_t cIndex = 0; cIndex < cBxIds.size(); cIndex++)
            {
                for(size_t cIndex2 = cIndex + 1; cIndex2 < cBxIds.size(); cIndex2++) { cBxHisto->Fill(cBxIds[cIndex] - cBxIds[cIndex2]); }
            }
            cSyncLoss += 1;
            LOG(INFO) << BOLDRED << "Sync loss in event " << +cEventCount << RESET;
        }
        else
        {
            cBxHisto->Fill(0);
        }
    }
    LOG(INFO) << BOLDBLUE << "Found " << cNstubs << " stubs in " << +cNevents << " events with a stub and a hit in the same CBC. " << cSyncLoss << " events with a sync loss between CICs." << RESET;
}
// State machine control functions
void StubQuickCheck::Running() { Initialise(); }

void StubQuickCheck::Stop()
{
    this->SaveResults();
    fResultFile->Flush();
    dumpConfigFiles();

    SaveResults();
    CloseResultFile();
    Destroy();
}

void StubQuickCheck::Pause() {}

void StubQuickCheck::Resume() {}
#endif
