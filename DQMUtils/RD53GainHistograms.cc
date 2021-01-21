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
    nEvents    = this->findValueInSettings(settingsMap, "nEvents");
    nSteps     = this->findValueInSettings(settingsMap, "VCalHnsteps");
    startValue = this->findValueInSettings(settingsMap, "VCalHstart");
    stopValue  = this->findValueInSettings(settingsMap, "VCalHstop");
    offset     = this->findValueInSettings(settingsMap, "VCalMED");

    auto hOcc2D = CanvasContainer<TH2F>("Gain", "Gain", nSteps, startValue - offset, stopValue - offset, nEvents, 0, RD53Shared::setBits(RD53EvtEncoder::NBIT_TOT / RD53Constants::NPIX_REGION));
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy2D, hOcc2D, "#DeltaVCal", "ToT");

    auto hErrorReadOut2D = CanvasContainer<TH2F>("ReadoutErrors", "Readout Errors", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, ErrorReadOut2D, hErrorReadOut2D, "Columns", "Rows");

    auto hErrorFit2D = CanvasContainer<TH2F>("FitErrors", "Fit Errors", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, ErrorFit2D, hErrorFit2D, "Columns", "Rows");

    auto hGain1D = CanvasContainer<TH1F>("Gain1D", "Gain1D", 100, 0, 20e-3);
    bookImplementer(theOutputFile, theDetectorStructure, Gain1D, hGain1D, "Gain (ToT/VCal)", "Entries");

    auto hIntercept1D = CanvasContainer<TH1F>("Intercept1D", "Intercept1D", 100, -INTERCEPT_HALFRANGE, INTERCEPT_HALFRANGE);
    bookImplementer(theOutputFile, theDetectorStructure, Intercept1D, hIntercept1D, "Intercept (ToT)", "Entries");

    auto hGain2D = CanvasContainer<TH2F>("Gain2D", "Gain Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, Gain2D, hGain2D, "Column", "Row");

    auto hIntercept2D = CanvasContainer<TH2F>("Intercept2D", "Intercept Map", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, Intercept2D, hIntercept2D, "Column", "Row");
}

bool GainHistograms::fill(std::vector<char>& dataBuffer)
{
    ChannelContainerStream<OccupancyAndPh, uint16_t> theOccStreamer("GainOcc");
    ChannelContainerStream<GainAndIntercept>         theGainAndInterceptStreamer("GainGainAndIntercept");

    if(theOccStreamer.attachBuffer(&dataBuffer))
    {
        theOccStreamer.decodeChipData(DetectorData);
        GainHistograms::fillOccupancy(DetectorData, theOccStreamer.getHeaderElement());
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theGainAndInterceptStreamer.attachBuffer(&dataBuffer))
    {
        theGainAndInterceptStreamer.decodeChipData(DetectorData);
        GainHistograms::fillGainAndIntercept(DetectorData);
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

                    auto* hOcc2D = Occupancy2D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH2F>>().fTheHistogram;
                    auto* ErrorReadOut2DHist =
                        ErrorReadOut2D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH2F>>().fTheHistogram;

                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                        {
                            if(cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy != RD53Shared::ISDISABLED) hOcc2D->Fill(DELTA_VCAL, cChip->getChannel<OccupancyAndPh>(row, col).fPh);
                            if(cChip->getChannel<OccupancyAndPh>(row, col).readoutError == true) ErrorReadOut2DHist->Fill(col + 1, row + 1);
                        }
                }
}

void GainHistograms::fillGainAndIntercept(const DetectorDataContainer& GainAndInterceptContainer)
{
    for(const auto cBoard: GainAndInterceptContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getChannelContainer<GainAndIntercept>() == nullptr) continue;

                    auto* Gain1DHist = Gain1D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;
                    auto* Intercept1DHist =
                        Intercept1D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;
                    auto* Gain2DHist = Gain2D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH2F>>().fTheHistogram;
                    auto* Intercept2DHist =
                        Intercept2D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH2F>>().fTheHistogram;
                    auto* ErrorFit2DHist =
                        ErrorFit2D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH2F>>().fTheHistogram;

                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                            if(cChip->getChannel<GainAndIntercept>(row, col).fGain == RD53Shared::FITERROR)
                                ErrorFit2DHist->Fill(col + 1, row + 1);
                            else if(cChip->getChannel<GainAndIntercept>(row, col).fGain != 0)
                            {
                                Gain1DHist->Fill(cChip->getChannel<GainAndIntercept>(row, col).fGain);
                                Intercept1DHist->Fill(cChip->getChannel<GainAndIntercept>(row, col).fIntercept);
                                Gain2DHist->SetBinContent(col + 1, row + 1, cChip->getChannel<GainAndIntercept>(row, col).fGain);
                                Intercept2DHist->SetBinContent(col + 1, row + 1, cChip->getChannel<GainAndIntercept>(row, col).fIntercept);
                            }
                }
}

void GainHistograms::process()
{
    draw<TH2F>(Occupancy2D, "gcolz", true, "Charge (electrons)");
    draw<TH2F>(ErrorReadOut2D, "gcolz");
    draw<TH2F>(ErrorFit2D, "gcolz");
    draw<TH1F>(Gain1D, "", true, "Gain (ToT/electrons)");
    draw<TH1F>(Intercept1D);
    draw<TH2F>(Gain2D, "gcolz");
    draw<TH2F>(Intercept2D, "gcolz");
}
