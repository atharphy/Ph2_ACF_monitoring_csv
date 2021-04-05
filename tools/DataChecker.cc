#include "DataChecker.h"
//#ifdef __USE_ROOT__

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
    LOG(INFO) << BOLDMAGENTA << "pulse shape will be scanned from " << +cInitialTh << " to " << +cFinalTh << " in " << +cThStep << " steps." << RESET;

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
                TH2D* cHist =   new TH2D(cName, Form("Number of P clusters found by CIC%d; Injection Time [Bx]; Number of P clusters", (int)cHybrid->getId()), 500, 0 - 0.5, 500 - 0.5, 10, 0 - 0.5, 10 - 0.5);
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

                cName = Form("h_StubsExpected_Cic%d", cHybrid->getId() );
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                TProfile2D* cProfile2D = new TProfile2D(cName, Form("P-cluster Matching [raw count only], digi-inject test CIC%d; MPA Id; Number of Expected Stubs ", (int)cHybrid->getId()), 8, 0 - 0.5, 8 - 0.5, 20 , 0 - 0.5, 20 - 0.5);
                bookHistogram(cHybrid, "PclusterMatchRawN", cProfile2D);

                cName = Form("h_L1Eye_Cic%d", cHybrid->getId() );
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile2D = new TProfile2D(cName, Form("P-cluster Matching [L1-eye], digi-inject test CIC%d; MPA Id; Phase ", (int)cHybrid->getId()), 8, 0 - 0.5, 8 - 0.5, 20 , 0 - 0.5, 20 - 0.5);
                bookHistogram(cHybrid, "PclusterMatchingL1Eye", cProfile2D);
                   
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
            LOG (INFO) << BOLDBLUE << "Threshold set to " << +cThreshold << RESET;
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
                //if(cHybrid->getIndex() > 0) continue;

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
                        { LOG(DEBUG) << BOLDRED << "\t Event# " << +cEventIndx << " BxId#" << +cBxId << " un-expected Stub in event.... Address " << +cStubAddress << " row " << +cRow << RESET; }
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
                            { LOG(INFO) << BOLDRED << "\t\t.. expected stub address : " << +(cInjection.fRow) * 2 << " and row " << +cInjection.fColumn << RESET; } // injections
                        }                                                                                                                                           // stubs
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
                                    { LOG(INFO) << BOLDRED << "\t\t.. expected stub address : " << +(cInjection.fRow) * 2 << " and row " << +cInjection.fColumn << RESET; } // injections
                                }                                                                                                                                           // stubs
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
// first I would just like to test the nominal 
// data transmission 
// before scanning the eye 
void DataChecker::PSNominal()
{ 
    size_t cMaxNstubs=16; 
    LOG (INFO) << BOLDBLUE << "Nominal PS data checker ... inject data from MPA --> CIC --> back-end" << RESET;
    int      cLatencyOffset = -1;
    auto     cSetting       = fSettingsMap.find("Nevents");
    uint32_t cNevents       = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    cSetting  = fSettingsMap.find("Attempts"); 
    uint32_t cMaxAttempts       = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 10;
    cSetting  = fSettingsMap.find("SLVSDrive"); 
    uint8_t  cSLVSDrive = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 3;
    cSetting  = fSettingsMap.find("MaxOffset"); 
    int  cMaxOffset = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 5;

    // configure clusters
    cSetting  = fSettingsMap.find("MaxPClusters"); 
    size_t nMaxClusters= (cSetting != std::end(fSettingsMap)) ? cSetting->second : 2;

    LOG(DEBUG) << BOLDBLUE << "ReadNEvents data test with " << +cNevents << RESET;

    uint8_t                cStubWindow = 1; // stub window in half pixels (1)
    uint8_t                cMode       = 2; // (0) pixel-strip, (1) strip-strip, (2) pixel-pixel, (3) strip-pixel

    // random c++
    std::srand (std::time(NULL));
    std::random_device cRndm{};
    std::mt19937 cGen{cRndm()};

    // configure latencies 
    // L1 latency in MPA
    // stub latency in FW 
    auto cStubOffset  = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->getStubOffset();
    for(auto cBoard: *fDetectorContainer)
    {
        // check trigger source
        // and reload
        uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
        cTriggerSrc = (cTriggerSrc == 6) ? cTriggerSrc : 6;
        LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
        cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);

        uint16_t cDelay   = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
        uint16_t cLatency = cDelay + cLatencyOffset ;
        LOG (INFO) << BOLDBLUE << "Setting L1 latency in MPA to " << +cLatency << RESET;
        int      cReTimeValue = -1;
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
            } // hybrid
        }// module
        int  cStubLatency = cLatency - (cStubOffset + cReTimeValue);
        LOG(INFO) << BOLDBLUE << "Setting L1 latency to " << +cLatency << " and stub latency to " << +cStubLatency << RESET;
        // read events
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
    }//boards

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
                        if(cChip->getFrontEndType() != FrontEndType::MPA ) continue;
                        fReadoutChipInterface->WriteChipReg(cChip, "SLVSDrive", cSLVSDrive);
                    }
                } // chip
            } // hybrid
        }// module
    }//boards

    // generate injections in this MPA 
    std::uniform_int_distribution<int> cFlatDistCols(2, 13);
    std::uniform_int_distribution<int> cFlatDistRows(2, 118); // avoid colums 1 and 120 
    std::uniform_int_distribution<int> cNClusterDist(0, nMaxClusters); 
    std::uniform_int_distribution<int> cMPAsDist(1, 7);
    std::uniform_int_distribution<int> cMPAIdDist(0, 7);



    // set offsets on CICs
    bool cModifyL1Phase=true; 
    std::vector<int> cOffsets{ 0 }; 
    if( cModifyL1Phase ) 
    {
        for( int cExtraOffset  = 1 ; cExtraOffset <= cMaxOffset ; cExtraOffset++) 
        {
            cOffsets.push_back( cExtraOffset );
            cOffsets.push_back( cExtraOffset*-1 );
        }
    }
    for( auto cOffset : cOffsets )
    {
        LOG (INFO) << BOLDMAGENTA << "Offset from optimal phase of "
            << +cOffset 
            << " taps." 
            << RESET;
        DetectorDataContainer fPhaseTaps, fOriginalPhaseTaps; 
        ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, fPhaseTaps);
        ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, fOriginalPhaseTaps);
        for(auto cBoard: *fDetectorContainer)
        {
            auto& fTaps = fPhaseTaps.at(cBoard->getIndex());
            auto& fTapsOrig = fOriginalPhaseTaps.at( cBoard->getIndex());
            for(auto cOpticalGroup: *cBoard)
            {
                auto& fTapsOG = fTaps->at(cOpticalGroup->getIndex());
                auto& fTapsOrigOG = fTapsOrig->at( cOpticalGroup->getIndex());
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& fTapsHybrid = fTapsOG->at(cHybrid->getIndex());
                    auto& fTapsOrigHybrid = fTapsOrigOG->at( cHybrid->getIndex());
                    auto& cCic   = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    //auto cOptimalTaps = fCicInterface->GetOptimalTaps(cCic);
                    //size_t cPhyPort=0; 
                    //size_t cPhyPortChnl=0; 
                    //size_t cCounter=0; 
                    for(auto cChip: *cHybrid)
                    {
                        if( cChip->getFrontEndType() == FrontEndType::SSA) continue;
                        std::string cOutput="";
                        char cBuffer[80];
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
                        size_t cPhyPortL1 = (cChip->getId() >3 ) ? 11 : 10; 
                        size_t cPhyPortChnlL1   = (cChip->getId()%4); 
                        auto& fTapsChip = fTapsHybrid->at(cChip->getIndex());
                        auto& fTapsOrigChip = fTapsOrigHybrid->at(cChip->getIndex());
                        fCicInterface->SetOptimalTap(cCic, cPhyPortL1, cPhyPortChnlL1, cOffset);
                        auto cOptimalTaps = fCicInterface->GetOptimalTaps(cCic);
                        fTapsChip->getSummary<uint8_t>() = cOptimalTaps[cPhyPortChnlL1][cPhyPortL1];
                        if( cOffset == 0 ) fTapsOrigChip->getSummary<uint8_t>() = cOptimalTaps[cPhyPortChnlL1][cPhyPortL1];
                        sprintf(cBuffer, "%.2d ", fTapsChip->getSummary<uint8_t>());
                        cOutput += cBuffer;
                        LOG(INFO) << BOLDBLUE << "Optimal tap found on FE" << +cChip->getId() << " : " << cOutput << RESET;
                        
                    }
                } // hybrid
            }// optical group
        }// board

        for( size_t cAttempt = 0 ; cAttempt < cMaxAttempts ; cAttempt++ )
        {
            if( cAttempt%10 == 0 )
                LOG (INFO) << BOLDBLUE << "Attempt#" << +cAttempt << RESET;
            
            // prepare data container 
            // zero what needs zeroing

            DetectorDataContainer fInjectedPClusters, fReadoutPClusters;
            DetectorDataContainer fPixelInjections;  
            ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fInjectedPClusters);
            ContainerFactory::copyAndInitChip<std::vector<uint32_t>>(*fDetectorContainer, fReadoutPClusters);
            ContainerFactory::copyAndInitChip<std::vector<Injection>>(*fDetectorContainer, fPixelInjections);
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cInjections = fInjectedPClusters.at(cBoard->getIndex());
                auto& cMatched = fReadoutPClusters.at(cBoard->getIndex());
                auto& cInj = fPixelInjections.at(cBoard->getIndex());
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());
                    auto& cMatchedOGs = cMatched->at(cOpticalGroup->getIndex());
                    auto& cInjOG = cInj->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
                        auto& cMatchedHybrid = cMatchedOGs->at(cHybrid->getIndex());
                        auto& cInjHybrid = cInjOG->at(cHybrid->getIndex());
                        for(auto cChip: *cHybrid)
                        {
                            auto& cInjectionsChip = cInjectionsHybrid->at(cChip->getIndex());
                            cInjectionsChip->getSummary<uint32_t>() = 0;
                            auto& cMatchedChip = cMatchedHybrid->at(cChip->getIndex());
                            auto& cSummary = cMatchedChip->getSummary<std::vector<uint32_t>>();
                            cSummary.clear();
                            auto& cInjChip = cInjHybrid->at(cChip->getIndex());
                            auto& cSummaryInj = cInjChip->getSummary<std::vector<Injection>>();
                            cSummaryInj.clear();
                        }
                    }
                }
            }

            // set-up MPA for injection
            size_t cTotalNumberOfClusters=0; 
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cInjections = fInjectedPClusters.at(cBoard->getIndex());
                auto& cInj = fPixelInjections.at(cBoard->getIndex());
                for(auto cOpticalReadout: *cBoard)
                {
                    auto& cInjectionsOpticalGroup = cInjections->at(cOpticalReadout->getIndex());
                    auto& cInjOG = cInj->at(cOpticalReadout->getIndex());
                    for(auto cHybrid: *cOpticalReadout)
                    {
                        auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
                        auto& cInjHybrid = cInjOG->at(cHybrid->getIndex());
                        
                        // size_t cNMPAs = cMPAsDist(cGen); 
                        // std::vector<int> cMPAs; cMPAs.clear();
                        // do
                        // {
                        //     int cMPA = cMPAIdDist(cGen);
                        //     if ( std::find( cMPAs.begin(), cMPAs.end(), cMPA ) == cMPAs.end() )
                        //     {
                        //         cMPAs.push_back( cMPA );
                        //     }

                        // }while( cMPAs.size() < cNMPAs ) ;

                        // if( cMPAs.size() == 0 ) continue;

                        // LOG (INFO) << BOLDMAGENTA << "Injecting clusters in " 
                        //     << +cMPAs.size() << " MPAs." << RESET;
                        for(auto cChip: *cHybrid) // for each chip (makes sense)
                        {
                            // for the moment - only written for CBC3
                            if(cChip->getFrontEndType() == FrontEndType::MPA)
                            {
                                // if( std::find( cMPAs.begin(), cMPAs.end(), cChip->getId()) == cMPAs.end() ) 
                                //     continue;

                                auto& cInjectionsChip = cInjectionsHybrid->at(cChip->getIndex());
                                auto& cInjChp = cInjHybrid->at(cChip->getIndex());
                                // activate stub mode
                                fReadoutChipInterface->WriteChipReg(cChip, "StubMode", cMode);
                                fReadoutChipInterface->WriteChipReg(cChip, "StubWindow", cStubWindow);
                                // 
                                std::vector<uint8_t>   cColumns(0);     // 5 , 10 };
                                std::vector<uint8_t>   cRows(0);       // 20 , 30};
                                std::vector<Injection> cInjections(0);
                                size_t cNclstrs = cNClusterDist(cGen);
                                // this is because each cluster becomes a stub .. 
                                // mask all pixels first 
                                (static_cast<PSInterface*>(fReadoutChipInterface))->WriteChipReg(cChip, "DigitalSync", 0x00);
            
                                if( cNclstrs == 0 || cTotalNumberOfClusters >= cMaxNstubs ) continue;

                                std::vector<uint32_t> cPixelIds(0); // these will be used to generate stubs
                                auto& cSummaryInj = cInjChp->getSummary<std::vector<Injection>>();
                                do
                                {
                                    Injection cInjection;
                                    cInjection.fColumn = (cFlatDistCols(cGen));
                                    cInjection.fRow    = (cFlatDistRows(cGen));
                                    uint32_t           cPixelId = (uint32_t)(cInjection.fColumn) * 120 + (uint32_t)cInjection.fRow;
                                    if( std::find( cPixelIds.begin(), cPixelIds.end(), cPixelId ) == cPixelIds.end() ) 
                                    {
                                        // check if the pixel is displaced by at least 4 pixels 
                                        bool cTooClose=false;
                                        for( size_t cIndx= 0; cIndx < cInjections.size(); cIndx++)
                                        {
                                            if( cInjection.fColumn == cInjections[cIndx].fColumn ) 
                                            {
                                                cTooClose = cTooClose || std::fabs( cInjection.fRow - cInjections[cIndx].fRow) < 5; 
                                            }

                                            if( cInjection.fRow == cInjections[cIndx].fRow ) 
                                            {
                                                cTooClose = cTooClose || std::fabs( cInjection.fColumn - cInjections[cIndx].fColumn) < 5; 
                                            }
                                        }
                                        if( !cTooClose && cTotalNumberOfClusters < cMaxNstubs)
                                        {
                                            LOG (DEBUG) << BOLDMAGENTA << "MPA#" << +cChip->getId() 
                                                << " injecting in row " 
                                                << +cInjection.fRow  
                                                << " columnn "
                                                << +cInjection.fColumn 
                                                << RESET;
                                            cPixelIds.push_back( cPixelId );
                                            cInjections.push_back(cInjection);
                                            cSummaryInj.push_back(cInjection);
                                            cTotalNumberOfClusters += 1; 
                                        }
                                    }
                                }while( cPixelIds.size() < cNclstrs && cTotalNumberOfClusters < cMaxNstubs );  // create injection patterns
                                cInjectionsChip->getSummary<uint32_t>() = cInjections.size();
                                LOG (DEBUG) << BOLDBLUE << "Injecting " << +cInjectionsChip->getSummary<uint32_t>()<< " clusters/stubs in MPA#" << +cChip->getId() << RESET;
                                (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, cInjections);
                            }
                        } // chip
                    }// hybrid
                }// module
            }//boards
            if( cAttempt%10 == 0 )
                LOG (INFO) << BOLDMAGENTA << "In total have " 
                    << +cTotalNumberOfClusters
                    << " clusters in this run."
                    << RESET;

            for(auto cBoard: *fDetectorContainer)
            {
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
                    if( cEvent->GetEventCount()  == cNevents - 1 ) 
                        continue;
                    
                    for(auto cOpticalGroup: *cBoard)
                    {
                        auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());
                        auto& cInjOG = cInj->at(cOpticalGroup->getIndex());
                        for(auto cHybrid: *cOpticalGroup)
                        {
                            auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
                            auto& cInjHybrid = cInjOG->at(cHybrid->getIndex());
                            auto cL1Status = (static_cast<D19cCic2Event*>(cEvent))->L1Status(cHybrid->getId());
                            auto cErrorBitCic = (static_cast<D19cCic2Event*>(cEvent))->Error(cHybrid->getId(), 8);
                            for(auto cChip: *cHybrid)
                            {
                                if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                                auto& cInjChp = cInjHybrid->at(cChip->getIndex());
                                auto& cSummaryInj = cInjChp->getSummary<std::vector<Injection>>();
                                // don't bother checking when I havne't injected
                                if ( cSummaryInj.size() == 0 ) continue; 


                                auto cMPAL1Error = fReadoutChipInterface->ReadChipReg(cChip, "ErrorL1");
                                uint32_t cL1Id = cEvent->L1Id( cHybrid->getId(), cChip->getId() );
                                if( cChip->getId() == 0 ) LOG (DEBUG) << BOLDMAGENTA << "Event#" << +cEvent->GetEventCount() << " L1Id " << +cL1Id << RESET;
                                auto cStubs = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                                auto cPClusters = (static_cast<D19cCic2Event*>(cEvent))->GetPixelClusters(cHybrid->getId(), cChip->getId());
                                auto cSClusters = (static_cast<D19cCic2Event*>(cEvent))->GetStripClusters(cHybrid->getId(), cChip->getId());
                                auto cErrorBit = (static_cast<D19cCic2Event*>(cEvent))->Error(cHybrid->getId(), cChip->getId());
                                // check pixel clusters
                                size_t cNmatched=0;
                                for( auto cM : cSummaryInj) 
                                {
                                    bool cFound=false;
                                    for( auto cPCluster :  cPClusters) 
                                    {
                                        if( cPCluster.fAddress == cM.fRow && cPCluster.fZpos == cM.fColumn ) 
                                            cFound = true;
                                    }
                                    cNmatched += (cFound) ? 1 : 0; 
                                    if( !cFound )
                                        LOG (DEBUG) << BOLDRED << "MPA#" << +cChip->getId() 
                                            << "\t\t\t\t L1Id " << +cL1Id
                                            << " MISSING PCluster : address : " << unsigned(cM.fRow) 
                                            << ", row " << unsigned(cM.fColumn) 
                                            << " FE status bit from is " 
                                            << +cErrorBit 
                                            << " CIC status bit is "
                                            << +cErrorBitCic
                                            << std::bitset<9>(cL1Status&0x1FF)
                                            << " L1 error is "
                                            << std::bitset<8>(+cMPAL1Error)
                                            << RESET;
                                    else
                                        LOG (DEBUG) << BOLDGREEN << "MPA#" << +cChip->getId() 
                                            << "\t\t\t\t L1Id " << +cL1Id
                                            << " FOUND PCluster : address : " << unsigned(cM.fRow) 
                                            << ", row " << unsigned(cM.fColumn) 
                                            << " FE status bit from is " 
                                            << +cErrorBit 
                                            << " CIC status bit is "
                                            << +cErrorBitCic
                                            << std::bitset<9>(cL1Status&0x1FF)
                                            << " L1 error is "
                                            << std::bitset<8>(+cMPAL1Error)
                                            << RESET;
                                        
                                }
                                // for(auto cPCluster: cPClusters)
                                // {
                                //     bool cFound=false;
                                //     LOG(DEBUG) << BOLDBLUE << "\t\t\t\t PCluster : address : " << unsigned(cPCluster.fAddress) 
                                //         << ", width " << unsigned(cPCluster.fWidth) << ", row "
                                //         << unsigned(cPCluster.fZpos) << RESET;
                                //     for( size_t cIndx=0; cIndx < cSummaryInj.size(); cIndx++) 
                                //     {
                                //         if( cPCluster.fAddress == cSummaryInj[cIndx].fRow && cPCluster.fZpos == cSummaryInj[cIndx].fColumn ) 
                                //             cFound = true;
                                //     }
                                //     cNmatched += (cFound) ? 1 : 0; 
                                //     if( !cFound )
                                //         LOG(DEBUG) << BOLDBLUE << "\t\t\t\t PCluster : address : " << unsigned(cPCluster.fAddress) 
                                //             << ", width " << unsigned(cPCluster.fWidth) << ", row "
                                //             << unsigned(cPCluster.fZpos) << RESET;
                                            
                                // }
                                LOG (DEBUG) << BOLDMAGENTA  << "MPA#" << +cChip->getId() 
                                    << " have found "
                                    << +cNmatched 
                                    << " P-clusters out of "
                                    << +cSummaryInj.size() 
                                    << " injected."
                                    << RESET;
                                auto& cInjectionsThisChip = cInjectionsHybrid->at(cChip->getIndex());
                                auto& cSummary = cInjectionsThisChip->getSummary<std::vector<uint32_t>>();
                                cSummary.push_back( cNmatched ); 
                                if( (1+cEvent->GetEventCount())%100 ==  0 ) 
                                {
                                    LOG (INFO) << BOLDMAGENTA << "Event#" << +cEvent->GetEventCount() 
                                            << "\t\tMPA#" << +cChip->getId()
                                            << "\t... found the " << +cStubs.size() 
                                            << " stubs in this event, "
                                            << +cPClusters.size() 
                                            << " pixel clusters and "
                                            << +cSClusters.size() 
                                            << " strip clusters "
                                            << " summary has "
                                            << +cSummary.size() << " entries "
                                            << RESET;
                                }
                                
                                //check stubs are where you put them
                                //not checking bend for now
                                size_t cNMatchedStubs=0;
                                for(auto cStub: cStubs)
                                {
                                    auto     cStubAddress = cStub.getPosition();
                                    auto     cRow         = cStub.getRow();
                                    uint32_t cPixelId     = (cStubAddress / 2) + cRow * 120;
                                    bool cMatch=false;
                                    for( auto cInj : cSummaryInj) 
                                    {
                                        uint32_t           cId = (uint32_t)(cInj.fColumn) * 120 + (uint32_t)cInj.fRow;
                                        if( cPixelId == cId ) cMatch = true;
                                    }
                                    if( cMatch )
                                        LOG (DEBUG) << "Matched stub " 
                                            << " address " << +cStubAddress 
                                            << " row " << +cRow 
                                            << RESET;
                                    cNMatchedStubs += (cMatch) ? 1 : 0; 
                                }
                            } // ROCs
                        } // hybrids or CICs
                    } // optical group loop
                }// event loop
            }

            // summarize results 
            for(auto cBoard: *fDetectorContainer)
            {
                auto& cInjections = fReadoutPClusters.at(cBoard->getIndex());
                auto& cExpected = fInjectedPClusters.at(cBoard->getIndex());
                auto& cTaps = fPhaseTaps.at(cBoard->getIndex());
                auto& cTapsOrig = fOriginalPhaseTaps.at(cBoard->getIndex());
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());
                    auto& cExpectedOpticalGroup = cExpected->at(cOpticalGroup->getIndex());
                    auto& cTapsOpticalGroup = cTaps->at(cOpticalGroup->getIndex());
                    auto& cTapsOrigOpticalGroup = cTapsOrig->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
                        auto& cExpectedHybrid = cExpectedOpticalGroup->at(cHybrid->getIndex());
                        auto& cTapsHybrid = cTapsOpticalGroup->at(cHybrid->getIndex());
                        auto& cTapsOrigHybrid = cTapsOrigOpticalGroup->at(cHybrid->getIndex());
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                            auto& cInjectionsThisChip = cInjectionsHybrid->at(cChip->getIndex());
                            auto& cExpectedClstrsThisChip = cExpectedHybrid->at(cChip->getIndex());
                            auto& cTapsChip = cTapsHybrid->at(cChip->getIndex());
                            auto& cTapsOrigChip = cTapsOrigHybrid->at(cChip->getIndex());
                            
                            auto cSummary = cInjectionsThisChip->getSummary<std::vector<uint32_t>>(); 
                            float cExpected = (float)(cExpectedClstrsThisChip->getSummary<uint32_t>());
                            if( cExpected ==0 ) continue;

                            std::vector<float> cData(0);
                            for(auto cPt : cSummary )
                            {
                                cData.push_back( (float)cPt );
                                if( (float)cPt != cExpected ) 
                                {
                                    LOG (DEBUG) << BOLDRED << "MPA#" << +cChip->getId() 
                                        << " expected  " << +cExpected << " clusters and have found "
                                        << +cPt << RESET;
                                }
                                #ifdef __USE_ROOT__
                                    for( size_t cIndx=0; cIndx < cExpected; cIndx++)
                                    {
                                        int cFill = (cIndx < (float)cPt) ? 1 : 0 ; 
                                        TProfile2D* cMatched = static_cast<TProfile2D*>(getHist(cHybrid,"PclusterMatchingL1Eye"));
                                        cMatched->Fill( cChip->getId(), (float)(cTapsChip->getSummary<uint8_t>()) , cFill) ; 
                                        if( cTapsChip->getSummary<uint8_t>() == cTapsOrigChip->getSummary<uint8_t>() )
                                        {
                                            cMatched = static_cast<TProfile2D*>(getHist(cHybrid, "PclusterMatchRawN"));
                                            cMatched->Fill( cChip->getId(), cTotalNumberOfClusters , cFill);//(float)cPt/cExpected ) ; 
                                        }
                                    }
                                #endif
                            }
                            auto cStats = getStats(cData); 
                            if( cAttempt%10 == 0 )
                            {
                                LOG (INFO) << BOLDBLUE << "MPA#" << +cChip->getId() 
                                    << " phase tap " << +cTapsChip->getSummary<uint8_t>()
                                    << " [offset of " << cOffset
                                    << " ]\t..."
                                    << " on average have found " << +cStats.first 
                                    << " clusters and expect "
                                    << +cExpectedClstrsThisChip->getSummary<uint32_t>() 
                                    << RESET;
                            }
                            // #ifdef __USE_ROOT__
                            //     TProfile2D* cMatched = static_cast<TProfile2D*>(getHist(cHybrid, "PclusterMatchRawN"));
                            //     cMatched->Fill( cChip->getId(), cTotalNumberOfClusters , cStats.first/cExpected ) ; 
                            // #endif
                    
                            
                        } // ROCs
                    } // hybrids or CICs
                } // optical group loop
            }
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
        cTriggerSrc = (cTriggerSrc == 6) ? cTriggerSrc : 6;
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
                    if(pShiftRegMode) { 
                        fReadoutChipInterface->WriteChipReg(cChip, "DigitalPattern", cPattern); 
                    } // shit register mode
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
                        if( pShiftRegMode )
                        {
                            auto cLines    = (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->ScopeStubLines();
                            int  cLineIndx = 0;
                            for(auto cLine: cLines)
                            {
                                if( cLineIndx == 4 ) continue;
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

    //std::vector<uint16_t> cDelaysBetwn{90, 180, 360};
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
    this->DigitalInjectionTest(true, false);
    // auto cSetting = fSettingsMap.find ( "Nevents" );
    // uint32_t cNevents = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 100;
    // LOG (INFO) << BOLDBLUE << "ReadNEvents data test with " << +cNevents << RESET;
    // std::stringstream outp;
    // for(auto cBoard: *fDetectorContainer)
    // {
    //     auto cEventType = cBoard->getEventType();
    //     cBoard->setEventType(EventType::VR);
    //     for(auto cOpticalGroup: *cBoard)
    //     {
    //         for(auto cHybrid: *cOpticalGroup)
    //         {
    //             // matching
    //             uint16_t cTh1 = (cHybrid->getId() % 2 == 0) ? 900 : 1;
    //             uint16_t cTh2 = (cHybrid->getId() % 2 == 0) ? 1 : 900;
    //             for(auto cChip: *cHybrid)
    //             {
    //                 if( cChip->getFrontEndType() == FrontEndType::CBC3)
    //                 {
    //                     uint16_t cTh = (cChip->getId() % 2 == 0) ? cTh1 : cTh2;
    //                     LOG(INFO) << BOLDBLUE << "Threshold on RoC#" << +cChip->getId() << " set to " << +cTh << RESET;
    //                     fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cChip), "VCth", cTh);
    //                 }
    //                 else if( cChip->getFrontEndType() == FrontEndType::MPA )
    //                 {
    //                     auto cReadoutMode = fReadoutChipInterface->ReadChipReg(cChip,"ReadoutMode");
    //                     LOG (INFO) << BOLDBLUE << "MPA#" << +cChip->getId()
    //                         << " : readout mode [" << +cReadoutMode << " ]" << RESET;
    //                 }
    //             }
    //         }
    //     }

    // //     LOG (INFO) << BOLDBLUE << "Checking ReadNEvents by reading "
    // //         << +cNevents
    // //         << " from BeBoard#"
    // //         << +cBoard->getIndex()
    // //         << RESET;

    // //     BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);
    // //     this->ReadNEvents(cBeBoard, cNevents);
    // //     // const std::vector<Event*>& cEvents = this->GetEvents(cBeBoard);
    // //     // LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadData" << RESET;

    // //     // uint32_t cN = 0;
    // //     // for(auto& cEvent: cEvents)
    // //     // {
    // //     //     if(cN % 5 == 0)
    // //     //     {
    // //     //         LOG(INFO) << ">>> Event #" << cN << RESET;
    // //     //         ;
    // //     //         outp.str("");
    // //     //         outp << *cEvent;
    // //     //         LOG(INFO) << outp.str();
    // //     //     }
    // //     //     cN++;
    // //     // }
    // //     cBoard->setEventType(cEventType);
    // }
    // LOG(INFO) << BOLDBLUE << "Done!" << RESET;
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
                do
                {
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
                                        do
                                        {
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
//#endif
