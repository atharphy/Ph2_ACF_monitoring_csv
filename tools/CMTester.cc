
#include "CMTester.h"
#ifdef __USE_ROOT__

// This has no bad-strip masking and does not take a reduced number of active strips into account yet!

// PUBLIC METHODS
CMTester::CMTester() : Tool() {}

CMTester::~CMTester() {}

void CMTester::Initialize()
{
    // gStyle->SetOptStat( 000000 );
    // gStyle->SetTitleOffset( 1.3, "Y" );
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                uint32_t cHybridId = cHybrid->getId();

                for(auto cCbc: *cHybrid)
                {
                    uint32_t cCbcId = cCbc->getId();

                    // Fill Canvas Map
                    TCanvas* ctmpCanvas = new TCanvas(Form("c_online_canvas_fe%d_cbc%d", cHybridId, cCbcId), Form("FE%d CBC%d Online Canvas", cHybridId, cCbcId), 800, 800);
                    ctmpCanvas->Divide(2, 2);
                    fCanvasMap[cCbc] = ctmpCanvas;

                    // here create an empty std::set<int> for noisy strips
                    std::set<int> cTmpSet;
                    fNoiseStripMap[cCbc] = cTmpSet;

                    // here create the CBC-wise histos

                    // histogram for the number of hits
                    TString  cName = Form("h_nhits_Fe%dCbc%d", cHybridId, cCbcId);
                    TObject* cObj  = gROOT->FindObject(cName);

                    if(cObj) delete cObj;

                    TH1F* cHist = new TH1F(cName, Form("Number of Hits FE%d CBC%d; Hits; Count", cHybridId, cCbcId), NCHANNELS + 1, -.5, NCHANNELS + 0.5);
                    cHist->SetLineColor(9);
                    cHist->SetLineWidth(2);
                    bookHistogram(cCbc, "nhits", cHist);

                    // 2D profile for the combined odccupancy
                    cName = Form("p_combinedoccupancy_Fe%dCbc%d", cHybridId, cCbcId);
                    cObj  = (TProfile2D*)gROOT->FindObject(cName);

                    if(cObj) delete cObj;

                    //  no clue why i can not call it cName but when I do, it produces a segfault!!
                    TProfile2D* c2DOccProfile = new TProfile2D(
                        cName, Form("Combined Occupancy FE%d CBC%d; Strip; Strip; Occupancy", cHybridId, cCbcId), NCHANNELS + 1, -0.5, NCHANNELS + 0.5, NCHANNELS + 1, -0.5, NCHANNELS + 0.5);

                    c2DOccProfile->SetMarkerColor(1);
                    bookHistogram(cCbc, "combinedoccupancy", c2DOccProfile);

                    // 2D Profile for correlation coefficient
                    cName = Form("p_correlation_Fe%dCbc%d", cHybridId, cCbcId);
                    cObj  = gROOT->FindObject(cName);

                    if(cObj) delete cObj;

                    TH2F* c2DHist = new TH2F(
                        cName, Form("Correlation FE%d CBC%d; Strip; Strip; Correlation coefficient", cHybridId, cCbcId), NCHANNELS + 1, -.5, NCHANNELS + 0.5, NCHANNELS + 1, -.5, NCHANNELS + 0.5);
                    bookHistogram(cCbc, "correlation", c2DHist);

                    // 1D projection of the combined odccupancy
                    cName = Form("p_occupancyprojection_Fe%dCbc%d", cHybridId, cCbcId);
                    cObj  = gROOT->FindObject(cName);

                    if(cObj) delete cObj;

                    TProfile* cProfile = new TProfile(cName, Form("Projection of combined Occupancy FE%d CBC%d;  NNeighbors; Probability", cHybridId, cCbcId), NCHANNELS + 1, -.5, NCHANNELS + 0.5);
                    cProfile->SetLineColor(9);
                    cProfile->SetLineWidth(2);
                    bookHistogram(cCbc, "occupancyprojection", cProfile);

                    // 1D projection of the combined occupancy, but nearest neighbor calculated on both sides
                    cName = Form("p_occupancyprojectionsymmetric_Fe%dCbc%d", cHybridId, cCbcId);
                    cObj  = gROOT->FindObject(cName);

                    if(cObj) delete cObj;

                    cProfile =
                        new TProfile(cName, Form("Projection of combined Occupancy (+ and -) FE%d CBC%d;  NNeighbors (+-N); Probability", cHybridId, cCbcId), NCHANNELS + 1, -.5, NCHANNELS + 0.5);
                    cProfile->SetLineColor(9);
                    cProfile->SetLineWidth(2);
                    bookHistogram(cCbc, "occupancyprojectionplusminus", cProfile);

                    // 1D projection of the uncorrelated odccupancy
                    cName = Form("p_uncorr_occupancyprojection_Fe%dCbc%d", cHybridId, cCbcId);
                    cObj  = gROOT->FindObject(cName);

                    if(cObj) delete cObj;

                    cProfile = new TProfile(cName, Form("Projection of uncorrelated Occupancy FE%d CBC%d;  NNeighbors; Probability", cHybridId, cCbcId), NCHANNELS + 1, -.5, NCHANNELS + 0.5);
                    cProfile->SetLineColor(2);
                    cProfile->SetLineWidth(2);
                    bookHistogram(cCbc, "uncorr_occupancyprojection", cProfile);

                    // 1D projection of the correlation
                    cName = Form("p_correlationprojection_Fe%dCbc%d", cHybridId, cCbcId);
                    cObj  = gROOT->FindObject(cName);

                    if(cObj) delete cObj;

                    cProfile = new TProfile(cName, Form("Projection of Correlation FE%d CBC%d;  NNeighbors; Correlation", cHybridId, cCbcId), NCHANNELS + 1, -.5, NCHANNELS + 0.5);
                    cProfile->SetLineColor(9);
                    cProfile->SetLineWidth(2);
                    bookHistogram(cCbc, "correlationprojection", cProfile);

                    // 1D hit probability profile
                    cName = Form("p_hitprob_Fe%dCbc%d", cHybridId, cCbcId);
                    cObj  = gROOT->FindObject(cName);

                    if(cObj) delete cObj;

                    cProfile = new TProfile(cName, Form("Hit Probability FE%d CBC%d;  Strip; Probability", cHybridId, cCbcId), NCHANNELS + 1, -.5, NCHANNELS + 0.5);
                    cProfile->SetLineColor(9);
                    cProfile->SetLineWidth(2);
                    bookHistogram(cCbc, "hitprob", cProfile);

                    // dummy TF1* for fit & dummy TH1F* for 0CM
                    cName = Form("f_nhitsfit_Fe%dCbc%d", cHybridId, cCbcId);
                    cObj  = gROOT->FindObject(cName);

                    if(cObj) delete cObj;

                    TF1* cCmFit = new TF1(cName, hitProbFunction, 0, 255, 4);
                    bookHistogram(cCbc, "nhitsfit", cCmFit);

                    cName = Form("h_nocm_Fe%dCbc%d", cHybridId, cCbcId);
                    cObj  = gROOT->FindObject(cName);

                    if(cObj) delete cObj;

                    TH1F* cNoCM = new TH1F(cName, "Noise hit distributtion", NCHANNELS + 1, -0.5, NCHANNELS + 0.5);
                    cNoCM->SetLineColor(16);
                    bookHistogram(cCbc, "nocm", cNoCM);
                }

                // PER MODULE PLOTS
                uint32_t cNCbc = cHybrid->size();

                // 2D profile for the combined odccupancy
                TString  cName = Form("p_module_combinedoccupancy_Fe%d", cHybridId);
                TObject* cObj  = gROOT->FindObject(cName);

                if(cObj) delete cObj;

                TProfile2D* c2DProfile = new TProfile2D(cName,
                                                        Form("Combined Occupancy FE%d; Strip; Strip; Occupancy", cHybridId),
                                                        cNCbc * NCHANNELS + 1,
                                                        -.5,
                                                        cNCbc * NCHANNELS + 0.5,
                                                        cNCbc * NCHANNELS + 1,
                                                        -.5,
                                                        cNCbc * NCHANNELS + 0.5);
                bookHistogram(cHybrid, "module_combinedoccupancy", c2DProfile);

                // 2D Hist for correlation coefficient
                cName = Form("p_module_correlation_Fe%d", cHybridId);
                cObj  = gROOT->FindObject(cName);

                if(cObj) delete cObj;

                TH2F* c2DHist = new TH2F(cName,
                                         Form("Correlation FE%d; Strip; Strip; Correlation coefficient", cHybridId),
                                         cNCbc * NCHANNELS + 1,
                                         -.5,
                                         cNCbc * NCHANNELS + 0.5,
                                         cNCbc * NCHANNELS + 1,
                                         -.5,
                                         cNCbc * NCHANNELS + 0.5);
                bookHistogram(cHybrid, "module_correlation", c2DHist);

                // 1D projection of the combined odccupancy
                cName = Form("p_module_occupancyprojection_Fe%d", cHybridId);
                cObj  = gROOT->FindObject(cName);

                if(cObj) delete cObj;

                TProfile* cProfile = new TProfile(cName, Form("Projection of combined Occupancy FE%d;  NNeighbors; Probability", cHybridId), cNCbc * NCHANNELS + 1, -.5, cNCbc * NCHANNELS + 0.5);
                cProfile->SetLineColor(9);
                cProfile->SetLineWidth(2);
                bookHistogram(cHybrid, "module_occupancyprojection", cProfile);

                // 1D projection of the uncorrelated occupancy
                cName = Form("p_module_uncorr_occupancyprojection_Fe%d", cHybridId);
                cObj  = gROOT->FindObject(cName);

                if(cObj) delete cObj;

                cProfile = new TProfile(cName, Form("Projection of uncorrelated Occupancy FE%d;  NNeighbors; Probability", cHybridId), cNCbc * NCHANNELS + 1, -.5, cNCbc * NCHANNELS + 0.5);
                cProfile->SetLineColor(2);
                cProfile->SetLineWidth(2);
                bookHistogram(cHybrid, "module_uncorr_occupancyprojection", cProfile);

                // 1D projection of the correlation
                cName = Form("p_module_correlationprojection_Fe%d", cHybridId);
                cObj  = gROOT->FindObject(cName);

                if(cObj) delete cObj;

                cProfile = new TProfile(cName, Form("Projection of Correlation FE%d;  NNeighbors; Correlation", cHybridId), cNCbc * NCHANNELS + 1, -.5, cNCbc * NCHANNELS + 0.5);
                cProfile->SetLineColor(9);
                cProfile->SetLineWidth(2);
                bookHistogram(cHybrid, "module_correlationprojection", cProfile);
            }
        }
    }

    // initializeHists();

    LOG(INFO) << "Histograms and Settings initialised.";
}

