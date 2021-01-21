#include "ExtraChecks.h"
#ifdef __USE_ROOT__
#include "CBCChannelGroupHandler.h"
#include "Channel.h"
#include "CommonVisitors.h"
#include "ContainerFactory.h"
#include "Occupancy.h"
#include "Visitor.h"

#include <map>

#include "TCanvas.h"
#include "TGraphErrors.h"
#include "TH2.h"
#include "TProfile.h"
#include "TProfile2D.h"
#include "TString.h"
#include "TText.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

// std::map<Chip*, uint16_t> CicFEAlignment::fVplusMap;

ExtraChecks::ExtraChecks() : PedeNoise()
{
    fPedestalContainer.reset();
    fNoiseContainer.reset();
    fOccupancyContainer.reset();
    fHitCheckContainer.reset();
    fStubCheckContainer.reset();
}

ExtraChecks::~ExtraChecks()
{
    // delete fOffsetCanvas;
    // delete fOccupancyCanvas;
}

void ExtraChecks::Initialise()
{
    // this is needed if you're going to use groups anywhere
    fChannelGroupHandler = new CBCChannelGroupHandler(); // This will be erased in tool.resetPointers()
    fChannelGroupHandler->setChannelGroupParameters(16, 2);
#ifdef __USE_ROOT__
//    fDQMHistogram.book(fResultFile,*fDetectorContainer);
#endif
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // histograms per cbc
                for(auto cChip: *cHybrid)
                {
                    TString  cName = Form("h_BendCheck_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    TObject* cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    // occupancy
                    cName = Form("h_Occupancy_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    TH2D* cHist =
                        new TH2D(cName, Form("Occupancy CBC%d; Threshold [DAC units]; Number of hits", (int)cChip->getId()), 1023, 0 - 0.5, 1023. - 0.5, NCHANNELS + 10, 0 - 0.5, 10 + NCHANNELS - 0.5);
                    bookHistogram(static_cast<ReadoutChip*>(cChip), "Occupancy", cHist);

                    // error bits
                    cName = Form("h_ErrorBits_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName, Form("Error Flag CBC%d; Event Id; Threshold [DAC units]; Error Bit", (int)cChip->getId()), 200, 0, 200, 1023, 0 - 0.5, 1023. - 0.5);
                    bookHistogram(static_cast<ReadoutChip*>(cChip), "ErrorFlag", cHist);

                    // pipeline addrress
                    cName = Form("h_Pipeline_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName, Form("Pipeline Address CBC%d; Event Id; Threshold [DAC units]; Pipeline Address", (int)cChip->getId()), 200, 0, 200, 1023, 0 - 0.5, 1023. - 0.5);
                    bookHistogram(static_cast<ReadoutChip*>(cChip), "Pipeline", cHist);
                    // L1Id
                    cName = Form("h_L1Id_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    cHist = new TH2D(cName, Form("L1 Id CBC%d; Event Id; Threshold [DAC units]; L1 Id", (int)cChip->getId()), 200, 0, 200, 1023, 0 - 0.5, 1023. - 0.5);
                    bookHistogram(static_cast<ReadoutChip*>(cChip), "L1", cHist);

                    cName = Form("h_PedeNoise_Fe%dCbc%d", cHybrid->getId(), cChip->getId());
                    cObj  = gROOT->FindObject(cName);
                    if(cObj) delete cObj;
                    TH1D* cHist1D = new TH1D(cName, Form("Pedestal and noise - CBC%d; Channel; Pedestal and noise [DAC units]", (int)cChip->getId()), NCHANNELS, 0 - 0.5, NCHANNELS - 0.5);
                    bookHistogram(static_cast<ReadoutChip*>(cChip), "PedeNoise", cHist1D);
                }
                // matched stubs
                TString  cName = Form("h_MatchedStubs");
                TObject* cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                TProfile* cProfile = new TProfile(cName, Form("Number of matched stubs - CIC%d; CBC; Fraction of matched stubs", (int)cHybrid->getId()), 8, 0 - 0.5, 8 - 0.5);
                bookHistogram(cHybrid, "MatchedStubs", cProfile);
                // correct bend
                cName = Form("h_CorrectBend");
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile = new TProfile(cName, Form("Fraction of events with correct bend - CIC%d; CBC; Fraction with correct bends", (int)cHybrid->getId()), 8, 0 - 0.5, 8 - 0.5);
                bookHistogram(cHybrid, "CorrectBend", cProfile);
                // wTP
                cName = Form("h_CorrectBend_TP");
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                TProfile2D* cProf2D = new TProfile2D(cName, Form("Fraction of events with correct bend - CIC%d; CBC; Stub Latency", (int)cHybrid->getId()), 8, 0 - 0.5, 8 - 0.5, 512, 0, 512);
                bookHistogram(cHybrid, "CorrectBend_TP", cProf2D);

                // correct seed
                cName = Form("h_CorrectSeed");
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile = new TProfile(cName, Form("Fraction of events with correct seed - CIC%d; CBC; Fraction with correct seed", (int)cHybrid->getId()), 8, 0 - 0.5, 8 - 0.5);
                bookHistogram(cHybrid, "CorrectSeed", cProfile);
                // wTP
                cName = Form("h_CorrectSeed_TP");
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProf2D = new TProfile2D(cName, Form("Fraction of events with correct seed - CIC%d; CBC; Stub Latency", (int)cHybrid->getId()), 8, 0 - 0.5, 8 - 0.5, 512, 0, 512);
                bookHistogram(cHybrid, "CorrectSeed_TP", cProf2D);

                // matched hits
                cName = Form("h_MatchedHits");
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile = new TProfile(cName, Form("Number of matched hits - CIC%d; CBC; Fraction of matched hits", (int)cHybrid->getId()), 8, 0 - 0.5, 8 - 0.5);
                bookHistogram(cHybrid, "MatchedHits", cProfile);

                // bunch crossing counters
                cName = Form("h_BunchCrossingCounter");
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                TH2D* cHist2D = new TH2D(cName, Form("Number of matched stubs - CIC%d; Event Id; FE ASIC Id [CBC]", (int)cHybrid->getId()), 1000, 0 - 0.5, 1000 - 0.5, 8, 0 - 0., 8 - 0.5);
                bookHistogram(cHybrid, "BxCounter", cHist2D);
                // number of stubs
                // wTP
                cName = Form("h_Nstubs_TP");
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist2D = new TProfile2D(cName, Form("Number of stubs - CIC%d; CBC; Stub Latency", (int)cHybrid->getId()), 8, 0 - 0.5, 8 - 0.5, 512, 0, 512);
                bookHistogram(cHybrid, "Nstubs_TP", cHist2D);

                cName = Form("h_L1Status_Fe%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist2D = new TH2D(cName, Form("Error Flag CIC%d; Event Id; Chip Id; Error Bit", (int)cHybrid->getId()), 1000, 0, 1000, 9, 0 - 0.5, 9 - 0.5);
                bookHistogram(cHybrid, "L1Status", cHist2D);

                cName = Form("h_NominalOccupancy_Fe%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                TH1D* cHist = new TH1D(cName, Form("Occupancy FE%d at nominal threshold; Number of hits", (int)cHybrid->getId()), 10 + NCHANNELS * 8, 0 - 0.5, 10 + NCHANNELS * 8 - 0.5);
                bookHistogram(cHybrid, "NominalOccupancy", cHist);

                cName = Form("h_hitMap_Fe%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist2D = new TH2D(cName, Form("HitMap [FEH%d]; Strip Number; Number of Hits", (int)cHybrid->getId()), 8 * NCHANNELS / 2., 0 - 0.5, 8 * NCHANNELS / 2. - 0.5, 2, 0, 2);
                cHist2D->GetYaxis()->SetBinLabel(1, "Bottom");
                cHist2D->GetYaxis()->SetBinLabel(2, "Top");
                bookHistogram(cHybrid, "HitMap", cHist2D);

                cName = Form("h_hitMap_BottomSensor_Fe%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist2D = new TH2D(cName,
                                   Form("Hit Map Bottom Sensor [FEH%d]; Threshold [DAC units]; Strip Number; Number of Hits", (int)cHybrid->getId()),
                                   1023,
                                   0 - 0.5,
                                   1023. - 0.5,
                                   8 * NCHANNELS / 2.,
                                   0 - 0.5,
                                   8 * NCHANNELS / 2. - 0.5);
                bookHistogram(cHybrid, "HitMap_BottomSensor", cHist2D);

                cName = Form("h_hitMap_TopSensor_Fe%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cHist2D = new TH2D(cName,
                                   Form("Hit Map Top Sensor [FEH%d]; Threshold [DAC units]; Strip Number; Number of Hits", (int)cHybrid->getId()),
                                   1023,
                                   0 - 0.5,
                                   1023. - 0.5,
                                   8 * NCHANNELS / 2.,
                                   0 - 0.5,
                                   8 * NCHANNELS / 2. - 0.5);
                bookHistogram(cHybrid, "HitMap_TopSensor", cHist2D);

                // pedestal and noise
                cName = Form("h_Pedestal_PerSide");
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                TProfile2D* cProfile2D = new TProfile2D(cName,
                                                        Form("Pedestal distribution [FEH%d]; Strip Number; Sensor Layer; Pedestal [DAC units]", (int)cHybrid->getId()),
                                                        8 * NCHANNELS / 2,
                                                        0 - 0.5,
                                                        8 * NCHANNELS / 2 - 0.5,
                                                        2,
                                                        0,
                                                        2);
                cProfile2D->GetYaxis()->SetBinLabel(1, "Bottom");
                cProfile2D->GetYaxis()->SetBinLabel(2, "Top");
                bookHistogram(cHybrid, "Pedestal_perSide", cProfile2D);

                cName = Form("h_Noise_PerSide");
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile2D = new TProfile2D(
                    cName, Form("Noise distribution [FEH%d]; Strip Number; Sensor Layer; Noise [DAC units]", (int)cHybrid->getId()), 8 * NCHANNELS / 2, 0 - 0.5, 8 * NCHANNELS / 2 - 0.5, 2, 0, 2);
                bookHistogram(cHybrid, "Noise_perSide", cProfile2D);

                cName = Form("h_Occupancy_Fe%d", cHybrid->getId());
                cObj  = gROOT->FindObject(cName);
                if(cObj) delete cObj;
                cProfile2D = new TProfile2D(cName, Form("Occupancy FE%d; Threshold [DAC units]; Number of strips with a hit", (int)cHybrid->getId()), 1023, 0 - 0.5, 1023. - 0.5, 2, 0, 2, "S");
                cProfile2D->GetYaxis()->SetBinLabel(1, "Bottom");
                cProfile2D->GetYaxis()->SetBinLabel(2, "Top");
                bookHistogram(cHybrid, "Occupancy_perSide", cProfile2D);
            }
            // event counter
            TString  cName = Form("h_EventCount_Be%d", cBoard->getId());
            TObject* cObj  = gROOT->FindObject(cName);
            if(cObj) delete cObj;
            TProfile* cProfile = new TProfile(cName, Form("Received events BE Board%d; Threshold [DAC units]; Number of hits", (int)cBoard->getId()), 1023, 0 - 0.5, 1023 - 0.5);
            bookHistogram(cBoard, "ReadoutEvents", cProfile);

            cName = Form("h_V1V5");
            cObj  = gROOT->FindObject(cName);
            if(cObj) delete cObj;
            cProfile = new TProfile(cName, Form("Measurement of 1V5 on SEH; Distance from pedestal; Corrected ADC reading [V]"), 100, -50 - 0.5, 50 - 0.5, "S");
            bookHistogram(cBoard, "Vmonitor1V5", cProfile);
        }
    }
    //
    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    // read original thresholds from chips ...
    fDetectorDataContainer = &fThresholds;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, fThresholds);
    // read original logic configuration from chips .. [Pipe&StubInpSel&Ptwidth , HIP&TestMode]
    fDetectorDataContainer = &fLogic;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, fLogic);
    fDetectorDataContainer = &fHIPs;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, fHIPs);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cHybrid)->getLinkId());
                // configure CBCs
                for(auto cChip: *cHybrid)
                {
                    fThresholds.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                        static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(static_cast<ReadoutChip*>(cChip), "VCth");
                    fLogic.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                        static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(static_cast<ReadoutChip*>(cChip), "Pipe&StubInpSel&Ptwidth");
                    fHIPs.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                        static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(static_cast<ReadoutChip*>(cChip), "HIP&TestMode");
                }
            }
        }
    }

    // for stub + hit data check
    ContainerFactory::copyAndInitStructure<int>(*fDetectorContainer, fHitCheckContainer);
    ContainerFactory::copyAndInitStructure<int>(*fDetectorContainer, fStubCheckContainer);
    zeroContainers();
}

