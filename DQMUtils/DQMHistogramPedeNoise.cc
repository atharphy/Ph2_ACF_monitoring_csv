/*!
        \file                DQMHistogramPedeNoise.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
 */

#include "DQMUtils/DQMHistogramPedeNoise.h"
#include "HWDescription/ReadoutChip.h"
#include "RootUtils/HistContainer.h"
#include "RootUtils/RootContainerFactory.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/EmptyContainer.h"
#include "Utils/Occupancy.h"
#include "Utils/ThresholdAndNoise.h"
#include "Utils/Utilities.h"

using namespace Ph2_HwDescription;

//========================================================================================================================
DQMHistogramPedeNoise::DQMHistogramPedeNoise() {}

//========================================================================================================================
DQMHistogramPedeNoise::~DQMHistogramPedeNoise() {}

//========================================================================================================================
void DQMHistogramPedeNoise::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // copy detector structrure
    fDetectorContainer = &theDetectorStructure;

    // find front-end types
    for(auto cBoard: *fDetectorContainer)
    {
        auto cFrontEndTypes = cBoard->connectedFrontEndTypes();
        fWithCBC            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::CBC3) != cFrontEndTypes.end();
        fWithSSA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA2) != cFrontEndTypes.end();
        fWithMPA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA2) != cFrontEndTypes.end();
    }

    std::vector<FrontEndType> cStripTypes             = {FrontEndType::CBC3, FrontEndType::SSA2};
    std::vector<FrontEndType> cPixelTypes             = {FrontEndType::MPA2};
    auto                      selectStripChipFunction = [cStripTypes](const ChipContainer* pChip)
    { return (std::find(cStripTypes.begin(), cStripTypes.end(), static_cast<const ReadoutChip*>(pChip)->getFrontEndType()) != cStripTypes.end()); };
    auto selectPixelChipFunction = [cPixelTypes](const ChipContainer* pChip)
    { return (std::find(cPixelTypes.begin(), cPixelTypes.end(), static_cast<const ReadoutChip*>(pChip)->getFrontEndType()) != cPixelTypes.end()); };

    // find maximum number of channels
    std::vector<size_t> cNPixelChannels(0), cNStripChannels(0);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto cNChannels = theDetectorStructure.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->size();
                    auto cType      = cChip->getFrontEndType();
                    if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA2) { cNStripChannels.push_back(cNChannels); }
                    else if(cType == FrontEndType::MPA2) { cNPixelChannels.push_back(cNChannels); }
                }
            }
        }
    }
    if(fWithCBC || fWithSSA) { fNStripChannels = *std::max_element(std::begin(cNStripChannels), std::end(cNStripChannels)); }
    if(fWithMPA) { fNPixelChannels = *std::max_element(std::begin(cNPixelChannels), std::end(cNPixelChannels)); }

    auto cSetting = pSettingsMap.find("PlotSCurves");
    fPlotSCurves  = (cSetting != std::end(pSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;
    cSetting      = pSettingsMap.find("FitSCurves");
    fFitSCurves   = (cSetting != std::end(pSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;
    if(fFitSCurves) fPlotSCurves = true;

    std::string queryFunctionName = "ChipType";
    if(fWithCBC || fWithSSA)
    {
        // Set query function to only include strip chips in the data container
        fDetectorContainer->addReadoutChipQueryFunction(selectStripChipFunction, queryFunctionName);

        if(fPlotSCurves)
        {
            uint16_t nYbins = (fWithSSA) ? 255 : 1024;
            float    minY   = -0.5;
            float    maxY   = (fWithSSA) ? 254.5 : 1023.5;

            HistContainer<TH2F> theTH2FChipStripSCurve("SCurve", "SCurve", fNStripChannels, -0.5, fNStripChannels - 0.5, nYbins, minY, maxY);
            RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorChipStripSCurveHistograms, theTH2FChipStripSCurve);

            if(fFitSCurves)
            {
                HistContainer<TH1F> theTH1FChannelStripSCurveContainer("SCurve", "SCurve", nYbins, minY, maxY);
                RootContainerFactory::bookChannelHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChannelStripSCurveHistograms, theTH1FChannelStripSCurveContainer);
            }
        }

        // Pedestal
        HistContainer<TH1F> theTH1FChipStripPedestalContainer("PedestalDistribution", "PedestalDistribution", 2048, -0.5, 1023.5);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipStripPedestalHistograms, theTH1FChipStripPedestalContainer);
        //
        HistContainer<TH1F> theTH1FChannelStripPedestalContainer("ChannelPedestalDistribution", "ChannelPedestal", fNStripChannels, -0.5, float(fNStripChannels) - 0.5);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChannelStripPedestalHistograms, theTH1FChannelStripPedestalContainer);

        // Noise
        HistContainer<TH1F> theTH1FHybridStripNoiseContainer("HybridStripNoiseDistribution", "HybridStripNoise", fNStripChannels * 8, -0.5, float(fNStripChannels) * 8 - 0.5);
        RootContainerFactory::bookHybridHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorHybridStripNoiseHistograms, theTH1FHybridStripNoiseContainer);
        //
        HistContainer<TH1F> theTH1FChipStripNoiseContainer("NoiseDistribution", "NoiseDistribution", 200, 0., 20.);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipStripNoiseHistograms, theTH1FChipStripNoiseContainer);
        //
        HistContainer<TH1F> theTH1FChannelStripNoiseContainer("ChannelNoiseDistribution", "ChannelNoise", fNStripChannels, -0.5, float(fNStripChannels) - 0.5);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChannelStripNoiseHistograms, theTH1FChannelStripNoiseContainer);

        if(fWithCBC)
        {
            // Strip Noise Even
            HistContainer<TH1F> theTH1FHybridStripNoiseEvenContainer("HybridNoiseEvenDistribution", "HybridNoiseEven", fNStripChannels * 4, -0.5, float(fNStripChannels) * 4 - 0.5);
            RootContainerFactory::bookHybridHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorHybridStripNoiseEvenHistograms, theTH1FHybridStripNoiseEvenContainer);
            //
            HistContainer<TH1F> theTH1FChannelStripNoiseEvenContainer("ChannelNoiseEvenDistribution", "ChannelNoiseEven", fNStripChannels / 2, -0.5, float(fNStripChannels / 2) - 0.5);
            RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChannelStripNoiseEvenHistograms, theTH1FChannelStripNoiseEvenContainer);

            // Strip Noise Odd
            HistContainer<TH1F> theTH1FHybridStripNoiseOddContainer("HybridNoiseOddDistribution", "HybridNoiseOdd", fNStripChannels * 4, -0.5, float(fNStripChannels) * 4 - 0.5);
            RootContainerFactory::bookHybridHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorHybridStripNoiseOddHistograms, theTH1FHybridStripNoiseOddContainer);
            //
            HistContainer<TH1F> theTH1FChannelStripNoiseOddContainer("ChannelNoiseOddDistribution", "ChannelNoiseOdd", fNStripChannels / 2, -0.5, float(fNStripChannels / 2) - 0.5);
            RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChannelStripNoiseOddHistograms, theTH1FChannelStripNoiseOddContainer);
        }

        // Validation
        HistContainer<TH1F> theTH1FStripValidationContainer("Occupancy", "Occupancy", fNStripChannels, -0.5, float(fNStripChannels) - 0.5);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorStripValidationHistograms, theTH1FStripValidationContainer);

        // Reset query function from only including strip chips in the data container
        fDetectorContainer->removeReadoutChipQueryFunction(queryFunctionName);
    }

    if(fWithMPA)
    {
        // Set query function to only include strip chips in the data container
        fDetectorContainer->addReadoutChipQueryFunction(selectPixelChipFunction, queryFunctionName);

        if(fPlotSCurves)
        {
            uint16_t nYbins = 255;
            float    minY   = -0.5;
            float    maxY   = 254.5;

            HistContainer<TH2F> theTH2FChipPixelSCurve("SCurve", "SCurve", fNPixelChannels, -0.5, fNPixelChannels - 0.5, nYbins, minY, maxY);
            RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorChipPixelSCurveHistograms, theTH2FChipPixelSCurve);

            if(fFitSCurves)
            {
                HistContainer<TH1F> theTH1FChannelPixelSCurveContainer("SCurve", "SCurve", nYbins, minY, maxY);
                RootContainerFactory::bookChannelHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChannelPixelSCurveHistograms, theTH1FChannelPixelSCurveContainer);
            }
        }

        // Pedestal
        HistContainer<TH1F> theTH1FChipPixelPedestalContainer("PedestalDistribution", "PedestalDistribution", 2048, -0.5, 1023.5);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipPixelPedestalHistograms, theTH1FChipPixelPedestalContainer);
        //
        HistContainer<TH1F> theTH1FChannelPixelPedestalContainer("ChannelPedestalDistribution", "ChannelPedestal", fNPixelChannels, -0.5, float(fNPixelChannels) - 0.5);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChannelPixelPedestalHistograms, theTH1FChannelPixelPedestalContainer);

        // Noise
        HistContainer<TH1F> theTH1FHybridPixelNoiseContainer("HybridPixelNoiseDistribution", "HybridPixelNoise", fNPixelChannels * 8, -0.5, float(fNPixelChannels) * 8 - 0.5);
        RootContainerFactory::bookHybridHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorHybridPixelNoiseHistograms, theTH1FHybridPixelNoiseContainer);
        //
        HistContainer<TH1F> theTH1FChipPixelNoiseContainer("NoiseDistribution", "NoiseDistribution", 200, 0., 20.);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipPixelNoiseHistograms, theTH1FChipPixelNoiseContainer);
        // 1D pixel noise
        HistContainer<TH1F> theTH1FChannelPixelNoiseContainer("ChannelNoiseDistribution", "ChannelNoise", fNPixelChannels, -0.5, float(fNPixelChannels) - 0.5);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChannelPixelNoiseHistograms, theTH1FChannelPixelNoiseContainer);
        // 2D Pixel Noise
        HistContainer<TH2F> theTH2FChannel2DPixelNoiseContainer("2DPixelNoise", "2DChannelNoise", 120, -0.5, float(120) - 0.5, fNPixelChannels / 120, -0.5, float(fNPixelChannels / 120) - 0.5);
        RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorChannel2DPixelNoiseHistograms, theTH2FChannel2DPixelNoiseContainer);

        // Validation
        HistContainer<TH1F> theTH1FPixelValidationContainer("Occupancy", "Occupancy", fNPixelChannels, -0.5, float(fNPixelChannels) - 0.5);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorPixelValidationHistograms, theTH1FPixelValidationContainer);

        // Reset query function from only including strip chips in the data container
        fDetectorContainer->removeReadoutChipQueryFunction(queryFunctionName);
    }

    // SCurve
    if(fPlotSCurves && fFitSCurves) { ContainerFactory::copyAndInitStructure<ThresholdAndNoise>(theDetectorStructure, fThresholdAndNoiseContainer); }

    // Hybrid Noise
    HistContainer<TH1F> theTH1FHybridNoiseContainer("HybridNoiseDistribution", "HybridNoiseDistribution", 200, 0., 20.);
    RootContainerFactory::bookHybridHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorHybridNoiseHistograms, theTH1FHybridNoiseContainer);
}