void CMTester::ScanNoiseChannels()
{
    LOG(INFO) << "Scanning for noisy channels! ";
    uint32_t cTotalEvents = 500;

    for(auto pBoard: *fDetectorContainer)
    {
        uint32_t cN      = 1;
        uint32_t cNthAcq = 0;

        // fBeBoardInterface->Start ( pBoard );

        // while ( cN <=  cTotalEvents )
        //{
        BeBoard* theBoard = static_cast<BeBoard*>(pBoard);
        ReadNEvents(theBoard, cTotalEvents);
        const std::vector<Event*>& events = GetEvents(theBoard);

        // Loop over Events from this Acquisition
        for(auto& cEvent: events)
        {
            for(auto cOpticalReadout: *pBoard)
            {
                for(auto cHybrid: *cOpticalReadout)
                {
                    for(auto cCbc: *cHybrid)
                    {
                        // just re-use the hitprobability histogram here?
                        // this has to go into a dedicated method
                        TProfile* cNoiseStrips = dynamic_cast<TProfile*>(getHist(cCbc, "hitprob"));

                        const std::vector<bool>& list  = cEvent->DataBitVector(cHybrid->getId(), cCbc->getId());
                        int                      cChan = 0;

                        for(const auto& b: list)
                        {
                            int fillvalue = (b) ? 1 : 0;
                            cNoiseStrips->Fill(cChan++, fillvalue);
                        }
                    }
                }
            }

            if(cN % 100 == 0)
                // updateHists();
                LOG(INFO) << "Acquired " << cN << " Events for Noise Strip Scan!";

            cN++;
        }

        cNthAcq++;
        //} // End of Analyze Events of last Acquistion loop

        // fBeBoardInterface->Stop ( pBoard );
    }

    // done taking data, now iterate over p_noisestrips and find out the bad strips, push them into the fNoiseStripMap,
    // then clear the histogram
    for(const auto& cCbc: fChipHistMap)
    {
        TProfile* cNoiseStrips = dynamic_cast<TProfile*>(getHist(cCbc.first, "hitprob"));

        auto cNoiseSet = fNoiseStripMap.find(cCbc.first);

        if(cNoiseSet == std::end(fNoiseStripMap))
            LOG(ERROR) << " Error: Could not find noisy strip container for CBC " << int(cCbc.first->getId());
        else
        {
            double cMean = cNoiseStrips->GetMean(2);

            LOG(INFO) << "Found average Occupancy of " << cMean;

            for(int cBin = 0; cBin < cNoiseStrips->GetNbinsX(); cBin++)
            {
                double cStripOccupancy = cNoiseStrips->GetBinContent(cBin);

                if(fabs(cStripOccupancy - cMean) > cMean / 2)
                {
                    cNoiseSet->second.insert(cNoiseStrips->GetBinCenter(cBin));
                    LOG(INFO) << "Found noisy Strip on CBC " << int(cCbc.first->getId()) << " : " << cNoiseStrips->GetBinCenter(cBin);
                }
            }
        }

        cNoiseStrips->Reset();
    }
}