void ExtraChecks::zeroContainers()
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    fHitCheckContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<int>()  = 0;
                    fStubCheckContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<int>() = 0;
                }
            }
        }
    }
}

void ExtraChecks::writeObjects()
{
    this->SaveResults();
    /*#ifdef __USE_ROOT__
        fDQMHistogramHybridTest.process();
    #endif*/
    fResultFile->Flush();
}
// State machine control functions
void ExtraChecks::Start(int currentRun) { Initialise(); }
void ExtraChecks::FindOpens()
{
    auto     cSetting = fSettingsMap.find("Nevents");
    uint32_t cNevents = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    for(int cAntennaGroup = 0; cAntennaGroup < 1; cAntennaGroup++)
    {
        // to do
        // select antenna switch

        // figure out correct latency range based on the firmware register settings
        // hard coded for now
        uint16_t              cStart    = 50;
        uint16_t              cMaxValue = 60;
        uint16_t              cStep     = 1;
        std::vector<uint16_t> cListOfLatencies(std::floor((cMaxValue - cStart) / cStep));
        uint16_t              cValue = cStart - cStep;
        std::generate(cListOfLatencies.begin(), cListOfLatencies.end(), [&]() { return cValue += cStep; });
        // prepare container
        std::vector<DetectorDataContainer*> cContainerVector(0);
        for(__attribute__((unused)) auto cLatency: cListOfLatencies)
        {
            cContainerVector.emplace_back(new DetectorDataContainer());
            ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *cContainerVector.back());
        }
        // scan dac
        scanDac("TriggerLatency", cListOfLatencies, cNevents, cContainerVector, cNevents);
        // get list of channels connected to this antenna ..
        std::vector<uint16_t> cChannels{0, 10, 100}; // for now.. just make up a list ...
        // now

        for(auto cLatency: cListOfLatencies)
        {
            LOG(INFO) << BOLDBLUE << "Latency value of " << +cLatency << RESET;
            for(auto cBoard: *fDetectorContainer)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        for(auto cChip: *cHybrid)
                        {
                            for(auto cChannel: cChannels)
                            {
                                auto& cOcc = cContainerVector.at(cContainerVector.size() - 1)
                                                 ->at(cBoard->getIndex())
                                                 ->at(cOpticalGroup->getIndex())
                                                 ->at(cHybrid->getIndex())
                                                 ->at(cChip->getIndex())
                                                 ->getChannelContainer<Occupancy>()
                                                 ->at(cChannel)
                                                 .fOccupancy;
                                LOG(INFO) << "Found an occupancy of " << cOcc << " for channel " << +cChannel << " of CBC" << +cChip->getId() << " on FE" << +cHybrid->getId() << RESET;
                            }
                        }
                    }
                }
            }
        }
    }
}

void ExtraChecks::Evaluate(int pSigma, uint16_t pTriggerRate, bool pDisableStubs)
{
    // parse xml file
    // now read the settings from the map
    auto     cSetting = fSettingsMap.find("Nevents");
    uint32_t cNevents = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;

    // get number of attempts
    cSetting         = fSettingsMap.find("Attempts");
    size_t cAttempts = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 10;

    LOG(INFO) << BOLDBLUE << "Quick [manual] check of noise and pedetal of the FE ASICs  ..." << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                // configure CBCs
                for(auto cChip: *cFe)
                {
                    // enable stub logic
                    if(pDisableStubs) { static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(static_cast<ReadoutChip*>(cChip), false, pDisableStubs, 0); }
                    else
                        static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(static_cast<ReadoutChip*>(cChip), "Sampled", true, true);
                }
            }
        }
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    }

    // get trigger rate from xml
    cSetting = fSettingsMap.find("TriggerRate");
    // get trigger multiplicity from xml
    cSetting = fSettingsMap.find("TriggerMultiplicity");

    // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(0,pTriggerRate,3,0,cDefaultStubLatency);
    // scan threshold and look at events (checking pipeline errors , L1 counters ,etc. )
    // extract pedestal and noise .. store in histogram
    ContainerFactory::copyAndInitChannel<float>(*fDetectorContainer, fNoiseContainer);
    ContainerFactory::copyAndInitChannel<float>(*fDetectorContainer, fPedestalContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard*           theBoard  = static_cast<BeBoard*>(cBoard);
        uint16_t           cStart    = 450;
        uint16_t           cMaxValue = 675;
        uint16_t           cStep     = 1;
        std::vector<float> cListOfThresholds(std::floor((cMaxValue - cStart) / cStep));
        uint16_t           cValue = cStart - cStep;
        std::generate(cListOfThresholds.begin(), cListOfThresholds.end(), [&]() { return cValue += cStep; });
        // prepare container to store result of threshold scan

        std::vector<DetectorDataContainer*> cContainerVector(0);
        TProfile*                           cEventHist = static_cast<TProfile*>(getHist(cBoard, "ReadoutEvents"));
        size_t                              cStepCount = 0;
        for(auto cVcth: cListOfThresholds)
        {
            // push new container into vector
            cContainerVector.emplace_back(new DetectorDataContainer());
            ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *cContainerVector.back());
            // set DAC .. read events
            this->setSameDacBeBoard(theBoard, "VCth", cVcth);
            for(size_t cIteration = 0; cIteration < cAttempts; cIteration++)
            {
                // fBeBoardInterface->ChipReSync ( cBoard );
                this->ReadNEvents(theBoard, cNevents);
                const std::vector<Event*>& cEvents = this->GetEvents(theBoard);
                if(cIteration == 0 && cStepCount % 10 == 0)
                { LOG(INFO) << BOLDBLUE << "Threshold set to " << cVcth << "...\tIteration " << +cIteration << " : " << +cEvents.size() << " events read back from fc7." << RESET; }

                cEventHist->Fill(cVcth, (int)cEvents.size());
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cFe: *cOpticalGroup)
                    {
                        TProfile2D* cHistOcc      = static_cast<TProfile2D*>(getHist(cFe, "Occupancy_perSide"));
                        TH2D*       cHitMapBottom = static_cast<TProfile2D*>(getHist(cFe, "HitMap_BottomSensor"));
                        TH2D*       cHitMapTop    = static_cast<TProfile2D*>(getHist(cFe, "HitMap_TopSensor"));

                        std::vector<int> cHitCounterBottom(0);
                        std::vector<int> cHitCounterTop(0);
                        for(auto cChip: *cFe)
                        {
                            int cNhits = 0;
                            LOG(DEBUG) << BOLDBLUE << "CBC" << +cChip->getId() << RESET;
                            TH2D* cHist         = static_cast<TH2D*>(getHist(static_cast<ReadoutChip*>(cChip), "Occupancy"));
                            TH2D* cHistPipeline = static_cast<TH2D*>(getHist(static_cast<ReadoutChip*>(cChip), "Pipeline"));
                            TH2D* cHistErrors   = static_cast<TH2D*>(getHist(static_cast<ReadoutChip*>(cChip), "ErrorFlag"));
                            TH2D* cHistL1Id     = static_cast<TH2D*>(getHist(static_cast<ReadoutChip*>(cChip), "L1"));
                            int   cEventCounter = cIteration * cNevents;
                            for(auto cEvent: cEvents)
                            {
                                // debug information
                                auto     cEventCount = cEvent->GetEventCount();
                                uint32_t cL1Id       = cEvent->L1Id(cFe->getId(), cChip->getId());
                                uint32_t cPipeline   = cEvent->PipelineAddress(cFe->getId(), cChip->getId());
                                uint32_t cError      = cEvent->Error(cFe->getId(), cChip->getId());
                                uint32_t cStatus     = static_cast<D19cCic2Event*>(cEvent)->Status(cFe->getId());
                                if((cEventCount % 10) == 0)
                                {
                                    LOG(DEBUG) << BOLDBLUE << "\t\t....Event " << +cEventCount << " ----  L1Id " << +cL1Id << " Pipeline address " << +cPipeline << " status bits from "
                                               << std::bitset<9>(cStatus) << RESET;
                                }
                                cHistPipeline->Fill(cEventCounter, cVcth, cPipeline);
                                cHistErrors->Fill(cEventCounter, cVcth, 1 + cError);
                                cHistL1Id->Fill(cEventCounter, cVcth, cL1Id);
                                if(cError != 0) LOG(INFO) << BOLDRED << "Event " << +cEventCounter << " : error in FE" << +cFe->getId() << " CBC" << +cChip->getId() << RESET;
                                // hits
                                std::vector<uint32_t> cHits = cEvent->GetHits(cFe->getId(), cChip->getId());
                                cHist->Fill(cVcth, static_cast<int>(cHits.size()));
                                for(auto cHit: cHits)
                                {
                                    int cSensorChannel = std::floor(cHit / 2.0) + cChip->getId() * 127;
                                    if(cHit % 2 == 0)
                                    {
                                        cHitCounterBottom.push_back(1);
                                        cHitMapBottom->Fill(cVcth, cSensorChannel, 1);
                                    }
                                    else
                                    {
                                        cHitCounterTop.push_back(1);
                                        cHitMapTop->Fill(cVcth, cSensorChannel, 1);
                                    }
                                    cContainerVector.at(cContainerVector.size() - 1)
                                        ->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cFe->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannelContainer<Occupancy>()
                                        ->at(cHit)
                                        .fOccupancy += 1;
                                    cNhits += 1;
                                }
                                cEventCounter++;
                            }
                        }
                        float cMeanHits_Bottom = std::accumulate(cHitCounterBottom.begin(), cHitCounterBottom.end(), 0.) / cEvents.size();
                        float cMeanHits_Top    = std::accumulate(cHitCounterTop.begin(), cHitCounterTop.end(), 0.) / cEvents.size();
                        cHistOcc->Fill(cVcth, 0., cMeanHits_Bottom);
                        cHistOcc->Fill(cVcth, 1., cMeanHits_Top);
                    }
                }
            }
            cStepCount++;
        }

        // process result of threshold scan to obtain pedestal and noise
        auto& cThisNoiseContainer   = fNoiseContainer.at(cBoard->getIndex());
        auto& cThisPedestalContiner = fPedestalContainer.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                TProfile2D* cPedestalHist = static_cast<TProfile2D*>(getHist(static_cast<OuterTrackerModule*>(cFe), "Pedestal_perSide"));
                TProfile2D* cNoiseHist    = static_cast<TProfile2D*>(getHist(static_cast<OuterTrackerModule*>(cFe), "Noise_perSide"));

                LOG(DEBUG) << BOLDBLUE << "FE" << +cFe->getId() << RESET;
                auto&  cHybridNoise    = cThisNoiseContainer->at(cOpticalGroup->getIndex())->at(cFe->getIndex());
                auto&  cHybridPedestal = cThisPedestalContiner->at(cOpticalGroup->getIndex())->at(cFe->getIndex());
                size_t cIter           = 0;

                for(auto cChip: *cFe)
                {
                    auto& cReadoutChipNoise    = cHybridNoise->at(cChip->getIndex());
                    auto& cReadoutChipPedestal = cHybridPedestal->at(cChip->getIndex());
                    for(uint8_t cChannel = 0; cChannel < NCHANNELS; cChannel++)
                    {
                        std::vector<float> cTmp(cListOfThresholds.size(), 0);
                        cIter = 0;
                        for(auto& cDetectorContainer: cContainerVector)
                        {
                            cTmp[cIter] = cDetectorContainer->at(cBoard->getIndex())
                                              ->at(cOpticalGroup->getIndex())
                                              ->at(cFe->getIndex())
                                              ->at(cChip->getIndex())
                                              ->getChannelContainer<Occupancy>()
                                              ->at(cChannel)
                                              .fOccupancy;
                            cDetectorContainer->at(cBoard->getIndex())
                                ->at(cOpticalGroup->getIndex())
                                ->at(cFe->getIndex())
                                ->at(cChip->getIndex())
                                ->getChannelContainer<Occupancy>()
                                ->at(cChannel)
                                .fOccupancy = 0;
                            cIter++;
                        }
                        std::pair<float, float> cNoiseEval                               = evalNoise(cTmp, cListOfThresholds);
                        cReadoutChipPedestal->getChannelContainer<float>()->at(cChannel) = cNoiseEval.first;
                        cReadoutChipNoise->getChannelContainer<float>()->at(cChannel)    = cNoiseEval.second;
                        LOG(DEBUG) << BOLDBLUE << "\t\t... Channel" << +cChannel << " : pedestal is " << cNoiseEval.first << " and noise is " << cNoiseEval.second << RESET;
                        TH1D* cHist = static_cast<TH1D*>(getHist(static_cast<ReadoutChip*>(cChip), "PedeNoise"));
                        cHist->SetBinContent(cHist->GetXaxis()->FindBin(cChannel), cNoiseEval.first);
                        cHist->SetBinError(cHist->GetXaxis()->FindBin(cChannel), cNoiseEval.second);
                        cTmp.clear();

                        int cSensorChannel = std::floor(cChannel / 2.0) + cChip->getId() * 127;
                        cPedestalHist->Fill(cSensorChannel, (cChannel % 2 == 0) ? 0 : 1, cNoiseEval.first);
                        cNoiseHist->Fill(cSensorChannel, (cChannel % 2 == 0) ? 0 : 1, cNoiseEval.second);
                    }
                    auto  cPedestal     = cReadoutChipPedestal->getChannelContainer<float>();
                    auto  cNoise        = cReadoutChipNoise->getChannelContainer<float>();
                    float cMeanPedestal = std::accumulate(cPedestal->begin(), cPedestal->end(), 0.) / cPedestal->size();
                    float cMeanNoise    = std::accumulate(cNoise->begin(), cNoise->end(), 0.) / cNoise->size();
                    // set noise to 3 sigma away from pedesata;
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(static_cast<ReadoutChip*>(cChip), "VCth", cMeanPedestal - pSigma * cMeanNoise);
                    LOG(INFO) << BOLDBLUE << "\tCBC" << +cChip->getId() << " : mean pedestal is " << cMeanPedestal << " and mean noise is " << cMeanNoise << " : setting threshold to "
                              << cMeanPedestal - pSigma * cMeanNoise << RESET;
                }
            }
        }
        cContainerVector.clear();
    }
    // to be included ---- automatic readout of current consumption
}

