#include "CMTester.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"

// PUBLIC METHODS
CMTester::CMTester() : Tool() {}

CMTester::~CMTester() {}

void CMTester::Initialize()
{    
    parseSettings();
    
    fNevents = 6000; //TODO should come from settings file

#ifdef __USE_ROOT__
    fDQMHistogramOTCommonNoise.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif

    LOG(INFO) << "Histograms and Settings initialised.";
}

void CMTester::ScanNoiseChannels()
{
    /*
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
        const std::vector<Event*>& events = GetEvents();

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
    */
}

void CMTester::TakeData()
{
    ThresholdVisitor cVisitor(fReadoutChipInterface);
    this->accept(cVisitor);
    fVcth = cVisitor.getThreshold();
    LOG(INFO) << "Checking threshold on latest CBC that was touched...: " << fVcth << std::endl;

    DetectorDataContainer theHitContainer;
    //channel, chip, hybrid, optical group, board, detector
    //can have 0 or 255 hits, need NCHANNELS+1 (inclusive)
    ContainerFactory::copyAndInitStructure<EmptyContainer, GenericDataArray<(NCHANNELS+1), uint32_t>, GenericDataArray<((NCHANNELS+1)*NCHIPS_OT), uint32_t>, EmptyContainer, EmptyContainer, EmptyContainer>(*fDetectorContainer, theHitContainer);

    //LESYA TODO
    //currently missing the channel by channel data per event, need to think about how to implement this to make the 2D plot.

    for(auto cBoard: theHitContainer)
    {
        //BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        BeBoard* theBoard = static_cast<BeBoard*>(fDetectorContainer->at(cBoard->getIndex()));

        uint32_t cN      = 0;

        fBeBoardInterface->Start(theBoard);
        ReadNEvents(theBoard, fNevents);
        const std::vector<Event*>& events = GetEvents();

        for(auto& cEvent: events)
        {

            if(cN > fNevents) continue; // Needed when using ReadData on CBC3

            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    uint32_t cHybridHits = 0;
                    for(auto cCbc: *cHybrid)
                    {
                        uint32_t cEventHits = cEvent->GetNHits(cHybrid->getId(), cCbc->getId());

                        //basically filling the histogram, then we will set bin content later
                        cCbc->getSummary<GenericDataArray<(NCHANNELS+1), uint32_t>>()[cEventHits] += 1;
                        cHybridHits += cEventHits;

                    }
                    //save per hybrid
                   cHybrid->getSummary<GenericDataArray<((NCHANNELS+1)*NCHIPS_OT), uint32_t>>()[cHybridHits] = cHybridHits; 
                }
            }
            
            //print out event counter
            if(cN % 100 == 0)
            {
                LOG(INFO) << cN << " Events recorded!";
                // updateHists();
            }
            cN++;

        }
    }
#ifdef __USE_ROOT__
    fDQMHistogramOTCommonNoise.fillHitPlots(theHitContainer);
#else
    auto theHitStream = prepareHybridContainerStreamer<EmptyContainer, GenericDataArray<(NCHANNELS+1), uint32_t>, GenericDataArray<((NCHANNELS+1)*NCHIPS_OT), uint32_t>>("CMNoise_HitStream");
    for(auto board: theHitContainer)
    {
        if(fStreamerEnabled) theHitStream.streamAndSendBoard(board, fNetworkStreamer);
    }
#endif

}

float CMTester::getLambda(ChipContainer *theCbc)
{

    float average  = averageMap[theCbc]/fNevents;
    float variance = squareAverageMap[theCbc]/fNevents - (average*average);

    float sinAlfa = sin(2*M_PI * (variance - average*(1-(average/NCHANNELS)))/(NCHANNELS*(NCHANNELS-1)));

    float lambda = sqrt(sinAlfa/(1-sinAlfa));

    return lambda;
}


