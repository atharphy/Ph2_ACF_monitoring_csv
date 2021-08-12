/*!
  \file                  RD53BERtestHistograms.cc
  \brief                 Implementation of BERtest calibration histograms
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53BERtestHistograms.h"

using namespace Ph2_HwDescription;

void BERtestHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    auto hBERtest = CanvasContainer<TH1F>("BERtest", "BERtest", 1, 0, 1);
    bookImplementer(theOutputFile, theDetectorStructure, BERtest, hBERtest, "N.A.", "Bit Error Rate value");
}

bool BERtestHistograms::fill(std::vector<char>& dataBuffer)
{
    ChipContainerStream<EmptyContainer, double> theStreamer("BERtest");

    if(theStreamer.attachBuffer(&dataBuffer))
    {
        theStreamer.decodeChipData(DetectorData);
        BERtestHistograms::fillBERtest(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }

    return false;
}

void BERtestHistograms::fillBERtest(const DetectorDataContainer& BERtestContainer)
{
    for(const auto cBoard: BERtestContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<uint16_t>() == nullptr) continue;

                    auto* BERtestHist = BERtest.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<CanvasContainer<TH1F>>()
                                            .fTheHistogram;

                    BERtestHist->SetBinContent(1, cChip->getSummary<double>());
                }
}

void BERtestHistograms::process() { draw<TH1F>(BERtest); }
