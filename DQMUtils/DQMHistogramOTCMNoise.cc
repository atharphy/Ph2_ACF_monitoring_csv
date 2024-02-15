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

    auto cSetting = pSettingsMap.find("CMNevents");
    if(cSetting != std::end(pSettingsMap))
        fNevents = boost::any_cast<double>(cSetting->second);
    else
        fNevents = 200000; // this should never be the case,since we ran events to get here.

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

    HistContainer<TH1F> hChipHits("ChipHits", "ChipHits", NCHANNELS + 2, -0.5, NCHANNELS + 1 + 0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fChipHitHistograms, hChipHits);

    //TODO Reduce the number of bins by a factor 2
    HistContainer<TH1F> hChipHitsEven("ChipHitsEven", "ChipHitsEven", NCHANNELS + 2, -0.5, NCHANNELS + 1 + 0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fChipHitHistogramsEven, hChipHitsEven);

    HistContainer<TH1F> hChipHitsOdd("ChipHitsOdd", "ChipHitsOdd", NCHANNELS + 2, -0.5, NCHANNELS + 1 + 0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fChipHitHistogramsOdd, hChipHitsOdd);


    HistContainer<TH1F> hHybridHits("HybridHits", "HybridHits", HYBRID_CHANNELS_OT + 2, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fHybridHitHistograms, hHybridHits);

    HistContainer<TH1F> hHybridHitsEven("HybridHitsEven", "HybridHitsEven", HYBRID_CHANNELS_OT + 2, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fHybridHitHistogramsEven, hHybridHitsEven);

    HistContainer<TH1F> hHybridHitsOdd("HybridHitsOdd", "HybridHitsOdd", HYBRID_CHANNELS_OT + 2, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fHybridHitHistogramsOdd, hHybridHitsOdd);

    HistContainer<TH1F> hModuleHits("ModuleHits", "ModuleHits", TOTAL_CHANNELS_OT + 1, -0.5, TOTAL_CHANNELS_OT + 0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fModuleHitHistograms, hModuleHits);

    HistContainer<TH1F> hModuleHitsEven("ModuleHitsEven", "ModuleHitsEven", TOTAL_CHANNELS_OT + 1, -0.5, TOTAL_CHANNELS_OT + 0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fModuleHitHistogramsEven, hModuleHitsEven);

    HistContainer<TH1F> hModuleHitsOdd("ModuleHitsOdd", "ModuleHitsOdd", TOTAL_CHANNELS_OT + 1, -0.5, TOTAL_CHANNELS_OT + 0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fModuleHitHistogramsOdd, hModuleHitsOdd);

    HistContainer<TH2F> h2DModuleSensorCorrelation("ModuleSensorCorrelation", "ModuleSensorCorrelation", TOTAL_CHANNELS_OT/2 + 2, -0.5, TOTAL_CHANNELS_OT/2 + 1 + 0.5, TOTAL_CHANNELS_OT/2 + 2, -0.5, TOTAL_CHANNELS_OT/2 + 1 + 0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleSensorCorrelation, h2DModuleSensorCorrelation);

    HistContainer<TH2F> h2DHybridSensorCorrelation("HybridSensorCorrelation", "HybridSensorCorrelation", HYBRID_CHANNELS_OT/2 + 2, -0.5, HYBRID_CHANNELS_OT/2 + 1 + 0.5, HYBRID_CHANNELS_OT/2 + 2, -0.5, HYBRID_CHANNELS_OT/2 + 1 + 0.5);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, f2DHybridSensorCorrelation, h2DHybridSensorCorrelation);

    HistContainer<TH2F> h2DChipSensorCorrelation("ChipSensorCorrelation", "ChipSensorCorrelation", NCHANNELS/2 + 2, -0.5, NCHANNELS/2 + 1 + 0.5, NCHANNELS/2 + 2, -0.5, NCHANNELS/2 + 1 + 0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, f2DChipSensorCorrelation, h2DChipSensorCorrelation);

    HistContainer<TH2F> h2DHybridCorrelation("CrossHybridCorrelation", "CrossHybridCorrelation", HYBRID_CHANNELS_OT + 2, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5, HYBRID_CHANNELS_OT + 2, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DHybridCorrelation, h2DHybridCorrelation);

    HistContainer<TH2F> h2DChipCorrelation("ChipCorrelation", "ChipCorrelation", NCHANNELS + 2, -0.5, NCHANNELS + 1 + 0.5, HYBRID_CHANNELS_OT + 2, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, f2DChipCorrelation, h2DChipCorrelation);

    if(f2DHistograms)
    {
        HistContainer<TH2F> h2DModuleHits("2DModuleHits", "2DModuleHits", TOTAL_CHANNELS_OT + 2, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5, TOTAL_CHANNELS_OT + 2, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistograms, h2DModuleHits);

        HistContainer<TH2F> h2DModuleHitsEven("2DModuleHitsEven", "2DModuleHitsEven", TOTAL_CHANNELS_OT + 2, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5, TOTAL_CHANNELS_OT + 2, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistogramsEven, h2DModuleHitsEven);

        HistContainer<TH2F> h2DModuleHitsOdd("2DModuleHitsOdd", "2DModuleHitsOdd", TOTAL_CHANNELS_OT + 2, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5, TOTAL_CHANNELS_OT + 2, -0.5, TOTAL_CHANNELS_OT + 1 + 0.5);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistogramsOdd, h2DModuleHitsOdd);

        HistContainer<TH2F> h2DHybridHits("2DHybridHits", "2DHybridHits", NCHANNELS * NCHIPS_OT + 2, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5, HYBRID_CHANNELS_OT + 2, -0.5, HYBRID_CHANNELS_OT + 1 + 0.5);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, f2DHybridHitHistograms, h2DHybridHits);

        HistContainer<TH2F> h2DChipHits("2DChipHits", "2DChipHits", NCHANNELS + 2, -0.5, NCHANNELS + 1 + 0.5, NCHANNELS + 2, -0.5, NCHANNELS + 1 + 0.5);
        RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, f2DChipHitHistograms, h2DChipHits);

        HistContainer<TH2F> h2DHybridHits_chip("2DHybridHits_chip", "2DHybridHits_chip", NCHIPS_OT, 0, HYBRID_CHANNELS_OT, NCHIPS_OT, 0, HYBRID_CHANNELS_OT);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, f2DHybridHitHistograms_chip, h2DHybridHits_chip);

        HistContainer<TH2F> h2DModuleHits_chip("2DModuleHits_chip", "2DModuleHits_chip", NCHIPS_OT * 2, 0, TOTAL_CHANNELS_OT, NCHIPS_OT * 2, 0, TOTAL_CHANNELS_OT);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistograms_chip, h2DModuleHits_chip);

        HistContainer<TH2F> h2DModuleHitsEven_chip("2DModuleHitsEven_chip", "2DModuleHitsEven_chip", NCHIPS_OT * 2, 0, TOTAL_CHANNELS_OT, NCHIPS_OT * 2, 0, TOTAL_CHANNELS_OT);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f2DModuleHitHistogramsEven_chip, h2DModuleHitsEven_chip);

        HistContainer<TH2F> h2DModuleHitsOdd_chip("2DModuleHitsOdd_chip", "2DModuleHitsOdd_chip", NCHIPS_OT * 2, 0, TOTAL_CHANNELS_OT, NCHIPS_OT * 2, 0, TOTAL_CHANNELS_OT);
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