void ExtraChecks::OccupancyCheck(uint16_t pTriggerRate, bool pDisableStubs)
{
    auto     cSetting = fSettingsMap.find("Nevents");
    uint32_t cNevents = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;

    LOG(INFO) << BOLDBLUE << "Quick [manual] check of noise and pedetal of the FE ASICs  ..." << RESET;
    uint16_t cDefaultStubLatency = 50;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cFe)->getLinkId());
                // configure CBCs
                for(auto cChip: *cFe)
                {
                    // enable stub logic
                    if(pDisableStubs) { static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(static_cast<ReadoutChip*>(cChip), false, pDisableStubs, 0); }
                    else
                        static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(static_cast<ReadoutChip*>(cChip), "Sampled", true, true);
                }
            }
        }
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    }

    // measure hit occupancy - look for correlations
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(0, pTriggerRate, 3, 0, cDefaultStubLatency);
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Measuring hit correlations..." << RESET;
        for(size_t cIteration = 0; cIteration < 100; cIteration++)
        {
            LOG(INFO) << BOLDBLUE << "Iteration : " << +cIteration << RESET;
            this->ReadNEvents(theBoard, cNevents);
            const std::vector<Event*>& cEvents = this->GetEvents(theBoard);
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cFe: *cOpticalGroup)
                {
                    TH2D* cHitMap       = static_cast<TH2D*>(getHist(cFe, "HitMap"));
                    TH1D* cHitOccupancy = static_cast<TH1D*>(getHist(cFe, "NominalOccupancy"));

                    std::vector<int> cModuleOccupancy(0);
                    for(auto cEvent: cEvents)
                    {
                        std::vector<uint32_t> cHitDummy(0);
                        for(auto cChip: *cFe)
                        {
                            std::vector<uint32_t> cHits  = cEvent->GetHits(cFe->getId(), cChip->getId());
                            auto                  cCbcId = cChip->getId();
                            std::transform(cHits.begin(), cHits.end(), cHits.begin(), [cCbcId](int c) { return cCbcId * 254 + c; });
                            cHitDummy.insert(cHitDummy.end(), cHits.begin(), cHits.end());
                            for(auto cHit: cHits)
                            {
                                int cSensorChannel = std::floor(cHit / 2.0) + cChip->getId() * 127;
                                cHitMap->Fill(cSensorChannel, (cHit % 2 == 0) ? 0 : 1, 1);
                            }
                        }
                        cModuleOccupancy.push_back(cHitDummy.size());
                        cHitOccupancy->Fill(cHitDummy.size());
                    }
                    float cMeanOccupancy = std::accumulate(cModuleOccupancy.begin(), cModuleOccupancy.end(), 0.) / cModuleOccupancy.size();
                    char  cBuffer[200];
                    std::sprintf(cBuffer, "\tModule occupancy found to be %.2e for FE%d", cMeanOccupancy / (cFe->size() * NCHANNELS), cFe->getId());
                    LOG(INFO) << BOLDBLUE << cBuffer << RESET;
                }
            }
        }
    }
}
void ExtraChecks::ExternalTriggers(uint16_t pNconsecutive, const std::string& pSource)
{
    std::stringstream outp;
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(0, 10, 4, 0);
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        // configure trigger
        if(pSource == "TLU")
        {
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.trigger_source", 4);
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled", 1);
        }
        else
        {
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.trigger_source", 5);
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled", 0);
        }
        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", pNconsecutive);

        LOG(INFO) << BOLDRED << "Opening shutter ... press any key to close .." << RESET;
        fBeBoardInterface->Start(theBoard);
        do
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while(std::cin.get() != '\n');
        fBeBoardInterface->Stop(theBoard);

        LOG(INFO) << BOLDBLUE << "Stopping triggers..." << RESET;
        this->ReadData(theBoard, true);
        const std::vector<Event*>& cEvents = this->GetEvents(theBoard);
        LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadData" << RESET;
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cChip: *cFe)
                {
                    for(auto cEvent: cEvents)
                    {
                        uint32_t cPipeline   = cEvent->PipelineAddress(cFe->getId(), cChip->getId());
                        auto     cEventCount = cEvent->GetEventCount();
                        // uint32_t cL1Id = static_cast<D19cCicEvent*>(cEvent)->L1Id( cFe->getId(), cChip->getId() );
                        LOG(INFO) << BOLDBLUE << "Event " << +cEventCount << "\t\t....CBC" << +cChip->getId() << " on FE" << +cFe->getId() << " ----  Pipeline address " << +cPipeline << RESET;
                    }
                    LOG(INFO) << RESET;
                }
            }
        }
    }
    LOG(INFO) << BOLDBLUE << "Done!" << RESET;
}

void ExtraChecks::ConsecutiveTriggers(uint8_t pNconsecutive)
{
    auto              cSetting = fSettingsMap.find("Nevents");
    uint32_t          cNevents = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    std::stringstream outp;
    // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(0,10,3,0);
    // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureConsecutiveTriggerFSM(
    // cNevents, 0 , 0 );
    LOG(INFO) << BOLDBLUE << "Going to try and send " << +cNevents << " consecutive triggers to FE" << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        // set threshold
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        this->setSameDacBeBoard(theBoard, "VCth", 582);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cFe)->getLinkId());
                for(auto cChip: *cFe)
                {
                    if(cChip->getId() == 0) static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(static_cast<ReadoutChip*>(cChip), "VCth", 100);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(static_cast<ReadoutChip*>(cChip), {100, 150}, {0, 0}, true);
                }
            }
        }
        //
        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x0);
        fBeBoardInterface->ChipReSync(theBoard);
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ResetReadout();
        for(size_t cIndex = 0; cIndex < cNevents; cIndex++)
        {
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Trigger(pNconsecutive);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        this->ReadData(theBoard, true);

        // //fBeBoardInterface->Start(theBoard);
        // std::this_thread::sleep_for (std::chrono::seconds (20) );
        // fBeBoardInterface->Stop(theBoard);
        // this->ReadData( theBoard , true);
        // this->ReadNEvents ( theBoard , cNevents );
        const std::vector<Event*>& cEvents = this->GetEvents(theBoard);
        LOG(INFO) << BOLDBLUE << +cEvents.size() << " events read back from FC7 with ReadData" << RESET;
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cChip: *cFe)
                {
                    for(auto cEvent: cEvents)
                    {
                        uint32_t cPipeline   = cEvent->PipelineAddress(cFe->getId(), cChip->getId());
                        auto     cEventCount = cEvent->GetEventCount();
                        LOG(INFO) << BOLDBLUE << "Event " << +cEventCount << "\t\t....CBC" << +cChip->getId() << " on FE" << +cFe->getId() << " ----  Pipeline address " << +cPipeline << RESET;
                        // uint32_t cL1Id = static_cast<D19cCicEvent*>(cEvent)->L1Id( cFe->getId(), cChip->getId() );
                        // LOG (INFO) << "Event " << +cEventCount << "\t\t....CBC " << +cChip->getId() << "on FE" <<
                        // +cFe->getId() << " ----  Pipeline address " << +cPipeline << RESET;
                    }
                    LOG(INFO) << RESET;
                }
            }
        }
    }
    LOG(INFO) << BOLDBLUE << "Done!" << RESET;
}