void CMTester::TakeData()
{
    std::stringstream outp;
    parseSettings();

    ThresholdVisitor cVisitor(fReadoutChipInterface);
    this->accept(cVisitor);
    fVcth = cVisitor.getThreshold();
    LOG(INFO) << "Checking threshold on latest CBC that was touched...: " << fVcth << std::endl;

    //    cVisitor.setOption ('w');
    //    cVisitor.setThreshold (595);
    //    cVcth = cVisitor.getThreshold();
    //    std::cout<<"Now my threshold is: "<<cVcth<<std::endl;

    // CbcRegReader cReader ( fReadoutChipInterface, "VCth" );
    // accept( cReader );

    for(auto pBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(pBoard);

        uint32_t cN      = 0;
        uint32_t cNthAcq = 0;

        fBeBoardInterface->Start(theBoard);
        // while ( cN <=  fNevents )
        //{
        // Run( theBoard, cNthAcq );
        // ReadData (theBoard);
        ReadNEvents(theBoard, fNevents);
        const std::vector<Event*>& events = GetEvents(theBoard);

        // Loop over Events from this Acquisition

        for(auto& cEvent: events)
        {
            // LOG (INFO) << cN << " "<< *cEvent;

            if(cN > fNevents) continue; // Needed when using ReadData on CBC3

            analyze(theBoard, cEvent);

            if(cN % 100 == 0)
            {
                LOG(INFO) << cN << " Events recorded!";
                updateHists();
            }

            cN++;
        }

        cNthAcq++;
        //} // End of Analyze Events of last Acquistion loop

        // fBeBoardInterface->Stop ( pBoard );
    }

    updateHists();
}

