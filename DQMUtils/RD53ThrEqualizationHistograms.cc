/*!
  \file                  RD53ThrEqualizationHistograms.cc
  \brief                 Implementation of ThrEqualization calibration histograms
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ThrEqualizationHistograms.h"
#include "../HWDescription/RD53A.h"
#include "../Utils/ChannelContainerStream.h"

using namespace Ph2_HwDescription;

void ThrEqualizationHistograms::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    nRows = RD53Shared::firstChip->getNRows();
    nCols = RD53Shared::firstChip->getNCols();

    // #######################
    // # Retrieve parameters #
    // #######################
    nEvents         = this->findValueInSettings<double>(settingsMap, "nEvents");
    size_t TDACsize = RD53Shared::setBits(RD53Constants::NBIT_TDAC) + 1;

    const size_t colStart = this->findValueInSettings<double>(settingsMap, "COLstart");
    const size_t colStop  = this->findValueInSettings<double>(settingsMap, "COLstop");
    frontEnd              = RD53Shared::firstChip->getMajorityFE(colStart, colStop);
    if(frontEnd == &RD53A::DIFF) TDACsize *= 2;

    auto hThrEqualization = CanvasContainer<TH1F>("ThrEqualization", "ThrEqualization", nEvents + 1, 0, 1 + 1. / nEvents);
    bookImplementer(theOutputFile, theDetectorStructure, ThrEqualization, hThrEqualization, "Efficiency", "Entries");

    auto hTDAC1D = CanvasContainer<TH1F>("TDAC1D", "TDAC Distribution", TDACsize, 0, TDACsize);
    bookImplementer(theOutputFile, theDetectorStructure, TDAC1D, hTDAC1D, "TDAC", "Entries");

    auto hTDAC2D = CanvasContainer<TH2F>("TDAC2D", "TDAC Map", nCols, 0, nCols, nRows, 0, nRows);
    bookImplementer(theOutputFile, theDetectorStructure, TDAC2D, hTDAC2D, "Column", "Row");
}

bool ThrEqualizationHistograms::fill(std::vector<char>& dataBuffer)
{
    ChannelContainerStream<OccupancyAndPh> theOccStreamer("ThrEqualizationOcc");
    ChannelContainerStream<uint16_t>       theTDACStreamer("ThrEqualizationTDAC");

    if(theOccStreamer.attachBuffer(&dataBuffer))
    {
        theOccStreamer.decodeChipData(DetectorData);
        ThrEqualizationHistograms::fillOccupancy(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theTDACStreamer.attachBuffer(&dataBuffer))
    {
        theTDACStreamer.decodeChipData(DetectorData);
        ThrEqualizationHistograms::fillTDAC(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }

    return false;
}

void ThrEqualizationHistograms::fillOccupancy(const DetectorDataContainer& OccupancyContainer)
{
    for(const auto cBoard: OccupancyContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getChannelContainer<OccupancyAndPh>() == nullptr) continue;

                    auto* hThrEqualization = ThrEqualization.getObject(cBoard->getId())
                                                 ->getObject(cOpticalGroup->getId())
                                                 ->getObject(cHybrid->getId())
                                                 ->getObject(cChip->getId())
                                                 ->getSummary<CanvasContainer<TH1F>>()
                                                 .fTheHistogram;

                    for(auto row = 0u; row < nRows; row++)
                        for(auto col = 0u; col < nCols; col++)
                            if(cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy != RD53Shared::ISDISABLED)
                                hThrEqualization->Fill(cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy + hThrEqualization->GetBinWidth(0) / 2);
                }
}

void ThrEqualizationHistograms::fillTDAC(const DetectorDataContainer& TDACContainer)
{
    size_t TDACsize = RD53Shared::setBits(RD53Constants::NBIT_TDAC) + 1;
    if(frontEnd == &RD53A::DIFF) TDACsize *= 2;

    for(const auto cBoard: TDACContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getChannelContainer<uint16_t>() == nullptr) continue;

                    auto* hTDAC1D =
                        TDAC1D.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;

                    auto* hTDAC2D =
                        TDAC2D.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<CanvasContainer<TH2F>>().fTheHistogram;

                    for(auto row = 0u; row < nRows; row++)
                        for(auto col = 0u; col < nCols; col++)
                            if(cChip->getChannel<uint16_t>(row, col) != TDACsize)
                            {
                                hTDAC1D->Fill(cChip->getChannel<uint16_t>(row, col));
                                hTDAC2D->SetBinContent(col + 1, row + 1, cChip->getChannel<uint16_t>(row, col));
                            }
                }
}

void ThrEqualizationHistograms::process()
{
    draw<TH1F>(ThrEqualization);
    draw<TH1F>(TDAC1D);
    draw<TH2F>(TDAC2D, "gcolz");
}