template <typename T1, typename T2, typename T3, typename T4>
bool DQMHistogramOTCMNoise::processInputStream(std::string streamName, std::string & inputStream , bool (DQMHistogramOTCMNoise::*function)(DetectorDataContainer&)){
    ContainerSerialization theSerializer(streamName);    
    try{
        if(theSerializer.attachDeserializer(inputStream))
        {
            LOG(INFO)<<"Matched stream "<<streamName<<"!"<<RESET;
            DetectorDataContainer fDetectorData = theSerializer.deserializeOpticalGroupContainer<T1,T2,T3,T4>(fDetectorContainer);
            (this->*function)(fDetectorData);
            return true;
        }
    }
    catch (const std::exception& e) // reference to the base of a polymorphic object
    {
        LOG(INFO)<<BOLDRED<<" Unable to read stream "<<streamName<<": "<<e.what()<<RESET;
    }
    return false;

}

bool DQMHistogramOTCMNoise::fill(std::string& inputStream)
{    
    if (processInputStream<EmptyContainer,GenericDataArray<uint32_t, 3*(NCHANNELS + 1)>,EmptyContainer,EmptyContainer>
                                            ("OTCMNoiseChipHitStream",inputStream,&DQMHistogramOTCMNoise::fillChipHitPlots))
                        return 1;
    if (processInputStream<EmptyContainer,EmptyContainer,GenericDataArray<uint32_t,3*(HYBRID_CHANNELS_OT + 1)>,EmptyContainer>
                                            ("OTCMNoiseHybridHitStream",inputStream,&DQMHistogramOTCMNoise::fillHybridHitPlots))
                        return 1;
    if (processInputStream<EmptyContainer,EmptyContainer,EmptyContainer,GenericDataArray<uint32_t,3*(TOTAL_CHANNELS_OT + 1)>>
                                            ("OTCMNoiseModuleHitStream",inputStream,&DQMHistogramOTCMNoise::fillModuleHitPlots))
                        return 1;
    if (processInputStream<EmptyContainer,GenericDataArray<uint32_t,NCHANNELS + 1, HYBRID_CHANNELS_OT + 1>,EmptyContainer,GenericDataArray<uint32_t,HYBRID_CHANNELS_OT + 1, HYBRID_CHANNELS_OT + 1>>
                                            ("OTCMNoise2DHybridCorrelationStream",inputStream,&DQMHistogramOTCMNoise::fillHybridCorrelationPlots))
                        return 1;
    if (processInputStream<EmptyContainer,GenericDataArray<uint32_t,(NCHANNELS / 2 + 1), (NCHANNELS / 2 + 1)>,EmptyContainer,EmptyContainer>
                                            ("OTCMNoise2DSensorChipCorrelationStream",inputStream,&DQMHistogramOTCMNoise::fillSensorChipCorrelationPlots))
                        return 1;
    if (processInputStream<EmptyContainer,EmptyContainer,GenericDataArray<uint32_t,(HYBRID_CHANNELS_OT / 2 + 1), (HYBRID_CHANNELS_OT / 2 + 1)>,EmptyContainer>
                                            ("OTCMNoise2DSensorHybridCorrelationStream",inputStream,&DQMHistogramOTCMNoise::fillSensorHybridCorrelationPlots))
                        return 1;
    if (processInputStream<EmptyContainer,EmptyContainer,EmptyContainer,GenericDataArray<uint32_t,(TOTAL_CHANNELS_OT / 2 + 1), (TOTAL_CHANNELS_OT / 2 + 1)>>
                                            ("OTCMNoise2DSensorModuleCorrelationStream",inputStream,&DQMHistogramOTCMNoise::fillSensorModuleCorrelationPlots))
                        return 1;

    if (processInputStream<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray<uint32_t,TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>
                                            ("OTCMNoise2DHitStream",inputStream,&DQMHistogramOTCMNoise::fill2DHitPlots))
                        return 1;
                        
    LOG(INFO)<<BOLDRED<<"OTCMNoise stream wasn't claimed!"<<RESET;
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
            TH2F* moduleHitHistogram          = f2DModuleHitHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogramEven      = f2DModuleHitHistogramsEven.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogramOdd       = f2DModuleHitHistogramsOdd.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogram_chip     = f2DModuleHitHistograms_chip.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogramEven_chip = f2DModuleHitHistogramsEven_chip.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            TH2F* moduleHitHistogramOdd_chip  = f2DModuleHitHistogramsOdd_chip.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;

            for(size_t iCh1 = 0; iCh1 < TOTAL_CHANNELS_OT; iCh1++)
            {
                for(size_t iCh2 = 0; iCh2 < TOTAL_CHANNELS_OT; iCh2++)
                {
                    moduleHitHistogram->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[iCh1][iCh2]);

                    // to fill chip-level, need to sum up each bin
                    auto bin_x     = moduleHitHistogram_chip->GetXaxis()->FindBin(iCh1);
                    auto bin_y     = moduleHitHistogram_chip->GetYaxis()->FindBin(iCh2);
                    auto prev_hits = moduleHitHistogram_chip->GetBinContent(bin_x, bin_y);
                    moduleHitHistogram_chip->SetBinContent(bin_x, bin_y, prev_hits + opticalGroup->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[iCh1][iCh2]);

                    // Even/odd
                    if(iCh1 % 2 == 0 && iCh2 % 2 == 0)
                    {
                        moduleHitHistogramEven->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[iCh1][iCh2]);

                        auto prev_hits_even = moduleHitHistogramEven_chip->GetBinContent(bin_x, bin_y);
                        moduleHitHistogramEven_chip->SetBinContent(
                            bin_x, bin_y, prev_hits_even + opticalGroup->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[iCh1][iCh2]);
                    }
                    else if(iCh1 % 2 == 1 && iCh2 % 2 == 1)
                    {
                        moduleHitHistogramOdd->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[iCh1][iCh2]);

                        auto prev_hits_odd = moduleHitHistogramOdd_chip->GetBinContent(bin_x, bin_y);
                        moduleHitHistogramOdd_chip->SetBinContent(
                            bin_x, bin_y, prev_hits_odd + opticalGroup->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[iCh1][iCh2]);
                    }
                }
            }

            for(auto hybrid: *opticalGroup)
            {
                TH2F* hybridHitHistogram =
                    f2DHybridHitHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                TH2F* hybridHitHistogram_chip =
                    f2DHybridHitHistograms_chip.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                uint32_t hybridOffset = HYBRID_CHANNELS_OT + 1;

                for(size_t iCh1 = 0; iCh1 < TOTAL_CHANNELS_OT; iCh1++)
                {
                    for(size_t iCh2 = 0; iCh2 < TOTAL_CHANNELS_OT; iCh2++)
                    {
                        // on hybrid 0
                        if(iCh1 < hybridOffset && iCh2 < hybridOffset && hybrid->getId() == 0)
                        {
                            auto bin_x     = hybridHitHistogram_chip->GetXaxis()->FindBin(iCh1);
                            auto bin_y     = hybridHitHistogram_chip->GetYaxis()->FindBin(iCh2);
                            auto prev_hits = hybridHitHistogram_chip->GetBinContent(bin_x, bin_y);
                            hybridHitHistogram->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[iCh1][iCh2]);
                            hybridHitHistogram_chip->SetBinContent(bin_x, bin_y, prev_hits + opticalGroup->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[iCh1][iCh2]);
                        }
                        // on hybrid 1
                        else if(iCh1 >= hybridOffset && iCh2 >= hybridOffset && hybrid->getId() == 1)
                        {
                            auto bin_x     = hybridHitHistogram_chip->GetXaxis()->FindBin(iCh1 - hybridOffset);
                            auto bin_y     = hybridHitHistogram_chip->GetYaxis()->FindBin(iCh2 - hybridOffset);
                            auto prev_hits = hybridHitHistogram_chip->GetBinContent(bin_x, bin_y);

                            hybridHitHistogram->SetBinContent(
                                iCh1 - hybridOffset, iCh2 - hybridOffset, opticalGroup->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[iCh1][iCh2]);
                            hybridHitHistogram_chip->SetBinContent(bin_x, bin_y, prev_hits + opticalGroup->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[iCh1][iCh2]);
                        }
                    }
                }

                for(auto chip: *hybrid)
                {
                    TH2F* chipHitHistogram = f2DChipHitHistograms.getObject(board->getId())
                                                 ->getObject(opticalGroup->getId())
                                                 ->getObject(hybrid->getId())
                                                 ->getObject(chip->getId())
                                                 ->getSummary<HistContainer<TH2F>>()
                                                 .fTheHistogram;
                    uint16_t iChan_high = chip->getId() + 1 + (hybrid->getId() * NCHIPS_OT);
                    uint16_t iChan_low  = chip->getId() + (hybrid->getId() * NCHIPS_OT);

                    uint32_t chipOffset = (hybrid->getId() * HYBRID_CHANNELS_OT) + (chip->getId() * (NCHANNELS));

                    for(size_t iCh1 = chipChannelBoundaries[iChan_low]; iCh1 < chipChannelBoundaries[iChan_high]; iCh1++)
                    {
                        for(size_t iCh2 = chipChannelBoundaries[iChan_low]; iCh2 <= chipChannelBoundaries[iChan_high]; iCh2++)
                        {
                            chipHitHistogram->SetBinContent(
                                iCh1 - chipOffset, iCh2 - chipOffset, opticalGroup->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[iCh1][iCh2]);
                        }
                    }
                }
            }
        }
    }

    return true;
}


