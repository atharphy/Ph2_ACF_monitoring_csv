/*!
  \file                  RD53PowerTrimmingHistograms.cc
  \brief                 Source file of Power Trimming histograms
  \author                Luca GUZZI
  \version               1.0
  \date                  03/02/26
  \support               email to luca.guzzi@cern.ch
*/

#include "RD53PowerTrimmingHistograms.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;

using dataType = std::vector<std::pair<uint16_t, float>>;

void PowerTrimmingHistograms::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& settingsMap)
{
    fDetectorContainer = &theDetectorStructure;
    RD53Shared::setFirstChip(theDetectorStructure);

    int max_dac = 1024;

    auto hComparatorCurrent = CanvasContainer<TH1F>("ComparatorCurrent", "I vs. Time", max_dac, 0, max_dac);
    auto hPreampCurrent     = CanvasContainer<TH1F>("PreampCurrent", "I vs. Time", max_dac, 0, max_dac);
    auto hLDACCurrent       = CanvasContainer<TH1F>("LDACCurrent", "I vs. Time", max_dac, 0, max_dac);

    bookChipImplementer(theOutputFile, theDetectorStructure, theComparatorCurrentContainer, hComparatorCurrent, "Time (HH:MM:SS)", "Current (mA)");
    bookChipImplementer(theOutputFile, theDetectorStructure, thePreamplifierCurrentContainer, hPreampCurrent, "Time (HH:MM:SS)", "Current (mA)");
    bookChipImplementer(theOutputFile, theDetectorStructure, theLDACCurrentContainer, hLDACCurrent, "Time (HH:MM:SS)", "Current (mA)");

    auto hAna      = CanvasContainer<TH1F>("ANA_IN_CURR", "Analog Current vs Time", max_dac, 0, max_dac);
    auto hDig      = CanvasContainer<TH1F>("DIG_IN_CURR", "Digital Current vs Time", max_dac, 0, max_dac);
    auto hVAna     = CanvasContainer<TH1F>("VINA", "VINA vs Time", max_dac, 0, max_dac);
    auto hVDAna    = CanvasContainer<TH1F>("VDDA", "VDDA vs Time", max_dac, 0, max_dac);
    auto hVDig     = CanvasContainer<TH1F>("VIND", "VIND vs Time", max_dac, 0, max_dac);
    auto hVDDig    = CanvasContainer<TH1F>("VDDD", "VDDD vs Time", max_dac, 0, max_dac);
    auto hIrf      = CanvasContainer<TH1F>("Iref", "Iref vs Time", max_dac, 0, max_dac);
    auto hAnaShunt = CanvasContainer<TH1F>("ANA_SHUNT_CURR", "Analog Shunt Current vs Time", max_dac, 0, max_dac);
    auto hDigShunt = CanvasContainer<TH1F>("DIG_SHUNT_CURR", "Digital Shunt Current vs Time", max_dac, 0, max_dac);

    bookChipImplementer(theOutputFile, theDetectorStructure, hAnaInCurrContainer, hAna, "Time (HH:MM:SS)", "Current (mA)");
    bookChipImplementer(theOutputFile, theDetectorStructure, hDigInCurrContainer, hDig, "Time (HH:MM:SS)", "Current (mA)");
    bookChipImplementer(theOutputFile, theDetectorStructure, hVINAContainer, hVAna, "Time (HH:MM:SS)", "Voltage (V)");
    bookChipImplementer(theOutputFile, theDetectorStructure, hVDDAContainer, hVDAna, "Time (HH:MM:SS)", "Voltage (V)");
    bookChipImplementer(theOutputFile, theDetectorStructure, hVINDContainer, hVDig, "Time (HH:MM:SS)", "Voltage (V)");
    bookChipImplementer(theOutputFile, theDetectorStructure, hVDDDContainer, hVDDig, "Time (HH:MM:SS)", "Voltage (V)");
    bookChipImplementer(theOutputFile, theDetectorStructure, hIrefContainer, hIrf, "Time (HH:MM:SS)", "Iref");
    bookChipImplementer(theOutputFile, theDetectorStructure, hAnaShuntContainer, hAnaShunt, "Time (HH:MM:SS)", "Current (mA)");
    bookChipImplementer(theOutputFile, theDetectorStructure, hDigShuntContainer, hDigShunt, "Time (HH:MM:SS)", "Current (mA)");

    AreHistoBooked = true;
}