void CMTester::FinishRun()
{
    /*
    
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
            LOG(INFO) << BOLDRED << "Average noise on FE " << +static_cast<ReadoutChip*>(cCbc.first)->getHybridId() << " CBC " << +cCbc.first->getId() << " : " << fTotalNoise[iCbc] << " . At Vcth "
                      << cVcth << " CM is " << CMnoiseFrac << "+/-" << CMnoiseFracErr << "%, so " << CMnoiseFrac * fTotalNoise[iCbc] << " VCth." << RESET;
        else
            LOG(INFO) << BOLDRED << "FE " << +static_cast<ReadoutChip*>(cCbc.first)->getHybridId() << " CBC " << +cCbc.first->getId() << " . At Vcth " << cVcth << " CM is " << CMnoiseFrac << "+/-"
                      << CMnoiseFracErr << "%" << RESET;

         LOG(INFO) << BOLDBLUE << "Lambda = " << getLambda(cCbc.first) << RESET;
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
    LOG(INFO) << "per hybrid ... ";

    // now hybrid wise
    for(auto& cFe: fHybridHistMap)
    {
        TString cName = Form("FE%d", cFe.first->getId());

        // get histograms
        TProfile2D* cTmpOccProfile  = dynamic_cast<TProfile2D*>(getHist(cFe.first, "hybrid_combinedoccupancy"));
        TProfile*   cUncorrHitProb  = dynamic_cast<TProfile*>(getHist(cFe.first, "hybrid_uncorr_occupancyprojection"));
        TH2F*       cCorrelation2D  = dynamic_cast<TH2F*>(getHist(cFe.first, "hybrid_correlation"));
        TProfile*   cCorrProjection = dynamic_cast<TProfile*>(getHist(cFe.first, "hybrid_correlationprojection"));

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
    */
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS

void CMTester::analyze(BeBoard* pBoard, const Event* pEvent)
{
    /*
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            std::vector<bool> cHybridData; // use this to store data for all CBCs....

            for(auto cCbc: *cHybrid)
            {

                TH1F*       cTmpNHits         = dynamic_cast<TH1F*>(getHist(cCbc, "nhits"));
                int cEventHits = pEvent->GetNHits(cHybrid->getId(), cCbc->getId());
                cTmpNHits->Fill(cEventHits);
                averageMap      [cCbc] += cEventHits;
                squareAverageMap[cCbc] += cEventHits*cEventHits;
                continue;

                // here loop over the channels and fill the histograms
                // dont forget to get them first
                TProfile*   cTmpHitProb       = dynamic_cast<TProfile*>(getHist(cCbc, "hitprob"));
                TProfile2D* cTmpOccProfile    = dynamic_cast<TProfile2D*>(getHist(cCbc, "combinedoccupancy"));
                TProfile*   cTmpCombinedOcc   = dynamic_cast<TProfile*>(getHist(cCbc, "occupancyprojection"));
                TProfile*   cTmpCombinedOccPM = dynamic_cast<TProfile*>(getHist(cCbc, "occupancyprojectionplusminus"));

                int cNHits     = 0;

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

                    // move the CBC data in a vector that has data for the whole hybrid
                    cHybridData.push_back(chit);

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
                averageMap      [cCbc] += cNHits;
                squareAverageMap[cCbc] += cNHits*cNHits;

            }
            continue;

            // Here deal with per-hybrid Histograms
            TProfile2D* cTmpOccProfile  = dynamic_cast<TProfile2D*>(getHist(cHybrid, "hybrid_combinedoccupancy"));
            TProfile*   cTmpCombinedOcc = dynamic_cast<TProfile*>(getHist(cHybrid, "hybrid_occupancyprojection"));

            uint32_t cChanCt1 = 0;

            // since I use the hybrid bool vector i constructed myself this already includes simulation results if
            // simulation flag is set!
            for(auto cChan1: cHybridData)
            {
                uint32_t cChanCt2 = 0;

                for(auto cChan2: cHybridData)
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
    */

}


void CMTester::updateHists(bool pFinal)
{
    /*
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
    */
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
        fNevents = 10 * boost::any_cast<double>(cSetting->second);
    else
        fNevents = 2000;

    cSetting = fSettingsMap.find("doSimulate");

    if(cSetting != std::end(fSettingsMap))
        fDoSimulate = boost::any_cast<double>(cSetting->second);
    else
        fDoSimulate = false;

    cSetting = fSettingsMap.find("SimOccupancy");

    if(cSetting != std::end(fSettingsMap))
        fSimOccupancy = boost::any_cast<double>(cSetting->second);
    else
        fSimOccupancy = 50;

    LOG(INFO) << "Parsed the following settings:";
    LOG(INFO) << "	Nevents (.XML value times 10)= " << fNevents;
    LOG(INFO) << "	simulate = " << int(fDoSimulate);
    LOG(INFO) << "	sim. Occupancy (%) = " << int(fSimOccupancy);
}