//========================================================================================================================

bool DQMHistogramOTCMNoise::fillChipHitPlots(DetectorDataContainer& theHitData){
    return fillChipHitPlots(theHitData,false);
}


bool DQMHistogramOTCMNoise::fillChipHitPlots(DetectorDataContainer& theHitData, bool pFitDistributions){
    const int cNChannels = NCHANNELS;
    for(auto board: theHitData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    TH1F* theHistogramEven = fChipHitHistogramsEven.getObject(board->getId())
                                                ->getObject(opticalGroup->getId())
                                                ->getObject(hybrid->getId())
                                                ->getObject(chip->getId())
                                                ->getSummary<HistContainer<TH1F>>()
                                                .fTheHistogram;
                    TH1F* theHistogramOdd  = fChipHitHistogramsOdd.getObject(board->getId())
                                                ->getObject(opticalGroup->getId())
                                                ->getObject(hybrid->getId())
                                                ->getObject(chip->getId())
                                                ->getSummary<HistContainer<TH1F>>()
                                                .fTheHistogram;
                    TH1F* theHistogramSum = fChipHitHistograms.getObject(board->getId())
                                                ->getObject(opticalGroup->getId())
                                                ->getObject(hybrid->getId())
                                                ->getObject(chip->getId())
                                                ->getSummary<HistContainer<TH1F>>()
                                                .fTheHistogram;

                    // fill the histogram from the vector, deconvoluting Even/Odd/Sum
                    auto cDataSummary = chip->getSummary<GenericDataArray<uint32_t,3*(cNChannels + 1)>>();
                    for(uint16_t iChan = 0; iChan < cNChannels + 1; iChan++) {
                        theHistogramEven->SetBinContent(iChan+1,cDataSummary[iChan]);
                        theHistogramOdd ->SetBinContent(iChan+1,cDataSummary[(cNChannels+1)   + iChan]);
                        theHistogramSum ->SetBinContent(iChan+1,cDataSummary[2*(cNChannels+1) + iChan]);
                    }
                    theHistogramEven->Sumw2();
                    theHistogramOdd->Sumw2();
                    theHistogramSum->Sumw2();
                    
                    if (pFitDistributions){
                        // do fitting
                        TF1* cChipFit = new TF1("chipFit", hitProbabilityFunction, 0, cNChannels + 1, 4);
                        fitCMNoise(theHistogramEven, cChipFit, cNChannels/2);
                        LOG(INFO) << BOLDRED << "FE " << hybrid->getId() << " CBC " << chip->getId() << " even strip common mode is " << fabs(cChipFit->GetParameter(1)) << "+/-" << fabs(cChipFit->GetParError(1)) << "%"
                                  << RESET;
                        fitCMNoise(theHistogramOdd, cChipFit, cNChannels/2);
                        LOG(INFO) << BOLDRED << "FE " << hybrid->getId() << " CBC " << chip->getId() << " odd strip common mode is " << fabs(cChipFit->GetParameter(1)) << "+/-" << fabs(cChipFit->GetParError(1)) << "%"
                                  << RESET;
                        fitCMNoise(theHistogramSum, cChipFit, cNChannels);
                        LOG(INFO) << BOLDRED << "FE " << hybrid->getId() << " CBC " << chip->getId() <<" common mode is " << fabs(cChipFit->GetParameter(1)) << "+/-" << fabs(cChipFit->GetParError(1)) << "%"
                                << RESET;
                    }
                }
            }
        }
    }
    return 1;
}