void CMTester::FinishRun()
{
    //  Iterate through maps, pick histogram that I need and the other one
    LOG(INFO) << "Fitting and computing aditional histograms ... ";
    // first CBCs
    LOG(INFO) << "per CBC ..";

    ThresholdVisitor cVisitor(fReadoutChipInterface); // No Vcth given, so default option is 'r'
    int              iCbc = 0;
    for(auto cCbc: fChipHistMap)
    {
        static_cast<ReadoutChip*>(cCbc.first)->accept(cVisitor);
        uint32_t cVcth = cVisitor.getThreshold();

        TH1F* cTmpNHits = dynamic_cast<TH1F*>(getHist(cCbc.first, "nhits"));
        TH1F* cNoCM     = dynamic_cast<TH1F*>(getHist(cCbc.first, "nocm"));
        TF1*  cNHitsFit = dynamic_cast<TF1*>(getHist(cCbc.first, "nhitsfit"));

        // here I need the number of active channels which i can get from the noise strip set
        auto     cNoiseStrips = fNoiseStripMap.find(cCbc.first);
        uint32_t cNactiveChan = (cNoiseStrips != std::end(fNoiseStripMap)) ? (NCHANNELS - cNoiseStrips->second.size()) : NCHANNELS;

        // Fit NHits and create 0 CM
        fitDistribution(cTmpNHits, cNHitsFit, cNactiveChan);
        createNoiseDistribution(cNoCM, cNHitsFit->GetParameter(0), 0, cNHitsFit->GetParameter(2), cNHitsFit->GetParameter(3));

        float CMnoiseFrac    = fabs(cNHitsFit->GetParameter(1));
        float CMnoiseFracErr = fabs(cNHitsFit->GetParError(1));
        if(fTotalNoise[iCbc] > 0)
            LOG(INFO) << BOLDRED << "Average noise on FE " << +static_cast<ReadoutChip*>(cCbc.first)->getFeId() << " CBC " << +cCbc.first->getId() << " : " << fTotalNoise[iCbc] << " . At Vcth "
                      << cVcth << " CM is " << CMnoiseFrac << "+/-" << CMnoiseFracErr << "%, so " << CMnoiseFrac * fTotalNoise[iCbc] << " VCth." << RESET;
        else
            LOG(INFO) << BOLDRED << "FE " << +static_cast<ReadoutChip*>(cCbc.first)->getFeId() << " CBC " << +cCbc.first->getId() << " . At Vcth " << cVcth << " CM is " << CMnoiseFrac << "+/-"
                      << CMnoiseFracErr << "%" << RESET;

        // now compute the correlation coefficient and the uncorrelated probability
        TProfile2D* cTmpOccProfile  = dynamic_cast<TProfile2D*>(getHist(cCbc.first, "combinedoccupancy"));
        TProfile*   cUncorrHitProb  = dynamic_cast<TProfile*>(getHist(cCbc.first, "uncorr_occupancyprojection"));
        TH2F*       cCorrelation2D  = dynamic_cast<TH2F*>(getHist(cCbc.first, "correlation"));
        TProfile*   cCorrProjection = dynamic_cast<TProfile*>(getHist(cCbc.first, "correlationprojection"));

        for(int cIdx = 0; cIdx < cTmpOccProfile->GetNbinsX(); cIdx++)
        {
            for(int cIdy = 0; cIdy < cTmpOccProfile->GetNbinsY(); cIdy++)
            {
                double xx = cTmpOccProfile->GetBinContent(cIdx, cIdx);
                double yy = cTmpOccProfile->GetBinContent(cIdy, cIdy);
                double xy = cTmpOccProfile->GetBinContent(cIdx, cIdy);

                // Fill the correlation & the uncorrelated probability
                // frac(Oxy-OxOy)(sqrt(Ox-Ox^2)*sqrt(Oy-Oy^2))
                cCorrelation2D->SetBinContent(cIdx, cIdy, (xy - xx * yy) / (sqrt(xx - pow(xx, 2)) * sqrt(yy - pow(yy, 2))));

                if(xx != 0 && yy != 0) cUncorrHitProb->Fill(cIdx - cIdy, xx * yy);

                // and finally project the correlation
                xy = cCorrelation2D->GetBinContent(cIdx, cIdy);

                if(xy == xy) cCorrProjection->Fill(cIdx - cIdy, xy);
            }
        }
        iCbc++;
    }

    LOG(INFO) << " done!";
    LOG(INFO) << "per module ... ";

    // now module wise
    for(auto& cFe: fModuleHistMap)
    {
        TString cName = Form("FE%d", cFe.first->getId());

        // get histograms
        TProfile2D* cTmpOccProfile  = dynamic_cast<TProfile2D*>(getHist(cFe.first, "module_combinedoccupancy"));
        TProfile*   cUncorrHitProb  = dynamic_cast<TProfile*>(getHist(cFe.first, "module_uncorr_occupancyprojection"));
        TH2F*       cCorrelation2D  = dynamic_cast<TH2F*>(getHist(cFe.first, "module_correlation"));
        TProfile*   cCorrProjection = dynamic_cast<TProfile*>(getHist(cFe.first, "module_correlationprojection"));

        for(int cIdx = 0; cIdx < cTmpOccProfile->GetNbinsX(); cIdx++)
        {
            for(int cIdy = 0; cIdy < cTmpOccProfile->GetNbinsY(); cIdy++)
            {
                double xx = cTmpOccProfile->GetBinContent(cIdx, cIdx);
                double yy = cTmpOccProfile->GetBinContent(cIdy, cIdy);
                double xy = cTmpOccProfile->GetBinContent(cIdx, cIdy);

                // Fill the correlation & the uncorrelated probability
                // frac(Oxy-OxOy)(sqrt(Ox-Ox^2)*sqrt(Oy-Oy^2))
                cCorrelation2D->SetBinContent(cIdx, cIdy, (xy - xx * yy) / (sqrt(xx - pow(xx, 2)) * sqrt(yy - pow(yy, 2))));

                if(xx != 0 && yy != 0) cUncorrHitProb->Fill(cIdx - cIdy, xx * yy);

                // and finally project the correlation
                xy = cCorrelation2D->GetBinContent(cIdx, cIdy);

                if(xy == xy) cCorrProjection->Fill(cIdx - cIdy, xy);
            }
        }
    }

    LOG(INFO) << " done!";
    // Not drawing anything yet
    updateHists(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

void CMTester::analyze(BeBoard* pBoard, const Event* pEvent)
{
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            std::vector<bool> cModuleData; // use this to store data for all CBCs....

            for(auto cCbc: *cHybrid)
            {
                // here loop over the channels and fill the histograms
                // dont forget to get them first
                TH1F*       cTmpNHits         = dynamic_cast<TH1F*>(getHist(cCbc, "nhits"));
                TProfile*   cTmpHitProb       = dynamic_cast<TProfile*>(getHist(cCbc, "hitprob"));
                TProfile2D* cTmpOccProfile    = dynamic_cast<TProfile2D*>(getHist(cCbc, "combinedoccupancy"));
                TProfile*   cTmpCombinedOcc   = dynamic_cast<TProfile*>(getHist(cCbc, "occupancyprojection"));
                TProfile*   cTmpCombinedOccPM = dynamic_cast<TProfile*>(getHist(cCbc, "occupancyprojectionplusminus"));

                int cNHits     = 0;
                int cEventHits = pEvent->GetNHits(cHybrid->getId(), cCbc->getId());

                if(cEventHits > 250) LOG(INFO) << " Found an event with " << cEventHits << " hits on a CBC! Is this expected?";

                // here add a check if the strip is masked and if I am simulating or not!
                std::vector<bool> cSimResult;

                if(fDoSimulate)
                {
                    for(int cChan = 0; cChan < 254; cChan++)
                    {
                        bool cResult = randHit(fSimOccupancy / float(100));
                        cSimResult.push_back(cResult);
                    }
                }

                for(int cChan = 0; cChan < NCHANNELS; cChan++)
                {
                    bool chit;

                    if(fDoSimulate)
                        chit = cSimResult.at(cChan);
                    else
                        chit = pEvent->DataBit(cHybrid->getId(), cCbc->getId(), cChan);

                    // move the CBC data in a vector that has data for the whole module
                    cModuleData.push_back(chit);

                    //  count hits/event
                    if(chit && !isMasked(static_cast<ReadoutChip*>(cCbc), cChan)) cNHits++;

                    // Fill Single Strip Efficiency
                    if(!isMasked(static_cast<ReadoutChip*>(cCbc), cChan)) cTmpHitProb->Fill(cChan, int(chit));

                    // For combined occupancy 1D projection & 2D profile
                    for(int cChan2 = 0; cChan2 < 254; cChan2++)
                    {
                        bool chit2;

                        if(fDoSimulate)
                            chit2 = cSimResult.at(cChan2);
                        else
                            chit2 = pEvent->DataBit(cHybrid->getId(), cCbc->getId(), cChan2);

                        int cfillValue = 0;

                        if(chit && chit2) cfillValue = 1;

                        if(!isMasked(static_cast<ReadoutChip*>(cCbc), cChan) && !isMasked(static_cast<ReadoutChip*>(cCbc), cChan2))
                        {
                            // Fill 2D occupancy
                            cTmpOccProfile->Fill(cChan, cChan2, cfillValue);

                            // Fill projection: this could be done in FinishRun() but then no live updates
                            if(cChan - cChan2 >= 0) cTmpCombinedOcc->Fill(cChan - cChan2, cfillValue);

                            // Cross-check: what if we also consider the -N neighbors, not just +N ones? Should get the
                            // same result...
                            cTmpCombinedOccPM->Fill(abs(cChan - cChan2), cfillValue);
                        }
                    }
                }

                // Fill NHits Histogram
                cTmpNHits->Fill(cNHits);
            }

            // Here deal with per-module Histograms
            TProfile2D* cTmpOccProfile  = dynamic_cast<TProfile2D*>(getHist(cHybrid, "module_combinedoccupancy"));
            TProfile*   cTmpCombinedOcc = dynamic_cast<TProfile*>(getHist(cHybrid, "module_occupancyprojection"));

            uint32_t cChanCt1 = 0;

            // since I use the module bool vector i constructed myself this already includes simulation results if
            // simulation flag is set!
            for(auto cChan1: cModuleData)
            {
                uint32_t cChanCt2 = 0;

                for(auto cChan2: cModuleData)
                {
                    int fillvalue = 0;

                    if(cChan1 && cChan2) fillvalue = 1;

                    if(!isMasked(cChanCt1) && !isMasked(cChanCt2))
                    {
                        cTmpOccProfile->Fill(cChanCt1, cChanCt2, fillvalue);
                        cTmpCombinedOcc->Fill(cChanCt1 - cChanCt2, fillvalue);
                    }

                    cChanCt2++;
                }

                cChanCt1++;
            }
        }
    }
}

