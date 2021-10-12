/*!
  \file                  RD53LatencyHistograms.cc
  \brief                 Implementation of Latency calibration histograms
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53LatencyHistograms.h"

using namespace Ph2_HwDescription;

void LatencyHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    // #######################
    // # Retrieve parameters #
    // #######################
    nTRIGxEvent = this->findValueInSettings<double>(settingsMap, "nTRIGxEvent");
    startValue  = this->findValueInSettings<double>(settingsMap, "LatencyStart");
    stopValue   = this->findValueInSettings<double>(settingsMap, "LatencyStop");

    auto hLatency = CanvasContainer<TH1F>("Latency", "Latency", stopValue - startValue + nTRIGxEvent, startValue, stopValue + nTRIGxEvent);
    bookImplementer(theOutputFile, theDetectorStructure, Latency, hLatency, "Latency (n.bx)", "Entries");

    auto hOcc1D = CanvasContainer<TH1F>("LatencyScan", "Latency Scan", stopValue - startValue + nTRIGxEvent, startValue, stopValue + nTRIGxEvent);
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy1D, hOcc1D, "Latency (n.bx)", "Efficiency");
}

bool LatencyHistograms::fill(std::vector<char>& dataBuffer)
{
    const size_t LatencySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ChipContainerStream<EmptyContainer, GenericDataArray<LatencySize>> theOccStreamer("LatencyOcc");
    ChipContainerStream<EmptyContainer, uint16_t>                      theLatencyStreamer("LatencyLatency");

    if(theOccStreamer.attachBuffer(&dataBuffer))
    {
        theOccStreamer.decodeChipData(DetectorData);
        LatencyHistograms::fillOccupancy(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theLatencyStreamer.attachBuffer(&dataBuffer))
    {
        theLatencyStreamer.decodeChipData(DetectorData);
        LatencyHistograms::fillLatency(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }

    return false;
}

void LatencyHistograms::fillOccupancy(const DetectorDataContainer& OccupancyContainer)
{
    const size_t LatencySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(const auto cBoard: OccupancyContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<GenericDataArray<LatencySize>>() == nullptr) continue;

                    auto* Occupancy1DHist = Occupancy1D.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<CanvasContainer<TH1F>>()
                                                .fTheHistogram;

                    for(size_t i = startValue; i <= stopValue; i += nTRIGxEvent)
                        Occupancy1DHist->SetBinContent(Occupancy1DHist->FindBin(i), cChip->getSummary<GenericDataArray<LatencySize>>().data[(i - startValue) / nTRIGxEvent]);
                }
}

void LatencyHistograms::fillLatency(const DetectorDataContainer& LatencyContainer)
{
    for(const auto cBoard: LatencyContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<uint16_t>() == nullptr) continue;

                    auto* LatencyHist = Latency.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<CanvasContainer<TH1F>>()
                                            .fTheHistogram;

                    for(auto i = 0u; i < nTRIGxEvent; i++) LatencyHist->Fill(cChip->getSummary<uint16_t>() - i);
                }
}

void LatencyHistograms::process()
{
    draw<TH1F>(Occupancy1D);
    draw<TH1F>(Latency);
}
