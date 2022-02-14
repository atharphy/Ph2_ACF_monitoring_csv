#include "../DQMUtils/DQMHistogramOTCommonNoise.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/OpticalGroupContainerStream.h"
#include "TFile.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"



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
    // SoC utilities only - END

    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);

    HistContainer<TH1F> hChipHits("ChipHits", "ChipHits", NCHANNELS+1, -0.5, NCHANNELS+1+0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fChipHitHistograms, hChipHits);

    HistContainer<TH1F> hHybridHits("HybridHits", "HybridHits", NCHANNELS*NCHIPS_OT+1, -0.5, NCHANNELS*NCHIPS_OT+1+0.5);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fHybridHitHistograms, hHybridHits);
    
    HistContainer<TH1F> hModuleHits("ModuleHits", "ModuleHits", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT+0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fModuleHitHistograms, hModuleHits);

    HistContainer<TH1F> hModuleHitsEven("ModuleHitsEven", "ModuleHitsEven", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT+0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fModuleHitHistogramsEven, hModuleHitsEven);
    
    HistContainer<TH1F> hModuleHitsOdd("ModuleHitsOdd", "ModuleHitsOdd", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT+0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fModuleHitHistogramsOdd, hModuleHitsOdd);

    HistContainer<TH2F> h2DModuleHits("2DModuleHits", "2DModuleHits", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT*2+1.5, TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT*2+1.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistograms, h2DModuleHits);
       
    HistContainer<TH2F> h2DModuleHitsEven("2DModuleHitsEven", "2DModuleHitsEven", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT*2+1.5, TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT*2+0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistogramsEven, h2DModuleHitsEven);

    HistContainer<TH2F> h2DModuleHitsOdd("2DModuleHitsOdd", "2DModuleHitsOdd", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT*2+1.5, TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT*2+0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistogramsOdd, h2DModuleHitsOdd);

    HistContainer<TH2F> h2DHybridHits("2DHybridHits", "2DHybridHits", NCHANNELS*NCHIPS_OT+1, -0.5, NCHANNELS*NCHIPS_OT+1+1.5, NCHANNELS*NCHIPS_OT+1, -0.5, NCHANNELS*NCHIPS_OT+1+-.5);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, f2DHybridHitHistograms, h2DHybridHits);

    HistContainer<TH2F> h2DChipHits("2DChipHits", "2DChipHits", NCHANNELS, -0.5, NCHANNELS, NCHANNELS, -0.5, NCHANNELS+0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, f2DChipHitHistograms, h2DChipHits);


    
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
    // Contains CM Noise summary per channel at hybrid and chip level
    OpticalGroupContainerStream<EmptyContainer, GenericDataArray<NCHANNELS+1, uint32_t>, GenericDataArray<HYBRID_CHANNELS_OT+1, uint32_t>, GenericDataArray<TOTAL_CHANNELS_OT+1, uint32_t>> theOpticalGroupHitStreamer("CMNoise_HitStream");
    OpticalGroupContainerStream<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>> the2DOpticalGroupHitStreamer("CMNoise_2DHitStream");

    // Try to see if the char buffer matched what I'm expection (container of uint32_t from OTCommonNoise
    // procedure)
    if(theOpticalGroupHitStreamer.attachBuffer(&dataBuffer))
    {
        theOpticalGroupHitStreamer.decodeData(fDetectorData);
        fillHitPlots(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }
    if(the2DOpticalGroupHitStreamer.attachBuffer(&dataBuffer))
    {
        the2DOpticalGroupHitStreamer.decodeData(fDetectorData);
        fill2DHitPlots(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }

    return false;
}

//========================================================================================================================
bool DQMHistogramOTCommonNoise::fill2DHitPlots(DetectorDataContainer& the2DHitData)
{

    //make a vector of the channel boundaries of each chip
    //checking later I will start with 1, so we can check that a channel is between two bins, add an extra for the last bin and an extra for 0
    std::vector<uint32_t> chipChannelBoundaries;
    for(size_t iChip = 0; iChip < (NCHIPS_OT*2)+2; iChip++){
        chipChannelBoundaries.push_back(iChip * NCHANNELS);
    }

    for(auto board: the2DHitData)
    {
        for(auto opticalGroup: *board)
        {
            TH2F* moduleHitHistogram = f2DModuleHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogramEven = f2DModuleHitHistogramsEven.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogramOdd = f2DModuleHitHistogramsOdd.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;

            for(size_t iCh1=0; iCh1 < TOTAL_CHANNELS_OT; iCh1++){
                for(size_t iCh2=0; iCh2 < TOTAL_CHANNELS_OT; iCh2++){
                    moduleHitHistogram->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2) );
                    
                    //Even/odd
                    if( iCh1%2 == 0 && iCh2%2 == 0){
                        moduleHitHistogramEven->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2) );
                    }
                    else if( iCh1%2 == 1 && iCh2%2 == 1){
                        moduleHitHistogramOdd->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2) );
                    }


                }
            }

            for( auto hybrid: *opticalGroup){

                TH2F* hybridHitHistogram = f2DHybridHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                uint32_t hybridOffset =  HYBRID_CHANNELS_OT+1;

                for(size_t iCh1=0; iCh1 < TOTAL_CHANNELS_OT; iCh1++){
                    for(size_t iCh2=0; iCh2 < TOTAL_CHANNELS_OT; iCh2++){
                        //on hybrid 0
                        if(iCh1 < hybridOffset && iCh2 < hybridOffset && hybrid->getIndex() == 0){
                            hybridHitHistogram->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2) );
                        }
                        //on hybrid 1
                        else if(iCh1 >= hybridOffset && iCh2 >= hybridOffset && hybrid->getIndex() == 1){
                            hybridHitHistogram->SetBinContent(iCh1-hybridOffset, iCh2-hybridOffset, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2) );
                        }
                    }
                }

                for( auto chip : *hybrid){
                    
                    TH2F* chipHitHistogram = f2DChipHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    uint16_t iChan_high = chip->getIndex()+1 + (hybrid->getIndex()*NCHIPS_OT);
                    uint16_t iChan_low  = chip->getIndex()   + (hybrid->getIndex()*NCHIPS_OT);

                    uint32_t chipOffset = (hybrid->getIndex() * HYBRID_CHANNELS_OT ) + (chip->getIndex() * (NCHANNELS) );
                    LOG(INFO) << "filling chip " << chip->getIndex() << " with offset " << chipOffset;
                    LOG(INFO) << "        the boundaries are " << chipChannelBoundaries[iChan_low] << " " << chipChannelBoundaries[iChan_high];



                    for(size_t iCh1= chipChannelBoundaries[iChan_low]  ; iCh1 < chipChannelBoundaries[iChan_high]; iCh1++){
                        for(size_t iCh2= chipChannelBoundaries[iChan_low]; iCh2 <= chipChannelBoundaries[iChan_high]; iCh2++){                                
                            chipHitHistogram->SetBinContent(iCh1-chipOffset, iCh2-chipOffset, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2) );
                            
                        }
                    }
                }

            }
        }
    }


    return true;

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
                    for(uint16_t iChan=0; iChan < NCHANNELS + 1; iChan++)
                    {
                        
                        chipHitHistogram->SetBinContent(iChan, chip->getSummary<GenericDataArray<NCHANNELS+1, uint32_t>>()[iChan]);
                    }
                    
                }
                TH1F* hybridHitHistogram = fHybridHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                for(uint16_t iChan=0; iChan < (NCHANNELS * NCHIPS_OT)+1; iChan++)
                {
                    hybridHitHistogram->SetBinContent(iChan, hybrid->getSummary<GenericDataArray<HYBRID_CHANNELS_OT+1, uint32_t>>()[iChan]);
                }
            }

            TH1F* moduleHitHistogram = fModuleHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
            TH1F* moduleHitHistogramEven = fModuleHitHistogramsEven.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
            TH1F* moduleHitHistogramOdd = fModuleHitHistogramsOdd.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;

            for(uint16_t iChan=0; iChan < (NCHANNELS + 1) * NCHIPS_OT*2; iChan++)
            {


                moduleHitHistogram->SetBinContent(iChan, opticalGroup->getSummary<GenericDataArray<TOTAL_CHANNELS_OT, uint32_t>>()[iChan]);

                if(iChan %2 == 0 ) moduleHitHistogramEven->SetBinContent(iChan, opticalGroup->getSummary<GenericDataArray<TOTAL_CHANNELS_OT, uint32_t>>()[iChan]);
                else moduleHitHistogramOdd->SetBinContent(iChan, opticalGroup->getSummary<GenericDataArray<TOTAL_CHANNELS_OT, uint32_t>>()[iChan]);
            }
        }
    }

        //Now fit to get the common noise
    // TH1F* cTmpNHits = static_cast<TH1F*>(chipHitHistogram->Clone());
    // //fit with custom function (defined below) with 4 parameters -- threshold, CMNoise fraction, # events, # active strips
    // TF1*  cCmFit = new TF1(cName, hitProbFunction, 0, 255, 4);
    // cTmpNHits->Reset();
    // fitCMNoise(cTmpNHits, cNHitsFit, NCHANNELS+1);

    // float CMNoise = fabs(cNHitsFit->GetParameter(1));
    // float CMNoiseError = fabs(cNHitsFit->GetParError(1));

    // LOG(INFO) << BOLDRED << "FE " << +hybrid->getIndex() << " CBC " << +chip->getIndex() << " CM is " << CMNoise << "+/-"
    // << CMNoiseError << "%" << RESET;

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






