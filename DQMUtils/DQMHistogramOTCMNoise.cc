#include "DQMUtils/DQMHistogramOTCMNoise.h"
#include "RootUtils/RootContainerFactory.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include "Utils/Utilities.h"

//========================================================================================================================
DQMHistogramOTCMNoise::DQMHistogramOTCMNoise() {}

//========================================================================================================================
DQMHistogramOTCMNoise::~DQMHistogramOTCMNoise() {}

//========================================================================================================================
void DQMHistogramOTCMNoise::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorData ready to receive the information fromm the stream
    // SoC utilities only - END
    fDetectorContainer = &theDetectorStructure;

    auto cSetting = pSettingsMap.find("Nevents");
    if(cSetting != std::end(pSettingsMap))
        fNevents = boost::any_cast<double>(cSetting->second);
    else
        fNevents = 1000; // this should never be the case,since we ran events to get here.

    cSetting = pSettingsMap.find("CMNoise_2DHistograms");
    if(cSetting != std::end(pSettingsMap))
    {
        if(boost::any_cast<double>(cSetting->second) == 1)
            f2DHistograms = true;
        else
            f2DHistograms = false;
    }
    else
        f2DHistograms = false;

    HistContainer<TH1F> hChipHits("ChipHits", "ChipHits", NCHANNELS + 1, -0.5, NCHANNELS + 1 + 0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fChipHitHistograms, hChipHits);

    HistContainer<TH1F> hHybridHits("HybridHits", "HybridHits", HYBRID_CHANNELS_OT + 1, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fHybridHitHistograms, hHybridHits);

    HistContainer<TH1F> hModuleHits("ModuleHits", "ModuleHits", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT + 0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fModuleHitHistograms, hModuleHits);

    HistContainer<TH1F> hModuleHitsEven("ModuleHitsEven", "ModuleHitsEven", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT + 0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fModuleHitHistogramsEven, hModuleHitsEven);

    HistContainer<TH1F> hModuleHitsOdd("ModuleHitsOdd", "ModuleHitsOdd", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT + 0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fModuleHitHistogramsOdd, hModuleHitsOdd);

    if(f2DHistograms)
    {
        HistContainer<TH2F> h2DModuleHits("2DModuleHits", "2DModuleHits", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5, TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistograms, h2DModuleHits);

        HistContainer<TH2F> h2DModuleHitsEven("2DModuleHitsEven", "2DModuleHitsEven", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5, TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistogramsEven, h2DModuleHitsEven);

        HistContainer<TH2F> h2DModuleHitsOdd("2DModuleHitsOdd", "2DModuleHitsOdd", TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5, TOTAL_CHANNELS_OT, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistogramsOdd, h2DModuleHitsOdd);

        HistContainer<TH2F> h2DHybridHits("2DHybridHits", "2DHybridHits", NCHANNELS * NCHIPS_OT + 1, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5, HYBRID_CHANNELS_OT + 1, -0.5, HYBRID_CHANNELS_OT + 1 + -.5);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, f2DHybridHitHistograms, h2DHybridHits);

        HistContainer<TH2F> h2DChipHits("2DChipHits", "2DChipHits", NCHANNELS, -0.5, NCHANNELS + 1 + 0.5, NCHANNELS, -0.5, NCHANNELS + 1 + 0.5);
        RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, f2DChipHitHistograms, h2DChipHits);

        HistContainer<TH2F> h2DHybridHits_chip("2DHybridHits_chip", "2DHybridHits_chip", NCHIPS_OT, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5, NCHIPS_OT, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, f2DHybridHitHistograms_chip, h2DHybridHits_chip);

        HistContainer<TH2F> h2DModuleHits_chip("2DModuleHits_chip", "2DModuleHits_chip", NCHIPS_OT * 2, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5, NCHIPS_OT * 2, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistograms_chip, h2DModuleHits_chip);

        HistContainer<TH2F> h2DModuleHitsEven_chip("2DModuleHitsEven_chip", "2DModuleHitsEven_chip", NCHIPS_OT * 2, 0, TOTAL_CHANNELS_OT + 1, NCHIPS_OT * 2, 0, TOTAL_CHANNELS_OT + 1);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistogramsEven_chip, h2DModuleHitsEven_chip);

        HistContainer<TH2F> h2DModuleHitsOdd_chip("2DModuleHitsOdd_chip", "2DModuleHitsOdd_chip", NCHIPS_OT * 2, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5, NCHIPS_OT * 2, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistogramsOdd_chip, h2DModuleHitsOdd_chip);
    }
}

