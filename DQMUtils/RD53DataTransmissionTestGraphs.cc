/*!
  \file                  RD53DataTransmissionTestGraphs.cc
  \brief                 Implementation of TAP scan graphs
  \author                Marijus AMBROZAS
  \version               1.0
  \date                  26/04/21
  Support:               email to marijus.ambrozas@cern.ch
*/

#include "RD53DataTransmissionTestGraphs.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void DataTransmissionTestGraphs::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    // #######################
    // # Retrieve parameters #
    // #######################
    BERtarget      = this->findValueInSettings<double>(settingsMap, "BERtarget");
    given_time     = this->findValueInSettings<double>(settingsMap, "byTime");
    frames_or_time = this->findValueInSettings<double>(settingsMap, "framesORtime");

    auto gTAP0scan = CanvasContainer<TGraphAsymmErrors>(11);
    bookImplementer(theOutputFile, theDetectorStructure, TAP0scan, gTAP0scan, "TAP0", "Bit Error Rate");
    auto hTAP0tgt = CanvasContainer<TH1F>("TAP0tgt", "TAP0 at target BER", 1024, 0 - 0.5, 1024 - 0.5);
    bookImplementer(theOutputFile, theDetectorStructure, TAP0tgt, hTAP0tgt, "TAP0 at target BER", "Value");
}

bool DataTransmissionTestGraphs::fill(std::vector<char>& dataBuffer)
{
    ChipContainerStream<EmptyContainer, std::array<std::tuple<uint16_t, double, double, double>, 11>> theTAP0scanStreamer("DataTransmissionTestTAP0scan");
    ChipContainerStream<EmptyContainer, uint16_t>                                                     theTAP0tgtStreamer("DataTransmissionTestTAP0tgteshold");

    if(theTAP0scanStreamer.attachBuffer(&dataBuffer))
    {
        theTAP0scanStreamer.decodeChipData(DetectorData);
        DataTransmissionTestGraphs::fillTAP0scan(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theTAP0tgtStreamer.attachBuffer(&dataBuffer))
    {
        theTAP0tgtStreamer.decodeChipData(DetectorData);
        DataTransmissionTestGraphs::fillTAP0tgt(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    return false;
}

void DataTransmissionTestGraphs::fillTAP0scan(const DetectorDataContainer& TAP0scanContainer)
{
    for(const auto cBoard: TAP0scanContainer)
    {
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<std::array<std::tuple<uint16_t, double, double, double>, 11>>() == nullptr) continue;
                    auto* TAP0scanGraph =
                        TAP0scan.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TGraphAsymmErrors>>().fTheHistogram;

                    for(auto i = 0u; i < 11u; i++) // set bin errors manually
                    {
                        TAP0scanGraph->SetPoint(i,
                                                ((double)(std::get<0>((cChip->getSummary<std::array<std::tuple<uint16_t, double, double, double>, 11>>())[i]))),
                                                std::get<1>((cChip->getSummary<std::array<std::tuple<uint16_t, double, double, double>, 11>>())[i]));
                        TAP0scanGraph->SetPointError(i,
                                                     0.5,
                                                     0.5,
                                                     std::get<2>((cChip->getSummary<std::array<std::tuple<uint16_t, double, double, double>, 11>>())[i]),
                                                     std::get<3>((cChip->getSummary<std::array<std::tuple<uint16_t, double, double, double>, 11>>())[i]));
                    }
                    TAP0scanGraph->SetMarkerStyle(8);
                }
    }
}

void DataTransmissionTestGraphs::fillTAP0tgt(const DetectorDataContainer& TAP0tgtContainer)
{
    for(const auto cBoard: TAP0tgtContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<uint16_t>() == nullptr) continue;

                    auto* TAP0tgtHist =
                        TAP0tgt.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;

                    TAP0tgtHist->SetBinContent(TAP0tgtHist->GetBin(cChip->getSummary<uint16_t>()), 1);
                    TAP0tgtHist->SetBinError(TAP0tgtHist->GetBin(cChip->getSummary<uint16_t>()), 0);
                }
}

void DataTransmissionTestGraphs::process()
{
    draw<TGraphAsymmErrors>(TAP0scan, "APZ0");
    draw<TH1F>(TAP0tgt, "P*");
}
