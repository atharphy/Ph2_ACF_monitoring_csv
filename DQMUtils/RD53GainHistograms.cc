/*!
  \file                  RD53GainHistograms.cc
  \brief                 Implementation of Gain calibration histograms
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53GainHistograms.h"

using namespace Ph2_HwDescription;

void GainHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
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

    auto hOcc2D = CanvasContainer<TH2F>("Gain", "Gain", nSteps, startValue - offset, stopValue - offset, nEvents, 0, RD53Shared::setBits(RD53EvtEncoder::NBIT_TOT / RD53Constants::NPIX_REGION));
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy2D, hOcc2D, "#DeltaVCal", "ToT");

    auto hOcc3D = CanvasContainer<TH3F>("GainMap", "Gain Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows, nSteps, startValue - offset, stopValue - offset);
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy3D, hOcc3D, "Column", "Row", "#DeltaVCal");

    auto hErrorReadOut2D = CanvasContainer<TH2F>("ReadoutErrors", "Readout Errors", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, ErrorReadOut2D, hErrorReadOut2D, "Columns", "Rows");

    auto hErrorFit2D = CanvasContainer<TH2F>("FitErrors", "Fit Errors", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, ErrorFit2D, hErrorFit2D, "Columns", "Rows");

    auto hIntercept1D = CanvasContainer<TH1F>("Intercept1D", "Intercept1D", 100, -INTERCEPT_HALFRANGE, INTERCEPT_HALFRANGE);
    bookImplementer(theOutputFile, theDetectorStructure, Intercept1D, hIntercept1D, "Intercept (ToT)", "Entries");

    auto hSlope1D = CanvasContainer<TH1F>("Slope1D", "Slope1D", 100, -SLOPE_HALFRANGE, SLOPE_HALFRANGE);
    bookImplementer(theOutputFile, theDetectorStructure, Slope1D, hSlope1D, "Slope (ToT/VCal)", "Entries");

    auto hInterceptLowQ1D = CanvasContainer<TH1F>("InterceptLowQ1D", "InterceptLowQ1D", 100, -INTERCEPT_HALFRANGE, INTERCEPT_HALFRANGE);
    bookImplementer(theOutputFile, theDetectorStructure, InterceptLowQ1D, hInterceptLowQ1D, "Intercept for low charge range (ToT)", "Entries");

    auto hSlopeLowQ1D = CanvasContainer<TH1F>("SlopeLowQ1D", "SlopeLowQ1D", 100, -SLOPE_HALFRANGE, SLOPE_HALFRANGE);
    bookImplementer(theOutputFile, theDetectorStructure, SlopeLowQ1D, hSlopeLowQ1D, "Slope for low charge range (ToT/VCal)", "Entries");

    auto hChi2DoF1D = CanvasContainer<TH1F>("Chi2DoF1D", "Chi2DoF1D", 100, 0, 2);
    bookImplementer(theOutputFile, theDetectorStructure, Chi2DoF1D, hChi2DoF1D, "#chi^{2}/D.o.F.", "Entries");

    auto hIntercept2D = CanvasContainer<TH2F>("Intercept2D", "Intercept Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, Intercept2D, hIntercept2D, "Column", "Row");

    auto hSlope2D = CanvasContainer<TH2F>("Slope2D", "Slope Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, Slope2D, hSlope2D, "Column", "Row");

    auto hInterceptLowQ2D = CanvasContainer<TH2F>("InterceptLowQ2D", "InterceptLowQ Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, InterceptLowQ2D, hInterceptLowQ2D, "Column", "Row");

    auto hSlopeLowQ2D = CanvasContainer<TH2F>("SlopeLowQ2D", "SlopeLowQ Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, SlopeLowQ2D, hSlopeLowQ2D, "Column", "Row");

    auto hChi2DoF2D = CanvasContainer<TH2F>("Chi2DoF2D", "Chi2DoF Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, Chi2DoF2D, hChi2DoF2D, "Column", "Row");
}

bool GainHistograms::fill(std::vector<char>& dataBuffer)
{
    ChannelContainerStream<OccupancyAndPh, uint16_t> theOccStreamer("GainOcc");
    ChannelContainerStream<GainFit>                  theGainStreamer("GainGain");

    if(theOccStreamer.attachBuffer(&dataBuffer))
    {
        theOccStreamer.decodeChipData(DetectorData);
        GainHistograms::fillOccupancy(DetectorData, theOccStreamer.getHeaderElement());
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theGainStreamer.attachBuffer(&dataBuffer))
    {
        theGainStreamer.decodeChipData(DetectorData);
        GainHistograms::fillGain(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }

    return false;
}

void GainHistograms::fillOccupancy(const DetectorDataContainer& OccupancyContainer, int DELTA_VCAL)
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

                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                        {
                            if(cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy != RD53Shared::ISDISABLED)
                            {
                                hOcc2D->Fill(DELTA_VCAL, cChip->getChannel<OccupancyAndPh>(row, col).fPh);
                                hOcc3D->SetBinContent(col + 1, row + 1, hOcc3D->GetZaxis()->FindBin(DELTA_VCAL), cChip->getChannel<OccupancyAndPh>(row, col).fPh);
                            }
                            if(cChip->getChannel<OccupancyAndPh>(row, col).readoutError == true) ErrorReadOut2DHist->Fill(col + 1, row + 1);
                        }
                }
}

void GainHistograms::fillGain(const DetectorDataContainer& GainContainer)
{
    for(const auto cBoard: GainContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getChannelContainer<GainFit>() == nullptr) continue;

                    auto* Intercept1DHist = Intercept1D.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<CanvasContainer<TH1F>>()
                                                .fTheHistogram;
                    auto* Slope1DHist = Slope1D.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<CanvasContainer<TH1F>>()
                                            .fTheHistogram;
                    auto* InterceptLowQ1DHist = InterceptLowQ1D.getObject(cBoard->getId())
                                                    ->getObject(cOpticalGroup->getId())
                                                    ->getObject(cHybrid->getId())
                                                    ->getObject(cChip->getId())
                                                    ->getSummary<CanvasContainer<TH1F>>()
                                                    .fTheHistogram;
                    auto* SlopeLowQ1DHist = SlopeLowQ1D.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<CanvasContainer<TH1F>>()
                                                .fTheHistogram;
                    auto* Chi2DoF1DHist = Chi2DoF1D.getObject(cBoard->getId())
                                              ->getObject(cOpticalGroup->getId())
                                              ->getObject(cHybrid->getId())
                                              ->getObject(cChip->getId())
                                              ->getSummary<CanvasContainer<TH1F>>()
                                              .fTheHistogram;

                    auto* Intercept2DHist = Intercept2D.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<CanvasContainer<TH2F>>()
                                                .fTheHistogram;
                    auto* Slope2DHist = Slope2D.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<CanvasContainer<TH2F>>()
                                            .fTheHistogram;
                    auto* InterceptLowQ2DHist = InterceptLowQ2D.getObject(cBoard->getId())
                                                    ->getObject(cOpticalGroup->getId())
                                                    ->getObject(cHybrid->getId())
                                                    ->getObject(cChip->getId())
                                                    ->getSummary<CanvasContainer<TH2F>>()
                                                    .fTheHistogram;
                    auto* SlopeLowQ2DHist = SlopeLowQ2D.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<CanvasContainer<TH2F>>()
                                                .fTheHistogram;

                    auto* Chi2DoF2DHist = Chi2DoF2D.getObject(cBoard->getId())
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
                            if(cChip->getChannel<GainFit>(row, col).fChi2 == RD53Shared::FITERROR)
                                ErrorFit2DHist->Fill(col + 1, row + 1);
                            else if(cChip->getChannel<GainFit>(row, col).fChi2 != 0)
                            {
                                // #################
                                // # 1D histograms #
                                // #################
                                Intercept1DHist->Fill(cChip->getChannel<GainFit>(row, col).fIntercept);
                                Slope1DHist->Fill(cChip->getChannel<GainFit>(row, col).fSlope);
                                InterceptLowQ1DHist->Fill(cChip->getChannel<GainFit>(row, col).fInterceptLowQ);
                                SlopeLowQ1DHist->Fill(cChip->getChannel<GainFit>(row, col).fSlopeLowQ);
                                Chi2DoF1DHist->Fill(cChip->getChannel<GainFit>(row, col).fChi2 / cChip->getChannel<GainFit>(row, col).fDoF);

                                // #################
                                // # 2D histograms #
                                // #################
                                Slope2DHist->SetBinContent(col + 1, row + 1, cChip->getChannel<GainFit>(row, col).fSlope);
                                Slope2DHist->SetBinError(col + 1, row + 1, cChip->getChannel<GainFit>(row, col).fSlopeError);
                                Intercept2DHist->SetBinContent(col + 1, row + 1, cChip->getChannel<GainFit>(row, col).fIntercept);
                                Intercept2DHist->SetBinError(col + 1, row + 1, cChip->getChannel<GainFit>(row, col).fInterceptError);
                                InterceptLowQ2DHist->SetBinContent(col + 1, row + 1, cChip->getChannel<GainFit>(row, col).fInterceptLowQ);
                                InterceptLowQ2DHist->SetBinError(col + 1, row + 1, cChip->getChannel<GainFit>(row, col).fInterceptLowQError);
                                SlopeLowQ2DHist->SetBinContent(col + 1, row + 1, cChip->getChannel<GainFit>(row, col).fSlopeLowQ);
                                SlopeLowQ2DHist->SetBinError(col + 1, row + 1, cChip->getChannel<GainFit>(row, col).fSlopeLowQError);
                                Chi2DoF2DHist->SetBinContent(col + 1, row + 1, cChip->getChannel<GainFit>(row, col).fChi2 / cChip->getChannel<GainFit>(row, col).fDoF);
                            }
                }
}

void GainHistograms::process()
{
    draw<TH2F>(Occupancy2D, "gcolz", "electron", "Charge (electrons)");
    draw<TH3F>(Occupancy3D, "gcolz");
    draw<TH2F>(ErrorReadOut2D, "gcolz");
    draw<TH2F>(ErrorFit2D, "gcolz");

    draw<TH1F>(Intercept1D);
    draw<TH1F>(Slope1D, "", "electron", "Slope (ToT/electrons)");
    draw<TH1F>(InterceptLowQ1D);
    draw<TH1F>(SlopeLowQ1D, "", "electron", "Slope for low charge range (ToT/electrons)");
    draw<TH1F>(Chi2DoF1D);

    draw<TH2F>(Intercept2D, "gcolz");
    draw<TH2F>(Slope2D, "gcolz");
    draw<TH2F>(InterceptLowQ2D, "gcolz");
    draw<TH2F>(SlopeLowQ2D, "gcolz");
    draw<TH2F>(Chi2DoF2D, "gcolz");
}