void ExtraChecks::MonitorAmux(bool pAll)
{
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        // get result of pedestal and noise
        auto& cThisPedestalContiner = fPedestalContainer.at(cBoard->getIndex());

        // one chip at a time
        TProfile* cMonitor1V5 = static_cast<TProfile*>(getHist(cBoard, "Vmonitor1V5"));
        if(pAll)
        {
            // all chips together
            LOG(INFO) << BOLDBLUE << "Scanning threshold on all chips simultaneously - and recording AMUX voltages..." << RESET;
            // fixed values
            std::vector<uint16_t> cThresholdValues{0, 50, 100};
            for(auto cThresholdValue: cThresholdValues)
            {
                // set DAC
                this->setSameDacBeBoard(theBoard, "VCth", cThresholdValue);
                LOG(INFO) << BOLDBLUE << "Threshold on all chips set to " << +cThresholdValue << " DAC units..." << RESET;
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cFe: *cOpticalGroup)
                    {
                        TProfile2D* cScan = static_cast<TProfile2D*>(getHist(cFe, "VcthScan"));
                        for(auto cChip: *cFe)
                        {
                            std::vector<float> cValues(0);
                            for(size_t cIter = 0; cIter < 5; cIter++)
                            {
                                std::pair<uint16_t, float> cReading = ReadAmux(cFe->getId(), cChip->getId(), "VCth", theBoard->ifOptical());
                                cValues.push_back(cReading.second);
                                cScan->Fill(cChip->getId(), cThresholdValue, cReading.second);
                                //
                                // cReading = ReadAmux(cFe->getId(), cChip->getId() , "VBGbias", theBoard->ifOptical()
                                // ); cScanVBGbias->Fill( cChip->getId() , cThresholdValue , cReading.second );
                                // //
                                // cReading = ReadAmux(cFe->getId(), cChip->getId() , "VBG_LDO", theBoard->ifOptical()
                                // ); cScanVBGldo->Fill( cChip->getId() , cThresholdValue , cReading.second );
                            }
                            std::pair<float, float> cStats = getStats(cValues);
                            LOG(INFO) << BOLDBLUE << "\tFE" << +cFe->getId() << "\t...CBC" << +cChip->getId() << " : " << cStats.first << " [ " << cStats.second << " ]" << RESET;
                        }
                    }
                }
            }
            // fixed distances
            for(int cDistance = -20; cDistance <= 20; cDistance++)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cFe: *cOpticalGroup)
                    {
                        for(auto cChip: *cFe)
                        {
                            auto     cPedestal     = cThisPedestalContiner->at(cOpticalGroup->getIndex())->at(cFe->getIndex())->at(cChip->getIndex())->getChannelContainer<float>();
                            float    cMeanPedestal = std::accumulate(cPedestal->begin(), cPedestal->end(), 0.) / cPedestal->size();
                            uint16_t cThreshold    = static_cast<uint16_t>(std::floor(cMeanPedestal + cDistance));
                            static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(static_cast<ReadoutChip*>(cChip), "VCth", cThreshold);
                        }
                    }
                }
                // measure
                std::vector<float> cMonitorVoltage(0);
                for(size_t cIter = 0; cIter < 5; cIter++)
                {
                    std::pair<uint16_t, float> cVM1V5 = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->readADC("VM1V5", true);
                    cMonitorVoltage.push_back(cVM1V5.second);
                    cMonitor1V5->Fill(cDistance, cVM1V5.second);
                }
                std::pair<float, float> cMonitor = getStats(cMonitorVoltage);
                LOG(INFO) << BOLDBLUE << "Threshold on all chips set to " << +cDistance << " DAC units away from the pedestal; Vmonitor [1.5V] on SEH reads " << cMonitor.first << " V ("
                          << cMonitor.second * 1e3 << " mV)" << RESET;
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cFe: *cOpticalGroup)
                    {
                        auto&       cHybridPedestal = cThisPedestalContiner->at(cOpticalGroup->getIndex())->at(cFe->getIndex());
                        TProfile2D* cScan           = static_cast<TProfile2D*>(getHist(cFe, "VcthScan"));
                        for(auto cChip: *cFe)
                        {
                            auto&              cReadoutChipPedestal = cHybridPedestal->at(cChip->getIndex());
                            auto               cPedestal            = cReadoutChipPedestal->getChannelContainer<float>();
                            float              cMeanPedestal        = std::accumulate(cPedestal->begin(), cPedestal->end(), 0.) / cPedestal->size();
                            uint16_t           cThreshold           = static_cast<uint16_t>(std::floor(cMeanPedestal + cDistance));
                            std::vector<float> cValues(0);
                            for(size_t cIter = 0; cIter < 5; cIter++)
                            {
                                std::pair<uint16_t, float> cReading = ReadAmux(cFe->getId(), cChip->getId(), "VCth", theBoard->ifOptical());
                                cValues.push_back(cReading.second);
                                cScan->Fill(cChip->getId(), cThreshold, cReading.second);
                                //
                                // cReading = ReadAmux(cFe->getId(), cChip->getId() , "VBGbias", theBoard->ifOptical()
                                // ); cScanVBGbias->Fill( cChip->getId() , cThreshold , cReading.second );
                                // //
                                // cReading = ReadAmux(cFe->getId(), cChip->getId() , "VBG_LDO", theBoard->ifOptical()
                                // ); cScanVBGldo->Fill( cChip->getId() , cThreshold , cReading.second );
                            }
                            std::pair<float, float> cStats = getStats(cValues);
                            LOG(INFO) << BOLDBLUE << "\tFE" << +cFe->getId() << "\t...CBC" << +cChip->getId() << " : " << cStats.first << " [ " << cStats.second << " ]" << RESET;
                        }
                    }
                }
            }
        }
        else
        {
            LOG(INFO) << BOLDBLUE << "Scanning threshold on one chip at a time - and recording AMUX voltages..." << RESET;
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cFe: *cOpticalGroup)
                {
                    LOG(INFO) << BOLDBLUE << "FE" << +cFe->getId() << RESET;
                    auto& cHybridPedestal = cThisPedestalContiner->at(cOpticalGroup->getIndex())->at(cFe->getIndex());
                    LOG(INFO) << BOLDBLUE << "Scanning threshold on chips [one at a time] - and recording AMUX voltages..." << RESET;
                    TProfile2D* cScan = static_cast<TProfile2D*>(getHist(cFe, "VcthScan_perChip"));
                    for(uint8_t cChipIndex = 0; cChipIndex < cFe->size(); cChipIndex++)
                    {
                        auto& cReadoutChipPedestal = cHybridPedestal->at(cChipIndex);
                        auto  cPedestal            = cReadoutChipPedestal->getChannelContainer<float>();
                        float cMeanPedestal        = std::accumulate(cPedestal->begin(), cPedestal->end(), 0.) / cPedestal->size();
                        LOG(INFO) << BOLDBLUE << "\tCBC" << +cChipIndex << " : mean pedestal is " << cMeanPedestal << RESET;
                        // set threshold on all CBCs except the one being scanned to 0
                        for(auto cChip: *cFe)
                        {
                            if(cChip->getIndex() == cChipIndex) continue;
                            static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(static_cast<ReadoutChip*>(cChip), "VCth", static_cast<uint16_t>(0));
                        }

                        // now scan thereshold on one of the CBCs
                        for(auto cChip: *cFe)
                        {
                            if(cChip->getIndex() != cChipIndex) continue;
                            // fixed values
                            std::vector<float> cThresholdValues{0., 50., 900., 1000.};
                            for(auto cThreshold: cThresholdValues)
                            {
                                static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(static_cast<ReadoutChip*>(cChip), "VCth", static_cast<uint16_t>(cThreshold));
                                for(size_t cIter = 0; cIter < 5; cIter++)
                                {
                                    std::pair<uint16_t, float> cReading = ReadAmux(cFe->getId(), cChip->getId(), "VCth", theBoard->ifOptical());
                                    cScan->Fill(cChip->getId(), cThreshold, cReading.second);
                                    if((cChip->getIndex() == cChipIndex) && cIter == 0)
                                        LOG(INFO) << BOLDBLUE << "\t\t.... Setting threshold to " << +static_cast<uint16_t>(cThreshold)
                                                  << " and recording voltage at output of AMUX : " << cReading.second << RESET;

                                    // cReading = ReadAmux(cFe->getId(), cChip->getId() , "VBGbias", cBoard->ifOptical()
                                    // ); cScanVBGbias->Fill( cChip->getId() , cThreshold , cReading.second );
                                    // //
                                    // cReading = ReadAmux(cFe->getId(), cChip->getId() , "VBG_LDO", cBoard->ifOptical()
                                    // ); cScanVBGldo->Fill( cChip->getId() , cThreshold , cReading.second );
                                }
                            }
                            // near the pedestal
                            for(int cDistance = -10; cDistance <= 10; cDistance++)
                            {
                                static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(static_cast<ReadoutChip*>(cChip), "VCth", static_cast<uint16_t>(std::floor(cMeanPedestal + cDistance)));
                                for(size_t cIter = 0; cIter < 5; cIter++)
                                {
                                    std::pair<uint16_t, float> cReading = ReadAmux(cFe->getId(), cChip->getId(), "VCth", theBoard->ifOptical());
                                    cScan->Fill(cChip->getId(), std::floor(cMeanPedestal + cDistance), cReading.second);
                                    if((cChip->getIndex() == cChipIndex) && cIter == 0)
                                        LOG(INFO) << BOLDBLUE << "\t\t.... Setting threshold to " << +static_cast<uint16_t>(cMeanPedestal + cDistance)
                                                  << " and recording voltage at output of AMUX : " << cReading.second << " mV." << RESET;
                                    // bias
                                    // cReading = ReadAmux(cFe->getId(), cChip->getId() , "VBGbias", cBoard->ifOptical()
                                    // ); cScanVBGbias->Fill( cChip->getId() , std::floor(cMeanPedestal + cDistance) ,
                                    // cReading.second );
                                    // //ldo
                                    // cReading = ReadAmux(cFe->getId(), cChip->getId() , "VBGbias", cBoard->ifOptical()
                                    // ); cScanVBGldo->Fill( cChip->getId() , std::floor(cMeanPedestal + cDistance) ,
                                    // cReading.second );
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
// check hits and stubs with the TP
void ExtraChecks::DataCheckTP(std::vector<uint8_t> pChipIds, uint8_t pTPamplitude, int pTPgroup, int pBendCode, bool pScan)
{
    auto     cSetting          = fSettingsMap.find("Nevents");
    uint32_t cEventsPerAttempt = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    // configure FE chips so that stubs are detected [i.e. make sure HIP
    // suppression is off ]
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cFe)->getLinkId());
                // configure CBCs
                for(auto cChip: *cFe)
                {
                    // switch off HitOr
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(static_cast<ReadoutChip*>(cChip), "HitOr", 0);
                    // enable stub logic
                    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(static_cast<ReadoutChip*>(cChip), "Sampled", true, true);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(static_cast<ReadoutChip*>(cChip), false, false, 0);
                }
            }
        }
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    }

    // generate stubs in exactly one chip
    int cBend = 0;
    // seeds and bends needed to generate fixed pattern on SLVS lines carrying stub information from CBCs --> CICs
    std::vector<uint8_t> cSeeds{static_cast<uint8_t>((pTPgroup * 2 + 16 * 0 + 1) * 2)};
    std::vector<int>     cBends(cSeeds.size(), cBend);
    SetStubWindowOffsets(pBendCode, cBend);
    std::vector<int> cExpectedHits(0);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cFe)->getLinkId());
                for(auto cChip: *cFe)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    if(std::find(pChipIds.begin(), pChipIds.end(), cChip->getId()) != pChipIds.end())
                    {
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theChip, cSeeds, cBends, false);
                        for(size_t cIndex = 0; cIndex < cSeeds.size(); cIndex++)
                        {
                            std::vector<uint8_t> cHits = static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern(theChip, cSeeds[cIndex], cBends[cIndex]);
                            for(auto& cHit: cHits)
                            {
                                if(std::find(cExpectedHits.begin(), cExpectedHits.end(), cHit) == cExpectedHits.end()) cExpectedHits.push_back(cHit);
                            }
                        }
                    }
                    else
                        static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(theChip, true);
                }
            }
        }
    }
    LOG(INFO) << BOLDBLUE << "Expect to see hits in channels " << RESET;
    for(auto cHit: cExpectedHits) { LOG(INFO) << BOLDBLUE << "\t\t.... " << +cHit << RESET; }

    // set-up for TP
    fAllChan                     = true;
    fMaskChannelsFromOtherGroups = !this->fAllChan;
    this->enableTestPulse(true);
    this->SetTestPulse(true);
    setSameGlobalDac("TestPulsePotNodeSel", pTPamplitude);
    uint16_t cTPdelay = 0;
    // check that the hits are there... so find test pulse
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        // configure test pulse trigger
        uint16_t cFirmwareTPdelay = 100;
        uint16_t cDelay           = 100;
        uint16_t cTPsequence      = 1000;
        uint16_t cFastReset       = 0;
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(cFirmwareTPdelay, cDelay, cTPsequence, cFastReset);
        this->setSameDacBeBoard(theBoard, "TestPulseDelay", cTPdelay);
        for(int cLatencyOffset = -1; cLatencyOffset < 0; cLatencyOffset++)
        {
            uint16_t cLatency = cDelay + cLatencyOffset;
            LOG(INFO) << BOLDBLUE << "Latency set to " << +cLatency << RESET;
            this->setSameDacBeBoard(theBoard, "TriggerLatency", cLatency);
            fBeBoardInterface->ChipReSync(theBoard);

            uint16_t cStubLatency    = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
            uint16_t cStartScanRange = (pScan) ? 0 : cStubLatency;
            uint16_t cStopScanRange  = (pScan) ? 100 : cStubLatency + 1;

            for(uint16_t cStubLatency = cStartScanRange; cStubLatency < cStopScanRange; cStubLatency++) // for( uint16_t cStubLatency = (cDelay-85) ; cStubLatency < (cDelay-59) ;
                                                                                                        // cStubLatency++ )
            {
                fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
                LOG(INFO) << BOLDBLUE << "Stub latency set to " << +cStubLatency << RESET;
                // fBeBoardInterface->ChipReSync ( theBoard );

                this->ReadNEvents(theBoard, cEventsPerAttempt);
                const std::vector<Event*>& cEvents = this->GetEvents(theBoard);
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cFe: *cOpticalGroup)
                    {
                        int         cNstubs       = 0;
                        TProfile2D* cMatchedBends = static_cast<TProfile2D*>(getHist(cFe, "CorrectBend_TP"));
                        TProfile2D* cMatchedSeeds = static_cast<TProfile2D*>(getHist(cFe, "CorrectSeed_TP"));
                        TProfile2D* cStubsFound   = static_cast<TProfile2D*>(getHist(cFe, "Nstubs_TP"));
                        // debug information
                        for(auto cEvent: cEvents)
                        {
                            auto cBxId       = cEvent->BxId(cFe->getId());
                            auto cEventCount = cEvent->GetEventCount();
                            LOG(INFO) << BOLDBLUE << "Event " << +cEventCount << " - BxId - " << +cBxId << RESET;
                        }
                        int cNmatchedHits = 0;
                        for(auto cChip: *cFe)
                        {
                            if(std::find(pChipIds.begin(), pChipIds.end(), cChip->getId()) == pChipIds.end()) continue;

                            int cNstubsCBC = 0;
                            // check stubs + hits
                            for(auto cEvent: cEvents)
                            {
                                // debug information
                                auto cHits = cEvent->GetHits(cFe->getId(), cChip->getId());
                                for(auto cHit: cHits)
                                {
                                    if(std::find(cExpectedHits.begin(), cExpectedHits.end(), cHit) != cExpectedHits.end()) { cNmatchedHits += 1; }
                                }

                                auto cStubs = cEvent->StubVector(cFe->getId(), cChip->getId());
                                for(auto cStub: cStubs)
                                {
                                    cMatchedSeeds->Fill(cChip->getId(), cDelay - cStubLatency, cStub.getPosition() == cSeeds[0]);
                                    cMatchedBends->Fill(cChip->getId(), cDelay - cStubLatency, cStub.getBend() == pBendCode);
                                    LOG(DEBUG) << BOLDMAGENTA << "Stub with seed " << +cStub.getPosition() << " with bend " << +cStub.getBend() << RESET;
                                }
                                if(cStubs.size() == 0)
                                {
                                    cMatchedBends->Fill(cChip->getId(), cDelay - cStubLatency, 0);
                                    cMatchedSeeds->Fill(cChip->getId(), cDelay - cStubLatency, 0);
                                }
                                cNstubs += cStubs.size();
                                cNstubsCBC += cStubs.size();
                            }
                            cStubsFound->Fill(cChip->getId(), cDelay - cStubLatency, cNstubsCBC);
                        }
                        LOG(INFO) << BOLDBLUE << "Stub latency of " << +cStubLatency << " FE" << +cFe->getId() << " : " << cNstubs << " stubs and " << +cNmatchedHits << " matched hits." << RESET;
                    }
                }
            }
        }
    }
    // disable TP
    this->enableTestPulse(false);
    this->SetTestPulse(false);
    setSameGlobalDac("TestPulsePotNodeSel", 0x00);

    // unmask all channels and reset offsets
    // also re-configure thresholds + hit/stub detect logic to original values
    // and re-load configuration of fast command block from register map loaded from xml file
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard             = static_cast<BeBoard*>(cBoard);
        auto&    cThresholdsThisBoard = fThresholds.at(cBoard->getIndex());
        auto&    cLogicThisBoard      = fLogic.at(cBoard->getIndex());
        auto&    cHIPsThisBoard       = fHIPs.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                auto& cThresholdsThisHybrid = cThresholdsThisBoard->at(cOpticalGroup->getIndex())->at(cFe->getIndex());
                auto& cLogicThisHybrid      = cLogicThisBoard->at(cOpticalGroup->getIndex())->at(cFe->getIndex());
                auto& cHIPsThisHybrid       = cHIPsThisBoard->at(cOpticalGroup->getIndex())->at(cFe->getIndex());
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cFe)->getLinkId());
                for(auto cChip: *cFe)
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
        LOG(INFO) << BOLDBLUE << "Re-loading original coonfiguration of fast command block from hardware description file [.xml] " << RESET;
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFastCommandBlock(theBoard);
        //
        fBeBoardInterface->ChipReSync(theBoard);
    }
}