bool DQMHistogramOTCMNoise::fillHybridHitPlots(DetectorDataContainer& theHitData){
    const int cNChannels = HYBRID_CHANNELS_OT;
    for(auto board: theHitData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                TH1F* theHistogramEven = fHybridHitHistogramsEven.getObject(board->getId())
                                            ->getObject(opticalGroup->getId())
                                            ->getObject(hybrid->getId())
                                            ->getSummary<HistContainer<TH1F>>()
                                            .fTheHistogram;
                TH1F* theHistogramOdd  = fHybridHitHistogramsOdd.getObject(board->getId())
                                            ->getObject(opticalGroup->getId())
                                            ->getObject(hybrid->getId())
                                            ->getSummary<HistContainer<TH1F>>()
                                            .fTheHistogram;
                TH1F* theHistogramSum = fHybridHitHistograms.getObject(board->getId())
                                            ->getObject(opticalGroup->getId())
                                            ->getObject(hybrid->getId())
                                            ->getSummary<HistContainer<TH1F>>()
                                            .fTheHistogram;

                // fill the histogram from the vector, deconvoluting Even/Odd/Sum
                auto cDataSummary = hybrid->getSummary<GenericDataArray<uint32_t, 3*(cNChannels + 1)>>();
                for(uint16_t iChan = 0; iChan < cNChannels + 1; iChan++) {
                    theHistogramEven->SetBinContent(iChan+1,cDataSummary[iChan]);
                    theHistogramOdd ->SetBinContent(iChan+1,cDataSummary[(cNChannels+1)   + iChan]);
                    theHistogramSum ->SetBinContent(iChan+1,cDataSummary[2*(cNChannels+1) + iChan]);
                }
                theHistogramEven->Sumw2();
                theHistogramOdd->Sumw2();
                theHistogramSum->Sumw2();
            }
        }
    }
    return 1;
}