//========================================================================================================================
void DQMHistogramOTCMNoise::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTCMNoise::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTCMNoise::fill(std::string& inputStream)
{
    
    ContainerSerialization theHitSerialization("OTCMNoiseHitStream");
    ContainerSerialization the2DHitSerialization("OTCMNoise2DHitStream");

    if(theHitSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched OTCMNoise HitStream!!!!!\n";
        DetectorDataContainer fDetectorData = theHitSerialization.deserializeOpticalGroupContainer<EmptyContainer,
                                                                                                   GenericDataArray<NCHANNELS + 1, uint32_t>,
                                                                                                   GenericDataArray<HYBRID_CHANNELS_OT + 1, uint32_t>,
                                                                                                   GenericDataArray<TOTAL_CHANNELS_OT + 1, uint32_t>>(fDetectorContainer);
        fillHitPlots(fDetectorData);
        return true;
    }
    if(theHitSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched OTCMNoise 2DHitStream!!!!!\n";
        DetectorDataContainer fDetectorData =
            theHitSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>(
                fDetectorContainer);
        fill2DHitPlots(fDetectorData);
        return true;
    }
    return false;
}

//========================================================================================================================
bool DQMHistogramOTCMNoise::fill2DHitPlots(DetectorDataContainer& the2DHitData)
{
    // make a vector of the channel boundaries of each chip
    // checking later I will start with 1, so we can check that a channel is between two bins, add an extra for the last bin and an extra for 0
    std::vector<uint32_t> chipChannelBoundaries;
    for(size_t iChip = 0; iChip < (NCHIPS_OT * 2) + 2; iChip++) { chipChannelBoundaries.push_back(iChip * NCHANNELS); }

    for(auto board: the2DHitData)
    {
        for(auto opticalGroup: *board)
        {
            TH2F* moduleHitHistogram          = f2DModuleHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogramEven      = f2DModuleHitHistogramsEven.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogramOdd       = f2DModuleHitHistogramsOdd.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogram_chip     = f2DModuleHitHistograms_chip.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogramEven_chip = f2DModuleHitHistogramsEven_chip.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogramOdd_chip  = f2DModuleHitHistogramsOdd_chip.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;

            for(size_t iCh1 = 0; iCh1 < TOTAL_CHANNELS_OT; iCh1++)
            {
                for(size_t iCh2 = 0; iCh2 < TOTAL_CHANNELS_OT; iCh2++)
                {
                    moduleHitHistogram->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2));

                    // to fill chip-level, need to sum up each bin
                    auto bin_x     = moduleHitHistogram_chip->GetXaxis()->FindBin(iCh1);
                    auto bin_y     = moduleHitHistogram_chip->GetYaxis()->FindBin(iCh2);
                    auto prev_hits = moduleHitHistogram_chip->GetBinContent(bin_x, bin_y);
                    moduleHitHistogram_chip->SetBinContent(bin_x, bin_y, prev_hits + opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2));

                    // Even/odd
                    if(iCh1 % 2 == 0 && iCh2 % 2 == 0)
                    {
                        moduleHitHistogramEven->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2));

                        auto prev_hits_even = moduleHitHistogramEven_chip->GetBinContent(bin_x, bin_y);
                        moduleHitHistogramEven_chip->SetBinContent(
                            bin_x, bin_y, prev_hits_even + opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2));
                    }
                    else if(iCh1 % 2 == 1 && iCh2 % 2 == 1)
                    {
                        moduleHitHistogramOdd->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2));

                        auto prev_hits_odd = moduleHitHistogramOdd_chip->GetBinContent(bin_x, bin_y);
                        moduleHitHistogramOdd_chip->SetBinContent(
                            bin_x, bin_y, prev_hits_odd + opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2));
                    }
                }
            }

            for(auto hybrid: *opticalGroup)
            {
                TH2F* hybridHitHistogram = f2DHybridHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                TH2F* hybridHitHistogram_chip =
                    f2DHybridHitHistograms_chip.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                uint32_t hybridOffset = HYBRID_CHANNELS_OT + 1;

                for(size_t iCh1 = 0; iCh1 < TOTAL_CHANNELS_OT; iCh1++)
                {
                    for(size_t iCh2 = 0; iCh2 < TOTAL_CHANNELS_OT; iCh2++)
                    {
                        // on hybrid 0
                        if(iCh1 < hybridOffset && iCh2 < hybridOffset && hybrid->getIndex() == 0)
                        {
                            auto bin_x     = hybridHitHistogram_chip->GetXaxis()->FindBin(iCh1);
                            auto bin_y     = hybridHitHistogram_chip->GetYaxis()->FindBin(iCh2);
                            auto prev_hits = hybridHitHistogram_chip->GetBinContent(bin_x, bin_y);
                            hybridHitHistogram->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2));
                            hybridHitHistogram_chip->SetBinContent(
                                bin_x, bin_y, prev_hits + opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2));
                        }
                        // on hybrid 1
                        else if(iCh1 >= hybridOffset && iCh2 >= hybridOffset && hybrid->getIndex() == 1)
                        {
                            auto bin_x     = hybridHitHistogram_chip->GetXaxis()->FindBin(iCh1 - hybridOffset);
                            auto bin_y     = hybridHitHistogram_chip->GetYaxis()->FindBin(iCh2 - hybridOffset);
                            auto prev_hits = hybridHitHistogram_chip->GetBinContent(bin_x, bin_y);

                            hybridHitHistogram->SetBinContent(
                                iCh1 - hybridOffset, iCh2 - hybridOffset, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2));
                            hybridHitHistogram_chip->SetBinContent(
                                bin_x, bin_y, prev_hits + opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2));
                        }
                    }
                }

                for(auto chip: *hybrid)
                {
                    TH2F* chipHitHistogram =
                        f2DChipHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    uint16_t iChan_high = chip->getIndex() + 1 + (hybrid->getIndex() * NCHIPS_OT);
                    uint16_t iChan_low  = chip->getIndex() + (hybrid->getIndex() * NCHIPS_OT);

                    uint32_t chipOffset = (hybrid->getIndex() * HYBRID_CHANNELS_OT) + (chip->getIndex() * (NCHANNELS));

                    for(size_t iCh1 = chipChannelBoundaries[iChan_low]; iCh1 < chipChannelBoundaries[iChan_high]; iCh1++)
                    {
                        for(size_t iCh2 = chipChannelBoundaries[iChan_low]; iCh2 <= chipChannelBoundaries[iChan_high]; iCh2++)
                        {
                            chipHitHistogram->SetBinContent(
                                iCh1 - chipOffset, iCh2 - chipOffset, opticalGroup->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(iCh1, iCh2));
                        }
                    }
                }
            }
        }
    }

    return true;
}