void ExtraChecks::ReconstructTP(uint8_t pTPamplitude, uint8_t pGroup, uint8_t pStep)
{
    // settings for TP trigger
    uint8_t cFirmwareTPdelay      = 100;
    uint8_t cFirmwareTriggerDelay = 200;

    // parse xml file
    // now read the settings from the map
    auto     cSetting        = fSettingsMap.find("Nevents");
    uint32_t cEventsPerPoint = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;

    // set-up for TP
    fAllChan                     = false;
    fMaskChannelsFromOtherGroups = !this->fAllChan;

    this->enableTestPulse(true);
    this->SetTestPulse(true);

    setSameGlobalDac("TestPulsePotNodeSel", pTPamplitude);
    // configure test pulse trigger
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(cFirmwareTPdelay, cFirmwareTriggerDelay, 1000);
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Bx0Alignment();
    // check that the hits are there... so find test pulse
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        // this->setSameDacBeBoard(cBoard, "TestPulseGroup", pGroup);
        int cDelayAfterTPfastCommand = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
        for(int cSamplingTime = 50; cSamplingTime <= 75; cSamplingTime += static_cast<int>(pStep))
        {
            uint16_t cTriggerLatency = static_cast<uint16_t>(cDelayAfterTPfastCommand - std::floor(cSamplingTime / 25.));
            int      cTPdelay        = -1 * cSamplingTime + 25 - 25 * (cTriggerLatency - cDelayAfterTPfastCommand);
            this->setSameDacBeBoard(theBoard, "TriggerLatency", cTriggerLatency);
            this->setSameDacBeBoard(theBoard, "TestPulseDelay", cTPdelay);
            LOG(INFO) << BOLDMAGENTA << "Starting threshold scan : Latency set to " << +cTriggerLatency << " TP delay set to " << cTPdelay << RESET;

            // bitwise scan
            // this->SetTestAllChannels(fAllChan);
            // fMaskChannelsFromOtherGroups = !this->fAllChan;
            // DetectorDataContainer     theOccupancyContainer;
            // fDetectorDataContainer = &theOccupancyContainer;
            // ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
            // this->bitWiseScan("VCth", cEventsPerPoint, 0.50);
            // // now find the pedestal
            // for (auto& cFe : cBoard->fModuleVector)
            // {
            //     LOG (INFO) << BOLDBLUE << "FE" << +cFe->getId() << RESET;
            //     for (auto& cChip : cFe->fReadoutChipVector)
            //     {
            //         uint16_t cThreshold = cChip->getReg("VCth1") + (  cChip->getReg("VCth2")<<8 );
            //         LOG (INFO) << BOLDGREEN << "\t...Vcth on CBC" << +cChip->getId() << " found to be : " <<
            //         cThreshold << " Vcth units." << RESET;
            //         // for(ChannelDataContainer<Occupancy>::iterator cChannel =  cChip->begin<Occupancy>(); cChannel
            //         != cChip->end<Occupancy>(); cChannel++, cIndex++)
            //         // {
            //         //     auto& cOccupancy = cChannel->fOccupancy;
            //         // }
            //     }
            // }

            uint16_t              cStart    = 450;
            uint16_t              cMaxValue = 650;
            uint16_t              cStep     = 1;
            std::vector<uint16_t> cListOfThresholds(std::floor((cMaxValue - cStart) / cStep));
            uint16_t              cValue = cStart - cStep;
            std::generate(cListOfThresholds.begin(), cListOfThresholds.end(), [&]() { return cValue += cStep; });
            // prepare scan
            std::vector<DetectorDataContainer*> cContainerVector(0);
            cContainerVector.reserve(cListOfThresholds.size());
            std::string cDacName = "VCth";
            for(size_t cIndex = 0; cIndex < cListOfThresholds.size(); cIndex++)
            {
                cContainerVector.emplace_back(new DetectorDataContainer());
                ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *cContainerVector.back());
                // do scan
                this->setSameDacBeBoard(theBoard, cDacName, cListOfThresholds.at(cIndex));
                fDetectorDataContainer = cContainerVector.at(cContainerVector.size() - 1);
                measureBeBoardData(cBoard->getIndex(), cEventsPerPoint);
                // look at result
                LOG(INFO) << BOLDMAGENTA << "\t\t...Scanning " << cDacName << " to reconstruct TP - threshold now set to " << cListOfThresholds.at(cIndex) << RESET;
                //" found average occupancy on chip0 to be : " << cExample.fOccupancy << RESET;
            }
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cFe: *cOpticalGroup)
                {
                    LOG(INFO) << BOLDBLUE << "FE" << +cFe->getId() << RESET;
                    for(auto cChip: *cFe)
                    {
                        for(uint8_t cChannel = 0; cChannel < NCHANNELS; cChannel++)
                        {
                            std::vector<float> cTmp(cListOfThresholds.size(), 0);
                            std::vector<float> cValues(cListOfThresholds.size(), 0);
                            size_t             cIter = 0;
                            for(auto cDetectorContainer: cContainerVector)
                            {
                                cTmp[cIter] = cDetectorContainer->at(cBoard->getIndex())
                                                  ->at(cOpticalGroup->getIndex())
                                                  ->at(cFe->getIndex())
                                                  ->at(cChip->getIndex())
                                                  ->getChannelContainer<Occupancy>()
                                                  ->at(cChannel)
                                                  .fOccupancy;
                                cValues[cIter] = static_cast<float>(cListOfThresholds[cIter]);
                                cDetectorContainer->at(cBoard->getIndex())
                                    ->at(cOpticalGroup->getIndex())
                                    ->at(cFe->getIndex())
                                    ->at(cChip->getIndex())
                                    ->getChannelContainer<Occupancy>()
                                    ->at(cChannel)
                                    .fOccupancy = 0;
                                cIter++;
                            }
                            std::pair<float, float> cNoiseEval = this->evalNoise(cTmp, cValues);
                            if(cChannel % 5 == 0 && cChip->getId() == 0)
                                LOG(INFO) << BOLDBLUE << "Chip" << +cChip->getId() << " T = " << 0 << " - Channel" << +cChannel << " : pedestal is " << cNoiseEval.first << " and noise is "
                                          << cNoiseEval.second << RESET;
                            cTmp.clear();
                        }
                    }
                }
            }
            cContainerVector.clear();
        }
    }
    // disable TP
    this->enableTestPulse(false);
    this->SetTestPulse(false);
    setSameGlobalDac("TestPulsePotNodeSel", 0x00);
}
// check stubs ..
void ExtraChecks::QuickStubCheck(std::vector<uint8_t> pChipIds, uint16_t pTriggerRate, uint8_t pSeed, int pBend)
{
    auto     cSetting            = fSettingsMap.find("Nevents");
    uint32_t cEventsPerPoint     = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    uint16_t cDefaultStubLatency = 50;

    // configure FE chips so that stubs are detected [i.e. make sure HIP
    // suppression is off ]
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cHybrid)->getLinkId());
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    // switch off HitOr
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theChip, "HitOr", 0);
                    // enable stub logic
                    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(theChip, "Sampled", true, true);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(theChip, false, false, 0);
                }
            }
        }
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    }

    // generate stubs in exactly N chip(s)
    uint8_t cBendCode = 0x00;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cHybrid)->getLinkId());
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    if(std::find(pChipIds.begin(), pChipIds.end(), cChip->getId()) != pChipIds.end())
                    {
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theChip, {pSeed}, {pBend}, true);
                        // read bend LUT
                        std::vector<uint8_t> cBendLUT = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theChip);
                        cBendCode                     = cBendLUT[(pBend / 2. - (-7.0)) / 0.5]; // each bend code is stored in this vector - bend encoding start at
                                                                                               // -7 strips, increments by 0.5 strips
                        LOG(INFO) << BOLDBLUE << "Injecting a stub in position " << +pSeed << " with bend " << pBend << " --- bend code is 0x" << std::hex << +cBendCode << std::dec << RESET;
                    }
                    else
                        static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(theChip, true);
                }
            }
        }
    }

    // configure trigger generation in firmware
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(0, pTriggerRate, 3, 0, cDefaultStubLatency);
    // in theory can scan over this .. but 3 is the right number

    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);

        auto cStubPackageDelay = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
        LOG(INFO) << BOLDBLUE << "Stub package delay to " << +cStubPackageDelay << RESET;

        // uint16_t cDelay = fBeBoardInterface->ReadBoardReg( theBoard,
        // "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse") ;
        // why do I need this?
        // this->setSameDacBeBoard(theBoard, "TriggerLatency", cDelay);
        this->ReadNEvents(theBoard, cEventsPerPoint);
        const std::vector<Event*>& cEvents = this->GetEvents(theBoard);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                auto cFeId = cFe->getId();
                LOG(INFO) << BOLDBLUE << "Link Id : " << +static_cast<OuterTrackerModule*>(cFe)->getLinkId() << RESET;
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cFe)->getLinkId());
                TH2D*     cBxCounter    = static_cast<TH2D*>(getHist(cFe, "BxCounter"));
                TProfile* cMatchedStubs = static_cast<TProfile*>(getHist(cFe, "MatchedStubs"));
                TProfile* cMatchedBends = static_cast<TProfile*>(getHist(cFe, "CorrectBend"));
                TProfile* cMatchedSeeds = static_cast<TProfile*>(getHist(cFe, "CorrectSeed"));
                for(auto cChip: *cFe)
                {
                    auto cChipId = cChip->getId();

                    // if( std::find(pChipIds.begin(), pChipIds.end(), cChipId) == pChipIds.end()  )
                    //    continue;
                    int cNhits = 0;
                    LOG(INFO) << BOLDBLUE << "\t.. CBC" << +cChipId << RESET;
                    int cEventCounter = 0;
                    for(auto cEvent: cEvents)
                    {
                        // debug information
                        auto cBxId       = cEvent->BxId(cFeId);
                        auto cEventCount = cEvent->GetEventCount();
                        LOG(INFO) << BOLDBLUE << "\t\t... Event " << +cEventCount << " FE" << +cFeId << " - Bx " << +cBxId << RESET;

                        cBxCounter->Fill(static_cast<float>(cEventCount), cChipId, cBxId);
                        // hits
                        auto cHits = cEvent->GetHits(cFeId, cChipId);
                        for(auto cHit: cHits) { LOG(DEBUG) << BOLDMAGENTA << "\t\t... hit found in channel " << +cHit << RESET; }
                        cNhits += cHits.size();
                        // std::vector<uint8_t> cExpectedHits =
                        // static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern( cChip, pSeed, pBend
                        // ); for( auto cExpectedHit : cExpectedHits )
                        // {
                        //     cMatchedHits->Fill( cChipId , std::find(  cHits.begin(), cHits.end(), cExpectedHit) !=
                        //     cHits.end() ) ;
                        // }
                        // if( cHits.size() == 0 )
                        //     cMatchedHits->Fill( cChipId , 0);

                        auto cStubs = cEvent->StubVector(cFeId, cChipId);
                        for(auto cStub: cStubs)
                        {
                            LOG(INFO) << BOLDMAGENTA << "\t\t... stub seed " << +cStub.getPosition() << " --- bend code of " << +cStub.getBend() << RESET;
                            cMatchedStubs->Fill(cChipId, cStub.getPosition() == pSeed && cStub.getBend() == cBendCode);
                            cMatchedBends->Fill(cChipId, cStub.getBend() == cBendCode);
                            cMatchedSeeds->Fill(cChipId, cStub.getPosition() == pSeed);
                        }
                        cEventCounter++;
                        if(cStubs.size() == 0)
                        {
                            cMatchedBends->Fill(cChipId, 0);
                            cMatchedSeeds->Fill(cChipId, 0);
                            cMatchedStubs->Fill(cChipId, 0);
                        }
                    }
                }
            }
        }
    }

    // unmask all channels and reset offsets
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cHybrid)->getLinkId());
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(theChip, false);
                    // set offsets back to default value
                    fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset12", (0 << 4) | (0 << 0));
                    fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset34", (0 << 4) | (0 << 0));
                }
            }
        }
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    }
}
// check hits and stubs using noise
void ExtraChecks::DataCheck(std::vector<uint8_t> pChipIds, uint16_t pTriggerRate, uint8_t pSeed, int pBend, bool pScan)
{
    auto     cSetting            = fSettingsMap.find("Nevents");
    uint32_t cEventsPerPoint     = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    uint16_t cDefaultStubLatency = 50;

    // configure FE chips so that stubs are detected [i.e. make sure HIP
    // suppression is off ]
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    // switch off HitOr
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theChip, "HitOr", 0);
                    // enable stub logic
                    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(theChip, "Sampled", true, true);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(theChip, false, false, 0);
                }
            }
        }
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    }
    // generate stubs in exactly one chip
    uint8_t          cBendCode = 0x00;
    std::vector<int> cExpectedHits(0);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cHybrid)->getLinkId());
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    if(std::find(pChipIds.begin(), pChipIds.end(), cChip->getId()) != pChipIds.end())
                    {
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theChip, {pSeed}, {pBend}, true);
                        // read bend LUT
                        std::vector<uint8_t> cBendLUT = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theChip);
                        cBendCode                     = cBendLUT[(pBend / 2. - (-7.0)) / 0.5]; // each bend code is stored in this vector - bend encoding start at
                                                                                               // -7 strips, increments by 0.5 strips
                        LOG(DEBUG) << BOLDBLUE << "Injecting a stub in position " << +pSeed << " with bend " << pBend << " --- bend code is 0x" << std::hex << +cBendCode << std::dec << RESET;
                        std::vector<uint8_t> cHits = static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern(theChip, pSeed, pBend);
                        for(auto& cHit: cHits)
                        {
                            if(std::find(cExpectedHits.begin(), cExpectedHits.end(), cHit) == cExpectedHits.end()) cExpectedHits.push_back(cHit);
                        }
                    }
                    else
                        static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(theChip, true);
                }
            }
        }
    }

    LOG(DEBUG) << BOLDBLUE << "Expect to see hits in channels " << RESET;
    for(auto cHit: cExpectedHits) { LOG(DEBUG) << BOLDBLUE << "\t\t.... " << +cHit << RESET; }

    // configure trigger generation in firmware
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(0, pTriggerRate, 3, 0, cDefaultStubLatency);
    uint16_t cStartScanRange = (pScan) ? 0 : 0;
    uint16_t cStopScanRange  = (pScan) ? 8 : 1;

    for(uint8_t cPackageDelay = cStartScanRange; cPackageDelay < cStopScanRange; cPackageDelay++)
    {
        // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Bx0Alignment(cPackageDelay);
        zeroContainers();
        for(auto cBoard: *fDetectorContainer)
        {
            BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
            if(pScan)
            {
                LOG(INFO) << BOLDBLUE << "Setting package delay to " << +cPackageDelay << RESET;
                fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay", cPackageDelay);
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Bx0Alignment();
            }
            // read N events
            this->ReadNEvents(theBoard, cEventsPerPoint); //*(1+cTriggerMultiplicity) );
            const std::vector<Event*>& cEvents = this->GetEvents(theBoard);
            LOG(INFO) << BOLDMAGENTA << "Read back " << +cEvents.size() << " events from board." << RESET;
            for(auto cEvent: cEvents)
            {
                auto cEventCount = cEvent->GetEventCount();
                LOG(DEBUG) << BOLDBLUE << "Event " << +cEventCount << RESET;
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cFe: *cOpticalGroup)
                    {
                        auto cFeId = cFe->getId();
                        auto cBxId = cEvent->BxId(cFe->getId());
                        LOG(DEBUG) << BOLDBLUE << "Link Id : " << +static_cast<OuterTrackerModule*>(cFe)->getLinkId() << " FE " << +cFeId << " - Bx Id " << +cBxId << RESET;
                        // cBxCounter->Fill( static_cast<float>(cEventCount) , cFeId , cBxId );
                        TH2D* cL1Status     = static_cast<TH2D*>(getHist(cFe, "L1Status"));
                        TH2D* cMatchedStubs = static_cast<TH2D*>(getHist(cFe, "MatchedStubs"));
                        TH2D* cMatchedHits  = static_cast<TH2D*>(getHist(cFe, "MatchedHits"));

                        for(auto cChip: *cFe)
                        {
                            auto cChipId = cChip->getId();
                            if(std::find(pChipIds.begin(), pChipIds.end(), cChipId) == pChipIds.end()) continue;

                            auto cErrorBit = cEvent->Error(cFeId, cChipId);
                            cL1Status->Fill(cEventCount, cChipId, cErrorBit);

                            uint32_t cL1Id = cEvent->L1Id(cFeId, cChipId);
                            LOG(DEBUG) << BOLDBLUE << "Chip" << +cChipId << " : L1 counter " << +cL1Id << " error bits " << +cErrorBit << RESET;
                            // hits
                            auto cHits = cEvent->GetHits(cFeId, cChipId);
                            for(auto cHit: cHits) { LOG(DEBUG) << BOLDMAGENTA << "\t... hit found in channel " << +cHit << " of readout chip" << +cChipId << RESET; }
                            std::vector<uint8_t> cExpectedHits = static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern(static_cast<ReadoutChip*>(cChip), pSeed, pBend);
                            size_t               cMatched      = 0;
                            for(auto cExpectedHit: cExpectedHits)
                            {
                                bool cMatchFound = std::find(cHits.begin(), cHits.end(), cExpectedHit) != cHits.end();
                                cMatched += cMatchFound;
                                cMatchedHits->Fill(cChipId, cMatchFound);
                            }
                            if(cMatched == cExpectedHits.size())
                            {
                                auto& cOcc = fHitCheckContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cFe->getIndex())->at(cChip->getIndex())->getSummary<int>();
                                cOcc += static_cast<int>(cMatched == cExpectedHits.size());
                            }
                            LOG(DEBUG) << BOLDMAGENTA << +cMatched << " matched hits found in readout chip" << +cChipId << " [ " << +cExpectedHits.size() << " expected.]" << RESET;
                            auto cStubs = cEvent->StubVector(cFeId, cChipId);
                            for(auto cStub: cStubs)
                            {
                                LOG(DEBUG) << BOLDMAGENTA << "\t... stub seed " << +cStub.getPosition() << " --- bend code of " << +cStub.getBend() << " expect seed " << +pSeed << " and bend code "
                                           << +cBendCode << RESET;
                                bool cMatchFound = (cStub.getPosition() == pSeed && cStub.getBend() == cBendCode);
                                cMatchedStubs->Fill(cChipId, cMatchFound);
                                auto& cOcc = fStubCheckContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cFe->getIndex())->at(cChip->getIndex())->getSummary<int>();
                                cOcc += static_cast<int>(cMatchFound);
                            }
                        }
                    }
                }
            }

            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto cChipId = cChip->getId();
                        if(std::find(pChipIds.begin(), pChipIds.end(), cChipId) == pChipIds.end()) continue;

                        auto cHitCheck  = fHitCheckContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<int>();
                        auto cStubCheck = fStubCheckContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<int>();
                        LOG(INFO) << BOLDBLUE << "Found " << +cHitCheck << " matched hits and " << +cStubCheck << " matched stubs in readout chip" << +cChipId << RESET;
                    }
                }
            }
        }
    }

    // unmask all channels and reset offsets
    // also re-configure thresholds + hit/stub detect logic to original values
    // and re-load configuration of fast command block from register map loaded from xml file
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cHybrid)->getLinkId());
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(theChip, false);
                    // set offsets back to default value
                    fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset12", (0 << 4) | (0 << 0));
                    fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset34", (0 << 4) | (0 << 0));
                    static_cast<CbcInterface*>(fReadoutChipInterface)
                        ->WriteChipReg(theChip, "VCth", fThresholds.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>());
                    static_cast<CbcInterface*>(fReadoutChipInterface)
                        ->WriteChipReg(
                            theChip, "Pipe&StubInpSel&Ptwidth", fLogic.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>());
                    static_cast<CbcInterface*>(fReadoutChipInterface)
                        ->WriteChipReg(theChip, "HIP&TestMode", fHIPs.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>());
                }
            }
        }
    }
}
void ExtraChecks::L1Eye()
{
    uint8_t cChipId = 2;
    for(uint8_t cPhase = 0; cPhase < 15; cPhase += 1)
    {
        LOG(INFO) << BOLDBLUE << "Setting optimal phase tap in CIC to " << +cPhase << RESET;
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerModule*>(cHybrid)->fCic;
                    fCicInterface->SetStaticPhaseAlignment(cCic, cChipId, 0, cPhase);
                }
            }
        }
        for(size_t cAttempt = 0; cAttempt < 10; cAttempt++)
        {
            // zero container
            zeroContainers();
            DataCheck({cChipId}, 10, 10, 0, false);
        }
    }
}
void ExtraChecks::StubCheck(uint8_t pChipId, bool pUseNoise, uint8_t pTestPulseAmplitude, int pTPgroup, int pAttempts)
{
    uint8_t  cFirmwareTPdelay      = 100;
    uint8_t  cFirmwareTriggerDelay = 200;
    uint32_t cEventsPerAttempt     = 100;
    uint16_t cDefaultStubLatency   = 50;
    int      cBend                 = 0;
    uint8_t  cBendCode             = 0x0b;
    // seeds and bends needed to generate fixed pattern on SLVS lines carrying stub information from CBCs --> CICs
    std::vector<uint8_t> cSeeds{static_cast<uint8_t>((pTPgroup * 2 + 16 * 0 + 1) * 2), static_cast<uint8_t>((pTPgroup * 2 + 16 * 2 + 1) * 2), static_cast<uint8_t>((pTPgroup * 2 + 16 * 4 + 1) * 2)};
    std::vector<int>     cBends(3, cBend);
    SetStubWindowOffsets(cBendCode, cBend);
    // inject stubs in all FE chips
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip); // switch off HitOr
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(static_cast<ReadoutChip*>(theChip), "HitOr", 0);
                    // enable stub logic
                    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(static_cast<ReadoutChip*>(theChip), "Sampled", true, true);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(static_cast<ReadoutChip*>(theChip), false, false, 0);
                }
            }
        }
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    }
    if(!pUseNoise)
    {
        fAllChan                     = true;
        fMaskChannelsFromOtherGroups = !this->fAllChan;
        this->enableTestPulse(!pUseNoise);
        this->SetTestPulse(!pUseNoise);
        setSameGlobalDac("TestPulsePotNodeSel", pTestPulseAmplitude);
    }
    // generate stubs in exactly one chip
    uint8_t  cChipId    = pChipId;
    size_t   cSeedIndex = 0;
    uint16_t cThreshold = 550;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    if(cChip->getId() == cChipId)
                    {
                        static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theChip, {cSeeds[cSeedIndex]}, {cBends[cSeedIndex]}, pUseNoise);
                        if(!pUseNoise)
                        {
                            ThresholdVisitor cThresholdVisitor(fReadoutChipInterface, cThreshold);
                            static_cast<ReadoutChip*>(cChip)->accept(cThresholdVisitor);
                        }
                    }
                    else
                        static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(theChip, true);
                }
            }
        }
    }
    if(pUseNoise)
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(0, 50, 3, 0, cDefaultStubLatency);
    else
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(cFirmwareTPdelay, cFirmwareTriggerDelay, 1000);
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Bx0Alignment();
    // check that the hits are there... so find test pulse
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        // fBeBoardInterface->ChipReSync ( cBoard );
        uint16_t cMinValue = 0;
        uint16_t cDelay    = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
        uint16_t cStart    = std::max(cMinValue, static_cast<uint16_t>(cDelay - 3));
        uint8_t  cNbits    = static_cast<ReadoutChip*>(cBoard->at(0)->at(0)->at(0))->getNumberOfBits("TriggerLatency");
        uint16_t cMaxValue = std::min(static_cast<uint16_t>(cDelay + 1), static_cast<uint16_t>(std::pow(2, cNbits) - 1));
        uint16_t cStep     = 1;
        if(pUseNoise) cMaxValue = cStart + cStep;
        std::vector<uint16_t> cListOfValues(std::floor((cMaxValue - cStart) / cStep));
        uint16_t              cValue = cStart - cStep;
        std::generate(cListOfValues.begin(), cListOfValues.end(), [&]() { return cValue += cStep; });
        for(uint16_t cStubLatency = 137; cStubLatency < 138; cStubLatency++)
        {
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
            for(auto cTriggerLatency: cListOfValues)
            {
                this->setSameDacBeBoard(theBoard, "TriggerLatency", cTriggerLatency);
                for(uint16_t cTPdelay = 0; cTPdelay < 25; cTPdelay += 3)
                {
                    this->setSameDacBeBoard(theBoard, "TestPulseDelay", cTPdelay);
                    float cTime = 25 * (cFirmwareTriggerDelay - cTriggerLatency) - cTPdelay;
                    // 25*(cFirmwareTriggerDelay - 1 - cTriggerLatency) - cTPdelay;
                    float cTimeStubs = 25 * (cStubLatency - cFirmwareTPdelay) - cTPdelay; // there was a -1 here
                    LOG(INFO) << BOLDBLUE << "Stub latency set to " << +cStubLatency << " trigger latency set to " << +cTriggerLatency << " test pulse delay set to " << +cTPdelay
                              << " : time from test pulse is " << +cTime << " , time from fast reset is " << cTimeStubs << " ]." << RESET;
                    cStart    = (pUseNoise) ? 750 : 400;
                    cMaxValue = (pUseNoise) ? 751 : 650;
                    cStep     = 1;
                    std::vector<float> cListOfThresholds(std::floor((cMaxValue - cStart) / cStep));
                    cValue = cStart - cStep;
                    std::generate(cListOfThresholds.begin(), cListOfThresholds.end(), [&]() { return cValue += cStep; });
                    // prepare container to store result of trigger latency  scan
                    std::vector<DetectorDataContainer*> cContainerVector(0);
                    for(auto cVcth: cListOfThresholds)
                    {
                        // push new container into vector
                        cContainerVector.emplace_back(new DetectorDataContainer());
                        ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *cContainerVector.back());
                        // fDetectorDataContainer = cContainerVector.at( cContainerVector.size()-1);
                        // set DAC .. read events
                        this->setSameDacBeBoard(theBoard, "VCth", cVcth);
                        this->ReadNEvents(theBoard, cEventsPerAttempt);
                        // const std::vector<Event*>& cEvents = this->GetEvents ( theBoard );
                        /*for (auto& cFe : cBoard->fModuleVector)
                        {
                            for (auto& cChip : cFe->fReadoutChipVector)
                            {
                                if( cChip->getId() != cChipId )
                                    continue;
                                int cNstubs=0;
                                TH2D* cMatchedStubs  = static_cast<TH2D*> ( getHist ( static_cast<ReadoutChip*>(cChip),
                        "MatchedStubs" ) ); TH2D* cFoundStubs  = static_cast<TH2D*> ( getHist (
                        static_cast<ReadoutChip*>(cChip), "Stubs" ) );
                                //
                                TH2D* cMatchedHits  = static_cast<TH2D*> ( getHist ( static_cast<ReadoutChip*>(cChip),
                        "MatchedHits" ) ); TH2D* cFoundHits  = static_cast<TH2D*> ( getHist (
                        static_cast<ReadoutChip*>(cChip), "Hits" ) ); int cNhits=0; for( auto cEvent : cEvents )
                                {
                                    //debug information
                                    auto cEventCount = cEvent->GetEventCount();
                                    uint32_t cL1Id = static_cast<D19cCicEvent*>(cEvent)->L1Id( cFe->getId(),
                        cChip->getId() ); uint32_t cPipeline = cEvent->PipelineAddress( cFe->getId(), cChip->getId() );
                                    //hits
                                    auto cHits = cEvent->GetHits( cFe->getId(), cChip->getId() ) ;
                                    cFoundHits->Fill( cTime , cVcth, cHits.size() );
                                    cNhits += cHits.size();
                                    std::vector<uint8_t> cExpectedHits =
                        static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern( cChip,
                        cSeeds[cSeedIndex], cBends[cSeedIndex] ); for( auto cExpectedHit : cExpectedHits )
                                    {
                                        bool cHitFound = std::find(  cHits.begin(), cHits.end(), cExpectedHit) !=
                        cHits.end() ; cContainerVector.at(
                        cContainerVector.size()-1)->at(cBoard->getIndex())->at(cFe->getIndex())->at(cChip->getIndex())->getChannelContainer<Occupancy>()->at(cExpectedHit).fOccupancy
                        += static_cast<int>(cHitFound) ; if( cHitFound ) cMatchedHits->Fill( cTime , cVcth, 1 );
                                    }
                                    auto cStubs = cEvent->StubVector( cFe->getId(), cChip->getId() );
                                    cFoundStubs->Fill( cTimeStubs , cVcth , cStubs.size() );
                                    for( auto cStub: cStubs )
                                    {
                                        if( cStub.getPosition() == cSeeds[cSeedIndex] && cStub.getBend() == cBendCode )
                                            cMatchedStubs->Fill( cTimeStubs , cVcth , 1);
                                    }
                                    cNstubs += cEvent->StubVector( cFe->getId(), cChip->getId() ).size() ;
                                }
                                if( static_cast<int>(cVcth)%10 == 0)
                                    LOG (INFO) << BOLDBLUE << "Vcth = " << cVcth << "... FE" << +cFe->getId() << " Chip"
                        << +cChip->getId() << " --- " << cNstubs << " stubs -- " << cNhits << " hits." << RESET;
                            }
                        }*/
                    }
                    /*for (auto& cFe : cBoard->fModuleVector)
                    {
                        for (auto& cChip : cFe->fReadoutChipVector)
                        {
                            if( cChip->getId() != cChipId )
                                continue;
                            std::vector<uint8_t> cExpectedHits =
                    static_cast<CbcInterface*>(fReadoutChipInterface)->stubInjectionPattern( cChip, cSeeds[cSeedIndex],
                    cBends[cSeedIndex] ); for(auto cChannel : cExpectedHits)
                            {
                                std::vector<float> cTmp(cListOfThresholds.size(), 0);
                                size_t cIter=0;
                                for(auto& cDetectorContainer : cContainerVector )
                                {
                                    cTmp[cIter] =
                    cDetectorContainer->at(cBoard->getIndex())->at(cFe->getIndex())->at(cChip->getIndex())->getChannelContainer<Occupancy>()->at(cChannel).fOccupancy;
                                    cDetectorContainer->at(cBoard->getIndex())->at(cFe->getIndex())->at(cChip->getIndex())->getChannelContainer<Occupancy>()->at(cChannel).fOccupancy
                    = 0; cIter++;
                                }
                                std::pair<float,float> cNoiseEval = evalNoise( cTmp, cListOfThresholds);
                                TH2D* cHist = static_cast<TH2D*> ( getHist ( static_cast<ReadoutChip*>(cChip),
                    "ScurvesTiming" ) ); cHist->SetBinContent( cHist->FindBin( cTime , cChannel) , cNoiseEval.first);
                                cHist->SetBinError( cHist->FindBin(cTime , cChannel), cNoiseEval.second );
                                LOG (INFO) << BOLDBLUE << "Time : " << cTime << " -- For channel " << +cChannel << "
                    found a pedestal of " << cNoiseEval.first << " and a noise of " << cNoiseEval.second << RESET;
                                cTmp.clear();
                            }
                        }
                    }*/
                    cContainerVector.clear();
                }
            }
        }
    }
    // remember to turn of TP and un-mask!!!
    if(!pUseNoise)
    {
        this->enableTestPulse(pUseNoise);
        this->SetTestPulse(pUseNoise);
        setSameGlobalDac("TestPulsePotNodeSel", pTestPulseAmplitude);
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(0, 50, 3, 0, cDefaultStubLatency);
    }
    // unmask all channels and reset offsets
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->MaskAllChannels(theChip, false);
                    // set offsets back to default value
                    fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset12", (0 << 4) | (0 << 0));
                    fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset34", (0 << 4) | (0 << 0));
                }
            }
        }
        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    }
}
void ExtraChecks::Stop()
{
    this->SaveResults();
    fResultFile->Flush();
    dumpConfigFiles();

    SaveResults();
    CloseResultFile();
    Destroy();
}
void ExtraChecks::FindShorts(uint16_t pThreshold, uint16_t pTPamplitude)
{
    // prepare container
    ContainerFactory::copyAndInitChannel<int>(*fDetectorContainer, fShortsContainer);
    uint8_t  cTPdelay              = 0;
    auto     cSetting              = fSettingsMap.find("Nevents");
    uint32_t cEventsPerAttempt     = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    uint8_t  cFirmwareTPdelay      = 100;
    uint8_t  cFirmwareTriggerDelay = 200;

    // configure FE chips so that stubs are detected [i.e. make sure HIP
    // suppression is off ]
    bool cOptical = false;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        cOptical          = cOptical || theBoard->ifOptical();
        for(auto cOpticalGroup: *cBoard)
            for(auto cFe: *cOpticalGroup)
            {
                if(theBoard->ifOptical()) static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(static_cast<OuterTrackerModule*>(cFe)->getLinkId());
                // configure CBCs
                for(auto cChip: *cFe)
                {
                    // switch off HitOr
                    static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(cChip, "HitOr", 0);
                    // enable stub logic
                    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(cChip, "Sampled", true, true);
                    static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(cChip, false, false, 0);
                }
            }
        fBeBoardInterface->ChipReSync(theBoard);
    }

    // set-up for TP
    fAllChan                     = true;
    fMaskChannelsFromOtherGroups = !this->fAllChan;
    this->enableTestPulse(true);
    this->SetTestPulse(true);
    setSameGlobalDac("TestPulsePotNodeSel", 0xFF - pTPamplitude);
    LOG(INFO) << BOLDBLUE << "Starting short finding loop." << RESET;
    // configure test pulse trigger
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(cFirmwareTPdelay, cFirmwareTriggerDelay, 1000);
    if(cOptical) static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Bx0Alignment();
    // check that the hits are there... so find test pulse
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        // first, set VCth to the target value for each CBC
        this->setSameDacBeBoard(theBoard, "VCth", pThreshold);
        auto&    cThisShortsContainer = fShortsContainer.at(cBoard->getIndex());
        uint16_t cDelay               = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
        this->setSameDacBeBoard(theBoard, "TriggerLatency", cDelay - 1);
        this->setSameDacBeBoard(theBoard, "TestPulseDelay", cTPdelay);
        uint8_t cTestGroup = 0;
        for(auto cGroup: *fChannelGroupHandler)
        {
            setSameGlobalDac("TestPulseGroup", cTestGroup);
            // bitset for this group
            std::bitset<NCHANNELS> cBitset = std::bitset<NCHANNELS>(static_cast<const ChannelGroup<NCHANNELS>*>(cGroup)->getBitset());
            LOG(INFO) << "Injecting charge into front-end object using test capacitor " << +cTestGroup << " : L1A latency set to " << +cDelay << RESET;
            this->ReadNEvents(theBoard, cEventsPerAttempt);
            const std::vector<Event*>& cEvents = this->GetEvents(theBoard);
            for(auto cOpticalGroup: *cBoard)
            {
                auto cOpticalGroupShorts = cThisShortsContainer->at(cOpticalGroup->getIndex());
                for(auto cFe: *cOpticalGroup)
                {
                    auto& cHybridShorts = cOpticalGroupShorts->at(cFe->getIndex());
                    for(auto cChip: *cFe)
                    {
                        auto cReadoutChipShorts = cHybridShorts->at(cChip->getIndex());
                        int  cNhits             = 0;
                        for(auto cEvent: cEvents)
                        {
                            // debug information
                            auto cEventCount = cEvent->GetEventCount();
                            // hits
                            auto cHits = cEvent->GetHits(cFe->getId(), cChip->getId());
                            LOG(INFO) << BOLDBLUE << "\t\tGroup " << +cTestGroup << " FE" << +cFe->getId() << " .. CBC" << +cChip->getId() << "...Event " << +cEventCount << " FE" << +cFe->getId()
                                      << " - " << +cHits.size() << " hits found/" << +cBitset.count() << " channels in test group" << RESET;
                            for(auto cHit: cHits)
                            {
                                if(cBitset[cHit] == 0) { cReadoutChipShorts->getChannelContainer<int>()->at(cHit) += 1; }
                            }
                            cNhits += cHits.size();
                        }
                        // get list of channels with hits; remember - I've only added a hit if the channel is not in
                        // this test group
                        auto  cShorts  = cReadoutChipShorts->getChannelContainer<int>();
                        float cNshorts = cShorts->size() - std::count(cShorts->begin(), cShorts->end(), 0); //
                        LOG(INFO) << BOLDBLUE << "\t\t\t FE" << +cFe->getId() << " CBC" << +cChip->getId() << " : number of shorts is  " << cNshorts << RESET;
                    }
                }
            }
            cTestGroup++;
        }
    }

    // disable TP
    this->enableTestPulse(false);
    this->SetTestPulse(false);
    setSameGlobalDac("TestPulsePotNodeSel", 0x00);
}
void ExtraChecks::Pause() {}

