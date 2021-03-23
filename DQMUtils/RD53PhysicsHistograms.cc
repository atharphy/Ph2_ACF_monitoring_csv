/*!
  \file                  RD53PhysicsHistograms.cc
  \brief                 Implementation of Physics histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53PhysicsHistograms.h"

using namespace Ph2_HwDescription;

void PhysicsHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    const size_t ToTsize   = RD53Shared::setBits(RD53EvtEncoder::NBIT_TOT / RD53Constants::NPIX_REGION) + 1;
    const size_t BCIDsize  = RD53Shared::setBits(RD53EvtEncoder::NBIT_BCID) + 1;
    const size_t TrgIDsize = RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1;

    auto hToT1D = CanvasContainer<TH1F>("ToT1D", "ToT Distribution", ToTsize, 0, ToTsize);
    bookImplementer(theOutputFile, theDetectorStructure, ToT1D, hToT1D, "ToT", "Entries");

    auto hToT2D = CanvasContainer<TH2F>("ToT2D", "ToT Distribution", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, ToT2D, hToT2D, "Columns", "Rows");

    auto hOcc2D = CanvasContainer<TH2F>("Occ2D", "Occupancy", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy2D, hOcc2D, "Columns", "Rows");

    auto hErrorReadOut2D = CanvasContainer<TH2F>("ReadoutErrors", "Readout Errors", RD53::nCols, 0, RD53::nCols, RD53::nRows, 0, RD53::nRows);
    bookImplementer(theOutputFile, theDetectorStructure, ErrorReadOut2D, hErrorReadOut2D, "Columns", "Rows");

    auto hBCID = CanvasContainer<TH1F>("BCID", "BCID", BCIDsize, 0, BCIDsize);
    bookImplementer(theOutputFile, theDetectorStructure, BCID, hBCID, "#DeltaBCID", "Entries");

    auto hTriggerID = CanvasContainer<TH1F>("TriggerID", "TriggerID", TrgIDsize, 0, TrgIDsize);
    bookImplementer(theOutputFile, theDetectorStructure, TriggerID, hTriggerID, "#DeltaTrigger-ID", "Entries");
}

bool PhysicsHistograms::fill(std::vector<char>& dataBuffer)
{
    const size_t BCIDsize  = RD53Shared::setBits(RD53EvtEncoder::NBIT_BCID) + 1;
    const size_t TrgIDsize = RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1;

    ChannelContainerStream<OccupancyAndPh>                           theOccStreamer("PhysicsOcc");
    ChipContainerStream<EmptyContainer, GenericDataArray<BCIDsize>>  theBCIDStreamer("PhysicsBCID");
    ChipContainerStream<EmptyContainer, GenericDataArray<TrgIDsize>> theTrgIDStreamer("PhysicsTrgID");

    if(theOccStreamer.attachBuffer(&dataBuffer))
    {
        theOccStreamer.decodeChipData(DetectorData);
        PhysicsHistograms::fill(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theBCIDStreamer.attachBuffer(&dataBuffer))
    {
        theBCIDStreamer.decodeChipData(DetectorData);
        PhysicsHistograms::fillBCID(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theTrgIDStreamer.attachBuffer(&dataBuffer))
    {
        theTrgIDStreamer.decodeChipData(DetectorData);
        PhysicsHistograms::fillTrgID(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }

    return false;
}

void PhysicsHistograms::fill(const DetectorDataContainer& DataContainer)
{
    for(const auto cBoard: DataContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getChannelContainer<OccupancyAndPh>() == nullptr) continue;

                    auto* ToT1DHist = ToT1D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;
                    auto* ToT2DHist = ToT2D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH2F>>().fTheHistogram;
                    auto* Occupancy2DHist =
                        Occupancy2D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH2F>>().fTheHistogram;
                    auto* ErrorReadOut2DHist =
                        ErrorReadOut2D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH2F>>().fTheHistogram;

                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                        {
                            if(cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy != 0)
                            {
                                ToT1DHist->Fill(cChip->getChannel<OccupancyAndPh>(row, col).fPh);
                                ToT2DHist->SetBinContent(col + 1, row + 1, ToT2DHist->GetBinContent(col + 1, row + 1) + cChip->getChannel<OccupancyAndPh>(row, col).fPh);
                                Occupancy2DHist->SetBinContent(col + 1, row + 1, Occupancy2DHist->GetBinContent(col + 1, row + 1) + cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy);
                            }

                            if(cChip->getChannel<OccupancyAndPh>(row, col).readoutError == true) ErrorReadOut2DHist->Fill(col + 1, row + 1);
                        }
                }
}

void PhysicsHistograms::fillBCID(const DetectorDataContainer& DataContainer)
{
    const size_t BCIDsize = RD53Shared::setBits(RD53EvtEncoder::NBIT_BCID) + 1;

    for(const auto cBoard: DataContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<GenericDataArray<BCIDsize>>() == nullptr) continue;

                    auto* BCIDHist = BCID.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;

                    for(auto i = 0u; i < BCIDsize; i++) BCIDHist->SetBinContent(i + 1, BCIDHist->GetBinContent(i + 1) + cChip->getSummary<GenericDataArray<BCIDsize>>().data[i]);
                }
}

void PhysicsHistograms::fillTrgID(const DetectorDataContainer& DataContainer)
{
    const size_t TrgIDsize = RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1;

    for(const auto cBoard: DataContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<GenericDataArray<TrgIDsize>>() == nullptr) continue;

                    auto* TriggerIDHist =
                        TriggerID.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;

                    for(auto i = 0u; i < TrgIDsize; i++) TriggerIDHist->SetBinContent(i + 1, TriggerIDHist->GetBinContent(i + 1) + cChip->getSummary<GenericDataArray<TrgIDsize>>().data[i]);
                }
}

void PhysicsHistograms::process()
{
    draw<TH1F>(ToT1D);
    draw<TH2F>(ToT2D, "gcolz");
    draw<TH2F>(Occupancy2D, "gcolz");
    draw<TH2F>(ErrorReadOut2D, "gcolz");
    draw<TH1F>(BCID);
    draw<TH1F>(TriggerID);
}