double DQMHistogramOTCommonNoise::findMaximum(TH1F* pHistogram)
{
    int maxbin = pHistogram->GetMaximumBin();
    return pHistogram->GetXaxis()->GetBinCenter(maxbin);
}

double DQMHistogramOTCommonNoise::hitProbability(double pThreshold)
{
    return 0.5 - (TMath::Erf(pThreshold / sqrt(2)) / 2);
    // area above threshold under the gaussian curve.
    // The Factors are to only treat the positive half
    // 1-erf(x/sqrt(2)/2 + .5)
}

double DQMHistogramOTCommonNoise::inverse_hitProbability(double pProbability)
{
    // the inverse of the above function!
    return sqrt(2) * TMath::ErfInverse(1 - 2 * pProbability);
}

double DQMHistogramOTCommonNoise::binomialPdf(int n, int k, double p) { return TMath::Binomial(n, k) * pow(p, k) * pow((1 - p), n - k); }


double DQMHistogramOTCommonNoise::hitProbFunction(double* pStrips, Double_t* pPar)
{
    uint32_t cNSamplingsCM = 100;
    uint32_t cSigmaRange = 6;

    const double samplingHalfStep = cSigmaRange / static_cast<double>(cNSamplingsCM);
    double&      threshold        = pPar[0];
    double&      cmnFraction      = pPar[1];
    double&      nEvents          = pPar[2];
    double&      nActiveStrips    = pPar[3];

    double result = 0;
    double hitProb;
    double sampleProbability, x;

    int iStrips = int(ceil(*pStrips - 0.5));                 // round to nearest integer
    if((iStrips < 0) || (iStrips > nActiveStrips)) return 0; // only defined in range

    for(uint32_t 
     j = 0; j < cNSamplingsCM; ++j)
    {
        // loop over all x values
        x = -cSigmaRange + j * 2 * samplingHalfStep;
        // approximate probability at sampling point by interpolating
        sampleProbability = hitProbability(x - samplingHalfStep);
        sampleProbability -= hitProbability(x + samplingHalfStep);

        // probability of hit taking cmn into account
        hitProb = hitProbability(threshold + x * cmnFraction);
        // distribution function scaled to nevents
        result += binomialPdf(int(nActiveStrips), iStrips, hitProb) * sampleProbability * nEvents;
    }
    return result;
}