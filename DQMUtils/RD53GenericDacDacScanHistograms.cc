/*!
  \file                  RD53GenericDacDacScanHistograms.h
  \brief                 Implementation of generic DAC DAC scan histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/05/21
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53GenericDacDacScanHistograms.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;

void GenericDacDacScanHistograms::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& settingsMap)
{
    fDetectorContainer = &theDetectorStructure;
    RD53Shared::setFirstChip(theDetectorStructure);

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

    // #####################################
    // # Make proper axis for secial cases #
    // #####################################
    size_t            startValueX = startValueDAC1;
    size_t            stopValueX  = stopValueDAC1 + stepDAC1;
    size_t            startValueY = startValueDAC2;
    size_t            stopValueY  = stopValueDAC2 + stepDAC2;
    std::stringstream titleX("");
    std::stringstream titleY("");

    if(regNameDAC2.find("CAL_EDGE_FINE_DELAY") != std::string::npos)
    {
        const auto frontEnd = RD53Shared::firstChip->getFEtype(RD53Shared::firstChip->getNRows() / 2, RD53Shared::firstChip->getNCols() / 2);
        const auto unitTime =
            1. / RD53Constants::ACCELERATOR_CLK * 1000 / ((RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CAL_EDGE_FINE_DELAY")) + 1) / (2. / frontEnd->nLatencyBins2Span));
        titleY << "Injection Delay (ns)";
        startValueY *= unitTime;
        stopValueY *= unitTime;
    }
    else if(regNameDAC1.find("CAL_EDGE_FINE_DELAY") != std::string::npos)
    {
        const auto frontEnd = RD53Shared::firstChip->getFEtype(RD53Shared::firstChip->getNRows() / 2, RD53Shared::firstChip->getNCols() / 2);
        const auto unitTime =
            1. / RD53Constants::ACCELERATOR_CLK * 1000 / ((RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CAL_EDGE_FINE_DELAY")) + 1) / (2. / frontEnd->nLatencyBins2Span));
        titleX << "Injection Delay (ns)";
        startValueX *= unitTime;
        stopValueX *= unitTime;
    }

    if(regNameDAC1.find("VCAL") != std::string::npos)
    {
        const auto& regMap = RD53Shared::firstChip->getRegMap();
        titleX << "#DeltaVCal";
        startValueX -= regMap.find("VCAL_MED")->second.fValue;
        stopValueX -= regMap.find("VCAL_MED")->second.fValue;
    }
    else if(regNameDAC2.find("VCAL") != std::string::npos)
    {
        const auto& regMap = RD53Shared::firstChip->getRegMap();
        titleY << "#DeltaVCal";
        startValueY -= regMap.find("VCAL_MED")->second.fValue;
        stopValueY -= regMap.find("VCAL_MED")->second.fValue;
    }

    if(titleX.str() == "") titleX << regNameDAC1;
    if(titleY.str() == "") titleY << regNameDAC2;

    auto hGenericDac1Scan = CanvasContainer<TH1F>("GenericDac1Scan", "GenericDac1Scan", (stopValueDAC1 - startValueDAC1) / stepDAC1 + 1, startValueDAC1, stopValueDAC1 + stepDAC1);
    bookImplementer(theOutputFile, theDetectorStructure, GenericDac1Scan, hGenericDac1Scan, regNameDAC1.c_str(), "Entries");

    auto hGenericDac2Scan = CanvasContainer<TH1F>("GenericDac2Scan", "GenericDac2Scan", (stopValueDAC2 - startValueDAC2) / stepDAC2 + 1, startValueDAC2, stopValueDAC2 + stepDAC2);
    bookImplementer(theOutputFile, theDetectorStructure, GenericDac2Scan, hGenericDac2Scan, regNameDAC2.c_str(), "Entries");

    auto hOcc2D = CanvasContainer<TH2F>("GenericDacDacScanScan",
                                        "Generic DAC-DAC Scan",
                                        (stopValueDAC1 - startValueDAC1) / stepDAC1 + 1,
                                        startValueX,
                                        stopValueX,
                                        (stopValueDAC2 - startValueDAC2) / stepDAC2 + 1,
                                        startValueY,
                                        stopValueY);
    bookImplementer(theOutputFile, theDetectorStructure, Occupancy2D, hOcc2D, titleX.str().c_str(), titleY.str().c_str());
}

bool GenericDacDacScanHistograms::fill(std::string& inputStream)
{
    ContainerSerialization theOccupancySerialization("GenericDacDacScanOccupancy");
    ContainerSerialization theDACDACSerialization("GenericDacDacScanDACDAC");

    if(theOccupancySerialization.attachDeserializer(inputStream))
    {
        DetectorDataContainer fDetectorData = theOccupancySerialization.deserializeChipContainer<EmptyContainer, std::vector<float>>(fDetectorContainer);
        GenericDacDacScanHistograms::fillOccupancy(fDetectorData);
        return true;
    }
    if(theDACDACSerialization.attachDeserializer(inputStream))
    {
        DetectorDataContainer fDetectorData = theDACDACSerialization.deserializeChipContainer<EmptyContainer, std::pair<uint16_t, uint16_t>>(fDetectorContainer);
        GenericDacDacScanHistograms::fillGenericDacDacScan(fDetectorData);
        return true;
    }
    return false;
}

void GenericDacDacScanHistograms::fillOccupancy(const DetectorDataContainer& OccupancyContainer)
{
    for(const auto cBoard: OccupancyContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->hasSummary() == false) continue;

                    auto* Occupancy2DHist = Occupancy2D.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<CanvasContainer<TH1F>>()
                                                .fTheHistogram;

                    for(auto i = 0; i < Occupancy2DHist->GetNbinsX(); i++)
                        for(auto j = 0; j < Occupancy2DHist->GetNbinsY(); j++)
                            Occupancy2DHist->SetBinContent(i + 1, j + 1, cChip->getSummary<std::vector<float>>().at(i * Occupancy2DHist->GetNbinsY() + j));
                }
}

void GenericDacDacScanHistograms::fillGenericDacDacScan(const DetectorDataContainer& GenericDacDacScanContainer)
{
    for(const auto cBoard: GenericDacDacScanContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->hasSummary() == false) continue;

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
    draw<TH2F>(Occupancy2D, "gcolz");
    draw<TH1F>(GenericDac1Scan);
    draw<TH1F>(GenericDac2Scan);
}
