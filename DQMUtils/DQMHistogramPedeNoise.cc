/*!
        \file                DQMHistogramPedeNoise.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
 */

#include "../DQMUtils/DQMHistogramPedeNoise.h"
#include "../HWDescription/ReadoutChip.h"
#include "../RootUtils/HistContainer.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"
#include "../Utils/ThresholdAndNoise.h"
#include "../Utils/Utilities.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramPedeNoise::DQMHistogramPedeNoise() {}

//========================================================================================================================
DQMHistogramPedeNoise::~DQMHistogramPedeNoise() {}

//========================================================================================================================
void DQMHistogramPedeNoise::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap)
{
    std::cout << __PRETTY_FUNCTION__ << " " << theDetectorStructure.at(0)->at(0)->at(0)->size() << std::endl;
    uint32_t NCH = theDetectorStructure.at(0)->at(0)->at(0)->at(0)->size();
    // if
    // (static_cast<Ph2_HwDescription::ReadoutChip*>(theDetectorStructure.at(0)->at(0)->at(0)->at(0))->getFrontEndType()
    // == FrontEndType::SSA) NCH = NSSACHANNELS;

    auto cSetting = pSettingsMap.find("PlotSCurves");
    fPlotSCurves  = (cSetting != std::end(pSettingsMap)) ? cSetting->second : 0;
    cSetting      = pSettingsMap.find("FitSCurves");
    fFitSCurves   = (cSetting != std::end(pSettingsMap)) ? cSetting->second : 0;
    if(fFitSCurves) fPlotSCurves = true;

    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);

    // SCurve
    if(fPlotSCurves)
    {
        uint16_t nYbins = 1024;
        float    minY   = -0.5;
        float    maxY   = 1023.5;
        if(cWithSSA or cWithMPA)
        {
            nYbins = 255;
            minY   = -0.5;
            maxY   = 254.5;
        }

        HistContainer<TH2F> theTH2FSCurve("SCurve", "SCurve", NCH, -0.5, NCH - 0.5, nYbins, minY, maxY);
        RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorSCurveHistograms, theTH2FSCurve);
        if(fFitSCurves)
        {
            HistContainer<TH1F> theTH1FSCurveContainer("SCurve", "SCurve", nYbins, minY, maxY);
            RootContainerFactory::bookChannelHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChannelSCurveHistograms, theTH1FSCurveContainer);

            ContainerFactory::copyAndInitStructure<ThresholdAndNoise>(theDetectorStructure, fThresholdAndNoiseContainer);
        }
    }

    // Pedestal
    HistContainer<TH1F> theTH1FPedestalContainer("PedestalDistribution", "Pedestal Distribution", 2048, -0.5, 1023.5);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorPedestalHistograms, theTH1FPedestalContainer);

    // Noise
    HistContainer<TH1F> theTH1FNoiseContainer("NoiseDistribution", "Noise Distribution", 200, 0., 20.);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorNoiseHistograms, theTH1FNoiseContainer);

    // Strip Noise
    HistContainer<TH1F> theTH1FStripNoiseContainer("StripNoiseDistribution", "Strip Noise", NCH, -0.5, float(NCH) - 0.5);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorStripNoiseHistograms, theTH1FStripNoiseContainer);

    // Strip Pedestal
    HistContainer<TH1F> theTH1FStripPedestalContainer("StripPedestalDistribution", "Strip Pedestal", NCH, -0.5, float(NCH) - 0.5);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorStripPedestalHistograms, theTH1FStripPedestalContainer);

    // Strip Noise Even
    HistContainer<TH1F> theTH1FStripNoiseEvenContainer("StripNoiseEvenDistribution", "Strip Noise Even", NCH / 2, -0.5, 126.5);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorStripNoiseEvenHistograms, theTH1FStripNoiseEvenContainer);

    // Strip Noise Odd
    HistContainer<TH1F> theTH1FStripNoiseOddContainer("StripNoiseOddDistribution", "Strip Noise Odd", NCH / 2, -0.5, 126.5);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorStripNoiseOddHistograms, theTH1FStripNoiseOddContainer);

    // Hybrid Noise
    HistContainer<TH1F> theTH1FHybridNoiseContainer("HybridNoiseDistribution", "Hybrid Noise Distribution", 200, 0., 20.);
    RootContainerFactory::bookHybridHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorHybridNoiseHistograms, theTH1FHybridNoiseContainer);

    // Hybrid Strip Noise
    HistContainer<TH1F> theTH1FHybridStripNoiseContainer("HybridStripNoiseDistribution", "Hybrid Strip Noise", NCH * 8, -0.5, float(NCH) * 8 - 0.5);
    RootContainerFactory::bookHybridHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorHybridStripNoiseHistograms, theTH1FHybridStripNoiseContainer);

    // Hybrid Strip Even Noise
    HistContainer<TH1F> theTH1FHybridStripNoiseEvenContainer("HybridStripNoiseEvenDistribution", "Hybrid Strip Noise Even", NCH * 4, -0.5, float(NCH) * 4 - 0.5);
    RootContainerFactory::bookHybridHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorHybridStripNoiseEvenHistograms, theTH1FHybridStripNoiseEvenContainer);

    // Hybrid Strip Odd Noise
    HistContainer<TH1F> theTH1FHybridStripNoiseOddContainer("HybridStripNoiseOddDistribution", "Hybrid Strip Noise Odd", NCH * 4, -0.5, float(NCH) * 4 - 0.5);
    RootContainerFactory::bookHybridHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorHybridStripNoiseOddHistograms, theTH1FHybridStripNoiseOddContainer);

    // Validation
    HistContainer<TH1F> theTH1FValidationContainer("Occupancy", "Occupancy", NCH, -0.5, float(NCH) - 0.5);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorValidationHistograms, theTH1FValidationContainer);
}

