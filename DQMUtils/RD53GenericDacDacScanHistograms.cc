/*!
  \file                  RD53GenericDacDacScanHistograms.h
  \brief                 Implementation of generic DAC DAC scan histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/05/21
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53GenericDacDacScanHistograms.h"

using namespace Ph2_HwDescription;

void GenericDacDacScanHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    const std::vector<int> CDRfreq = {140, 145, 150, 155, 160, 165, 170, 176, 180}; // @CONST@

    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    // #######################
    // # Retrieve parameters #
    // #######################
    regNameDAC1    = this->findValueInSettings<std::string>(settingsMap, "RegNameDAC1");
    startValueDAC1 = this->findValueInSettings<double>(settingsMap, "StartValueDAC1");
    stopValueDAC1  = this->findValueInSettings<double>(settingsMap, "StopValueDAC1");
    stepDAC1       = this->findValueInSettings<double>(settingsMap, "StepDAC1");
    regNameDAC2    = this->findValueInSettings<std::string>(settingsMap, "RegNameDAC2");
    startValueDAC2 = this->findValueInSettings<double>(settingsMap, "StartValueDAC2");
    stopValueDAC2  = this->findValueInSettings<double>(settingsMap, "StopValueDAC2");
    stepDAC2       = this->findValueInSettings<double>(settingsMap, "StepDAC2");

    auto hGenericDac1Scan = CanvasContainer<TH1F>("GenericDac1Scan", "GenericDac1Scan", (stopValueDAC1 - startValueDAC1) / stepDAC1 + 1, startValueDAC1, stopValueDAC1 + stepDAC1);
    bookImplementer(theOutputFile, theDetectorStructure, GenericDac1Scan, hGenericDac1Scan, regNameDAC1.c_str(), "Entries");

    auto hGenericDac2Scan = CanvasContainer<TH1F>("GenericDac2Scan", "GenericDac2Scan", (stopValueDAC2 - startValueDAC2) / stepDAC2 + 1, startValueDAC2, stopValueDAC2 + stepDAC2);
    bookImplementer(theOutputFile, theDetectorStructure, GenericDac2Scan, hGenericDac2Scan, regNameDAC2.c_str(), "Entries");

    auto hOcc2D = CanvasContainer<TH2F>("GenericDacDacScanScan",
                                        "Generic DAC-DAC Scan",
                                        (stopValueDAC1 - startValueDAC1) / stepDAC1 + 1,
                                        startValueDAC1,
                                        stopValueDAC1 + stepDAC1,
                                        (stopValueDAC2 - startValueDAC2) / stepDAC2 + 1,
                                        startValueDAC2,
                                        stopValueDAC2 + stepDAC2);
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy2D, hOcc2D, regNameDAC1.c_str(), regNameDAC2.c_str());
}

bool GenericDacDacScanHistograms::fill(std::vector<char>& dataBuffer)
{
    const size_t GenericDacDacScanSize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ChipContainerStream<EmptyContainer, GenericDataArray<GenericDacDacScanSize>> theOccStreamer("GenericDacDacScanOcc");
    ChipContainerStream<EmptyContainer, std::pair<uint16_t, uint16_t>>           theGenericDacDacScanStreamer("GenericDacDacScanDACDAC");

    if(theOccStreamer.attachBuffer(&dataBuffer))
    {
        theOccStreamer.decodeChipData(DetectorData);
        GenericDacDacScanHistograms::fillOccupancy(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theGenericDacDacScanStreamer.attachBuffer(&dataBuffer))
    {
        theGenericDacDacScanStreamer.decodeChipData(DetectorData);
        GenericDacDacScanHistograms::fillGenericDacDacScan(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }

    return false;
}

void GenericDacDacScanHistograms::fillOccupancy(const DetectorDataContainer& OccupancyContainer)
{
    const size_t GenericDacDacScanSize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(const auto cBoard: OccupancyContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<GenericDataArray<GenericDacDacScanSize>>() == nullptr) continue;

                    auto* Occupancy2DHist = Occupancy2D.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<CanvasContainer<TH1F>>()
                                                .fTheHistogram;

                    for(auto i = 0; i < Occupancy2DHist->GetNbinsX(); i++)
                        for(auto j = 0; j < Occupancy2DHist->GetNbinsY(); j++)
                            Occupancy2DHist->SetBinContent(i + 1, j + 1, cChip->getSummary<GenericDataArray<GenericDacDacScanSize>>().data[i * Occupancy2DHist->GetNbinsY() + j]);
                }
}

void GenericDacDacScanHistograms::fillGenericDacDacScan(const DetectorDataContainer& GenericDacDacScanContainer)
{
    for(const auto cBoard: GenericDacDacScanContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getSummaryContainer<uint16_t>() == nullptr) continue;

                    auto* GenericDac1ScanHist = GenericDac1Scan.getObject(cBoard->getId())
                                                    ->getObject(cOpticalGroup->getId())
                                                    ->getObject(cHybrid->getId())
                                                    ->getObject(cChip->getId())
                                                    ->getSummary<CanvasContainer<TH1F>>()
                                                    .fTheHistogram;
                    auto* GenericDac2ScanHist = GenericDac2Scan.getObject(cBoard->getId())
                                                    ->getObject(cOpticalGroup->getId())
                                                    ->getObject(cHybrid->getId())
                                                    ->getObject(cChip->getId())
                                                    ->getSummary<CanvasContainer<TH1F>>()
                                                    .fTheHistogram;

                    GenericDac1ScanHist->Fill(cChip->getSummary<std::pair<uint16_t, uint16_t>>().first);
                    GenericDac2ScanHist->Fill(cChip->getSummary<std::pair<uint16_t, uint16_t>>().second);
                }
}

void GenericDacDacScanHistograms::process()
{
    draw<TH2F>(Occupancy2D, "gcolz", "frequency", "VDDD (V)");
    draw<TH1F>(GenericDac1Scan);
    draw<TH1F>(GenericDac2Scan);
}
