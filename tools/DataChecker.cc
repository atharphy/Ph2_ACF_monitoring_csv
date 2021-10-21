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
    uint16_t cInitialTh = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 400;
    cSetting            = fSettingsMap.find("PulseShapeFinalVcth");
    uint16_t cFinalTh   = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 600;
    cSetting            = fSettingsMap.find("PulseShapeVCthStep");
    uint16_t cThStep    = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 5;
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
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cInjections = fInjections.at(cBoard->getIndex());
        auto& cMismatches = fDataMismatches.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());
            auto& cMismatchesOpticalGroup = cMismatches->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
                auto& cMismatchesHybrid = cMismatchesOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cInjectionsChip = cInjectionsHybrid->at(cChip->getIndex());
                    auto& cMismatchesChip = cMismatchesHybrid->at(cChip->getIndex());
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
    size_t cEventsPerPoint = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 100;

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
    uint8_t           cSweepThreshold = this->findValueInSettings<double>("AsyncSweepTh");
    uint8_t           cThreshold      = this->findValueInSettings<double>("cThreshold");
    uint8_t           cThresholdStart = (cSweepThreshold == 0) ? cThreshold : 0;
    uint8_t           cThresholdStop  = (cSweepThreshold == 0) ? cThreshold + 5 : 200;
    std::stringstream outp;
    for(auto cBoard: *fDetectorContainer)
    {
        for(uint8_t cThreshold = cThresholdStart; cThreshold < cThresholdStop; cThreshold += 5)
        {
            // set thresholds
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 1);
                        fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
                        fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", this->findValueInSettings<double>("AsyncCalDac"));
                    }
                }
            }

            // read counters
            BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
            LOG(INFO) << BOLDRED << "Reading counters .. " << RESET;
            this->ReadNEvents(theBoard, this->findValueInSettings<double>("Nevents"));
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
                            for(uint8_t cChnl = 0; cChnl < 5; cChnl++) { LOG(INFO) << BOLDBLUE << "Counter value Strip#" << +cChnl << " is " << cHits[cChnl] << RESET; }
                        }
                    }
                }
            }
            LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadNEvents" << RESET;
        }
        // const std::vector<Event*>& cEvents = this->GetEvents();
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
    uint32_t          cNevents = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 100;
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
    uint32_t cNevents    = this->findValueInSettings<double>("Nevents");
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
void DataChecker::ReadNeventsTest()
{
    // auto cSetting = fSettingsMap.find ( "Nevents" );
    // uint32_t cNevents = ( cSetting != std::end ( fSettingsMap ) ) ? cSetting->second : 100;
    std::stringstream outp;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // matching
                uint16_t cTh1 = (cHybrid->getId() % 2 == 0) ? 900 : 1;
                uint16_t cTh2 = (cHybrid->getId() % 2 == 0) ? 1 : 900;
                for(auto cChip: *cHybrid)
                {
                    uint16_t cTh = (cChip->getId() % 2 == 0) ? cTh1 : cTh2;
                    LOG(INFO) << BOLDBLUE << "Threshold on RoC#" << +cChip->getId() << " set to " << +cTh << RESET;
                    fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cChip), "VCth", cTh);
                }
            }
        }
        BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);
        this->ReadNEvents(cBeBoard, 1);
        const std::vector<Event*>& cEvents = this->GetEvents();
        LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadData" << RESET;

        uint32_t cN = 0;
        for(auto& cEvent: cEvents)
        {
            if(cN % 5 == 0)
            {
                LOG(INFO) << ">>> Event #" << cN << RESET;
                ;
                outp.str("");
                outp << *cEvent;
                LOG(INFO) << outp.str();
            }
            cN++;
        }
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
    uint32_t cEventsPerPoint = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 100;

    // get trigger multiplicity from xml
    cSetting                       = fSettingsMap.find("TriggerMultiplicity");
    bool     cConfigureTriggerMult = (cSetting != std::end(fSettingsMap));
    uint16_t cTriggerMult          = cConfigureTriggerMult ? boost::any_cast<double>(cSetting->second) : 0;

    // get stub delay scan range from xml
    cSetting                  = fSettingsMap.find("StubDelay");
    bool cModifyStubScanRange = (cSetting != std::end(fSettingsMap));
    int  cStubDelay           = cModifyStubScanRange ? boost::any_cast<double>(cSetting->second) : 48;

    // get target threshold
    cSetting = fSettingsMap.find("Threshold");

    // get number of attempts
    cSetting = fSettingsMap.find("Attempts");
    // get mode
    cSetting      = fSettingsMap.find("Mode");
    uint8_t cMode = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;
    // get latency offset
    cSetting = fSettingsMap.find("LatencyOffset");
    // resync between attempts
    cSetting = fSettingsMap.find("ReSync");

    // if TP is used - enable it
    cSetting              = fSettingsMap.find("PulseShapePulseAmplitude");
    fTPconfig.tpAmplitude = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 100;

    // get TP amplitude range
    // get threshold range

    // get threshold range
    cSetting            = fSettingsMap.find("PulseShapeInitialVcth");
    uint16_t cInitialTh = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 400;
    cSetting            = fSettingsMap.find("PulseShapeFinalVcth");
    uint16_t cFinalTh   = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 600;
    cSetting            = fSettingsMap.find("PulseShapeVCthStep");
    uint16_t cThStep    = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 5;

    // get TP delay range
    cSetting                 = fSettingsMap.find("PulseShapeInitialDelay");
    uint16_t cInitialTPdleay = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;
    cSetting                 = fSettingsMap.find("PulseShapeFinalDelay");
    uint16_t cFinalTPdleay   = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 25;
    cSetting                 = fSettingsMap.find("PulseShapeDelayStep");
    uint16_t cTPdelayStep    = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 3;

    // get injected stub from xmls
    std::pair<uint8_t, int> cStub;
    cSetting     = fSettingsMap.find("StubSeed");
    cStub.first  = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 10;
    cSetting     = fSettingsMap.find("StubBend");
    cStub.second = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;
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
        //                     const std::vector<Event*>& cEvents = this->GetEvents();
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
        //         //             const std::vector<Event*>& cEvents = this->GetEvents();
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
    if(cSetting != std::end(fSettingsMap)) pWithNoise = (boost::any_cast<double>(cSetting->second) == 1);

    // get number of events from xml
    cSetting                 = fSettingsMap.find("Nevents");
    uint32_t cEventsPerPoint = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 100;

    // get trigger rate from xml
    cSetting                   = fSettingsMap.find("TriggerRate");
    bool     cConfigureTrigger = (cSetting != std::end(fSettingsMap));
    uint16_t cTriggerRate      = cConfigureTrigger ? boost::any_cast<double>(cSetting->second) : 100;
    // get trigger multiplicity from xml
    cSetting                       = fSettingsMap.find("TriggerMultiplicity");
    bool     cConfigureTriggerMult = (cSetting != std::end(fSettingsMap));
    uint16_t cTriggerMult          = cConfigureTriggerMult ? boost::any_cast<double>(cSetting->second) : 0;

    // get stub delay scan range from xml
    cSetting                  = fSettingsMap.find("StubDelay");
    bool cModifyStubScanRange = (cSetting != std::end(fSettingsMap));
    int  cStubDelay           = cModifyStubScanRange ? boost::any_cast<double>(cSetting->second) : 48;

    // get injected stub from xmls
    cSetting     = fSettingsMap.find("StubSeed");
    cStub.first  = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 10;
    cSetting     = fSettingsMap.find("StubBend");
    cStub.second = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;
    LOG(DEBUG) << BOLDBLUE << "Injecting a stub in position " << +cStub.first << " with bend " << cStub.second << " to test data integrity..." << RESET;

    // get target threshold
    cSetting                  = fSettingsMap.find("Threshold");
    uint16_t cTargetThreshold = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 580;

    // get number of attempts
    cSetting         = fSettingsMap.find("Attempts");
    size_t cAttempts = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 10;

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
    uint8_t cMode = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;

    // get latency offset
    cSetting           = fSettingsMap.find("LatencyOffset");
    int cLatencyOffset = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;

    // resync between attempts
    cSetting     = fSettingsMap.find("ReSync");
    bool cResync = (cSetting != std::end(fSettingsMap)) ? (boost::any_cast<double>(cSetting->second) == 1) : false;

    uint16_t cTPdelay = 0;

    // if TP is used - enable it
    if(!pWithNoise)
    {
        cSetting              = fSettingsMap.find("PulseShapePulseAmplitude");
        fTPconfig.tpAmplitude = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 100;

        // get TP delay
        cSetting = fSettingsMap.find("PulseShapePulseDelay");
        cTPdelay = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;

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

// check L1 eye
void DataChecker::L1Eye(std::vector<uint8_t> pChipIds)
{
    // uint8_t cChipId=0;
    // uint8_t cSeed=10;
    // uint8_t cBendCode=0;

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

void DataChecker::StubCheck(std::vector<uint8_t> pChipIds)
{
    std::string  cDAQFileName    = "StubCheck.daq";
    FileHandler* cDAQFileHandler = new FileHandler(cDAQFileName, 'w');

    uint8_t cSweepPackageDelay = this->findValueInSettings<double>("SweepPackageDelay");
    uint8_t cSweepStubDelay    = this->findValueInSettings<double>("SweepStubDelay");

    uint8_t cFirmwareTPdelay      = this->findValueInSettings<double>("StubFWTestPulseDelay");
    uint8_t cFirmwareTriggerDelay = this->findValueInSettings<double>("StubFWTriggerDelay");

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
    uint16_t cThreshold          = this->findValueInSettings<double>("StubThreshold");
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

        for(int cAttempt = 0; cAttempt < this->findValueInSettings<double>("StubAttempts"); cAttempt++)
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
    uint8_t cSweepPackageDelay = this->findValueInSettings<double>("SweepPackageDelay");
    uint8_t cSweepStubDelay    = this->findValueInSettings<double>("SweepStubDelay");
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
    uint32_t cEventsPerPoint = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 100;

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
    uint32_t cNevents = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 100;
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