bool DQMHistogramOTCMNoise::fillModuleHitPlots(DetectorDataContainer& theHitData){
    const int cNChannels = TOTAL_CHANNELS_OT;
    for(auto board: theHitData)
    {
        for(auto opticalGroup: *board)
        {
            TH1F* theHistogramEven = fModuleHitHistogramsEven.getObject(board->getId())
                                        ->getObject(opticalGroup->getId())
                                        ->getSummary<HistContainer<TH1F>>()
                                        .fTheHistogram;
            TH1F* theHistogramOdd  = fModuleHitHistogramsOdd.getObject(board->getId())
                                        ->getObject(opticalGroup->getId())
                                        ->getSummary<HistContainer<TH1F>>()
                                        .fTheHistogram;
            TH1F* theHistogramSum = fModuleHitHistograms.getObject(board->getId())
                                        ->getObject(opticalGroup->getId())
                                        ->getSummary<HistContainer<TH1F>>()
                                        .fTheHistogram;

            // fill the histogram from the vector, deconvoluting Even/Odd/Sum
            auto cDataSummary = opticalGroup->getSummary<GenericDataArray<uint32_t, 3*(cNChannels + 1)>>();
            for(uint16_t iChan = 0; iChan < cNChannels + 1; iChan++) {
                theHistogramEven->SetBinContent(iChan+1,cDataSummary[iChan]);
                theHistogramOdd ->SetBinContent(iChan+1,cDataSummary[(cNChannels+1)   + iChan]);
                theHistogramSum ->SetBinContent(iChan+1,cDataSummary[2*(cNChannels+1) + iChan]);
            }
            theHistogramEven->Sumw2();
            theHistogramOdd->Sumw2();
            theHistogramSum->Sumw2();
        }
    }
    return 1;
}


