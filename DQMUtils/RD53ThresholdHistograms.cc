/*!
  \file                  RD53ThresholdHistograms.cc
  \brief                 Implementation of Threshold calibration histograms
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ThresholdHistograms.h"

using namespace Ph2_HwDescription;

void ThresholdHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    uint16_t rangeThreshold = RD53Shared::setBits(static_cast<RD53*>(theDetectorStructure.at(0)->at(0)->at(0)->at(0))->getNumberOfBits("Vthreshold_LIN")) + 1;

    auto hThrehsold = CanvasContainer<TH1F>("Threhsold", "Threhsold", rangeThreshold, 0, rangeThreshold);
    bookImplementer(theOutputFile, theDetectorStructure, Threhsold, hThrehsold, "Threhsold", "Entries");
}

bool ThresholdHistograms::fill(std::vector<char>& dataBuffer)
{
    ChipContainerStream<EmptyContainer, uint16_t> theThrStreamer("Threshold");

    if(theThrStreamer.attachBuffer(&dataBuffer))
    {
        theThrStreamer.decodeChipData(DetectorData);
        ThresholdHistograms::fill(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }

    return false;
}

void ThresholdHistograms::fill(const DetectorDataContainer& DataContainer)
{
    for(const auto cBoard: DataContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<uint16_t>() == nullptr) continue;

                    auto* hThrehsold =
                        Threhsold.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;

                    hThrehsold->Fill(cChip->getSummary<uint16_t>());
                }
}

void ThresholdHistograms::process() { draw<TH1F>(Threhsold); }
