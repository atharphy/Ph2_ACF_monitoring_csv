#include "DQMUtils/DQMHistogramOTPScommonNoise.h"
#include "HWDescription/ReadoutChip.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"

using namespace Ph2_HwDescription;

//========================================================================================================================
DQMHistogramOTPScommonNoise::DQMHistogramOTPScommonNoise() {}

//========================================================================================================================
DQMHistogramOTPScommonNoise::~DQMHistogramOTPScommonNoise() {}

//========================================================================================================================
void DQMHistogramOTPScommonNoise::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;

    // double theNumberOfEvents    = findValueInSettings<double>(pSettingsMap, "OTPScommonNoise_NumberOfEvents", 10000);

    auto        selectSSAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2); };
    std::string selectSSAfunctionName = "SelectSSAfunction";

    auto        selectMPAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string selectMPAfunctionName = "SelectMPAfunction";

    fDetectorContainer->addReadoutChipQueryFunction(selectSSAfunction, selectSSAfunctionName);

    HistContainer<TH1F> hSSAHits("CommonNoiseHits", "Common noise hits", MAXCICCHANNELS + 2, -0.5, MAXCICCHANNELS + 1 + 0.5);
    hSSAHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits");
    hSSAHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");
    hSSAHits.fTheHistogram->GetXaxis()->SetRangeUser(0, 120);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fStripHitHistograms, hSSAHits);

    HistContainer<TH1F> hStripHybridHits("CommonNoiseHitsStrip", "Common noise hits strip", MAXCICCHANNELS + 2, -0.5, MAXCICCHANNELS + 1 + 0.5);
    hStripHybridHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits");
    hStripHybridHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStripHybridHitHistograms, hStripHybridHits);

    HistContainer<TH1F> hStripModuleHits("CommonNoiseHitsStrip", "Common noise hits strip", MAXCICCHANNELS + 2, -0.5, MAXCICCHANNELS + 1 + 0.5);
    hStripModuleHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits");
    hStripModuleHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fStripModuleHitHistograms, hStripModuleHits);

    fDetectorContainer->removeReadoutChipQueryFunction(selectSSAfunctionName);

    fDetectorContainer->addReadoutChipQueryFunction(selectMPAfunction, selectMPAfunctionName);

    HistContainer<TH1F> hMPAHits("CommonNoiseHits", "Common noise hits", MAXCICCHANNELS + 2, -0.5, MAXCICCHANNELS + 1 + 0.5);
    hMPAHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits");
    hMPAHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");
    hMPAHits.fTheHistogram->GetXaxis()->SetRangeUser(0, 120);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fPixelHitHistograms, hMPAHits);

    HistContainer<TH1F> hPixelHybridHits("CommonNoiseHitsPixel", "Common noise hits pixel", MAXCICCHANNELS + 2, -0.5, MAXCICCHANNELS + 1 + 0.5);
    hPixelHybridHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits");
    hPixelHybridHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fPixelHybridHitHistograms, hPixelHybridHits);

    HistContainer<TH1F> hPixelModuleHits("CommonNoiseHitsPixel", "Common noise hits pixel", MAXCICCHANNELS + 2, -0.5, MAXCICCHANNELS + 1 + 0.5);
    hPixelModuleHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits");
    hPixelModuleHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fPixelModuleHitHistograms, hPixelModuleHits);

    for(uint8_t chip = NCHIPS_OT; chip < 2 * NCHIPS_OT; chip++)
    {
        HistContainer<TH2F> hSSAtoMPAcorrelation(Form("SSA(%d)toMPA(%d)_CommonNoiseCorrelation", chip - NCHIPS_OT, chip),
                                                 Form("SSA(%d) to MPA(%d) common noise correlation", chip - NCHIPS_OT, chip),
                                                 MAXCICCHANNELS + 2,
                                                 -0.5,
                                                 MAXCICCHANNELS + 1 + 0.5,
                                                 MAXCICCHANNELS + 2,
                                                 -0.5,
                                                 MAXCICCHANNELS + 1 + 0.5);
        hSSAtoMPAcorrelation.fTheHistogram->GetYaxis()->SetTitle("Number of hits MPA");
        hSSAtoMPAcorrelation.fTheHistogram->GetXaxis()->SetTitle("Number of hits SSA");
        hSSAtoMPAcorrelation.fTheHistogram->GetXaxis()->SetRangeUser(0, 120);
        hSSAtoMPAcorrelation.fTheHistogram->GetYaxis()->SetRangeUser(0, 120);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fSSAtoMPAcorrelation[chip], hSSAtoMPAcorrelation);
    }

    fDetectorContainer->removeReadoutChipQueryFunction(selectMPAfunctionName);

    HistContainer<TH2F> hStripPixelHybridHits(
        "CommonNoiseStripPixelCorrelation", "Common noise strip pixel correlation", MAXCICCHANNELS + 2, -0.5, MAXCICCHANNELS + 1 + 0.5, MAXCICCHANNELS + 2, -0.5, MAXCICCHANNELS + 1 + 0.5);
    hStripPixelHybridHits.fTheHistogram->GetYaxis()->SetTitle("Number of hits pixels");
    hStripPixelHybridHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits strips");
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStripPixelHybridHistograms, hStripPixelHybridHits);

    HistContainer<TH2F> hStripPixelModuleHits(
        "CommonNoiseStripPixelCorrelation", "Common noise strip pixel correlation", MAXCICCHANNELS + 2, -0.5, MAXCICCHANNELS + 1 + 0.5, MAXCICCHANNELS + 2, -0.5, MAXCICCHANNELS + 1 + 0.5);
    hStripPixelModuleHits.fTheHistogram->GetYaxis()->SetTitle("Number of hits pixels");
    hStripPixelModuleHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits strips");
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fStripPixelModuleHistograms, hStripPixelModuleHits);
    // SoC utilities only - END
}

