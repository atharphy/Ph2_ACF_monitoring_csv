#include "DataChecker.h"
#ifdef __USE_ROOT__

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "BackEndAlignment.h"
#include "Occupancy.h"
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

#include <random>

DataChecker::DataChecker() : Tool() { fRegMapContainer.reset(); }

DataChecker::~DataChecker() {}

void DataChecker::Initialise()
{
    // get threshold range
    auto     cSetting   = fSettingsMap.find("PulseShapeInitialVcth");
    uint16_t cInitialTh = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 400;
    cSetting            = fSettingsMap.find("PulseShapeFinalVcth");
    uint16_t cFinalTh   = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 600;
    cSetting            = fSettingsMap.find("PulseShapeVCthStep");
    uint16_t cThStep    = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 5;
    int      cSteps     = std::ceil((cFinalTh - cInitialTh) / (float)cThStep);

    // this is needed if you're going to use groups anywhere
    fChannelGroupHandler = new CBCChannelGroupHandler(); // This will be erased in tool.resetPointers()
    fChannelGroupHandler->setChannelGroupParameters(16, 2);
#ifdef __USE_ROOT__
//    fDQMHistogram.book(fResultFile,*fDetectorContainer);
#endif

    // retreive original settings for all chips
    ContainerFactory::copyAndInitChip<ChipRegMap>(*fDetectorContainer, fRegMapContainer);
    ContainerFactory::copyAndInitStructure<ChannelList>(*fDetectorContainer, fInjections);
    ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fDataMismatches);
    ContainerFactory::copyAndInitStructure<EventsList>(*fDetectorContainer, fBadEvents);
    ContainerFactory::copyAndInitStructure<EventsList>(*fDetectorContainer, fGoodEvents);

    // ContainerFactory::copyAndInitChip<std::vector<uint32_t>>(*fDetectorContainer, fBxIdsMatches);
    // ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fBxIdsMismatches);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cInjections = fInjections.at(cBoard->getIndex());
        auto& cMismatches = fDataMismatches.at(cBoard->getIndex());
        auto& cBadEvents  = fBadEvents.at(cBoard->getIndex());
        auto& cGoodEvents = fGoodEvents.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());
            auto& cMismatchesOpticalGroup = cMismatches->at(cOpticalGroup->getIndex());
            auto& cBadEventsOpticalGroup  = cBadEvents->at(cBoard->getIndex());
            auto& cGoodEventsOpticalGroup = cGoodEvents->at(cBoard->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
                auto& cMismatchesHybrid = cMismatchesOpticalGroup->at(cHybrid->getIndex());
                auto& cBadEventsHybrid  = cBadEventsOpticalGroup->at(cHybrid->getIndex());
                auto& cGoodEventsHybrid = cGoodEventsOpticalGroup->at(cHybrid->getIndex());
                // cBxIdsMatchesHybrid->getSummary<std::vector<uint32_t>().clear();

                for(auto cChip: *cHybrid)
                {
                    auto& cInjectionsChip = cInjectionsHybrid->at(cChip->getIndex());
                    auto& cMismatchesChip = cMismatchesHybrid->at(cChip->getIndex());
                    auto& cBadEventsChip  = cBadEventsHybrid->at(cChip->getIndex());
                    auto& cGoodEventsChip = cGoodEventsHybrid->at(cChip->getIndex());
                    //
                    cBadEventsChip->getSummary<EventsList>().clear();
                    cGoodEventsChip->getSummary<EventsList>().clear();
                    cInjectionsChip->getSummary<ChannelList>().clear();
                    cMismatchesChip->getSummary<uint32_t>() = 0;
                    fRegMapContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<ChipRegMap>() =
                        static_cast<ReadoutChip*>(cChip)->getRegMap();
                }
            }
        }
    }

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    // matched hits
                    TString  cName = Form("h_Hits_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    TObject* cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    TH2D* cHist2D = new TH2D(cName, Form("Number of hits - CBC%d; Trigger Number; Pipeline Address", (int)cChip->getId()), 40, 0 - 0.5, 40 - 0.5, 520, 0 - 0.5, 520 - 0.5);
                    bookHistogram(cChip, "Hits_perFe", cHist2D);

                    cName = Form("h_MatchedHits_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist2D = new TH2D(cName, Form("Number of matched hits - CBC%d; Trigger Number; Pipeline Address", (int)cChip->getId()), 40, 0 - 0.5, 40 - 0.5, 520, 0 - 0.5, 520 - 0.5);
                    bookHistogram(cChip, "MatchedHits_perFe", cHist2D);

                    cName = Form("h_EyeL1_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    TProfile2D* cProfile2D =
                        new TProfile2D(cName, Form("Number of matched hits - CBC%d; Phase Tap; Trigger Number", (int)cChip->getId()), 20, 0 - 0.5, 20 - 0.5, 40, 0 - 0.5, 40 - 0.5);
                    bookHistogram(cChip, "MatchedHits_eye", cProfile2D);

                    cName = Form("h_TestPulse_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cProfile2D =
                        new TProfile2D(cName, Form("Number of matched hits - CBC%d; Time [ns]; Test Pulse Amplitude [DAC units]", (int)cChip->getId()), 500, -250, 250, cSteps, cInitialTh, cFinalTh);
                    bookHistogram(cChip, "MatchedHits_TestPulse", cProfile2D);

                    cName = Form("h_StubLatency_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cProfile2D = new TProfile2D(cName,
                                                Form("Number of matched stubs - CBC%d; Latency [40 MHz clock cycles]; "
                                                     "Test Pulse Amplitude [DAC units]",
                                                     (int)cChip->getId()),
                                                512,
                                                0,
                                                512,
                                                cSteps,
                                                cInitialTh,
                                                cFinalTh);
                    bookHistogram(cChip, "StubLatency", cProfile2D);

                    cName = Form("h_HitLatency_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cProfile2D = new TProfile2D(cName,
                                                Form("Number of matched hits - CBC%d; Latency [40 MHz clock cycles]; "
                                                     "Test Pulse Amplitude [DAC units]",
                                                     (int)cChip->getId()),
                                                512,
                                                0,
                                                512,
                                                cSteps,
                                                cInitialTh,
                                                cFinalTh);
                    bookHistogram(cChip, "HitLatency", cProfile2D);

                    cName = Form("h_NoiseHits_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    TProfile* cHist = new TProfile(cName, Form("Number of noise hits - CBC%d; Channelr", (int)cChip->getId()), NCHANNELS, 0 - 0.5, NCHANNELS - 0.5);
                    bookHistogram(cChip, "NoiseHits", cHist);

                    cName = Form("h_MissedHits_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    TH1D* cHist1D = new TH1D(cName, Form("Events between missed hits - CBC%d; Channelr", (int)cChip->getId()), 1000, 0 - 0.5, 1000 - 0.5);
                    bookHistogram(cChip, "FlaggedEvents", cHist1D);

                    cName = Form("h_ptCut_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TProfile(cName, Form("Fraction of stubs matched to hits - CBC%d; Window Offset [half-strips]", (int)cChip->getId()), 14, -7 - 0.5, 7 - 0.5);
                    bookHistogram(cChip, "PtCut", cHist);
                }

                // matched stubs
                TString  cName = Form("h_Stubs_Cic%d", cHybrid->getId());
                TObject* cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                TH2D* cHist2D = new TH2D(cName, Form("Number of stubs - CIC%d; Trigger Number; Bunch Crossing Id", (int)cHybrid->getId()), 40, 0 - 0.5, 40 - 0.5, 4000, 0 - 0.5, 4000 - 0.5);
                bookHistogram(cHybrid, "Stubs", cHist2D);

                cName = Form("h_MatchedStubs_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist2D = new TH2D(cName, Form("Number of matched stubs - CIC%d; Trigger Number; Bunch Crossing Id", (int)cHybrid->getId()), 40, 0 - 0.5, 40 - 0.5, 4000, 0 - 0.5, 4000 - 0.5);
                bookHistogram(cHybrid, "MatchedStubs", cHist2D);

                cName = Form("h_MissedHits_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist2D = new TH2D(cName, Form("Number of missed hits - CIC%d; Iteration number; CBC Id", (int)cHybrid->getId()), 100, 0 - 0.5, 100 - 0.5, 8, 0 - 0.5, 8 - 0.5);
                bookHistogram(cHybrid, "MissedHits", cHist2D);

                // TProfile* cProfile = new TProfile ( cName, Form("Number of matched stubs - CIC%d; CBC; Fraction of
                // matched stubs",(int)cHybrid->getId()) ,8  , 0 -0.5 , 8 -0.5 ); bookHistogram ( cHybrid ,
                // "MatchedStubs", cProfile );

                // matched hits
                // cName = Form ( "h_MatchedHits");
                // cObj = gROOT->FindObject ( cName );
                // if ( cObj ) delete cObj;
                // cProfile = new TProfile ( cName, Form("Number of matched hits - CIC%d; CBC; Fraction of matched
                // hits",(int)cHybrid->getId()) ,8  , 0 -0.5 , 8 -0.5 ); bookHistogram ( cHybrid , "MatchedHits",
                // cProfile );

                // cName = Form ( "h_L1Status_Fe%d", cHybrid->getId() );
                // cObj = gROOT->FindObject ( cName );
                // if ( cObj ) delete cObj;
                // TH2D* cHist2D = new TH2D ( cName, Form("Error Flag CIC%d; Event Id; Chip Id; Error
                // Bit",(int)cHybrid->getId()) , 1000, 0 , 1000 , 9, 0-0.5 , 9-0.5 ); bookHistogram ( cHybrid,
                // "L1Status", cHist2D );
            }
        }
    }

    // read original thresholds from chips ...
    fDetectorDataContainer = &fThresholds;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, fThresholds);
    // read original logic configuration from chips .. [Pipe&StubInpSel&Ptwidth , HIP&TestMode]
    fDetectorDataContainer = &fLogic;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, fLogic);
    fDetectorDataContainer = &fHIPs;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, fHIPs);
    ContainerFactory::copyAndInitChip<int>(*fDetectorContainer, fHitCheckContainer);
    ContainerFactory::copyAndInitChip<int>(*fDetectorContainer, fStubCheckContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::CBC3)
                    {
                        ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                        fThresholds.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                            static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(theChip, "VCth");
                        fLogic.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                            static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(theChip, "Pipe&StubInpSel&Ptwidth");
                        fHIPs.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                            static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(theChip, "HIP&TestMode");
                    }
                }
            }
        }
    }

    // histograms
    // create some histograms that this tool needs
    for(auto cBoard: *fDetectorContainer)
    {
// book histogram that I need for BxId check
#ifdef __USE_ROOT__
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // number of P clusters per events
                TString  cName = Form("h_NPClusters_BxIds_Cic%d", cHybrid->getId());
                TObject* cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                // make sure errors are standard deviation of y
                TH2D* cHist =
                    new TH2D(cName, Form("Number of P clusters found by CIC%d; Injection Time [Bx]; Number of P clusters", (int)cHybrid->getId()), 500, 0 - 0.5, 500 - 0.5, 10, 0 - 0.5, 10 - 0.5);
                bookHistogram(cHybrid, "NPClusters", cHist);

                // number of stubs
                cName = Form("h_NStubs_BxIds_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                // make sure errors are standard deviation of y
                cHist = new TH2D(cName, Form("Number of Stubs found by CIC%d; Injection Time [Bx]; Number of Stubs", (int)cHybrid->getId()), 500, 0 - 0.5, 500 - 0.5, 10, 0 - 0.5, 10 - 0.5);
                bookHistogram(cHybrid, "NStubs", cHist);

                // number of clusters per Bx
                cName = Form("h_BxIds_NPclusters_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                // make sure errors are standard deviation of y
                cHist = new TH2D(cName, Form("Number of P clusters found by CIC%d; BxId; Number of P clusters", (int)cHybrid->getId()), 4000, 0 - 0.5, 4000 - 0.5, 10, 0 - 0.5, 10 - 0.5);
                bookHistogram(cHybrid, "BxId_Pclusters", cHist);

                // number of clusters per Bx
                cName = Form("h_BxIds_NStubs_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist = new TH2D(cName, Form("Number of Stubs found by CIC%d; BxId; Number of stubs", (int)cHybrid->getId()), 4000, 0 - 0.5, 4000 - 0.5, 10, 0 - 0.5, 10 - 0.5);
                bookHistogram(cHybrid, "BxId_Stubs", cHist);

                // event classification
                // first per bx Id
                cName = Form("h_EventClass_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist =
                    new TH2D(cName, Form("Event classification, digi-inject test CIC%d; #Delta_{Injection}; Classification", (int)cHybrid->getId()), 1000, 0 - 0.5, 1000 - 0.5, 10, 0 - 0.5, 10 - 0.5);
                cHist->GetYaxis()->SetBinLabel(1, "No P-clusters");
                cHist->GetYaxis()->SetBinLabel(2, "No Stubs");
                cHist->GetYaxis()->SetBinLabel(2, "Wrong number of Stubs");
                cHist->GetYaxis()->SetBinLabel(3, "Mismatch in P-clusters");
                cHist->GetYaxis()->SetBinLabel(4, "Mismatch in stubs");
                cHist->GetYaxis()->SetBinLabel(5, "Match");
                bookHistogram(cHybrid, "EventClass", cHist);

                // first per L1 Id
                cName = Form("h_EventClass_L1Id_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist = new TH2D(cName, Form("Event classification, digi-inject test CIC%d; L1Id; Classification", (int)cHybrid->getId()), 512, 0 - 0.5, 512 - 0.5, 10, 0 - 0.5, 10 - 0.5);
                bookHistogram(cHybrid, "EventClassL1Id", cHist);

                cName = Form("h_StubsExpected_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                TProfile2D* cProfile2D = new TProfile2D(
                    cName, Form("P-cluster Matching [raw count only], digi-inject test CIC%d; MPA Id; Number of Expected Stubs ", (int)cHybrid->getId()), 8, 0 - 0.5, 8 - 0.5, 20, 0 - 0.5, 20 - 0.5);
                bookHistogram(cHybrid, "PclusterMatchRawN", cProfile2D);

                cName = Form("h_L1Eye_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile2D = new TProfile2D(cName, Form("P-cluster Matching [L1-eye], digi-inject test CIC%d; MPA Id; Phase ", (int)cHybrid->getId()), 10, 0, 10, 20, 0, 20);
                bookHistogram(cHybrid, "PclusterMatchingL1Eye", cProfile2D);

                // first per L1 Id
                cName = Form("h_BxDifference_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                TH1D* cHist1D = new TH1D(cName, Form("Bx Id, digi-inject test CIC%d; Bx Id difference", (int)cHybrid->getId()), 1000, -500 - 0.5, 500 - 0.5);
                bookHistogram(cHybrid, "BxIdDifference", cHist1D);

                cName = Form("h_StubLatency_FE%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist = new TH2D(cName, Form("Stub counter, digi-inject test CIC%d; Stub Latency; MPA Id ", (int)cHybrid->getId()), 512, 0, 512, 8, 0, 8);
                bookHistogram(cHybrid, "StubLatency", cHist);

                cName = Form("h_HitLatency_FE%d_Pxls", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cHist) delete cObj;
                cHist = new TH2D(cName, Form("Pixel cluster counter, digi-inject test CIC%d; Hit Latency; MPA Id ", (int)cHybrid->getId()), 512, 0, 512, 8, 0, 8);
                bookHistogram(cHybrid, "PixelHitLatency", cHist);

                cName = Form("h_StubLatency_FE%d_Pxls", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cHist) delete cObj;
                cHist = new TH2D(cName, Form("Stub latency, digi-inject test CIC%d; Hit Latency; MPA Id ", (int)cHybrid->getId()), 512, 0, 512, 8, 0, 8);
                bookHistogram(cHybrid, "PixelStubLatency", cHist);

                cName = Form("h_HitLatency_FE%d_Strps", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cHist) delete cObj;
                cHist = new TH2D(cName, Form("Strip cluster counter, digi-inject test CIC%d; Hit Latency; SSA Id ", (int)cHybrid->getId()), 512, 0, 512, 8, 0, 8);
                bookHistogram(cHybrid, "StripHitLatency", cHist);

                //
                cName = Form("h_StubCounter_FE%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist = new TH2D(cName, Form("Stub counter, digi-inject test CIC%d; Number of injected Stubs; Number of stubs in the readout", (int)cHybrid->getId()), 20, 0, 20, 20, 0, 20);
                bookHistogram(cHybrid, "StubCounter", cHist);

                cName = Form("h_StubCounter2_FE%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile2D = new TProfile2D(cName, Form("Stub counter, digi-inject test CIC%d; Number of injected Stubs; MPA Id;", (int)cHybrid->getId()), 20, 0, 20, 10, 0, 10);
                bookHistogram(cHybrid, "StubCounterIds", cProfile2D);

                cName = Form("h_ClusterCounter_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile2D = new TProfile2D(cName, Form("Cluster counter, digi-inject test CIC%d; Total number of injected Clusters; MPA Id;", (int)cHybrid->getId()), 100, 0, 100, 16, 0, 16);
                bookHistogram(cHybrid, "ClusterCounter", cProfile2D);

                cName = Form("h_BxMatching_FE%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist = new TH2D(cName, Form("Bx counter, digi-inject test CIC%d; #Delta Bx_{Injection}; #Delta Bx_{Readout}", (int)cHybrid->getId()), 500, 0, 500, 500, 0, 500);
                bookHistogram(cHybrid, "BxMatching", cHist);

                cName = Form("h_BxCounter_FE%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist1D = new TH1D(cName, Form("Bx counter, digi-inject test CIC%d; #Delta Bx_{Readout}; Count", (int)cHybrid->getId()), 500, 0, 500);
                bookHistogram(cHybrid, "BxCounter", cHist1D);

                //
                cName = Form("h_EventCounter_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist1D = new TH1D(cName, Form("Event Counter, digi-inject test CIC%d; Number of Injected Clusters; Requested Events", (int)cHybrid->getId()), 100, 0, 100);
                bookHistogram(cHybrid, "EventCounter", cHist1D);

                cName = Form("h_MissingEventCounter_Cic%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist1D = new TH1D(cName, Form("Missing Event Counter, digi-inject test CIC%d; Number of Injected Clusters; Readout Events", (int)cHybrid->getId()), 100, 0, 100);
                bookHistogram(cHybrid, "MissingEventCounter", cHist1D);

                //
                cName = Form("h_L1Monitor_FE%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist = new TH2D(cName, Form("L1 Monitor, digi-inject test CIC%d; Event Count; L1 Id", (int)cHybrid->getId()), 100, 0, 100, 520, 0, 520);
                bookHistogram(cHybrid, "L1Monitor", cHist);

                cName = Form("h_L1Status_FE%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile2D = new TProfile2D(cName, Form("L1 Status, digi-inject test CIC%d; #Delta B_{x}; MPA Id;", (int)cHybrid->getId()), 500, 0, 500, 10, 0, 10);
                bookHistogram(cHybrid, "L1StatusMonitor", cProfile2D);

                cName = Form("h_L1StatusCSize_FE%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile2D = new TProfile2D(cName, Form("L1 Status, digi-inject test CIC%d; Total Number of Clusters; MPA Id;", (int)cHybrid->getId()), 500, 0, 500, 10, 0, 10);
                bookHistogram(cHybrid, "L1StatusMonitorClusterSize", cProfile2D);

                cName = Form("h_L1MonitorProfile_FE%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile2D = new TProfile2D(cName, Form("L1 Monitor, digi-inject test CIC%d; L1 Id; Total number of clusters", (int)cHybrid->getId()), 100, 0, 100, 100, 0, 100, "S");
                bookHistogram(cHybrid, "L1MonitorProfile", cProfile2D);

                cName = Form("h_EvntMonitorProfile_FE%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile2D = new TProfile2D(cName, Form("Event Monitor, digi-inject test CIC%d; Event Id; Total number of clusters", (int)cHybrid->getId()), 5000, 0, 5000, 100, 0, 100, "S");
                bookHistogram(cHybrid, "EvntMonitorProfile", cProfile2D);

                for(auto cChip: *cHybrid)
                {
                    cName = Form("h_Inj_FE%d", cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName, Form("Injected P-cluster, digi-inject test MPA%d CIC%d; Row; Column", (int)cChip->getId(), (int)cHybrid->getId()), 20, 0, 20, 130, 0, 130);
                    bookHistogram(cChip, "InjectionMap", cHist);

                    cName = Form("h_MatchedInj_FE%d", cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName, Form("Matched P-cluster, digi-inject test MPA%d CIC%d; Row; Column", (int)cChip->getId(), (int)cHybrid->getId()), 20, 0, 20, 130, 0, 130);
                    bookHistogram(cChip, "MatchedInjectionMap", cHist);

                    cName = Form("h_MismatchedInj_FE%d", cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName, Form("Mismatched Clusters, digi-inject test MPA%d CIC%d; Row; Column", (int)cChip->getId(), (int)cHybrid->getId()), 20, 0, 20, 130, 0, 130);
                    bookHistogram(cChip, "MismatchedInjectionMap", cHist);

                    cName = Form("h_ClusterCounter_FE%d", cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName,
                                     Form("Cluster Counter, digi-inject test CIC%d MPA%d; Number of Clusters injected; Average number of clusters readout", (int)cChip->getId(), (int)cHybrid->getId()),
                                     30,
                                     0,
                                     30,
                                     30,
                                     0,
                                     30);
                    bookHistogram(cChip, "ClusterCounter", cHist);

                    cName = Form("h_L1Monitor_FE%d", cHybrid->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName, Form("L1 Monitor, digi-inject test CIC%d; L1 Id; Total number of clusters", (int)cChip->getId()), 100, 0, 100, 100, 0, 100);
                    bookHistogram(cChip, "L1Monitor", cHist);

                    cName = Form("h_L1MonitorMatched_FE%d", cHybrid->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName, Form("L1 Monitor, digi-inject test CIC%d; L1 Id; Total number of clusters", (int)cChip->getId()), 100, 0, 100, 100, 0, 100);
                    bookHistogram(cChip, "L1MonitorMatched", cHist);

                    cName = Form("h_L1MonitorProfile_FE%d", cHybrid->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cProfile2D = new TProfile2D(cName, Form("L1 Monitor, digi-inject test CIC%d; L1 Id; Total number of clusters", (int)cChip->getId()), 100, 0, 100, 100, 0, 100, "S");
                    bookHistogram(cChip, "L1MonitorProfile", cProfile2D);

                    cName = Form("h_EvntMonitorProfile_FE%d", cHybrid->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cProfile2D = new TProfile2D(cName, Form("Event Monitor, digi-inject test CIC%d; Event Id; Total number of clusters", (int)cChip->getId()), 5000, 0, 5000, 100, 0, 100, "S");
                    bookHistogram(cChip, "EvntMonitorProfile", cProfile2D);

                    cName = Form("h_MismatchProfile_FE%d", cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    TProfile* cProfile =
                        new TProfile(cName, Form("Event Monitor, digi-inject test CIC%d; Total number of clusters; First L1Id with a mismatch", (int)cChip->getId()), 100, 0, 100, "S");
                    bookHistogram(cChip, "MismatchProfile", cProfile);

                    if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                    cName = Form("h_PInj_FE%d", cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist1D =
                        new TH1D(cName, Form("P-cluster Matching [Inj], digi-inject test MPA%d CIC%d; Number of Clusters; Count", (int)cChip->getId(), (int)cHybrid->getId()), 10, 0 - 0.5, 10 - 0.5);
                    bookHistogram(cChip, "PclusterInjCount", cHist1D);

                    cName = Form("h_PInj2D_FE%d", cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName,
                                     Form("P-cluster Matching [Inj], digi-inject test MPA%d CIC%d; Number of Clusters; Total number of clusters", (int)cChip->getId(), (int)cHybrid->getId()),
                                     10,
                                     0 - 0.5,
                                     10 - 0.5,
                                     100,
                                     0 - 0.5,
                                     100 - 0.5);
                    bookHistogram(cChip, "PclusterInjCount2D", cHist);

                    cName = Form("h_PMatched_FE%d", cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist1D = new TH1D(
                        cName, Form("P-cluster Matching [Matched], digi-inject test CIC%d MPA%d; Number of Clusters; Count", (int)cChip->getId(), (int)cHybrid->getId()), 10, 0 - 0.5, 10 - 0.5);
                    bookHistogram(cChip, "PclusterMatchedCount", cHist1D);

                    cName = Form("h_PMatched2D_FE%d", cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName,
                                     Form("P-cluster Matching [Matched], digi-inject test CIC%d MPA%d; Number of Clusters; Total number of clusters", (int)cChip->getId(), (int)cHybrid->getId()),
                                     10,
                                     0 - 0.5,
                                     10 - 0.5,
                                     100,
                                     0 - 0.5,
                                     100 - 0.5);
                    bookHistogram(cChip, "PclusterMatchedCount2D", cHist);

                    cName = Form("h_StubCounter_H%d_FE%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cProfile2D =
                        new TProfile2D(cName, Form("Stub counter, digi-inject test CIC%d; Number of injected Stubs; Number of stubs in this MPA", (int)cHybrid->getId()), 20, 0, 20, 20, 0, 20);
                    bookHistogram(cChip, "StubCounter", cProfile2D);

                    cName = Form("h_StubCounter2_H%d_FE%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(
                        cName, Form("Stub counter, digi-inject test CIC%d; Number of stubs in this MPA; Number of stubs in readout [in this MPA]", (int)cHybrid->getId()), 20, 0, 20, 20, 0, 20);
                    bookHistogram(cChip, "StubCounter2", cHist);

                    cName = Form("h_L1StatusGlblCls_FE%d", cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName, Form("L1 Status, digi-inject test CIC%d; Total Number of Clusters in L1 packet; Error Status;", (int)cHybrid->getId()), 100, 0, 100, 4, 0, 4);
                    cHist->GetYaxis()->SetBinLabel(1, "No error");
                    cHist->GetYaxis()->SetBinLabel(2, "FE Error/Overflow");
                    cHist->GetYaxis()->SetBinLabel(3, "Soft Overflow");
                    cHist->GetYaxis()->SetBinLabel(4, "Global Overflow");
                    bookHistogram(cChip, "L1StatusMonitorGlbl", cHist);

                    cName = Form("h_L1StatusLclCls_FE%d", cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName, Form("L1 Status, digi-inject test CIC%d; Total Number of Clusters in this MPA; Error Status;", (int)cHybrid->getId()), 100, 0, 100, 4, 0, 4);
                    cHist->GetYaxis()->SetBinLabel(1, "No error");
                    cHist->GetYaxis()->SetBinLabel(2, "FE Error/Overflow");
                    cHist->GetYaxis()->SetBinLabel(3, "Soft Overflow");
                    cHist->GetYaxis()->SetBinLabel(4, "Global Overflow");
                    bookHistogram(cChip, "L1StatusMonitorLcl", cHist);
                }
            }
        }
#endif
    }
    zeroContainers();
}

void DataChecker::zeroContainers()
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    fHitCheckContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>()  = 0;
                    fStubCheckContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = 0;
                }
            }
        }
    }
}
void DataChecker::print(std::vector<uint8_t> pChipIds)
{
    for(auto cBoard: fHitCheckContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto cChipId = cChip->getId();
                    if(std::find(pChipIds.begin(), pChipIds.end(), cChipId) == pChipIds.end()) continue;
                    auto cHitCheck  = cChip->getSummary<uint16_t>();
                    auto cStubCheck = fStubCheckContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                    LOG(INFO) << BOLDBLUE << "\t\t...Found " << +cHitCheck << " matched hits and " << +cStubCheck << " matched stubs in readout chip" << +cChipId << RESET;
                }
            }
        }
    }
}

void DataChecker::AnaInjectionTestPS(uint32_t pMaxTriggersToAccept)
{
    // configure fast command block
    size_t cNrepetitions = 1;
    for(auto cBoard: *fDetectorContainer)
    {
        // stop triggers
        fBeBoardInterface->Stop(cBoard);
        // repeat the sequence N times
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.generic_fcmd.number_of_repetitions", cNrepetitions);
        // make sure fast command duration is 0
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.fast_duration", 0x0);
        // make sure I accept all trgigers
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0);
        // make sure data handshake is disabled
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x00);
    }
    // re-load configuration
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetTriggerFSM();
    // also .. reset the readout
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();

    // configure latencies
    auto   cSetting       = fSettingsMap.find("DelayAfterInjection");
    size_t cCalPulseDelay = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 85;
    cSetting              = fSettingsMap.find("DistributeInjections");
    int  cInjDistrFlag    = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    bool cDistributeInj   = (cInjDistrFlag == 1);
    //
    cSetting            = fSettingsMap.find("Bend");
    int     cBend       = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    uint8_t cStubWindow = (std::fabs(cBend) + 1) * 2; // stub window in half pixels (1)
    // check for stubs
    cSetting            = fSettingsMap.find("CheckForStubs");
    int  cCheckForStubs = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    bool cWStubs        = (cCheckForStubs == 1);
    // mode
    // (0) pixel-strip, (1) strip-strip, (2) pixel-pixel, (3) strip-pixel
    cSetting            = fSettingsMap.find("TestMode");
    uint8_t cMode       = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    cSetting            = fSettingsMap.find("TriggerMultiplicity");
    size_t cTriggerMult = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    cSetting            = fSettingsMap.find("FifoDepth");
    int cFifoDepth      = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 8;
    // configure FIFO depth in SSAs
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                auto&    cCic   = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                uint16_t cValue = fCicInterface->ReadChipReg(cCic, "FE_ENABLE");
                LOG(INFO) << BOLDMAGENTA << "FE_ENABLE register in CIC set to 0x" << std::hex << +cValue << std::dec << RESET;
                for(auto cChip: *cHybrid) // for each chip (makes sense)
                {
                    // for the moment - only written for CBC3
                    if(cChip->getFrontEndType() != FrontEndType::SSA) continue;

                    fReadoutChipInterface->WriteChipReg(cChip, "OutPattern7/FIFOconfig", cFifoDepth);
                }
            }
        }
    }

    size_t cNEventsPerAttempt = 1; // * 30 * 10;
    // random c++
    std::srand(std::time(NULL));
    std::random_device cRndm{};
    std::mt19937       cGen{cRndm()};

    //
    cSetting = fSettingsMap.find("ActiveMPAs");
    // int cLastMPA = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    // configure injections
    cSetting         = fSettingsMap.find("MinPclusters");
    int cMinNpClstrs = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    cSetting         = fSettingsMap.find("MaxPclusters");
    int cMaxNpClstrs = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    cSetting         = fSettingsMap.find("MinSclusters");
    int cMinNsClstrs = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    cSetting         = fSettingsMap.find("MaxSclusters");
    int cMaxNsClstrs = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    cSetting         = fSettingsMap.find("MaxStubs");
    int cMaxNstubs   = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    // cluster distributions
    std::uniform_int_distribution<int> cFlatDistPxlCltrs(cMinNpClstrs, cMaxNpClstrs);
    std::uniform_int_distribution<int> cFlatDistStrpCltrs(cMinNsClstrs, cMaxNsClstrs);
    // LOG(INFO) << BOLDMAGENTA << "P-cluster distribution : " << cMinNpClstrs << " to " << cMaxNpClstrs << RESET;
    // LOG(INFO) << BOLDMAGENTA << "S-cluster distribution : " << cMinNsClstrs << " to " << cMaxNsClstrs << RESET;
    // int  cMaxStubSel    = (cDistributeInj) ? 8 : 1;
    // auto cStubOffset = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->getStubOffset();
    cSetting                   = fSettingsMap.find("ScanL1Latency");
    uint8_t cScanL1Latency     = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    int     cOptimalOffset     = -1 + (cTriggerMult > 1);
    int     cL1MinOffset       = (cScanL1Latency == 1) ? cOptimalOffset - 2 : cOptimalOffset;
    int     cL1MaxOffset       = (cScanL1Latency == 1) ? cOptimalOffset + cTriggerMult + 2 : cL1MinOffset + 1;
    cSetting                   = fSettingsMap.find("ScanStubLatency");
    uint8_t cScanStubLatency   = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    int     cOptimalStubOffset = 0;
    int     cStubMinOffset     = (cScanStubLatency == 1 && cWStubs) ? cOptimalStubOffset - 5 : cOptimalStubOffset;
    int     cStubMaxOffset     = (cScanStubLatency == 1 && cWStubs) ? cOptimalStubOffset + 5 : cOptimalStubOffset + 1;
    LOG(INFO) << BOLDMAGENTA << "DataChecker::InjectionTestPS TestMode is " << +cMode << RESET;
    for(int cLatencyOffset = cL1MinOffset; cLatencyOffset < cL1MaxOffset; cLatencyOffset++)
    {
        for(int cStubOffset = cStubMinOffset; cStubOffset < cStubMaxOffset; cStubOffset++)
        {
            LOG(INFO) << BOLDMAGENTA << "L1 Latency offset is " << +cLatencyOffset << "\t...Stub Latency offset is " << +cStubOffset << RESET;
            // configure latencies
            DetectorDataContainer cLatencyPerFE;
            ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, cLatencyPerFE);
            DetectorDataContainer cStubLatencyPerBoard;
            ContainerFactory::copyAndInitBoard<uint16_t>(*fDetectorContainer, cStubLatencyPerBoard);
            DetectorDataContainer cPackageDelayPerBoard;
            ContainerFactory::copyAndInitBoard<uint16_t>(*fDetectorContainer, cPackageDelayPerBoard);

            for(auto cBoard: *fDetectorContainer)
            {
                uint16_t cDelay        = cCalPulseDelay;
                int      cReTimeValue  = -1;
                auto&    cFeLatency    = cLatencyPerFE.at(cBoard->getIndex());
                auto&    cBrdLatency   = cStubLatencyPerBoard.at(cBoard->getIndex());
                auto&    cBrdDelay     = cPackageDelayPerBoard.at(cBoard->getIndex());
                auto&    cPackageDelay = cBrdDelay->getSummary<uint16_t>();
                cPackageDelay          = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
                auto cInitStubOffset   = cBoard->getStubOffset();
                for(auto cOpticalReadout: *cBoard)
                {
                    auto& cFeLatencyOG = cFeLatency->at(cOpticalReadout->getIndex());
                    for(auto cHybrid: *cOpticalReadout)
                    {
                        auto& cFeLatencyHybrid = cFeLatencyOG->at(cHybrid->getIndex());
                        for(auto cChip: *cHybrid) // for each chip (makes sense)
                        {
                            auto& cFeLatencyChip = cFeLatencyHybrid->at(cChip->getIndex());
                            auto& cFeLatencySmry = cFeLatencyChip->getSummary<uint16_t>();
                            if(cChip->getFrontEndType() != FrontEndType::SSA)
                            {
                                cFeLatencySmry = (cDelay) + cLatencyOffset;
                                if(cChip->getFrontEndType() == FrontEndType::MPA && cReTimeValue < 0) { cReTimeValue = fReadoutChipInterface->ReadChipReg(cChip, "RetimePix"); }
                            }
                            else // SSA needs an additional clock cycle of delay
                            {
                                cFeLatencySmry = (cDelay) + (cLatencyOffset - 1);
                            }
                            if(cDistributeInj) cFeLatencySmry = cFeLatencySmry + cChip->getId();
                            fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cFeLatencySmry);
                        } // chip
                    }     // hybrid
                }         // module
                if(cWStubs)
                {
                    auto& cStubLatency = cBrdLatency->getSummary<uint16_t>();
                    cStubLatency       = cDelay + cLatencyOffset - (cInitStubOffset + cStubOffset + cReTimeValue);
                    // cStubLatency       = cStubOffset;
                    fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
                    LOG(INFO) << BOLDMAGENTA << "\t\t.. Package delay set to " << +cPackageDelay << "... stub latency set to " << +cStubLatency << RESET;
                }
            } // boards

            size_t cAttempt  = 0;
            bool   cFinished = false;
            // uint32_t cMaxTriggers=pMaxTriggersToAccept;
            PSEvents              cInjectedPSevents;
            DetectorDataContainer cTriggersPerBoard;
            ContainerFactory::copyAndInitBoard<uint32_t>(*fDetectorContainer, cTriggersPerBoard);
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cTrgCntBrd = cTriggersPerBoard.at(cBoard->getIndex());
                auto& cNtriggers = cTrgCntBrd->getSummary<uint32_t>();
                cNtriggers       = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
            }
            // zero event id
            // and zero trigger id
            fPSevent.fEventId      = 0;
            fPSevent.fTriggerId    = 0;
            size_t cInjectedEvents = 0;

            std::map<int, std::vector<Injection>> cInjectionScheme;
            cInjectionScheme.clear();
            do {
                cInjectionScheme[cAttempt] = this->GeneratePSInjections(cMaxNstubs);
                LOG(INFO) << BOLDMAGENTA << "Injection#" << +cAttempt << "\t\t..." << RESET;
                std::sort(std::begin(cInjectionScheme[cAttempt]), std::end(cInjectionScheme[cAttempt]), [](Injection a, Injection b) { return a.fRow < b.fRow; });
                // std::sort(std::begin(cInjectionScheme[cAttempt]), std::end(cInjectionScheme[cAttempt]), [](Injection a, Injection b) { return a.fColumn < b.fColumn; });

                // LOG(INFO) << BOLDMAGENTA << "Attempt#" << +cAttempt << RESET;
                // uint8_t cMaxClustersPerMPA = cFlatDistPxlCltrs(cGen);
                // uint8_t cMaxClustersPerSSA = cFlatDistStrpCltrs(cGen);
                // if(cAttempt % 10 == 0)
                //     LOG(INFO) << BOLDMAGENTA << "Attempt#" << +cAttempt << " -- injecting " << +cMaxClustersPerMPA << " pixel clusters "
                //               << " and " << +cMaxClustersPerSSA << " strip clusters." << RESET;
                // if(cMaxClustersPerSSA == 20) LOG(INFO) << BOLDRED << "\t\t... Attempt#" << +cAttempt << " 20 S-clusters ... " << RESET;
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                for(auto cBoard: *fDetectorContainer) { fBeBoardInterface->ChipReSync(cBoard); }
                std::this_thread::sleep_for(std::chrono::microseconds(500));

                // use generic fast commands to inject N times
                // generate fast commands
                this->FastCommandInjections(cNEventsPerAttempt);
                for(auto cBoard: *fDetectorContainer)
                {
                    // auto& cInjectionsBrd = cInjections.at(cBoard->getIndex());
                    for(auto cOpticalReadout: *cBoard)
                    {
                        // auto& cInjectionsOG = cInjectionsBrd->at(cOpticalReadout->getIndex());
                        for(auto cHybrid: *cOpticalReadout)
                        {
                            // auto& cInjectionsHybrid = cInjectionsOG->at(cHybrid->getIndex());
                            // auto& cInjs             = cInjectionsHybrid->getSummary<std::vector<Injection>>();
                            std::vector<uint8_t> cIds(0);
                            for(auto cChip: *cHybrid)
                            {
                                if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                                cIds.push_back(cChip->getId() % 8);
                            }

                            for(auto cId: cIds)
                            {
                                uint8_t cPattern = cDistributeInj ? (1 << cId) : (0x1 << 0);
                                for(auto cChip: *cHybrid)
                                {
                                    if(cChip->getFrontEndType() == FrontEndType::MPA) continue;
                                    if(cChip->getId() % 8 != cId) continue;

                                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                                    fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x01);
                                    fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_L_ALL", cPattern);
                                    for(auto cInjection: cInjectionScheme[cAttempt]) { fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cInjection.fRow), 0x9); }
                                }
                                for(auto cChip: *cHybrid)
                                {
                                    if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                                    if(cChip->getId() % 8 != cId) continue;

                                    fReadoutChipInterface->WriteChipReg(cChip, "StubMode", cMode);
                                    fReadoutChipInterface->WriteChipReg(cChip, "StubWindow", cStubWindow);
                                    (static_cast<PSInterface*>(fReadoutChipInterface))->WriteChipReg(cChip, "DigitalSync", 0x00);
                                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                                    std::vector<Injection> cMPAInj;
                                    cMPAInj.clear();
                                    for(size_t cInjIndx = 0; cInjIndx < cInjectionScheme[cAttempt].size(); cInjIndx++)
                                    {
                                        if(cInjIndx == cAttempt % cMaxNstubs) cMPAInj.push_back(cInjectionScheme[cAttempt].at(cInjIndx));
                                    }
                                    (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, cMPAInj, cPattern);
                                }
                            }
                        } // hybrid
                    }     // module
                }         // boards - injections

                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFCMDBram(fFastCommands);
                // LOG (INFO) << BOLDMAGENTA << "Fast command block used to send " << +fTriggeredBxs.size() << " triggers." << RESET;
                cInjectedEvents += fTriggeredBxs.size();
                cFinished = true;
                for(auto cBoard: *fDetectorContainer)
                {
                    // start generic  - ctrl signal high
                    fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x1);
                    // stop generic  - ctrl signal low
                    fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x0);
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    auto&  cTrgCntBrd     = cTriggersPerBoard.at(cBoard->getIndex());
                    auto&  cNtriggersInit = cTrgCntBrd->getSummary<uint32_t>();
                    auto   cNtriggers     = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
                    size_t cIter          = 0;
                    do {
                        std::this_thread::sleep_for(std::chrono::microseconds(100));
                        // auto cNWords    = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
                        cNtriggers = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
                        // LOG(INFO) << BOLDMAGENTA << "\t\t.. Iter#" << +cIter << " trigger in counter is " << cNtriggers << " .. expect to see " << +fTriggeredBxs.size() << RESET;
                        cIter++;
                    } while((cNtriggers - cNtriggersInit) < fTriggeredBxs.size());
                }
                cFinished = (cInjectedEvents >= pMaxTriggersToAccept);
                cAttempt++;
            } while(!cFinished); // inject N clusters for M triggers

            // now readout all data
            for(auto cBoard: *fDetectorContainer)
            {
                //
                auto& cBrdLatency  = cStubLatencyPerBoard.at(cBoard->getIndex());
                auto& cStubLatency = cBrdLatency->getSummary<uint16_t>();
                //
                auto& cBrdDelay     = cPackageDelayPerBoard.at(cBoard->getIndex());
                auto& cPackageDelay = cBrdDelay->getSummary<uint16_t>();
                //
                // auto&                 cFeLatency = cLatencyPerFE.at(cBoard->getIndex());
                std::vector<uint32_t> cData(0);
                uint32_t              cNevents = ReadData(cBoard, cData, false);
                DecodeData(cBoard, cData, cNevents, fBeBoardInterface->getBoardType(cBoard));
                const std::vector<Event*>& cPh2Events     = GetEvents();
                size_t                     cEventsReadout = cPh2Events.size();
                LOG(INFO) << BOLDMAGENTA << "Readout " << cEventsReadout << " events from uDTC.. injected " << cInjectedEvents << RESET;
                // fPSevent.fReadoutSuccess = cPh2Events.size() == cInjectedPSevents.size();
                size_t cEventCounter    = 0;
                fPSevent.fStubLatency   = cStubLatency;
                fPSevent.fPackageDelay  = cPackageDelay;
                fPSevent.fStubOffset    = cStubOffset;
                fPSevent.fLatencyOffset = cLatencyOffset;
                // int cTotalStubsFound = 0;
                // int cTotalStubsExpected = 0;
                //
                cSetting                 = fSettingsMap.find("TriggerMultiplicity");
                size_t cTriggerMult      = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
                size_t cInjectionCounter = 0;
                auto   cEventIter        = cPh2Events.begin();
                do {
                    auto cInjections = cInjectionScheme[cInjectionCounter];
                    LOG(INFO) << BOLDMAGENTA << "Checking result for injection#" << cInjectionCounter << RESET;
                    for(auto cInj: cInjections) { LOG(INFO) << BOLDYELLOW << "\t\t.. injecting in row " << +cInj.fRow << " columnn " << +cInj.fColumn << RESET; }
                    for(size_t cTriggerId = 0; cTriggerId < cTriggerMult; cTriggerId++)
                    {
                        if(cEventIter >= cPh2Events.end()) continue;

                        for(auto cOpticalGroup: *cBoard)
                        {
                            for(auto cHybrid: *cOpticalGroup)
                            {
                                fPSevent.fL1IdCic      = static_cast<D19cCic2Event*>(*cEventIter)->L1Id(cHybrid->getId(), 42);
                                fPSevent.fL1Status     = static_cast<D19cCic2Event*>(*cEventIter)->L1Status(cHybrid->getId());
                                fPSevent.fBxId         = (*cEventIter)->BxId(cHybrid->getId());
                                fPSevent.fHybridId     = cHybrid->getId();
                                fPSevent.fPClusterSize = static_cast<D19cCic2Event*>(*cEventIter)->GetNPixelClusters(cHybrid->getId());
                                fPSevent.fSClusterSize = static_cast<D19cCic2Event*>(*cEventIter)->GetNStripClusters(cHybrid->getId());
                                // size_t cNStubsExpected = 1*cHybrid->size();
                                // size_t cNPclustersExpected = 1*cHybrid->size();
                                // size_t cNSclustersExpected = cInjections.size()*cHybrid->size();
                                size_t cNStubs = 0;
                                for(auto cChip: *cHybrid)
                                {
                                    if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                                    fPSevent.fChipL1Id   = static_cast<D19cCic2Event*>(*cEventIter)->L1Id(cHybrid->getId(), cChip->getId());
                                    auto cPclstrs        = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(cHybrid->getId(), cChip->getId());
                                    auto cSclstrs        = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(cHybrid->getId(), cChip->getId());
                                    auto cStubs          = static_cast<D19cCic2Event*>(*cEventIter)->StubVector(cHybrid->getId(), cChip->getId());
                                    fPSevent.fNPclusters = cPclstrs.size();
                                    fPSevent.fNSclusters = cSclstrs.size();
                                    fPSevent.fStubSize   = cStubs.size();
                                    cNStubs += cStubs.size();
                                    if(fPSevent.fNPclusters > 0 || fPSevent.fNSclusters > 0)
                                    {
                                        LOG(INFO) << BOLDGREEN << "Trigger#" << +cTriggerId << " in a burst of " << (1 + cTriggerMult) << " MPA" << +cChip->getId() << " found " << fPSevent.fNPclusters
                                                  << " P clusters, " << fPSevent.fNSclusters << " S clusters, and " << fPSevent.fStubSize << " stubs" << RESET;
                                        for(auto cCluster: cPclstrs)
                                        {
                                            LOG(INFO) << BOLDYELLOW << "P-cluster in row " << +cCluster.fAddress << " column " << +cCluster.fZpos << " width is " << +cCluster.fWidth << RESET;
                                        }
                                        for(auto cCluster: cSclstrs)
                                        {
                                            LOG(INFO) << BOLDCYAN << "S-cluster in row " << +cCluster.fAddress << " column " << (0) << " width is " << +cCluster.fWidth << RESET;
                                        }
                                        if(fPSevent.fStubSize != 0)
                                        {
                                            std::sort(std::begin(cStubs), std::end(cStubs), [](Stub a, Stub b) { return a.getRow() < b.getRow(); });
                                            // std::sort(std::begin(cStubs), std::end(cStubs), [](Stub a, Stub b) { return a.getPosition() < b.getPosition(); });
                                            size_t cStubCntr = 0;
                                            for(auto cStub: cStubs)
                                            {
                                                LOG(INFO) << BOLDCYAN << "\t\tStub#" << +cStubCntr << " Position " << +cStub.getPosition() << " - Row " << +cStub.getRow() << " - Bend "
                                                          << +cStub.getBend() << RESET;
                                                cStubCntr++;
                                            }
                                        }
                                    }
                                } // chip
                                // LOG (INFO) << BOLDGREEN << "Trigger#" << +cTriggerId << " in a burst of " << (1+ cTriggerMult)
                                //             << " CIC " << +cHybrid->getId()
                                //             << " found " << +cNStubs  << " stubs when " << cNStubsExpected << " were expected "
                                //             << " found " << +fPSevent.fPClusterSize  << " P-clusters when " << cNPclustersExpected << " were expected "
                                //             << " found " << +fPSevent.fSClusterSize  << " S-clusters when " << cNSclustersExpected << " were expected "
                                //             << RESET;
                            } // hybrid
                        }     // OG
                        cEventIter++;
                        cEventCounter++;
                    } // trigger loop
                    cInjectionCounter++;
                } while(cEventIter < cPh2Events.end());
                // LOG (INFO) << BOLDMAGENTA << "Found " << cTotalStubsFound << " when " << cTotalStubsExpected << " were expected." << RESET;
            }
            // reset readout
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();
        } // stub sel
    }     // configure latencies
}
// what do I want to do
// set-up injection to allow for N clusters
// send N triggers
// re-configure injection to allow for so many clusters
void DataChecker::InjectionTestPS(uint32_t pMaxTriggersToAccept)
{
// prepare root trees
#ifdef __USE_ROOT__
    for(auto cBoard: *fDetectorContainer)
    {
        TString  cName = Form("InjectedData_BeBoard%d", cBoard->getId());
        TObject* cObj  = gROOT->FindObject(cName);
        if(cObj) delete cObj;

        TTree* cTree = new TTree(cName, "Injections");
        cTree->Branch("EventId", &fPSevent.fEventId);
        cTree->Branch("AttemptId", &fPSevent.fAttemptId);
        cTree->Branch("TriggerId", &fPSevent.fTriggerId);         //"TriggerId/I");
        cTree->Branch("InjectionId", &fPSevent.fInjectionId);     //
        cTree->Branch("BxId", &fPSevent.fBxId);                   //"BxId/I");
        cTree->Branch("HybridId", &fPSevent.fHybridId);           //"HybridId/I");
        cTree->Branch("ChipId", &fPSevent.fChipId);               //"ChipId/I");
        cTree->Branch("NSclusters", &fPSevent.fNSclusters);       //"NSclusters/I");
        cTree->Branch("NPclusters", &fPSevent.fNPclusters);       //"NPclusters/I");
        cTree->Branch("StubOffset", &fPSevent.fStubOffset);       //"BxId/I");
        cTree->Branch("LatencyOffset", &fPSevent.fLatencyOffset); //"L1IdCic/I");
        cTree->Branch("LatencyStubs", &fPSevent.fStubLatency);    //"L1IdCic/I");
        this->bookHistogram(cBoard, "InjectedData", cTree);

        cName = Form("ReadData_BeBoard%d", cBoard->getId());
        cObj  = gROOT->FindObject(cName);
        if(cObj) delete cObj;

        cTree = new TTree(cName, "DataMonitoring");
        cTree->Branch("ReadoutSuccess", &fPSevent.fReadoutSuccess);
        cTree->Branch("EventId", &fPSevent.fEventId);
        cTree->Branch("TriggerId", &fPSevent.fTriggerId);   //"TriggerId/I");
        cTree->Branch("BxId", &fPSevent.fBxId);             //"BxId/I");
        cTree->Branch("StubOffset", &fPSevent.fStubOffset); //"BxId/I");
        cTree->Branch("LatencyOffset", &fPSevent.fLatencyOffset);
        cTree->Branch("LatencyMPA", &fPSevent.fMPALatency);     //"L1IdCic/I");
        cTree->Branch("LatencySSA", &fPSevent.fSSALatency);     //"L1IdCic/I");
        cTree->Branch("LatencyStubs", &fPSevent.fStubLatency);  //"L1IdCic/I");
        cTree->Branch("L1IdCic", &fPSevent.fL1IdCic);           //"L1IdCic/I");
        cTree->Branch("L1Status", &fPSevent.fL1Status);         //"L1Status/I");
        cTree->Branch("HybridId", &fPSevent.fHybridId);         //"HybridId/I");
        cTree->Branch("ChipId", &fPSevent.fChipId);             //"ChipId/I");
        cTree->Branch("ChipL1Id", &fPSevent.fChipL1Id);         //"ChipL1Id/I");
        cTree->Branch("NSclusters", &fPSevent.fNSclusters);     //"NSclusters/I");
        cTree->Branch("NPclusters", &fPSevent.fNPclusters);     //"NPclusters/I");
        cTree->Branch("PClusterSize", &fPSevent.fPClusterSize); //,"PClusterSize/I");
        cTree->Branch("SClusterSize", &fPSevent.fSClusterSize); //,"SClusterSize/I");
        cTree->Branch("StubSize", &fPSevent.fStubSize);         //,"StubSize/I");
        cTree->Branch("PackageDelay", &fPSevent.fPackageDelay);
        this->bookHistogram(cBoard, "ReadbackData", cTree);
    }
#endif

    // configure fast command block
    size_t cNrepetitions = 1;
    for(auto cBoard: *fDetectorContainer)
    {
        // stop triggers
        fBeBoardInterface->Stop(cBoard);
        // repeat the sequence N times
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.generic_fcmd.number_of_repetitions", cNrepetitions);
        // make sure fast command duration is 0
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.fast_duration", 0x0);
        // make sure I accept all trgigers
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0);
        // make sure data handshake is disabled
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x00);
    }
    // re-load configuration
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetTriggerFSM();
    // also .. reset the readout
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();

    // configure latencies
    auto   cSetting       = fSettingsMap.find("DelayAfterInjection");
    size_t cCalPulseDelay = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 85;
    cSetting              = fSettingsMap.find("DistributeInjections");
    int  cInjDistrFlag    = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    bool cDistributeInj   = (cInjDistrFlag == 1);
    //
    cSetting            = fSettingsMap.find("Bend");
    int     cBend       = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    uint8_t cStubWindow = (std::fabs(cBend) + 1) * 2; // stub window in half pixels (1)
    // check for stubs
    cSetting            = fSettingsMap.find("CheckForStubs");
    int  cCheckForStubs = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    bool cWStubs        = (cCheckForStubs == 1);
    // mode
    // (0) pixel-strip, (1) strip-strip, (2) pixel-pixel, (3) strip-pixel
    cSetting            = fSettingsMap.find("TestMode");
    uint8_t cMode       = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    cSetting            = fSettingsMap.find("TriggerMultiplicity");
    size_t cTriggerMult = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    cSetting            = fSettingsMap.find("FifoDepth");
    int cFifoDepth      = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 8;
    // configure FIFO depth in SSAs
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                auto&    cCic   = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                uint16_t cValue = fCicInterface->ReadChipReg(cCic, "FE_ENABLE");
                LOG(INFO) << BOLDMAGENTA << "FE_ENABLE register in CIC set to 0x" << std::hex << +cValue << std::dec << RESET;
                for(auto cChip: *cHybrid) // for each chip (makes sense)
                {
                    // for the moment - only written for CBC3
                    if(cChip->getFrontEndType() != FrontEndType::SSA) continue;

                    fReadoutChipInterface->WriteChipReg(cChip, "OutPattern7/FIFOconfig", cFifoDepth);
                }
            }
        }
    }

    size_t cNEventsPerAttempt = 1; // * 30 * 10;
    // random c++
    std::srand(std::time(NULL));
    std::random_device cRndm{};
    std::mt19937       cGen{cRndm()};

    //
    cSetting = fSettingsMap.find("ActiveMPAs");
    // int cLastMPA = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    // configure injections
    cSetting         = fSettingsMap.find("MinPclusters");
    int cMinNpClstrs = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    cSetting         = fSettingsMap.find("MaxPclusters");
    int cMaxNpClstrs = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    cSetting         = fSettingsMap.find("MinSclusters");
    int cMinNsClstrs = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    cSetting         = fSettingsMap.find("MaxSclusters");
    int cMaxNsClstrs = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    cSetting         = fSettingsMap.find("MaxStubs");
    int cMaxNstubs   = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    // cluster distributions
    std::uniform_int_distribution<int> cFlatDistPxlCltrs(cMinNpClstrs, cMaxNpClstrs);
    std::uniform_int_distribution<int> cFlatDistStrpCltrs(cMinNsClstrs, cMaxNsClstrs);
    // LOG(INFO) << BOLDMAGENTA << "P-cluster distribution : " << cMinNpClstrs << " to " << cMaxNpClstrs << RESET;
    // LOG(INFO) << BOLDMAGENTA << "S-cluster distribution : " << cMinNsClstrs << " to " << cMaxNsClstrs << RESET;
    // int  cMaxStubSel    = (cDistributeInj) ? 8 : 1;
    // auto cStubOffset = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->getStubOffset();
    cSetting                   = fSettingsMap.find("ScanL1Latency");
    uint8_t cScanL1Latency     = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    int     cOptimalOffset     = -1 + (cTriggerMult > 1);
    int     cL1MinOffset       = (cScanL1Latency == 1) ? cOptimalOffset - 2 : cOptimalOffset;
    int     cL1MaxOffset       = (cScanL1Latency == 1) ? cOptimalOffset + cTriggerMult + 2 : cL1MinOffset + 1;
    cSetting                   = fSettingsMap.find("ScanStubLatency");
    uint8_t cScanStubLatency   = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    int     cOptimalStubOffset = 0;
    int     cStubMinOffset     = (cScanStubLatency == 1 && cWStubs) ? cOptimalStubOffset - 5 : cOptimalStubOffset;
    int     cStubMaxOffset     = (cScanStubLatency == 1 && cWStubs) ? cOptimalStubOffset + 5 : cOptimalStubOffset + 1;
    LOG(INFO) << BOLDMAGENTA << "DataChecker::InjectionTestPS TestMode is " << +cMode << RESET;
    for(int cLatencyOffset = cL1MinOffset; cLatencyOffset < cL1MaxOffset; cLatencyOffset++)
    {
        for(int cStubOffset = cStubMinOffset; cStubOffset < cStubMaxOffset; cStubOffset++)
        {
            LOG(INFO) << BOLDMAGENTA << "L1 Latency offset is " << +cLatencyOffset << "\t...Stub Latency offset is " << +cStubOffset << RESET;
            // configure latencies
            DetectorDataContainer cLatencyPerFE;
            ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, cLatencyPerFE);
            DetectorDataContainer cStubLatencyPerBoard;
            ContainerFactory::copyAndInitBoard<uint16_t>(*fDetectorContainer, cStubLatencyPerBoard);
            DetectorDataContainer cPackageDelayPerBoard;
            ContainerFactory::copyAndInitBoard<uint16_t>(*fDetectorContainer, cPackageDelayPerBoard);

            for(auto cBoard: *fDetectorContainer)
            {
                uint16_t cDelay        = cCalPulseDelay;
                int      cReTimeValue  = -1;
                auto&    cFeLatency    = cLatencyPerFE.at(cBoard->getIndex());
                auto&    cBrdLatency   = cStubLatencyPerBoard.at(cBoard->getIndex());
                auto&    cBrdDelay     = cPackageDelayPerBoard.at(cBoard->getIndex());
                auto&    cPackageDelay = cBrdDelay->getSummary<uint16_t>();
                cPackageDelay          = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
                auto cInitStubOffset   = cBoard->getStubOffset();
                for(auto cOpticalReadout: *cBoard)
                {
                    auto& cFeLatencyOG = cFeLatency->at(cOpticalReadout->getIndex());
                    for(auto cHybrid: *cOpticalReadout)
                    {
                        auto& cFeLatencyHybrid = cFeLatencyOG->at(cHybrid->getIndex());
                        for(auto cChip: *cHybrid) // for each chip (makes sense)
                        {
                            auto& cFeLatencyChip = cFeLatencyHybrid->at(cChip->getIndex());
                            auto& cFeLatencySmry = cFeLatencyChip->getSummary<uint16_t>();
                            if(cChip->getFrontEndType() != FrontEndType::SSA)
                            {
                                cFeLatencySmry = (cDelay) + cLatencyOffset;
                                if(cChip->getFrontEndType() == FrontEndType::MPA && cReTimeValue < 0) { cReTimeValue = fReadoutChipInterface->ReadChipReg(cChip, "RetimePix"); }
                            }
                            else // SSA needs an additional clock cycle of delay
                            {
                                cFeLatencySmry = (cDelay) + (cLatencyOffset - 1);
                            }
                            if(cDistributeInj) cFeLatencySmry = cFeLatencySmry + cChip->getId();
                            fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cFeLatencySmry);
                        } // chip
                    }     // hybrid
                }         // module
                if(cWStubs)
                {
                    auto& cStubLatency = cBrdLatency->getSummary<uint16_t>();
                    cStubLatency       = cDelay + cLatencyOffset - (cInitStubOffset + cStubOffset + cReTimeValue);
                    // cStubLatency       = cStubOffset;
                    fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
                    LOG(INFO) << BOLDMAGENTA << "\t\t.. Package delay set to " << +cPackageDelay << "... stub latency set to " << +cStubLatency << RESET;
                }
            } // boards

            size_t cAttempt  = 0;
            bool   cFinished = false;
            // uint32_t cMaxTriggers=pMaxTriggersToAccept;
            PSEvents              cInjectedPSevents;
            DetectorDataContainer cTriggersPerBoard;
            ContainerFactory::copyAndInitBoard<uint32_t>(*fDetectorContainer, cTriggersPerBoard);
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cTrgCntBrd = cTriggersPerBoard.at(cBoard->getIndex());
                auto& cNtriggers = cTrgCntBrd->getSummary<uint32_t>();
                cNtriggers       = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
            }
            // zero event id
            // and zero trigger id
            fPSevent.fEventId      = 0;
            fPSevent.fTriggerId    = 0;
            size_t cInjectedEvents = 0;

            std::map<int, std::vector<Injection>> cInjectionScheme;
            cInjectionScheme.clear();
            do {
                cInjectionScheme[cAttempt] = this->GeneratePSInjections(cMaxNstubs);
                LOG(INFO) << BOLDMAGENTA << "Injection#" << +cAttempt << "\t\t..." << RESET;
                std::sort(std::begin(cInjectionScheme[cAttempt]), std::end(cInjectionScheme[cAttempt]), [](Injection a, Injection b) { return a.fRow < b.fRow; });
                // std::sort(std::begin(cInjectionScheme[cAttempt]), std::end(cInjectionScheme[cAttempt]), [](Injection a, Injection b) { return a.fColumn < b.fColumn; });

                // LOG(INFO) << BOLDMAGENTA << "Attempt#" << +cAttempt << RESET;
                // uint8_t cMaxClustersPerMPA = cFlatDistPxlCltrs(cGen);
                // uint8_t cMaxClustersPerSSA = cFlatDistStrpCltrs(cGen);
                // if(cAttempt % 10 == 0)
                //     LOG(INFO) << BOLDMAGENTA << "Attempt#" << +cAttempt << " -- injecting " << +cMaxClustersPerMPA << " pixel clusters "
                //               << " and " << +cMaxClustersPerSSA << " strip clusters." << RESET;
                // if(cMaxClustersPerSSA == 20) LOG(INFO) << BOLDRED << "\t\t... Attempt#" << +cAttempt << " 20 S-clusters ... " << RESET;
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                for(auto cBoard: *fDetectorContainer) { fBeBoardInterface->ChipReSync(cBoard); }
                std::this_thread::sleep_for(std::chrono::microseconds(500));

                // use generic fast commands to inject N times
                // generate fast commands
                this->FastCommandInjections(cNEventsPerAttempt);
                // std::vector<float> cDifferences(fTriggeredBxs.size());
                // std::adjacent_difference(fTriggeredBxs.begin(), fTriggeredBxs.end(), cDifferences.begin());
                // cDifferences.erase(cDifferences.begin(), cDifferences.begin() + 1);
                // cDifferences.erase(cDifferences.end() - 1, cDifferences.end());
                // auto cStats = getStats(cDifferences);
                // if( cAttempt%1 == 0 )
                //     LOG(INFO) << BOLDMAGENTA << "\t...Generic fast command block used to send " << +fNInjectedTriggers << " triggers have " << fTriggeredBxs.size() << " Bx with a trigger "
                //               << " <Trigger Seperation> " << +cStats.first << " Bx."
                //               << " RMS Trigger Separation " << +cStats.second << " Bx." << RESET;
                // std::vector<int> cIds = GenerateIds();
                // DetectorDataContainer cInjections;
                // ContainerFactory::copyAndInitHybrid<std::vector<Injection>>(*fDetectorContainer, cInjections);
                for(auto cBoard: *fDetectorContainer)
                {
                    // auto& cInjectionsBrd = cInjections.at(cBoard->getIndex());
                    for(auto cOpticalReadout: *cBoard)
                    {
                        // auto& cInjectionsOG = cInjectionsBrd->at(cOpticalReadout->getIndex());
                        for(auto cHybrid: *cOpticalReadout)
                        {
                            // auto& cInjectionsHybrid = cInjectionsOG->at(cHybrid->getIndex());
                            // auto& cInjs             = cInjectionsHybrid->getSummary<std::vector<Injection>>();
                            std::vector<uint8_t> cIds(0);
                            for(auto cChip: *cHybrid)
                            {
                                if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                                cIds.push_back(cChip->getId() % 8);
                            }

                            for(auto cId: cIds)
                            {
                                for(auto cChip: *cHybrid)
                                {
                                    if(cChip->getFrontEndType() == FrontEndType::MPA) continue;
                                    if(cChip->getId() % 8 != cId) continue;

                                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                                    fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x01);
                                    for(auto cInjection: cInjectionScheme[cAttempt]) { fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cInjection.fRow), 0x9); }
                                }
                                for(auto cChip: *cHybrid)
                                {
                                    if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                                    if(cChip->getId() % 8 != cId) continue;

                                    fReadoutChipInterface->WriteChipReg(cChip, "StubMode", cMode);
                                    fReadoutChipInterface->WriteChipReg(cChip, "StubWindow", cStubWindow);
                                    (static_cast<PSInterface*>(fReadoutChipInterface))->WriteChipReg(cChip, "DigitalSync", 0x00);
                                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                                    uint8_t                cPattern = cDistributeInj ? (1 << (7 - cChip->getId())) : (0x1 << 0);
                                    std::vector<Injection> cMPAInj;
                                    cMPAInj.clear();
                                    for(size_t cInjIndx = 0; cInjIndx < cInjectionScheme[cAttempt].size(); cInjIndx++)
                                    {
                                        if(cInjIndx == cAttempt % cMaxNstubs) cMPAInj.push_back(cInjectionScheme[cAttempt].at(cInjIndx));
                                    }
                                    (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, cMPAInj, cPattern);
                                }
                            }
                        } // hybrid
                    }     // module
                }         // boards - injections

                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFCMDBram(fFastCommands);
                // LOG (INFO) << BOLDMAGENTA << "Fast command block used to send " << +fTriggeredBxs.size() << " triggers." << RESET;
                cInjectedEvents += fTriggeredBxs.size();
                cFinished = true;
                for(auto cBoard: *fDetectorContainer)
                {
                    // start generic  - ctrl signal high
                    fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x1);
                    // stop generic  - ctrl signal low
                    fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x0);
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    auto&  cTrgCntBrd     = cTriggersPerBoard.at(cBoard->getIndex());
                    auto&  cNtriggersInit = cTrgCntBrd->getSummary<uint32_t>();
                    auto   cNtriggers     = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
                    size_t cIter          = 0;
                    do {
                        std::this_thread::sleep_for(std::chrono::microseconds(100));
                        // auto cNWords    = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
                        cNtriggers = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
                        // LOG(INFO) << BOLDMAGENTA << "\t\t.. Iter#" << +cIter << " trigger in counter is " << cNtriggers << " .. expect to see " << +fTriggeredBxs.size() << RESET;
                        cIter++;
                    } while((cNtriggers - cNtriggersInit) < fTriggeredBxs.size());
                }
                cFinished = (cInjectedEvents >= pMaxTriggersToAccept);
                cAttempt++;
            } while(!cFinished); // inject N clusters for M triggers

            // now readout all data
            for(auto cBoard: *fDetectorContainer)
            {
                //
                auto& cBrdLatency  = cStubLatencyPerBoard.at(cBoard->getIndex());
                auto& cStubLatency = cBrdLatency->getSummary<uint16_t>();
                //
                auto& cBrdDelay     = cPackageDelayPerBoard.at(cBoard->getIndex());
                auto& cPackageDelay = cBrdDelay->getSummary<uint16_t>();
                //
                // auto&                 cFeLatency = cLatencyPerFE.at(cBoard->getIndex());
                std::vector<uint32_t> cData(0);
                uint32_t              cNevents = ReadData(cBoard, cData, false);
                DecodeData(cBoard, cData, cNevents, fBeBoardInterface->getBoardType(cBoard));
                const std::vector<Event*>& cPh2Events     = GetEvents();
                size_t                     cEventsReadout = cPh2Events.size();
                LOG(INFO) << BOLDMAGENTA << "Readout " << cEventsReadout << " events from uDTC.. injected " << cInjectedEvents << RESET;
                // fPSevent.fReadoutSuccess = cPh2Events.size() == cInjectedPSevents.size();
                size_t cEventCounter    = 0;
                fPSevent.fStubLatency   = cStubLatency;
                fPSevent.fPackageDelay  = cPackageDelay;
                fPSevent.fStubOffset    = cStubOffset;
                fPSevent.fLatencyOffset = cLatencyOffset;
                //
                cSetting                 = fSettingsMap.find("TriggerMultiplicity");
                size_t cTriggerMult      = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
                size_t cInjectionCounter = 0;
                auto   cEventIter        = cPh2Events.begin();
                size_t cMatchCounter     = 0;
                do {
                    auto cInjections = cInjectionScheme[cInjectionCounter];
                    LOG(INFO) << BOLDMAGENTA << "Checking result for injection#" << cInjectionCounter << RESET;
                    // for( auto cInj : cInjections)
                    // {
                    //     LOG (INFO) << BOLDYELLOW << "\t\t.. injecting in row "
                    //         << +cInj.fRow
                    //         << " columnn "
                    //         << +cInj.fColumn
                    //         << RESET;
                    // }
                    std::vector<uint8_t> cFullMatches(cTriggerMult, 0);
                    for(size_t cTriggerId = 0; cTriggerId < cTriggerMult; cTriggerId++)
                    {
                        if(cEventIter >= cPh2Events.end()) continue;

                        for(auto cOpticalGroup: *cBoard)
                        {
                            for(auto cHybrid: *cOpticalGroup)
                            {
                                fPSevent.fL1IdCic      = static_cast<D19cCic2Event*>(*cEventIter)->L1Id(cHybrid->getId(), 42);
                                fPSevent.fL1Status     = static_cast<D19cCic2Event*>(*cEventIter)->L1Status(cHybrid->getId());
                                fPSevent.fBxId         = (*cEventIter)->BxId(cHybrid->getId());
                                fPSevent.fHybridId     = cHybrid->getId();
                                fPSevent.fPClusterSize = static_cast<D19cCic2Event*>(*cEventIter)->GetNPixelClusters(cHybrid->getId());
                                fPSevent.fSClusterSize = static_cast<D19cCic2Event*>(*cEventIter)->GetNStripClusters(cHybrid->getId());
                                // size_t cNStubsExpected = 1*cHybrid->size();
                                // size_t cNPclustersExpected = 1*cHybrid->size();
                                // size_t cNSclustersExpected = cInjections.size()*cHybrid->size();
                                size_t cNStubs   = 0;
                                size_t cNmatches = 0;
                                size_t cNmpas    = 0;
                                for(auto cChip: *cHybrid)
                                {
                                    if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
                                    fPSevent.fChipL1Id   = static_cast<D19cCic2Event*>(*cEventIter)->L1Id(cHybrid->getId(), cChip->getId());
                                    auto cPclstrs        = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(cHybrid->getId(), cChip->getId());
                                    auto cSclstrs        = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(cHybrid->getId(), cChip->getId());
                                    auto cStubs          = static_cast<D19cCic2Event*>(*cEventIter)->StubVector(cHybrid->getId(), cChip->getId());
                                    fPSevent.fNPclusters = cPclstrs.size();
                                    fPSevent.fNSclusters = cSclstrs.size();
                                    fPSevent.fStubSize   = cStubs.size();
                                    cNStubs += cStubs.size();
                                    bool cMatched = false;
                                    if(fPSevent.fNPclusters > 0 || fPSevent.fNSclusters > 0)
                                    {
                                        cMatched = (fPSevent.fNPclusters == 1 && fPSevent.fNSclusters == cInjections.size() && cStubs.size() == fPSevent.fNPclusters);
                                        // if( cMatched )
                                        //     LOG (INFO) << BOLDGREEN << "Trigger#" << +cTriggerId << " in a burst of " << (1+ cTriggerMult)
                                        //         << " MPA" << +cChip->getId() << " found " << fPSevent.fNPclusters << " P clusters, " << fPSevent.fNSclusters
                                        //         << " S clusters, and "
                                        //         << fPSevent.fStubSize
                                        //         << " stubs"
                                        //         << RESET;
                                        // else
                                        //     LOG (INFO) << BOLDRED << "Trigger#" << +cTriggerId << " in a burst of " << (1+ cTriggerMult)
                                        //         << " MPA" << +cChip->getId() << " found " << fPSevent.fNPclusters << " P clusters, " << fPSevent.fNSclusters
                                        //         << " S clusters, and "
                                        //         << fPSevent.fStubSize
                                        //         << " stubs"
                                        //         << RESET;

                                        // for(auto cCluster : cPclstrs )
                                        // {
                                        //     LOG (INFO) << BOLDYELLOW << "P-cluster in row " << +cCluster.fAddress
                                        //         << " column " << +cCluster.fZpos << " width is " << +cCluster.fWidth
                                        //         << RESET;
                                        // }
                                        // for(auto cCluster : cSclstrs )
                                        // {
                                        //     LOG (INFO) << BOLDCYAN << "S-cluster in row " << +cCluster.fAddress
                                        //         << " column " << (0) << " width is " << +cCluster.fWidth
                                        //         << RESET;
                                        // }
                                        if(fPSevent.fStubSize != 0)
                                        {
                                            std::sort(std::begin(cStubs), std::end(cStubs), [](Stub a, Stub b) { return a.getRow() < b.getRow(); });
                                            size_t cStubCntr = 0;
                                            for(auto cStub: cStubs)
                                            {
                                                cMatched = (cStub.getPosition() == cPclstrs[0].fAddress * 2 && cStub.getRow() == cPclstrs[0].fZpos);
                                                // if( cMatched )
                                                //     LOG (INFO) << BOLDGREEN << "\t\tStub#" << +cStubCntr
                                                //         << " Position " << +cStub.getPosition() << " - Row " << +cStub.getRow() << " - Bend " << +cStub.getBend() << RESET;
                                                // else
                                                //     LOG (INFO) << BOLDRED << "\t\tStub#" << +cStubCntr
                                                //         << " Position " << +cStub.getPosition() << " - Row " << +cStub.getRow() << " - Bend " << +cStub.getBend() << RESET;
                                                cStubCntr++;
                                            }
                                        }
                                    }
                                    cNmpas++;
                                    cNmatches += (cMatched) ? 1 : 0;
                                } // chip
                                cFullMatches.at(cTriggerId) = (cNmatches == cNmpas) ? 1 : 0;
                            } // hybrid
                        }     // OG
                        cEventIter++;
                        cEventCounter++;
                    } // trigger loop
                    cMatchCounter += std::accumulate(cFullMatches.begin(), cFullMatches.end(), 0);
                    cInjectionCounter++;
                } while(cEventIter < cPh2Events.end());
                LOG(INFO) << BOLDMAGENTA << cMatchCounter << " out of " << cInjectionCounter << " injections match... " << RESET;
            }
            // reset readout
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();
        } // stub sel
    }     // configure latencies
}
void DataChecker::ReadDataTestPS(BeBoard* pBoard, uint32_t pNevents)
{
    LOG(INFO) << BOLDMAGENTA << "DataChecker::ReadDataTestPS with " << pNevents << " evnts." << RESET;
    std::vector<uint32_t> cCompleteData(0);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNevents);
    fBeBoardInterface->Start(pBoard);
    bool cAllTriggersSent = false;
    do {
        auto cNtriggersRxd = fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        if((int)(std::floor((float)cNtriggersRxd / 10.)) % (pNevents / 100) == 0) LOG(INFO) << BOLDMAGENTA << "\t\t..." << cNtriggersRxd << " triggers received..." << RESET;
        cAllTriggersSent = cNtriggersRxd >= pNevents;
    } while(!cAllTriggersSent);
    LOG(INFO) << BOLDBLUE << "Stopping triggers..." << RESET;
    fBeBoardInterface->Stop(pBoard);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    auto                  cNtriggersSent = fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
    std::vector<uint32_t> cData(0);
    uint32_t              cNevents = ReadData(pBoard, cData, false);
    std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
    DecodeData(pBoard, cCompleteData, cNevents, fBeBoardInterface->getBoardType(pBoard));
    const std::vector<Event*>& cPh2Events     = GetEvents();
    size_t                     cEventsReadout = cPh2Events.size();
#ifdef __USE_ROOT__
    TString  cName = Form("ReadData_Monitoring_BeBoard%d", pBoard->getId());
    TObject* cObj  = gROOT->FindObject(cName);
    if(cObj) delete cObj;

    TTree* cTree = new TTree(cName, "DataMonitoring");
    cTree->Branch("ReadoutSuccess", &fPSevent.fReadoutSuccess);
    cTree->Branch("AttemptId", &fPSevent.fAttemptId);
    cTree->Branch("EventId", &fPSevent.fEventId);
    cTree->Branch("TriggerId", &fPSevent.fTriggerId);       //"TriggerId/I");
    cTree->Branch("BxId", &fPSevent.fBxId);                 //"BxId/I");
    cTree->Branch("L1IdCic", &fPSevent.fL1IdCic);           //"L1IdCic/I");
    cTree->Branch("L1Status", &fPSevent.fL1Status);         //"L1Status/I");
    cTree->Branch("HybridId", &fPSevent.fHybridId);         //"HybridId/I");
    cTree->Branch("ChipId", &fPSevent.fChipId);             //"ChipId/I");
    cTree->Branch("ChipL1Id", &fPSevent.fChipL1Id);         //"ChipL1Id/I");
    cTree->Branch("NSclusters", &fPSevent.fNSclusters);     //"NSclusters/I");
    cTree->Branch("NPclusters", &fPSevent.fNPclusters);     //"NPclusters/I");
    cTree->Branch("PClusterSize", &fPSevent.fPClusterSize); //,"PClusterSize/I");
    cTree->Branch("SClusterSize", &fPSevent.fSClusterSize); //,"SClusterSize/I");
    cTree->Branch("StubSize", &fPSevent.fStubSize);         //,"StubSize/I");
    this->bookHistogram(pBoard, "DataMonitoring", cTree);

    auto cNtriggersRxd       = fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
    fPSevent.fReadoutSuccess = cPh2Events.size() == cNtriggersRxd;
    size_t cEventCounter     = 0;
    for(auto& cEvent: cPh2Events)
    {
        fPSevent.fEventId   = cEvent->GetEventCount();
        fPSevent.fTriggerId = cEvent->GetExternalTriggerId();
        if(cEventCounter % 1000 == 0) LOG(INFO) << BOLDGREEN << "Processing event#" << +cEventCounter << RESET;
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                fPSevent.fL1IdCic      = static_cast<D19cCic2Event*>(cEvent)->L1Id(cHybrid->getId(), 42);
                fPSevent.fL1Status     = static_cast<D19cCic2Event*>(cEvent)->L1Status(cHybrid->getId());
                fPSevent.fBxId         = cEvent->BxId(cHybrid->getId());
                fPSevent.fHybridId     = cHybrid->getId();
                fPSevent.fPClusterSize = static_cast<D19cCic2Event*>(cEvent)->GetNPixelClusters(cHybrid->getId());
                fPSevent.fSClusterSize = static_cast<D19cCic2Event*>(cEvent)->GetNStripClusters(cHybrid->getId());
                // printPSevent();
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
                    fPSevent.fChipId     = cChip->getId();
                    fPSevent.fChipL1Id   = static_cast<D19cCic2Event*>(cEvent)->L1Id(cHybrid->getId(), cChip->getId());
                    auto Pclus           = static_cast<D19cCic2Event*>(cEvent)->GetPixelClusters(cHybrid->getId(), cChip->getId());
                    auto Sclus           = static_cast<D19cCic2Event*>(cEvent)->GetStripClusters(cHybrid->getId(), cChip->getId());
                    fPSevent.fNPclusters = Pclus.size();
                    fPSevent.fNSclusters = Sclus.size();
                    fPSevent.fStubSize   = static_cast<D19cCic2Event*>(cEvent)->StubVector(cHybrid->getId(), cChip->getId()).size();
                    cTree->Fill();
                }
            } // hybrid
        }     // optical group
        cEventCounter++;
    }
#endif
    LOG(INFO) << BOLDMAGENTA << "DataChecker::ReadDataTestPS Has readout " << +cEventsReadout << " events from " << +cNtriggersSent << " sent." << RESET;
}
void DataChecker::matchEvents(BeBoard* pBoard, std::vector<uint8_t> pChipIds, std::pair<uint8_t, int> pExpectedStub)
{
    // LOG (INFO) << BOLDMAGENTA << "Let's see what's on the stub lines" << RESET;
    // (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->StubDebug(true,5);

    // get trigger multiplicity from register
    size_t cTriggerMult = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");

    // get number of events from xml
    auto   cSetting        = fSettingsMap.find("Nevents");
    size_t cEventsPerPoint = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;

    uint8_t cSeed = pExpectedStub.first;
    int     cBend = pExpectedStub.second;

    auto& cThisHitCheckContainer  = fHitCheckContainer.at(pBoard->getIndex());
    auto& cThisStubCheckContainer = fStubCheckContainer.at(pBoard->getIndex());

    const std::vector<Event*>& cEvents = this->GetEvents();
    LOG(DEBUG) << BOLDMAGENTA << "Read back " << +cEvents.size() << " events from board." << RESET;

    for(auto cOpticalGroup: *pBoard)
    {
        auto& cThisOpticalGroupHitCheck  = cThisHitCheckContainer->at(cOpticalGroup->getIndex());
        auto& cThisOpticalGroupStubCheck = cThisStubCheckContainer->at(cOpticalGroup->getIndex());

        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cHybridHitCheck  = cThisOpticalGroupHitCheck->at(cHybrid->getIndex());
            auto& cHybridStubCheck = cThisOpticalGroupStubCheck->at(cHybrid->getIndex());

            auto  cHybridId     = cHybrid->getId();
            TH2D* cMatchedStubs = static_cast<TH2D*>(getHist(cHybrid, "MatchedStubs"));
            TH2D* cAllStubs     = static_cast<TH2D*>(getHist(cHybrid, "Stubs"));
            TH2D* cMissedHits   = static_cast<TH2D*>(getHist(cHybrid, "MissedHits"));

            // matching
            for(auto cChip: *cHybrid)
            {
                ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                auto         cChipId = cChip->getId();
                if(std::find(pChipIds.begin(), pChipIds.end(), cChipId) == pChipIds.end()) continue;

                TH2D*       cAllHits        = static_cast<TH2D*>(getHist(cChip, "Hits_perFe"));
                TH2D*       cMatchedHits    = static_cast<TH2D*>(getHist(cChip, "MatchedHits_perFe"));
                TProfile2D* cMatchedHitsEye = static_cast<TProfile2D*>(getHist(cChip, "MatchedHits_eye"));

                TH1D* cFlaggedEvents = static_cast<TH1D*>(getHist(cChip, "FlaggedEvents"));

                // container for this chip
                auto& cReadoutChipHitCheck  = cHybridHitCheck->at(cChip->getIndex());
                auto& cReadoutChipStubCheck = cHybridStubCheck->at(cChip->getIndex());

                std::vector<uint8_t> cBendLUT = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theChip);
                // each bend code is stored in this vector - bend encoding start at -7 strips, increments by 0.5 strips
                uint8_t              cBendCode     = cBendLUT[(cBend / 2. - (-7.0)) / 0.5];
                std::vector<uint8_t> cExpectedHits = static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern(theChip, cSeed, cBend);
                LOG(INFO) << BOLDMAGENTA << "Injected a stub with seed " << +cSeed << " with bend " << +cBend << RESET;
                for(auto cHitExpected: cExpectedHits) LOG(INFO) << BOLDMAGENTA << "\t.. expect a hit in channel " << +cHitExpected << RESET;
                auto cEventIterator = cEvents.begin();
                LOG(DEBUG) << BOLDMAGENTA << "CBC" << +cChip->getId() << RESET;
                for(size_t cEventIndex = 0; cEventIndex < cEventsPerPoint; cEventIndex++) // for each event
                {
                    uint32_t cPipeline_first = 0;
                    uint32_t cBxId_first     = 0;
                    bool     cMissedEvent    = false;
                    LOG(INFO) << BOLDMAGENTA << "Event " << +cEventIndex << RESET;
                    for(size_t cTriggerIndex = 0; cTriggerIndex <= cTriggerMult; cTriggerIndex++) // cTriggerMult consecutive triggers were sent
                    {
                        auto     cEvent    = *cEventIterator;
                        auto     cBxId     = cEvent->BxId(cHybrid->getId());
                        uint32_t cPipeline = cEvent->PipelineAddress(cHybridId, cChipId);
                        cBxId_first        = (cTriggerIndex == 0) ? cBxId : cBxId_first;
                        cPipeline_first    = (cTriggerIndex == 0) ? cPipeline : cPipeline_first;
                        LOG(DEBUG) << BOLDBLUE << "Chip" << +cChipId << " Trigger number " << +cTriggerIndex << " : Pipeline address " << +cPipeline << " -- bx id is " << +cBxId_first << RESET;

                        // hits
                        auto cHits = cEvent->GetHits(cHybridId, cChipId);
                        cAllHits->Fill(cTriggerIndex, cPipeline, cHits.size());
                        for(auto cHit: cHits) { LOG(INFO) << BOLDMAGENTA << "\t... hit found in channel " << +cHit << " of readout chip" << +cChipId << RESET; }
                        size_t cMatched = 0;
                        for(auto cExpectedHit: cExpectedHits)
                        {
                            bool cMatchFound = std::find(cHits.begin(), cHits.end(), cExpectedHit) != cHits.end();
                            cMatched += cMatchFound;
                            cMatchedHits->Fill(cTriggerIndex, cPipeline, static_cast<int>(cMatchFound));
                            cMatchedHitsEye->Fill(fPhaseTap, cTriggerIndex, static_cast<int>(cMatchFound));
                        }
                        cMissedHits->Fill((int)(fAttempt), cChipId, cExpectedHits.size() - cMatched);
                        if(cMatched == cExpectedHits.size())
                        {
                            auto& cOcc = cReadoutChipHitCheck->getSummary<int>();
                            cOcc += static_cast<int>(cMatched == cExpectedHits.size());
                        }
                        else
                        {
                            cMissedEvent = true;
                        }

                        // stubs
                        auto cStubs         = cEvent->StubVector(cHybridId, cChipId);
                        int  cNmatchedStubs = 0;
                        for(auto cStub: cStubs)
                        {
                            LOG(INFO) << BOLDMAGENTA << "\t... stub seed " << +cStub.getPosition() << " --- bend code of " << +cStub.getBend() << " expect seed " << +cSeed << " and bend code "
                                      << +cBendCode << RESET;
                            bool  cMatchFound = (cStub.getPosition() == cSeed && cStub.getBend() == cBendCode);
                            auto& cOcc        = cReadoutChipStubCheck->getSummary<int>();
                            cOcc += static_cast<int>(cMatchFound);
                            cNmatchedStubs += static_cast<int>(cMatchFound);
                            cMatchedStubs->Fill(cTriggerIndex, cBxId, static_cast<int>(cMatchFound));
                        }
                        cAllStubs->Fill(cTriggerIndex, cBxId, cStubs.size());
                        LOG(INFO) << BOLDMAGENTA "Chip" << +cChipId << "\t\t...Trigger number " << +cTriggerIndex << " : Pipeline address " << +cPipeline << " :" << +cMatched
                                  << " matched hits found [ " << +cHits.size() << " in total]. " << +cNmatchedStubs << " matched stubs." << RESET;
                        cEventIterator++;
                    }
                    if(cMissedEvent)
                    {
                        int cDistanceToLast = fEventCounter - fMissedEvent;
                        LOG(DEBUG) << BOLDMAGENTA << "Event with a missing hit " << fEventCounter << " -- distance to last missed event is " << cDistanceToLast << RESET;
                        cFlaggedEvents->Fill(cDistanceToLast);
                        fMissedEvent = fEventCounter;
                    }
                    fEventCounter++;
                }
            }
        }

        // noise hits
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                ReadoutChip* theChip    = static_cast<ReadoutChip*>(cChip);
                auto         cChipId    = cChip->getId();
                TProfile*    cNoiseHits = static_cast<TProfile*>(getHist(cChip, "NoiseHits"));

                std::vector<uint8_t> cBendLUT = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theChip);
                // each bend code is stored in this vector - bend encoding start at -7 strips, increments by 0.5 strips
                std::vector<uint8_t> cExpectedHits = static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern(theChip, cSeed, cBend);
                auto                 cHybridId     = cHybrid->getId();

                auto cEventIterator = cEvents.begin();
                LOG(DEBUG) << BOLDMAGENTA << "CBC" << +cChip->getId() << RESET;
                for(size_t cEventIndex = 0; cEventIndex < cEventsPerPoint; cEventIndex++) // for each event
                {
                    uint32_t cPipeline_first = 0;
                    uint32_t cBxId_first     = 0;
                    LOG(DEBUG) << BOLDMAGENTA << "\t..Event" << +cEventIndex << RESET;

                    for(size_t cTriggerIndex = 0; cTriggerIndex <= cTriggerMult; cTriggerIndex++) // cTriggerMult consecutive triggers were sent
                    {
                        auto     cEvent    = *cEventIterator;
                        auto     cBxId     = cEvent->BxId(cHybrid->getId());
                        uint32_t cPipeline = cEvent->PipelineAddress(cHybridId, cChipId);
                        cBxId_first        = (cTriggerIndex == 0) ? cBxId : cBxId_first;
                        cPipeline_first    = (cTriggerIndex == 0) ? cPipeline : cPipeline_first;

                        // hits
                        auto cHits = cEvent->GetHits(cHybridId, cChipId);
                        for(int cChannel = 0; cChannel < NCHANNELS; cChannel++)
                        {
                            bool cHitFound = std::find(cHits.begin(), cHits.end(), cChannel) != cHits.end();
                            // if( cHitFound && std::find(pChipIds.begin(), pChipIds.end(), cChipId) == pChipIds.end() )
                            //    LOG (INFO) << BOLDMAGENTA << "\t... noise hit found in channel " << +cChannel << " of
                            //    readout chip" << +cChipId << RESET;
                            cNoiseHits->Fill(cChannel, cHitFound);
                        }
                        // for( auto cHit : cHits )
                        // {
                        //     bool cExpected = std::find(  cExpectedHits.begin(), cExpectedHits.end(), cHit) !=
                        //     cExpectedHits.end(); if( std::find(pChipIds.begin(), pChipIds.end(), cChipId) ==
                        //     pChipIds.end() )
                        //         cNoiseHits->Fill(cHit);
                        //     else
                        //     {
                        //         if(!cExpected)
                        //         {
                        //             // this is not going to work..I've masked out everyhing else!
                        //             cNoiseHits->Fill(cHit);
                        //         }
                        //     }
                        // }
                        cEventIterator++;
                    }
                }
            }
        }
    }
}
void DataChecker::AsyncTest()
{
    uint8_t           cSweepThreshold = this->findValueInSettings("AsyncSweepTh");
    uint8_t           cThreshold      = this->findValueInSettings("AsyncThreshold");
    uint8_t           cThresholdStart = (cSweepThreshold == 0) ? cThreshold : 0;
    uint8_t           cThresholdStop  = (cSweepThreshold == 0) ? cThreshold + 5 : 200;
    std::stringstream outp;
    for(auto cBoard: *fDetectorContainer)
    {
        auto cEventType = cBoard->getEventType();
        // will only decode those events
        // check what kind of FEs I have and based on that .. set the event type
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::MPA)
                    {
                        cBoard->setEventType(EventType::PSAS);
                        // cBoard->setEventType(EventType::MPAAS);
                    }
                    else
                        cBoard->setEventType(EventType::SSAAS);
                }
            }
        }

        for(uint8_t cThreshold = cThresholdStart; cThreshold < cThresholdStop; cThreshold += 5)
        {
            LOG(INFO) << BOLDBLUE << "Threshold set to " << +cThreshold << RESET;
            // set thresholds
            // and configure injection
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 1);
                        fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
                        fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", this->findValueInSettings("AsyncCalDac"));
                    }
                }
            }

            // read counters
            BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
            LOG(INFO) << BOLDRED << "Reading counters .. " << RESET;
            this->ReadNEvents(theBoard, this->findValueInSettings("Nevents"));
            const std::vector<Event*>& cEvents = this->GetEvents();
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        for(auto cEvent: cEvents)
                        {
                            auto cHits = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                            for(uint8_t cChnl = 0; cChnl < 5; cChnl++)
                            {
                                if(cChip->getFrontEndType() == FrontEndType::SSA)
                                    LOG(INFO) << BOLDBLUE << "Counter value Strip#" << +cChnl << " is " << cHits[cChnl] << RESET;
                                else
                                    LOG(INFO) << BOLDBLUE << "Counter value Pix#" << +cChnl << " is " << cHits[cChnl] << RESET;
                            }
                        }
                    }
                }
            }
            LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadNEvents" << RESET;
        }
        cBoard->setEventType(cEventType);
        // const std::vector<Event*>& cEvents = this->GetEvents ( theBoard );
    }
    LOG(INFO) << BOLDBLUE << "Done!" << RESET;
}
void DataChecker::ReadDataTest()
{
    std::stringstream outp;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    // ReadoutChip *cReadoutChip = static_cast<ReadoutChip*>(cChip);
                    if(cChip->getId() == 0)
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(cChip, {10, 244}, {0, 0}, true);
                    else
                        fReadoutChipInterface->WriteChipReg(cChip, "VCth", 100);
                    // static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs( cChip , {2} , {0}, true );
                }
            }
        }
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));

        // LOG (INFO) << BOLDRED << "Opening shutter ... press any key to close .." << RESET;
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Starting triggers..." << RESET;
        fBeBoardInterface->Start(theBoard);
        LOG(INFO) << BOLDRED << "Shutter opened ... press any key to close .." << RESET;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // do
        // {
        //     std::this_thread::sleep_for (std::chrono::milliseconds (10) );
        // }while( std::cin.get()!='\n');
        LOG(INFO) << BOLDRED << "Reading data .. " << RESET;
        this->ReadData(theBoard, true);
        const std::vector<Event*>& cEvents = this->GetEvents();
        LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadData" << RESET;

        uint32_t cN = 0;
        for(auto& cEvent: cEvents)
        {
            LOG(DEBUG) << ">>> Event #" << cN;
            outp.str("");
            outp << *cEvent;
            LOG(DEBUG) << outp.str();
            cN++;
        }
        LOG(INFO) << BOLDBLUE << "Stopping triggers..." << RESET;
        fBeBoardInterface->Stop(theBoard);
    }
    LOG(INFO) << BOLDBLUE << "Done!" << RESET;
}
void DataChecker::WriteSlinkTest(std::string pDAQFileName)
{
    std::string  cDAQFileName    = (pDAQFileName == "") ? "test.daq" : pDAQFileName;
    FileHandler* cDAQFileHandler = new FileHandler(cDAQFileName, 'w');

    auto              cSetting = fSettingsMap.find("Nevents");
    uint32_t          cNevents = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    std::stringstream outp;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                uint16_t cTh1 = (cHybrid->getId() % 2 == 0) ? 900 : 1;
                uint16_t cTh2 = (cHybrid->getId() % 2 == 0) ? 1 : 900;
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* cReadoutChip = static_cast<ReadoutChip*>(cChip);
                    if(cReadoutChip->getId() % 2 == 0)
                    {
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "VCth", cTh1);
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(cReadoutChip, {10, 244}, {0, 0}, true);
                    }
                    else
                    {
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "VCth", cTh2);
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(cReadoutChip, {2}, {0}, true);
                    }
                }
            }
        }

        BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);
        this->ReadNEvents(cBeBoard, cNevents);
        const std::vector<Event*>& cEvents = this->GetEvents();
        LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadData" << RESET;
        uint32_t cN = 0;
        for(auto& cEvent: cEvents)
        {
            outp.str("");
            outp << *cEvent;

            SLinkEvent cSLev    = cEvent->GetSLinkEvent(cBeBoard);
            auto       cPayload = cSLev.getData<uint32_t>();
            cDAQFileHandler->setData(cPayload);

            if(cN % 10 == 0)
            {
                LOG(INFO) << ">>> Event #" << cN++;
                LOG(INFO) << outp.str();
                for(auto cWord: cPayload) LOG(INFO) << BOLDMAGENTA << std::bitset<32>(cWord) << RESET;
            }
        }
    }
    LOG(INFO) << BOLDBLUE << "Done!" << RESET;
    cDAQFileHandler->closeFile();
    delete cDAQFileHandler;
}
void DataChecker::CollectEvents()
{
    uint32_t cNevents    = this->findValueInSettings("Nevents");
    uint32_t cMaxNevents = 65535;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* cBeBoard         = static_cast<BeBoard*>(cBoard);
        int      cNBursts         = 1 + cNevents / cMaxNevents;
        int      cNrecordedEvents = 0;
        for(int cBurst = 0; cBurst < cNBursts; cBurst++)
        {
            int cEventsToRead = (cBurst == (cNBursts - 1)) ? (cNevents % cMaxNevents) : cMaxNevents;
            this->ReadNEvents(cBeBoard, cEventsToRead);
            const std::vector<Event*>& cEvents = this->GetEvents();
            LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadData" << RESET;
            cNrecordedEvents += cEvents.size();
        }
    }
}

void DataChecker::CheckPSData(BeBoard* pBoard, std::vector<Injection> pInjections)
{
    auto& cBadEvents  = fBadEvents.at(pBoard->getIndex());
    auto& cGoodEvents = fGoodEvents.at(pBoard->getIndex());

    auto     cSetting = fSettingsMap.find("Nevents");
    int      cScale   = 1000;
    uint32_t cNevents = (cSetting != std::end(fSettingsMap)) ? (cSetting->second) * cScale : 100;
    LOG(DEBUG) << BOLDBLUE << "Checking PSdata by reading " << +cNevents << " from BeBoard#" << +pBoard->getIndex() << RESET;

    std::vector<uint32_t> cPixelIds(0);
    std::vector<uint8_t>  cRows(0);
    std::vector<uint8_t>  cColumns(0);
    for(auto cInjection: pInjections)
    {
        cRows.push_back(cInjection.fRow);
        cColumns.push_back(cInjection.fColumn);
        cPixelIds.push_back((uint32_t)(cInjection.fColumn) * 120 + (uint32_t)cInjection.fRow);
    } // injections

    uint16_t cDelay                = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    uint16_t cCalPulseDelay        = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse");
    uint16_t cTimeBetweenCalPulses = cCalPulseDelay + cDelay;

    this->ReadNEvents(pBoard, cNevents);
    const std::vector<Event*>& cEvents = this->GetEvents();
    LOG(DEBUG) << BOLDBLUE << "Read back " << +cEvents.size() << " events from the FC7 ..." << RESET;
    uint32_t         cNMatchedEvents = 0;
    std::vector<int> cMatchedBxIds(0);
    std::vector<int> cMismatchedBxIds(0);
    int              cEventIndx = 0;
    std::vector<int> cBxIdsGbl(0);
    for(auto cEvent: cEvents)
    {
        bool cEventMatchesL1    = true;
        bool cEventMatchesStubs = true;

        for(auto cOpticalGroup: *pBoard)
        {
            auto& cBadEventsOpticalGroup = cBadEvents->at(cOpticalGroup->getIndex());
            auto& cGdEventsOpticalGroup  = cGoodEvents->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cBadEventsHybrid = cBadEventsOpticalGroup->at(cHybrid->getIndex());
                auto& cGdEventsHybrid  = cGdEventsOpticalGroup->at(cOpticalGroup->getIndex());

                // for now I'm only checking one CIC
                // if(cHybrid->getIndex() > 0) continue;

                auto cBxId = (int)cEvent->BxId(cHybrid->getId());
                cBxIdsGbl.push_back(cBxId);
                // only check for differences when
                // there is more than one bx here
                int cBxDifference = 0;
                if(cBxIdsGbl.size() > 1)
                {
                    cBxDifference = cBxId - cBxIdsGbl[cBxIdsGbl.size() - 2];
                    if(cBxDifference < 0) cBxDifference = cBxId + (3564 - cBxIdsGbl[cBxIdsGbl.size() - 2]);
                }

                // now loop over chips and compare data
                for(auto cChip: *cHybrid)
                {
                    auto& cBadEventsChip = cBadEventsHybrid->at(cHybrid->getIndex());
                    auto& cGdEventsChip  = cGdEventsHybrid->at(cOpticalGroup->getIndex());

                    auto&    cBadEventsList  = cBadEventsChip->getSummary<EventsList>();
                    auto&    cGoodEventsList = cGdEventsChip->getSummary<EventsList>();
                    EventTag cEvntTag;
                    auto     cL1Id = (static_cast<D19cCic2Event*>(cEvent))->L1Id(cHybrid->getId(), cChip->getId());
                    EventId  cEventId;
                    cEventId.first  = cL1Id;
                    cEventId.second = cBxId;
                    cEvntTag.first  = cEventId;
                    if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                    // check if it is in p-p or s-p mode

                    auto cErrorBit = (static_cast<D19cCic2Event*>(cEvent))->Error(cHybrid->getId(), cChip->getId());
                    auto cStubs    = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                    // check S and P clusters
                    auto cPClusters = (static_cast<D19cCic2Event*>(cEvent))->GetPixelClusters(cHybrid->getId(), cChip->getId());
                    // auto cSClusters = (static_cast<D19cCic2Event*>(cEvent))->GetStripClusters(cHybrid->getId(), cChip->getId());

                    // in pixel-pixel mode . only get P clusters
                    bool cMatchedPCluster = (cPClusters.size() > 0);
#ifdef __USE_ROOT__
                    TH2D* cNPclusters = static_cast<TH2D*>(getHist(cHybrid, "NPClusters"));
                    cNPclusters->Fill(cTimeBetweenCalPulses, cPClusters.size());
                    TH2D* cNStubs = static_cast<TH2D*>(getHist(cHybrid, "NStubs"));
                    cNStubs->Fill(cTimeBetweenCalPulses, cStubs.size());
                    TH2D* cBxIdsPClusters = static_cast<TH2D*>(getHist(cHybrid, "BxId_Pclusters"));
                    cBxIdsPClusters->Fill(cBxId, cPClusters.size());
                    TH2D* cBxIdsStubs = static_cast<TH2D*>(getHist(cHybrid, "BxId_Stubs"));
                    cBxIdsStubs->Fill(cBxId, cStubs.size());
#endif

                    for(auto cPCluster: cPClusters)
                    {
                        uint8_t cRow    = cPCluster.fAddress;
                        uint8_t cColumn = cPCluster.fZpos;
                        // check column and row
                        bool cFound = false;
                        for(auto cInjectedCluster: pInjections)
                        {
                            if(cInjectedCluster.fRow == cRow && cInjectedCluster.fColumn == cColumn) { cFound = true; }
                        }
                        cMatchedPCluster  = cMatchedPCluster && cFound;
                        bool cRowNotFound = std::find(cRows.begin(), cRows.end(), cRow) == cRows.end();
                        bool cColNotFound = std::find(cColumns.begin(), cColumns.end(), cColumn) == cColumns.end();
                        if(cRowNotFound || cColNotFound)
                        {
                            if(cRowNotFound)
                                LOG(INFO) << BOLDRED << "\t Event#" << +cEventIndx << " BxId#" << +cBxId << " un-expected P-cluster.... Row " << +cRow << " does not match expected." RESET;
                            else
                                LOG(INFO) << BOLDRED << "\t Event#" << +cEventIndx << " BxId#" << +cBxId << " un-expected P-cluster.... Column " << +cColumn << " does not match expected." RESET;
                        }
                    }
                    if((cPClusters.size()) == 0) LOG(DEBUG) << BOLDRED << "Event#" << +cEventIndx << " BxId#" << +cBxId << " has no n P clusters!" << RESET;

                    // also check stubs
                    // assumption is that #stubs == #injections
                    bool cMatchedStubs = (cStubs.size() == cPixelIds.size());
                    for(auto cStub: cStubs)
                    {
                        auto     cStubAddress = cStub.getPosition();
                        auto     cRow         = cStub.getRow();
                        uint32_t cPixelId     = (cStubAddress / 2) + cRow * 120;
                        bool     cFound       = (std::find(cPixelIds.begin(), cPixelIds.end(), cPixelId) != cPixelIds.end());
                        cMatchedStubs         = cMatchedStubs && cFound;
                        if(std::find(cPixelIds.begin(), cPixelIds.end(), cPixelId) == cPixelIds.end())
                        {
                            LOG(DEBUG) << BOLDRED << "\t Event# " << +cEventIndx << " BxId#" << +cBxId << " un-expected Stub in event.... Address " << +cStubAddress << " row " << +cRow << RESET;
                        }
                    }
                    if((cStubs.size()) == 0)
                        LOG(DEBUG) << BOLDRED << "Event#" << +cEventIndx << " BxId#" << +cBxId << " has no stubs!" << RESET;
                    else
                        LOG(DEBUG) << BOLDBLUE << "Have " << +cStubs.size() << "  stub(s) in the event .. and it/they " << ((cMatchedStubs) ? "match" : "don't match") << " what I expect." << RESET;
                    int cClass = 0;
                    if(cPClusters.size() == 0)
                        cClass = 0;
                    else if(cStubs.size() == 0)
                        cClass = 1;
                    else if(cStubs.size() != pInjections.size())
                    {
                        cClass           = 2;
                        auto cPreviousBx = cBxIdsGbl[cBxIdsGbl.size() - 2];
                        LOG(INFO) << BOLDYELLOW << "\t Event# " << +cEventIndx << " BxId#" << +cBxId << " previous BxId#" << +cPreviousBx << " expected difference in Bx is "
                                  << (cTimeBetweenCalPulses + 3) << " classified as an event type " << +cClass << " un-expected number of stubs in event.. expect " << +pInjections.size()
                                  << " and I see " << +cStubs.size() << RESET;
                        for(auto cStub: cStubs)
                        {
                            auto cStubAddress = cStub.getPosition();
                            auto cRow         = cStub.getRow();
                            LOG(INFO) << BOLDYELLOW << "\t\t... address " << +cStubAddress << " row " << +cRow << RESET;
                        } // stubs
                    }
                    else if(!cMatchedPCluster && cMatchedStubs)
                    {
                        cClass = 3;
                    }
                    else if(cMatchedPCluster && !cMatchedStubs)
                    {
                        cClass = 4;
                        // if the stubs don't match .. print out  why
                        for(auto cStub: cStubs)
                        {
                            auto cStubAddress = cStub.getPosition();
                            auto cRow         = cStub.getRow();
                            LOG(INFO) << BOLDRED << "\t Event# " << +cEventIndx << " BxId#" << +cBxId << " classified as an event type " << +cClass << " un-expected Stub in event.... Address "
                                      << +cStubAddress << " row " << +cRow << RESET;
                            for(auto cInjection: pInjections)
                            {
                                LOG(INFO) << BOLDRED << "\t\t.. expected stub address : " << +(cInjection.fRow) * 2 << " and row " << +cInjection.fColumn << RESET;
                            } // injections
                        }     // stubs
                    }
                    else if(cMatchedPCluster && cMatchedStubs)
                    {
                        cClass = 5;
                        if(cBxDifference != 0)
                        {
                            // if the difference in BxIds are ok
                            if(cBxDifference != cTimeBetweenCalPulses + 3)
                            {
                                cClass = 6;
                                for(auto cStub: cStubs)
                                {
                                    auto cStubAddress = cStub.getPosition();
                                    auto cRow         = cStub.getRow();
                                    LOG(INFO) << BOLDYELLOW << "\t Event# " << +cEventIndx << " expected Bx is " << (cBxIdsGbl[cBxIdsGbl.size() - 2] + cTimeBetweenCalPulses + 3)
                                              << " instead I see BxId#" << +cBxId << " classified as an event type " << +cClass << " un-expected Stub in event.... Address " << +cStubAddress << " row "
                                              << +cRow << RESET;
                                    for(auto cInjection: pInjections)
                                    {
                                        LOG(INFO) << BOLDRED << "\t\t.. expected stub address : " << +(cInjection.fRow) * 2 << " and row " << +cInjection.fColumn << RESET;
                                    } // injections
                                }     // stubs
                            }
                        } // check number of Bxs
                    }
                    cEvntTag.second = cClass;

                    // push back into evnent list
                    if(cClass != 5)
                        cBadEventsList.push_back(cEvntTag);
                    else
                        cGoodEventsList.push_back(cEvntTag);

                    bool cPrint = (cClass != 5);
                    if(cPrint)
                        LOG(DEBUG) << BOLDRED << "Event# " << +cEventIndx << " L1 Id is " << +cL1Id << " error bit for this FE is " << +cErrorBit << " Bx difference is " << +cBxDifference
                                   << " event classification is " << +cClass << RESET;
#ifdef __USE_ROOT__
                    TH2D* cEventClass = static_cast<TH2D*>(getHist(cHybrid, "EventClass"));
                    cEventClass->Fill(std::fabs(cBxDifference), cClass);
                    cEventClass = static_cast<TH2D*>(getHist(cHybrid, "EventClassL1Id"));
                    cEventClass->Fill(cL1Id, cClass);
#endif
                    cEventMatchesL1    = cEventMatchesL1 && cMatchedPCluster;
                    cEventMatchesStubs = cEventMatchesStubs && cMatchedStubs;
                } // ROCs
            }     // hybrids or CICs
        }         // optical group loop
        cNMatchedEvents += (cEventMatchesL1 && cEventMatchesStubs) ? 1 : 0;
        cEventIndx++;
    } // event loop
}
uint32_t DataChecker::GenericTriggerConfig(BeBoard* pBoard, int cNrepetitions)
{
    LOG(DEBUG) << BOLDMAGENTA << "DataChecker setting TriggerConfig " << RESET;

    // n events
    auto     cSetting = fSettingsMap.find("Nevents");
    uint32_t cNevents = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;

    // configure trigger blocks
    auto cTriggerMult       = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    int  cNInjectedTriggers = fNInjectedTriggers * cNrepetitions;
    cNevents                = cNrepetitions * cNInjectedTriggers * (1 + cTriggerMult);
    // repeat the sequence N times
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.generic_fcmd.number_of_repetitions", cNrepetitions);
    // make sure fast command duration is 0
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control.fast_duration", 0x0);

    // make sure I accept all trgigers
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNevents);

    // when using generic fast commands I want to read data
    // so first stop the trigger sources in the FC7
    uint32_t cNWords    = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.readout_block.general.words_cnt");
    uint32_t cNtriggers = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
    LOG(DEBUG) << BOLDMAGENTA << "Before stopping triggers : " << +cNWords << " words in the readout and " << +cNtriggers << " triggers in the trigger_in counter." << RESET;

    // stop triggers
    fBeBoardInterface->Stop(pBoard);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // re-load configuration
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetTriggerFSM();

    cNWords    = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.readout_block.general.words_cnt");
    cNtriggers = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
    LOG(DEBUG) << BOLDMAGENTA << "After stopping triggers [no reset]: " << +cNWords << " words in the readout and " << +cNtriggers << " triggers in the trigger_in counter " << RESET;

    // make sure data handshake is disabled
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x00);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // set the packet size to be exactly equal to the number we expect
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.packet_nbr", cNevents - 1);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // make sure this is set
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNevents);
    std::this_thread::sleep_for(std::chrono::microseconds(10));

    // re-load configuration
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetTriggerFSM();
    // also .. reset the readout
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();
    // and just check trigger config

    return cNevents;
}
void DataChecker::PrepareDigitalInjection(DetectorDataContainer& pInjectionScheme)
{
    uint8_t cStubWindow = 1; // stub window in half pixels (1)
    uint8_t cMode       = 2; // (0) pixel-strip, (1) strip-strip, (2) pixel-pixel, (3) strip-pixel
    // set-up MPA for injection
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cInjections = pInjectionScheme.at(cBoard->getIndex());
        for(auto cOpticalReadout: *cBoard)
        {
            auto& cInjectionsOG = cInjections->at(cOpticalReadout->getIndex());
            for(auto cHybrid: *cOpticalReadout)
            {
                auto& cInjectionsHybrid = cInjectionsOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid) // for each chip (makes sense)
                {
                    // for the moment - only written for CBC3
                    if(cChip->getFrontEndType() == FrontEndType::MPA)
                    {
                        auto& cInjectionsChip = cInjectionsHybrid->at(cChip->getIndex());
                        auto& cSummaryInj     = cInjectionsChip->getSummary<std::vector<Injection>>();
                        // activate stub mode
                        fReadoutChipInterface->WriteChipReg(cChip, "StubMode", cMode);
                        fReadoutChipInterface->WriteChipReg(cChip, "StubWindow", cStubWindow);
                        // mask all pixels first
                        (static_cast<PSInterface*>(fReadoutChipInterface))->WriteChipReg(cChip, "DigitalSync", 0x00);

                        if(cSummaryInj.size() == 0) continue;
                        LOG(DEBUG) << BOLDMAGENTA << "Injecting " << +cSummaryInj.size() << " in MPA#" << +cChip->getId() << RESET;
                        (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, cSummaryInj);
                    }
                } // chip
            }     // hybrid
        }         // module
    }             // boards
}
void DataChecker::FastCommandInjections(int cNtrials)
{
    // LOG(INFO) << BOLDMAGENTA << "Cal Pulse Injections with GFCMDs " << RESET;

    auto   cSetting       = fSettingsMap.find("DelayAfterInjection");
    size_t cCalPulseDelay = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 85;
    cSetting              = fSettingsMap.find("CheckForStubs");
    int  cCheckForStubs   = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    bool cWStubs          = (cCheckForStubs == 1);
    // figure out if injection duration
    // needs to be 1 or 8 clock cycles
    cSetting                 = fSettingsMap.find("DistributeInjections");
    int    cInjDistrFlag     = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    bool   cDistributeInj    = (cInjDistrFlag == 1);
    size_t cCalPulseDuration = cDistributeInj ? 8 : 1;
    cSetting                 = fSettingsMap.find("TriggerSeparation");
    size_t cSeparation       = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 10;
    bool   cUseClearCounter  = false;
    cSetting                 = fSettingsMap.find("TriggerMultiplicity");
    size_t cTriggerMult      = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    if(cCalPulseDelay < 85 && cWStubs)
    {
        LOG(INFO) << BOLDMAGENTA << "Minimum delay is 85 clocks... changing to that" << RESET;
        cCalPulseDelay = 85;
    }

    // random c++
    std::srand(std::time(NULL));
    std::random_device          cRndm{};
    std::mt19937                cGen{cRndm()};
    std::poisson_distribution<> cDistTrigSep(cSeparation);

    std::vector<uint16_t> fTriggerSeparation;
    fTriggerSeparation.clear();
    // clear fast command vector
    fFastCommands.clear();
    fNInjectedTriggers = 0;

    // start with fSizeGap empty fast commands
    // then 1 BC0
    // then fSizeGap empty fast commands
    size_t fSizeGap = 20;
    for(size_t cIndx = 0; cIndx < (cUseClearCounter) ? 2 * fSizeGap : 0; cIndx++)
    {
        if(cIndx == fSizeGap) fFastCommands.push_back(0xC3);
        fFastCommands.push_back(0xC1);
    }
    // count triggers

    size_t cTriggerCounter = 0;
    // size_t cGap = cTriggerSpacing - (cCalPulseDuration+cCalPulseDelay);
    size_t cBxId = 0;
    fTriggeredBxs.clear();
    fBurstIds.clear();
    size_t cBurst = 0;
    for(size_t cIndx = 0; cIndx < (size_t)cNtrials; cIndx++)
    {
        // TP lasts N clock cycles
        for(size_t cBx = 0; cBx < cCalPulseDuration; cBx++)
        {
            if(fFastCommands.size() == 16383) continue;
            fFastCommands.push_back(0xC5);
            cBxId++;
        }
        // cCalPulseDelay Bx then L1A
        for(size_t cBx = 0; cBx <= cCalPulseDelay; cBx++)
        {
            if(fFastCommands.size() == 16383) continue;
            fFastCommands.push_back(0xC1);
            cBxId++;
        }
        for(size_t cBx = 0; cBx < 1; cBx++)
        {
            if(fFastCommands.size() == 16383) continue;
            for(size_t cTriggerId = 0; cTriggerId < cTriggerMult; cTriggerId++)
            {
                if(fFastCommands.size() == 16383) continue;
                fFastCommands.push_back(0xC9);
                fTriggeredBxs.push_back(cBxId);
                fBurstIds.push_back(cBurst);
                // LOG (INFO) << BOLDMAGENTA << "\t\t.. DataChecker::FastCommandInjections trigger in Bx" << +cBxId << RESET;
                cBxId++;
                cTriggerCounter++;
                fNInjectedTriggers++;
            }
            cBurst++;
        }
        // gap then next tP
        // if I want to make this a random distribution of triggers then
        // rather than always using the same gap I need to change this
        // only thing is I am limited by the fact that I need
        // approximately 80 Bx in the back-end
        size_t cTriggerGap = std::round(cDistTrigSep(cGen));
        for(size_t cBx = 0; cBx < cTriggerGap; cBx++)
        {
            if(fFastCommands.size() == 16383) continue;
            fFastCommands.push_back(0xC1);
            cBxId++;
        }
    }
    // and then make sure you end with fSizeGap empty fast command
    for(size_t cIndx = 0; cIndx < fSizeGap; cIndx++)
    {
        if(fFastCommands.size() == 16383) continue;
        fFastCommands.push_back(0xC1);
    }
}
bool DataChecker::GenericFastCommands()
{
    bool cSucessReadout = true;
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFCMDBram(fFastCommands);
    for(auto cBoard: *fDetectorContainer)
    {
        // trigger configuration
        bool     cSuccess         = false;
        size_t   cReadoutAttempts = 0;
        uint32_t cNtriggers       = 0;
        uint32_t cNevents         = 0;
        while(!cSuccess && cReadoutAttempts < 10)
        {
            cNevents = this->GenericTriggerConfig(cBoard);
            if(cReadoutAttempts > 0)
            {
                LOG(INFO) << BOLDRED << "Re-trying to send triggers.. mismatch in number of triggers sent and words received"
                          << " in the back-end." << RESET;
            }
            LOG(DEBUG) << BOLDMAGENTA << "Trigger configured to ensure " << +cNevents << " events in the readout" << RESET;

            // start generic  - ctrl signal high
            fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x1);
            // stop generic  - ctrl signal low
            fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.fast_command_block.control.start_generic", 0x0);

            // wait until all triggers have been sent
            uint32_t cCounter = 0;
            uint32_t cNWords  = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
            do {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                cNWords    = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
                cNtriggers = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
                // print to screen
                LOG(DEBUG) << BOLDGREEN << "Iter#" << +cCounter << " : " << +cNWords << " words in the readout and " << RESET;
                cCounter++;
            } while(cCounter < 100 && cNtriggers < cNevents);

            // now wait until number of words have stopped increasing
            uint32_t cNWordsPrev = 0;
            cCounter             = 0;
            do {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                cNWordsPrev = (cCounter == 0) ? 0 : cNWords;
                cNWords     = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.readout_block.general.words_cnt");
                // print to screen
                LOG(DEBUG) << BOLDGREEN << "Iter#" << +cCounter << " : " << +cNWords << " words in the readout and "
                           << " previous iteration found " << +cNWordsPrev << RESET;
                cCounter++;
            } while(cCounter < 100 && (cNWordsPrev != cNWords));

            LOG(DEBUG) << BOLDGREEN << "After " << +cCounter << " iterations: " << +cNWords << " words in the readout"
                       << " and " << +cNtriggers << " triggers found by the trigger_in_counter" << RESET;
            cSuccess = (cNtriggers == cNevents);
            cReadoutAttempts++;
        }
        // stop triggers
        // Read Data
        size_t cReadBackEvents = 0;
        if(cNtriggers > 0)
        {
            // wait another 10 ms
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cReadBackEvents = this->ReadData(cBoard, true);
            cSuccess        = cSuccess && (cReadBackEvents == cNevents);
        }

        if(!cSuccess)
            LOG(INFO) << BOLDRED << "Trigger in counter " << +cNtriggers << " and number of words in the readout is " << +cReadBackEvents << " .... expected both to be " << +cNevents << RESET;
        cSucessReadout = cSucessReadout && cSuccess;
    }
    return cSucessReadout;
}

// retreive BxIds
// taking into account rollover
std::vector<float> DataChecker::GetBxIds(std::vector<float> pRawBxIds)
{
    std::vector<float> cBxIds(0);
    int                cNRollOvers   = 0;
    uint16_t           cMaxBxCounter = 3564;
    for(size_t cIndx = 1; cIndx < pRawBxIds.size(); cIndx++)
    {
        uint16_t cBxIdPrev     = pRawBxIds[cIndx - 1];
        uint16_t cBxId         = pRawBxIds[cIndx];
        int      cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBxIdPrev % cMaxBxCounter);
        cNRollOvers += (cBxId < cBxIdPrev); //((cBxIdPrev >= 2500) && (cBxIdPrev < cMaxBxCounter)) && ( cBxId < cBxIdPrev) ? 1 : 0;
        cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBxId % cMaxBxCounter) - cBxDifference;
        cBxIds.push_back(cBxDifference);
        LOG(DEBUG) << BOLDMAGENTA << "\t.. Bx Difference is " << +cBxDifference << RESET;
    }
    return cBxIds;
}
std::vector<int> DataChecker::GenerateIds()
{
    //
    auto     cSetting    = fSettingsMap.find("ActiveMPAs");
    uint32_t cActiveMPAs = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 8;

    // random c++
    std::srand(std::time(NULL));
    std::random_device cRndm{};
    std::mt19937       cGen{cRndm()};

    std::uniform_int_distribution<int> cDist(1, cActiveMPAs);
    std::uniform_int_distribution<int> cIdDist(0, 7);
    size_t                             cNFEs = cDist(cGen);
    std::vector<int>                   cIds;
    cIds.clear();
    do {
        int cId = cIdDist(cGen);
        if(std::find(cIds.begin(), cIds.end(), cId) == cIds.end()) { cIds.push_back(cId); }

    } while(cIds.size() < cNFEs);
    return cIds;
}
// pixel-pixel mode
std::vector<Injection> DataChecker::GenerateInjections(int pMaxClusters, int pMaxNstubs)
{
    std::vector<Injection> cInjections(0);

    // random c++
    std::srand(std::time(NULL));
    std::random_device cRndm{};
    std::mt19937       cGen{cRndm()};

    // generate injections in this MPA
    std::uniform_int_distribution<int> cFlatDistCols(2, 13);
    std::uniform_int_distribution<int> cFlatDistRows(2, 118); // avoid colums 1 and 120
    std::uniform_int_distribution<int> cNClusterDist(1, pMaxClusters);

    std::vector<uint8_t> cColumns(0); // 5 , 10 };
    std::vector<uint8_t> cRows(0);    // 20 , 30};
    size_t               cNclstrs = cNClusterDist(cGen);

    int                   cTotalNumberOfClusters = 0;
    std::vector<uint32_t> cPixelIds(0); // these will be used to generate stubs
    do {
        Injection cInjection;
        cInjection.fColumn = (cFlatDistCols(cGen));
        cInjection.fRow    = (cFlatDistRows(cGen));
        uint32_t cPixelId  = (uint32_t)(cInjection.fColumn) * 120 + (uint32_t)cInjection.fRow;
        if(std::find(cPixelIds.begin(), cPixelIds.end(), cPixelId) == cPixelIds.end())
        {
            // check if the pixel is displaced by at least 4 pixels
            bool cTooClose = false;
            for(size_t cIndx = 0; cIndx < cInjections.size(); cIndx++)
            {
                if(cInjection.fColumn == cInjections[cIndx].fColumn) { cTooClose = cTooClose || std::fabs(cInjection.fRow - cInjections[cIndx].fRow) < 5; }

                if(cInjection.fRow == cInjections[cIndx].fRow) { cTooClose = cTooClose || std::fabs(cInjection.fColumn - cInjections[cIndx].fColumn) < 5; }
            }
            if(!cTooClose && cTotalNumberOfClusters < pMaxNstubs)
            {
                LOG(DEBUG) << BOLDMAGENTA << "Iinjecting in row " << +cInjection.fRow << " columnn " << +cInjection.fColumn << RESET;
                cPixelIds.push_back(cPixelId);
                cInjections.push_back(cInjection);
                cTotalNumberOfClusters += 1;
            }
        }
    } while(cPixelIds.size() < cNclstrs && cTotalNumberOfClusters < pMaxNstubs); // create injection patterns
    return cInjections;
}
// strip-pixel mode
std::vector<Injection> DataChecker::GeneratePSInjections(int pMaxNstubs)
{
    // int                    cClosestColAllowed = 1;
    std::vector<Injection> cInjections(0);

    // random c++
    std::srand(std::time(NULL));
    std::random_device cRndm{};
    std::mt19937       cGen{cRndm()};

    // generate injections in this MPA
    // std::uniform_int_distribution<int> cFlatDistSeeds(2, 13);
    // std::uniform_int_distribution<int> cFlatDistStrips(5, 110);
    std::uniform_int_distribution<int> cNStubDist(1, pMaxNstubs);

    std::vector<uint8_t>  cColumns(0); // 5 , 10 };
    std::vector<uint8_t>  cRows(0);    // 20 , 30};
    auto                  cSetting            = fSettingsMap.find("RandomizeInjections");
    uint8_t               cRandom             = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    size_t                cStubs              = cRandom ? cNStubDist(cGen) : pMaxNstubs;
    int                   cTotalNumberOfStubs = 0;
    std::vector<uint32_t> cPixelIds(0); // these will be used to generate stubs
    // LOG (DEBUG) << BOLDMAGENTA << "\t...Injecting "
    //                     << +cStubs
    //                     << " stubs... "
    //                     << RESET;
    do {
        // Seed
        Injection cInjection;
        cInjection.fColumn = 1 + cPixelIds.size();        //(cFlatDistSeeds(cGen));
        cInjection.fRow    = (1 + cPixelIds.size()) * 10; //(cFlatDistStrips(cGen));
        uint32_t cPixelId  = (uint32_t)(cInjection.fColumn) * 120 + (uint32_t)cInjection.fRow;
        cPixelIds.push_back(cPixelId);
        cInjections.push_back(cInjection);
        cColumns.push_back(cInjection.fColumn);
        cRows.push_back(cInjection.fRow);
        cTotalNumberOfStubs += 1;

        // if(std::find(cPixelIds.begin(), cPixelIds.end(), cPixelId) == cPixelIds.end())
        // {
        //     // never inject in the same row more than once
        //     // confusing when it comes to checking SSA data
        //     // check if you've already injected in this row
        //     bool cInject = (cColumns.size() == 0);
        //     if(std::find(cRows.begin(), cRows.end(), cInjection.fRow) == cRows.end() && !cInject)
        //     {
        //         // check that you're at least one row away
        //         std::vector<float> cTmpR(cRows.size(), 0);
        //         std::transform(cRows.begin(), cRows.end(), cTmpR.begin(), [&](float el) { return std::fabs(el - cInjection.fRow); });
        //         std::sort(cTmpR.begin(), cTmpR.end());
        //         cTmpR.erase(unique(cTmpR.begin(), cTmpR.end()), cTmpR.end());
        //         cInject = (cTmpR[0] > cClosestColAllowed);

        //         if(!cInject) continue;

        //         if( cColumns.size() > 0 )
        //         {
        //             // check that you're at least one column away from anything
        //             std::vector<float> cTmpC(cColumns.size(), 0);
        //             std::transform(cColumns.begin(), cColumns.end(), cTmpC.begin(), [&](float el) { return std::fabs(el - cInjection.fColumn); });
        //             std::sort(cTmpC.begin(), cTmpC.end());
        //             cTmpC.erase(unique(cTmpC.begin(), cTmpC.end()), cTmpC.end());
        //             cInject = (cTmpC[0] > cClosestColAllowed);
        //         }

        //         if(!cInject) continue;

        //         // ok .. have not so this is good
        //         // figure out how far away I an noq
        //         // std::vector<float> cTmp(cPixelIds.size(), 0);
        //         // std::transform(cPixelIds.begin(), cPixelIds.end(), cTmp.begin(), [&](float el) { return std::fabs(el - ((uint32_t)(cInjection.fColumn) * 120 + (uint32_t)cInjection.fRow)); });
        //         // std::sort(cTmp.begin(), cTmp.end());
        //         // cTmp.erase(unique(cTmp.begin(), cTmp.end()), cTmp.end());
        //         // // if the smallest distance is < x .. don't inject
        //         // cInject = (cTmp[0] >= cClosestColAllowed);

        //         // if( cInject )
        //         //     LOG (DEBUG) << BOLDGREEN << "\t\t\t Closest injection is " << cTmp[0]
        //         //         << " pixels away [ "
        //         //         << +cInjection.fColumn
        //         //         << "] closest allowed is "
        //         //         << +cClosestColAllowed
        //         //         << RESET;
        //         // else
        //         //     LOG (DEBUG) << BOLDRED << "\t\t\t Closest injection is " << cTmp[0]
        //         //         << " pixels away [ "
        //         //         << +cInjection.fColumn
        //         //         << "] closest allowed is "
        //         //         << +cClosestColAllowed
        //         //         << RESET;
        //     }
        //     if(cInject && cTotalNumberOfStubs < pMaxNstubs)
        //     {
        //         cPixelIds.push_back(cPixelId);
        //         cInjections.push_back(cInjection);
        //         cColumns.push_back(cInjection.fColumn);
        //         cRows.push_back(cInjection.fRow);
        //         cTotalNumberOfStubs += 1;
        //     }
        // }
    } while(cPixelIds.size() < cStubs && cTotalNumberOfStubs < pMaxNstubs); // create injection patterns
    return cInjections;
}
std::vector<uint8_t> DataChecker::GeneratePSstrpClusters(int pMaxNSclusters)
{
    std::vector<uint8_t> cInjections(0);
    // random c++
    std::srand(std::time(NULL));
    std::random_device cRndm{};
    std::mt19937       cGen{cRndm()};

    // generate injections in this MPA
    std::uniform_int_distribution<int> cFlatDistStrips(5, 110);
    std::vector<uint8_t>               cRows(0); // 20 , 30};
    do {
        // Seed
        uint8_t cRow = (cFlatDistStrips(cGen));
        // nothing yet .. so .. add
        bool cAddToList = (cRows.size() == 0);
        if(!cAddToList)
        {
            // can't find it in the list .. that's a good start
            cAddToList = std::find(cRows.begin(), cRows.end(), cRow) == cRows.end();
            if(cAddToList)
            {
                // check that you're at least one row away
                std::vector<float> cTmpR(cRows.size(), 0);
                std::transform(cRows.begin(), cRows.end(), cTmpR.begin(), [&](float el) { return std::fabs(el - cRow); });
                std::sort(cTmpR.begin(), cTmpR.end()); // closest
                cTmpR.erase(unique(cTmpR.begin(), cTmpR.end()), cTmpR.end());
                // LOG (INFO) << BOLDMAGENTA << "Thinking of adding row number " << +cRow << RESET;
                // for( auto cTmp : cTmpR ) LOG (INFO) << BOLDMAGENTA << "\t..Difference .. " << +cTmp << RESET;
                cAddToList = (cTmpR[0] > 1);
            }
        }
        if(cAddToList)
        {
            // LOG (INFO) << BOLDGREEN << " Addding row#" << +cRow << RESET;
            cRows.push_back(cRow);
        }
    } while(cRows.size() < (size_t)pMaxNSclusters); // create injection patterns
    return cRows;
}
std::vector<Injection> DataChecker::GeneratePSpxlClusters(int pMaxNPclusters)
{
    int                    cClosestColAllowed = 2;
    std::vector<Injection> cInjections(0);

    // random c++
    std::srand(std::time(NULL));
    std::random_device cRndm{};
    std::mt19937       cGen{cRndm()};

    // generate injections in this MPA
    std::uniform_int_distribution<int> cFlatDistSeeds(2, 13);
    std::uniform_int_distribution<int> cFlatDistStrips(5, 110);

    std::vector<uint8_t>  cColumns(0);  // 5 , 10 };
    std::vector<uint8_t>  cRows(0);     // 20 , 30};
    std::vector<uint32_t> cPixelIds(0); // these will be used to generate stubs
    do {
        // Seed
        Injection cInjection;
        cInjection.fColumn = (cFlatDistSeeds(cGen));
        cInjection.fRow    = (cFlatDistStrips(cGen));
        uint32_t cPixelId  = (uint32_t)(cInjection.fColumn) * 120 + (uint32_t)cInjection.fRow;
        if(std::find(cPixelIds.begin(), cPixelIds.end(), cPixelId) == cPixelIds.end())
        {
            // never inject in the same row more than once
            // confusing when it comes to checking SSA data
            // check if you've already injected in this row
            bool cInject = (cColumns.size() == 0);
            if(std::find(cRows.begin(), cRows.end(), cInjection.fRow) == cRows.end() && !cInject)
            {
                // check that you're at least one row away
                std::vector<float> cTmpR(cRows.size(), 0);
                std::transform(cRows.begin(), cRows.end(), cTmpR.begin(), [&](float el) { return std::fabs(el - cInjection.fRow); });
                std::sort(cTmpR.begin(), cTmpR.end());
                cTmpR.erase(unique(cTmpR.begin(), cTmpR.end()), cTmpR.end());
                cInject = (cTmpR[0] > 1);

                if(!cInject) continue;

                // ok .. have not so this is good
                // figure out how far away I an noq
                std::vector<float> cTmp(cPixelIds.size(), 0);
                std::transform(cPixelIds.begin(), cPixelIds.end(), cTmp.begin(), [&](float el) { return std::fabs(el - ((uint32_t)(cInjection.fColumn) * 120 + (uint32_t)cInjection.fRow)); });
                std::sort(cTmp.begin(), cTmp.end());
                cTmp.erase(unique(cTmp.begin(), cTmp.end()), cTmp.end());
                // if the smallest distance is < x .. don't inject
                cInject = (cTmp[0] >= cClosestColAllowed);
            }
            if(cInject)
            {
                cPixelIds.push_back(cPixelId);
                cInjections.push_back(cInjection);
                cColumns.push_back(cInjection.fColumn);
                cRows.push_back(cInjection.fRow);
            }
        }
    } while(cPixelIds.size() < (size_t)pMaxNPclusters); // create injection patterns
    return cInjections;
}
void DataChecker::PreparePSInjection(DetectorDataContainer& pInjectionScheme)
{
    auto cSetting          = fSettingsMap.find("Bend");
    int  cBend             = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    cSetting               = fSettingsMap.find("DistributeInjections");
    int     cInjDistrFlag  = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    bool    cDistributeInj = (cInjDistrFlag == 1);
    uint8_t cStubWindow    = (std::fabs(cBend) + 1) * 2; // stub window in half pixels (1)
    uint8_t cMode          = 0;                          // (0) pixel-strip, (1) strip-strip, (2) pixel-pixel, (3) strip-pixel
    // set-up MPA for injection
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cInjections = pInjectionScheme.at(cBoard->getIndex());
        for(auto cOpticalReadout: *cBoard)
        {
            auto& cInjectionsOG = cInjections->at(cOpticalReadout->getIndex());
            for(auto cHybrid: *cOpticalReadout)
            {
                auto&                                   cInjectionsHybrid = cInjectionsOG->at(cHybrid->getIndex());
                std::map<uint8_t, std::vector<uint8_t>> cInjectionsPerChip;
                for(auto cChip: *cHybrid) // for each chip (makes sense)
                {
                    // for the moment - only written for CBC3
                    if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                    auto& cInjectionsChip = cInjectionsHybrid->at(cChip->getIndex());
                    auto& cSummaryInj     = cInjectionsChip->getSummary<std::vector<Injection>>();
                    // activate stub mode
                    fReadoutChipInterface->WriteChipReg(cChip, "StubMode", cMode);
                    fReadoutChipInterface->WriteChipReg(cChip, "StubWindow", cStubWindow);
                    // mask all pixels first
                    (static_cast<PSInterface*>(fReadoutChipInterface))->WriteChipReg(cChip, "DigitalSync", 0x00);
                    if(cSummaryInj.size() == 0) continue;

                    // LOG (INFO) << BOLDMAGENTA << "Injecting " << +cSummaryInj.size() << " in SSA-MPA pair#" << +cChip->getId() << RESET;
                    uint8_t cPattern = cDistributeInj ? (1 << (7 - cChip->getId())) : (0x1 << 0);
                    (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, cSummaryInj, cPattern);
                    auto cIterator = cInjectionsPerChip.find(cChip->getId());
                    if(cIterator == cInjectionsPerChip.end())
                    {
                        std::vector<uint8_t> cRows(0);
                        for(auto cInj: cSummaryInj)
                        {
                            LOG(DEBUG) << BOLDMAGENTA << "\t...Injecting in pixel#" << +cInj.fColumn << " strip#" << +cInj.fRow << " in MPA#" << +cChip->getId() << RESET;
                            cRows.push_back(cInj.fRow);
                        }
                        cInjectionsPerChip.insert(std::make_pair(cChip->getId(), cRows));
                    }
                } // chip

                // mask all channels in all SSAs
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() != FrontEndType::SSA) continue;

                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                } // chip

                // inject in some rows
                for(const auto& cInjection: cInjectionsPerChip)
                {
                    auto cRows = cInjection.second;
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() != FrontEndType::SSA) continue;

                        if(cChip->getId() != cInjection.first) continue;

                        uint8_t cPattern = cDistributeInj ? (1 << (7 - cChip->getId())) : (0x1 << 0);
                        fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_L_ALL", cPattern);
                        fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x01);
                        for(auto cRow: cRows)
                        {
                            LOG(DEBUG) << BOLDMAGENTA << "\t...Injecting in"
                                       << " strip#" << +cRow << " in SSA#" << +cChip->getId() << RESET;
                            fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cRow + cBend), 0x9);
                        }
                    } // chip
                }
            } // hybrid
        }     // module
    }         // boards
}
void DataChecker::PSTriggerTests()
{
    // decode setting
    auto     cSetting   = fSettingsMap.find("Attempts");
    uint32_t cAttempts  = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 10;
    fTriggerTestCounter = 0;
    for(size_t cAttempt = 0; cAttempt < cAttempts; cAttempt++)
    {
        if(fTriggerTestCounter % 10 == 0) LOG(INFO) << BOLDMAGENTA << "Attempt#" << +cAttempt << RESET;
        this->PSTriggerTest();
        fTriggerTestCounter++;
    }
}
void DataChecker::PSTriggerTest()
{
    // decode setting
    auto     cSetting      = fSettingsMap.find("Nevents");
    uint32_t cNtrials      = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 10;
    cSetting               = fSettingsMap.find("MaxPClusters");
    int cMaxClustersPerMPA = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    cSetting               = fSettingsMap.find("DelayAfterInjection");
    size_t cCalPulseDelay  = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 85;
    cSetting               = fSettingsMap.find("DistributeInjections");
    int  cInjDistrFlag     = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    bool cDistributeInj    = (cInjDistrFlag == 1);
    cSetting               = fSettingsMap.find("Bend");
    int cBend              = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    cSetting               = fSettingsMap.find("CheckForStubs");
    int  cCheckForStubs    = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 1;
    bool cWStubs           = (cCheckForStubs == 1);
    cSetting               = fSettingsMap.find("FifoDepth");
    int cFifoDepth         = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 8;

    // configure FIFO depth in SSAs
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                for(auto cChip: *cHybrid) // for each chip (makes sense)
                {
                    // for the moment - only written for CBC3
                    if(cChip->getFrontEndType() != FrontEndType::SSA) continue;

                    fReadoutChipInterface->WriteChipReg(cChip, "OutPattern7/FIFOconfig", cFifoDepth);
                }
            }
        }
    }
    // prepare injection scheme
    DetectorDataContainer cInjectionScheme;
    ContainerFactory::copyAndInitChip<std::vector<Injection>>(*fDetectorContainer, cInjectionScheme);
    std::vector<int> cIds = GenerateIds();
    // data container holding number
    // of P-clusters
    DetectorDataContainer cCicInjections;
    ContainerFactory::copyAndInitHybrid<uint32_t>(*fDetectorContainer, cCicInjections);
    // in pixel-pixel mode this is the same
    // as the number of stubs
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cInjections = cInjectionScheme.at(cBoard->getIndex());
        auto& cCicInjc    = cCicInjections.at(cBoard->getIndex());
        for(auto cOpticalReadout: *cBoard)
        {
            auto& cInjectionsOG = cInjections->at(cOpticalReadout->getIndex());
            auto& cCicInjcOG    = cCicInjc->at(cOpticalReadout->getIndex());
            for(auto cHybrid: *cOpticalReadout)
            {
                auto& cInjectionsHybrid = cInjectionsOG->at(cHybrid->getIndex());
                //
                auto& cCicInjHybrid  = cCicInjcOG->at(cHybrid->getIndex());
                auto& cCicInjSummary = cCicInjHybrid->getSummary<uint32_t>();
                cCicInjSummary       = 0;
                for(auto cChip: *cHybrid) // for each chip (makes sense)
                {
                    // for the moment - only written for CBC3
                    if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                    if(std::find(cIds.begin(), cIds.end(), cChip->getId()) == cIds.end()) continue;

                    LOG(INFO) << BOLDMAGENTA << "Injecting in SSA-MPA pair#" << +cChip->getId() << RESET;
                    auto cInjections = GeneratePSInjections(cMaxClustersPerMPA);
                    // auto cInjections = GenerateInjections(cMaxClustersPerMPA);
                    auto& cInjectionsChip = cInjectionsHybrid->at(cChip->getIndex());
                    auto& cSummaryInj     = cInjectionsChip->getSummary<std::vector<Injection>>();
                    cSummaryInj.clear();
                    for(auto cInjection: cInjections)
                    {
                        // // I've messed up rows and cols earlier
                        cInjection.fFeId = cChip->getId();
                        // LOG (DEBUG) << BOLDMAGENTA << "Adding injection in FE#" << +cChip->getId()
                        //     << " strip " << +cInjection.fRow
                        //     << " pixel column " << +cInjection.fColumn
                        //     << RESET ;
                        cSummaryInj.push_back(cInjection);
                        cCicInjSummary++;
                    }
                    // LOG (DEBUG) << BOLDMAGENTA << "Injecting " << +cSummaryInj.size()
                    //     << " P-clusters in MPA#" << +cChip->getId()
                    //     << RESET;
                } // chip
                // if injection lasts more than one Bx
                // need to mulitple here
                // LOG (INFO) << BOLDMAGENTA << "Injected " << +cCicInjSummary
                //     << " stubs in CIC#" << +cHybrid->getId()
                //     << RESET;
            } // hybrid
        }     // module
    }         // boards
    PreparePSInjection(cInjectionScheme);
    // PrepareDigitalInjection(cInjectionScheme);

    // generate fast commands
    this->FastCommandInjections(cNtrials);
    std::vector<float> cDifferences(fTriggeredBxs.size());
    std::adjacent_difference(fTriggeredBxs.begin(), fTriggeredBxs.end(), cDifferences.begin());
    cDifferences.erase(cDifferences.begin(), cDifferences.begin() + 1);
    cDifferences.erase(cDifferences.end() - 1, cDifferences.end());

    auto cStats = getStats(cDifferences);
    if(fTriggerTestCounter % 10 == 0)
        LOG(INFO) << BOLDMAGENTA << "\t...Generic fast command block used to send " << +fNInjectedTriggers << " triggers have " << fTriggeredBxs.size() << " Bx with a trigger "
                  << " <Trigger Seperation> " << +cStats.first << " Bx."
                  << " RMS Trigger Separation " << +cStats.second << " Bx." << RESET;
    // rough latency scan
    // auto cStubOffset    = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->getStubOffset();
    // int  cMaxStubSel    = (cDistributeInj) ? 8 : 1;
    int cOptimalOffset = -1;
    for(int cLatencyOffset = cOptimalOffset; cLatencyOffset < cOptimalOffset + 1; cLatencyOffset++)
    {
        LOG(DEBUG) << BOLDMAGENTA << "L1 Latency offset is " << +cLatencyOffset << RESET;
        for(int cStubOffset = 50; cStubOffset < 70; cStubOffset++)
        // for(int cStubSel = 0; cStubSel < cMaxStubSel; cStubSel++)
        {
            // configure latencies
            DetectorDataContainer cLatencyPerFE;
            ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, cLatencyPerFE);
            DetectorDataContainer cStubLatencyPerBoard;
            ContainerFactory::copyAndInitBoard<uint16_t>(*fDetectorContainer, cStubLatencyPerBoard);
            for(auto cBoard: *fDetectorContainer)
            {
                uint16_t cDelay       = cCalPulseDelay;
                int      cReTimeValue = -1;
                auto&    cFeLatency   = cLatencyPerFE.at(cBoard->getIndex());
                auto&    cBrdLatency  = cStubLatencyPerBoard.at(cBoard->getIndex());
                for(auto cOpticalReadout: *cBoard)
                {
                    auto& cFeLatencyOG = cFeLatency->at(cOpticalReadout->getIndex());
                    for(auto cHybrid: *cOpticalReadout)
                    {
                        auto& cFeLatencyHybrid = cFeLatencyOG->at(cHybrid->getIndex());
                        for(auto cChip: *cHybrid) // for each chip (makes sense)
                        {
                            auto& cFeLatencyChip = cFeLatencyHybrid->at(cChip->getIndex());
                            auto& cFeLatencySmry = cFeLatencyChip->getSummary<uint16_t>();
                            if(cChip->getFrontEndType() != FrontEndType::SSA)
                            {
                                // uint16_t cLatency = cDelay + cLatencyOffset ;
                                cFeLatencySmry = cDelay + cLatencyOffset;
                                if(cChip->getFrontEndType() == FrontEndType::MPA && cReTimeValue < 0) { cReTimeValue = fReadoutChipInterface->ReadChipReg(cChip, "RetimePix"); }
                            }
                            else // SSA needs an additional clock cycle of delay
                            {
                                // uint16_t cLatency = cDelay + (cLatencyOffset-1) ;
                                cFeLatencySmry = cDelay + (cLatencyOffset - 1);
                            }
                            if(cDistributeInj) cFeLatencySmry = cFeLatencySmry + cChip->getId();
                            fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cFeLatencySmry);
                            std::string cType = (cChip->getFrontEndType() != FrontEndType::SSA) ? "MPA" : "SSA";
                            // LOG (DEBUG) << BOLDBLUE << "Setting L1 latency in " << cType << "#"
                            //     << +cChip->getId()
                            //     << " to " << +cFeLatencySmry
                            //     << RESET;
                        } // chip
                    }     // hybrid
                }         // module
                if(cWStubs)
                {
                    auto& cStubLatency = cBrdLatency->getSummary<uint16_t>();
                    cStubLatency       = cDelay + cLatencyOffset - (cStubOffset + cReTimeValue);
                    // cStubLatency = cDelay + cLatencyOffset - (cStubOffset + cReTimeValue) + cStubSel;
                    auto cPackageDelay = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
                    fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
                    LOG(INFO) << BOLDMAGENTA << "Package delay set to " << +cPackageDelay << "... stub latency set to " << +cStubLatency << RESET;
                }
            } // boards

            // transmit fast comamnds and readout data
            bool cSuccessfulReadout = this->GenericFastCommands();
            if(!cSuccessfulReadout)
            {
                LOG(INFO) << BOLDRED << "Problem in the readout... " << RESET;
                // have a quick look at the data
                const std::vector<Event*>& cEvents = this->GetEvents();
                LOG(INFO) << BOLDRED << "Attempt#" << +fTriggerTestCounter << "\t... Managed to read-back " << +cEvents.size() << " events "
                          << " out of " << +cNtrials << " expected." << RESET;

                // how many clusters were injected
                for(auto cBoard: *fDetectorContainer)
                {
                    auto& cCicInjc = cCicInjections.at(cBoard->getIndex());
                    for(auto cOpticalGroup: *cBoard)
                    {
                        auto& cInjections0G = cCicInjc->at(cOpticalGroup->getIndex());
                        for(auto cHybrid: *cOpticalGroup)
                        {
                            auto& cInjectionsHybrid = cInjections0G->at(cHybrid->getIndex());
                            auto& cInjCIC           = cInjectionsHybrid->getSummary<uint32_t>();
#ifdef __USE_ROOT__
                            // found
                            TH1D* cEvCounterHist = static_cast<TH1D*>(getHist(cHybrid, "EventCounter"));
                            cEvCounterHist->Fill(cInjCIC, cNtrials);
                            // missing
                            cEvCounterHist = static_cast<TH1D*>(getHist(cHybrid, "MissingEventCounter"));
                            cEvCounterHist->Fill(cInjCIC, cEvents.size());
#endif

                            LOG(INFO) << BOLDRED << "Number of injected clusters in CIC#" << +cHybrid->getId() << " is  " << +cInjCIC << RESET;
                        }
                    }
                }
                // // look at events
                // for(auto cEvent: cEvents)
                // {
                //     for( auto cBoard : *fDetectorContainer )
                //     {
                //         for(auto cOpticalGroup: *cBoard)
                //         {
                //             for(auto cHybrid: *cOpticalGroup)
                //             {
                //                 auto cL1Status = (static_cast<D19cCic2Event*>(cEvent))->L1Status(cHybrid->getId());
                //                 uint32_t cL1Id = cEvent->L1Id( cHybrid->getId(), 0 );
                //                 // LOG (INFO) << BOLDRED << "Event#" << +cEvent->GetEventCount()
                //                 //     << " CIC L1 Status is " << std::bitset<9>(cL1Status)
                //                 //     << " L1Id from CIC is " << +cL1Id
                //                 //     << " expect "
                //                 //     << RESET;
                //                 // for(auto cChip: *cHybrid)
                //                 // {
                //                 //     if( cChip->getFrontEndType() == FrontEndType::SSA ) continue;

                //                 //     auto cSClusters = (static_cast<D19cCic2Event*>(cEvent))->GetStripClusters(cHybrid->getId(), cChip->getId());
                //                 //     auto cStubs = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                //                 //     auto cPClusters = (static_cast<D19cCic2Event*>(cEvent))->GetPixelClusters(cHybrid->getId(), cChip->getId());

                //                 //     LOG (INFO) << BOLDRED << "\t..MPA#" << +cChip->getId()
                //                 //         << " found " << +cSClusters.size()
                //                 //         << " S-clusters, "
                //                 //         << +cPClusters.size()
                //                 //         << " P-clusters "
                //                 //         << " and "
                //                 //         << +cStubs.size()
                //                 //         << " stubs."
                //                 //         << RESET;
                //                 //     // for( auto cSCluster : cSClusters )
                //                 //     // {
                //                 //     //     Injection cInjection;
                //                 //     //     cInjection.fRow =  cSCluster.fAddress;// - cBend ;
                //                 //     //     cInjection.fColumn = 0;
                //                 //     //     cInjection.fFeId = cChip->getId();
                //                 //     //     LOG (INFO) << BOLDRED << "\t\t.. found S-cluster in SSA#" << +cChip->getId()
                //                 //     //         << " strip " << +cInjection.fRow
                //                 //     //         << " in readout."
                //                 //     //         << RESET ;
                //                 //     // }
                //                 //     // for( auto cPCluster : cPClusters )
                //                 //     // {
                //                 //     //     Injection cInjection;
                //                 //     //     cInjection.fRow =  cPCluster.fAddress;
                //                 //     //     cInjection.fColumn = cPCluster.fZpos;
                //                 //     //     cInjection.fFeId = cChip->getId();
                //                 //     //     LOG (DEBUG) << BOLDRED << "\t\t.. found P-cluster in MPA#" << +cChip->getId()
                //                 //     //         << " strip " << +cInjection.fRow
                //                 //     //         << " pixel column is " << +cInjection.fColumn
                //                 //     //         << " in readout."
                //                 //     //         << RESET ;
                //                 //     // }
                //                 // }
                //             }
                //         }
                //     }
                // }

                // don't attempt to match things
                // for this run
                continue;
            }
            // retrieve events
            const std::vector<Event*>& cNewEvents = this->GetEvents();
            LOG(DEBUG) << BOLDMAGENTA << "Have read-back " << +cNewEvents.size() << " events" << RESET;

// fill event counter histogram
#ifdef __USE_ROOT__
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cCicInjc = cCicInjections.at(cBoard->getIndex());
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& cInjections0G = cCicInjc->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cInjectionsHybrid = cInjections0G->at(cHybrid->getIndex());
                        auto& cInjCIC           = cInjectionsHybrid->getSummary<uint32_t>();
                        // found
                        TH1D* cEvCounterHist = static_cast<TH1D*>(getHist(cHybrid, "EventCounter"));
                        cEvCounterHist->Fill(cInjCIC, cNtrials);
                        // missing
                        cEvCounterHist = static_cast<TH1D*>(getHist(cHybrid, "MissingEventCounter"));
                        cEvCounterHist->Fill(cInjCIC, cNewEvents.size());
                    }
                }
            }
#endif

            DetectorDataContainer cStubsInReadout, cBxIdsInReadout, cL1StatusCic, cL1IdsCIC;
            ContainerFactory::copyAndInitHybrid<std::vector<float>>(*fDetectorContainer, cStubsInReadout);
            ContainerFactory::copyAndInitHybrid<std::vector<float>>(*fDetectorContainer, cBxIdsInReadout);
            ContainerFactory::copyAndInitHybrid<std::vector<uint16_t>>(*fDetectorContainer, cL1StatusCic);
            ContainerFactory::copyAndInitHybrid<std::vector<uint16_t>>(*fDetectorContainer, cL1IdsCIC);

            DetectorDataContainer cStubsPerFE;
            ContainerFactory::copyAndInitChip<std::vector<float>>(*fDetectorContainer, cStubsPerFE);
            DetectorDataContainer cHitsPerFE;
            ContainerFactory::copyAndInitChip<std::vector<float>>(*fDetectorContainer, cHitsPerFE);
            DetectorDataContainer cClustersPerFE;
            ContainerFactory::copyAndInitChip<std::vector<Injection>>(*fDetectorContainer, cClustersPerFE);
            DetectorDataContainer cStbsPerFE;
            ContainerFactory::copyAndInitChip<std::vector<Stub>>(*fDetectorContainer, cStbsPerFE);
            DetectorDataContainer cL1Cntr;
            ContainerFactory::copyAndInitChip<std::vector<uint16_t>>(*fDetectorContainer, cL1Cntr);

            for(auto cBoard: *fDetectorContainer)
            {
                auto& cReadoutStubs = cStubsInReadout.at(cBoard->getIndex());
                auto& cBxIds        = cBxIdsInReadout.at(cBoard->getIndex());
                auto& cL1Status     = cL1StatusCic.at(cBoard->getIndex());
                auto& cL1Ids        = cL1IdsCIC.at(cBoard->getIndex());
                auto& cFeStubs      = cStubsPerFE.at(cBoard->getIndex());
                auto& cFeHits       = cHitsPerFE.at(cBoard->getIndex());
                auto& cFeClstrs     = cClustersPerFE.at(cBoard->getIndex());
                auto& cFeStbs       = cStbsPerFE.at(cBoard->getIndex());
                auto& cFeL1Cntrs    = cL1Cntr.at(cBoard->getIndex());
                for(auto cEvent: cNewEvents)
                {
                    // skip the last event since I know its
                    // going to be weird by construction
                    if(cEvent->GetEventCount() == cNewEvents.size() - 1) continue;

                    LOG(DEBUG) << BOLDMAGENTA << "\t..Event#" << +cEvent->GetEventCount() << RESET;
                    for(auto cOpticalGroup: *cBoard)
                    {
                        auto& cL1StatusOG     = cL1Status->at(cOpticalGroup->getIndex());
                        auto& cL1IdsOG        = cL1Ids->at(cOpticalGroup->getIndex());
                        auto& cReadoutStubsOG = cReadoutStubs->at(cOpticalGroup->getIndex());
                        auto& cBxIdsOG        = cBxIds->at(cOpticalGroup->getIndex());
                        auto& cFeStubsOG      = cFeStubs->at(cOpticalGroup->getIndex());
                        auto& cFeHitsOG       = cFeHits->at(cOpticalGroup->getIndex());
                        auto& cFeClustersOG   = cFeClstrs->at(cOpticalGroup->getIndex());
                        auto& cFeStbsOG       = cFeStbs->at(cOpticalGroup->getIndex());
                        auto& cFeL1CntrsOG    = cFeL1Cntrs->at(cOpticalGroup->getIndex());
                        for(auto cHybrid: *cOpticalGroup)
                        {
                            //
                            auto& cL1IdsHybrid = cL1IdsOG->at(cHybrid->getIndex());
                            auto& cL1IdsSmry   = cL1IdsHybrid->getSummary<std::vector<uint16_t>>();
                            //
                            auto& cL1StatusHybrid = cL1StatusOG->at(cHybrid->getIndex());
                            auto& cL1StatusSmry   = cL1StatusHybrid->getSummary<std::vector<uint16_t>>();
                            //
                            auto& cReadoutStubsHybrid = cReadoutStubsOG->at(cHybrid->getIndex());
                            auto& cReadoutStubsSmry   = cReadoutStubsHybrid->getSummary<std::vector<float>>();
                            //
                            auto& cBxIdsHybrid = cBxIdsOG->at(cHybrid->getIndex());
                            auto& cBxIdsSmry   = cBxIdsHybrid->getSummary<std::vector<float>>();
                            //
                            auto& cFeClustersHybrid = cFeClustersOG->at(cHybrid->getIndex());
                            auto& cFeStbsHybrid     = cFeStbsOG->at(cHybrid->getIndex());
                            // clear vector if
                            // this is the first event
                            if(cEvent->GetEventCount() == 0)
                            {
                                cReadoutStubsSmry.clear();
                                cBxIdsSmry.clear();
                                cL1StatusSmry.clear();
                                cL1IdsSmry.clear();
                            }

                            size_t cNstubsInReadout = 0;
                            auto&  cFeStubsHybrid   = cFeStubsOG->at(cHybrid->getIndex());
                            auto&  cFeHitsHybrid    = cFeHitsOG->at(cHybrid->getIndex());
                            auto&  cFeL1CntrsHybrid = cFeL1CntrsOG->at(cHybrid->getIndex());

                            auto cL1Id = cEvent->L1Id(cHybrid->getId(), 0);
                            for(auto cChip: *cHybrid)
                            {
                                if(std::find(cIds.begin(), cIds.end(), cChip->getId()) == cIds.end()) continue;

                                // auto cMPAL1Error = fReadoutChipInterface->ReadChipReg(cChip, "ErrorL1");
                                // auto cErrorBit = (static_cast<D19cCic2Event*>(cEvent))->Error(cHybrid->getId(), cChip->getId());
                                auto& cFeStubsChip    = cFeStubsHybrid->at(cChip->getIndex());
                                auto& cFeStubsSmry    = cFeStubsChip->getSummary<std::vector<float>>();
                                auto& cFeHitsChip     = cFeHitsHybrid->at(cChip->getIndex());
                                auto& cFeHitsSmry     = cFeHitsChip->getSummary<std::vector<float>>();
                                auto& cFeClustersChip = cFeClustersHybrid->at(cChip->getIndex());
                                auto& cFeClusterSmry  = cFeClustersChip->getSummary<std::vector<Injection>>();
                                auto& cFeStbsChip     = cFeStbsHybrid->at(cChip->getIndex());
                                auto& cFeStbsSmry     = cFeStbsChip->getSummary<std::vector<Stub>>();
                                auto& cFeL1CntrChip   = cFeL1CntrsHybrid->at(cChip->getIndex());
                                auto& cFeL1CntrSmry   = cFeL1CntrChip->getSummary<std::vector<uint16_t>>();
                                if(cEvent->GetEventCount() == 0)
                                {
                                    cFeHitsSmry.clear();
                                    cFeClusterSmry.clear();
                                    cFeStbsSmry.clear();
                                    cFeL1CntrSmry.clear();
                                }

                                std::string cClstrType, cChipType;
                                size_t      cStubsThisFE = 0;
                                if(cChip->getFrontEndType() == FrontEndType::SSA)
                                {
                                    cClstrType      = "strip-clusters";
                                    cChipType       = "SSA";
                                    auto cSClusters = (static_cast<D19cCic2Event*>(cEvent))->GetStripClusters(cHybrid->getId(), cChip->getId());
                                    for(auto cSCluster: cSClusters)
                                    {
                                        Injection cInjection;
                                        cInjection.fRow    = cSCluster.fAddress; // - cBend ;
                                        cInjection.fColumn = 0;
                                        cInjection.fFeId   = cChip->getId();
                                        cFeL1CntrSmry.push_back(cL1Id);
                                        cFeClusterSmry.push_back(cInjection);
                                        // LOG (DEBUG) << BOLDMAGENTA << "\t\t.. found S-cluster in SSA#" << +cChip->getId()
                                        //     << " strip " << +cInjection.fRow
                                        //     << " in readout."
                                        //     << RESET ;
                                    }
                                    cFeHitsSmry.push_back(cSClusters.size());
                                }
                                else
                                {
                                    cChipType  = "MPA";
                                    cClstrType = "pixel-clusters";

                                    if(cEvent->GetEventCount() == 0) cFeStubsSmry.clear();
                                    auto cStubs = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                                    cFeStbsSmry.insert(cFeStbsSmry.end(), std::make_move_iterator(cStubs.begin()), std::make_move_iterator(cStubs.end()));
                                    cFeStubsSmry.push_back(cStubs.size());
                                    cStubsThisFE    = cFeStubsSmry[cFeStubsSmry.size() - 1];
                                    auto cPClusters = (static_cast<D19cCic2Event*>(cEvent))->GetPixelClusters(cHybrid->getId(), cChip->getId());
                                    for(auto cPCluster: cPClusters)
                                    {
                                        Injection cInjection;
                                        cInjection.fRow    = cPCluster.fAddress;
                                        cInjection.fColumn = cPCluster.fZpos;
                                        cInjection.fFeId   = cChip->getId();
                                        // LOG (DEBUG) << BOLDMAGENTA << "\t\t.. found P-cluster in MPA#" << +cChip->getId()
                                        //     << " strip " << +cInjection.fRow
                                        //     << " pixel column is " << +cInjection.fColumn
                                        //     << " in readout."
                                        //     << RESET ;
                                        cFeClusterSmry.push_back(cInjection);
                                        cFeL1CntrSmry.push_back(cL1Id);
                                    }
                                    cFeHitsSmry.push_back(cPClusters.size());
                                }
                                cNstubsInReadout += cStubsThisFE;

                                if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                                // LOG (DEBUG) << BOLDMAGENTA << "\t\t.."
                                //     << cChipType << "#" << +cChip->getId()
                                //     << "\t\t L1Id is " << +cL1Id
                                //     << "\t... found " << +cStubsThisFE
                                //     << " stubs in this event, and "
                                //     << +cFeHitsSmry[ cFeHitsSmry.size() - 1 ]
                                //     << " " << cClstrType
                                //     << RESET;
                            } // ROCs
                            // log of number of stubs
                            // in the readout
                            LOG(DEBUG) << BOLDMAGENTA << "\t.. In total have " << +cNstubsInReadout << " stubs in the readout." << RESET;
                            cReadoutStubsSmry.push_back(cNstubsInReadout);
                            // log of BxIds
                            // from event header
                            auto cBxId = cEvent->BxId(cHybrid->getId());
                            cBxIdsSmry.push_back(cBxId);
                            //
                            auto cCicL1Status = (static_cast<D19cCic2Event*>(cEvent))->L1Status(cHybrid->getId());
                            cL1StatusSmry.push_back(cCicL1Status);
                            //
                            cL1IdsSmry.push_back(cL1Id);
                        } // hybrids or CICs
                    }     // optical group loop
                }         // event loop
            }             // Board loop

            // print out loop
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cBrdLatency        = cStubLatencyPerBoard.at(cBoard->getIndex());
                auto& cStubLatency       = cBrdLatency->getSummary<uint16_t>();
                auto& cClusterInjections = cInjectionScheme.at(cBoard->getIndex());
                auto& cReadoutStubs      = cStubsInReadout.at(cBoard->getIndex());
                auto& cCicInjc           = cCicInjections.at(cBoard->getIndex());
                auto& cL1Status          = cL1StatusCic.at(cBoard->getIndex());
                auto& cBxReadout         = cBxIdsInReadout.at(cBoard->getIndex());
                auto& cFeStubs           = cStubsPerFE.at(cBoard->getIndex());
                auto& cFeLatency         = cLatencyPerFE.at(cBoard->getIndex());
                auto& cFeHits            = cHitsPerFE.at(cBoard->getIndex());
                auto& cFeClusters        = cClustersPerFE.at(cBoard->getIndex());
                auto& cFeStbs            = cStbsPerFE.at(cBoard->getIndex());
                auto& cL1Ids             = cL1IdsCIC.at(cBoard->getIndex());
                auto& cFeL1Cntrs         = cL1Cntr.at(cBoard->getIndex());
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& cL1IdsOG             = cL1Ids->at(cOpticalGroup->getIndex());
                    auto& cClusterInjectionsOG = cClusterInjections->at(cOpticalGroup->getIndex());
                    auto& cCicInjcOG           = cCicInjc->at(cOpticalGroup->getIndex());
                    auto& cReadoutStubsOG      = cReadoutStubs->at(cOpticalGroup->getIndex());
                    auto& cBxOG                = cBxReadout->at(cOpticalGroup->getIndex());
                    auto& cFeStubsOG           = cFeStubs->at(cOpticalGroup->getIndex());
                    auto& cFeLatencyOG         = cFeLatency->at(cOpticalGroup->getIndex());
                    auto& cFeHitsOG            = cFeHits->at(cOpticalGroup->getIndex());
                    auto& cFeClustersOG        = cFeClusters->at(cOpticalGroup->getIndex());
                    auto& cFeStbsOG            = cFeStbs->at(cOpticalGroup->getIndex());
                    auto& cL1StatusOG          = cL1Status->at(cOpticalGroup->getIndex());
                    auto& cFeL1CntrsOG         = cFeL1Cntrs->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cL1IdsHybrid            = cL1IdsOG->at(cHybrid->getIndex());
                        auto& cL1IdsSmry              = cL1IdsHybrid->getSummary<std::vector<uint16_t>>();
                        auto& cFeL1CntrsHybrid        = cFeL1CntrsOG->at(cHybrid->getIndex());
                        auto& cFeLatencyHybrid        = cFeLatencyOG->at(cHybrid->getIndex());
                        auto& cFeHitsHybrid           = cFeHitsOG->at(cHybrid->getIndex());
                        auto& cFeClustersHybrid       = cFeClustersOG->at(cHybrid->getIndex());
                        auto& cClusterInjectionHybrid = cClusterInjectionsOG->at(cHybrid->getIndex());
                        auto& cFeStubsHybrid          = cFeStubsOG->at(cHybrid->getIndex());
                        auto& cReadoutStubsHybrid     = cReadoutStubsOG->at(cHybrid->getIndex());
                        auto& cReadoutStubsSmry       = cReadoutStubsHybrid->getSummary<std::vector<float>>();
                        auto& cCicInjHybrid           = cCicInjcOG->at(cHybrid->getIndex());
                        auto& cCicInjSummary          = cCicInjHybrid->getSummary<uint32_t>();
                        auto& cFeStbsHybrid           = cFeStbsOG->at(cHybrid->getIndex());
                        auto& cCicL1Status            = cL1StatusOG->at(cHybrid->getIndex());
                        auto& cCicL1Stat              = cCicL1Status->getSummary<std::vector<uint16_t>>();
                        // counter clusters
                        size_t cNClusters = 0;

                        for(int cIndx = 0; cIndx < cHybrid->size(); cIndx++)
                        {
                            auto& cInjSmryChip = cClusterInjectionHybrid->at(cIndx);
                            auto& cSmry        = cInjSmryChip->getSummary<std::vector<Injection>>();
                            cNClusters += cSmry.size();
                        }
                        std::map<uint16_t, uint8_t> cMatchedMapCic;
                        for(uint16_t cL1 = 1; cL1 <= cNtrials; cL1++)
                        {
                            auto cL1MpIter = cMatchedMapCic.find(cL1);
                            if(cL1MpIter == cMatchedMapCic.end()) { cMatchedMapCic.insert(std::make_pair(cL1, 0)); }
                        }
                        size_t cEventCount = 0;
                        // check L1 status
                        for(auto cCicL1: cCicL1Stat)
                        {
                            auto cL1Id = cL1IdsSmry[cEventCount];
                            // if( cCicL1 != 0  && fTriggerTestCounter%10 == 0 )
                            //     LOG (INFO) << BOLDRED << "Attempt#"
                            //         << +fTriggerTestCounter
                            //         << " Event#" << +cEventCount
                            //         << " L1 Status from CIC is "
                            //         << std::bitset<9>(cCicL1)
                            //         << RESET;

                            if(cL1Id == 511) LOG(INFO) << BOLDRED << "HARD OVERFLOW IN CIC" << RESET;
                            uint8_t  cCicStat    = (cCicL1 & (0x1 << 0)) >> 0;
                            uint32_t cNFEsInEror = 0;
                            for(auto cChip: *cHybrid)
                            {
                                if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                                int cErrorBit = (cCicL1 & (0x1 << (1 + cChip->getIndex()))) >> (1 + cChip->getIndex());
                                cNFEsInEror += (cErrorBit == 1) ? 1 : 0;
                            }
                            // if( cCicL1 != 0  )
                            //     LOG (INFO) << BOLDRED << BOLDRED << "Attempt#"
                            //         << +fTriggerTestCounter
                            //         << " Event#" << +cEventCount
                            //         << " L1 Status from CIC is "
                            //         << std::bitset<9>(cCicL1)
                            //         << RESET;
                            for(auto cChip: *cHybrid)
                            {
                                if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                                auto&   cInjSmryChip = cClusterInjectionHybrid->at(cChip->getIndex());
                                auto&   cSmry        = cInjSmryChip->getSummary<std::vector<Injection>>();
                                int     cErrorBit    = (cCicL1 & (0x1 << (1 + cChip->getIndex()))) >> (1 + cChip->getIndex());
                                uint8_t cErrorCode   = 0;
                                if(cCicStat == 0 && cErrorBit == 0)
                                    cErrorCode = 0; // no error
                                else if(cCicStat == 0 && cErrorBit == 1)
                                    cErrorCode = 1; // FE err or. G.Overflow
                                else if(cCicStat == 1 && cErrorBit == 1)
                                    cErrorCode = 2; // S.Overflow
                                else if(cCicStat == 1 && cNFEsInEror == 0)
                                    cErrorCode = 3; // G.Overflow

                                // if( cCicL1 != 0  )
                                // {
                                //     if( cErrorBit != 0 )
                                //     {
                                //         LOG (INFO) << BOLDMAGENTA << "\t...FE#"
                                //             << +cChip->getId()
                                //             << " L1 Id " << +cL1Id
                                //             << " event counter [glbl] " << (fTriggerTestCounter*cNtrials) + cL1Id
                                //             << " CIC status bit is " << +cCicStat
                                //             << " FE status bit is " << +cErrorBit
                                //             << " number of FEs in error is " << +cNFEsInEror
                                //             << " error code is " << +cErrorCode
                                //             << RESET;
                                //     }
                                // }
                                //
                                TH2D* cMon = static_cast<TH2D*>(getHist(cChip, "L1StatusMonitorGlbl"));
                                cMon->Fill(cNClusters, cErrorCode, 1);
                                cMon = static_cast<TH2D*>(getHist(cChip, "L1StatusMonitorLcl"));
                                if(cSmry.size() != 0) cMon->Fill(cSmry.size(), cErrorCode, 1);
                                //
                                TProfile2D* cStatusMonitor        = static_cast<TProfile2D*>(getHist(cHybrid, "L1StatusMonitor"));
                                TProfile2D* cStatusMonitorClsSize = static_cast<TProfile2D*>(getHist(cHybrid, "L1StatusMonitorClusterSize"));
                                cStatusMonitor->Fill((cEventCount == 0) ? 0 : cDifferences[cEventCount + 1], cChip->getId(), 1 - cErrorBit);
                                cStatusMonitorClsSize->Fill(cNClusters, cChip->getId(), 1 - cErrorBit);
                            }
#ifdef __USE_ROOT__
                            TH2D* cL1Monitor = static_cast<TProfile2D*>(getHist(cHybrid, "L1Monitor"));
                            cL1Monitor->Fill(cEventCount, cL1Id);
                            TProfile2D* cStatusMonitor        = static_cast<TProfile2D*>(getHist(cHybrid, "L1StatusMonitor"));
                            TProfile2D* cStatusMonitorClsSize = static_cast<TProfile2D*>(getHist(cHybrid, "L1StatusMonitorClusterSize"));
                            cStatusMonitor->Fill((cEventCount == 0) ? 0 : cDifferences[cEventCount + 1], 8, 1 - cCicStat);
                            cStatusMonitorClsSize->Fill(cNClusters, 8, 1 - cCicStat);
                            // for( int cIndx=0; cIndx < 9; cIndx++)
                            // {
                            //     int cErrorBit = (cCicL1 & (0x1 << cIndx)) >> cIndx;
                            //     cStatusMonitor->Fill( (cEventCount==0) ? 0 : cDifferences[cIndx+1], cIndx, 1-cErrorBit );
                            //     cStatusMonitorClsSize->Fill( cNClusters, cIndx, 1-cErrorBit );
                            // }
#endif

                            cEventCount++;
                        }
                        //
                        auto& cBxHybrid         = cBxOG->at(cHybrid->getIndex());
                        auto& cBxSummary        = cBxHybrid->getSummary<std::vector<float>>();
                        auto  cBxDifferences    = GetBxIds(cBxSummary);
                        auto  cStubReadoutStats = getStats(cReadoutStubsSmry);
                        if(fTriggerTestCounter % 10 == 0)
                            LOG(INFO) << BOLDMAGENTA << "\t..Injected " << +cCicInjSummary << " stub in CIC#" << +cHybrid->getId() << " on  average found " << +cStubReadoutStats.first
                                      << " stubs in the readout. Stub latency set to " << +cStubLatency << RESET;
// fill histograms as needed
#ifdef __USE_ROOT__
                        TH2D* cStubCounter = static_cast<TH2D*>(getHist(cHybrid, "StubCounter"));
                        for(auto cStubCount: cReadoutStubsSmry) { cStubCounter->Fill(cCicInjSummary, cStubCount); }
                        TH2D* cBxMatching = static_cast<TH2D*>(getHist(cHybrid, "BxMatching"));
                        TH1D* cBxCounter  = static_cast<TH1D*>(getHist(cHybrid, "BxCounter"));
                        for(size_t cIndx = 0; cIndx < cBxDifferences.size(); cIndx++)
                        {
                            // LOG (DEBUG) << BOLDMAGENTA << "\t\t.." << cIndx
                            //     << " BxDifference from injection is "
                            //     << +cDifferences[cIndx]
                            //     << " from event readout is "
                            //     << +cBxDifferences[cIndx]
                            //     << RESET;
                            cBxMatching->Fill(cDifferences[cIndx], cBxDifferences[cIndx]);
                            cBxCounter->Fill(cBxDifferences[cIndx]);
                        }
#endif

                        for(auto cChip: *cHybrid)
                        {
                            if(std::find(cIds.begin(), cIds.end(), cChip->getId()) == cIds.end()) continue;

                            // stubs per FE chip
                            auto&  cFeStbsChip = cFeStbsHybrid->at(cChip->getIndex());
                            auto&  cFeStbsSmry = cFeStbsChip->getSummary<std::vector<Stub>>();
                            size_t cNstubsTtl  = cFeStbsSmry.size();
                            LOG(DEBUG) << "In total have " << +cNstubsTtl << " stubs." << RESET;

                            // N stubs per FE chip
                            auto& cFeStubsChip = cFeStubsHybrid->at(cChip->getIndex());
                            auto& cFeStubsSmry = cFeStubsChip->getSummary<std::vector<float>>();
                            float cTotalNstubs = std::accumulate(cFeStubsSmry.begin(), cFeStubsSmry.end(), 0.);

                            // Latencies per FE chip
                            auto& cFeLatencyChip = cFeLatencyHybrid->at(cChip->getIndex());
                            auto& cFeLatencySmry = cFeLatencyChip->getSummary<uint16_t>();
                            // N clusters per FE chip
                            auto& cFeHitsChip = cFeHitsHybrid->at(cChip->getIndex());
                            auto& cFeHitsSmry = cFeHitsChip->getSummary<std::vector<float>>();
                            // float cTotalNHits = std::accumulate( cFeHitsSmry.begin(), cFeHitsSmry.end() , 0.);
                            auto cNHitsStats = getStats(cFeHitsSmry);
                            // Clusters per FE chip
                            auto&       cFeClusterChip = cFeClustersHybrid->at(cChip->getIndex());
                            auto&       cFeClusterSmry = cFeClusterChip->getSummary<std::vector<Injection>>();
                            std::string cType          = (cChip->getFrontEndType() == FrontEndType::SSA) ? "SSA" : "MPA";
                            std::string cClusterType   = (cChip->getFrontEndType() == FrontEndType::SSA) ? "S-cluster" : "P-cluster";
                            int         cChipOffset    = (cChip->getFrontEndType() == FrontEndType::SSA) ? 0 : 8;

                            // LOG (INFO) << BOLDMAGENTA << "Injection in "
                            //     << cType << "#" << +cChip->getId()
                            //     << RESET;
                            // expected hits
                            std::map<uint32_t, uint16_t>                    cMatchedMap;
                            std::map<uint32_t, std::map<uint16_t, uint8_t>> cMatchedMapL1;
                            size_t                                          cExpectedNHits = 0;
                            for(size_t cIndx = 0; cIndx < cClusterInjectionHybrid->size(); cIndx++)
                            {
                                auto& cInjSmryChip = cClusterInjectionHybrid->at(cIndx);
                                auto& cInjSmry     = cInjSmryChip->getSummary<std::vector<Injection>>();
                                for(auto cClusterInj: cInjSmry)
                                {
                                    if(cClusterInj.fFeId != cChip->getId()) continue;
                                    int cBendOffset  = (cChip->getFrontEndType() == FrontEndType::SSA) ? cBend : 0;
                                    int cExpectedCol = cClusterInj.fColumn;
                                    int cExpectedRow = cClusterInj.fRow + cBendOffset;

                                    uint32_t cPixelId  = (uint32_t)(cExpectedCol * 120) + (uint32_t)cExpectedRow;
                                    auto     cIterator = cMatchedMap.find(cPixelId);
                                    if(cIterator == cMatchedMap.end())
                                    {
                                        cMatchedMap.insert(std::make_pair(cPixelId, 0));
                                        cExpectedNHits++;
                                    }

                                    auto cMatchedIter = cMatchedMapL1.find(cPixelId);
                                    if(cMatchedIter == cMatchedMapL1.end())
                                    {
                                        std::map<uint16_t, uint8_t> cL1Mp;
                                        for(uint16_t cL1 = 1; cL1 <= cNtrials; cL1++)
                                        {
                                            auto cL1MpIter = cL1Mp.find(cL1);
                                            if(cL1MpIter == cL1Mp.end()) { cL1Mp.insert(std::make_pair(cL1, 0)); }
                                        }
                                        cMatchedMapL1.insert(std::make_pair(cPixelId, cL1Mp));
                                    }
                                }
                            }

// cluster counter hist
// not matching . just counting
#ifdef __USE_ROOT__
                            TH2D* cClusterCounter = static_cast<TH2D*>(getHist(cChip, "ClusterCounter"));
                            cClusterCounter->Fill(cExpectedNHits, cNHitsStats.first);
#endif

                            // check match for all the clusters
                            // readout
                            auto& cL1SmryChip = cFeL1CntrsHybrid->at(cChip->getIndex());
                            auto& cL1s        = cL1SmryChip->getSummary<std::vector<uint16_t>>();
                            for(size_t cIndx = 0; cIndx < cClusterInjectionHybrid->size(); cIndx++)
                            {
                                auto&  cInjSmryChip = cClusterInjectionHybrid->at(cIndx);
                                auto&  cInjSmry     = cInjSmryChip->getSummary<std::vector<Injection>>();
                                size_t cInjCounter  = 0;
                                for(auto cClusterInj: cInjSmry)
                                {
                                    if(cClusterInj.fFeId != cChip->getId()) continue;

                                    int      cBendOffset    = (cChip->getFrontEndType() == FrontEndType::SSA) ? cBend : 0;
                                    int      cExpectedPxl   = cClusterInj.fColumn;
                                    int      cExpectedStrip = cClusterInj.fRow + cBendOffset;
                                    uint32_t cPixelId       = (uint32_t)(cExpectedPxl * 120) + (uint32_t)cExpectedStrip;

                                    size_t                cCntr = 0;
                                    std::vector<uint16_t> cMatchedL1Ids(0);
                                    auto                  cMatchL1MapIter = cMatchedMapL1.find(cPixelId);
                                    for(auto cFeCluster: cFeClusterSmry)
                                    {
                                        bool cStripMatch = (cExpectedStrip == cFeCluster.fRow);
                                        bool cPixelMatch = (cChip->getFrontEndType() == FrontEndType::SSA) ? true : (cExpectedPxl == cFeCluster.fColumn);
                                        bool cMatchFound = cStripMatch && cPixelMatch;
                                        if(cMatchFound) cMatchedL1Ids.push_back(cL1s[cCntr]);
                                        cMatchedMapCic.find(cL1s[cCntr])->second += (cMatchFound) ? 1 : 0;
                                        // if( !cMatchFound )

                                        if(cMatchFound) { (cMatchL1MapIter->second).find(cL1s[cCntr])->second = 1; }
                                        cCntr++;

                                    } // loop over readout clusters
                                    cMatchedMap.find(cPixelId)->second = cMatchedL1Ids.size();
                                    if(cMatchedMap.find(cPixelId)->second != cNtrials - 1)
                                        LOG(INFO) << BOLDMAGENTA << "Injection in " << cType << "#" << +cChip->getId() << " .. attempt# " << (fTriggerTestCounter) << " \t\t\t... pixelId " << cPixelId
                                                  << " "
                                                  << " strip " << cExpectedStrip << " pixel " << cExpectedPxl << ". Expect " << (cNtrials - 1) << " matches."
                                                  << " and found " << +cMatchedMap.find(cPixelId)->second << " [ found " << +cMatchedL1Ids.size() << " matched L1 Ids]" << RESET;

                                    cInjCounter++;
                                    // LOG (INFO) << BOLDMAGENTA
                                    //     << " \t\t\t... pixelId " << cPixelId
                                    //     << " strip " << cExpectedStrip
                                    //     << " pixel " << cExpectedPxl
                                    //     << ". Expect " << (cNtrials-1)
                                    //     << " matches."
                                    //     << " and found "
                                    //     << +cMatchedMap.find(cPixelId)->second
                                    //     << " [ found "
                                    //     << +cMatchedL1Ids.size()
                                    //     << " matched L1 Ids]"
                                    //     << RESET;
                                } // loop over clusters injected in this FE
                            }     // loop over all clusters inected in this hybrid

                            // print matches for clusters
                            size_t cMatchedHits = 0;
                            for(auto cMapItem: cMatchedMapL1)
                            {
                                int  cExpectedRow  = (cChip->getFrontEndType() == FrontEndType::SSA) ? 0 : (cMapItem.first) / 120;
                                int  cExpectedStrp = (cMapItem.first) % 120;
                                int  cNmatches     = cMatchedMap.find(cMapItem.first)->second;
                                bool cMatch        = (cNmatches == (int)(cNtrials - 1));
                                int  cNmismatches  = (cNtrials - 1) - (int)cNmatches;
                                cMatchedHits += (cMatch) ? 1 : 0;
// if( cMatch)
//     LOG (INFO) << BOLDGREEN << "Pixel# " << +cMapItem.first
//         << " so row " << +cExpectedRow
//         << " and strip " << +cExpectedStrp
//         << " found " << +(int)cNmatches
//         << " matched and " << cNmismatches
//         << " mismatched events out of a total "
//         << cNtrials-1
//         << " expected. "
//         << RESET;
// if(!cMatch)
//     LOG (INFO) << BOLDRED << "Attempt#"
//         << +fTriggerTestCounter
//         << " "
//         << cType
//         << "#"
//         << +cChip->getId() << " : "
//         << " Pixel# " << +cMapItem.first
//         << " so row " << +cExpectedRow
//         << " and strip " << +cExpectedStrp
//         << " found " << +(int)cNmatches
//         << " matched and " << cNmismatches
//         << " mismatched events out of a total "
//         << cNtrials-1
//         << " expected. "
//         << RESET;

// histograms
#ifdef __USE_ROOT__
                                // hit maps
                                TH2D* cInjMap = static_cast<TH2D*>(getHist(cChip, "InjectionMap"));
                                cInjMap->Fill(cExpectedRow, cExpectedStrp, cNtrials - 1); // how many I expect
                                TH2D* cMatchedInjMap = static_cast<TH2D*>(getHist(cChip, "MatchedInjectionMap"));
                                cMatchedInjMap->Fill(cExpectedRow, cExpectedStrp, cNmatches);
                                TH2D* cMismatchedInjMap = static_cast<TH2D*>(getHist(cChip, "MismatchedInjectionMap"));
                                cMismatchedInjMap->Fill(cExpectedRow, cExpectedStrp, cNmismatches);
                                // hit latencies
                                std::string cHistName   = (cChip->getFrontEndType() == FrontEndType::SSA) ? "StripHitLatency" : "PixelHitLatency";
                                TH2D*       cHitLatency = static_cast<TProfile2D*>(getHist(cHybrid, cHistName));
                                cHitLatency->Fill(cFeLatencySmry, cChip->getId(), cNmatches);
                                // cluster counter
                                TProfile2D* cClusterHist = static_cast<TProfile2D*>(getHist(cHybrid, "ClusterCounter"));
                                cClusterHist->Fill(cNClusters, cChipOffset + cChip->getId(), (cMatch) ? 1 : 0);
                                // L1 Id map
                                TH2D*       cL1Hist        = static_cast<TH2D*>(getHist(cChip, "L1Monitor"));
                                TH2D*       cL1MatchedHist = static_cast<TH2D*>(getHist(cChip, "L1MonitorMatched"));
                                TProfile2D* cL1MatchedProf = static_cast<TProfile2D*>(getHist(cChip, "L1MonitorProfile"));
                                TProfile2D* cEvMatchedProf = static_cast<TProfile2D*>(getHist(cChip, "EvntMonitorProfile"));
                                TProfile*   cMismatchProf  = static_cast<TProfile*>(getHist(cChip, "MismatchProfile"));
                                // this will show that
                                // the last event is always 'empty'
                                cNmismatches = 0;
                                for(uint16_t cL1 = 1; cL1 <= cNtrials; cL1++)
                                {
                                    cL1Hist->Fill(cL1, cNClusters, 1);
                                    if(cMapItem.second.find(cL1) != cMapItem.second.end())
                                    {
                                        if(cMapItem.second.find(cL1)->second == 0 && cNmismatches == 0)
                                        {
                                            // if( cL1 != cNtrials )
                                            cMismatchProf->Fill(cNClusters, cL1);
                                            // first mismatch
                                        }
                                        cNmismatches += (cMapItem.second.find(cL1)->second == 0);
                                        cMatch = (cMapItem.second.find(cL1)->second == 1);
                                        cL1MatchedHist->Fill(cL1, cNClusters, cMapItem.second.find(cL1)->second);
                                        cL1MatchedProf->Fill(cL1, cNClusters, cMapItem.second.find(cL1)->second);
                                        if(fTriggerTestCounter * cNtrials + cL1 < 5000) { cEvMatchedProf->Fill(fTriggerTestCounter * cNtrials + cL1, cNClusters, cMapItem.second.find(cL1)->second); }
                                    }
                                }
#endif
                            }
// bool cAllReadoutMatch = (cMatchedHits == cTotalNHits );
// if( cAllReadoutMatch )
//     LOG (DEBUG) << "All readout "
//         << cClusterType
//         << " match those injected for "
//         << cType
//         << "#"
//         << +cChip->getId()
//         << RESET;

// stubs
// latency hists
#ifdef __USE_ROOT__
                            TH2D* cStubLatencyHist = static_cast<TH2D*>(getHist(cHybrid, "StubLatency"));
                            cStubLatencyHist->Fill(cStubLatency, cChip->getId(), cTotalNstubs);
#endif

                            if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

#ifdef __USE_ROOT__
                            // Injections per FE chip
                            auto&       cClusterInjChip = cClusterInjectionHybrid->at(cChip->getIndex());
                            auto&       cClusterInjSmry = cClusterInjChip->getSummary<std::vector<Injection>>();
                            TProfile2D* cStubCounterId  = static_cast<TProfile2D*>(getHist(cHybrid, "StubCounterIds"));
                            TProfile2D* cStubCounter    = static_cast<TProfile2D*>(getHist(cChip, "StubCounter"));
                            TH2D*       cStubCounter2   = static_cast<TH2D*>(getHist(cChip, "StubCounter2"));
                            for(auto cNstub: cFeStubsSmry)
                            {
                                cStubCounter->Fill(cCicInjSummary, cClusterInjSmry.size(), cNstub / cClusterInjSmry.size());
                                cStubCounter2->Fill(cClusterInjSmry.size(), cNstub);
                                cStubCounterId->Fill(cCicInjSummary, cChip->getId(), cNstub / cClusterInjSmry.size());
                            }
#endif
                        }

#ifdef __USE_ROOT__
                        TProfile2D* cL1MatchedProf = static_cast<TProfile2D*>(getHist(cHybrid, "L1MonitorProfile"));
                        TProfile2D* cEvMatchedProf = static_cast<TProfile2D*>(getHist(cHybrid, "EvntMonitorProfile"));
                        if(cNClusters == 0) continue;
                        for(auto cMapItem: cMatchedMapCic)
                        {
                            // // because one in each SSA-MPA pair
                            // if( cMapItem.second != 2*cNClusters && cMapItem.first != cNtrials)
                            //     LOG (INFO) << BOLDRED << "Attempt#"
                            //         << +fTriggerTestCounter
                            //         << " "
                            //         << " L1 " << +cMapItem.first
                            //         << "  found  "
                            //         << +cMapItem.second
                            //         << " matched readout clusters when "
                            //         << 2*cNClusters
                            //         << " were expected "
                            //         << RESET;
                            if(fTriggerTestCounter * cNtrials + cMapItem.first < 5000)
                                cEvMatchedProf->Fill(fTriggerTestCounter * cNtrials + cMapItem.first, cNClusters, cMapItem.second / (float)(2 * cNClusters));
                            cL1MatchedProf->Fill(cMapItem.first, cNClusters, cMapItem.second / (float)(2 * cNClusters));
                        }
#endif
                    }
                }
            } // board
        }
    } // latency loop
}
// first I would just like to test the nominal
// data transmission
// before scanning the eye
void DataChecker::PSNominal()
{
    size_t cMaxNstubs = 16;
    LOG(INFO) << BOLDBLUE << "Nominal PS data checker ... inject data from MPA --> CIC --> back-end" << RESET;
    int      cLatencyOffset = -1;
    auto     cSetting       = fSettingsMap.find("Nevents");
    uint32_t cNevents       = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    cSetting                = fSettingsMap.find("Attempts");
    uint32_t cMaxAttempts   = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 10;
    cSetting                = fSettingsMap.find("SLVSDrive");
    uint8_t cSLVSDrive      = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 3;
    cSetting                = fSettingsMap.find("MaxOffset");
    int cMaxOffset          = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 5;

    // configure clusters
    cSetting            = fSettingsMap.find("MaxPClusters");
    size_t nMaxClusters = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 2;

    LOG(DEBUG) << BOLDBLUE << "ReadNEvents data test with " << +cNevents << RESET;

    uint8_t cStubWindow = 1; // stub window in half pixels (1)
    uint8_t cMode       = 2; // (0) pixel-strip, (1) strip-strip, (2) pixel-pixel, (3) strip-pixel

    // random c++
    std::srand(std::time(NULL));
    std::random_device cRndm{};
    std::mt19937       cGen{cRndm()};

    // configure latencies
    // L1 latency in MPA
    // stub latency in FW
    auto cStubOffset = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->getStubOffset();
    for(auto cBoard: *fDetectorContainer)
    {
        // check trigger source
        // and reload
        uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
        cTriggerSrc          = (cTriggerSrc == 6) ? cTriggerSrc : 6;
        LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
        cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);

        uint16_t cDelay   = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
        uint16_t cLatency = cDelay + cLatencyOffset;
        LOG(DEBUG) << BOLDBLUE << "Setting L1 latency in MPA to " << +cLatency << RESET;
        int cReTimeValue = -1;
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                for(auto cChip: *cHybrid) // for each chip (makes sense)
                {
                    if(cChip->getFrontEndType() == FrontEndType::MPA)
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency);
                        if(cChip->getFrontEndType() == FrontEndType::MPA && cReTimeValue < 0) { cReTimeValue = fReadoutChipInterface->ReadChipReg(cChip, "RetimePix"); }
                    }
                } // chip
            }     // hybrid
        }         // module
        int cStubLatency = cLatency - (cStubOffset + cReTimeValue);
        LOG(INFO) << BOLDBLUE << "Setting L1 latency to " << +cLatency << " and stub latency to " << +cStubLatency << RESET;
        // read events
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);

        auto cPackageDelay = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
        LOG(INFO) << BOLDMAGENTA << "Package delay set to " << +cPackageDelay << RESET;
        //(static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->Bx0Alignment();
    } // boards

    // configure SLVS drive
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                for(auto cChip: *cHybrid) // for each chip (makes sense)
                {
                    if(cChip->getFrontEndType() == FrontEndType::MPA)
                    {
                        if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
                        fReadoutChipInterface->WriteChipReg(cChip, "SLVSDrive", cSLVSDrive);
                    }
                } // chip
            }     // hybrid
        }         // module
    }             // boards

    // generate injections in this MPA
    std::uniform_int_distribution<int> cFlatDistCols(2, 13);
    std::uniform_int_distribution<int> cFlatDistRows(2, 118); // avoid colums 1 and 120
    std::uniform_int_distribution<int> cNClusterDist(1, nMaxClusters);
    std::uniform_int_distribution<int> cMPAsDist(1, 8);
    std::uniform_int_distribution<int> cMPAIdDist(0, 7);

    // original phase taps
    DetectorDataContainer fOriginalPhaseTaps;
    ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, fOriginalPhaseTaps);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& fTapsOrig = fOriginalPhaseTaps.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& fTapsOrigOG = fTapsOrig->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& fTapsOrigHybrid = fTapsOrigOG->at(cHybrid->getIndex());
                auto& cCic            = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                // auto cOptimalTaps = fCicInterface->GetOptimalTaps(cCic);
                // size_t cPhyPort=0;
                // size_t cPhyPortChnl=0;
                // size_t cCounter=0;
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                    std::string cOutput = "";
                    char        cBuffer[80];
                    // first all the stub lines
                    // for(uint8_t cInput = 0; cInput < 5; cInput += 1)
                    // {
                    //     sprintf(cBuffer, "%.2d ", cOptimalTaps[cPhyPortChnl][cPhyPort]);
                    //     cOutput += cBuffer;
                    //     cPhyPort = ( (cCounter+1)%4 == 0 ) ? (cPhyPort+1) : cPhyPort;
                    //     cPhyPortChnl = cCounter%4;
                    //     cCounter++;
                    // }
                    // then the L1 line
                    size_t cPhyPortL1                    = (cChip->getId() > 3) ? 11 : 10;
                    size_t cPhyPortChnlL1                = (cChip->getId() % 4);
                    auto&  fTapsOrigChip                 = fTapsOrigHybrid->at(cChip->getIndex());
                    auto   cOptimalTaps                  = fCicInterface->GetOptimalTaps(cCic);
                    fTapsOrigChip->getSummary<uint8_t>() = cOptimalTaps[cPhyPortChnlL1][cPhyPortL1];
                    sprintf(cBuffer, "%.2d ", fTapsOrigChip->getSummary<uint8_t>());
                    cOutput += cBuffer;
                    LOG(INFO) << BOLDBLUE << "Optimal tap found on FE" << +cChip->getId() << " : " << cOutput << RESET;
                }
            } // hybrid
        }     // optical group
    }         // board

    // set offsets on CICs
    bool             cModifyL1Phase = true;
    std::vector<int> cOffsets{0};
    if(cModifyL1Phase)
    {
        for(int cExtraOffset = 1; cExtraOffset <= cMaxOffset; cExtraOffset++)
        {
            cOffsets.push_back(cExtraOffset);
            cOffsets.push_back(cExtraOffset * -1);
        }
    }
    int cDebugPrintOut = 10;
    for(auto cOffset: cOffsets)
    {
        LOG(INFO) << BOLDMAGENTA << "Offset from optimal phase of " << +cOffset << " taps." << RESET;
        DetectorDataContainer fPhaseTaps;
        ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, fPhaseTaps);
        for(auto cBoard: *fDetectorContainer)
        {
            auto& fTaps     = fPhaseTaps.at(cBoard->getIndex());
            auto& fTapsOrig = fOriginalPhaseTaps.at(cBoard->getIndex());
            for(auto cOpticalGroup: *cBoard)
            {
                auto& fTapsOG     = fTaps->at(cOpticalGroup->getIndex());
                auto& fTapsOrigOG = fTapsOrig->at(cOpticalGroup->getIndex());
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& fTapsHybrid     = fTapsOG->at(cHybrid->getIndex());
                    auto& fTapsOrigHybrid = fTapsOrigOG->at(cHybrid->getIndex());
                    auto& cCic            = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    // auto cOptimalTaps = fCicInterface->GetOptimalTaps(cCic);
                    // size_t cPhyPort=0;
                    // size_t cPhyPortChnl=0;
                    // size_t cCounter=0;
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                        std::string cOutput = "";
                        // first all the stub lines
                        // for(uint8_t cInput = 0; cInput < 5; cInput += 1)
                        // {
                        //     sprintf(cBuffer, "%.2d ", cOptimalTaps[cPhyPortChnl][cPhyPort]);
                        //     cOutput += cBuffer;
                        //     cPhyPort = ( (cCounter+1)%4 == 0 ) ? (cPhyPort+1) : cPhyPort;
                        //     cPhyPortChnl = cCounter%4;
                        //     cCounter++;
                        // }
                        // then the L1 line
                        size_t cPhyPortL1     = (cChip->getId() > 3) ? 11 : 10;
                        size_t cPhyPortChnlL1 = (cChip->getId() % 4);
                        auto&  fTapsChip      = fTapsHybrid->at(cChip->getIndex());
                        auto&  fTapsOrigChip  = fTapsOrigHybrid->at(cChip->getIndex());
                        int    cPhase         = fTapsOrigChip->getSummary<uint8_t>();
                        int    cPhaseMod      = cPhase + cOffset;
                        fCicInterface->SetOptimalTap(cCic, cPhyPortL1, cPhyPortChnlL1, cOffset);
                        cPhaseMod                        = (cPhaseMod < 0 || cPhaseMod > 0xF) ? cPhase : cPhaseMod;
                        fTapsChip->getSummary<uint8_t>() = cPhaseMod;
                    }
                } // hybrid
            }     // optical group
        }         // board

        for(size_t cAttempt = 0; cAttempt < cMaxAttempts; cAttempt++)
        {
            if(cAttempt % cDebugPrintOut == 0) LOG(INFO) << BOLDBLUE << "Attempt#" << +cAttempt << RESET;

            // prepare data container
            // zero what needs zeroing

            DetectorDataContainer fBxIds, fCicInjections;
            ContainerFactory::copyAndInitHybrid<std::vector<uint32_t>>(*fDetectorContainer, fBxIds);
            ContainerFactory::copyAndInitHybrid<uint32_t>(*fDetectorContainer, fCicInjections);
            DetectorDataContainer fInjectedPClusters, fReadoutPClusters;
            ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fInjectedPClusters);
            ContainerFactory::copyAndInitChip<std::vector<uint32_t>>(*fDetectorContainer, fReadoutPClusters);
            DetectorDataContainer fPixelInjections, fPixelExpected;
            ContainerFactory::copyAndInitChip<std::vector<Injection>>(*fDetectorContainer, fPixelInjections);
            ContainerFactory::copyAndInitChip<std::vector<Injection>>(*fDetectorContainer, fPixelExpected);
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cBxIds      = fBxIds.at(cBoard->getIndex());
                auto& cInjections = fInjectedPClusters.at(cBoard->getIndex());
                auto& cMatched    = fReadoutPClusters.at(cBoard->getIndex());
                auto& cInj        = fPixelInjections.at(cBoard->getIndex());
                auto& cExp        = fPixelExpected.at(cBoard->getIndex());
                auto& cCicInj     = fCicInjections.at(cBoard->getIndex());
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());
                    auto& cMatchedOGs             = cMatched->at(cOpticalGroup->getIndex());
                    auto& cInjOG                  = cInj->at(cOpticalGroup->getIndex());
                    auto& cExpOG                  = cExp->at(cOpticalGroup->getIndex());
                    auto& cBxIdsOG                = cBxIds->at(cOpticalGroup->getIndex());
                    auto& cCicInjOG               = cCicInj->at(cOpticalGroup->getIndex());

                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cExpHybrid        = cExpOG->at(cHybrid->getIndex());
                        auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
                        auto& cMatchedHybrid    = cMatchedOGs->at(cHybrid->getIndex());
                        auto& cInjHybrid        = cInjOG->at(cHybrid->getIndex());
                        auto& cBxIdsHybrid      = cBxIdsOG->at(cHybrid->getIndex());
                        auto& cSummaryBxIds     = cBxIdsHybrid->getSummary<std::vector<uint32_t>>();
                        cSummaryBxIds.clear();
                        auto& cCicInjHybrid                   = cCicInjOG->at(cHybrid->getIndex());
                        cCicInjHybrid->getSummary<uint32_t>() = 0;
                        for(auto cChip: *cHybrid)
                        {
                            auto& cInjectionsChip                   = cInjectionsHybrid->at(cChip->getIndex());
                            cInjectionsChip->getSummary<uint32_t>() = 0;
                            auto& cMatchedChip                      = cMatchedHybrid->at(cChip->getIndex());
                            auto& cSummary                          = cMatchedChip->getSummary<std::vector<uint32_t>>();
                            cSummary.clear();
                            auto& cInjChip    = cInjHybrid->at(cChip->getIndex());
                            auto& cSummaryInj = cInjChip->getSummary<std::vector<Injection>>();
                            cSummaryInj.clear();
                            auto& cExpChip    = cExpHybrid->at(cChip->getIndex());
                            auto& cSummaryExp = cExpChip->getSummary<std::vector<Injection>>();
                            cSummaryExp.clear();
                        }
                    }
                }
            }

            // set-up MPA for injection
            size_t cTotalNumberOfClusters = 0;
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cInjections = fInjectedPClusters.at(cBoard->getIndex());
                auto& cInj        = fPixelInjections.at(cBoard->getIndex());
                auto& cCicInj     = fCicInjections.at(cBoard->getIndex());
                for(auto cOpticalReadout: *cBoard)
                {
                    auto& cInjectionsOpticalGroup = cInjections->at(cOpticalReadout->getIndex());
                    auto& cInjOG                  = cInj->at(cOpticalReadout->getIndex());
                    auto& cCicInjOG               = cCicInj->at(cOpticalReadout->getIndex());
                    for(auto cHybrid: *cOpticalReadout)
                    {
                        auto&            cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
                        auto&            cInjHybrid        = cInjOG->at(cHybrid->getIndex());
                        auto&            cCicInjHybrid     = cCicInjOG->at(cHybrid->getIndex());
                        auto&            cCicInjSummary    = cCicInjHybrid->getSummary<uint32_t>();
                        size_t           cNMPAs            = cMPAsDist(cGen);
                        std::vector<int> cMPAs;
                        cMPAs.clear();
                        do {
                            int cMPA = cMPAIdDist(cGen);
                            if(std::find(cMPAs.begin(), cMPAs.end(), cMPA) == cMPAs.end()) { cMPAs.push_back(cMPA); }

                        } while(cMPAs.size() < cNMPAs);
                        if(cMPAs.size() == 0) continue;

                        if(cAttempt % cDebugPrintOut == 0) LOG(INFO) << BOLDMAGENTA << "Injecting clusters in " << +cMPAs.size() << " MPAs." << RESET;
                        for(auto cChip: *cHybrid) // for each chip (makes sense)
                        {
                            // for the moment - only written for CBC3
                            if(cChip->getFrontEndType() == FrontEndType::MPA)
                            {
                                if(std::find(cMPAs.begin(), cMPAs.end(), cChip->getId()) == cMPAs.end()) continue;

                                auto& cInjectionsChip = cInjectionsHybrid->at(cChip->getIndex());
                                auto& cInjChp         = cInjHybrid->at(cChip->getIndex());
                                // activate stub mode
                                fReadoutChipInterface->WriteChipReg(cChip, "StubMode", cMode);
                                fReadoutChipInterface->WriteChipReg(cChip, "StubWindow", cStubWindow);
                                //
                                std::vector<uint8_t>   cColumns(0); // 5 , 10 };
                                std::vector<uint8_t>   cRows(0);    // 20 , 30};
                                std::vector<Injection> cInjections(0);
                                size_t                 cNclstrs = cNClusterDist(cGen);
                                // this is because each cluster becomes a stub ..
                                // mask all pixels first
                                (static_cast<PSInterface*>(fReadoutChipInterface))->WriteChipReg(cChip, "DigitalSync", 0x00);

                                if(cNclstrs == 0 || cTotalNumberOfClusters >= cMaxNstubs) continue;

                                std::vector<uint32_t> cPixelIds(0); // these will be used to generate stubs
                                auto&                 cSummaryInj = cInjChp->getSummary<std::vector<Injection>>();
                                do {
                                    Injection cInjection;
                                    cInjection.fColumn = (cFlatDistCols(cGen));
                                    cInjection.fRow    = (cFlatDistRows(cGen));
                                    uint32_t cPixelId  = (uint32_t)(cInjection.fColumn) * 120 + (uint32_t)cInjection.fRow;
                                    if(std::find(cPixelIds.begin(), cPixelIds.end(), cPixelId) == cPixelIds.end())
                                    {
                                        // check if the pixel is displaced by at least 4 pixels
                                        bool cTooClose = false;
                                        for(size_t cIndx = 0; cIndx < cInjections.size(); cIndx++)
                                        {
                                            // if( cInjection.fColumn == cInjections[cIndx].fColumn )
                                            //{
                                            cTooClose = cTooClose || std::fabs(cInjection.fRow - cInjections[cIndx].fRow) < 5;
                                            //}

                                            // if( cInjection.fRow == cInjections[cIndx].fRow )
                                            //{
                                            cTooClose = cTooClose || std::fabs(cInjection.fColumn - cInjections[cIndx].fColumn) < 5;
                                            //}
                                        }
                                        if(!cTooClose && cTotalNumberOfClusters < cMaxNstubs)
                                        {
                                            LOG(DEBUG) << BOLDMAGENTA << "MPA#" << +cChip->getId() << " injecting in row " << +cInjection.fRow << " columnn " << +cInjection.fColumn << RESET;
                                            cPixelIds.push_back(cPixelId);
                                            cInjections.push_back(cInjection);
                                            cSummaryInj.push_back(cInjection);
                                            cTotalNumberOfClusters += 1;
                                        }
                                    }
                                } while(cPixelIds.size() < cNclstrs && cTotalNumberOfClusters < cMaxNstubs); // create injection patterns
                                cInjectionsChip->getSummary<uint32_t>() = cInjections.size();
                                cCicInjSummary += cInjections.size();
                                if(cAttempt % cDebugPrintOut == 0)
                                    LOG(INFO) << BOLDBLUE << "Injecting " << +cInjectionsChip->getSummary<uint32_t>() << " clusters/stubs in MPA#" << +cChip->getId() << RESET;
                                (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, cInjections);
                            }
                        } // chip
                    }     // hybrid
                }         // module
            }             // boards
            if(cAttempt % cDebugPrintOut == 0) LOG(INFO) << BOLDMAGENTA << "In total have " << +cTotalNumberOfClusters << " clusters in this run." << RESET;

            // read events
            DetectorDataContainer fPClusters;
            ContainerFactory::copyAndInitChip<std::vector<Injection>>(*fDetectorContainer, fPClusters);
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cExp        = fPixelExpected.at(cBoard->getIndex());
                auto& cPClusters  = fPClusters.at(cBoard->getIndex());
                auto& cBxIds      = fBxIds.at(cBoard->getIndex());
                auto& cInjections = fReadoutPClusters.at(cBoard->getIndex());
                LOG(DEBUG) << BOLDMAGENTA << "Requesting " << +cNevents << " events from the board " << RESET;
                ReadNEvents(cBoard, cNevents);
                const std::vector<Event*>& cEventsWithStubs = this->GetEvents();
                LOG(DEBUG) << BOLDBLUE << "Read back " << +cEventsWithStubs.size() << " events from the FC7 ..." << RESET;
                auto& cInj = fPixelInjections.at(cBoard->getIndex());
                for(auto cEvent: cEventsWithStubs)
                {
                    // skip the last event since I know its
                    // going to be weird by construction
                    if(cEvent->GetEventCount() == cNevents - 1) continue;

                    for(auto cOpticalGroup: *cBoard)
                    {
                        auto& cExpOG                  = cExp->at(cOpticalGroup->getIndex());
                        auto& cPClustersOG            = cPClusters->at(cOpticalGroup->getIndex());
                        auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());
                        auto& cInjOG                  = cInj->at(cOpticalGroup->getIndex());
                        auto& cBxIdsOG                = cBxIds->at(cOpticalGroup->getIndex());

                        for(auto cHybrid: *cOpticalGroup)
                        {
                            auto& cExpHybrid        = cExpOG->at(cHybrid->getIndex());
                            auto& cPClustersHybrid  = cPClustersOG->at(cHybrid->getIndex());
                            auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
                            auto& cInjHybrid        = cInjOG->at(cHybrid->getIndex());
                            auto  cL1Status         = (static_cast<D19cCic2Event*>(cEvent))->L1Status(cHybrid->getId());
                            auto  cErrorBitCic      = (static_cast<D19cCic2Event*>(cEvent))->Error(cHybrid->getId(), 8);
                            auto& cBxIdsHybrid      = cBxIdsOG->at(cHybrid->getIndex());
                            auto& cSummaryBxIds     = cBxIdsHybrid->getSummary<std::vector<uint32_t>>();
                            auto  cBxId             = cEvent->BxId(cHybrid->getId());
                            cSummaryBxIds.push_back(cBxId);
                            for(auto cChip: *cHybrid)
                            {
                                if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                                auto& cPClustersChip   = cPClustersHybrid->at(cChip->getIndex());
                                auto& cPClustersSmry   = cPClustersChip->getSummary<std::vector<Injection>>();
                                auto& cInjChp          = cInjHybrid->at(cChip->getIndex());
                                auto& cSummaryInj      = cInjChp->getSummary<std::vector<Injection>>();
                                auto& cExpChip         = cExpHybrid->at(cChip->getIndex());
                                auto& cExpPClusterSmry = cExpChip->getSummary<std::vector<Injection>>();

                                // don't bother checking when I havne't injected
                                if(cSummaryInj.size() == 0) continue;

                                auto     cMPAL1Error = fReadoutChipInterface->ReadChipReg(cChip, "ErrorL1");
                                uint32_t cL1Id       = cEvent->L1Id(cHybrid->getId(), cChip->getId());
                                if(cChip->getId() == 0) LOG(DEBUG) << BOLDMAGENTA << "Event#" << +cEvent->GetEventCount() << " L1Id " << +cL1Id << RESET;
                                auto cStubs     = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                                auto cPClusters = (static_cast<D19cCic2Event*>(cEvent))->GetPixelClusters(cHybrid->getId(), cChip->getId());
                                auto cSClusters = (static_cast<D19cCic2Event*>(cEvent))->GetStripClusters(cHybrid->getId(), cChip->getId());
                                auto cErrorBit  = (static_cast<D19cCic2Event*>(cEvent))->Error(cHybrid->getId(), cChip->getId());
                                // check pixel clusters
                                size_t cNmatched = 0;

                                for(auto cM: cSummaryInj)
                                {
                                    bool      cFound = false;
                                    Injection cInjection;
                                    cInjection.fRow    = cM.fRow;
                                    cInjection.fColumn = cM.fColumn;
                                    cExpPClusterSmry.push_back(cInjection);
                                    for(auto cPCluster: cPClusters)
                                    {
                                        if(cPCluster.fAddress == cM.fRow && cPCluster.fZpos == cM.fColumn) cFound = true;
                                    }
                                    cNmatched += (cFound) ? 1 : 0;
                                    if(!cFound)
                                        LOG(DEBUG) << BOLDRED << "MPA#" << +cChip->getId() << "\t\t\t\t L1Id " << +cL1Id << " MISSING PCluster : address : " << unsigned(cM.fRow) << ", row "
                                                   << unsigned(cM.fColumn) << " FE status bit from is " << +cErrorBit << " CIC status bit is " << +cErrorBitCic << std::bitset<9>(cL1Status & 0x1FF)
                                                   << " L1 error is " << std::bitset<8>(+cMPAL1Error) << RESET;
                                    else
                                        LOG(DEBUG) << BOLDGREEN << "MPA#" << +cChip->getId() << "\t\t\t\t L1Id " << +cL1Id << " FOUND PCluster : address : " << unsigned(cM.fRow) << ", row "
                                                   << unsigned(cM.fColumn) << " FE status bit from is " << +cErrorBit << " CIC status bit is " << +cErrorBitCic << std::bitset<9>(cL1Status & 0x1FF)
                                                   << " L1 error is " << std::bitset<8>(+cMPAL1Error) << " BxId is " << +cSummaryBxIds[cSummaryBxIds.size() - 1] << " " << +cBxId << RESET;
                                }

                                for(auto cPCluster: cPClusters)
                                {
                                    Injection cInjection;
                                    cInjection.fRow    = cPCluster.fAddress;
                                    cInjection.fColumn = cPCluster.fZpos;
                                    cPClustersSmry.push_back(cInjection);
                                }
                                // LOG (INFO) << BOLDMAGENTA  << "MPA#" << +cChip->getId()
                                //     << " have found "
                                //     << +cPClusters.size()
                                //     << " P-clusters out of "
                                //     << +cSummaryInj.size()
                                //     << " injected... of those "
                                //     << +cNmatched
                                //     << " match."
                                //     << RESET;
                                auto& cInjectionsThisChip = cInjectionsHybrid->at(cChip->getIndex());
                                auto& cSummary            = cInjectionsThisChip->getSummary<std::vector<uint32_t>>();
                                cSummary.push_back(cNmatched);
                                // if( (1+cEvent->GetEventCount())%100 ==  0 )
                                //{
                                LOG(DEBUG) << BOLDMAGENTA << "Event#" << +cEvent->GetEventCount() << "\t\tMPA#" << +cChip->getId() << "\t... found the " << +cStubs.size() << " stubs in this event, "
                                           << +cPClusters.size() << " pixel clusters and " << +cSClusters.size() << " strip clusters "
                                           << " summary has " << +cSummary.size() << " entries "
                                           << " p-cluster readout summary has " << +cPClustersSmry.size() << " entries "
                                           << " p-cluster injection summary has " << +cExpPClusterSmry.size() << " entries. "
                                           << " BxId is " << +cSummaryBxIds[cSummaryBxIds.size() - 1] << " " << +cBxId << RESET;
                                //}

                                // check stubs are where you put them
                                // not checking bend for now
                                size_t cNMatchedStubs = 0;
                                for(auto cStub: cStubs)
                                {
                                    auto     cStubAddress = cStub.getPosition();
                                    auto     cRow         = cStub.getRow();
                                    uint32_t cPixelId     = (cStubAddress / 2) + cRow * 120;
                                    bool     cMatch       = false;
                                    for(auto cInj: cSummaryInj)
                                    {
                                        uint32_t cId = (uint32_t)(cInj.fColumn) * 120 + (uint32_t)cInj.fRow;
                                        if(cPixelId == cId) cMatch = true;
                                    }
                                    if(cMatch)
                                        LOG(DEBUG) << "Matched stub "
                                                   << " address " << +cStubAddress << " row " << +cRow << RESET;
                                    cNMatchedStubs += (cMatch) ? 1 : 0;
                                }
                            } // ROCs
                        }     // hybrids or CICs
                    }         // optical group loop
                }             // event loop
            }

            // summarize results
            // hit maps
            // injected - fPixelExpected
            // readout - fPClusters
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cInjected = fPixelExpected.at(cBoard->getIndex());
                auto& cReadout  = fPClusters.at(cBoard->getIndex());
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& cInjOG     = cInjected->at(cOpticalGroup->getIndex());
                    auto& cReadoutOG = cReadout->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cInjHybrid     = cInjOG->at(cHybrid->getIndex());
                        auto& cReadoutHybrid = cReadoutOG->at(cHybrid->getIndex());
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                            auto& cInjChip = cInjHybrid->at(cChip->getIndex());
                            auto& cExpInjs = cInjChip->getSummary<std::vector<Injection>>();

                            auto& cReadoutChip = cReadoutHybrid->at(cChip->getIndex());
                            auto& cReadoutInjs = cReadoutChip->getSummary<std::vector<Injection>>();

                            LOG(DEBUG) << BOLDMAGENTA << "MPA#" << +cChip->getId() << " found " << +cExpInjs.size() << " in injection log "
                                       << " and " << +cReadoutInjs.size() << " in readout log " << RESET;

                            // injection map
                            for(auto cInj: cExpInjs)
                            {
#ifdef __USE_ROOT__
                                TH2D* cInjMap = static_cast<TH2D*>(getHist(cChip, "InjectionMap"));
                                cInjMap->Fill(cInj.fColumn, cInj.fRow, 1);
#endif
                            }

                            // readout map
                            for(auto cInj: cReadoutInjs)
                            {
#ifdef __USE_ROOT__
                                TH2D* cInjMapMatched = static_cast<TH2D*>(getHist(cChip, "MatchedInjectionMap"));
                                cInjMapMatched->Fill(cInj.fColumn, cInj.fRow, 1);
#endif
                            }
                        }
                    }
                }
            }

            // summarize results
            // bx Id
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cBxIds = fBxIds.at(cBoard->getIndex());
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& cBxIdsOG = cBxIds->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto&    cBxIdsHybrid  = cBxIdsOG->at(cHybrid->getIndex());
                        auto&    cSummaryBxIds = cBxIdsHybrid->getSummary<std::vector<uint32_t>>();
                        int      cNRollOvers   = 0;
                        uint16_t cMaxBxCounter = 3564;
                        for(size_t cIndx = 1; cIndx < cSummaryBxIds.size(); cIndx++)
                        {
                            int cBxDifference = (cNRollOvers)*cMaxBxCounter + (cSummaryBxIds[cIndx - 1] % cMaxBxCounter);
                            cNRollOvers += ((cSummaryBxIds[cIndx - 1] >= 2500) && (cSummaryBxIds[cIndx - 1] < cMaxBxCounter)) && (cSummaryBxIds[cIndx] < cSummaryBxIds[cIndx - 1]) ? 1 : 0;
                            cBxDifference = (cNRollOvers)*cMaxBxCounter + (cSummaryBxIds[cIndx] % cMaxBxCounter) - cBxDifference;
                            LOG(DEBUG) << BOLDMAGENTA << "\t.. Bx Difference is " << +cBxDifference << RESET;
#ifdef __USE_ROOT__
                            TH1D* cBxDiffHist = static_cast<TH1D*>(getHist(cHybrid, "BxIdDifference"));
                            cBxDiffHist->Fill(cBxDifference);
#endif
                        }
                    } // hybrids or CICs
                }     // optical group loop
            }

            for(auto cBoard: *fDetectorContainer)
            {
                auto& cCicInj   = fCicInjections.at(cBoard->getIndex());
                auto& cInjected = fPixelExpected.at(cBoard->getIndex());
                auto& cReadout  = fPClusters.at(cBoard->getIndex());
                auto& cTaps     = fPhaseTaps.at(cBoard->getIndex());
                auto& cTapsOrig = fOriginalPhaseTaps.at(cBoard->getIndex());
                auto& cClusters = fInjectedPClusters.at(cBoard->getIndex());
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& cCicInjOG             = cCicInj->at(cOpticalGroup->getIndex());
                    auto& cInjOG                = cInjected->at(cOpticalGroup->getIndex());
                    auto& cReadoutOG            = cReadout->at(cOpticalGroup->getIndex());
                    auto& cTapsOpticalGroup     = cTaps->at(cOpticalGroup->getIndex());
                    auto& cTapsOrigOpticalGroup = cTapsOrig->at(cOpticalGroup->getIndex());
                    auto& cClustersOpticalGroup = cClusters->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cCicInjHybrid  = cCicInjOG->at(cHybrid->getIndex());
                        auto& cCicInjSummary = cCicInjHybrid->getSummary<uint32_t>();
                        LOG(DEBUG) << BOLDMAGENTA << "Injecting " << +cCicInjSummary << " clusters in CIC#" << +cHybrid->getId() << RESET;
                        auto& cInjHybrid      = cInjOG->at(cHybrid->getIndex());
                        auto& cReadoutHybrid  = cReadoutOG->at(cHybrid->getIndex());
                        auto& cTapsHybrid     = cTapsOpticalGroup->at(cHybrid->getIndex());
                        auto& cTapsOrigHybrid = cTapsOrigOpticalGroup->at(cHybrid->getIndex());
                        auto& cClustersHybrid = cClustersOpticalGroup->at(cHybrid->getIndex());

                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                            auto& cInjChip = cInjHybrid->at(cChip->getIndex());
                            auto& cExpInjs = cInjChip->getSummary<std::vector<Injection>>();

                            auto& cReadoutChip = cReadoutHybrid->at(cChip->getIndex());
                            auto& cReadoutInjs = cReadoutChip->getSummary<std::vector<Injection>>();

                            auto& cTapsChip     = cTapsHybrid->at(cChip->getIndex());
                            auto& cTapsOrigChip = cTapsOrigHybrid->at(cChip->getIndex());

                            auto&  cClustersChip      = cClustersHybrid->at(cChip->getIndex());
                            size_t cExpectedNClusters = cClustersChip->getSummary<uint32_t>();
                            if(cExpInjs.size() == 0) continue;

                            LOG(DEBUG) << BOLDMAGENTA << "\t..MPA#" << +cChip->getId() << " found " << +cExpInjs.size() << " in injection log "
                                       << " and " << +cReadoutInjs.size() << " in readout log "
                                       << " this run had " << +cExpectedNClusters << " clusters injected; "
                                       << " phase tap is " << +cTapsChip->getSummary<uint8_t>() << "; original tap is " << +cTapsOrigChip->getSummary<uint8_t>() << RESET;

#ifdef __USE_ROOT__
                            if(cTapsChip->getSummary<uint8_t>() == cTapsOrigChip->getSummary<uint8_t>())
                            {
                                for(size_t cIndx = 0; cIndx < cExpInjs.size(); cIndx++)
                                {
                                    int         cFill    = (cIndx < (float)cReadoutInjs.size()) ? 1 : 0;
                                    TProfile2D* cMatched = static_cast<TProfile2D*>(getHist(cHybrid, "PclusterMatchRawN"));
                                    cMatched->Fill(cChip->getId(), cCicInjSummary, cFill);

                                    TH1D* cInjCount = static_cast<TH1D*>(getHist(cChip, "PclusterInjCount"));
                                    cInjCount->Fill(cExpectedNClusters, 1);
                                    TH2D* cInjCount2D = static_cast<TH2D*>(getHist(cChip, "PclusterInjCount2D"));
                                    cInjCount2D->Fill(cExpectedNClusters, cCicInjSummary, 1);

                                    if(cIndx >= cReadoutInjs.size()) continue;

                                    TH1D* cMatchCount = static_cast<TH1D*>(getHist(cChip, "PclusterMatchedCount"));
                                    cMatchCount->Fill(cExpectedNClusters, 1);
                                    TH2D* cMtchCount2D = static_cast<TH2D*>(getHist(cChip, "PclusterMatchedCount2D"));
                                    cMtchCount2D->Fill(cExpectedNClusters, cCicInjSummary, 1);
                                }
                            }
                            for(size_t cIndx = 0; cIndx < cExpInjs.size(); cIndx++)
                            {
                                int         cFill = (cIndx < (float)cReadoutInjs.size()) ? 1 : 0;
                                TProfile2D* cEye  = static_cast<TProfile2D*>(getHist(cHybrid, "PclusterMatchingL1Eye"));
                                cEye->Fill(cChip->getId(), cTapsChip->getSummary<uint8_t>(), cFill);
                            }
                            //
#endif

                        } // ROCs
                    }     // hybrids or CICs
                }         // optical group loop
            }

            // for(auto cBoard: *fDetectorContainer)
            // {
            //     auto& cExp = fPixelExpected.at(cBoard->getIndex());
            //     auto& cInjPxls = fPixelInjections.at(cBoard->getIndex());
            //     auto& cInjections = fReadoutPClusters.at(cBoard->getIndex());
            //     auto& cExpected = fInjectedPClusters.at(cBoard->getIndex());
            //     auto& cTaps = fPhaseTaps.at(cBoard->getIndex());
            //     auto& cTapsOrig = fOriginalPhaseTaps.at(cBoard->getIndex());
            //     auto& cBxIds = fBxIds.at(cBoard->getIndex());
            //     auto& cCicInj = fCicInjections.at(cBoard->getIndex());
            //     for(auto cOpticalGroup: *cBoard)
            //     {
            //         auto& cExpOG = cExp->at(cOpticalGroup->getIndex());
            //         auto& cInjPxlOGs = cInjPxls->at(cOpticalGroup->getIndex());
            //         auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());
            //         auto& cExpectedOpticalGroup = cExpected->at(cOpticalGroup->getIndex());
            //         auto& cTapsOpticalGroup = cTaps->at(cOpticalGroup->getIndex());
            //         auto& cTapsOrigOpticalGroup = cTapsOrig->at(cOpticalGroup->getIndex());
            //         auto& cBxIdsOG = cBxIds->at(cOpticalGroup->getIndex());
            //         auto& cCicInjOG = cCicInj->at(cOpticalGroup->getIndex());
            //         for(auto cHybrid: *cOpticalGroup)
            //         {
            //             auto& cExpHybrid = cExpOG->at(cHybrid->getIndex());
            //             auto& cInjPxlHybrid = cInjPxlOGs->at(cHybrid->getIndex());
            //             auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
            //             auto& cExpectedHybrid = cExpectedOpticalGroup->at(cHybrid->getIndex());
            //             auto& cTapsHybrid = cTapsOpticalGroup->at(cHybrid->getIndex());
            //             auto& cTapsOrigHybrid = cTapsOrigOpticalGroup->at(cHybrid->getIndex());
            //             auto& cBxIdsHybrid = cBxIdsOG->at(cHybrid->getIndex());
            //             auto& cSummaryBxIds = cBxIdsHybrid->getSummary<std::vector<uint32_t>>();
            //             auto& cCicInjHybrid = cCicInjOG->at(cHybrid->getIndex());
            //             auto& cCicInjSummary = cCicInjHybrid->getSummary<uint32_t>();
            //             int              cNRollOvers = 0;
            //             uint16_t cMaxBxCounter = 3564;
            //             for( size_t cIndx=1; cIndx < cSummaryBxIds.size(); cIndx++)
            //             {
            //                 int cBxDifference = (cNRollOvers)*cMaxBxCounter + (cSummaryBxIds[cIndx-1] % cMaxBxCounter);
            //                 cNRollOvers += ((cSummaryBxIds[cIndx- 1] >= 2500) && (cSummaryBxIds[cIndx - 1] < cMaxBxCounter)) && ( cSummaryBxIds[cIndx] < cSummaryBxIds[cIndx - 1]) ? 1 : 0;
            //                 cBxDifference = (cNRollOvers)*cMaxBxCounter + (cSummaryBxIds[cIndx] % cMaxBxCounter) - cBxDifference;
            //                 LOG (DEBUG) << BOLDMAGENTA << "\t.. Bx Difference is " << +cBxDifference  << RESET;
            //                 #ifdef __USE_ROOT__
            //                     TH1D* cBxDiffHist = static_cast<TH1D*>(getHist(cHybrid,"BxIdDifference"));
            //                     cBxDiffHist->Fill( cBxDifference );
            //                 #endif
            //             }
            //             for(auto cChip: *cHybrid)
            //             {
            //                 if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

            //                 auto& cExpChip = cExpHybrid->at(cChip->getIndex());
            //                 auto& cExpPClusterSmry = cExpChip->getSummary<std::vector<Injection>>();
            //                 //
            //                 auto& cInjectionsThisChip = cInjectionsHybrid->at(cChip->getIndex());
            //                 auto cSummary = cInjectionsThisChip->getSummary<std::vector<uint32_t>>();
            //                 //
            //                 auto& cExpectedClstrsThisChip = cExpectedHybrid->at(cChip->getIndex());
            //                 //
            //                 auto& cTapsChip = cTapsHybrid->at(cChip->getIndex());
            //                 auto& cTapsOrigChip = cTapsOrigHybrid->at(cChip->getIndex());
            //                 //
            //                 auto& cInjPxlThisChip = cInjPxlHybrid->at(cChip->getIndex());
            //                 auto cPxlInjections = cInjPxlThisChip->getSummary<std::vector<Injection>>();
            //                 float cExpected = (float)(cExpectedClstrsThisChip->getSummary<uint32_t>());
            //                 if( cExpected ==0 ) continue;

            //                 LOG (INFO) << BOLDMAGENTA << "MPA#" << +cChip->getId()
            //                     << " found " << +cPxlInjections.size() << " in injection log "
            //                     << " and " << +cExpPClusterSmry.size() << " in readout log "
            //                     << RESET;

            //                 // injection map
            //                 for( auto cInj : cPxlInjections )
            //                 {
            //                     #ifdef __USE_ROOT__
            //                         TH2D* cInjMap = static_cast<TH2D*>(getHist(cChip,"InjectionMap"));
            //                         cInjMap->Fill( cInj.fColumn, cInj.fRow, 1 );
            //                     #endif
            //                 }

            //                 // readout map
            //                 for( auto cInj : cExpPClusterSmry )
            //                 {
            //                     #ifdef __USE_ROOT__
            //                         TH2D* cInjMapMatched = static_cast<TH2D*>(getHist(cChip,"MatchedInjectionMap"));
            //                         cInjMapMatched->Fill( cInj.fColumn, cInj.fRow , 1 );
            //                     #endif
            //                 }

            //                 #ifdef __USE_ROOT__
            //                     if( cTapsChip->getSummary<uint8_t>() == cTapsOrigChip->getSummary<uint8_t>() )
            //                     {
            //                         TH1D* cInjCount = static_cast<TH1D*>(getHist(cChip,"PclusterInjCount"));
            //                         cInjCount->Fill( cExpected, cSummary.size()*cExpected );
            //                         TH1D* cMtchCount = static_cast<TH1D*>(getHist(cChip,"PclusterMatchedCount"));
            //                         cMtchCount->Fill( cExpected, std::accumulate(cSummary.begin(), cSummary.end(), 0.) );
            //                         TH2D* cInjCount2D = static_cast<TH2D*>(getHist(cChip,"PclusterInjCount2D"));
            //                         cInjCount2D->Fill( cExpected, cCicInjSummary,  cSummary.size()*cExpected );
            //                         TH2D* cMtchCount2D = static_cast<TH2D*>(getHist(cChip,"PclusterMatchedCount2D"));
            //                         cMtchCount2D->Fill( cExpected, cCicInjSummary, std::accumulate(cSummary.begin(), cSummary.end(), 0.) );
            //                     }
            //                 #endif
            //                 std::vector<float> cData(0);
            //                 for(auto cPt : cSummary )
            //                 {
            //                     cData.push_back( (float)cPt );
            //                     if( (float)cPt != cExpected )
            //                     {
            //                         LOG (DEBUG) << BOLDRED << "MPA#" << +cChip->getId()
            //                             << " expected  " << +cExpected << " clusters and have found "
            //                             << +cPt << RESET;
            //                     }
            //                     #ifdef __USE_ROOT__

            //                         for( size_t cIndx=0; cIndx < cExpected; cIndx++)
            //                         {
            //                             int cFill = (cIndx < (float)cPt) ? 1 : 0 ;
            //                             TProfile2D* cMatched = static_cast<TProfile2D*>(getHist(cHybrid,"PclusterMatchingL1Eye"));
            //                             cMatched->Fill( cChip->getId(), (float)(cTapsChip->getSummary<uint8_t>()) , cFill) ;
            //                             if( cTapsChip->getSummary<uint8_t>() == cTapsOrigChip->getSummary<uint8_t>() )
            //                             {
            //                                 cMatched = static_cast<TProfile2D*>(getHist(cHybrid, "PclusterMatchRawN"));
            //                                 cMatched->Fill( cChip->getId(), cTotalNumberOfClusters , cFill);//(float)cPt/cExpected ) ;
            //                             }
            //                         }
            //                     #endif
            //                 }
            //                 auto cStats = getStats(cData);
            //                 if( cAttempt%cDebugPrintOut == 0 )
            //                 {
            //                     LOG (INFO) << BOLDBLUE << "MPA#" << +cChip->getId()
            //                         << " phase tap " << +cTapsChip->getSummary<uint8_t>()
            //                         << " original phase tap is " << +cTapsOrigChip->getSummary<uint8_t>()
            //                         << " [offset of " << cOffset
            //                         << " ]\t..."
            //                         << " on average have found " << +cStats.first
            //                         << " clusters and expect "
            //                         << +cExpectedClstrsThisChip->getSummary<uint32_t>()
            //                         << " total number of clusters injected in this hybrid are "
            //                         << +cCicInjSummary
            //                         << RESET;
            //                 }
            //             } // ROCs
            //         } // hybrids or CICs
            //     } // optical group loop
            // }
        }
    }
}
// check L1 eye
void DataChecker::Eye_CIC()
{
    int      cLatencyOffset = -1;
    auto     cSetting       = fSettingsMap.find("Nevents");
    uint32_t cNevents       = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    LOG(DEBUG) << BOLDBLUE << "ReadNEvents data test with " << +cNevents << RESET;

    uint8_t                cStubWindow = 1; // stub window in half pixels (1)
    uint8_t                cMode       = 2; // (0) pixel-strip, (1) strip-strip, (2) pixel-pixel, (3) strip-pixel
    std::vector<uint8_t>   cColumns{2};     // 5 , 10 };
    std::vector<uint8_t>   cRows{10};       // 20 , 30};
    std::vector<Injection> cInjections(0);
    for(size_t cIndx = 0; cIndx < cColumns.size(); cIndx++)
    {
        Injection cInjection;
        cInjection.fColumn = cColumns[cIndx];
        cInjection.fRow    = cRows[cIndx];
        cInjections.push_back(cInjection);
    } // create injection patterns

    // configure MPA to be in p-p mode
    // this is enough for this test
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                    // digital sync this pattern on pixel 1
                    LOG(INFO) << BOLDBLUE << "Controlling injection .." << RESET;
                    (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, cInjections);
                    // activate stub mode
                    fReadoutChipInterface->WriteChipReg(cChip, "StubMode", cMode);
                    fReadoutChipInterface->WriteChipReg(cChip, "StubWindow", cStubWindow);
                } // chip
            }     // hybrid
        }         // optical group
    }             // board

    // now configure latencies and timing between injections
    // check events for different latencies
    for(auto cBoard: *fDetectorContainer)
    {
        // check trigger source
        // and reload
        uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
        LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
        cTriggerSrc = (cTriggerSrc == 6) ? cTriggerSrc : 6;

        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
        fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);

        uint16_t cDelay       = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
        int      cLatency     = cDelay + cLatencyOffset;
        int      cReTimeValue = -1;
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency);
                    if(cChip->getFrontEndType() == FrontEndType::MPA && cReTimeValue < 0) { cReTimeValue = fReadoutChipInterface->ReadChipReg(cChip, "RetimePix"); }
                } // chip
            }     // hybrid
        }         // module
        auto cStubOffset  = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->getStubOffset();
        int  cStubLatency = cLatency - (cStubOffset + cReTimeValue);
        LOG(INFO) << BOLDBLUE << "Setting L1 latency to " << +cLatency << " and stub latency to " << +cStubLatency << RESET;
        // read events
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
    }

    // now .. loop over offsets and scan data
    for(int cOffset = -1; cOffset < +1; cOffset++)
    {
        // set offsets on CICs
        bool cValidOffset = true;
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cCic   = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    cValidOffset = cValidOffset && fCicInterface->SetOptimalTaps(cCic, cOffset);
                } // hybrid
            }     // optical group
        }         // board
        if(!cValidOffset) continue;

        LOG(INFO) << BOLDBLUE << "Offset of " << +cOffset << " from optimal tap on CIC inputs.." << RESET;
        for(auto cBoard: *fDetectorContainer)
        {
            this->CheckPSData(cBoard, cInjections);
            auto& cBadEvents  = fBadEvents.at(cBoard->getIndex());
            auto& cGoodEvents = fGoodEvents.at(cBoard->getIndex());
            for(auto cOpticalGroup: *cBoard)
            {
                auto& cBadEventsOG = cBadEvents->at(cOpticalGroup->getIndex());
                auto& cGdEventsOG  = cGoodEvents->at(cOpticalGroup->getIndex());
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cBadEventsHybrid = cBadEventsOG->at(cHybrid->getIndex());
                    auto& cGdEventsHybrid  = cGdEventsOG->at(cHybrid->getIndex());
                    for(auto cChip: *cHybrid)
                    {
                        auto& cBadEventsChip = cBadEventsHybrid->at(cChip->getIndex());
                        auto& cBadEventsList = cBadEventsChip->getSummary<EventsList>();

                        auto& cGdEventsChip = cGdEventsHybrid->at(cChip->getIndex());
                        auto& cGdEventsList = cGdEventsChip->getSummary<EventsList>();

                        LOG(INFO) << BOLDBLUE << "Found " << +cBadEventsList.size() << " bad events and " << +cGdEventsList.size() << " good events." << RESET;

                        // // look at events in class 0
                        // for( int cSelection=6; cSelection>=0; cSelection--)
                        // {
                        //     if( cSelection == 5 ) continue; // this is a good event
                        //     std::vector<int> cL1Ids_BdEvnts(0);
                        //     for( auto cBadEventTag : cBadEventsList )
                        //     {
                        //         EventId cId=cBadEventTag.first;
                        //         uint8_t cTg=cBadEventTag.second;
                        //         //no p clusters
                        //         if(cTg==cSelection) cL1Ids_BdEvnts.push_back(cId.first);
                        //     }
                        //     LOG (INFO) << BOLDBLUE << "\t.. " << +cL1Ids_BdEvnts.size() << " events with classification " << +cSelection << RESET;
                        //     if( cSelection == 0 )
                        //     {
                        //         auto cIterator = std::find( cL1Ids_BdEvnts.begin(), cL1Ids_BdEvnts.end(), 511 );
                        //         std::vector<int> cNBadEvents_511(0);
                        //         std::vector<int> cNBadEvents_Rndm(0);
                        //         int cN511sfound=0;
                        //         while( cIterator!= cL1Ids_BdEvnts.end() )
                        //         {
                        //             auto cNextPosition = std::find( cIterator+1, cL1Ids_BdEvnts.end(), 511 );
                        //             if( cNextPosition != cL1Ids_BdEvnts.end() )
                        //             {
                        //                 int cNBad_511=0;
                        //                 auto cIter = cIterator;
                        //                 do
                        //                 {
                        //                     if( cIter!= cIterator)
                        //                     {
                        //                         int cDiff = (int)(*cIter) - (int)(*(cIter-1));
                        //                         if( cDiff != 1  && cDiff != -511 ){
                        //                             LOG (DEBUG) << BOLDRED << "\t.. L1 difference of " << +cDiff << " between bad events."
                        //                                 << " L1Id is " << *cIter
                        //                                 << " [L1Id mod 16 = " << +((int)(*cIter)%16)
                        //                                 << " ] previous event had an L1Id of " << *(cIter-1)
                        //                                 << RESET;
                        //                             cNBadEvents_Rndm.push_back(*cIter);
                        //                         }
                        //                         else cNBad_511++;
                        //                     }
                        //                     else cNBad_511++;
                        //                     cIter++;
                        //                 }while( cIter != cNextPosition );
                        //                 cNBadEvents_511.push_back(cNBad_511);
                        //             }//look for next bad event in the list
                        //             cIterator = cNextPosition;
                        //             cN511sfound++;
                        //         }// list of bad events
                        //         auto cSum = std::accumulate(cNBadEvents_511.begin(), cNBadEvents_511.end(), 0.0);
                        //         auto cMean = cSum/cNBadEvents_511.size();
                        //         auto cMax = std::max_element(cNBadEvents_511.begin(), cNBadEvents_511.end());
                        //         auto cMin = std::min_element(cNBadEvents_511.begin(), cNBadEvents_511.end());
                        //         double cSqSum = std::inner_product(cNBadEvents_511.begin(), cNBadEvents_511.end(), cNBadEvents_511.begin(), 0.0);
                        //         double cStdDev = std::sqrt(cSqSum / cNBadEvents_511.size() - cMean * cMean);
                        //         LOG (INFO) << BOLDBLUE << "\t\t..Found " << +cN511sfound
                        //             << " times where an L1Id of 511 was found in a readout event.."
                        //             << +cSum
                        //             << " of those L1Ids are consecutive ones missing immediately after an L1Id of 511."
                        //             << RESET;
                        //         LOG (INFO) << BOLDBLUE << "\t\t .. On average, the " << +cMean
                        //             << " events following an L1Id of 511 are bad..."
                        //             << " StdDev : " << cStdDev
                        //             << " Maxium :  " << (*cMax)
                        //             << " Minimum : " << (*cMin)
                        //             << RESET;
                        //         LOG (INFO) << BOLDBLUE << "\t\t .. " << +cNBadEvents_Rndm.size() << " are some others population. "
                        //             << RESET;
                        //     }
                        // }
                        // remember to clear
                        cBadEventsList.clear();
                        cGdEventsList.clear();
                    } // chip
                }     // hybrid
            }         // module
        }
    }
}
void DataChecker::DigitalInjectionTest(bool pBypassCic, bool pShiftRegMode)
{
    auto     cSetting = fSettingsMap.find("Nevents");
    uint32_t cNevents = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    LOG(DEBUG) << BOLDBLUE << "ReadNEvents data test with " << +cNevents << RESET;
    uint8_t cPattern = 0xAA;

    uint8_t cStubWindow = 1; // stub window in half pixels (1)
    uint8_t cMode       = 2; // (0) pixel-strip, (1) strip-strip, (2) pixel-pixel, (3) strip-pixel

    std::vector<uint8_t>   cColumns{2}; // 5 , 10 };
    std::vector<uint8_t>   cRows{10};   // 20 , 30};
    std::vector<Injection> cInjections(0);
    for(size_t cIndx = 0; cIndx < cColumns.size(); cIndx++)
    {
        Injection cInjection;
        cInjection.fColumn = cColumns[cIndx];
        cInjection.fRow    = cRows[cIndx];
        cInjections.push_back(cInjection);
    } // create injection patterns

    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);

        uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
        cTriggerSrc          = (cTriggerSrc == 6) ? cTriggerSrc : 6;
        LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
        cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);

        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // let's try and look at CIC mux
                if(pBypassCic)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fCicInterface->SelectOutput(cCic, false); // force it to be off
                }
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                    fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_0", 1); // 1
                    fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_1", 2); // 2
                    fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_2", 3); // 3
                    fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_3", 4); // 4
                    fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_4", 5); // 5
                    fReadoutChipInterface->WriteChipReg(cChip, "OutSetting_5", 0); // L1 line

                    // I want to test my row configuration stuff
                    if(pShiftRegMode) { fReadoutChipInterface->WriteChipReg(cChip, "DigitalPattern", cPattern); } // shit register mode
                    else
                    {
                        // digital sync this pattern on pixel 1
                        LOG(INFO) << BOLDBLUE << "Controlling injection .." << RESET;
                        (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, cInjections);
                        // activate stub mode
                        fReadoutChipInterface->WriteChipReg(cChip, "StubMode", cMode);
                        fReadoutChipInterface->WriteChipReg(cChip, "StubWindow", cStubWindow);
                    }
                } // chip
                if(pBypassCic)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    std::vector<uint8_t> cPhyPorts(12, 0);
                    std::iota(cPhyPorts.begin(), cPhyPorts.end(), 0);
                    for(auto cPhyPort: cPhyPorts)
                    {
                        std::string cMPAHeader = "111111110";
                        LOG(INFO) << BOLDBLUE << "PhyPort" << +cPhyPort << RESET;
                        fCicInterface->SelectMux(cCic, cPhyPort); // 0 , stubs MPA5(FE0)
                        if(!pShiftRegMode)
                        {
                            fBeBoardInterface->Start(cBeBoard);
                            fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBeBoard));
                        }
                        if(pShiftRegMode)
                        {
                            auto cLines    = (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->ScopeStubLines();
                            int  cLineIndx = 0;
                            for(auto cLine: cLines)
                            {
                                if(cLineIndx == 4) continue;
                                LOG(INFO) << BOLDBLUE << "Line#" << +cLineIndx << BOLDRED << cLine << RESET;
                                cLineIndx++;
                            }
                        }
                        else
                        {
                            for(int cAttempt = 0; cAttempt < 100; cAttempt++)
                            {
                                auto cLines    = (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->ScopeStubLines();
                                int  cLineIndx = 0;
                                for(auto cLine: cLines)
                                {
                                    if(cLine.find(cMPAHeader) != std::string::npos)
                                        LOG(INFO) << BOLDBLUE << "Attempt# " << +cAttempt << " PhyPort#" << +cPhyPort << ",Line#" << +cLineIndx << " : " << BOLDGREEN << cLine << RESET;
                                    else
                                        LOG(DEBUG) << BOLDBLUE << "Line#" << +cLineIndx << BOLDRED << cLine << RESET;
                                    cLineIndx++;
                                }
                                //(static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, 4);
                                // std::this_thread::sleep_for(std::chrono::microseconds(10));
                            }
                        }
                        if(!pShiftRegMode)
                        {
                            fBeBoardInterface->Stop(cBeBoard);
                            for(auto cChip: *cHybrid)
                            {
                                if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
                                auto cRegValue = fReadoutChipInterface->ReadChipReg(cChip, "ErrorL1");
                                LOG(INFO) << BOLDBLUE << "ErrorL1 register is 0x" << std::hex << +cRegValue << std::dec << RESET;
                            }
                        }
                    }
                }
            } // hybrid
        }     // optical group
    }         // configure CIC and MPA

    // std::vector<uint16_t> cDelaysBetwn{90, 180, 360};
    // // check events for different latencies
    // for(auto cBoard: *fDetectorContainer)
    // {
    //     // check trigger source
    //     // and reload
    //     uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    //     LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    //     cTriggerSrc = (cTriggerSrc == 6) ? cTriggerSrc : 6;
    //     for(auto cDelayBetwn: cDelaysBetwn)
    //     {
    //         std::vector<std::pair<std::string, uint32_t>> cRegVec;
    //         cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    //         cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", cDelayBetwn});
    //         fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);

    //         // figure out what stub offset was set to
    //         auto     cStubOffset  = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->getStubOffset();
    //         int      cReTimeValue = -1;
    //         uint16_t cDelay       = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    //         for(uint16_t cLatency = cDelay - 1; cLatency < cDelay; cLatency++)
    //         {
    //             for(auto cOpticalGroup: *cBoard)
    //             {
    //                 for(auto cHybrid: *cOpticalGroup)
    //                 {
    //                     for(auto cChip: *cHybrid)
    //                     {
    //                         fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cLatency);
    //                         if(cChip->getFrontEndType() == FrontEndType::MPA && cReTimeValue < 0) { cReTimeValue = fReadoutChipInterface->ReadChipReg(cChip, "RetimePix"); }
    //                     } // chip
    //                 }     // hybrid
    //             }         // module

    //             int cStubLatency = cLatency - (cStubOffset + cReTimeValue);
    //             LOG(INFO) << BOLDBLUE << "Setting L1 latency to " << +cLatency << " and stub latency to " << +cStubLatency << RESET;
    //             // read events
    //             fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);

    //             // do this 10 times
    //             for(int cAttempt = 0; cAttempt < 50; cAttempt++)
    //             {
    //                 if(cAttempt % 10 == 0) LOG(INFO) << BOLDBLUE << "Attempt#" << +cAttempt << RESET;
    //                 this->CheckPSData(cBoard, cInjections);
    //             } // attempt loop

    //             // and now look at all the bad events
    //             uint16_t cCalPulseDelay        = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse");
    //             uint16_t cTimeBetweenCalPulses = cCalPulseDelay + cDelay;
    //             LOG(INFO) << BOLDBLUE << "An L1A/CalPulse is sent once every  " << (cTimeBetweenCalPulses) << " Bx." << RESET;
    //             auto& cBadEvents  = fBadEvents.at(cBoard->getIndex());
    //             auto& cGoodEvents = fGoodEvents.at(cBoard->getIndex());
    //             for(auto cOpticalGroup: *cBoard)
    //             {
    //                 auto& cBadEventsOG = cBadEvents->at(cOpticalGroup->getIndex());
    //                 auto& cGdEventsOG  = cGoodEvents->at(cOpticalGroup->getIndex());
    //                 for(auto cHybrid: *cOpticalGroup)
    //                 {
    //                     auto& cBadEventsHybrid = cBadEventsOG->at(cHybrid->getIndex());
    //                     auto& cGdEventsHybrid  = cGdEventsOG->at(cHybrid->getIndex());
    //                     for(auto cChip: *cHybrid)
    //                     {
    //                         auto& cBadEventsChip = cBadEventsHybrid->at(cChip->getIndex());
    //                         auto& cBadEventsList = cBadEventsChip->getSummary<EventsList>();

    //                         auto& cGdEventsChip = cGdEventsHybrid->at(cChip->getIndex());
    //                         auto& cGdEventsList = cGdEventsChip->getSummary<EventsList>();

    //                         LOG(INFO) << BOLDBLUE << "Found " << +cBadEventsList.size() << " bad events and " << +cGdEventsList.size() << " good events." << RESET;

    //                         // look at events in class 0
    //                         for(int cSelection = 6; cSelection >= 0; cSelection--)
    //                         {
    //                             if(cSelection == 5) continue; // this is a good event
    //                             std::vector<int> cL1Ids_BdEvnts(0);
    //                             for(auto cBadEventTag: cBadEventsList)
    //                             {
    //                                 EventId cId = cBadEventTag.first;
    //                                 uint8_t cTg = cBadEventTag.second;
    //                                 // no p clusters
    //                                 if(cTg == cSelection) cL1Ids_BdEvnts.push_back(cId.first);
    //                             }
    //                             LOG(INFO) << BOLDBLUE << "\t.. " << +cL1Ids_BdEvnts.size() << " events with classification " << +cSelection << RESET;
    //                             if(cSelection == 0)
    //                             {
    //                                 auto             cIterator = std::find(cL1Ids_BdEvnts.begin(), cL1Ids_BdEvnts.end(), 511);
    //                                 std::vector<int> cNBadEvents_511(0);
    //                                 std::vector<int> cNBadEvents_Rndm(0);
    //                                 int              cN511sfound = 0;
    //                                 while(cIterator != cL1Ids_BdEvnts.end())
    //                                 {
    //                                     auto cNextPosition = std::find(cIterator + 1, cL1Ids_BdEvnts.end(), 511);
    //                                     if(cNextPosition != cL1Ids_BdEvnts.end())
    //                                     {
    //                                         int  cNBad_511 = 0;
    //                                         auto cIter     = cIterator;
    //                                         do
    //                                         {
    //                                             if(cIter != cIterator)
    //                                             {
    //                                                 int cDiff = (int)(*cIter) - (int)(*(cIter - 1));
    //                                                 if(cDiff != 1 && cDiff != -511)
    //                                                 {
    //                                                     LOG(DEBUG) << BOLDRED << "\t.. L1 difference of " << +cDiff << " between bad events."
    //                                                                << " L1Id is " << *cIter << " [L1Id mod 16 = " << +((int)(*cIter) % 16) << " ] previous event had an L1Id of " << *(cIter - 1)
    //                                                                << RESET;
    //                                                     cNBadEvents_Rndm.push_back(*cIter);
    //                                                 }
    //                                                 else
    //                                                     cNBad_511++;
    //                                             }
    //                                             else
    //                                                 cNBad_511++;
    //                                             cIter++;
    //                                         } while(cIter != cNextPosition);
    //                                         cNBadEvents_511.push_back(cNBad_511);
    //                                     } // look for next bad event in the list
    //                                     cIterator = cNextPosition;
    //                                     cN511sfound++;
    //                                 } // list of bad events
    //                                 auto   cSum    = std::accumulate(cNBadEvents_511.begin(), cNBadEvents_511.end(), 0.0);
    //                                 auto   cMean   = cSum / cNBadEvents_511.size();
    //                                 auto   cMax    = std::max_element(cNBadEvents_511.begin(), cNBadEvents_511.end());
    //                                 auto   cMin    = std::min_element(cNBadEvents_511.begin(), cNBadEvents_511.end());
    //                                 double cSqSum  = std::inner_product(cNBadEvents_511.begin(), cNBadEvents_511.end(), cNBadEvents_511.begin(), 0.0);
    //                                 double cStdDev = std::sqrt(cSqSum / cNBadEvents_511.size() - cMean * cMean);
    //                                 LOG(INFO) << BOLDBLUE << "\t\t..Found " << +cN511sfound << " times where an L1Id of 511 was found in a readout event.." << +cSum
    //                                           << " of those L1Ids are consecutive ones missing immediately after an L1Id of 511." << RESET;
    //                                 LOG(INFO) << BOLDBLUE << "\t\t .. On average, the " << +cMean << " events following an L1Id of 511 are bad..."
    //                                           << " StdDev : " << cStdDev << " Maxium :  " << (*cMax) << " Minimum : " << (*cMin) << RESET;
    //                                 LOG(INFO) << BOLDBLUE << "\t\t .. " << +cNBadEvents_Rndm.size() << " are some others population. " << RESET;
    //                             }
    //                         }
    //                         // remember to clear
    //                         cBadEventsList.clear();
    //                         cGdEventsList.clear();
    //                     } // chip
    //                 }     // hybrid
    //             }         // module
    //         }             // latency scan
    //     }                 // delay scan

    // } // board
}
void DataChecker::L1Eye(std::vector<uint8_t> pChipIds)
{
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(this);
    cBackEndAligner.Initialise();
    for(uint8_t cPhase = 4; cPhase < 14; cPhase += 1)
    {
        fPhaseTap = cPhase;
        // zero container
        zeroContainers();
        LOG(INFO) << BOLDBLUE << "Setting optimal phase tap in CIC to " << +cPhase << RESET;
        for(auto cBoard: *fDetectorContainer)
        {
            // for (auto cOpticalGroup : *cBoard)
            // {
            //     for (auto& cHybrid : *cOpticalGroup)
            //     {
            //         auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            //         //fCicInterface->ResetPhaseAligner(cCic);
            //         for(auto cChipId : pChipIds )
            //         {
            //             // bool cConfigured = fCicInterface->SetStaticPhaseAlignment(  cCic , cChipId ,  0 , cPhase);
            //             // check if a resync is needed
            //             //fCicInterface->CheckReSync( static_cast<OuterTrackerHybrid*>(cHybrid)->fCic);
            //         }
            //     }
            // }
            // send a resync
            fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
            // re-do back-end alignment
            // cBackEndAligner.L1Alignment2S(cBoard);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        // run data check
        this->DataCheck(pChipIds);
        // print results
        // this->print({cChipId});
    }
}

void DataChecker::ReadNeventsTest()
{
    // this->DigitalInjectionTest(true, false);
    bool     cWithNoise = false;
    auto     cSetting   = fSettingsMap.find("Nevents");
    uint32_t cNevents   = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    LOG(INFO) << BOLDBLUE << "ReadNEvents data test with " << +cNevents << RESET;
    std::stringstream outp;
    for(auto cBoard: *fDetectorContainer)
    {
        // auto cEventType = cBoard->getEventType();
        // bool cSparsified = cBoard->getSparsification();
        // fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", cSparsified);
        bool cSkip = false;
        if(!cSkip)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() == FrontEndType::CBC3)
                        {
                            std::vector<uint8_t> cSeeds{10};
                            cSeeds[0] = 2 * (cChip->getId() + 1) + 5;
                            std::vector<int> cBends{0};
                            for(size_t cIndx = 0; cIndx < cSeeds.size(); cIndx += 1)
                            {
                                auto cHitList = (static_cast<CbcInterface*>(fReadoutChipInterface))->stubInjectionPattern(cChip, cSeeds[cIndx], cBends[cIndx]);
                                LOG(INFO) << BOLDBLUE << "RoC#" << +cChip->getId() << " expect to see hits in channels : " << RESET;
                                for(auto cHit: cHitList) LOG(INFO) << BOLDMAGENTA << "\t\t.." << +cHit << RESET;
                            }
                            (static_cast<CbcInterface*>(fReadoutChipInterface))->injectStubs(cChip, cSeeds, cBends, cWithNoise);
                        }
                        else if(cChip->getFrontEndType() == FrontEndType::MPA)
                        {
                            auto cReadoutMode = fReadoutChipInterface->ReadChipReg(cChip, "ReadoutMode");
                            LOG(INFO) << BOLDBLUE << "MPA#" << +cChip->getId() << " : readout mode [" << +cReadoutMode << " ]" << RESET;
                        }
                    }
                }
            }
        }
        cNevents = 1;
        LOG(INFO) << BOLDBLUE << "Checking ReadNEvents by reading " << +cNevents << " event from BeBoard#" << +cBoard->getIndex() << RESET;

        BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);
        this->ReadNEvents(cBeBoard, cNevents);
        const std::vector<Event*>& cEvents = this->GetEvents();
        LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadData" << RESET;

        uint32_t cN = 0;
        for(auto& cEvent: cEvents)
        {
            for(auto cBoard: *fDetectorContainer)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        for(auto cChip: *cHybrid)
                        {
                            auto cStubs           = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                            auto cHits            = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                            auto cPipelineAddress = cEvent->PipelineAddress(cHybrid->getId(), cChip->getId());
                            auto cL1Id            = cEvent->L1Id(cHybrid->getId(), cChip->getId());
                            LOG(INFO) << BOLDGREEN << "ROC#" << +cChip->getId() << " L1Id is " << +cL1Id << " found " << +cHits.size() << " hits at pipeline address " << +cPipelineAddress
                                      << " , also found " << +cStubs.size() << " stubs in the event" << RESET;
                            for(auto cHit: cHits) LOG(INFO) << BOLDGREEN << "\t\t... hit in channel#" << +cHit << RESET;
                        }
                    }
                }
            }
            // if(cN % 5 == 0)
            // {
            //     LOG(INFO) << ">>> Event #" << cN << RESET;
            //     ;
            //     outp.str("");
            //     outp << *cEvent;
            //     LOG(INFO) << outp.str();
            // }
            cN++;
        }
        // cBoard->setEventType(cEventType);
    }
    LOG(INFO) << BOLDBLUE << "Done!" << RESET;
}
void DataChecker::TestPulse(std::vector<uint8_t> pChipIds)
{
    // Prepare container to hold  measured occupancy
    DetectorDataContainer cMeasurement;
    fDetectorDataContainer = &cMeasurement;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    // get number of events from xml
    auto     cSetting        = fSettingsMap.find("Nevents");
    uint32_t cEventsPerPoint = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;

    // get trigger multiplicity from xml
    cSetting                       = fSettingsMap.find("TriggerMultiplicity");
    bool     cConfigureTriggerMult = (cSetting != std::end(fSettingsMap));
    uint16_t cTriggerMult          = cConfigureTriggerMult ? cSetting->second : 0;

    // get stub delay scan range from xml
    cSetting                  = fSettingsMap.find("StubDelay");
    bool cModifyStubScanRange = (cSetting != std::end(fSettingsMap));
    int  cStubDelay           = cModifyStubScanRange ? cSetting->second : 48;

    // get target threshold
    cSetting = fSettingsMap.find("Threshold");

    // get number of attempts
    cSetting = fSettingsMap.find("Attempts");
    // get mode
    cSetting      = fSettingsMap.find("Mode");
    uint8_t cMode = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    // get latency offset
    cSetting = fSettingsMap.find("LatencyOffset");
    // resync between attempts
    cSetting = fSettingsMap.find("ReSync");

    // if TP is used - enable it
    cSetting              = fSettingsMap.find("PulseShapePulseAmplitude");
    fTPconfig.tpAmplitude = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;

    // get TP amplitude range
    // get threshold range

    // get threshold range
    cSetting            = fSettingsMap.find("PulseShapeInitialVcth");
    uint16_t cInitialTh = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 400;
    cSetting            = fSettingsMap.find("PulseShapeFinalVcth");
    uint16_t cFinalTh   = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 600;
    cSetting            = fSettingsMap.find("PulseShapeVCthStep");
    uint16_t cThStep    = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 5;

    // get TP delay range
    cSetting                 = fSettingsMap.find("PulseShapeInitialDelay");
    uint16_t cInitialTPdleay = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    cSetting                 = fSettingsMap.find("PulseShapeFinalDelay");
    uint16_t cFinalTPdleay   = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 25;
    cSetting                 = fSettingsMap.find("PulseShapeDelayStep");
    uint16_t cTPdelayStep    = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 3;

    // get injected stub from xmls
    std::pair<uint8_t, int> cStub;
    cSetting     = fSettingsMap.find("StubSeed");
    cStub.first  = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 10;
    cSetting     = fSettingsMap.find("StubBend");
    cStub.second = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    LOG(DEBUG) << BOLDBLUE << "Injecting a stub in position " << +cStub.first << " with bend " << cStub.second << " to test data integrity..." << RESET;

    // set-up for TP
    fAllChan                     = true;
    fMaskChannelsFromOtherGroups = !this->fAllChan;
    this->SetTestPulse(true);
    setSameGlobalDac("TestPulsePotNodeSel", 0xFF - fTPconfig.tpAmplitude);

    // configure FE chips so that stubs are detected [i.e. make sure HIP
    // suppression is off ]

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerHybrid*>(cHybrid)->getLinkId());
                // configure CBCs
                for(auto cChip: *cHybrid)
                {
                    // switch off HitOr
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(static_cast<ReadoutChip*>(cChip), "HitOr", 0);
                    // enable stub logic
                    if(cMode == 0)
                        static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(static_cast<ReadoutChip*>(cChip), "Sampled", true, true);
                    else
                        static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(static_cast<ReadoutChip*>(cChip), "Latched", true, true);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(static_cast<ReadoutChip*>(cChip), false, false, 0);
                }
            }
        }
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    }

    // generate stubs in exactly chips with IDs that match pChipIds
    std::vector<int> cExpectedHits(0);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    if(std::find(pChipIds.begin(), pChipIds.end(), cChip->getId()) != pChipIds.end())
                    {
                        std::vector<uint8_t> cBendLUT = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theChip);
                        // both stub and bend are in units of half strips
                        // if using TP then always inject a stub with bend 0 ..
                        // later will use offset window to modify bend [ should probably put this in inject stub ]
                        uint8_t cBend_halfStrips = cStub.second;
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theChip, {cStub.first}, {cBend_halfStrips}, false);
                        // each bend code is stored in this vector - bend encoding start at -7 strips, increments by 0.5
                        // strips set offsets needs to be fixed
                        /*
                        uint8_t cOffsetCode = static_cast<uint8_t>(std::fabs(cBend_strips*2)) |
                        (std::signbit(-1*cBend_strips) << 3);
                        //uint8_t cOffsetCode = static_cast<uint8_t>(std::fabs(cBend_strips*2)) |
                        (std::signbit(-1*cBend_strips) << 3);
                        // set offsets
                        uint8_t cOffetReg = (cOffsetCode << 4) | (cOffsetCode << 0);
                        LOG (INFO) << BOLDBLUE << "\t..--- bend code is 0x" << std::hex << +cBendCode << std::dec << "
                        ..setting offset window to  " << std::bitset<8>(cOffetReg) << RESET;
                        fReadoutChipInterface->WriteChipReg ( theChip, "CoincWind&Offset12", cOffetReg );
                        fReadoutChipInterface->WriteChipReg ( theChip, "CoincWind&Offset34", cOffetReg );
                        */
                        fReadoutChipInterface->enableInjection(theChip, true);
                        // static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg( theChip, "VCth" ,
                        // cTargetThreshold);
                    }
                    else
                        static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(theChip, true);
                }
            }
        }
    }

    // measure
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard          = static_cast<BeBoard*>(cBoard);
        uint16_t cBoardTriggerMult = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
        LOG(DEBUG) << BOLDBLUE << "Trigger multiplicity is set to " << +cBoardTriggerMult << " consecutive triggers per L1A." << RESET;
        if(cConfigureTriggerMult)
        {
            LOG(DEBUG) << BOLDBLUE << "Modifying trigger multiplicity to be " << +(1 + cTriggerMult) << " consecutive triggers per L1A for DataTest" << RESET;
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cTriggerMult);
        }

        // set TP amplitude
        setSameGlobalDac("TestPulsePotNodeSel", 0xFF - fTPconfig.tpAmplitude);
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(fTPconfig.firmwareTPdelay, fTPconfig.tpDelay, fTPconfig.tpSequence, fTPconfig.tpFastReset);
        for(uint16_t cDelay = cInitialTPdleay; cDelay <= cFinalTPdleay; cDelay += cTPdelayStep)
        {
            uint8_t  cDelayDAC   = 25 - cDelay % 25;
            uint16_t cLatencyDAC = fTPconfig.tpDelay - cDelay / 25;
            // configure TP delay on all chips
            setSameGlobalDac("TestPulseDelay", cDelayDAC);
            // configure hit latency on all chips
            setSameDacBeBoard(theBoard, "TriggerLatency", cLatencyDAC);
            // set stub latency on back-end board
            uint16_t cStubLatency = cLatencyDAC - 1 * cStubDelay;
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
            fBeBoardInterface->ChipReSync(theBoard); // NEED THIS! ??
            // loop over threshold here
            LOG(INFO) << BOLDMAGENTA << "Delay is " << -1 * cDelay << " TP delay is " << +cDelayDAC << " latency DAC set to " << +cLatencyDAC << RESET;
            for(uint16_t cThreshold = cInitialTh; cThreshold < cFinalTh; cThreshold += cThStep)
            {
                LOG(DEBUG) << BOLDMAGENTA << "\t\t...Threshold is " << +cThreshold << RESET;
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        for(auto cChip: *cHybrid)
                        {
                            if(std::find(pChipIds.begin(), pChipIds.end(), cChip->getId()) == pChipIds.end()) continue;
                            fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cChip), "VCth", cThreshold);
                        }
                    }
                }

                // start triggers
                fBeBoardInterface->Start(theBoard);
                auto cNtriggers = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
                do {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    cNtriggers = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");
                } while(cNtriggers < 100);
                fBeBoardInterface->Stop(theBoard);
                // this->ReadNEvents ( theBoard , cEventsPerPoint);
                this->ReadData(theBoard, true);
                const std::vector<Event*>& cEvents = this->GetEvents();
                // matching
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto cHybridId = cHybrid->getId();
                        for(auto cChip: *cHybrid)
                        {
                            ReadoutChip* theChip    = static_cast<ReadoutChip*>(cChip);
                            auto         cOffset    = 0;
                            auto         cThreshold = fReadoutChipInterface->ReadChipReg(theChip, "VCth");
                            auto         cChipId    = cChip->getId();
                            if(std::find(pChipIds.begin(), pChipIds.end(), cChipId) == pChipIds.end()) continue;

                            std::vector<uint8_t> cBendLUT = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theChip);
                            // each bend code is stored in this vector - bend encoding start at -7 strips, increments by
                            // 0.5 strips
                            uint8_t cBendCode = cBendLUT[((cStub.second + cOffset) / 2. - (-7.0)) / 0.5];

                            std::vector<uint8_t> cExpectedHits = static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern(theChip, cStub.first, cStub.second);
                            LOG(DEBUG) << BOLDMAGENTA << "Injected a stub with seed " << +cStub.first << " with bend " << +cStub.second << RESET;
                            for(auto cHitExpected: cExpectedHits) LOG(DEBUG) << BOLDMAGENTA << "\t.. expect a hit in channel " << +cHitExpected << RESET;

                            auto cEventIterator = cEvents.begin();
                            LOG(DEBUG) << BOLDMAGENTA << "CBC" << +cChip->getId() << RESET;
                            size_t cMatchedStubs = 0;
                            // vector to keep track of number of matches
                            std::vector<uint32_t> cHitMatches(cExpectedHits.size(), 0);
                            for(size_t cEventIndex = 0; cEventIndex < cEventsPerPoint; cEventIndex++) // for each event
                            {
                                uint32_t cPipeline_first = 0;
                                uint32_t cBxId_first     = 0;

                                if(cEventIndex == 0) LOG(DEBUG) << BOLDMAGENTA << "'\tEvent " << +cEventIndex << RESET;
                                bool cIncorrectPipeline = false;
                                for(size_t cTriggerIndex = 0; cTriggerIndex <= cTriggerMult; cTriggerIndex++) // cTriggerMult consecutive triggers were sent
                                {
                                    auto     cEvent    = *cEventIterator;
                                    auto     cBxId     = cEvent->BxId(cHybrid->getId());
                                    uint32_t cPipeline = cEvent->PipelineAddress(cHybridId, cChipId);
                                    cBxId_first        = (cTriggerIndex == 0) ? cBxId : cBxId_first;
                                    cPipeline_first    = (cTriggerIndex == 0) ? cPipeline : cPipeline_first;
                                    bool cCountEvent   = (static_cast<size_t>(cPipeline - cPipeline_first) == cTriggerIndex);
                                    if(cCountEvent)
                                    {
                                        // hits
                                        auto   cHits       = cEvent->GetHits(cHybridId, cChipId);
                                        size_t cMatched    = 0;
                                        int    cLatency_eq = cLatencyDAC - (cPipeline - cPipeline_first);
                                        double cTime_ns    = -1 * (cLatency_eq - fTPconfig.tpDelay) * 25 + cDelayDAC;
                                        auto   cIterator   = cExpectedHits.begin();
                                        do {
                                            bool cMatchFound = std::find(cHits.begin(), cHits.end(), *cIterator) != cHits.end();
                                            cHitMatches[std::distance(cExpectedHits.begin(), cIterator)] += cMatchFound;
                                            cMatched += cMatchFound;
                                            cIterator++;
                                        } while(cIterator < cExpectedHits.end());

                                        // stubs
                                        auto cStubs         = cEvent->StubVector(cHybridId, cChipId);
                                        int  cNmatchedStubs = 0;
                                        for(auto cHybridStub: cStubs)
                                        {
                                            LOG(DEBUG) << BOLDMAGENTA << "\t.. expected seed is " << +cStub.first << " measured seed is " << +cHybridStub.getPosition() << RESET;
                                            LOG(DEBUG) << BOLDMAGENTA << "\t.. expected bend code is 0x" << std::hex << +cBendCode << std::dec << " measured bend code is 0x" << std::hex
                                                       << +cHybridStub.getBend() << std::dec << RESET;
                                            bool cMatchFound = (cHybridStub.getPosition() == cStub.first && cHybridStub.getBend() == cBendCode);
                                            cNmatchedStubs += static_cast<int>(cMatchFound);
                                        }
                                        if(cNmatchedStubs == 1) { cMatchedStubs++; }

                                        if(cEventIndex == 0)
                                            LOG(DEBUG) << BOLDMAGENTA << "\t\t.. Threshold of " << +cThreshold << " [Trigger " << +cTriggerIndex << " Pipeline is " << +cPipeline << "] Delay of "
                                                       << +cTime_ns << " ns [ " << +cDelay << " ] after the TP ... Found " << +cMatched << " matched hits and " << +cNmatchedStubs << " matched stubs."
                                                       << RESET;
                                    }
                                    else
                                        cIncorrectPipeline = cIncorrectPipeline || true;
                                    cEventIterator++;
                                }
                                // if missed on pipeline .. count this event
                            }
                            for(auto cIterator = cHitMatches.begin(); cIterator < cHitMatches.end(); cIterator++)
                            {
                                LOG(INFO) << BOLDMAGENTA << "\t.. " << +(*cIterator) << " events out of a possible " << +cEvents.size() << " with a hit in channel "
                                          << +cExpectedHits[std::distance(cHitMatches.begin(), cIterator)] << RESET;
                            }
                        }
                    }
                }
            }
        }
        // change the chip latency
        // int cStartLatency = (cLatencyOffsetStart > cLatencyOffsetStop) ? (fTPconfig.tpDelay-1*cLatencyOffsetStart) :
        // (fTPconfig.tpDelay+cLatencyOffsetStart); int cStopLatency = (cLatencyOffsetStart > cLatencyOffsetStop) ?
        // (fTPconfig.tpDelay+cLatencyOffsetStop) : (fTPconfig.tpDelay+cLatencyOffsetStop+1); LOG (INFO) << BOLDMAGENTA
        // << "Scanning L1A latency between " << +cStartLatency << " and " << +cStopLatency << RESET;
        // for( uint16_t cLatency=cStartLatency; cLatency < cStopLatency; cLatency++)
        // {
        //     // configure TP trigger machine
        //     double cDeltaTrigger = fTPconfig.tpDelay; // number of clock cycles after TP
        //     for(uint8_t cTPdelay=cInitialTPdleay; cTPdelay < cFinalTPdleay ; cTPdelay+=cTPdelayStep)
        //     {
        //         // configure TP
        //         setSameGlobalDac("TestPulseDelay", cTPdelay);
        //         setSameGlobalDac("TestPulsePotNodeSel",  0xFF-fTPconfig.tpAmplitude );
        //         // configure hit latency on all chips
        //         setSameDacBeBoard(cBoard, "TriggerLatency", cLatency);
        //         // set stub latency on back-end board
        //         uint16_t cStubLatency = cLatency - 1*cStubDelay ;
        //         fBeBoardInterface->WriteBoardReg (cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay",
        //         cStubLatency); fBeBoardInterface->ChipReSync ( cBoard ); // NEED THIS! LOG (INFO) << BOLDMAGENTA <<
        //         "TP amplitude set to " << +fTPconfig.tpAmplitude << " -- TP delay set to " << +cTPdelay << RESET;
        //         // set TP size
        //         for( uint16_t cThreshold=585; cThreshold < 586; cThreshold++)
        //         {
        //             // set stub window offset
        //             for( int cOffset=cInitialWindowOffset; cOffset <= cFinalWindowOffset; cOffset++)
        //             {
        //                 LOG (INFO) << BOLDMAGENTA << "Correlation window offset set to " << +cOffset << RESET;
        //                 for (auto& cHybrid : cBoard->fHybridVector)
        //                 {
        //                     for (auto& cChip : cHybrid->fReadoutChipVector)
        //                     {
        //                         if( std::find(pChipIds.begin(), pChipIds.end(), cChip->getId()) == pChipIds.end() )
        //                             continue;

        //                         if( cOffset < 0)
        //                         {
        //                             uint8_t cOffetReg = ((0xF-cOffset+1) << 4) | ((0xF-cOffset+1) << 0);
        //                             fReadoutChipInterface->WriteChipReg ( cChip, "CoincWind&Offset12", cOffetReg );
        //                             fReadoutChipInterface->WriteChipReg ( cChip, "CoincWind&Offset34", cOffetReg );
        //                         }
        //                         else
        //                         {
        //                             uint8_t cOffetReg = ((-1*cOffset) << 4) | ((-1*cOffset) << 0);
        //                             fReadoutChipInterface->WriteChipReg ( cChip, "CoincWind&Offset12", cOffetReg );
        //                             fReadoutChipInterface->WriteChipReg ( cChip, "CoincWind&Offset34", cOffetReg );
        //                         }
        //                         fReadoutChipInterface->WriteChipReg ( cChip, "VCth", cThreshold );
        //                     }
        //                 }

        //                 // read N events and compare hits and stubs to injected stub
        //                 for( size_t cAttempt=0; cAttempt < cAttempts ; cAttempt++)
        //                 {
        //                     this->zeroContainers();

        //                     fAttempt = cAttempt;
        //                     LOG (DEBUG) << BOLDBLUE << "Iteration# " << +fAttempt << RESET;
        //                     // send a resync
        //                     //if( cResync)
        //                     //    fBeBoardInterface->ChipReSync ( cBoard );
        //                     this->ReadNEvents ( cBoard , cEventsPerPoint);
        //                     const std::vector<Event*>& cEvents = this->GetEvents ( cBoard );
        //                     // matching
        //                     for (auto& cHybrid : cBoard->fHybridVector)
        //                     {
        //                         auto cHybridId = cHybrid->getId();
        //                         for (auto& cChip : cHybrid->fReadoutChipVector)
        //                         {
        //                             auto cChipId = cChip->getId();
        //                             if( std::find(pChipIds.begin(), pChipIds.end(), cChipId) == pChipIds.end() )
        //                                 continue;

        //                             TProfile2D* cMatchedHitsTP = static_cast<TProfile2D*> ( getHist ( cChip,
        //                             "MatchedHits_TestPulse" ) ); TProfile2D* cMatchedHitsLatency =
        //                             static_cast<TProfile2D*> ( getHist ( cChip, "HitLatency" ) ); TProfile2D*
        //                             cMatchedStubsLatency = static_cast<TProfile2D*> ( getHist ( cChip, "StubLatency"
        //                             ) ); TProfile* cMatchedStubsHist = static_cast<TProfile*>( getHist(cChip,
        //                             "PtCut") );

        //                             std::vector<uint8_t> cBendLUT =
        //                             static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT( cChip );
        //                             // each bend code is stored in this vector - bend encoding start at -7 strips,
        //                             increments by 0.5 strips uint8_t cBendCode = cBendLUT[ ((cStub.second+cOffset)/2.
        //                             - (-7.0))/0.5 ];

        //                             std::vector<uint8_t> cExpectedHits =
        //                             static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern( cChip,
        //                             cStub.first, cStub.second  ); LOG (DEBUG) << BOLDMAGENTA << "Injected a stub with
        //                             seed " << +cStub.first << " with bend " << +cStub.second << RESET; for(auto
        //                             cHitExpected : cExpectedHits )
        //                                 LOG (DEBUG) << BOLDMAGENTA << "\t.. expect a hit in channel " <<
        //                                 +cHitExpected << RESET;

        //                             auto cEventIterator = cEvents.begin();
        //                             size_t cEventCounter=0;
        //                             LOG (INFO) << BOLDMAGENTA << "CBC" << +cChip->getId() << RESET;
        //                             size_t cMatchedHits=0;
        //                             size_t cMatchedStubs=0;
        //                             for( size_t cEventIndex=0; cEventIndex < cEventsPerPoint ; cEventIndex++) // for
        //                             each event
        //                             {
        //                                 uint32_t cPipeline_first=0;
        //                                 uint32_t cBxId_first=0;
        //                                 bool cMissedEvent=false;

        //                                 if( cEventIndex == 0 )
        //                                     LOG (INFO) << BOLDMAGENTA << "'\tEvent " << +cEventIndex << RESET;
        //                                 bool cIncorrectPipeline=false;
        //                                 for(size_t cTriggerIndex=0; cTriggerIndex <= cTriggerMult; cTriggerIndex++)
        //                                 // cTriggerMult consecutive triggers were sent
        //                                 {
        //                                     auto cEvent = *cEventIterator;
        //                                     auto cBxId = cEvent->BxId(cHybrid->getId());
        //                                     auto cErrorBit = cEvent->Error( cHybridId , cChipId );
        //                                     uint32_t cL1Id = cEvent->L1Id( cHybridId, cChipId );
        //                                     uint32_t cPipeline = cEvent->PipelineAddress( cHybridId, cChipId );
        //                                     cBxId_first = (cTriggerIndex == 0 ) ? cBxId : cBxId_first;
        //                                     cPipeline_first = (cTriggerIndex == 0 ) ? cPipeline : cPipeline_first;
        //                                     bool cCountEvent = ( static_cast<size_t>(cPipeline - cPipeline_first) ==
        //                                     cTriggerIndex ); if(cCountEvent)
        //                                     {
        //                                         //hits
        //                                         auto cHits = cEvent->GetHits( cHybridId, cChipId ) ;
        //                                         size_t cMatched=0;
        //                                         int cLatency_eq = cLatency  - (cPipeline - cPipeline_first);
        //                                         double cTime_ns = 1*(static_cast<float>(fTPconfig.tpDelay -
        //                                         cLatency_eq)*25. + (25.0 - cTPdelay)); for( auto cExpectedHit :
        //                                         cExpectedHits )
        //                                         {
        //                                             bool cMatchFound = std::find(  cHits.begin(), cHits.end(),
        //                                             cExpectedHit) != cHits.end(); cMatched += cMatchFound;
        //                                             //cMatchedHits->Fill(cTriggerIndex,cPipeline,
        //                                             static_cast<int>(cMatchFound) );
        //                                             //cMatchedHitsEye->Fill(fPhaseTap, cTriggerIndex,
        //                                             static_cast<int>(cMatchFound) );
        //                                         }
        //                                         if( cMatched == cExpectedHits.size() )
        //                                         {
        //                                             cMatchedHits ++;
        //                                             //cMatchedHitsLatency->Fill( cLatency_eq , cTPamplitude , 1 );
        //                                             //cMatchedHitsTP->Fill( cTime_ns , cTPamplitude , 1);
        //                                         }
        //                                         //else
        //                                         //{
        //                                         //    cMatchedHitsLatency->Fill( cLatency_eq , cTPamplitude , 0 );
        //                                         //    cMatchedHitsTP->Fill( cTime_ns , cTPamplitude , 0);
        //                                         //}
        //                                         //stubs
        //                                         auto cStubs = cEvent->StubVector( cHybridId, cChipId );
        //                                         int cNmatchedStubs=0;
        //                                         for( auto cHybridStub : cStubs )
        //                                         {
        //                                             LOG (DEBUG) << BOLDMAGENTA << "\t.. expected seed is " <<
        //                                             +cStub.first << " measured seed is " <<
        //                                             +cHybridStub.getPosition() << RESET; LOG (DEBUG) << BOLDMAGENTA
        //                                             << "\t.. expected bend code is 0x" << std::hex << +cBendCode <<
        //                                             std::dec << " measured bend code is 0x" << std::hex <<
        //                                             +cHybridStub.getBend() << std::dec << RESET; bool cMatchFound =
        //                                             (cHybridStub.getPosition() == cStub.first &&
        //                                             cHybridStub.getBend() == cBendCode); cMatchedStubsHist->Fill(
        //                                             cOffset , cMatchFound); cNmatchedStubs +=
        //                                             static_cast<int>(cMatchFound);
        //                                         }
        //                                         if( cNmatchedStubs == 1 )
        //                                         {
        //                                             cMatchedStubs ++;
        //                                             //cMatchedStubsLatency->Fill( cLatency_eq - cStubDelay  ,
        //                                             cTPamplitude , 1 );
        //                                         }
        //                                         //else
        //                                         //    cMatchedStubsLatency->Fill( cLatency_eq - cStubDelay  ,
        //                                         cTPamplitude , 0 );

        //                                         int cBin = cMatchedHitsLatency->GetXaxis()->FindBin( cLatency_eq +
        //                                         cTPdelay*(1.0/25) ); if( cEventIndex == 0 )
        //                                             LOG (INFO) << BOLDMAGENTA << "\t\t.. Trigger " << +cTriggerIndex
        //                                             << " Pipeline is " << +cPipeline << " -Latency is  " << +cLatency
        //                                             << " TP fine delay is " << +cTPdelay << " -- threshold of " <<
        //                                             +cThreshold << " Time in ns is " << +cTime_ns << " .Found " <<
        //                                             +cMatched << " matched hits and " <<  +cNmatchedStubs << "
        //                                             matched stubs." <<RESET;
        //                                             //LOG (INFO) << BOLDMAGENTA << "\t\t.. Trigger " <<
        //                                             +cTriggerIndex << " Pipeline is " << +cPipeline << " -Latency is
        //                                             " << +cLatency_eq << " , trigger sent " <<
        //                                             fTPconfig.tpDelay+cLatencyOffset << " clocks after fast reset, TP
        //                                             delay is " << +cTPdelay << " L1A " << cLatency << " clocks. Time
        //                                             in ns is " << +cTime_ns << " .Found " << +cMatched << " matched
        //                                             hits and " <<  +cNmatchedStubs << " matched stubs." <<RESET;
        //                                     }
        //                                     else
        //                                         cIncorrectPipeline = cIncorrectPipeline || true;
        //                                     cEventIterator++;
        //                                 }
        //                                 // if missed on pipeline .. count this event

        //                             }
        //                         }
        //                     }
        //                     //this->print(pChipIds);
        //                 }
        //             }
        //         }

        //         // for( uint16_t cTPamplitude=cInitialAmpl; cTPamplitude < cFinalAmpl; cTPamplitude+= cAmplStep)
        //         // {
        //         //     // set stub window offset
        //         //     for( int cOffset=cInitialWindowOffset; cOffset <= cFinalWindowOffset; cOffset++)
        //         //     {
        //         //         LOG (INFO) << BOLDMAGENTA << "Correlation window offset set to " << +cOffset << RESET;
        //         //         for (auto& cHybrid : cBoard->fHybridVector)
        //         //         {
        //         //             for (auto& cChip : cHybrid->fReadoutChipVector)
        //         //             {
        //         //                 if( std::find(pChipIds.begin(), pChipIds.end(), cChip->getId()) == pChipIds.end()
        //         )
        //         //                     continue;

        //         //                 if( cOffset < 0)
        //         //                 {
        //         //                     uint8_t cOffetReg = ((0xF-cOffset+1) << 4) | ((0xF-cOffset+1) << 0);
        //         //                     fReadoutChipInterface->WriteChipReg ( cChip, "CoincWind&Offset12", cOffetReg
        //         );
        //         //                     fReadoutChipInterface->WriteChipReg ( cChip, "CoincWind&Offset34", cOffetReg
        //         );
        //         //                 }
        //         //                 else
        //         //                 {
        //         //                     uint8_t cOffetReg = ((-1*cOffset) << 4) | ((-1*cOffset) << 0);
        //         //                     fReadoutChipInterface->WriteChipReg ( cChip, "CoincWind&Offset12", cOffetReg
        //         );
        //         //                     fReadoutChipInterface->WriteChipReg ( cChip, "CoincWind&Offset34", cOffetReg
        //         );
        //         //                 }
        //         //             }
        //         //         }

        //         //         // read N events and compare hits and stubs to injected stub
        //         //         for( size_t cAttempt=0; cAttempt < cAttempts ; cAttempt++)
        //         //         {
        //         //             this->zeroContainers();

        //         //             fAttempt = cAttempt;
        //         //             LOG (DEBUG) << BOLDBLUE << "Iteration# " << +fAttempt << RESET;
        //         //             // send a resync
        //         //             if( cResync)
        //         //                 fBeBoardInterface->ChipReSync ( cBoard );
        //         //             this->ReadNEvents ( cBoard , cEventsPerPoint);
        //         //             const std::vector<Event*>& cEvents = this->GetEvents ( cBoard );
        //         //             // matching
        //         //             for (auto& cHybrid : cBoard->fHybridVector)
        //         //             {
        //         //                 auto cHybridId = cHybrid->getId();
        //         //                 for (auto& cChip : cHybrid->fReadoutChipVector)
        //         //                 {
        //         //                     auto cChipId = cChip->getId();
        //         //                     if( std::find(pChipIds.begin(), pChipIds.end(), cChipId) == pChipIds.end() )
        //         //                         continue;

        //         //                     TProfile2D* cMatchedHitsTP = static_cast<TProfile2D*> ( getHist ( cChip,
        //         "MatchedHits_TestPulse" ) );
        //         //                     TProfile2D* cMatchedHitsLatency = static_cast<TProfile2D*> ( getHist ( cChip,
        //         "HitLatency" ) );
        //         //                     TProfile2D* cMatchedStubsLatency = static_cast<TProfile2D*> ( getHist ( cChip,
        //         "StubLatency" ) );
        //         //                     TProfile* cMatchedStubsHist = static_cast<TProfile*>( getHist(cChip, "PtCut")
        //         );

        //         //                     std::vector<uint8_t> cBendLUT =
        //         static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT( cChip );
        //         //                     // each bend code is stored in this vector - bend encoding start at -7 strips,
        //         increments by 0.5 strips
        //         //                     uint8_t cBendCode = cBendLUT[ ((cStub.second+cOffset)/2. - (-7.0))/0.5 ];

        //         //                     std::vector<uint8_t> cExpectedHits =
        //         static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern( cChip, cStub.first,
        //         cStub.second  );
        //         //                     LOG (DEBUG) << BOLDMAGENTA << "Injected a stub with seed " << +cStub.first <<
        //         " with bend " << +cStub.second << RESET;
        //         //                     for(auto cHitExpected : cExpectedHits )
        //         //                         LOG (DEBUG) << BOLDMAGENTA << "\t.. expect a hit in channel " <<
        //         +cHitExpected << RESET;

        //         //                     auto cEventIterator = cEvents.begin();
        //         //                     size_t cEventCounter=0;
        //         //                     LOG (INFO) << BOLDMAGENTA << "CBC" << +cChip->getId() << RESET;
        //         //                     size_t cMatchedHits=0;
        //         //                     size_t cMatchedStubs=0;
        //         //                     for( size_t cEventIndex=0; cEventIndex < cEventsPerPoint ; cEventIndex++) //
        //         for each event
        //         //                     {
        //         //                         uint32_t cPipeline_first=0;
        //         //                         uint32_t cBxId_first=0;
        //         //                         bool cMissedEvent=false;

        //         //                         if( cEventIndex == 0 )
        //         //                             LOG (INFO) << BOLDMAGENTA << "'\tEvent " << +cEventIndex << RESET;
        //         //                         bool cIncorrectPipeline=false;
        //         //                         for(size_t cTriggerIndex=0; cTriggerIndex <= cTriggerMult;
        //         cTriggerIndex++) // cTriggerMult consecutive triggers were sent
        //         //                         {
        //         //                             auto cEvent = *cEventIterator;
        //         //                             auto cBxId = cEvent->BxId(cHybrid->getId());
        //         //                             auto cErrorBit = cEvent->Error( cHybridId , cChipId );
        //         //                             uint32_t cL1Id = cEvent->L1Id( cHybridId, cChipId );
        //         //                             uint32_t cPipeline = cEvent->PipelineAddress( cHybridId, cChipId );
        //         //                             cBxId_first = (cTriggerIndex == 0 ) ? cBxId : cBxId_first;
        //         //                             cPipeline_first = (cTriggerIndex == 0 ) ? cPipeline : cPipeline_first;
        //         //                             bool cCountEvent = ( static_cast<size_t>(cPipeline - cPipeline_first)
        //         == cTriggerIndex );
        //         //                             if(cCountEvent)
        //         //                             {
        //         //                                 //hits
        //         //                                 auto cHits = cEvent->GetHits( cHybridId, cChipId ) ;
        //         //                                 size_t cMatched=0;
        //         //                                 int cLatency_eq = cLatency  - (cPipeline - cPipeline_first);
        //         //                                 double cTime_ns = 1*(static_cast<float>(fTPconfig.tpDelay -
        //         cLatency_eq)*25. + (25.0 - cTPdelay));
        //         //                                 for( auto cExpectedHit : cExpectedHits )
        //         //                                 {
        //         //                                     bool cMatchFound = std::find(  cHits.begin(), cHits.end(),
        //         cExpectedHit) != cHits.end();
        //         //                                     cMatched += cMatchFound;
        //         //                                     //cMatchedHits->Fill(cTriggerIndex,cPipeline,
        //         static_cast<int>(cMatchFound) );
        //         //                                     //cMatchedHitsEye->Fill(fPhaseTap, cTriggerIndex,
        //         static_cast<int>(cMatchFound) );
        //         //                                 }
        //         //                                 if( cMatched == cExpectedHits.size() )
        //         //                                 {
        //         //                                     cMatchedHits ++;
        //         //                                     cMatchedHitsLatency->Fill( cLatency_eq , cTPamplitude , 1 );
        //         //                                     cMatchedHitsTP->Fill( cTime_ns , cTPamplitude , 1);
        //         //                                 }
        //         //                                 else
        //         //                                 {
        //         //                                     cMatchedHitsLatency->Fill( cLatency_eq , cTPamplitude , 0 );
        //         //                                     cMatchedHitsTP->Fill( cTime_ns , cTPamplitude , 0);
        //         //                                 }
        //         //                                 //stubs
        //         //                                 auto cStubs = cEvent->StubVector( cHybridId, cChipId );
        //         //                                 int cNmatchedStubs=0;
        //         //                                 for( auto cHybridStub : cStubs )
        //         //                                 {
        //         //                                     LOG (DEBUG) << BOLDMAGENTA << "\t.. expected seed is " <<
        //         +cStub.first << " measured seed is " << +cHybridStub.getPosition() << RESET;
        //         //                                     LOG (DEBUG) << BOLDMAGENTA << "\t.. expected bend code is 0x"
        //         << std::hex << +cBendCode << std::dec << " measured bend code is 0x" << std::hex <<
        //         +cHybridStub.getBend() << std::dec << RESET;
        //         //                                     bool cMatchFound = (cHybridStub.getPosition() == cStub.first
        //         && cHybridStub.getBend() == cBendCode);
        //         //                                     cMatchedStubsHist->Fill( cOffset , cMatchFound);
        //         //                                     cNmatchedStubs += static_cast<int>(cMatchFound);
        //         //                                 }
        //         //                                 if( cNmatchedStubs == 1 )
        //         //                                 {
        //         //                                     cMatchedStubs ++;
        //         //                                     cMatchedStubsLatency->Fill( cLatency_eq - cStubDelay  ,
        //         cTPamplitude , 1 );
        //         //                                 }
        //         //                                 else
        //         //                                     cMatchedStubsLatency->Fill( cLatency_eq - cStubDelay  ,
        //         cTPamplitude , 0 );

        //         //                                 int cBin = cMatchedHitsLatency->GetXaxis()->FindBin( cLatency_eq +
        //         cTPdelay*(1.0/25) );
        //         //                                 if( cEventIndex == 0 )
        //         //                                     LOG (INFO) << BOLDMAGENTA << "\t\t.. Trigger " <<
        //         +cTriggerIndex << " Pipeline is " << +cPipeline << " -Latency is  " << +cLatency_eq << " , trigger
        //         sent " << fTPconfig.tpDelay+cLatencyOffset << " clocks after fast reset, TP delay is " << +cTPdelay
        //         << " L1A " << cLatency << " clocks. Time in ns is " << +cTime_ns << " .Found " << +cMatched << "
        //         matched hits and " <<  +cNmatchedStubs << " matched stubs." <<RESET;
        //         //                             }
        //         //                             else
        //         //                                 cIncorrectPipeline = cIncorrectPipeline || true;
        //         //                             cEventIterator++;
        //         //                         }
        //         //                         // if missed on pipeline .. count this event

        //         //                     }
        //         //                 }
        //         //             }
        //         //             //this->print(pChipIds);
        //         //         }
        //         //     }
        //         // }
        //     }
        // }
        if(cConfigureTriggerMult) fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cBoardTriggerMult);
    }

    // if TP was used - disable it
    // disable TP
    this->enableTestPulse(false);
    this->SetTestPulse(false);
    setSameGlobalDac("TestPulsePotNodeSel", 0x00);

    // unmask all channels and reset offsets
    // also re-configure thresholds + hit/stub detect logic to original values
    // and re-load configuration of fast command block from register map loaded from xml file
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cThresholdsThisBoard = fThresholds.at(cBoard->getIndex());
        auto& cLogicThisBoard      = fLogic.at(cBoard->getIndex());
        auto& cHIPsThisBoard       = fHIPs.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cThresholdsThisOpticalGroup = cThresholdsThisBoard->at(cOpticalGroup->getIndex());
            auto& cLogicThisOpticalGroup      = cLogicThisBoard->at(cOpticalGroup->getIndex());
            auto& cHIPsThisOpticalGroup       = cHIPsThisBoard->at(cOpticalGroup->getIndex());

            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cThresholdsThisHybrid = cThresholdsThisOpticalGroup->at(cHybrid->getIndex());
                auto& cLogicThisHybrid      = cLogicThisOpticalGroup->at(cHybrid->getIndex());
                auto& cHIPsThisHybrid       = cHIPsThisOpticalGroup->at(cHybrid->getIndex());
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerHybrid*>(cHybrid)->getLinkId());
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(theChip, false);
                    // set offsets back to default value
                    fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset12", (0 << 4) | (0 << 0));
                    fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset34", (0 << 4) | (0 << 0));

                    LOG(DEBUG) << BOLDBLUE << "Setting threshold on CBC" << +cChip->getId() << " back to " << +cThresholdsThisHybrid->at(cChip->getIndex())->getSummary<uint16_t>() << RESET;
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theChip, "VCth", cThresholdsThisHybrid->at(cChip->getIndex())->getSummary<uint16_t>());
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theChip, "Pipe&StubInpSel&Ptwidth", cLogicThisHybrid->at(cChip->getIndex())->getSummary<uint16_t>());
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theChip, "HIP&TestMode", cHIPsThisHybrid->at(cChip->getIndex())->getSummary<uint16_t>());
                }
            }
        }
        //
        LOG(DEBUG) << BOLDBLUE << "Re-loading original coonfiguration of fast command block from hardware description file [.xml] " << RESET;
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFastCommandBlock(static_cast<BeBoard*>(cBoard));
    }
}
// check hits and stubs using noise
void DataChecker::DataCheck(std::vector<uint8_t> pChipIds, uint8_t pSeed, int pBend)
{
    LOG(INFO) << BOLDBLUE << "Starting data checker.." << RESET;
    std::pair<uint8_t, int> cStub;

    // Prepare container to hold  measured occupancy
    DetectorDataContainer cMeasurement;
    fDetectorDataContainer = &cMeasurement;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    // use xml to figure out whether to use noise or charge injection
    bool pWithNoise = true; // default is to use noise
    auto cSetting   = fSettingsMap.find("UseNoise");
    if(cSetting != std::end(fSettingsMap)) pWithNoise = (cSetting->second == 1);

    // get number of events from xml
    cSetting                 = fSettingsMap.find("Nevents");
    uint32_t cEventsPerPoint = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;

    // get trigger rate from xml
    cSetting                   = fSettingsMap.find("TriggerRate");
    bool     cConfigureTrigger = (cSetting != std::end(fSettingsMap));
    uint16_t cTriggerRate      = cConfigureTrigger ? cSetting->second : 100;
    // get trigger multiplicity from xml
    cSetting                       = fSettingsMap.find("TriggerMultiplicity");
    bool     cConfigureTriggerMult = (cSetting != std::end(fSettingsMap));
    uint16_t cTriggerMult          = cConfigureTriggerMult ? cSetting->second : 0;

    // get stub delay scan range from xml
    cSetting                  = fSettingsMap.find("StubDelay");
    bool cModifyStubScanRange = (cSetting != std::end(fSettingsMap));
    int  cStubDelay           = cModifyStubScanRange ? cSetting->second : 48;

    // get injected stub from xmls
    cSetting     = fSettingsMap.find("StubSeed");
    cStub.first  = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 10;
    cSetting     = fSettingsMap.find("StubBend");
    cStub.second = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    LOG(DEBUG) << BOLDBLUE << "Injecting a stub in position " << +cStub.first << " with bend " << cStub.second << " to test data integrity..." << RESET;

    // get target threshold
    cSetting                  = fSettingsMap.find("Threshold");
    uint16_t cTargetThreshold = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 580;

    // get number of attempts
    cSetting         = fSettingsMap.find("Attempts");
    size_t cAttempts = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 10;

    // get injected stub from xmls
    cSetting = fSettingsMap.find("ManualPhaseAlignment");
    if((cSetting != std::end(fSettingsMap)))
    {
        // fPhaseTap = cSetting->second ;
        for(auto cBoard: *fDetectorContainer)
        {
            //     for(auto cOpticalGroup : *cBoard)
            //     {
            //         for (auto cHybrid : *cOpticalGroup)
            //         {
            //             auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            //             if( cCic != NULL )
            //             {
            //                 for(auto cChipId : pChipIds )
            //                 {
            //                     // bool cConfigured = fCicInterface->SetStaticPhaseAlignment(  cCic , cChipId ,  0 ,
            //                     fPhaseTap);
            //                 }
            //             }
            //         }
            //     }
            fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
        }
    }

    // get number of attempts
    cSetting      = fSettingsMap.find("Mode");
    uint8_t cMode = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;

    // get latency offset
    cSetting           = fSettingsMap.find("LatencyOffset");
    int cLatencyOffset = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;

    // resync between attempts
    cSetting     = fSettingsMap.find("ReSync");
    bool cResync = (cSetting != std::end(fSettingsMap)) ? (cSetting->second == 1) : false;

    uint16_t cTPdelay = 0;

    // if TP is used - enable it
    if(!pWithNoise)
    {
        cSetting              = fSettingsMap.find("PulseShapePulseAmplitude");
        fTPconfig.tpAmplitude = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;

        // get TP delay
        cSetting = fSettingsMap.find("PulseShapePulseDelay");
        cTPdelay = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;

        // set-up for TP
        fAllChan                     = true;
        fMaskChannelsFromOtherGroups = !this->fAllChan;
        this->SetTestPulse(true);
        setSameGlobalDac("TestPulsePotNodeSel", 0xFF - fTPconfig.tpAmplitude);
        setSameGlobalDac("TestPulseDelay", cTPdelay);
        cStub.second = 0; // for now - with TP can only test bend code of 0
    }

    // configure FE chips so that stubs are detected [i.e. make sure HIP
    // suppression is off ]
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // configure CBCs
                for(auto cChip: *cHybrid)
                {
                    // switch off HitOr
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(static_cast<ReadoutChip*>(cChip), "HitOr", 0);
                    // enable stub logic
                    if(cMode == 0)
                        static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(static_cast<ReadoutChip*>(cChip), "Sampled", true, true);
                    else
                        static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(static_cast<ReadoutChip*>(cChip), "Latched", true, true);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(static_cast<ReadoutChip*>(cChip), false, false, 0);
                }
            }
        }
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    }

    // generate stubs in exactly chips with IDs that match pChipIds
    std::vector<int> cExpectedHits(0);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    if(std::find(pChipIds.begin(), pChipIds.end(), cChip->getId()) != pChipIds.end())
                    {
                        std::vector<uint8_t> cBendLUT = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theChip);
                        // both stub and bend are in units of half strips
                        // if using TP then always inject a stub with bend 0 ..
                        // later will use offset window to modify bend [ should probably put this in inject stub ]
                        uint8_t cBend_halfStrips = (pWithNoise) ? cStub.second : 0;
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theChip, {cStub.first}, {cBend_halfStrips}, pWithNoise);
                        // each bend code is stored in this vector - bend encoding start at -7 strips, increments by 0.5
                        // strips set offsets needs to be fixed
                        /*
                        uint8_t cOffsetCode = static_cast<uint8_t>(std::fabs(cBend_strips*2)) |
                        (std::signbit(-1*cBend_strips) << 3);
                        //uint8_t cOffsetCode = static_cast<uint8_t>(std::fabs(cBend_strips*2)) |
                        (std::signbit(-1*cBend_strips) << 3);
                        // set offsets
                        uint8_t cOffetReg = (cOffsetCode << 4) | (cOffsetCode << 0);
                        LOG (INFO) << BOLDBLUE << "\t..--- bend code is 0x" << std::hex << +cBendCode << std::dec << "
                        ..setting offset window to  " << std::bitset<8>(cOffetReg) << RESET;
                        fReadoutChipInterface->WriteChipReg ( cChip, "CoincWind&Offset12", cOffetReg );
                        fReadoutChipInterface->WriteChipReg ( cChip, "CoincWind&Offset34", cOffetReg );
                        */
                        if(!pWithNoise) fReadoutChipInterface->enableInjection(theChip, true);
                    }
                    else if(!pWithNoise)
                        static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theChip, "VCth", cTargetThreshold);
                    else
                        static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(theChip, true);
                }
            }
        }
    }

    // zero containers
    // for( uint8_t cPackageDelay = 0 ; cPackageDelay < 8; cPackageDelay++)
    //{
    this->zeroContainers();
    // measure
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        // LOG (INFO) << BOLDMAGENTA << "Setting stub package delay to " << +cPackageDelay << RESET;
        // fBeBoardInterface->WriteBoardReg (cBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay",
        // cPackageDelay); static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Bx0Alignment();

        uint16_t cBoardTriggerMult = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
        uint16_t cBoardTriggerRate = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.user_trigger_frequency");
        LOG(DEBUG) << BOLDBLUE << "Trigger rate is set to " << +cBoardTriggerRate << " kHz" << RESET;
        LOG(DEBUG) << BOLDBLUE << "Trigger multiplicity is set to " << +cBoardTriggerMult << " consecutive triggers per L1A." << RESET;
        if(cConfigureTrigger && pWithNoise)
        {
            LOG(DEBUG) << BOLDBLUE << "Modifying trigger rate to be " << +cTriggerRate << " kHz for DataTest" << RESET;
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.user_trigger_frequency", cTriggerRate);
        }
        if(cConfigureTriggerMult)
        {
            LOG(DEBUG) << BOLDBLUE << "Modifying trigger multiplicity to be " << +(1 + cTriggerMult) << " consecutive triggers per L1A for DataTest" << RESET;
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cTriggerMult);
        }

        // using charge injection
        if(!pWithNoise)
        {
            // configure test pulse trigger
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(fTPconfig.firmwareTPdelay, fTPconfig.tpDelay, fTPconfig.tpSequence, fTPconfig.tpFastReset);
            // set trigger latency
            uint16_t cLatency = fTPconfig.tpDelay + cLatencyOffset; //+1;
            this->setSameDacBeBoard(theBoard, "TriggerLatency", cLatency);
            // set stub latency
            uint16_t cStubLatency = cLatency - 1 * cStubDelay;
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
            LOG(DEBUG) << BOLDBLUE << "Latency set to " << +cLatency << "...\tStub latency set to " << +cStubLatency << RESET;
            fBeBoardInterface->ChipReSync(theBoard);
        }
        // read N events and compare hits and stubs to injected stub
        for(size_t cAttempt = 0; cAttempt < cAttempts; cAttempt++)
        {
            fAttempt = cAttempt;
            LOG(INFO) << BOLDBLUE << "Iteration# " << +fAttempt << RESET;
            // send a resync
            if(cResync) fBeBoardInterface->ChipReSync(theBoard);
            this->ReadNEvents(theBoard, cEventsPerPoint);
            this->matchEvents(theBoard, pChipIds, cStub);
            this->print(pChipIds);
        }

        // and set it back to what it was
        if(cConfigureTrigger) fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.user_trigger_frequency", cBoardTriggerRate);
        if(cConfigureTriggerMult) fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cBoardTriggerMult);
    }
    //}

    // if TP was used - disable it
    if(!pWithNoise)
    {
        // disable TP
        this->enableTestPulse(false);
        this->SetTestPulse(false);
        setSameGlobalDac("TestPulsePotNodeSel", 0x00);
    }

    // unmask all channels and reset offsets
    // also re-configure thresholds + hit/stub detect logic to original values
    // and re-load configuration of fast command block from register map loaded from xml file
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cThresholdsThisBoard = fThresholds.at(cBoard->getIndex());
        auto& cLogicThisBoard      = fLogic.at(cBoard->getIndex());
        auto& cHIPsThisBoard       = fHIPs.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cThresholdsThisOpticalGroup = cThresholdsThisBoard->at(cOpticalGroup->getIndex());
            auto& cLogicThisOpticalGroup      = cLogicThisBoard->at(cOpticalGroup->getIndex());
            auto& cHIPsThisOpticalGroup       = cHIPsThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cThresholdsThisHybrid = cThresholdsThisOpticalGroup->at(cOpticalGroup->getIndex());
                auto& cLogicThisHybrid      = cLogicThisOpticalGroup->at(cOpticalGroup->getIndex());
                auto& cHIPsThisHybrid       = cHIPsThisOpticalGroup->at(cOpticalGroup->getIndex());
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerHybrid*>(cHybrid)->getLinkId());
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(theChip, false);
                    // set offsets back to default value
                    fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset12", (0 << 4) | (0 << 0));
                    fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset34", (0 << 4) | (0 << 0));

                    LOG(DEBUG) << BOLDBLUE << "Setting threshold on CBC" << +cChip->getId() << " back to " << +cThresholdsThisHybrid->at(cChip->getIndex())->getSummary<uint16_t>() << RESET;
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theChip, "VCth", cThresholdsThisHybrid->at(cChip->getIndex())->getSummary<uint16_t>());
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theChip, "Pipe&StubInpSel&Ptwidth", cLogicThisHybrid->at(cChip->getIndex())->getSummary<uint16_t>());
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theChip, "HIP&TestMode", cHIPsThisHybrid->at(cChip->getIndex())->getSummary<uint16_t>());
                }
            }
        }
        //
        LOG(DEBUG) << BOLDBLUE << "Re-loading original coonfiguration of fast command block from hardware description file [.xml] " << RESET;
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFastCommandBlock(static_cast<BeBoard*>(cBoard));
        //
        // fBeBoardInterface->ChipReSync ( cBoard );
    }
}

void DataChecker::StubCheck(std::vector<uint8_t> pChipIds)
{
    std::string  cDAQFileName    = "StubCheck.daq";
    FileHandler* cDAQFileHandler = new FileHandler(cDAQFileName, 'w');

    uint8_t cSweepPackageDelay = this->findValueInSettings("SweepPackageDelay");
    uint8_t cSweepStubDelay    = this->findValueInSettings("SweepStubDelay");

    uint8_t cFirmwareTPdelay      = this->findValueInSettings("StubFWTestPulseDelay");
    uint8_t cFirmwareTriggerDelay = this->findValueInSettings("StubFWTriggerDelay");

    // set-up for TP
    fAllChan                     = true;
    fMaskChannelsFromOtherGroups = !this->fAllChan;
    SetTestAllChannels(fAllChan);
    // enable TP injection
    // enableTestPulse( true );
    // configure test pulse trigger
    uint8_t cEnableFastReset = 0;
    uint8_t cEnableTP        = 1;
    uint8_t cEnableL1A       = 1;
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(cFirmwareTPdelay, cFirmwareTriggerDelay, 1000, cEnableFastReset, cEnableTP, cEnableL1A);

    // set threshold
    uint8_t  cTestPulseAmplitude = 0xFF - 100;
    uint16_t cThreshold          = this->findValueInSettings("StubThreshold");
    uint8_t  cTPgroup            = 0;
    // enable TP and set TP amplitude
    fTestPulse = true;
    setSameGlobalDac("TestPulsePotNodeSel", cTestPulseAmplitude);
    setSameGlobalDac("VCth", cThreshold);
    setSameGlobalDac("TestPulseDelay", 0);

    // seeds and bends needed to generate fixed pattern on SLVS lines carrying
    // stub information from CBCs --> CICs
    int     cBend       = 0;
    uint8_t cFirstSeed  = static_cast<uint8_t>(2 * (1 + std::floor((cTPgroup * 2 + 16 * 0) / 2.))); // in half strips
    uint8_t cSecondSeed = static_cast<uint8_t>(2 * (1 + std::floor((cTPgroup * 2 + 16 * 3) / 2.))); // in half strips
    uint8_t cThirdSeed  = static_cast<uint8_t>(2 * (1 + std::floor((cTPgroup * 2 + 16 * 5) / 2.))); // in half strips

    std::vector<uint8_t> cSeeds{cFirstSeed}; // cThirdSeed};
    std::vector<int>     cBends(cSeeds.size(), cBend);

    LOG(INFO) << BOLDMAGENTA << "First stub expected to be " << std::bitset<8>(cFirstSeed) << RESET;
    LOG(INFO) << BOLDMAGENTA << "Second stub line expected to be " << std::bitset<8>(cSecondSeed) << RESET;
    LOG(INFO) << BOLDMAGENTA << "Third stub line expected to be " << std::bitset<8>(cThirdSeed) << RESET;
    bool cWithCIC = false;
    for(auto cBoard: *fDetectorContainer)
    {
        auto     cBeBoard = static_cast<BeBoard*>(cBoard);
        uint16_t cDelay   = fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse") - 1;
        setSameDacBeBoard(cBeBoard, "TriggerLatency", cDelay);
        fBeBoardInterface->ChipReSync(cBeBoard); // NEED THIS! ??

        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                cWithCIC   = cWithCIC || (cCic != NULL);
                for(auto cChip: *cHybrid)
                {
                    auto cReadoutChip          = static_cast<ReadoutChip*>(cChip);
                    auto cReadoutChipInterface = static_cast<CbcInterface*>(fReadoutChipInterface);
                    // std::vector<uint8_t> cBendLUT = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT( cChip
                    // );
                    // // each bend code is stored in this vector - bend encoding start at -7 strips, increments by 0.5
                    // strips cBendCode = cBendLUT[ (cBend/2. - (-7.0))/0.5 ];

                    if(std::find(pChipIds.begin(), pChipIds.end(), cChip->getId()) != pChipIds.end())
                    {
                        // first pattern - stubs lines 0,1,3
                        cReadoutChipInterface->injectStubs(cReadoutChip, cSeeds, cBends, false);
                        // set threshold back to low
                        // fReadoutChipInterface->WriteChipReg(cReadoutChip,"VCth",cThreshold);
                        // fReadoutChipInterface->WriteChipReg ( cReadoutChip, "TestPulseGroup", cTPgroup );
                        // switch off HitOr
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "HitOr", 0);
                        // enable stub logic
                        cReadoutChipInterface->selectLogicMode(cReadoutChip, "Sampled", true, true);
                        // set pT cut to maximum
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "PtCut", 14);
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "TestPulse", (int)1);
                    }
                    else
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "VCth", 100);
                }
            }
        }

        for(int cAttempt = 0; cAttempt < this->findValueInSettings("StubAttempts"); cAttempt++)
        {
            LOG(INFO) << BOLDMAGENTA << "Attempt#" << +cAttempt << RESET;
            auto cOriginalDelay = fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
            LOG(INFO) << BOLDMAGENTA << "\t..Stub package delay set to " << +cOriginalDelay << RESET;
            int cPackageDelayStart = (cSweepPackageDelay == 0) ? cOriginalDelay : 0;
            int cPackageDelayStop  = (cSweepPackageDelay == 0) ? cOriginalDelay + 1 : 8;
            cPackageDelayStop      = (cWithCIC) ? cPackageDelayStop : cPackageDelayStart + 1;
            for(int cPackageDelay = cPackageDelayStart; cPackageDelay < cPackageDelayStop; cPackageDelay++)
            {
                if(cWithCIC)
                {
                    fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay", cPackageDelay);
                    (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->Bx0Alignment();
                }

                auto cStubLatency    = fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
                int  cStubDelayStart = (cSweepStubDelay == 0) ? cStubLatency - 2 : 0;
                int  cStubDelayStop  = (cSweepStubDelay == 0) ? cStubLatency + 2 : cDelay;
                for(auto cStubDelay = cStubDelayStart; cStubDelay <= cStubDelayStop; cStubDelay++)
                {
                    fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubDelay);
                    LOG(INFO) << BOLDBLUE << "\t..L1A latency set to " << +cDelay << " stub latency set to " << +cStubDelay << " so delay is " << +(cDelay - cStubDelay) << RESET;

                    this->ReadNEvents(cBeBoard, 5);
                    const std::vector<Event*>& cEventsWithStubs = this->GetEvents();
                    LOG(INFO) << BOLDBLUE << +cEventsWithStubs.size() << " events read back from FC7 with ReadData" << RESET;
                    for(auto& cEvent: cEventsWithStubs)
                    {
                        auto cEventCount = cEvent->GetEventCount();
                        LOG(INFO) << BOLDBLUE << "\t\t..Event " << +cEventCount << RESET;
                        for(auto cOpticalGroup: *cBoard)
                        {
                            for(auto cHybrid: *cOpticalGroup)
                            {
                                if(cWithCIC)
                                {
                                    auto cBx = cEvent->BxId(cHybrid->getId());
                                    LOG(INFO) << BOLDBLUE << "\t\t..Hybrid " << +cHybrid->getId() << " BxID " << +cBx << RESET;
                                }
                                else
                                    LOG(INFO) << BOLDBLUE << "\t\t..Hybrid " << +cHybrid->getId() << RESET;

                                for(auto cChip: *cHybrid)
                                {
                                    auto cStubs = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                                    auto cHits  = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                                    if(cStubs.size() > 0)
                                        LOG(INFO) << BOLDGREEN << "\t\t\t...Found " << +cStubs.size() << " stubs in the readout."
                                                  << " and " << +cHits.size() << " hits." << RESET;
                                    else
                                        LOG(INFO) << BOLDRED << "\t\t\t...Found " << +cStubs.size() << " stubs in the readout."
                                                  << " and " << +cHits.size() << " hits." << RESET;

                                } // chip
                            }     // hybrid
                        }         // opticalGroup
                        SLinkEvent cSLev    = cEvent->GetSLinkEvent(cBeBoard);
                        auto       cPayload = cSLev.getData<uint32_t>();
                        cDAQFileHandler->setData(cPayload);
                    } // event
                }     // latency loop
                fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
            } // package delay loop
            if(cWithCIC) fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay", cOriginalDelay);
        } // attempt loop
    }     // board loop
    LOG(INFO) << BOLDBLUE << "Done!" << RESET;
    cDAQFileHandler->closeFile();
    delete cDAQFileHandler;
}

void DataChecker::StubCheckWNoise(std::vector<uint8_t> pChipIds)
{
    uint8_t cSweepPackageDelay = this->findValueInSettings("SweepPackageDelay");
    uint8_t cSweepStubDelay    = this->findValueInSettings("SweepStubDelay");
    bool    cWithCIC           = false;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                cWithCIC   = cWithCIC || (cCic != NULL);
                for(auto cChip: *cHybrid)
                {
                    auto cReadoutChip          = static_cast<ReadoutChip*>(cChip);
                    auto cReadoutChipInterface = static_cast<CbcInterface*>(fReadoutChipInterface);
                    if(std::find(pChipIds.begin(), pChipIds.end(), cChip->getId()) != pChipIds.end())
                    {
                        // first pattern - stubs lines 0,1,3
                        cReadoutChipInterface->injectStubs(cReadoutChip, {10}, {0}, true);
                        // switch off HitOr
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "HitOr", 0);
                        // enable stub logic
                        cReadoutChipInterface->selectLogicMode(cReadoutChip, "Sampled", true, true);
                        // set pT cut to maximum
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "PtCut", 14);
                    }
                    else
                    {
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "VCth", 100);
                    }
                } // chip
            }     // hybrid
        }         // hybrid

        // now want to see the CIC output
        (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, 5);
        auto cBeBoard       = static_cast<BeBoard*>(cBoard);
        auto cOriginalDelay = fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
        LOG(INFO) << BOLDMAGENTA << "Stub package delay set to " << +cOriginalDelay << RESET;
        int cPackageDelayStart = (cSweepPackageDelay == 0) ? cOriginalDelay : 0;
        int cPackageDelayStop  = (cSweepPackageDelay == 0) ? cOriginalDelay + 1 : 8;
        cPackageDelayStop      = (cWithCIC) ? cPackageDelayStop : cPackageDelayStart + 1;
        for(int cPackageDelay = cPackageDelayStart; cPackageDelay < cPackageDelayStop; cPackageDelay++)
        {
            if(cWithCIC)
            {
                fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay", cPackageDelay);
                (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->Bx0Alignment();
            }

            auto cStubLatency    = fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
            int  cStubDelayStart = (cSweepStubDelay == 0) ? cStubLatency : 0;
            int  cStubDelayStop  = (cSweepStubDelay == 0) ? cStubLatency + 1 : 100;
            for(auto cStubDelay = cStubDelayStart; cStubDelay <= cStubDelayStop; cStubDelay++)
            {
                fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubDelay);
                fBeBoardInterface->ChipReSync(cBeBoard); // NEED THIS! ??
                LOG(INFO) << BOLDBLUE << "Stub latency set to " << +cStubDelay << RESET;

                this->ReadNEvents(cBeBoard, 5);
                const std::vector<Event*>& cEventsWithStubs = this->GetEvents();
                LOG(INFO) << BOLDBLUE << +cEventsWithStubs.size() << " events read back from FC7 with ReadData" << RESET;
                for(auto& cEvent: cEventsWithStubs)
                {
                    auto cEventCount = cEvent->GetEventCount();
                    LOG(INFO) << BOLDBLUE << "Event " << +cEventCount << RESET;
                    for(auto cOpticalGroup: *cBoard)
                    {
                        for(auto cHybrid: *cOpticalGroup)
                        {
                            if(cWithCIC)
                            {
                                auto cBx = cEvent->BxId(cHybrid->getId());
                                LOG(INFO) << BOLDBLUE << "Hybrid " << +cHybrid->getId() << " BxID " << +cBx << RESET;
                            }
                            else
                                LOG(INFO) << BOLDBLUE << "Hybrid " << +cHybrid->getId() << RESET;

                            for(auto cChip: *cHybrid)
                            {
                                auto cStubs = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                                auto cHits  = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                                if(cStubs.size() > 0)
                                    LOG(INFO) << BOLDGREEN << "ROC#" << +cChip->getId() << " Found " << +cStubs.size() << " stubs in the readout."
                                              << " and " << +cHits.size() << " hits." << RESET;
                                else
                                    LOG(INFO) << BOLDRED << "ROC#" << +cChip->getId() << " Found " << +cStubs.size() << " stubs in the readout."
                                              << " and " << +cHits.size() << " hits." << RESET;
                            }
                        }
                    }
                    // SLinkEvent cSLev = cEvent->GetSLinkEvent (cBeBoard);
                    // auto cPayload = cSLev.getData<uint32_t>();
                    // cDAQFileHandler->setData(cPayload);
                }
            }
        }
    }
    LOG(INFO) << BOLDBLUE << "Done!" << RESET;
}
void DataChecker::writeObjects()
{
    this->SaveResults();
    fResultFile->Flush();
}
// State machine control functions
void DataChecker::Running() { Initialise(); }

void DataChecker::Stop()
{
    this->SaveResults();
    fResultFile->Flush();
    dumpConfigFiles();

    SaveResults();
    CloseResultFile();
    Destroy();
}

void DataChecker::Pause() {}

void DataChecker::Resume() {}

void DataChecker::MaskForStubs(BeBoard* pBoard, uint16_t pSeed, bool pSeedLayer)
{
    uint32_t cSeedStrip        = std::floor(pSeed / 2.0); // counting from 1
    size_t   cNumberOfChannels = 1 + (pSeed % 2 != 0);
    for(size_t cIndex = 0; cIndex < cNumberOfChannels; cIndex++)
    {
        int  cSeed      = (cSeedStrip - 1) + cIndex;
        auto cChipId    = cSeed / 127;
        auto cChannelId = 2 * (cSeed % 127) + !pSeedLayer;
        LOG(DEBUG) << BOLDMAGENTA << ".. need to unmask strip " << +cSeed << " -- so channel " << +cChannelId << " of CBC " << +cChipId << RESET;
        auto& cInjThisBoard = fInjections.at(pBoard->getIndex());
        for(auto cOpticalGroup: *pBoard)
        {
            auto& cInjThisOpticalGroup = cInjThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cInjThisHybrid = cInjThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getId() != cChipId) continue;

                    auto& cInjThisChip   = cInjThisHybrid->at(cChip->getIndex());
                    auto& cInjectedSeeds = cInjThisChip->getSummary<ChannelList>();
                    cInjectedSeeds.push_back(cChannelId);
                }
            }
        }
    }
}

// TBD : modify to be per strip
// as fast as measuring the noise
void DataChecker::HitCheck2S(BeBoard* pBoard)
{
    // in half strips
    const size_t NCHNLS               = 254;
    int          cBend                = 0;
    auto&        cInjThisBoard        = fInjections.at(pBoard->getIndex());
    auto&        cThThisBoard         = fThresholds.at(pBoard->getIndex());
    auto&        cMismatchesThisBoard = fDataMismatches.at(pBoard->getIndex());

    // get number of events from xml
    auto     cSetting        = fSettingsMap.find("Nevents");
    uint32_t cEventsPerPoint = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;

    LOG(INFO) << BOLDBLUE << "Injecting hits to verify data quality in the back-end" << RESET;
    // bool cUseNoiseInjection=true;
    std::random_device              cRandom;
    std::mt19937                    cGeneratorSeed(cRandom());
    std::uniform_int_distribution<> cDistribution(2, NCHNLS * 8);
    auto                            cRandomGen = [&]() { return cDistribution(cGeneratorSeed); };

    //
    size_t cNSeedsPerInjection = 2;
    int    cNinjections        = 100;
    LOG(INFO) << BOLDBLUE << "Generating seeds for injection pattern " << RESET;
    std::vector<int> cSeeds(cNinjections * cNSeedsPerInjection);
    std::generate(cSeeds.begin(), cSeeds.end(), cRandomGen);
    for(int cInjection = 0; cInjection < cNinjections; cInjection++)
    {
        if(cInjection % (cNinjections / 10) == 0) LOG(INFO) << BOLDMAGENTA << "Injection " << +cInjection << RESET;
        auto             cStart = cSeeds.begin() + cInjection * cNSeedsPerInjection;
        auto             cEnd   = cStart + cNSeedsPerInjection;
        std::vector<int> cStubSeeds(cStart, cEnd);
        std::sort(cStubSeeds.begin(), cStubSeeds.end());
        // simplifying to avoid
        // interchip region for now
        std::vector<int> cGoodSeeds(0);
        for(auto cSeed: cStubSeeds)
        {
            int cStrip = (cSeed) % NCHNLS;
            if(cStrip != 1)
            {
                if(cGoodSeeds.size() == 0)
                    cGoodSeeds.push_back(cSeed);
                else if(std::fabs(cSeed - cGoodSeeds[cGoodSeeds.size() - 1] > 5 * (cBend + 2)))
                    cGoodSeeds.push_back(cSeed);
            }
        }
        if(cGoodSeeds.size() != cStubSeeds.size()) LOG(DEBUG) << BOLDRED << "Threw away a seed" << RESET;
        if(cGoodSeeds.size() == 0) continue;

        for(auto cSeed: cGoodSeeds)
        {
            LOG(DEBUG) << BOLDMAGENTA << "Seed " << +cSeed << RESET;
            MaskForStubs(pBoard, cSeed, true);
            MaskForStubs(pBoard, cSeed + cBend, false);
        }

        // lower threshold and mask
        for(auto cOpticalGroup: *pBoard)
        {
            auto& cInjThisOpticalGroup = cInjThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cInjThisHybrid = cInjThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cInjThisChip = cInjThisHybrid->at(cChip->getIndex());
                    auto& cChannels    = cInjThisChip->getSummary<ChannelList>();
                    if(cChannels.size() > 0)
                    {
                        // channel mask
                        ChannelGroup<NCHNLS, 1> cChannelMask;
                        cChannelMask.disableAllChannels();
                        for(auto cChannel: cChannels) cChannelMask.enableChannel(cChannel);

                        std::bitset<NCHNLS> cBitset = std::bitset<NCHNLS>(cChannelMask.getBitset());
                        LOG(DEBUG) << BOLDBLUE << "Injecting stubs in chip " << +cChip->getId() << " channel mask is " << cBitset << RESET;

                        // lower threshold + apply mask
                        auto cReadoutChip = static_cast<ReadoutChip*>(cChip);
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "VCth", 900);
                        fReadoutChipInterface->maskChannelsGroup(cReadoutChip, &cChannelMask);
                    }
                }
            }
        }

        // read events
        this->ReadNEvents(pBoard, cEventsPerPoint);
        const std::vector<Event*>& cEvents = this->GetEvents();
        LOG(DEBUG) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadData" << RESET;
        // check for matches
        for(auto cOpticalGroup: *pBoard)
        {
            auto& cInjThisOpticalGroup        = cInjThisBoard->at(cOpticalGroup->getIndex());
            auto& cMismatchesThisOpticalGroup = cMismatchesThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cInjThisHybrid        = cInjThisOpticalGroup->at(cHybrid->getIndex());
                auto& cMismatchesThisHybrid = cMismatchesThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cInjThisChip        = cInjThisHybrid->at(cChip->getIndex());
                    auto& cMismatchesThisChip = cMismatchesThisHybrid->at(cChip->getIndex());
                    auto& cChannels           = cInjThisChip->getSummary<ChannelList>();
                    auto& cMismatched         = cMismatchesThisChip->getSummary<uint32_t>();
                    if(cChannels.size() == 0) continue;

                    for(auto& cEvent: cEvents)
                    {
                        auto cBx         = cEvent->BxId(cHybrid->getId());
                        auto cEventCount = cEvent->GetEventCount();
                        LOG(DEBUG) << BOLDBLUE << "Event " << +cEventCount << " Hybrid " << +cHybrid->getId() << " BxID " << +cBx << RESET;

                        // check stubs
                        // TODO add the bend check
                        auto cStubs        = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                        bool cStubMismatch = (cStubs.size() == 0);
                        for(auto cStub: cStubs)
                        {
                            int cPosition = NCHNLS * cChip->getId() + cStub.getPosition();
                            LOG(DEBUG) << BOLDMAGENTA << "\t... Seed " << +cPosition << RESET;
                            bool cStubNotFound = (std::find(cGoodSeeds.begin(), cGoodSeeds.end(), cPosition) == cGoodSeeds.end());
                            if(cStubNotFound)
                                LOG(INFO) << BOLDRED << "Stub with seed " << +cPosition << " on chip " << +cChip->getId() << " not one of those "
                                          << " injected." << RESET;
                            cStubMismatch = cStubMismatch || cStubNotFound;
                        }

                        // check hits
                        auto cHits        = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                        bool cHitMismatch = (cChannels.size() != cHits.size());
                        for(auto cHit: cHits) cHitMismatch = cHitMismatch || std::find(cChannels.begin(), cChannels.end(), cHit) == cChannels.end();

                        if(cHitMismatch)
                        {
                            LOG(INFO) << BOLDRED << "Hit mismatch in "
                                      << " chip " << +cChip->getId() << " on hybrid " << +cHybrid->getId() << RESET;

                            cMismatched++;
                            if(cStubMismatch)
                            {
                                LOG(INFO) << BOLDRED << "Stub also don't match in "
                                          << " chip " << +cChip->getId() << " on hybrid " << +cHybrid->getId() << " found " << +cStubs.size() << " stubs ... " << RESET;
                            }
                        }
                        else if(cStubMismatch)
                        {
                            LOG(INFO) << BOLDRED << "Stub mismatch in "
                                      << " chip " << +cChip->getId() << " on hybrid " << +cHybrid->getId() << " found " << +cStubs.size() << " stubs ... " << RESET;
                            // for(auto cSeed : cGoodSeeds)
                            // {
                            //     auto cChipId = cSeed/NCHNLS;
                            //     auto cStrip = cSeed%NCHNLS;
                            //     LOG (INFO) << BOLDMAGENTA << "Seed "
                            //         << +cSeed
                            //         << " i.e. stub in chip "
                            //         << +cChipId
                            //         << " with seed "
                            //         << +cStrip
                            //         << RESET;
                            // }
                        }
                    }
                }
            }
        }

        // return threshold to normal
        for(auto cOpticalGroup: *pBoard)
        {
            auto& cInjThisOpticalGroup = cInjThisBoard->at(cOpticalGroup->getIndex());
            auto& cThThisOpticalGroup  = cThThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cInjThisHybrid = cInjThisOpticalGroup->at(cHybrid->getIndex());
                auto& cThThisHybrid  = cThThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cInjThisChip = cInjThisHybrid->at(cChip->getIndex());
                    auto& cThThisChip  = cThThisHybrid->at(cChip->getIndex());
                    auto& cChannels    = cInjThisChip->getSummary<ChannelList>();
                    if(cChannels.size() > 0)
                    {
                        LOG(DEBUG) << BOLDBLUE << "Returning chip " << +cChip->getId() << " back to normal " << RESET;

                        auto cReadoutChip = static_cast<ReadoutChip*>(cChip);
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "VCth", cThThisChip->getSummary<uint16_t>());

                        ChannelGroup<NCHNLS, 1> cChannelMask;
                        cChannelMask.enableAllChannels();
                        fReadoutChipInterface->maskChannelsGroup(cReadoutChip, &cChannelMask);
                    }
                    cChannels.clear();
                }
            }
        }
    }

    // summary
    for(auto cOpticalGroup: *pBoard)
    {
        auto& cMismatchesOpticalGroup = cMismatchesThisBoard->at(cOpticalGroup->getIndex());
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cMismatchesHybrid = cMismatchesOpticalGroup->at(cHybrid->getIndex());
            for(auto cChip: *cHybrid)
            {
                auto& cMismatchesChip = cMismatchesHybrid->at(cChip->getIndex());
                if(cMismatchesChip->getSummary<uint32_t>() > 0)
                {
                    LOG(INFO) << BOLDRED << "Data mismatch in chip " << +cChip->getId() << " ... STOPPING TEST." << RESET;
                    exit(FAILED_DATA_TEST);
                }
            }
        }
    }
}
void DataChecker::HitCheck()
{
    for(auto cBoard: *fDetectorContainer)
    {
        auto cBeBoard = static_cast<BeBoard*>(cBoard);

        OuterTrackerHybrid* cFirstHybrid = static_cast<OuterTrackerHybrid*>(cBoard->at(0)->at(0));
        // bool cWithCIC = cFirstHybrid->fCic != NULL;
        // if( cWithCIC )
        //     cAligned = this->CICAlignment(theBoard);
        ReadoutChip* theFirstReadoutChip = static_cast<ReadoutChip*>(cFirstHybrid->at(0));
        bool         cWithCBC            = (theFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
        if(cWithCBC) this->HitCheck2S(cBeBoard);
    }
}

void DataChecker::ClusterCheck(std::vector<uint8_t> pChannels)
{
    // prepare mask
    // just for CBCs for now
    fCBCMask.disableAllChannels();
    for(auto cChannel: pChannels) fCBCMask.enableChannel(cChannel);

    auto     cSetting = fSettingsMap.find("Nevents");
    uint32_t cNevents = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", 1);
        cBoard->setSparsification(true);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, true);
                for(auto cChip: *cHybrid)
                {
                    LOG(INFO) << BOLDBLUE << "Masking channels in chip" << +cChip->getId() << RESET;
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(cChip, "VCth", 1000);
                    fReadoutChipInterface->maskChannelsGroup(cChip, &fCBCMask);
                }
                this->ReadNEvents(cBoard, cNevents);
                const std::vector<Event*>& cEvents = this->GetEvents();
                LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadData" << RESET;
                for(auto cChip: *cHybrid)
                {
                    bool cAllFound = true;
                    for(auto cEvent: cEvents)
                    {
                        auto cHits = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                        for(auto cHit: cHits) { cAllFound = cAllFound && (std::find(pChannels.begin(), pChannels.end(), cHit) != pChannels.end()); }
                    }
                    if(cAllFound)
                        LOG(INFO) << BOLDBLUE << "Readback all injected hits in chip " << +cChip->getId() << RESET;
                    else
                        LOG(INFO) << BOLDRED << "Readback all injected hits in chip " << +cChip->getId() << RESET;
                }
            }
        }
    }
}
#endif