bool DQMHistogramOTCMNoise::fillSensorChipCorrelationPlots(DetectorDataContainer& theSensorData){
    for(auto board: theSensorData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {   
                    TH2F* h2DChipSensorCorrelation = f2DChipSensorCorrelation.getObject(board->getId())
                                ->getObject(opticalGroup->getId())
                                ->getObject(hybrid->getId())
                                ->getObject(chip->getId())
                                ->getSummary<HistContainer<TH2F>>()
                                .fTheHistogram;
                    for(uint16_t iCh1 = 0; iCh1 < NCHANNELS/2 + 1 ; iCh1++){
                        for(uint16_t iCh2 = 0; iCh2 < NCHANNELS/2 + 1; iCh2++){
                            h2DChipSensorCorrelation->SetBinContent(iCh1, iCh2, chip->getSummary<GenericDataArray<uint32_t, NCHANNELS/2 + 1,NCHANNELS/2 + 1>>()[iCh1][iCh2]);
                        }
                    }
                    h2DChipSensorCorrelation->Sumw2(0);
                }
            }
        }
    }
    
    return 1;
}

bool DQMHistogramOTCMNoise::fillSensorHybridCorrelationPlots(DetectorDataContainer& theSensorData){
    for(auto board: theSensorData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                TH2F* h2DHybridSensorCorrelation = f2DHybridSensorCorrelation.getObject(board->getId())
                            ->getObject(opticalGroup->getId())
                            ->getObject(hybrid->getId())
                            ->getSummary<HistContainer<TH2F>>()
                            .fTheHistogram;
                auto thisDataContainer = hybrid->getSummary<GenericDataArray<uint32_t, HYBRID_CHANNELS_OT/2 + 1,HYBRID_CHANNELS_OT/2 + 1>>();
                for(uint16_t iCh1 = 0; iCh1 < HYBRID_CHANNELS_OT/2 + 1; iCh1++){
                    for(uint16_t iCh2 = 0; iCh2 < HYBRID_CHANNELS_OT/2 + 1; iCh2++){
                        h2DHybridSensorCorrelation->SetBinContent(iCh1, iCh2, thisDataContainer[iCh1][iCh2]);
                    }
                }
                h2DHybridSensorCorrelation->Sumw2(0);
            }
        }
    }
    
    return 1;
}