//========================================================================================================================
void DQMHistogramOTPScommonNoise::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTPScommonNoise::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTPScommonNoise::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization theStripChipHitContainerSerialization("OTPScommonNoiseStripChipHit");
    if(theStripChipHitContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTPScommonNoiseStripChipHit!!!!!\n";
        DetectorDataContainer theDetectorData = theStripChipHitContainerSerialization.deserializeChipContainer<EmptyContainer, GenericDataArray<uint32_t, (MAXCICCHANNELS + 1)>>(fDetectorContainer);
        // Filling the histograms
        fillChipHitPlots(theDetectorData);
        return true;
    }

    ContainerSerialization thePixelChipHitContainerSerialization("OTPScommonNoisePixelChipHit");
    if(thePixelChipHitContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTPScommonNoisePixelChipHit!!!!!\n";
        DetectorDataContainer theDetectorData = thePixelChipHitContainerSerialization.deserializeChipContainer<EmptyContainer, GenericDataArray<uint32_t, (MAXCICCHANNELS + 1)>>(fDetectorContainer);
        // Filling the histograms
        fillChipHitPlots(theDetectorData);
        return true;
    }

    ContainerSerialization theStripHybridHitContainerSerialization("OTPScommonNoiseStripHybridHit");
    if(theStripHybridHitContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTPScommonNoiseStripHybridHit!!!!!\n";
        bool                  isSSA;
        DetectorDataContainer theDetectorData =
            theStripHybridHitContainerSerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, (MAXCICCHANNELS + 1)>>(fDetectorContainer, isSSA);
        // Filling the histograms
        fillHybridHitPlots(theDetectorData, isSSA);
        return true;
    }

    ContainerSerialization thePixelHybridHitContainerSerialization("OTPScommonNoisePixelHybridHit");
    if(thePixelHybridHitContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTPScommonNoisePixelHybridHit!!!!!\n";
        bool                  isSSA;
        DetectorDataContainer theDetectorData =
            thePixelHybridHitContainerSerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, (MAXCICCHANNELS + 1)>>(fDetectorContainer, isSSA);
        // Filling the histograms
        fillHybridHitPlots(theDetectorData, isSSA);
        return true;
    }

    ContainerSerialization theStripModuleHitContainerSerialization("OTPScommonNoiseStripModuleHit");
    if(theStripModuleHitContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTPScommonNoiseStripModuleHit!!!!!\n";
        bool                  isSSA;
        DetectorDataContainer theDetectorData =
            theStripModuleHitContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, (MAXCICCHANNELS * 2 + 1)>>(
                fDetectorContainer, isSSA);
        // Filling the histograms
        fillModuleHitPlots(theDetectorData, isSSA);
        return true;
    }

    ContainerSerialization thePixelModuleHitContainerSerialization("OTPScommonNoisePixelModuleHit");
    if(thePixelModuleHitContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTPScommonNoisePixelModuleHit!!!!!\n";
        bool                  isSSA;
        DetectorDataContainer theDetectorData =
            thePixelModuleHitContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, (MAXCICCHANNELS * 2 + 1)>>(
                fDetectorContainer, isSSA);
        // Filling the histograms
        fillModuleHitPlots(theDetectorData, isSSA);
        return true;
    }

    ContainerSerialization theSSAMPACorrelationContainerSerialization("OTPScommonNoiseSSAMPACorrelation");
    if(theSSAMPACorrelationContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTPScommonNoiseSSAMPACorrelation!!!!!\n";
        DetectorDataContainer theDetectorData =
            theSSAMPACorrelationContainerSerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, (MAXCICCHANNELS + 1), (MAXCICCHANNELS + 1)>>(
                fDetectorContainer);
        // Filling the histograms
        fillSSAtoMPACorrelationPlots(theDetectorData);
        return true;
    }

    ContainerSerialization theStripPixelHybridContainerSerialization("OTPScommonNoiseStripPixelHybridCorrelation");
    if(theStripPixelHybridContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTPScommonNoiseStripPixelHybridCorrelation!!!!!\n";
        DetectorDataContainer theDetectorData =
            theStripPixelHybridContainerSerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, (MAXCICCHANNELS + 1), (MAXCICCHANNELS + 1)>>(
                fDetectorContainer);
        // Filling the histograms
        fillStripPixelHybridCorrelationPlots(theDetectorData);
        return true;
    }

    ContainerSerialization theStripPixelModuleContainerSerialization("OTPScommonNoiseStripPixelModuleCorrelation");
    if(theStripPixelModuleContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTPScommonNoiseStripPixelModuleCorrelation!!!!!\n";
        DetectorDataContainer theDetectorData =
            theStripPixelModuleContainerSerialization
                .deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, (MAXCICCHANNELS * 2 + 1), (MAXCICCHANNELS * 2 + 1)>>(fDetectorContainer);
        // Filling the histograms
        fillStripPixelModuleCorrelationPlots(theDetectorData);
        return true;
    }

    // the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    //  for this stream)
    return false;
    // SoC utilities only - END
}

