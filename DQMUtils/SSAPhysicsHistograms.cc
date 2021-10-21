/*!
  \file                  SSAPhysicsHistograms.cc
  \brief                 Implementation of Physics histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "SSAPhysicsHistograms.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;

void SSAPhysicsHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);

    HistContainer<TH1F> theOcccupancyContainer = HistContainer<TH1F>("Occ2D", "Occupancy", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fOccupancy, theOcccupancyContainer);
}

bool SSAPhysicsHistograms::fill(std::vector<char>& dataBuffer)
{
    ChannelContainerStream<Occupancy> theOccStreamer("SSAPhysicsOcc");

    if(theOccStreamer.attachBuffer(&dataBuffer))
    {
        theOccStreamer.decodeChipData(fDetectorData);
        fillOccupancy(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }
    return false;
}

void SSAPhysicsHistograms::fillOccupancy(const DetectorDataContainer& DataContainer)
{
    for(const auto cBoard: DataContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->getChannelContainer<Occupancy>() == nullptr) continue;

                    auto* chipOccupancy =
                        fOccupancy.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    uint channelBin = 1;

                    // Get channel data and fill the histogram
                    for(auto channel: *cChip->getChannelContainer<Occupancy>()) // for on channel - begin
                    { chipOccupancy->Fill(channelBin++, channel.fOccupancy); }
                }
}

void SSAPhysicsHistograms::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    for(auto board: fOccupancy) // for on boards - begin
    {
        size_t boardIndex = board->getIndex();
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            size_t opticalGroupIndex = opticalGroup->getIndex();

            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                size_t hybridIndex = hybrid->getIndex();

                // Create a canvas do draw the plots
                TCanvas* cOccupancy = new TCanvas(("Occupancy_hybrid_" + std::to_string(hybrid->getId())).data(), ("Hits hybrid " + std::to_string(hybrid->getId())).data(), 0, 0, 650, 650);
                cOccupancy->Divide(hybrid->size());

                for(auto chip: *hybrid) // for on chip - begin
                {
                    size_t chipIndex = chip->getIndex();
                    cOccupancy->cd(chipIndex + 1);
                    // Retreive the corresponging chip histogram:
                    TH1F* chipHitHistogram = fOccupancy.at(boardIndex)->at(opticalGroupIndex)->at(hybridIndex)->at(chipIndex)->getSummary<HistContainer<TH1F>>().fTheHistogram;

                    // Format the histogram (here you are outside from the SoC so you can use all the ROOT functions you
                    // need)
                    chipHitHistogram->SetStats(false);
                    chipHitHistogram->DrawCopy();
                } // for on chip - end
            }     // for on hybrid - end
        }         // for on opticalGroup - end
    }             // for on boards - end
}
