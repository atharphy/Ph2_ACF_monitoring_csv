/*!
  \file                  RD53VoltageTuningHistograms.h
  \brief                 Header file of Voltage Tuning histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53VoltageTuningHistograms.h"

using namespace Ph2_HwDescription;

void VoltageTuningHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    auto hVoltageDig = CanvasContainer<TH1F>("VoltageDig", "Digital Voltage", 32, 0, 32);
    auto hVoltageAna = CanvasContainer<TH1F>("VoltageAna", "Analog Voltage", 32, 0, 32);

    bookImplementer(theOutputFile, theDetectorStructure, VoltageDig, hVoltageDig, "VoltageDig", "Entries");
    bookImplementer(theOutputFile, theDetectorStructure, VoltageAna, hVoltageAna, "VoltageAna", "Entries");
}

bool VoltageTuningHistograms::fill(std::vector<char>& dataBuffer)
{
    ChipContainerStream<EmptyContainer, uint16_t> theDigiStreamer("VoltageDig");
    ChipContainerStream<EmptyContainer, uint16_t> theAnaStreamer("VoltageAna");

    if(theDigiStreamer.attachBuffer(&dataBuffer))
    {
        theDigiStreamer.decodeChipData(DetectorData);
        VoltageTuningHistograms::fillDig(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theAnaStreamer.attachBuffer(&dataBuffer))
    {
        theAnaStreamer.decodeChipData(DetectorData);
        VoltageTuningHistograms::fillDig(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }

    return false;
}

void VoltageTuningHistograms::fillDig(const DetectorDataContainer& DataContainer)
{
    for(const auto cBoard: DataContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<uint16_t>() == nullptr) continue;

                    auto* hVoltageDig = VoltageDig.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<CanvasContainer<TH1F>>()
                                            .fTheHistogram;

                    hVoltageDig->Fill(cChip->getSummary<uint16_t>());
                }
}

void VoltageTuningHistograms::fillAna(const DetectorDataContainer& DataContainer)
{
    for(const auto cBoard: DataContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<uint16_t>() == nullptr) continue;

                    auto* hVoltageAna = VoltageAna.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<CanvasContainer<TH1F>>()
                                            .fTheHistogram;

                    hVoltageAna->Fill(cChip->getSummary<uint16_t>());
                }
}

void VoltageTuningHistograms::process()
{
    draw<TH1F>(VoltageDig);
    draw<TH1F>(VoltageAna);
}
