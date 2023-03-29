/*!
  \file                  RD53InjectionDelayHistograms.cc
  \brief                 Implementation of InjectionDelay calibration histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53InjectionDelayHistograms.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;

void InjectionDelayHistograms::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& settingsMap)
{
    fDetectorContainer = &theDetectorStructure;
    RD53Shared::setFirstChip(theDetectorStructure);

    // #######################
    // # Retrieve parameters #
    // #######################
    const auto frontEnd = RD53Shared::firstChip->getFEtype(RD53Shared::firstChip->getNRows() / 2, RD53Shared::firstChip->getNCols() / 2);
    startValue          = 0;
    stopValue           = frontEnd->nLatencyBins2Span * (RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CAL_EDGE_FINE_DELAY")) + 1) - 1;
    const auto unitTime = 1. / RD53Constants::ACCELERATOR_CLK * 1000 / ((RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CAL_EDGE_FINE_DELAY")) + 1) / (2. / frontEnd->nLatencyBins2Span));
    std::stringstream title;
    title << "Injection Delay (" << unitTime << " ns)";

    auto hInjectionDelay = CanvasContainer<TH1F>("InjectionDelay", "Injection Delay", stopValue - startValue + 1, startValue, stopValue + 1);
    bookImplementer(theOutputFile, theDetectorStructure, InjectionDelay, hInjectionDelay, title.str().c_str(), "Entries");

    auto hOcc1D = CanvasContainer<TH1F>("InjDelayScan", "Injection Delay Scan", stopValue - startValue + 1, startValue, stopValue + 1);
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy1D, hOcc1D, title.str().c_str(), "Efficiency");
}

bool InjectionDelayHistograms::fill(std::string& inputStream)
{
    const size_t InjDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ContainerSerialization theOccupancySerialization("InjectionDelayOccupancy");
    ContainerSerialization theInjectionDelaySerialization("InjectionDelayInjectionDelay");

    if(theOccupancySerialization.attachDeserializer(inputStream))
    {
        DetectorDataContainer fDetectorData = theOccupancySerialization.deserializeChipContainer<EmptyContainer, GenericDataArray<InjDelaySize>>(fDetectorContainer);
        InjectionDelayHistograms::fillOccupancy(fDetectorData);
        return true;
    }
    if(theInjectionDelaySerialization.attachDeserializer(inputStream))
    {
        DetectorDataContainer fDetectorData = theInjectionDelaySerialization.deserializeChipContainer<EmptyContainer, uint16_t>(fDetectorContainer);
        InjectionDelayHistograms::fillInjectionDelay(fDetectorData);
        return true;
    }
    return false;
}

void InjectionDelayHistograms::fillOccupancy(const DetectorDataContainer& OccupancyContainer)
{
    const size_t InjDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(const auto cBoard: OccupancyContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->hasSummary() == false) continue;

                    auto* Occupancy1DHist = Occupancy1D.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<CanvasContainer<TH1F>>()
                                                .fTheHistogram;

                    for(size_t i = startValue; i <= stopValue; i++)
                        Occupancy1DHist->SetBinContent(Occupancy1DHist->FindBin(i), cChip->getSummary<GenericDataArray<InjDelaySize>>().data[i - startValue]);
                }
}

void InjectionDelayHistograms::fillInjectionDelay(const DetectorDataContainer& InjectionDelayContainer)
{
    for(const auto cBoard: InjectionDelayContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->hasSummary() == false) continue;

                    auto* InjectionDelayHist = InjectionDelay.getObject(cBoard->getId())
                                                   ->getObject(cOpticalGroup->getId())
                                                   ->getObject(cHybrid->getId())
                                                   ->getObject(cChip->getId())
                                                   ->getSummary<CanvasContainer<TH1F>>()
                                                   .fTheHistogram;

                    InjectionDelayHist->Fill(cChip->getSummary<uint16_t>());
                }
}

void InjectionDelayHistograms::process()
{
    draw<TH1F>(Occupancy1D);
    draw<TH1F>(InjectionDelay);
}