void DQMHistogramOTPScommonNoise::fillChipHitPlots(DetectorDataContainer& theHitData)
{
    for(auto board: theHitData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    if(!chip->hasSummary()) continue;
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getObject(chip->getId());
                    if(theReadoutChip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        TH1F* theHistogram = fStripHitHistograms.getObject(board->getId())
                                                 ->getObject(opticalGroup->getId())
                                                 ->getObject(hybrid->getId())
                                                 ->getObject(chip->getId())
                                                 ->getSummary<HistContainer<TH1F>>()
                                                 .fTheHistogram;

                        fillEventsVsHitsHist<MAXCICCHANNELS + 1>(chip, *theHistogram);
                    }
                    else if(theReadoutChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        TH1F* theHistogram = fPixelHitHistograms.getObject(board->getId())
                                                 ->getObject(opticalGroup->getId())
                                                 ->getObject(hybrid->getId())
                                                 ->getObject(chip->getId())
                                                 ->getSummary<HistContainer<TH1F>>()
                                                 .fTheHistogram;

                        fillEventsVsHitsHist<MAXCICCHANNELS + 1>(chip, *theHistogram);
                    }
                }
            }
        }
    }
}

void DQMHistogramOTPScommonNoise::fillHybridHitPlots(DetectorDataContainer& theHitData, bool isStrip)
{
    for(auto board: theHitData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;
                if(isStrip)
                {
                    TH1F* theHistogram =
                        fStripHybridHitHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    fillEventsVsHitsHist<MAXCICCHANNELS + 1>(hybrid, *theHistogram);
                }
                else
                {
                    TH1F* theHistogram =
                        fPixelHybridHitHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    fillEventsVsHitsHist<MAXCICCHANNELS + 1>(hybrid, *theHistogram);
                }
            }
        }
    }
}

void DQMHistogramOTPScommonNoise::fillModuleHitPlots(DetectorDataContainer& theHitData, bool isStrip)
{
    for(auto board: theHitData)
    {
        for(auto opticalGroup: *board)
        {
            if(!opticalGroup->hasSummary()) continue;
            if(isStrip)
            {
                TH1F* theHistogram = fStripModuleHitHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                fillEventsVsHitsHist<MAXCICCHANNELS * 2 + 1>(opticalGroup, *theHistogram);
            }
            else
            {
                TH1F* theHistogram = fPixelModuleHitHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                fillEventsVsHitsHist<MAXCICCHANNELS * 2 + 1>(opticalGroup, *theHistogram);
            }
        }
    }
}

void DQMHistogramOTPScommonNoise::fillSSAtoMPACorrelationPlots(DetectorDataContainer& theSensorData)
{
    for(auto board: theSensorData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    if(!chip->hasSummary()) continue;
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getObject(chip->getId());
                    if(theReadoutChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        TH2F* h2DChipSensorCorrelation = fSSAtoMPAcorrelation[chip->getId()]
                                                             .getObject(board->getId())
                                                             ->getObject(opticalGroup->getId())
                                                             ->getObject(hybrid->getId())
                                                             ->getSummary<HistContainer<TH2F>>()
                                                             .fTheHistogram;
                        fillCorrelationHist<MAXCICCHANNELS + 1, MAXCICCHANNELS + 1>(chip, h2DChipSensorCorrelation);
                    }
                }
            }
        }
    }
}

void DQMHistogramOTPScommonNoise::fillStripPixelHybridCorrelationPlots(DetectorDataContainer& theSensorData)
{
    for(auto board: theSensorData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;
                TH2F* h2DHybridCorrelation =
                    fStripPixelHybridHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                fillCorrelationHist<MAXCICCHANNELS + 1, MAXCICCHANNELS + 1>(hybrid, h2DHybridCorrelation);
            }
        }
    }
}

void DQMHistogramOTPScommonNoise::fillStripPixelModuleCorrelationPlots(DetectorDataContainer& theSensorData)
{
    for(auto board: theSensorData)
    {
        for(auto opticalGroup: *board)
        {
            if(!opticalGroup->hasSummary()) continue;
            TH2F* h2DModuleCorrelation = fStripPixelModuleHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
            fillCorrelationHist<MAXCICCHANNELS * 2 + 1, MAXCICCHANNELS * 2 + 1>(opticalGroup, h2DModuleCorrelation);
        }
    }
}