//========================================================================================================================
bool DQMHistogramPedeNoise::fill(std::string& inputStream)
{
    ContainerSerialization theSCurveSerialization("PedeNoiseSCurve");
    ContainerSerialization theThresholdAndNoiseSerialization("PedeNoiseThresholdAndNoise");
    ContainerSerialization theValidationSerialization("PedeNoiseValidation");

    if(theSCurveSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched PedeNoise SCurve!!!!!\n";
        uint16_t              cStripValue, cPixelValue;
        DetectorDataContainer theDetectorData = theSCurveSerialization.deserializeHybridContainer<Occupancy, Occupancy, Occupancy>(fDetectorContainer, cStripValue, cPixelValue);
        fillSCurvePlots(cStripValue, cPixelValue, theDetectorData);
        return true;
    }
    if(theThresholdAndNoiseSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched PedeNoise Threshold And Noise!!!!!\n";
        DetectorDataContainer theDetectorData = theThresholdAndNoiseSerialization.deserializeHybridContainer<ThresholdAndNoise, ThresholdAndNoise, ThresholdAndNoise>(fDetectorContainer);
        fillPedestalAndNoisePlots(theDetectorData);
        return true;
    }
    if(theValidationSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched PedeNoise Validation!!!!!\n";
        DetectorDataContainer theDetectorData = theValidationSerialization.deserializeHybridContainer<Occupancy, Occupancy, Occupancy>(fDetectorContainer);
        fillValidationPlots(theDetectorData);
        return true;
    }

    return false;
}

