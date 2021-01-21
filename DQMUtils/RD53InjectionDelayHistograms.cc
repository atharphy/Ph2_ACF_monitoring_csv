/*!
  \file                  RD53InjectionDelayHistograms.cc
  \brief                 Implementation of InjectionDelay calibration histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53InjectionDelayHistograms.h"

using namespace Ph2_HwDescription;

void InjectionDelayHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    // #######################
    // # Retrieve parameters #
    // #######################
    startValue = 0;
    stopValue  = RD53Shared::NLATENCYBINS * (RD53Shared::setBits(static_cast<RD53*>(theDetectorStructure.at(0)->at(0)->at(0)->at(0))->getNumberOfBits("INJECTION_SELECT_DELAY")) + 1) - 1;

    auto hInjectionDelay = CanvasContainer<TH1F>("InjectionDelay", "Injection Delay", stopValue - startValue + 1, startValue, stopValue + 1);
    bookImplementer(theOutputFile, theDetectorStructure, InjectionDelay, hInjectionDelay, "Injection Delay (1.5625 ns)", "Entries");

    auto hOcc1D = CanvasContainer<TH1F>("InjDelayScan", "Injection Delay Scan", stopValue - startValue + 1, startValue, stopValue + 1);
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy1D, hOcc1D, "Injection Delay (1.5625 ns)", "Efficiency");
}

bool InjectionDelayHistograms::fill(std::vector<char>& dataBuffer)
{
    const size_t InjDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ChipContainerStream<EmptyContainer, GenericDataArray<InjDelaySize>> theOccStreamer("InjectionDelayOcc");
    ChipContainerStream<EmptyContainer, uint16_t>                       theInjectionDelayStreamer("InjectionDelayInjDelay");

    if(theOccStreamer.attachBuffer(&dataBuffer))
    {
        theOccStreamer.decodeChipData(DetectorData);
        InjectionDelayHistograms::fillOccupancy(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theInjectionDelayStreamer.attachBuffer(&dataBuffer))
    {
        theInjectionDelayStreamer.decodeChipData(DetectorData);
        InjectionDelayHistograms::fillInjectionDelay(DetectorData);
        DetectorData.cleanDataStored();
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
                    if(cChip->getSummaryContainer<GenericDataArray<InjDelaySize>>() == nullptr) continue;

                    auto* Occupancy1DHist =
                        Occupancy1D.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;

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
                    if(cChip->getSummaryContainer<uint16_t>() == nullptr) continue;

                    auto* InjectionDelayHist =
                        InjectionDelay.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;

                    InjectionDelayHist->Fill(cChip->getSummary<uint16_t>());
                }
}

void InjectionDelayHistograms::process()
{
    draw<TH1F>(Occupancy1D);
    draw<TH1F>(InjectionDelay);
}