void CMTester::updateHists(bool pFinal)
{
    // method to iterate over the histograms that I want to draw and update the canvases
    int iCbc = 0;
    for(auto& cCbc: fChipHistMap)
    {
        auto cCanvas = fCanvasMap.find(cCbc.first);

        if(cCanvas == fCanvasMap.end())
            LOG(INFO) << "Error: could not find the canvas for Chip " << int(cCbc.first->getId());
        else
        {
            TH1F*       cTmpNHits       = dynamic_cast<TH1F*>(getHist(cCbc.first, "nhits"));
            TProfile2D* cTmpOccProfile  = dynamic_cast<TProfile2D*>(getHist(cCbc.first, "combinedoccupancy"));
            TProfile*   cTmpCombinedOcc = dynamic_cast<TProfile*>(getHist(cCbc.first, "occupancyprojection"));
            TProfile*   cUncorrHitProb(nullptr);
            TH1F*       cNoCM(nullptr);
            TF1*        cCMFit(nullptr);
            TProfile*   cCorrProjection(nullptr);

            if(pFinal)
            {
                cUncorrHitProb  = dynamic_cast<TProfile*>(getHist(cCbc.first, "uncorr_occupancyprojection"));
                cNoCM           = dynamic_cast<TH1F*>(getHist(cCbc.first, "nocm"));
                cCMFit          = dynamic_cast<TF1*>(getHist(cCbc.first, "nhitsfit"));
                cCorrProjection = dynamic_cast<TProfile*>(getHist(cCbc.first, "correlationprojection"));
            }

            // Get the 4 things I want to draw and draw it!
            // 1. NHits
            cCanvas->second->cd(1);

            if(pFinal)
            {
                cNoCM->Draw();
                cTmpNHits->Draw("same");
                if(cCorrProjection != nullptr) cCMFit->Draw("same");
                TLegend* cLegend = new TLegend(0.13, 0.66, 0.38, 0.88, "");
                cLegend->SetBorderSize(0);
                cLegend->SetFillColor(kWhite);
                cLegend->AddEntry(cTmpNHits, "Data", "f");
                cLegend->AddEntry(cCMFit, Form("Fit (CM %4.2f+-%4.2f, THR %4.2f). ", fabs(cCMFit->GetParameter(1)), cCMFit->GetParError(1), cCMFit->GetParameter(0)), "l");
                if(cNoCM != nullptr) cLegend->AddEntry(cNoCM, "CM = 0", "l");
                if(fTotalNoise[iCbc] > 0) cLegend->AddEntry((TObject*)0, Form("Noise: %4.2f (total), %4.2f (CM)", fTotalNoise[iCbc], fabs(cCMFit->GetParameter(1)) * fTotalNoise[iCbc]), "");
                cLegend->SetTextSize(0.05);
                cLegend->Draw("same");
            }
            else
                cTmpNHits->Draw();

            // 2. 2D occupancy
            cCanvas->second->cd(2);
            cTmpOccProfile->Draw("colz");
            // 3. 1D combined occupancy
            cCanvas->second->cd(3);
            cTmpCombinedOcc->Draw();

            if(pFinal)
            {
                if(cUncorrHitProb != nullptr) cUncorrHitProb->Draw("hist same");
                TLegend* cLegend = new TLegend(0.13, 0.66, 0.38, 0.88, "");
                cLegend->SetBorderSize(0);
                cLegend->SetFillColor(kWhite);
                cLegend->AddEntry(cTmpCombinedOcc, "measured hit probability", "l");
                cLegend->AddEntry(cUncorrHitProb, "uncorrelated hit probability", "l");
                cLegend->SetTextSize(0.05);
                cLegend->Draw("same");

                // 4. Correlation projection
                cCanvas->second->cd(4);
                if(cCorrProjection != nullptr) cCorrProjection->Draw();
            }

            cCanvas->second->Update();
        }
        iCbc++;
    }

    this->HttpServerProcess();
}