void ExtraChecks::Resume() {}

void ExtraChecks::SetStubWindowOffsets(uint8_t pBendCode, int pBend)
{
    // select AMUX output on one of the CBCs
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                    // read bend LUT
                    std::vector<uint8_t> cBendLUT  = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theChip);
                    auto                 cIterator = std::find(cBendLUT.begin(), cBendLUT.end(), pBendCode);
                    if(cIterator != cBendLUT.end())
                    {
                        int     cPosition    = std::distance(cBendLUT.begin(), cIterator);
                        double  cBend_strips = -7. + 0.5 * cPosition;
                        uint8_t cOffsetCode  = static_cast<uint8_t>(std::abs(cBend_strips * 2)) | (std::signbit(-1 * cBend_strips) << 3);
                        // set offsets
                        fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset12", (cOffsetCode << 4) | (cOffsetCode << 0));
                        fReadoutChipInterface->WriteChipReg(theChip, "CoincWind&Offset34", (cOffsetCode << 4) | (cOffsetCode << 0));
                        LOG(DEBUG) << BOLDBLUE << "Bend code of " << std::bitset<4>(pBendCode) << " found for bend reg " << +cPosition << " which means " << cBend_strips << " strips [offset code "
                                   << std::bitset<4>(cOffsetCode) << "]." << RESET;
                    }
                }
            }
        }
    }
}
// void ExtraChecks::ScanVcthDAC(uint16_t pStart , uint16_t pStop , uint16_t pStep  )
// {
//     std::vector<uint16_t> cListOfThresholds( std::floor((pStop - pStart)/pStep) );
//     uint16_t cValue=pStart-pStep;
//     std::generate(cListOfThresholds.begin(), cListOfThresholds.end(), [&](){ return cValue+=pStep; });
//      // prepare scan
//     std::vector<DetectorDataContainer*> cContainerVector(0);
//     cContainerVector.reserve(cListOfThresholds.size());
//     std::string cDacName = "VCth";
//     for( size_t cIndex=0; cIndex< cListOfThresholds.size(); cIndex++)
//     {
//         cContainerVector.emplace_back(new DetectorDataContainer());
//         ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *cContainerVector.back() );
//         //do scan
//         for (auto cBoard : this->fBoardVector)
//         {
//             this->setSameDacBeBoard(cBoard , cDacName, cListOfThresholds.at(cIndex));
//             fDetectorDataContainer = cContainerVector.at( cContainerVector.size()-1);
//             for (auto& cFe : cBoard->fModuleVector)
//             {
//                 if(cIndex%10 == 0 )
//                     LOG (INFO) << BOLDBLUE << "FE" << +cFe->getId() << " -- Vcth on all CBCs set to " <<
//                     cListOfThresholds.at(cIndex) << " DAC units." << RESET;
//                 TProfile2D* cScan = static_cast<TProfile2D*> ( getHist ( cFe , "VcthScan" ) );
//                 for (auto& cChip : cFe->fReadoutChipVector)
//                 {
//                     for(size_t cIter=0; cIter < 5; cIter++)
//                     {
//                         std::pair<uint16_t,float> cReading = this->ReadAmux(cChip->getId() , "VCth");
//                         cScan->Fill( cChip->getId(), cListOfThresholds.at(cIndex), cReading.second );
//                     }
//                 }
//             }
//         }
//     }
// }
std::pair<uint16_t, float> ExtraChecks::ReadAmux(uint8_t pFeId, uint8_t pChipId, std::string pValueToRead, bool pOptical)
{
    // select AMUX output on one of the CBCs
    // for (auto cBoard : *fDetectorContainer)
    // {
    //     for(auto cOpticalGroup : *cBoard)
    //     {
    //         for ( auto cHybrid : *cOpticalGroup )
    //         {
    //             for ( auto cChip : *cHybrid )
    //             {
    //                 // select AMUX output
    //                 //this->fReadoutChipInterface->WriteChipReg( static_cast<ReadoutChip*>(cChip),  "AmuxOutput",
    //                 ((cChip->getId() != pChipId ) ? fAmuxMap["Floating"]: fAmuxMap[pValueToRead]) );
    //                 // now read voltage from the right source .
    //             }
    //         }
    //     }
    // }
    // read voltage using ADC [ either ADC on GBT-SCA .. or on plug-in card]
    std::pair<uint16_t, float> cReading;
    if(pOptical) { cReading = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->readADC(((pFeId == 0) ? "AMUX_R" : "AMUX_L"), false); }
    else
    {
        // To Add : plug-in card/UIB
    }
    return cReading;
}

#endif