//========================================================================================================================
void DQMHistogramPedeNoise::process()
{
    if(fFitSCurves) fitSCurves();

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                std::string validationCanvasName = "Validation_B_" + std::to_string(cBoard->getId()) + "_O_" + std::to_string(cOpticalGroup->getId()) + "_H_" + std::to_string(cHybrid->getId());
                std::string pedeNoiseCanvasName  = "PedeNoise_B_" + std::to_string(cBoard->getId()) + "_O_" + std::to_string(cOpticalGroup->getId()) + "_H_" + std::to_string(cHybrid->getId());

                TCanvas* cValidation = new TCanvas(validationCanvasName.data(), validationCanvasName.data(), 0, 0, 650, fPlotSCurves ? 900 : 650);
                TCanvas* cPedeNoise  = new TCanvas(pedeNoiseCanvasName.data(), pedeNoiseCanvasName.data(), 670, 0, 650, 650);

                cValidation->Divide(cHybrid->size(), fPlotSCurves ? 3 : 2);
                cPedeNoise->Divide(cHybrid->size(), 2);

                for(auto cChip: *cHybrid)
                {
                    auto cType = cChip->getFrontEndType();
                    if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA2)
                    {
                        cValidation->cd(cChip->getId() + 1 + cHybrid->size() * 0);
                        TH1F* validationHistogram = fDetectorStripValidationHistograms.getObject(cBoard->getId())
                                                        ->getObject(cOpticalGroup->getId())
                                                        ->getObject(cHybrid->getId())
                                                        ->getObject(cChip->getId())
                                                        ->getSummary<HistContainer<TH1F>>()
                                                        .fTheHistogram;
                        validationHistogram->SetStats(false);
                        validationHistogram->DrawCopy();
                        gPad->SetLogy();

                        cPedeNoise->cd(cChip->getId() + 1 + cHybrid->size() * 1);
                        fDetectorChipStripPedestalHistograms.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getObject(cChip->getId())
                            ->getSummary<HistContainer<TH1F>>()
                            .fTheHistogram->DrawCopy();

                        cPedeNoise->cd(cChip->getId() + 1 + cHybrid->size() * 0);
                        fDetectorChipStripNoiseHistograms.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getObject(cChip->getId())
                            ->getSummary<HistContainer<TH1F>>()
                            .fTheHistogram->DrawCopy();

                        if(fWithCBC)
                        {
                            cValidation->cd(cChip->getId() + 1 + cHybrid->size() * 1);
                            TH1F* cChannelStripNoiseEvenHistogram = fDetectorChannelStripNoiseEvenHistograms.getObject(cBoard->getId())
                                                                        ->getObject(cOpticalGroup->getId())
                                                                        ->getObject(cHybrid->getId())
                                                                        ->getObject(cChip->getId())
                                                                        ->getSummary<HistContainer<TH1F>>()
                                                                        .fTheHistogram;
                            TH1F* cChannelStripNoiseOddHistogram = fDetectorChannelStripNoiseOddHistograms.getObject(cBoard->getId())
                                                                       ->getObject(cOpticalGroup->getId())
                                                                       ->getObject(cHybrid->getId())
                                                                       ->getObject(cChip->getId())
                                                                       ->getSummary<HistContainer<TH1F>>()
                                                                       .fTheHistogram;
                            cChannelStripNoiseEvenHistogram->SetLineColor(kBlue);
                            cChannelStripNoiseEvenHistogram->SetMaximum(20);
                            cChannelStripNoiseEvenHistogram->SetMinimum(0);
                            cChannelStripNoiseOddHistogram->SetLineColor(kRed);
                            cChannelStripNoiseOddHistogram->SetMaximum(20);
                            cChannelStripNoiseOddHistogram->SetMinimum(0);
                            cChannelStripNoiseEvenHistogram->SetStats(false);
                            cChannelStripNoiseOddHistogram->SetStats(false);
                            cChannelStripNoiseEvenHistogram->DrawCopy();
                            cChannelStripNoiseOddHistogram->DrawCopy("same");

                            fDetectorChannelStripNoiseEvenHistograms.getObject(cBoard->getId())
                                ->getObject(cOpticalGroup->getId())
                                ->getObject(cHybrid->getId())
                                ->getObject(cChip->getId())
                                ->getSummary<HistContainer<TH1F>>()
                                .fTheHistogram->GetYaxis()
                                ->SetRangeUser(0., 20.);
                            fDetectorChannelStripNoiseOddHistograms.getObject(cBoard->getId())
                                ->getObject(cOpticalGroup->getId())
                                ->getObject(cHybrid->getId())
                                ->getObject(cChip->getId())
                                ->getSummary<HistContainer<TH1F>>()
                                .fTheHistogram->GetYaxis()
                                ->SetRangeUser(0., 20.);
                        }
                    }
                    else if(cType == FrontEndType::MPA2)
                    {
                        cValidation->cd(cChip->getId() + 1 + cHybrid->size() * 0);
                        TH1F* validationHistogram = fDetectorPixelValidationHistograms.getObject(cBoard->getId())
                                                        ->getObject(cOpticalGroup->getId())
                                                        ->getObject(cHybrid->getId())
                                                        ->getObject(cChip->getId())
                                                        ->getSummary<HistContainer<TH1F>>()
                                                        .fTheHistogram;
                        validationHistogram->SetStats(false);
                        validationHistogram->DrawCopy();
                        gPad->SetLogy();

                        cPedeNoise->cd(cChip->getId() + 1 + cHybrid->size() * 1);
                        fDetectorChipPixelPedestalHistograms.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getObject(cChip->getId())
                            ->getSummary<HistContainer<TH1F>>()
                            .fTheHistogram->DrawCopy();

                        cPedeNoise->cd(cChip->getId() + 1 + cHybrid->size() * 0);
                        fDetectorChipPixelNoiseHistograms.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getObject(cChip->getId())
                            ->getSummary<HistContainer<TH1F>>()
                            .fTheHistogram->DrawCopy();
                    }

                    if(fPlotSCurves)
                    {
                        if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA2)
                        {
                            TH2F* cChipStripSCurveHist = fDetectorChipStripSCurveHistograms.getObject(cBoard->getId())
                                                             ->getObject(cOpticalGroup->getId())
                                                             ->getObject(cHybrid->getId())
                                                             ->getObject(cChip->getId())
                                                             ->getSummary<HistContainer<TH2F>>()
                                                             .fTheHistogram;

                            float maxY = (fWithSSA) ? 254.5 : 1023.5;
                            cChipStripSCurveHist->GetYaxis()->SetRangeUser(-0.5, maxY);
                            TH1D* cTmp = cChipStripSCurveHist->ProjectionY();
                            cChipStripSCurveHist->GetYaxis()->SetRangeUser(cTmp->GetBinCenter(cTmp->FindFirstBinAbove(0.)) - 10, cTmp->GetBinCenter(cTmp->FindLastBinAbove(0.)) + 10);
                            delete cTmp;
                            cValidation->cd(cChip->getId() + 1 + cHybrid->size() * 2);
                            cChipStripSCurveHist->SetStats(false);
                            cChipStripSCurveHist->DrawCopy("colz");

                            fDetectorChannelStripNoiseHistograms.getObject(cBoard->getId())
                                ->getObject(cOpticalGroup->getId())
                                ->getObject(cHybrid->getId())
                                ->getObject(cChip->getId())
                                ->getSummary<HistContainer<TH1F>>()
                                .fTheHistogram->GetYaxis()
                                ->SetRangeUser(0., 20.);
                        }
                        else if(cType == FrontEndType::MPA2)
                        {
                            TH2F* cChipPixelSCurveHist = fDetectorChipPixelSCurveHistograms.getObject(cBoard->getId())
                                                             ->getObject(cOpticalGroup->getId())
                                                             ->getObject(cHybrid->getId())
                                                             ->getObject(cChip->getId())
                                                             ->getSummary<HistContainer<TH2F>>()
                                                             .fTheHistogram;

                            cChipPixelSCurveHist->GetYaxis()->SetRangeUser(-0.5, 254.5);
                            TH1D* cTmp = cChipPixelSCurveHist->ProjectionY();
                            cChipPixelSCurveHist->GetYaxis()->SetRangeUser(cTmp->GetBinCenter(cTmp->FindFirstBinAbove(0)) - 10, cTmp->GetBinCenter(cTmp->FindLastBinAbove(0.)) + 10);
                            // cSCurveHist->GetZaxis()->SetRangeUser(0,1.);
                            delete cTmp;
                            cValidation->cd(cChip->getId() + 1 + cHybrid->size() * 2);
                            cChipPixelSCurveHist->SetStats(false);
                            cChipPixelSCurveHist->DrawCopy("colz");

                            fDetectorChannelPixelNoiseHistograms.getObject(cBoard->getId())
                                ->getObject(cOpticalGroup->getId())
                                ->getObject(cHybrid->getId())
                                ->getObject(cChip->getId())
                                ->getSummary<HistContainer<TH1F>>()
                                .fTheHistogram->GetYaxis()
                                ->SetRangeUser(0., 20.);
                        }
                    }
                }

                if(fWithCBC || fWithSSA)
                {
                    fDetectorHybridStripNoiseHistograms.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getObject(cHybrid->getId())
                        ->getSummary<HistContainer<TH1F>>()
                        .fTheHistogram->GetXaxis()
                        ->SetRangeUser(-0.5, fNStripChannels * 8 - 0.5);
                    fDetectorHybridStripNoiseHistograms.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getObject(cHybrid->getId())
                        ->getSummary<HistContainer<TH1F>>()
                        .fTheHistogram->GetYaxis()
                        ->SetRangeUser(0., 20.);

                    if(fWithCBC)
                    {
                        TH1F* cHybridStripNoiseEvenHistogram = fDetectorHybridStripNoiseEvenHistograms.getObject(cBoard->getId())
                                                                   ->getObject(cOpticalGroup->getId())
                                                                   ->getObject(cHybrid->getId())
                                                                   ->getSummary<HistContainer<TH1F>>()
                                                                   .fTheHistogram;
                        TH1F* cHybridStripNoiseOddHistogram = fDetectorHybridStripNoiseOddHistograms.getObject(cBoard->getId())
                                                                  ->getObject(cOpticalGroup->getId())
                                                                  ->getObject(cHybrid->getId())
                                                                  ->getSummary<HistContainer<TH1F>>()
                                                                  .fTheHistogram;
                        cHybridStripNoiseEvenHistogram->SetLineColor(kBlue);
                        cHybridStripNoiseEvenHistogram->SetMaximum(20);
                        cHybridStripNoiseEvenHistogram->SetMinimum(0);
                        cHybridStripNoiseOddHistogram->SetLineColor(kRed);
                        cHybridStripNoiseOddHistogram->SetMaximum(20);
                        cHybridStripNoiseOddHistogram->SetMinimum(0);
                        cHybridStripNoiseEvenHistogram->SetStats(false);
                        cHybridStripNoiseOddHistogram->SetStats(false);

                        fDetectorHybridStripNoiseEvenHistograms.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getSummary<HistContainer<TH1F>>()
                            .fTheHistogram->GetXaxis()
                            ->SetRangeUser(-0.5, fNStripChannels * 8 - 0.5);
                        fDetectorHybridStripNoiseEvenHistograms.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getSummary<HistContainer<TH1F>>()
                            .fTheHistogram->GetYaxis()
                            ->SetRangeUser(0., 20.);

                        fDetectorHybridStripNoiseOddHistograms.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getSummary<HistContainer<TH1F>>()
                            .fTheHistogram->GetXaxis()
                            ->SetRangeUser(-0.5, fNStripChannels * 8 - 0.5);
                        fDetectorHybridStripNoiseOddHistograms.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getSummary<HistContainer<TH1F>>()
                            .fTheHistogram->GetYaxis()
                            ->SetRangeUser(0., 20.);
                    }
                }
                if(fWithMPA)
                {
                    fDetectorHybridPixelNoiseHistograms.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getObject(cHybrid->getId())
                        ->getSummary<HistContainer<TH1F>>()
                        .fTheHistogram->GetXaxis()
                        ->SetRangeUser(-0.5, fNPixelChannels * 8 - 0.5);
                    fDetectorHybridPixelNoiseHistograms.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getObject(cHybrid->getId())
                        ->getSummary<HistContainer<TH1F>>()
                        .fTheHistogram->GetYaxis()
                        ->SetRangeUser(0., 20.);
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramPedeNoise::reset(void) {}

//========================================================================================================================
void DQMHistogramPedeNoise::fillValidationPlots(DetectorDataContainer& theOccupancy)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    TH1F* cChipValidationHistogram = nullptr;
                    auto  cType                    = cChip->getFrontEndType();
                    if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA2)
                    {
                        cChipValidationHistogram = fDetectorStripValidationHistograms.getObject(cBoard->getId())
                                                       ->getObject(cOpticalGroup->getId())
                                                       ->getObject(cHybrid->getId())
                                                       ->getObject(cChip->getId())
                                                       ->getSummary<HistContainer<TH1F>>()
                                                       .fTheHistogram;
                    }
                    else if(cType == FrontEndType::MPA2)
                    {
                        cChipValidationHistogram = fDetectorPixelValidationHistograms.getObject(cBoard->getId())
                                                       ->getObject(cOpticalGroup->getId())
                                                       ->getObject(cHybrid->getId())
                                                       ->getObject(cChip->getId())
                                                       ->getSummary<HistContainer<TH1F>>()
                                                       .fTheHistogram;
                    }

                    auto theChipContainer = theOccupancy.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    if(theChipContainer->hasChannelContainer() == false) continue;
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            cChipValidationHistogram->SetBinContent(linearizeRowAndCols(row, col, theChipContainer->getNumberOfCols()) + 1,
                                                                    theChipContainer->getChannel<Occupancy>(row, col).fOccupancy);
                            cChipValidationHistogram->SetBinError(linearizeRowAndCols(row, col, theChipContainer->getNumberOfCols()) + 1,
                                                                  theChipContainer->getChannel<Occupancy>(row, col).fOccupancyError);
                        }
                    }
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramPedeNoise::fillPedestalAndNoisePlots(DetectorDataContainer& thePedestalAndNoise)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                TH1F *cHybridNoiseHistogram = nullptr, *cHybridChannelNoiseHistogram = nullptr, *cHybridStripNoiseEvenHistogram = nullptr, *cHybridStripNoiseOddHistogram = nullptr;

                cHybridNoiseHistogram =
                    fDetectorHybridNoiseHistograms.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                if(fWithCBC)
                {
                    cHybridStripNoiseEvenHistogram = fDetectorHybridStripNoiseEvenHistograms.getObject(cBoard->getId())
                                                         ->getObject(cOpticalGroup->getId())
                                                         ->getObject(cHybrid->getId())
                                                         ->getSummary<HistContainer<TH1F>>()
                                                         .fTheHistogram;
                    cHybridStripNoiseOddHistogram = fDetectorHybridStripNoiseOddHistograms.getObject(cBoard->getId())
                                                        ->getObject(cOpticalGroup->getId())
                                                        ->getObject(cHybrid->getId())
                                                        ->getSummary<HistContainer<TH1F>>()
                                                        .fTheHistogram;
                }
                for(auto cChip: *cHybrid)
                {
                    auto     cType                  = cChip->getFrontEndType();
                    uint32_t cNChannels             = 0;
                    TH1F *   cChipPedestalHistogram = nullptr, *cChipNoiseHistogram = nullptr, *cChannelPedestalHistogram = nullptr, *cChannelNoiseHistogram = nullptr,
                         *cChannelStripNoiseEvenHistogram = nullptr, *cChannelStripNoiseOddHistogram = nullptr;
                    TH2F* cChannel2DPixelNoiseHistogram = nullptr;
                    if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA2)
                    {
                        cNChannels = fNStripChannels;

                        cHybridChannelNoiseHistogram = fDetectorHybridStripNoiseHistograms.getObject(cBoard->getId())
                                                           ->getObject(cOpticalGroup->getId())
                                                           ->getObject(cHybrid->getId())
                                                           ->getSummary<HistContainer<TH1F>>()
                                                           .fTheHistogram;

                        cChipPedestalHistogram = fDetectorChipStripPedestalHistograms.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<HistContainer<TH1F>>()
                                                     .fTheHistogram;

                        cChannelPedestalHistogram = fDetectorChannelStripPedestalHistograms.getObject(cBoard->getId())
                                                        ->getObject(cOpticalGroup->getId())
                                                        ->getObject(cHybrid->getId())
                                                        ->getObject(cChip->getId())
                                                        ->getSummary<HistContainer<TH1F>>()
                                                        .fTheHistogram;

                        cChipNoiseHistogram = fDetectorChipStripNoiseHistograms.getObject(cBoard->getId())
                                                  ->getObject(cOpticalGroup->getId())
                                                  ->getObject(cHybrid->getId())
                                                  ->getObject(cChip->getId())
                                                  ->getSummary<HistContainer<TH1F>>()
                                                  .fTheHistogram;

                        cChannelNoiseHistogram = fDetectorChannelStripNoiseHistograms.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<HistContainer<TH1F>>()
                                                     .fTheHistogram;

                        if(cType == FrontEndType::CBC3)
                        {
                            cChannelStripNoiseEvenHistogram = fDetectorChannelStripNoiseEvenHistograms.getObject(cBoard->getId())
                                                                  ->getObject(cOpticalGroup->getId())
                                                                  ->getObject(cHybrid->getId())
                                                                  ->getObject(cChip->getId())
                                                                  ->getSummary<HistContainer<TH1F>>()
                                                                  .fTheHistogram;
                            cChannelStripNoiseOddHistogram = fDetectorChannelStripNoiseOddHistograms.getObject(cBoard->getId())
                                                                 ->getObject(cOpticalGroup->getId())
                                                                 ->getObject(cHybrid->getId())
                                                                 ->getObject(cChip->getId())
                                                                 ->getSummary<HistContainer<TH1F>>()
                                                                 .fTheHistogram;
                        }
                    }
                    else if(cType == FrontEndType::MPA2)
                    {
                        cNChannels = fNPixelChannels;

                        cHybridChannelNoiseHistogram = fDetectorHybridPixelNoiseHistograms.getObject(cBoard->getId())
                                                           ->getObject(cOpticalGroup->getId())
                                                           ->getObject(cHybrid->getId())
                                                           ->getSummary<HistContainer<TH1F>>()
                                                           .fTheHistogram;

                        cChipPedestalHistogram = fDetectorChipPixelPedestalHistograms.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<HistContainer<TH1F>>()
                                                     .fTheHistogram;

                        cChannelPedestalHistogram = fDetectorChannelPixelPedestalHistograms.getObject(cBoard->getId())
                                                        ->getObject(cOpticalGroup->getId())
                                                        ->getObject(cHybrid->getId())
                                                        ->getObject(cChip->getId())
                                                        ->getSummary<HistContainer<TH1F>>()
                                                        .fTheHistogram;

                        cChipNoiseHistogram = fDetectorChipPixelNoiseHistograms.getObject(cBoard->getId())
                                                  ->getObject(cOpticalGroup->getId())
                                                  ->getObject(cHybrid->getId())
                                                  ->getObject(cChip->getId())
                                                  ->getSummary<HistContainer<TH1F>>()
                                                  .fTheHistogram;

                        cChannelNoiseHistogram = fDetectorChannelPixelNoiseHistograms.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<HistContainer<TH1F>>()
                                                     .fTheHistogram;

                        cChannel2DPixelNoiseHistogram = fDetectorChannel2DPixelNoiseHistograms.getObject(cBoard->getId())
                                                            ->getObject(cOpticalGroup->getId())
                                                            ->getObject(cHybrid->getId())
                                                            ->getObject(cChip->getId())
                                                            ->getSummary<HistContainer<TH2F>>()
                                                            .fTheHistogram;
                    }

                    auto theChipContainer = thePedestalAndNoise.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    if(theChipContainer->hasChannelContainer() == false) continue;
                    for(uint16_t row = 0; row < theChipContainer->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < theChipContainer->getNumberOfCols(); ++col)
                        {
                            uint16_t channelIndex = linearizeRowAndCols(row, col, theChipContainer->getNumberOfCols());
                            float    cNoise = (std::isnan(theChipContainer->getChannel<ThresholdAndNoise>(row, col).fNoise)) ? 666 : theChipContainer->getChannel<ThresholdAndNoise>(row, col).fNoise;
                            float    cNoiseErr =
                                (std::isnan(theChipContainer->getChannel<ThresholdAndNoise>(row, col).fNoiseError)) ? 666 : theChipContainer->getChannel<ThresholdAndNoise>(row, col).fNoiseError;
                            float cPedestal =
                                (std::isnan(theChipContainer->getChannel<ThresholdAndNoise>(row, col).fThreshold)) ? 666 : theChipContainer->getChannel<ThresholdAndNoise>(row, col).fThreshold;
                            float cPedestalErr =
                                (std::isnan(theChipContainer->getChannel<ThresholdAndNoise>(row, col).fThreshold)) ? 666 : theChipContainer->getChannel<ThresholdAndNoise>(row, col).fThresholdError;
                            cChipPedestalHistogram->Fill(cPedestal);
                            cChipNoiseHistogram->Fill(cNoise);
                            cHybridNoiseHistogram->Fill(cNoise);

                            cChannelNoiseHistogram->SetBinContent(channelIndex + 1, cNoise);
                            cChannelNoiseHistogram->SetBinError(channelIndex + 1, cNoiseErr);
                            cChannelPedestalHistogram->SetBinContent(channelIndex + 1, cPedestal);
                            cChannelPedestalHistogram->SetBinError(channelIndex + 1, cPedestalErr);
                            cHybridChannelNoiseHistogram->SetBinContent(cNChannels * (cChip->getId() % 8) + channelIndex + 1, cNoise);
                            cHybridChannelNoiseHistogram->SetBinError(cNChannels * (cChip->getId() % 8) + channelIndex + 1, cNoiseErr);

                            if(cType == FrontEndType::CBC3)
                            {
                                if((int(col) % 2) == 0)
                                {
                                    cChannelStripNoiseEvenHistogram->SetBinContent(int(col / 2) + 1, cNoise);
                                    cChannelStripNoiseEvenHistogram->SetBinError(int(col / 2) + 1, cNoiseErr);
                                    cHybridStripNoiseEvenHistogram->SetBinContent(cNChannels / 2 * (cChip->getId() % 8) + int(col / 2) + 1, cNoise);
                                    cHybridStripNoiseEvenHistogram->SetBinError(cNChannels / 2 * (cChip->getId() % 8) + int(col / 2) + 1, cNoiseErr);
                                }
                                else
                                {
                                    cChannelStripNoiseOddHistogram->SetBinContent(int(col / 2) + 1, cNoise);
                                    cChannelStripNoiseOddHistogram->SetBinError(int(col / 2) + 1, cNoiseErr);
                                    cHybridStripNoiseOddHistogram->SetBinContent(cNChannels / 2 * (cChip->getId() % 8) + int(col / 2) + 1, cNoise);
                                    cHybridStripNoiseOddHistogram->SetBinError(cNChannels / 2 * (cChip->getId() % 8) + int(col / 2) + 1, cNoiseErr);
                                }
                            }
                            if(cType == FrontEndType::MPA2) { cChannel2DPixelNoiseHistogram->SetBinContent(col + 1, row + 1, cNoise); }
                        }
                    }
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramPedeNoise::fillSCurvePlots(uint16_t pStripTh, uint16_t pPixelTh, DetectorDataContainer& fSCurveOccupancy)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto     cType = cChip->getFrontEndType();
                    uint16_t cTh   = 0;
                    if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA2)
                        cTh = pStripTh;
                    else if(cType == FrontEndType::MPA2)
                        cTh = pPixelTh;

                    TH2F* cChipSCurve = nullptr;
                    if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA2)
                    {
                        cChipSCurve = fDetectorChipStripSCurveHistograms.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<HistContainer<TH2F>>()
                                          .fTheHistogram;
                    }
                    else if(cType == FrontEndType::MPA2)
                    {
                        cChipSCurve = fDetectorChipPixelSCurveHistograms.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<HistContainer<TH2F>>()
                                          .fTheHistogram;
                    }

                    auto theChipContainer = fSCurveOccupancy.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    if(theChipContainer->hasChannelContainer() == false) continue;
                    uint16_t cChannelNumber = 0;

                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            float tmpOccupancy      = theChipContainer->getChannel<Occupancy>(row, col).fOccupancy;
                            float tmpOccupancyError = theChipContainer->getChannel<Occupancy>(row, col).fOccupancyError;
                            cChipSCurve->SetBinContent(linearizeRowAndCols(row, col, cChip->getNumberOfCols()) + 1, cTh + 1, tmpOccupancy);
                            cChipSCurve->SetBinError(linearizeRowAndCols(row, col, cChip->getNumberOfCols()) + 1, cTh + 1, tmpOccupancyError);

                            if(fFitSCurves)
                            {
                                TH1F* cChannelSCurve = nullptr;

                                if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA2)
                                {
                                    cChannelSCurve = fDetectorChannelStripSCurveHistograms.getObject(cBoard->getId())
                                                         ->getObject(cOpticalGroup->getId())
                                                         ->getObject(cHybrid->getId())
                                                         ->getObject(cChip->getId())
                                                         ->getChannel<HistContainer<TH1F>>(row, col)
                                                         .fTheHistogram;
                                }
                                else if(cType == FrontEndType::MPA2)
                                {
                                    cChannelSCurve = fDetectorChannelPixelSCurveHistograms.getObject(cBoard->getId())
                                                         ->getObject(cOpticalGroup->getId())
                                                         ->getObject(cHybrid->getId())
                                                         ->getObject(cChip->getId())
                                                         ->getChannel<HistContainer<TH1F>>(row, col)
                                                         .fTheHistogram;
                                }
                                // std::cout << "Threshold = " << cTh + 1 << " - Occupancy = " <<  tmpOccupancy << std::endl;
                                cChannelSCurve->SetBinContent(cTh + 1, tmpOccupancy);
                                cChannelSCurve->SetBinError(cTh + 1, tmpOccupancyError);
                            }
                            ++cChannelNumber;
                        }
                    }
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramPedeNoise::fitSCurves()
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ChipDataContainer* theChipThresholdAndNoise =
                        fThresholdAndNoiseContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    auto cType = cChip->getFrontEndType();

                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            TH1F *cChannelSCurve = nullptr, *cChannelNoiseHistogram = nullptr, *cChannelPedestalHistogram = nullptr;
                            if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA2)
                            {
                                cChannelSCurve = fDetectorChannelStripSCurveHistograms.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getChannel<HistContainer<TH1F>>(row, col)
                                                     .fTheHistogram;

                                cChannelNoiseHistogram = fDetectorChannelStripNoiseHistograms.getObject(cBoard->getId())
                                                             ->getObject(cOpticalGroup->getId())
                                                             ->getObject(cHybrid->getId())
                                                             ->getObject(cChip->getId())
                                                             ->getSummary<HistContainer<TH1F>>()
                                                             .fTheHistogram;

                                cChannelPedestalHistogram = fDetectorChannelStripPedestalHistograms.getObject(cBoard->getId())
                                                                ->getObject(cOpticalGroup->getId())
                                                                ->getObject(cHybrid->getId())
                                                                ->getObject(cChip->getId())
                                                                ->getSummary<HistContainer<TH1F>>()
                                                                .fTheHistogram;
                            }
                            else if(cType == FrontEndType::MPA2)
                            {
                                cChannelSCurve = fDetectorChannelPixelSCurveHistograms.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getChannel<HistContainer<TH1F>>(row, col)
                                                     .fTheHistogram;

                                cChannelNoiseHistogram = fDetectorChannelPixelNoiseHistograms.getObject(cBoard->getId())
                                                             ->getObject(cOpticalGroup->getId())
                                                             ->getObject(cHybrid->getId())
                                                             ->getObject(cChip->getId())
                                                             ->getSummary<HistContainer<TH1F>>()
                                                             .fTheHistogram;

                                cChannelPedestalHistogram = fDetectorChannelPixelPedestalHistograms.getObject(cBoard->getId())
                                                                ->getObject(cOpticalGroup->getId())
                                                                ->getObject(cHybrid->getId())
                                                                ->getObject(cChip->getId())
                                                                ->getSummary<HistContainer<TH1F>>()
                                                                .fTheHistogram;
                            }

                            float cChannelNoise = cChannelNoiseHistogram->GetBinContent(linearizeRowAndCols(row, col, cChip->getNumberOfCols()) + 1);

                            float cChannelPedestal = cChannelPedestalHistogram->GetBinContent(linearizeRowAndCols(row, col, cChip->getNumberOfCols()) + 1);

                            TF1* cFit = new TF1("SCurveFit", MyErf, cChannelPedestal - (cChannelNoise * 5), cChannelPedestal + (cChannelNoise * 5), 2);

                            cFit->SetParameter(0, cChannelPedestal);
                            cFit->SetParameter(1, cChannelNoise);

                            // Fit
                            cChannelSCurve->Fit(cFit, "RQ");

                            theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(row, col).fThreshold      = cFit->GetParameter(0);
                            theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(row, col).fNoise          = cFit->GetParameter(1);
                            theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(row, col).fThresholdError = cFit->GetParError(0);
                            theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(row, col).fNoiseError     = cFit->GetParError(1);
                        }
                    }
                }
            }
        }
    }

    clearPedestalAndNoisePlots();
    fillPedestalAndNoisePlots(fThresholdAndNoiseContainer);
}