//========================================================================================================================
bool DQMHistogramOTCMNoise::fillHitPlots(DetectorDataContainer& theHitData)
{
    for(auto board: theHitData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    TH1F* cChipHitHistogram =
                        fChipHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;

                    // fill the histogram from the vector
                    for(uint16_t iChan = 0; iChan < NCHANNELS + 1; iChan++) { cChipHitHistogram->SetBinContent(iChan, chip->getSummary<GenericDataArray<NCHANNELS + 1, uint32_t>>()[iChan]); }
                    // do fitting
                    TF1* cChipFit = new TF1("chipFit", hitProbabilityFunction, 0, NCHANNELS + 1, 4);
                    fitCMNoise(cChipHitHistogram, cChipFit, NCHANNELS + 1);
                    LOG(INFO) << BOLDRED << "FE " << hybrid->getIndex() << " CBC " << chip->getIndex() << " CM is " << fabs(cChipFit->GetParameter(1)) << "+/-" << fabs(cChipFit->GetParError(1)) << "%"
                              << RESET;
                }
                TH1F* cHybridHitHistogram = fHybridHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                for(uint16_t iChan = 0; iChan < HYBRID_CHANNELS_OT + 1; iChan++)
                { cHybridHitHistogram->SetBinContent(iChan, hybrid->getSummary<GenericDataArray<HYBRID_CHANNELS_OT + 1, uint32_t>>()[iChan]); }

                /* unfortuantely this requires computing a binomial coefficient, which overflows a double in this range
                TF1* cHybridFit = new TF1("hybridFit", hitProbabilityFunction, 0, 2032, 4);
                fitCMNoise(cHybridHitHistogram, cHybridFit, 2032);
                LOG(INFO) << BOLDRED << "FE " << hybrid->getIndex() << " CM is " << fabs(cHybridFit->GetParameter(1)) << "+/-" << fabs(cHybridFit->GetParError(1)) << "%" << RESET;
                */
            }

            TH1F* cModuleHitHistogram     = fModuleHitHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
            TH1F* cModuleHitHistogramEven = fModuleHitHistogramsEven.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
            TH1F* cModuleHitHistogramOdd  = fModuleHitHistogramsOdd.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;

            for(uint16_t iChan = 0; iChan < (NCHANNELS + 1) * NCHIPS_OT * 2; iChan++)
            {
                cModuleHitHistogram->SetBinContent(iChan, opticalGroup->getSummary<GenericDataArray<TOTAL_CHANNELS_OT, uint32_t>>()[iChan]);

                if(iChan % 2 == 0)
                    cModuleHitHistogramEven->SetBinContent(iChan, opticalGroup->getSummary<GenericDataArray<TOTAL_CHANNELS_OT, uint32_t>>()[iChan]);
                else
                    cModuleHitHistogramOdd->SetBinContent(iChan, opticalGroup->getSummary<GenericDataArray<TOTAL_CHANNELS_OT, uint32_t>>()[iChan]);
            }
            /* unfortuantely this requires computing a binomial coefficient, which overflows a double in this range
            TF1* cModuleFit = new TF1("hybridFit", hitProbabilityFunction, 0, TOTAL_CHANNELS_OT+1, 4);
            fitCMNoise(cModuleHitHistogram, cModuleFit, TOTAL_CHANNELS_OT+1);
            LOG(INFO) << BOLDRED << "Full Module " << opticalGroup->getIndex() << " CM is " << fabs(cModuleFit->GetParameter(1)) << "+/-" << fabs(cModuleFit->GetParameter(1)) << "%" << RESET;
            fitCMNoise(cModuleHitHistogramEven, cModuleFit, TOTAL_CHANNELS_OT+1);
            LOG(INFO) << BOLDRED << "Full Module " << opticalGroup->getIndex() << " Even CM is " << fabs(cModuleFit->GetParameter(1)) << "+/-" << fabs(cModuleFit->GetParameter(1)) << "%" <<
            RESET; fitCMNoise(cModuleHitHistogramOdd, cModuleFit, TOTAL_CHANNELS_OT+1); LOG(INFO) << BOLDRED << "Full Module " << opticalGroup->getIndex() << " Odd CM is " <<
            fabs(cModuleFit->GetParameter(1)) << "+/-" << fabs(cModuleFit->GetParameter(1)) << "%" << RESET;
            */
        }
    }

    return true;
}