bool DQMHistogramOTCMNoise::fillSensorModuleCorrelationPlots(DetectorDataContainer& theSensorData){
    for(auto board: theSensorData)
    {
        for(auto opticalGroup: *board)
        {
            TH2F* h2DModuleSensorCorrelation = f2DModuleSensorCorrelation.getObject(board->getId())
                        ->getObject(opticalGroup->getId())
                        ->getSummary<HistContainer<TH2F>>()
                        .fTheHistogram;
            for(uint16_t iCh1 = 0; iCh1 < TOTAL_CHANNELS_OT/2 + 1; iCh1++){
                for(uint16_t iCh2 = 0; iCh2 < TOTAL_CHANNELS_OT/2 + 1; iCh2++){
                    h2DModuleSensorCorrelation->SetBinContent(iCh1, iCh2, opticalGroup->getSummary<GenericDataArray<uint32_t,TOTAL_CHANNELS_OT/2 + 1,TOTAL_CHANNELS_OT/2 + 1>>()[iCh1][iCh2]);
                }
            }
            h2DModuleSensorCorrelation->Sumw2(0);
        }
    }
    
    return 1;
}

bool DQMHistogramOTCMNoise::fillHybridCorrelationPlots(DetectorDataContainer& theHybridData){
        //Fill in hybrid Data:
    for(auto board: theHybridData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    TH2F* h2DChipCorrelation = f2DChipCorrelation.getObject(board->getId())
                                ->getObject(opticalGroup->getId())
                                ->getObject(hybrid->getId())
                                ->getObject(chip->getId())
                                ->getSummary<HistContainer<TH2F>>()
                                .fTheHistogram;

                    for(uint16_t iCh1 = 0; iCh1 < NCHANNELS + 1; iCh1++){
                        for(uint16_t iCh2 = 0; iCh2 < HYBRID_CHANNELS_OT + 1; iCh2++){
                            h2DChipCorrelation->SetBinContent(iCh1,iCh2,chip->getSummary<GenericDataArray<uint32_t,NCHANNELS + 1,HYBRID_CHANNELS_OT + 1>>()[iCh1][iCh2]);
                        }
                    }
                    h2DChipCorrelation->Sumw2(0);

                }
            }
            TH2F* h2DHybridCorrelation = f2DHybridCorrelation.getObject(board->getId())
                ->getObject(opticalGroup->getId())
                ->getSummary<HistContainer<TH2F>>()
                .fTheHistogram;

            for(uint16_t iCh1 = 0; iCh1 < HYBRID_CHANNELS_OT + 1; iCh1++){
                for(uint16_t iCh2 = 0; iCh2 < HYBRID_CHANNELS_OT + 1; iCh2++){
                    h2DHybridCorrelation->SetBinContent(iCh1,iCh2,opticalGroup->getSummary<GenericDataArray<uint32_t,HYBRID_CHANNELS_OT + 1,HYBRID_CHANNELS_OT + 1>>()[iCh1][iCh2]);
                }
            }
            h2DHybridCorrelation->Sumw2(0);

        }
    }


    return 1;
}

bool DQMHistogramOTCMNoise::fillHitProfile(DetectorDataContainer& theHitData){
    return 1;
}



//========================================================================================================================

// this used to be in CMFits.h -- Written by G. Auzinger
bool DQMHistogramOTCMNoise::fitCMNoise(TH1F* pHitCountHist, TF1* pFit, uint32_t pRange)
{
    // Reset uncertainties on input histogram
    pHitCountHist->Sumw2(0);
    pHitCountHist->Sumw2(1);
    // First-order approximation
    double prob = pHitCountHist->GetMean()*1. / pRange;//pHitCountHist->GetNbinsX();
    // double prob = pHitCountHist->GetMean();

    // retrieve the threshold from the maximum of the actual nhit distribution
    double threshold = inverse_hitProbability(prob);
    std::cout<<"Prob is:"<<prob<<std::endl;
    std::cout<<"Threshold is:"<<threshold<<std::endl;

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
