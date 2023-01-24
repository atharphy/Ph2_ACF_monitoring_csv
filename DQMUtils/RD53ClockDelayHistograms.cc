/*!
  \file                  RD53ClockDelayHistograms.cc
  \brief                 Implementation of ClockDelay calibration histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ClockDelayHistograms.h"

using namespace Ph2_HwDescription;

void ClockDelayHistograms::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);
    RD53Shared::setFirstChip(theDetectorStructure);

    // #######################
    // # Retrieve parameters #
    // #######################
    const auto frontEnd = RD53Shared::firstChip->getFEtype(RD53Shared::firstChip->getNRows() / 2, RD53Shared::firstChip->getNCols() / 2);
    startValue          = 0;
    stopValue           = frontEnd->nLatencyBins2Span * (RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CLK_DATA_DELAY_CLK")) + 1) - 1;
    const auto unitTime = 1. / RD53Constants::ACCELERATOR_CLK * 1000 / ((RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CAL_EDGE_FINE_DELAY")) + 1) / (2. / frontEnd->nLatencyBins2Span));
    std::stringstream title;
    title << "Clock Delay (" << unitTime << " ns)";

    auto hClockDelay = CanvasContainer<TH1F>("ClockDelay", "Clock Delay", stopValue - startValue + 1, startValue, stopValue + 1);
    bookImplementer(theOutputFile, theDetectorStructure, ClockDelay, hClockDelay, title.str().c_str(), "Entries");

    auto hOcc1D = CanvasContainer<TH1F>("ClkDelayScan", "Clock Delay Scan", stopValue - startValue + 1, startValue, stopValue + 1);
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy1D, hOcc1D, title.str().c_str(), "Efficiency");
}

bool ClockDelayHistograms::fill(std::vector<char>& dataBuffer)
{
    const size_t ClkDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ChipContainerStream<EmptyContainer, GenericDataArray<ClkDelaySize>> theOccStreamer("ClockDelayOcc");
    ChipContainerStream<EmptyContainer, uint16_t>                       theClockDelayStreamer("ClockDelayClkDelay");

    if(theOccStreamer.attachBuffer(&dataBuffer))
    {
        theOccStreamer.decodeChipData(DetectorData);
        ClockDelayHistograms::fillOccupancy(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theClockDelayStreamer.attachBuffer(&dataBuffer))
    {
        theClockDelayStreamer.decodeChipData(DetectorData);
        ClockDelayHistograms::fillClockDelay(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }

    return false;
}

void ClockDelayHistograms::fillOccupancy(const DetectorDataContainer& OccupancyContainer)
{
    const size_t ClkDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(const auto cBoard: OccupancyContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<GenericDataArray<ClkDelaySize>>() == nullptr) continue;

                    auto* Occupancy1DHist = Occupancy1D.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<CanvasContainer<TH1F>>()
                                                .fTheHistogram;

                    for(size_t i = startValue; i <= stopValue; i++)
                        Occupancy1DHist->SetBinContent(Occupancy1DHist->FindBin(i), cChip->getSummary<GenericDataArray<ClkDelaySize>>().data[i - startValue]);
                }
}

void ClockDelayHistograms::fillClockDelay(const DetectorDataContainer& ClockDelayContainer)
{
    for(const auto cBoard: ClockDelayContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<uint16_t>() == nullptr) continue;

                    auto* ClockDelayHist = ClockDelay.getObject(cBoard->getId())
                                               ->getObject(cOpticalGroup->getId())
                                               ->getObject(cHybrid->getId())
                                               ->getObject(cChip->getId())
                                               ->getSummary<CanvasContainer<TH1F>>()
                                               .fTheHistogram;

                    ClockDelayHist->Fill(cChip->getSummary<uint16_t>());
                }
}

void ClockDelayHistograms::process()
{
    draw<TH1F>(Occupancy1D);
    draw<TH1F>(ClockDelay);
}
