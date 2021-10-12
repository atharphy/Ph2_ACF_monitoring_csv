/*!
  \file                  RD53SCurveHistograms.cc
  \brief                 Implementation of SCurve calibration histograms
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53SCurveHistograms.h"

using namespace Ph2_HwDescription;

void SCurveHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    // #######################
    // # Retrieve parameters #
    // #######################
    nEvents    = this->findValueInSettings<double>(settingsMap, "nEvents");
    nSteps     = this->findValueInSettings<double>(settingsMap, "VCalHnsteps");
    startValue = this->findValueInSettings<double>(settingsMap, "VCalHstart");
    stopValue  = this->findValueInSettings<double>(settingsMap, "VCalHstop");
    offset     = this->findValueInSettings<double>(settingsMap, "VCalMED");

    auto hOcc2D = CanvasContainer<TH2F>("SCurves", "SCurves", nSteps, startValue - offset, stopValue - offset, 2 * nEvents + 1, 0, 2 + 1. / nEvents);
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy2D, hOcc2D, "#DeltaVCal", "Efficiency");

    auto hOcc3D = CanvasContainer<TH3F>("SCurveMap", "SCurve Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows, nSteps, startValue - offset, stopValue - offset);
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy3D, hOcc3D, "Column", "Row", "#DeltaVCal");

    auto hErrorReadOut2D = CanvasContainer<TH2F>("ReadoutErrors", "Readout Errors", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, ErrorReadOut2D, hErrorReadOut2D, "Columns", "Rows");

    auto hErrorFit2D = CanvasContainer<TH2F>("FitErrors", "Fit Errors", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, ErrorFit2D, hErrorFit2D, "Columns", "Rows");

    auto hThreshold1D = CanvasContainer<TH1F>("Threshold1D", "Threshold Distribution", 1000, startValue - offset, stopValue - offset);
    bookImplementer(theOutputFile, theDetectorStructure, Threshold1D, hThreshold1D, "Threshold (#DeltaVCal)", "Entries");

    auto hNoise1D = CanvasContainer<TH1F>("Noise1D", "Noise Distribution", 100, 0, 50);
    bookImplementer(theOutputFile, theDetectorStructure, Noise1D, hNoise1D, "Noise (#DeltaVCal)", "Entries");

    auto hThreshold2D = CanvasContainer<TH2F>("Threshold2D", "Threshold Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, Threshold2D, hThreshold2D, "Column", "Row");

    auto hNoise2D = CanvasContainer<TH2F>("Noise2D", "Noise Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, Noise2D, hNoise2D, "Column", "Row");

    auto hToT2D = CanvasContainer<TH2F>("ToT2D", "Integrated ToT Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, ToT2D, hToT2D, "Columns", "Rows");
}

bool SCurveHistograms::fill(std::vector<char>& dataBuffer)
{
    ChannelContainerStream<OccupancyAndPh, uint16_t> theOccStreamer("SCurveOcc");
    ChannelContainerStream<ThresholdAndNoise>        theThrAndNoiseStreamer("SCurveThrAndNoise");

    if(theOccStreamer.attachBuffer(&dataBuffer))
    {
        theOccStreamer.decodeChipData(DetectorData);
        SCurveHistograms::fillOccupancy(DetectorData, theOccStreamer.getHeaderElement());
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theThrAndNoiseStreamer.attachBuffer(&dataBuffer))
    {
        theThrAndNoiseStreamer.decodeChipData(DetectorData);
        SCurveHistograms::fillThrAndNoise(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }

    return false;
}

void SCurveHistograms::fillOccupancy(const DetectorDataContainer& OccupancyContainer, int DELTA_VCAL)
{
    for(const auto cBoard: OccupancyContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getChannelContainer<OccupancyAndPh>() == nullptr) continue;

                    auto* hOcc2D = Occupancy2D.getObject(cBoard->getId())
                                       ->getObject(cOpticalGroup->getId())
                                       ->getObject(cHybrid->getId())
                                       ->getObject(cChip->getId())
                                       ->getSummary<CanvasContainer<TH2F>>()
                                       .fTheHistogram;
                    auto* hOcc3D = Occupancy3D.getObject(cBoard->getId())
                                       ->getObject(cOpticalGroup->getId())
                                       ->getObject(cHybrid->getId())
                                       ->getObject(cChip->getId())
                                       ->getSummary<CanvasContainer<TH3F>>()
                                       .fTheHistogram;
                    auto* ErrorReadOut2DHist = ErrorReadOut2D.getObject(cBoard->getId())
                                                   ->getObject(cOpticalGroup->getId())
                                                   ->getObject(cHybrid->getId())
                                                   ->getObject(cChip->getId())
                                                   ->getSummary<CanvasContainer<TH2F>>()
                                                   .fTheHistogram;
                    auto* ToT2DHist =
                        ToT2D.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<CanvasContainer<TH2F>>().fTheHistogram;

                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                        {
                            if(cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy != RD53Shared::ISDISABLED)
                            {
                                hOcc2D->Fill(DELTA_VCAL, cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy + hOcc2D->GetYaxis()->GetBinWidth(0) / 2.);
                                hOcc3D->SetBinContent(col + 1, row + 1, hOcc3D->GetZaxis()->FindBin(DELTA_VCAL), cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy);
                                ToT2DHist->SetBinContent(col + 1, row + 1, ToT2DHist->GetBinContent(col + 1, row + 1) + cChip->getChannel<OccupancyAndPh>(row, col).fPh);
                                ToT2DHist->SetBinError(col + 1,
                                                       row + 1,
                                                       sqrt(ToT2DHist->GetBinError(col + 1, row + 1) * ToT2DHist->GetBinError(col + 1, row + 1) +
                                                            cChip->getChannel<OccupancyAndPh>(row, col).fPhError * cChip->getChannel<OccupancyAndPh>(row, col).fPhError));
                            }
                            if(cChip->getChannel<OccupancyAndPh>(row, col).readoutError == true) ErrorReadOut2DHist->Fill(col + 1, row + 1);
                        }

                    hOcc2D->GetYaxis()->SetRangeUser(0, 1 + 1. / nEvents);
                }
}

void SCurveHistograms::fillThrAndNoise(const DetectorDataContainer& ThrAndNoiseContainer)
{
    for(const auto cBoard: ThrAndNoiseContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getChannelContainer<ThresholdAndNoise>() == nullptr) continue;

                    auto* Threshold1DHist = Threshold1D.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<CanvasContainer<TH1F>>()
                                                .fTheHistogram;
                    auto* Noise1DHist = Noise1D.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<CanvasContainer<TH1F>>()
                                            .fTheHistogram;
                    auto* Threshold2DHist = Threshold2D.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<CanvasContainer<TH2F>>()
                                                .fTheHistogram;
                    auto* Noise2DHist = Noise2D.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<CanvasContainer<TH2F>>()
                                            .fTheHistogram;
                    auto* ErrorFit2DHist = ErrorFit2D.getObject(cBoard->getId())
                                               ->getObject(cOpticalGroup->getId())
                                               ->getObject(cHybrid->getId())
                                               ->getObject(cChip->getId())
                                               ->getSummary<CanvasContainer<TH2F>>()
                                               .fTheHistogram;

                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                            if(cChip->getChannel<ThresholdAndNoise>(row, col).fNoise == RD53Shared::FITERROR)
                                ErrorFit2DHist->Fill(col + 1, row + 1);
                            else if(cChip->getChannel<ThresholdAndNoise>(row, col).fNoise != 0)
                            {
                                // #################
                                // # 1D histograms #
                                // #################
                                Threshold1DHist->Fill(cChip->getChannel<ThresholdAndNoise>(row, col).fThreshold);
                                Noise1DHist->Fill(cChip->getChannel<ThresholdAndNoise>(row, col).fNoise);

                                // #################
                                // # 2D histograms #
                                // #################
                                Threshold2DHist->SetBinContent(col + 1, row + 1, cChip->getChannel<ThresholdAndNoise>(row, col).fThreshold);
                                Threshold2DHist->SetBinError(col + 1, row + 1, cChip->getChannel<ThresholdAndNoise>(row, col).fThresholdError);
                                Noise2DHist->SetBinContent(col + 1, row + 1, cChip->getChannel<ThresholdAndNoise>(row, col).fNoise);
                                Noise2DHist->SetBinError(col + 1, row + 1, cChip->getChannel<ThresholdAndNoise>(row, col).fNoiseError);
                            }
                }
}

void SCurveHistograms::process()
{
    draw<TH2F>(Occupancy2D, "gcolz", "electron", "Charge (electrons)");
    draw<TH3F>(Occupancy3D, "gcolz");
    draw<TH2F>(ErrorReadOut2D, "gcolz");
    draw<TH2F>(ErrorFit2D, "gcolz");
    draw<TH1F>(Threshold1D, "", "electron", "Threshold (electrons)");
    draw<TH1F>(Noise1D, "", "electron", "Noise (electrons)", true);
    draw<TH2F>(Threshold2D, "gcolz");
    draw<TH2F>(Noise2D, "gcolz");
    draw<TH2F>(ToT2D, "gcolz");
}