bool PowerTrimmingHistograms::fill(std::string& inputStream)
{
    ContainerSerialization theComparatorCurrentSerialization("PowerTrimmingComparatorCurrent");
    ContainerSerialization theLDACCurrentSerialization("PowerTrimmingLDACCurrent");

    if(theComparatorCurrentSerialization.attachDeserializer(inputStream))
    {
        DetectorDataContainer fDetectorData = theComparatorCurrentSerialization.deserializeChipContainer<EmptyContainer, dataType>(fDetectorContainer);
        PowerTrimmingHistograms::fillComparatorCurrentHisto(fDetectorData);
        return true;
    }
    if(theLDACCurrentSerialization.attachDeserializer(inputStream))
    {
        DetectorDataContainer fDetectorData = theLDACCurrentSerialization.deserializeChipContainer<EmptyContainer, dataType>(fDetectorContainer);
        PowerTrimmingHistograms::fillLDACCurrentHisto(fDetectorData);
        return true;
    }
    return false;
}

void PowerTrimmingHistograms::fillComparatorCurrentHisto(const DetectorDataContainer& dataContainer) { PowerTrimmingHistograms::fillHisto(dataContainer, theComparatorCurrentContainer); }

void PowerTrimmingHistograms::fillPreamplifierCurrentHisto(const DetectorDataContainer& dataContainer) { PowerTrimmingHistograms::fillHisto(dataContainer, thePreamplifierCurrentContainer); }

void PowerTrimmingHistograms::fillLDACCurrentHisto(const DetectorDataContainer& dataContainer) { PowerTrimmingHistograms::fillHisto(dataContainer, theLDACCurrentContainer); }

void PowerTrimmingHistograms::fillHisto(const DetectorDataContainer& dataContainer, const DetectorDataContainer& dataHistogram)
{
    for(const auto cBoard: dataContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->hasSummary() == false) continue;

                    auto* histo = dataHistogram.getObject(cBoard->getId())
                                      ->getObject(cOpticalGroup->getId())
                                      ->getObject(cHybrid->getId())
                                      ->getObject(cChip->getId())
                                      ->getSummary<CanvasContainer<TH1F>>()
                                      .fTheHistogram;

                    histo->SetMarkerStyle(20);
                    histo->SetMarkerSize(0.8);

                    for(auto entry: cChip->getSummary<dataType>())
                    {
                        histo->SetBinContent(entry.first + 1, entry.second);
                        histo->SetBinError(entry.first + 1, 0.0);
                    }
                }
}