//========================================================================================================================

// this used to be in CMFits.h -- Written by G. Auzinger
bool DQMHistogramOTCMNoise::fitCMNoise(TH1F* pHitCountHist, TF1* pFit, uint32_t pRange)
{
    // First-order approximation
    double prob = findMaximum(pHitCountHist) / pHitCountHist->GetNbinsX();

    // retrieve the threshold from the maximum of the actual nhit distribution
    double threshold = inverse_hitProbability(prob);

    // initialize cmnFraction to 0 anc later extract from fit
    double cmnFraction = 0.5;
    pFit->SetRange(0, pRange);

    // Set Parameters
    pFit->SetParameter(0, threshold);
    pFit->SetParameter(1, cmnFraction);

    // Fix Parameters nEvents & nActiveStrips as these I know
    pFit->FixParameter(2, fNevents);
    pFit->FixParameter(3, pRange);

    // Name Parameters
    pFit->SetParName(0, "threshold");
    pFit->SetParName(1, "cmnFraction");
    pFit->SetParName(2, "nEvents");
    pFit->SetParName(3, "nActiveStrips");

    // Fit and return
    pHitCountHist->Fit(pFit, "RQ+");

    return true;
}

double DQMHistogramOTCMNoise::findMaximum(TH1F* pHistogram)
{
    int maxbin = pHistogram->GetMaximumBin();
    return pHistogram->GetXaxis()->GetBinCenter(maxbin);
}

double DQMHistogramOTCMNoise::inverse_hitProbability(double probability) { return sqrt(2) * TMath::ErfInverse(1 - 2 * probability); }