//========================================================================================================================
bool DQMHistogramPedeNoise::fill(std::vector<char>& dataBuffer)
{
    HybridContainerStream<Occupancy, Occupancy, Occupancy> theOccupancy("PedeNoise");
    ChannelContainerStream<Occupancy, uint16_t>            theSCurve("PedeNoiseSCurve");
    ChannelContainerStream<ThresholdAndNoise>              theThresholdAndNoiseStream("PedeNoise");

    if(theOccupancy.attachBuffer(&dataBuffer))
    {
        std::cout << "Matched PedeNoise Occupancy!!!!!\n";
        theOccupancy.decodeHybridData(fDetectorData);
        fillValidationPlots(fDetectorData);

        fDetectorData.cleanDataStored();
        return true;
    }
    else if(theSCurve.attachBuffer(&dataBuffer))
    {
        std::cout << "Matched PedeNoise SCurve!!!!!\n";
        theSCurve.decodeChipData(fDetectorData);
        fillSCurvePlots(theSCurve.getHeaderElement(), fDetectorData);

        fDetectorData.cleanDataStored();
        return true;
    }
    else if(theThresholdAndNoiseStream.attachBuffer(&dataBuffer))
    {
        std::cout << "Matched PedeNoise ThresholdAndNoise!!!!!\n";
        theThresholdAndNoiseStream.decodeChipData(fDetectorData);
        fillPedestalAndNoisePlots(fDetectorData);

        fDetectorData.cleanDataStored();
        return true;
    }

    return false;
}

