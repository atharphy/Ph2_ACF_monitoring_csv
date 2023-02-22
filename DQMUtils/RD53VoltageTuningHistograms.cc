/*!
  \file                  RD53VoltageTuningHistograms.h
  \brief                 Header file of Voltage Tuning histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53VoltageTuningHistograms.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;

void VoltageTuningHistograms::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& settingsMap)
{
    fDetectorContainer = &theDetectorStructure;
    RD53Shared::setFirstChip(theDetectorStructure);

    auto hVoltageDig = CanvasContainer<TH1F>("VoltageDig", "Digital Voltage", 32, 0, 32);
    auto hVoltageAna = CanvasContainer<TH1F>("VoltageAna", "Analog Voltage", 32, 0, 32);

    bookImplementer(theOutputFile, theDetectorStructure, VoltageDig, hVoltageDig, "VoltageDig", "Entries");
    bookImplementer(theOutputFile, theDetectorStructure, VoltageAna, hVoltageAna, "VoltageAna", "Entries");
}

bool VoltageTuningHistograms::fill(std::vector<char>& dataBuffer)
{
    std::string inputStream(dataBuffer.begin(), dataBuffer.end());
    ContainerSerialization theVoltageDigitalSerialization("VoltageTuningVoltageDigital");
    ContainerSerialization theVoltageAnalogSerialization("VoltageTuningVoltageAnalog");

    if(theVoltageDigitalSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched VoltageTuning VoltageDigital!!!!!\n";
        DetectorDataContainer fDetectorData = theVoltageDigitalSerialization.deserializeChipContainer<EmptyContainer, uint16_t>(fDetectorContainer);
        VoltageTuningHistograms::fillDig(fDetectorData);
        return true;
    }
    if(theVoltageAnalogSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched VoltageTuning VoltageAnalog!!!!!\n";
        DetectorDataContainer fDetectorData = theVoltageAnalogSerialization.deserializeChipContainer<EmptyContainer, uint16_t>(fDetectorContainer);
        VoltageTuningHistograms::fillAna(fDetectorData);
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
