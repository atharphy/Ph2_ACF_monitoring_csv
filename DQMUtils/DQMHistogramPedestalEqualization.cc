/*!
        \file                DQMHistogramPedestalEqualization.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
 */

#include "../DQMUtils/DQMHistogramPedestalEqualization.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"
#include "../Utils/ThresholdAndNoise.h"
#include "../Utils/Utilities.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH1I.h"

//========================================================================================================================
DQMHistogramPedestalEqualization::DQMHistogramPedestalEqualization() {}

//========================================================================================================================
DQMHistogramPedestalEqualization::~DQMHistogramPedestalEqualization() {}

//========================================================================================================================
void DQMHistogramPedestalEqualization::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);

    HistContainer<TH1I> hVplus("VplusValue", "Vplus Value", 1, 0, 1);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fDetectorVplusHistograms, hVplus);

    HistContainer<TH1I> hOffset("OffsetValues", "Offset Values", 254, -.5, 253.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fDetectorOffsetHistograms, hOffset);

    HistContainer<TH1F> hOccupancy("OccupancyAfterOffsetEqualization", "Occupancy After Offset Equalization", 254, -.5, 253.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fDetectorOccupancyHistograms, hOccupancy);
}

//========================================================================================================================
bool DQMHistogramPedestalEqualization::fill(std::vector<char>& dataBuffer)
{
    ModuleContainerStream<EmptyContainer, uint16_t, EmptyContainer> theVcthStreamer("PedestalEqualization");
    ChannelContainerStream<Occupancy>                               theOccupancyStream("PedestalEqualization");
    ChannelContainerStream<uint8_t>                                 theOffsetStream("PedestalEqualization");

    if(theVcthStreamer.attachBuffer(&dataBuffer))
    {
        std::cout << "Matched PedestalEqualization Vcth!!!!!\n";
        theVcthStreamer.decodeModuleData(fDetectorData);
        fillVplusPlots(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }
    else if(theOccupancyStream.attachBuffer(&dataBuffer))
    {
        std::cout << "Matched PedestalEqualization Occupancy!!!!!\n";
        theOccupancyStream.decodeChipData(fDetectorData);
        fillOccupancyPlots(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }
    else if(theOffsetStream.attachBuffer(&dataBuffer))
    {
        std::cout << "Matched PedestalEqualization Offset!!!!!\n";
        theOffsetStream.decodeChipData(fDetectorData);
        fillOffsetPlots(fDetectorData);
        fDetectorData.cleanDataStored();
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
                TCanvas* offsetCanvas    = new TCanvas(("Offset_" + std::to_string(hybrid->getId())).data(), ("Offset " + std::to_string(hybrid->getId())).data(), 10, 0, 500, 500);
                TCanvas* occupancyCanvas = new TCanvas(("Occupancy_" + std::to_string(hybrid->getId())).data(), ("Occupancy " + std::to_string(hybrid->getId())).data(), 10, 525, 500, 500);

                offsetCanvas->DivideSquare(hybrid->size());
                occupancyCanvas->DivideSquare(hybrid->size());

                for(auto chip: *hybrid)
                {
                    offsetCanvas->cd(chip->getIndex() + 1);
                    TH1I* offsetHistogram = chip->getSummary<HistContainer<TH1I>>().fTheHistogram;
                    offsetHistogram->GetXaxis()->SetTitle("Channel");
                    offsetHistogram->GetYaxis()->SetTitle("Offset");
                    offsetHistogram->DrawCopy();

                    occupancyCanvas->cd(chip->getIndex() + 1);
                    TH1F* occupancyHistogram =
                        fDetectorOccupancyHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
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
                    TH1I* chipVplusHistogram =
                        fDetectorVplusHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1I>>().fTheHistogram;
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
                    if(chip->getChannelContainer<Occupancy>() == nullptr) continue;
                    TH1F* chipOccupancyHistogram =
                        fDetectorOccupancyHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    uint channelBin = 1;
                    for(auto channel: *chip->getChannelContainer<Occupancy>())
                    {
                        chipOccupancyHistogram->SetBinContent(channelBin, channel.fOccupancy);
                        chipOccupancyHistogram->SetBinError(channelBin++, channel.fOccupancyError);
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
                    if(chip->getChannelContainer<uint8_t>() == nullptr) continue;
                    TH1I* chipOffsetHistogram =
                        fDetectorOffsetHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1I>>().fTheHistogram;
                    uint channelBin = 1;
                    for(auto channel: *chip->getChannelContainer<uint8_t>()) { chipOffsetHistogram->SetBinContent(channelBin++, channel); }
                }
            }
        }
    }
}

//========================================================================================================================