void PowerTrimmingHistograms::fillCustomHistos(const std::vector<PowerTrimmingData>& dataList)
{
    if(dataList.empty()) return;

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto* hAna = hAnaInCurrContainer.getObject(cBoard->getId())
                                     ->getObject(cOpticalGroup->getId())
                                     ->getObject(cHybrid->getId())
                                     ->getObject(cChip->getId())
                                     ->getSummary<CanvasContainer<TH1F>>()
                                     .fTheHistogram;
                    auto* hDig = hDigInCurrContainer.getObject(cBoard->getId())
                                     ->getObject(cOpticalGroup->getId())
                                     ->getObject(cHybrid->getId())
                                     ->getObject(cChip->getId())
                                     ->getSummary<CanvasContainer<TH1F>>()
                                     .fTheHistogram;
                    auto* hVAna = hVINAContainer.getObject(cBoard->getId())
                                      ->getObject(cOpticalGroup->getId())
                                      ->getObject(cHybrid->getId())
                                      ->getObject(cChip->getId())
                                      ->getSummary<CanvasContainer<TH1F>>()
                                      .fTheHistogram;
                    auto* hVDAna = hVDDAContainer.getObject(cBoard->getId())
                                       ->getObject(cOpticalGroup->getId())
                                       ->getObject(cHybrid->getId())
                                       ->getObject(cChip->getId())
                                       ->getSummary<CanvasContainer<TH1F>>()
                                       .fTheHistogram;
                    auto* hVDig = hVINDContainer.getObject(cBoard->getId())
                                      ->getObject(cOpticalGroup->getId())
                                      ->getObject(cHybrid->getId())
                                      ->getObject(cChip->getId())
                                      ->getSummary<CanvasContainer<TH1F>>()
                                      .fTheHistogram;
                    auto* hVDDig = hVDDDContainer.getObject(cBoard->getId())
                                       ->getObject(cOpticalGroup->getId())
                                       ->getObject(cHybrid->getId())
                                       ->getObject(cChip->getId())
                                       ->getSummary<CanvasContainer<TH1F>>()
                                       .fTheHistogram;
                    auto* hIrf = hIrefContainer.getObject(cBoard->getId())
                                     ->getObject(cOpticalGroup->getId())
                                     ->getObject(cHybrid->getId())
                                     ->getObject(cChip->getId())
                                     ->getSummary<CanvasContainer<TH1F>>()
                                     .fTheHistogram;
                    auto* hAnaShunt = hAnaShuntContainer.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<CanvasContainer<TH1F>>()
                                          .fTheHistogram;
                    auto* hDigShunt = hDigShuntContainer.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<CanvasContainer<TH1F>>()
                                          .fTheHistogram;

                    auto* hComp = theComparatorCurrentContainer.getObject(cBoard->getId())
                                      ->getObject(cOpticalGroup->getId())
                                      ->getObject(cHybrid->getId())
                                      ->getObject(cChip->getId())
                                      ->getSummary<CanvasContainer<TH1F>>()
                                      .fTheHistogram;
                    auto* hPreamp = thePreamplifierCurrentContainer.getObject(cBoard->getId())
                                        ->getObject(cOpticalGroup->getId())
                                        ->getObject(cHybrid->getId())
                                        ->getObject(cChip->getId())
                                        ->getSummary<CanvasContainer<TH1F>>()
                                        .fTheHistogram;
                    auto* hLDAC = theLDACCurrentContainer.getObject(cBoard->getId())
                                      ->getObject(cOpticalGroup->getId())
                                      ->getObject(cHybrid->getId())
                                      ->getObject(cChip->getId())
                                      ->getSummary<CanvasContainer<TH1F>>()
                                      .fTheHistogram;

                    hAna->SetMarkerStyle(20);
                    hAna->SetMarkerSize(0.8);
                    hDig->SetMarkerStyle(20);
                    hDig->SetMarkerSize(0.8);
                    hVAna->SetMarkerStyle(20);
                    hVAna->SetMarkerSize(0.8);
                    hVDAna->SetMarkerStyle(20);
                    hVDAna->SetMarkerSize(0.8);
                    hVDig->SetMarkerStyle(20);
                    hVDig->SetMarkerSize(0.8);
                    hVDDig->SetMarkerStyle(20);
                    hVDDig->SetMarkerSize(0.8);
                    hIrf->SetMarkerStyle(20);
                    hIrf->SetMarkerSize(0.8);
                    hAnaShunt->SetMarkerStyle(20);
                    hAnaShunt->SetMarkerSize(0.8);
                    hDigShunt->SetMarkerStyle(20);
                    hDigShunt->SetMarkerSize(0.8);

                    for(const auto& data: dataList)
                    {
                        int binX = data.bit + 1;

                        hAna->SetBinContent(binX, data.ANA_IN_CURR);
                        hDig->SetBinContent(binX, data.DIG_IN_CURR);
                        hVAna->SetBinContent(binX, data.VINA);
                        hVDAna->SetBinContent(binX, data.VDDA);
                        hVDig->SetBinContent(binX, data.VIND);
                        hVDDig->SetBinContent(binX, data.VDDD);
                        hIrf->SetBinContent(binX, data.Iref);
                        hAnaShunt->SetBinContent(binX, data.ANA_SHUNT_CURR);
                        hDigShunt->SetBinContent(binX, data.DIG_SHUNT_CURR);

                        std::string time_str = std::to_string(static_cast<int>(data.timestamp));

                        hAna->GetXaxis()->SetBinLabel(binX, time_str.c_str());
                        hDig->GetXaxis()->SetBinLabel(binX, time_str.c_str());
                        hVAna->GetXaxis()->SetBinLabel(binX, time_str.c_str());
                        hVDAna->GetXaxis()->SetBinLabel(binX, time_str.c_str());
                        hVDig->GetXaxis()->SetBinLabel(binX, time_str.c_str());
                        hVDDig->GetXaxis()->SetBinLabel(binX, time_str.c_str());
                        hIrf->GetXaxis()->SetBinLabel(binX, time_str.c_str());
                        hAnaShunt->GetXaxis()->SetBinLabel(binX, time_str.c_str());
                        hDigShunt->GetXaxis()->SetBinLabel(binX, time_str.c_str());

                        if(hComp) hComp->GetXaxis()->SetBinLabel(binX, time_str.c_str());
                        if(hPreamp) hPreamp->GetXaxis()->SetBinLabel(binX, time_str.c_str());
                        if(hLDAC) hLDAC->GetXaxis()->SetBinLabel(binX, time_str.c_str());
                    }

                    int start_bin = dataList.front().bit + 1;
                    int end_bin   = dataList.back().bit + 1;

                    hAna->GetXaxis()->SetRangeUser(start_bin, end_bin);
                    hDig->GetXaxis()->SetRangeUser(start_bin, end_bin);
                    hVAna->GetXaxis()->SetRangeUser(start_bin, end_bin);
                    hVDAna->GetXaxis()->SetRangeUser(start_bin, end_bin);
                    hVDig->GetXaxis()->SetRangeUser(start_bin, end_bin);
                    hVDDig->GetXaxis()->SetRangeUser(start_bin, end_bin);
                    hIrf->GetXaxis()->SetRangeUser(start_bin, end_bin);
                    hAnaShunt->GetXaxis()->SetRangeUser(start_bin, end_bin);
                    hDigShunt->GetXaxis()->SetRangeUser(start_bin, end_bin);

                    if(hComp) hComp->GetXaxis()->SetRangeUser(start_bin, end_bin);
                    if(hPreamp) hPreamp->GetXaxis()->SetRangeUser(start_bin, end_bin);
                    if(hLDAC) hLDAC->GetXaxis()->SetRangeUser(start_bin, end_bin);
                }
}

void PowerTrimmingHistograms::process()
{
    drawChip<TH1F>(theComparatorCurrentContainer, "P");
    drawChip<TH1F>(thePreamplifierCurrentContainer, "P");
    drawChip<TH1F>(theLDACCurrentContainer, "P");

    drawChip<TH1F>(hAnaInCurrContainer, "P");
    drawChip<TH1F>(hDigInCurrContainer, "P");
    drawChip<TH1F>(hVINAContainer, "P");
    drawChip<TH1F>(hVDDAContainer, "P");
    drawChip<TH1F>(hVINDContainer, "P");
    drawChip<TH1F>(hVDDDContainer, "P");
    drawChip<TH1F>(hIrefContainer, "P");
    drawChip<TH1F>(hAnaShuntContainer, "P");
    drawChip<TH1F>(hDigShuntContainer, "P");
}