//========================================================================================================================
void DQMHistogramPedeNoise::clearPedestalAndNoisePlots()
{
    for(auto cBoard: *fDetectorContainer)
    {
        size_t boardId = cBoard->getId();
        for(auto cOpticalGroup: *cBoard)
        {
            size_t opticalGroupId = cOpticalGroup->getId();
            bool   is2Smodule     = (cOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S);
            for(auto cHybrid: *cOpticalGroup)
            {
                size_t hybridId = cHybrid->getId();

                fDetectorHybridNoiseHistograms.getHybrid(boardId, opticalGroupId, hybridId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                fDetectorHybridStripNoiseHistograms.getHybrid(boardId, opticalGroupId, hybridId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();

                if(is2Smodule)
                {
                    fDetectorHybridStripNoiseEvenHistograms.getHybrid(boardId, opticalGroupId, hybridId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                    fDetectorHybridStripNoiseOddHistograms.getHybrid(boardId, opticalGroupId, hybridId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                }
                else { fDetectorHybridPixelNoiseHistograms.getHybrid(boardId, opticalGroupId, hybridId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset(); }

                for(auto cChip: *cHybrid)
                {
                    size_t       chipId      = cChip->getId();
                    FrontEndType theChipType = cChip->getFrontEndType();
                    if(theChipType == FrontEndType::CBC3 || theChipType == FrontEndType::SSA2)
                    {
                        fDetectorChipStripPedestalHistograms.getChip(boardId, opticalGroupId, hybridId, chipId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                        fDetectorChannelStripPedestalHistograms.getChip(boardId, opticalGroupId, hybridId, chipId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                        fDetectorChipStripNoiseHistograms.getChip(boardId, opticalGroupId, hybridId, chipId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                        fDetectorChannelStripNoiseHistograms.getChip(boardId, opticalGroupId, hybridId, chipId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                    }

                    if(theChipType == FrontEndType::CBC3)
                    {
                        fDetectorChannelStripNoiseEvenHistograms.getChip(boardId, opticalGroupId, hybridId, chipId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                        fDetectorChannelStripNoiseOddHistograms.getChip(boardId, opticalGroupId, hybridId, chipId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                    }

                    if(theChipType == FrontEndType::MPA2)
                    {
                        fDetectorChipPixelPedestalHistograms.getChip(boardId, opticalGroupId, hybridId, chipId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                        fDetectorChannelPixelPedestalHistograms.getChip(boardId, opticalGroupId, hybridId, chipId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                        fDetectorChipPixelNoiseHistograms.getChip(boardId, opticalGroupId, hybridId, chipId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                        fDetectorChannelPixelNoiseHistograms.getChip(boardId, opticalGroupId, hybridId, chipId)->getSummary<HistContainer<TH1F>>().fTheHistogram->Reset();
                        fDetectorChannel2DPixelNoiseHistograms.getChip(boardId, opticalGroupId, hybridId, chipId)->getSummary<HistContainer<TH2F>>().fTheHistogram->Reset();
                    }
                }
            }
        }
    }
}