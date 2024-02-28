/*!
        \file                DQMHistogramPedestalEqualization.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
 */

#include "DQMUtils/DQMHistogramPedestalEqualization.h"
#include "RootUtils/RootContainerFactory.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH1I.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/EmptyContainer.h"
#include "Utils/Occupancy.h"
#include "Utils/ThresholdAndNoise.h"
#include "Utils/Utilities.h"

//========================================================================================================================
DQMHistogramPedestalEqualization::DQMHistogramPedestalEqualization() {}

//========================================================================================================================
DQMHistogramPedestalEqualization::~DQMHistogramPedestalEqualization() {}

//========================================================================================================================
void DQMHistogramPedestalEqualization::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    fDetectorContainer = &theDetectorStructure;

    NCH = theDetectorStructure.getFirstObject()->getFirstObject()->getFirstObject()->getFirstObject()->size();

    HistContainer<TH1I> hVplus("VplusValue", "Vplus Value", 1, 0, 1);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fDetectorVplusHistograms, hVplus);

    HistContainer<TH1I> hOffset("OffsetValues", "Offset Values", NCH, -0.5, float(NCH) - 0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fDetectorOffsetHistograms, hOffset);

    HistContainer<TH1F> hOccupancy("OccupancyAfterOffsetEqualization", "Occupancy After Offset Equalization", NCH, -0.5, float(NCH) - 0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fDetectorOccupancyHistograms, hOccupancy);
}

//========================================================================================================================
bool DQMHistogramPedestalEqualization::fill(std::string& inputStream)
{
    ContainerSerialization theVCthSerialization("PedestalEqualizationVCth");
    ContainerSerialization theOccupancySerialization("PedestalEqualizationOccupancy");
    ContainerSerialization theOffsetSerialization("PedestalEqualizationOffset");

    if(theVCthSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched PedestalEqualization Vcth!!!!!\n";
        DetectorDataContainer theDetectorData = theVCthSerialization.deserializeHybridContainer<EmptyContainer, uint16_t, EmptyContainer>(fDetectorContainer);
        fillVplusPlots(theDetectorData);
        return true;
    }
    if(theOccupancySerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched PedestalEqualization Occupancy!!!!!\n";
        DetectorDataContainer theDetectorData = theOccupancySerialization.deserializeHybridContainer<Occupancy, Occupancy, Occupancy>(fDetectorContainer);
        fillOccupancyPlots(theDetectorData);
        return true;
    }
    if(theOffsetSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched PedestalEqualization Offset!!!!!\n";
        DetectorDataContainer theDetectorData = theOffsetSerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, uint8_t>(fDetectorContainer);
        fillOffsetPlots(theDetectorData);
        return true;
    }

    return false;
}

//========================================================================================================================
void DQMHistogramPedestalEqualization::process()
{
    for(auto board: fDetectorOffsetHistograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string offsetCanvasName    = "Offset_B_" + std::to_string(board->getId()) + "_O_" + std::to_string(opticalGroup->getId()) + "_H_" + std::to_string(hybrid->getId());
                std::string occupancyCanvasName = "Occupancy_B_" + std::to_string(board->getId()) + "_O_" + std::to_string(opticalGroup->getId()) + "_H_" + std::to_string(hybrid->getId());

                TCanvas* offsetCanvas    = new TCanvas(offsetCanvasName.data(), offsetCanvasName.data(), 10, 0, 500, 500);
                TCanvas* occupancyCanvas = new TCanvas(occupancyCanvasName.data(), occupancyCanvasName.data(), 10, 525, 500, 500);

                offsetCanvas->DivideSquare(hybrid->size());
                occupancyCanvas->DivideSquare(hybrid->size());

                for(auto chip: *hybrid)
                {
                    offsetCanvas->cd(chip->getId() + 1);
                    TH1I* offsetHistogram = chip->getSummary<HistContainer<TH1I>>().fTheHistogram;
                    offsetHistogram->GetXaxis()->SetTitle("Channel");
                    offsetHistogram->GetYaxis()->SetTitle("Offset");
                    offsetHistogram->DrawCopy();

                    occupancyCanvas->cd(chip->getId() + 1);
                    TH1F* occupancyHistogram = fDetectorOccupancyHistograms.getObject(board->getId())
                                                   ->getObject(opticalGroup->getId())
                                                   ->getObject(hybrid->getId())
                                                   ->getObject(chip->getId())
                                                   ->getSummary<HistContainer<TH1F>>()
                                                   .fTheHistogram;
                    occupancyHistogram->GetXaxis()->SetTitle("Channel");
                    occupancyHistogram->GetYaxis()->SetTitle("Occupancy");
                    occupancyHistogram->DrawCopy();
                }
            }
        }
    }
}

//========================================================================================================================

void DQMHistogramPedestalEqualization::reset(void) {}

//========================================================================================================================
void DQMHistogramPedestalEqualization::fillVplusPlots(DetectorDataContainer& theVthr)
{
    for(auto board: theVthr)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    if(!chip->hasSummary()) continue;
                    TH1I* chipVplusHistogram = fDetectorVplusHistograms.getObject(board->getId())
                                                   ->getObject(opticalGroup->getId())
                                                   ->getObject(hybrid->getId())
                                                   ->getObject(chip->getId())
                                                   ->getSummary<HistContainer<TH1I>>()
                                                   .fTheHistogram;
                    chipVplusHistogram->SetBinContent(1, chip->getSummary<uint16_t>());
                }
            }
        }
    }
}

//========================================================================================================================

void DQMHistogramPedestalEqualization::fillOccupancyPlots(DetectorDataContainer& theOccupancy)
{
    for(auto board: theOccupancy)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    if(chip->hasChannelContainer() == false) continue;
                    TH1F* chipOccupancyHistogram = fDetectorOccupancyHistograms.getObject(board->getId())
                                                       ->getObject(opticalGroup->getId())
                                                       ->getObject(hybrid->getId())
                                                       ->getObject(chip->getId())
                                                       ->getSummary<HistContainer<TH1F>>()
                                                       .fTheHistogram;
                    for(uint16_t row = 0; row < chip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < chip->getNumberOfCols(); ++col)
                        {
                            chipOccupancyHistogram->SetBinContent(linearizeRowAndCols(row, col, chip->getNumberOfCols()), chip->getChannel<Occupancy>(row, col).fOccupancy);
                            chipOccupancyHistogram->SetBinError(linearizeRowAndCols(row, col, chip->getNumberOfCols()), chip->getChannel<Occupancy>(row, col).fOccupancyError);
                        }
                    }
                }
            }
        }
    }
}

//========================================================================================================================

void DQMHistogramPedestalEqualization::fillOffsetPlots(DetectorDataContainer& theOffsets)
{
    for(auto board: theOffsets)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    if(chip->hasChannelContainer() == false) continue;
                    TH1I* chipOffsetHistogram = fDetectorOffsetHistograms.getObject(board->getId())
                                                    ->getObject(opticalGroup->getId())
                                                    ->getObject(hybrid->getId())
                                                    ->getObject(chip->getId())
                                                    ->getSummary<HistContainer<TH1I>>()
                                                    .fTheHistogram;
                    for(uint16_t row = 0; row < chip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < chip->getNumberOfCols(); ++col)
                        { chipOffsetHistogram->SetBinContent(linearizeRowAndCols(row, col, chip->getNumberOfCols()), chip->getChannel<uint8_t>(row, col)); }
                    }
                }
            }
        }
    }
}

//========================================================================================================================