//========================================================================================================================
void DQMHistogramPedeNoise::process()
{
    if(fFitSCurves) fitSCurves();

    for(auto board: fDetectorPedestalHistograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                TH1F* hybridStripNoiseEvenHistogram =
                    fDetectorHybridStripNoiseEvenHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                TH1F* hybridStripNoiseOddHistogram =
                    fDetectorHybridStripNoiseOddHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                hybridStripNoiseEvenHistogram->SetLineColor(kBlue);
                hybridStripNoiseEvenHistogram->SetMaximum(20);
                hybridStripNoiseEvenHistogram->SetMinimum(0);
                hybridStripNoiseOddHistogram->SetLineColor(kRed);
                hybridStripNoiseOddHistogram->SetMaximum(20);
                hybridStripNoiseOddHistogram->SetMinimum(0);
                hybridStripNoiseEvenHistogram->SetStats(false);
                hybridStripNoiseOddHistogram->SetStats(false);

                TCanvas* cValidation =
                    new TCanvas(("Validation_hybrid_" + std::to_string(hybrid->getId())).data(), ("Validation hybrid " + std::to_string(hybrid->getId())).data(), 0, 0, 650, fPlotSCurves ? 900 : 650);
                TCanvas* cPedeNoise = new TCanvas(("PedeNoise_hybrid_" + std::to_string(hybrid->getId())).data(), ("PedeNoise hybrid " + std::to_string(hybrid->getId())).data(), 670, 0, 650, 650);

                cValidation->Divide(hybrid->size(), fPlotSCurves ? 3 : 2);
                cPedeNoise->Divide(hybrid->size(), 2);

                for(auto chip: *hybrid)
                {
                    cValidation->cd(chip->getIndex() + 1 + hybrid->size() * 0);
                    TH1F* validationHistogram = fDetectorValidationHistograms.at(board->getIndex())
                                                    ->at(opticalGroup->getIndex())
                                                    ->at(hybrid->getIndex())
                                                    ->at(chip->getIndex())
                                                    ->getSummary<HistContainer<TH1F>>()
                                                    .fTheHistogram;
                    validationHistogram->SetStats(false);
                    validationHistogram->DrawCopy();
                    gPad->SetLogy();

                    cValidation->cd(chip->getIndex() + 1 + hybrid->size() * 1);
                    TH1F* chipStripNoiseEvenHistogram = fDetectorStripNoiseEvenHistograms.at(board->getIndex())
                                                            ->at(opticalGroup->getIndex())
                                                            ->at(hybrid->getIndex())
                                                            ->at(chip->getIndex())
                                                            ->getSummary<HistContainer<TH1F>>()
                                                            .fTheHistogram;
                    TH1F* chipStripNoiseOddHistogram = fDetectorStripNoiseOddHistograms.at(board->getIndex())
                                                           ->at(opticalGroup->getIndex())
                                                           ->at(hybrid->getIndex())
                                                           ->at(chip->getIndex())
                                                           ->getSummary<HistContainer<TH1F>>()
                                                           .fTheHistogram;
                    chipStripNoiseEvenHistogram->SetLineColor(kBlue);
                    chipStripNoiseEvenHistogram->SetMaximum(20);
                    chipStripNoiseEvenHistogram->SetMinimum(0);
                    chipStripNoiseOddHistogram->SetLineColor(kRed);
                    chipStripNoiseOddHistogram->SetMaximum(20);
                    chipStripNoiseOddHistogram->SetMinimum(0);
                    chipStripNoiseEvenHistogram->SetStats(false);
                    chipStripNoiseOddHistogram->SetStats(false);
                    chipStripNoiseEvenHistogram->DrawCopy();
                    chipStripNoiseOddHistogram->DrawCopy("same");

                    cPedeNoise->cd(chip->getIndex() + 1 + hybrid->size() * 1);
                    fDetectorPedestalHistograms.at(board->getIndex())
                        ->at(opticalGroup->getIndex())
                        ->at(hybrid->getIndex())
                        ->at(chip->getIndex())
                        ->getSummary<HistContainer<TH1F>>()
                        .fTheHistogram->DrawCopy();

                    cPedeNoise->cd(chip->getIndex() + 1 + hybrid->size() * 0);
                    fDetectorNoiseHistograms.at(board->getIndex())
                        ->at(opticalGroup->getIndex())
                        ->at(hybrid->getIndex())
                        ->at(chip->getIndex())
                        ->getSummary<HistContainer<TH1F>>()
                        .fTheHistogram->DrawCopy();

                    if(fPlotSCurves)
                    {
                        TH2F* cSCurveHist = fDetectorSCurveHistograms.at(board->getIndex())
                                                ->at(opticalGroup->getIndex())
                                                ->at(hybrid->getIndex())
                                                ->at(chip->getIndex())
                                                ->getSummary<HistContainer<TH2F>>()
                                                .fTheHistogram;
                        TH1D* cTmp = cSCurveHist->ProjectionY();
                        cSCurveHist->GetYaxis()->SetRangeUser(cTmp->GetBinCenter(cTmp->FindFirstBinAbove(0)) - 10, cTmp->GetBinCenter(cTmp->FindLastBinAbove(0.99)) + 10);
                        delete cTmp;
                        cValidation->cd(chip->getIndex() + 1 + hybrid->size() * 2);
                        cSCurveHist->SetStats(false);
                        cSCurveHist->DrawCopy("colz");
                    }

                    fDetectorStripNoiseHistograms.at(board->getIndex())
                        ->at(opticalGroup->getIndex())
                        ->at(hybrid->getIndex())
                        ->at(chip->getIndex())
                        ->getSummary<HistContainer<TH1F>>()
                        .fTheHistogram->GetYaxis()
                        ->SetRangeUser(0., 20.);
                    fDetectorStripNoiseEvenHistograms.at(board->getIndex())
                        ->at(opticalGroup->getIndex())
                        ->at(hybrid->getIndex())
                        ->at(chip->getIndex())
                        ->getSummary<HistContainer<TH1F>>()
                        .fTheHistogram->GetYaxis()
                        ->SetRangeUser(0., 20.);
                    fDetectorStripNoiseOddHistograms.at(board->getIndex())
                        ->at(opticalGroup->getIndex())
                        ->at(hybrid->getIndex())
                        ->at(chip->getIndex())
                        ->getSummary<HistContainer<TH1F>>()
                        .fTheHistogram->GetYaxis()
                        ->SetRangeUser(0., 20.);
                }

                fDetectorHybridStripNoiseHistograms.at(board->getIndex())
                    ->at(opticalGroup->getIndex())
                    ->at(hybrid->getIndex())
                    ->getSummary<HistContainer<TH1F>>()
                    .fTheHistogram->GetXaxis()
                    ->SetRangeUser(-0.5, NCHANNELS * hybrid->size() - 0.5);
                fDetectorHybridStripNoiseHistograms.at(board->getIndex())
                    ->at(opticalGroup->getIndex())
                    ->at(hybrid->getIndex())
                    ->getSummary<HistContainer<TH1F>>()
                    .fTheHistogram->GetYaxis()
                    ->SetRangeUser(0., 20.);

                fDetectorHybridStripNoiseEvenHistograms.at(board->getIndex())
                    ->at(opticalGroup->getIndex())
                    ->at(hybrid->getIndex())
                    ->getSummary<HistContainer<TH1F>>()
                    .fTheHistogram->GetXaxis()
                    ->SetRangeUser(-0.5, NCHANNELS * hybrid->size() - 0.5);
                fDetectorHybridStripNoiseEvenHistograms.at(board->getIndex())
                    ->at(opticalGroup->getIndex())
                    ->at(hybrid->getIndex())
                    ->getSummary<HistContainer<TH1F>>()
                    .fTheHistogram->GetYaxis()
                    ->SetRangeUser(0., 20.);

                fDetectorHybridStripNoiseOddHistograms.at(board->getIndex())
                    ->at(opticalGroup->getIndex())
                    ->at(hybrid->getIndex())
                    ->getSummary<HistContainer<TH1F>>()
                    .fTheHistogram->GetXaxis()
                    ->SetRangeUser(-0.5, NCHANNELS * hybrid->size() - 0.5);
                fDetectorHybridStripNoiseOddHistograms.at(board->getIndex())
                    ->at(opticalGroup->getIndex())
                    ->at(hybrid->getIndex())
                    ->getSummary<HistContainer<TH1F>>()
                    .fTheHistogram->GetYaxis()
                    ->SetRangeUser(0., 20.);
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramPedeNoise::reset(void) {}

//========================================================================================================================
void DQMHistogramPedeNoise::fillValidationPlots(DetectorDataContainer& theOccupancy)
{
    for(auto board: theOccupancy)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                // std::cout << __PRETTY_FUNCTION__ << " The Hybrid Occupancy = " <<
                // hybrid->getSummary<Occupancy,Occupancy>().fOccupancy << std::endl;
                for(auto chip: *hybrid)
                {
                    TH1F* chipValidationHistogram = fDetectorValidationHistograms.at(board->getIndex())
                                                        ->at(opticalGroup->getIndex())
                                                        ->at(hybrid->getIndex())
                                                        ->at(chip->getIndex())
                                                        ->getSummary<HistContainer<TH1F>>()
                                                        .fTheHistogram;
                    uint channelBin = 1;

                    if(chip->getChannelContainer<Occupancy>() == nullptr) continue;
                    for(auto channel: *chip->getChannelContainer<Occupancy>())
                    {
                        chipValidationHistogram->SetBinContent(channelBin, channel.fOccupancy);
                        chipValidationHistogram->SetBinError(channelBin++, channel.fOccupancyError);
                    }
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramPedeNoise::fillPedestalAndNoisePlots(DetectorDataContainer& thePedestalAndNoise)
{
    for(auto board: thePedestalAndNoise)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                TH1F* hybridNoiseHistogram =
                    fDetectorHybridNoiseHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                TH1F* hybridStripNoiseHistogram =
                    fDetectorHybridStripNoiseHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                TH1F* hybridStripNoiseEvenHistogram =
                    fDetectorHybridStripNoiseEvenHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                TH1F* hybridStripNoiseOddHistogram =
                    fDetectorHybridStripNoiseOddHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;

                for(auto chip: *hybrid)
                {
                    TH1F* chipPedestalHistogram =
                        fDetectorPedestalHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    TH1F* chipNoiseHistogram =
                        fDetectorNoiseHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    TH1F* chipStripNoiseHistogram = fDetectorStripNoiseHistograms.at(board->getIndex())
                                                        ->at(opticalGroup->getIndex())
                                                        ->at(hybrid->getIndex())
                                                        ->at(chip->getIndex())
                                                        ->getSummary<HistContainer<TH1F>>()
                                                        .fTheHistogram;
                    TH1F* chipStripPedestalHistogram = fDetectorStripPedestalHistograms.at(board->getIndex())
                                                           ->at(opticalGroup->getIndex())
                                                           ->at(hybrid->getIndex())
                                                           ->at(chip->getIndex())
                                                           ->getSummary<HistContainer<TH1F>>()
                                                           .fTheHistogram;
                    TH1F* chipStripNoiseEvenHistogram = fDetectorStripNoiseEvenHistograms.at(board->getIndex())
                                                            ->at(opticalGroup->getIndex())
                                                            ->at(hybrid->getIndex())
                                                            ->at(chip->getIndex())
                                                            ->getSummary<HistContainer<TH1F>>()
                                                            .fTheHistogram;
                    TH1F* chipStripNoiseOddHistogram = fDetectorStripNoiseOddHistograms.at(board->getIndex())
                                                           ->at(opticalGroup->getIndex())
                                                           ->at(hybrid->getIndex())
                                                           ->at(chip->getIndex())
                                                           ->getSummary<HistContainer<TH1F>>()
                                                           .fTheHistogram;

                    if(chip->getChannelContainer<ThresholdAndNoise>() == nullptr) continue;
                    uint16_t channelNumber = 0;
                    for(auto channel: *chip->getChannelContainer<ThresholdAndNoise>())
                    {
                        chipPedestalHistogram->Fill(channel.fThreshold);
                        chipNoiseHistogram->Fill(channel.fNoise);
                        hybridNoiseHistogram->Fill(channel.fNoise);

                        if((int(channelNumber) % 2) == 0)
                        {
                            chipStripNoiseEvenHistogram->SetBinContent(int(channelNumber / 2) + 1, channel.fNoise);
                            chipStripNoiseEvenHistogram->SetBinError(int(channelNumber / 2) + 1, channel.fNoiseError);
                            hybridStripNoiseEvenHistogram->SetBinContent(NCHANNELS / 2 * chip->getIndex() + int(channelNumber / 2) + 1, channel.fNoise);
                            hybridStripNoiseEvenHistogram->SetBinError(NCHANNELS / 2 * chip->getIndex() + int(channelNumber / 2) + 1, channel.fNoiseError);
                        }
                        else
                        {
                            chipStripNoiseOddHistogram->SetBinContent(int(channelNumber / 2) + 1, channel.fNoise);
                            chipStripNoiseOddHistogram->SetBinError(int(channelNumber / 2) + 1, channel.fNoiseError);
                            hybridStripNoiseOddHistogram->SetBinContent(NCHANNELS / 2 * chip->getIndex() + int(channelNumber / 2) + 1, channel.fNoise);
                            hybridStripNoiseOddHistogram->SetBinError(NCHANNELS / 2 * chip->getIndex() + int(channelNumber / 2) + 1, channel.fNoiseError);
                        }

                        chipStripNoiseHistogram->SetBinContent(channelNumber + 1, channel.fNoise);
                        chipStripNoiseHistogram->SetBinError(channelNumber + 1, channel.fNoiseError);
                        chipStripPedestalHistogram->SetBinContent(channelNumber + 1, channel.fThreshold);
                        chipStripPedestalHistogram->SetBinError(channelNumber + 1, channel.fThresholdError);
                        hybridStripNoiseHistogram->SetBinContent(NCHANNELS * chip->getIndex() + channelNumber + 1, channel.fNoise);
                        hybridStripNoiseHistogram->SetBinError(NCHANNELS * chip->getIndex() + channelNumber + 1, channel.fNoiseError);

                        ++channelNumber;
                    }
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramPedeNoise::fillSCurvePlots(uint16_t vcthr, DetectorDataContainer& fSCurveOccupancy)
{
    for(auto board: fSCurveOccupancy)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    TH2F* chipSCurve =
                        fDetectorSCurveHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;

                    if(chip->getChannelContainer<Occupancy>() == nullptr) continue;
                    uint16_t channelNumber = 0;
                    for(auto channel: *chip->getChannelContainer<Occupancy>())
                    {
                        float tmpOccupancy      = channel.fOccupancy;
                        float tmpOccupancyError = channel.fOccupancyError;
                        chipSCurve->SetBinContent(channelNumber + 1, vcthr + 1, tmpOccupancy);
                        chipSCurve->SetBinError(channelNumber + 1, vcthr + 1, tmpOccupancyError);

                        if(fFitSCurves)
                        {
                            TH1F* channelSCurve = fDetectorChannelSCurveHistograms.at(board->getIndex())
                                                      ->at(opticalGroup->getIndex())
                                                      ->at(hybrid->getIndex())
                                                      ->at(chip->getIndex())
                                                      ->getChannel<HistContainer<TH1F>>(channelNumber)
                                                      .fTheHistogram;
                            channelSCurve->SetBinContent(vcthr + 1, tmpOccupancy);
                            channelSCurve->SetBinError(vcthr + 1, tmpOccupancyError);
                        }
                        ++channelNumber;
                    }
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramPedeNoise::fitSCurves()
{
    for(auto board: fDetectorChannelSCurveHistograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    ChipDataContainer* theChipThresholdAndNoise = fThresholdAndNoiseContainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex());

                    for(uint32_t cChannel = 0; cChannel < NCHANNELS; cChannel++)
                    {
                        TH1F* channelSCurve = chip->getChannel<HistContainer<TH1F>>(cChannel).fTheHistogram;

                        float cFirstNon0(0);
                        float cFirst1(0);

                        for(Int_t cBin = 1; cBin < channelSCurve->GetNbinsX() - 1; cBin++)
                        {
                            double cContent = channelSCurve->GetBinContent(cBin);

                            if(!cFirstNon0)
                            {
                                if(cContent) cFirstNon0 = channelSCurve->GetBinCenter(cBin);
                            }
                            else if(cContent > 0.85)
                            {
                                cFirst1 = channelSCurve->GetBinCenter(cBin);
                                break;
                            }
                        }

                        TF1* cFit = new TF1("SCurveFit", MyErf, cFirstNon0 - 10, cFirst1 + 10, 2);

                        // Get rough midpoint & width
                        double cMid   = (cFirst1 + cFirstNon0) * 0.5;
                        double cWidth = (cFirst1 - cFirstNon0) * 0.5;

                        cFit->SetParameter(0, cMid);
                        cFit->SetParameter(1, cWidth);

                        // Fit
                        channelSCurve->Fit(cFit, "RQ+0");

                        theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(cChannel).fThreshold      = cFit->GetParameter(0);
                        theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(cChannel).fNoise          = cFit->GetParameter(1);
                        theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(cChannel).fThresholdError = cFit->GetParError(0);
                        theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(cChannel).fNoiseError     = cFit->GetParError(1);
                    }
                }
            }
        }
    }

    fillPedestalAndNoisePlots(fThresholdAndNoiseContainer);
}
