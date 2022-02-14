#include "../DQMUtils/DQMHistogramOTCommonNoise.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/ContainerStream.h"
#include "TFile.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"



//========================================================================================================================
DQMHistogramOTCommonNoise::DQMHistogramOTCommonNoise() {}

//========================================================================================================================
DQMHistogramOTCommonNoise::~DQMHistogramOTCommonNoise() {}

//========================================================================================================================
void DQMHistogramOTCommonNoise::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorData ready to receive the information fromm the stream
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    // SoC utilities only - END

    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);

    HistContainer<TH1F> hChipHits("ChipHits", "ChipHits", NCHANNELS+1, -0.5, NCHANNELS+1.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fChipHitHistograms, hChipHits);

    HistContainer<TH1F> hHybridHits("HybridHits", "HybridHits", (NCHANNELS+1)*NCHIPS_OT, -0.5, NCHANNELS+1.5);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fHybridHitHistograms, hHybridHits);

    
}

//========================================================================================================================
void DQMHistogramOTCommonNoise::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    
}

//========================================================================================================================
void DQMHistogramOTCommonNoise::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTCommonNoise::fill(std::vector<char>& dataBuffer)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    // As example, I'm expecting to receive a data stream from an uint32_t contained from calibration "OTCommonNoise"
    HybridContainerStream<EmptyContainer, GenericDataArray<NCHANNELS, uint32_t>, GenericDataArray<NCHANNELS*NCHIPS_OT, uint32_t>> theHybridHitStreamer("CMNoise_HitStream");

    // Try to see if the char buffer matched what I'm expection (container of uint32_t from OTCommonNoise
    // procedure)
    if(theHybridHitStreamer.attachBuffer(&dataBuffer))
    {
        theHybridHitStreamer.decodeHybridData(fDetectorData);
        fillHitPlots(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }
    // the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    // for this stream)
    return false;
    // SoC utilities only - END
}

//========================================================================================================================
bool DQMHistogramOTCommonNoise::fillHitPlots(DetectorDataContainer& theHitData)
{
    for(auto board: theHitData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    TH1F* chipHitHistogram = fChipHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    //fill the histogram from the vector
                    for(uint16_t iChan; iChan < NCHANNELS + 1; iChan++)
                    {
                        LOG(INFO) << "filling histogram with channel " << iChan << " and info " << chip->getSummary<GenericDataArray<(NCHANNELS + 1), uint32_t>>()[iChan];
                        chipHitHistogram->SetBinContent(iChan, chip->getSummary<GenericDataArray<(NCHANNELS + 1), uint32_t>>()[iChan]);
                    }
                    
                    //Now fit to get the common noise
                    //TH1F* cTmpNHits = static_cast<TH1F*>(chipHitHistogram->Clone());
                    //TF1*  cNHitsFit = dynamic_cast<TF1*>(chipHitHistogram->Clone());
                    //cTmpNHits->Reset();
                    //fitCMNoise(cTmpNHits, cNHitsFit, NCHANNELS+1);

                    //float CMNoise = fabs(cNHitsFit->GetParameter(1));
                    //float CMNoiseError = fabs(cNHitsFit->GetParError(1));

                    //LOG(INFO) << BOLDRED << "FE " << +hybrid->getIndex() << " CBC " << +chip->getIndex() << " CM is " << CMNoise << "+/-"
                    // << CMNoiseError << "%" << RESET;
                    
                }

                for(uint16_t iChan; iChan < (NCHANNELS + 1) * NCHIPS_OT; iChan++)
                {
                    TH1F* hybridHitHistogram = fHybridHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    hybridHitHistogram->SetBinContent(iChan, hybrid->getSummary<GenericDataArray<((NCHANNELS + 1) * NCHIPS_OT), uint32_t>>()[iChan]);
                }
            }
        }
    }

    //TODO do fitting

    return true;
}

//========================================================================================================================

//this used to be in CMFits.h -- Written by G. Auzinger
bool DQMHistogramOTCommonNoise::fitCMNoise(TH1F* pHitCountHist, TF1* pFit, uint32_t pRange)
{  
    // First-order approximation
    double prob = findMaximum(pHitCountHist) / pHitCountHist->GetNbinsX();

    // retrieve the threshold from the maximum of the actual nhit distribution
    double threshold = inverse_hitProbability(prob);
    LOG(INFO) << "Threshold is " << threshold; 

    // initialize cmnFraction to 0 anc later extract from fit
    double cmnFraction = 0;
    pFit->SetRange(0, pRange);

    // Set Parameters
    pFit->SetParameter(0, threshold);
    pFit->SetParameter(1, cmnFraction);

    // Fix Parameters nEvents & nActiveStrips as these I know
    pFit->FixParameter(2, pHitCountHist->GetEntries());
    pFit->FixParameter(3, pRange);

    // Name Parameters
    pFit->SetParName(0, "threshold");
    pFit->SetParName(1, "cmnFraction");
    pFit->SetParName(2, "nEvents");
    pFit->SetParName(3, "nActiveStrips");

    // Fit and return
    pHitCountHist->Fit(pFit, "RQNM+");

    return true;
}


double DQMHistogramOTCommonNoise::findMaximum(TH1F* histogram)
{
    int maxbin = histogram->GetMaximumBin();
    return histogram->GetXaxis()->GetBinCenter(maxbin);
}

double DQMHistogramOTCommonNoise::hitProbability(double threshold)
{
    return 0.5 - (TMath::Erf(threshold / sqrt(2)) / 2);
    // area above threshold under the gaussian curve.
    // The Factors are to only treat the positive half
    // 1-erf(x/sqrt(2)/2 + .5)
}

double DQMHistogramOTCommonNoise::inverse_hitProbability(double probability)
{
    // the inverse of the above function!
    return sqrt(2) * TMath::ErfInverse(1 - 2 * probability);
}