bool CMTester::randHit(float pProbability)
{
    float val = float(rand()) / RAND_MAX;

    if(val < pProbability)
        return true;
    else
        return false;
}

bool CMTester::isMasked(ReadoutChip* pCbc, int pChannel)
{
    auto cNoiseStripSet = fNoiseStripMap.find(pCbc);

    if(cNoiseStripSet == std::end(fNoiseStripMap))
    {
        LOG(ERROR) << "Error: could not find the set of noisy strips for CBC " << int(cNoiseStripSet->first->getId());
        return false;
    }
    else
    {
        auto cNoiseStrip = cNoiseStripSet->second.find(pChannel);

        if(cNoiseStrip == std::end(cNoiseStripSet->second))
            return false;
        else
            return true;
    }
}

bool CMTester::isMasked(int pGlobalChannel)
{
    uint32_t cCbcId;

    if(pGlobalChannel < 254)
        cCbcId = 0;
    else if(pGlobalChannel > 253 && pGlobalChannel < 508)
        cCbcId = 1;
    else if(pGlobalChannel > 507 && pGlobalChannel < 762)
        cCbcId = 2;
    else if(pGlobalChannel > 761 && pGlobalChannel < 1016)
        cCbcId = 3;
    else if(pGlobalChannel > 1015 && pGlobalChannel < 1270)
        cCbcId = 4;
    else if(pGlobalChannel > 1269 && pGlobalChannel < 1524)
        cCbcId = 5;
    else if(pGlobalChannel > 1523 && pGlobalChannel < 1778)
        cCbcId = 6;
    else if(pGlobalChannel > 1777 && pGlobalChannel < 2032)
        cCbcId = 5;
    else
        return true;

    for(const auto& cNoiseStripSet: fNoiseStripMap)
    {
        if(int(cNoiseStripSet.first->getId()) == cCbcId)
        {
            auto cNoiseStrip = cNoiseStripSet.second.find(pGlobalChannel - cCbcId * 254);

            if(cNoiseStrip == std::end(cNoiseStripSet.second))
                return false;
            else
                return true;
        }
        else
            return false;
    }
    return false;
}

void CMTester::SetTotalNoise(std::vector<double> pTotalNoise)
{
    // Just used in plotting.
    fTotalNoise = pTotalNoise;
}

void CMTester::parseSettings()
{
    // now read the settings from the map
    auto cSetting = fSettingsMap.find("Nevents");

    if(cSetting != std::end(fSettingsMap))
        fNevents = 10 * cSetting->second;
    else
        fNevents = 2000;

    cSetting = fSettingsMap.find("doSimulate");

    if(cSetting != std::end(fSettingsMap))
        fDoSimulate = cSetting->second;
    else
        fDoSimulate = false;

    cSetting = fSettingsMap.find("SimOccupancy");

    if(cSetting != std::end(fSettingsMap))
        fSimOccupancy = cSetting->second;
    else
        fSimOccupancy = 50;

    LOG(INFO) << "Parsed the following settings:";
    LOG(INFO) << "	Nevents (.XML value times 10)= " << fNevents;
    LOG(INFO) << "	simulate = " << int(fDoSimulate);
    LOG(INFO) << "	sim. Occupancy (%) = " << int(fSimOccupancy);
}

#endif
