/*!
  \file                  Physics2SHistograms.cc
  \brief                 Implementation of Physics histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "Physics2SHistograms.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/ChipContainerStream.h"
#include "../Utils/ChannelContainerStream.h"

#include "../Utils/Data2S.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;

void Physics2SHistograms::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    HistContainer<TH1F> theTopSensorOccupancyHistogram    = HistContainer<TH1F>("TopSensorOccupancy", "Top Sensor Occupancy", NCHANNELS / 2, -0.5, float(NCHANNELS / 2.) - 0.5);
    HistContainer<TH1F> theBottomSensorOccupancyHistogram = HistContainer<TH1F>("BottomSensorOccupancy", "Bottom Sensor Occupancy", NCHANNELS / 2, -0.5, float(NCHANNELS / 2.) - 0.5);
    HistContainer<TH1F> theStubPositionHistogram          = HistContainer<TH1F>("Stub Position", "Stub Position", NCHANNELS, -0.25, float(NCHANNELS / 2.) - 0.25);

    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fTopSensorHistogramContainer, theTopSensorOccupancyHistogram);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fBottomSensorHistogramContainer, theBottomSensorOccupancyHistogram);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fStubHistogramContainer, theStubPositionHistogram);
}

// void Physics2SHistograms::fillData(const DetectorDataContainer& DataContainer)
// {
//     for(const auto board: DataContainer)
// 	{
//         for(const auto opticalGroup: *board)
// 		{
//             for(const auto hybrid: *opticalGroup)
// 			{
//                 for(const auto chip: *hybrid)
//                 {
//                     TH1F* topClusterHistograms = fTopClusterHistograms.at(board->getIndex())
//                                                         ->at(opticalGroup->getIndex())
//                                                         ->at(hybrid->getIndex())
//                                                         ->at(chip->getIndex())
//                                                         ->getSummary<HistContainer<TH1F>>()
//                                                         .fTheHistogram;
//                     TH1F* bottomClusterHistograms = fBottomClusterHistograms.at(board->getIndex())
//                                                         ->at(opticalGroup->getIndex())
//                                                         ->at(hybrid->getIndex())
//                                                         ->at(chip->getIndex())
//                                                         ->getSummary<HistContainer<TH1F>>()
//                                                         .fTheHistogram;
//                     TH1F* stubHistograms = fStubPositionHistograms.at(board->getIndex())
//                                                         ->at(opticalGroup->getIndex())
//                                                         ->at(hybrid->getIndex())
//                                                         ->at(chip->getIndex())
//                                                         ->getSummary<HistContainer<TH1F>>()
//                                                         .fTheHistogram;
// 					auto data2S = chip->getSummary<Data2S<NCHANNELS, MAX_NUMBER_OF_STUB_CLUSTERS_2S>>();

//                     for(int pos=0; pos<MAX_NUMBER_OF_STUB_CLUSTERS_2S; ++pos)
//                     {
// 						stubHistograms->Fill(data2S.fStubs[pos].getPosition());
// 		            }
//                     for(int pos=0; pos<NCHANNELS; ++pos)
// 		            {
// 						if(data2S.fClusters[pos].fSensor == 0)topClusterHistograms->Fill(data2S.fClusters[pos].getBaricentre());
// 						else bottomClusterHistograms->Fill(data2S.fClusters[pos].getBaricentre());
// 		            }

//                 }
//             }
//         }
//     }
// }

void Physics2SHistograms::fillOccupancy(const DetectorDataContainer& DataContainer)
{
    for(const auto board: DataContainer)
    {
        for(const auto opticalGroup: *board)
        {
            for(const auto hybrid: *opticalGroup)
            {
                for(const auto chip: *hybrid)
                {
                    if(chip->getChannelContainer<Occupancy>() == nullptr) continue;

                    TH1F* topSensorHistogram =
                        fTopSensorHistogramContainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;

                    TH1F* bottomSensorHistogram = fBottomSensorHistogramContainer.at(board->getIndex())
                                                      ->at(opticalGroup->getIndex())
                                                      ->at(hybrid->getIndex())
                                                      ->at(chip->getIndex())
                                                      ->getSummary<HistContainer<TH1F>>()
                                                      .fTheHistogram;

                    uint16_t channelNumber = 0;
                    for(auto channel: *chip->getChannelContainer<Occupancy>())
                    {
                        if((int(channelNumber) % 2) == 0) { bottomSensorHistogram->Fill(int(channelNumber / 2) + 1, channel.fOccupancy); }
                        else
                        {
                            topSensorHistogram->Fill(int(channelNumber / 2) + 1, channel.fOccupancy);
                        }
                        ++channelNumber;
                    }
                }
            }
        }
    }
}

void Physics2SHistograms::fillStub(const DetectorDataContainer& DataContainer)
{
    for(const auto board: DataContainer)
    {
        for(const auto opticalGroup: *board)
        {
            for(const auto hybrid: *opticalGroup)
            {
                for(const auto chip: *hybrid)
                {
                    if(chip->getChannelContainer<float>() == nullptr) continue;

                    TH2F* stubHistogram =
                        fStubHistogramContainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;

                    uint16_t channelNumber = 0;
                    for(auto channel: *chip->getChannelContainer<float>())
                    {
                        {
                            stubHistogram->Fill(float(channelNumber / 2.), channel);
                        }
                        ++channelNumber;
                    }
                }
            }
        }
    }
}

bool Physics2SHistograms::fill(std::vector<char>& dataBuffer)
{
    // std::cout<<__PRETTY_FUNCTION__ << "Begin of function"<<std::endl;
    // ChipContainerStream<EmptyContainer, PSSync<MAX_NUMBER_OF_STRIP_CLUSTERS, MAX_NUMBER_OF_PIXEL_CLUSTERS,MAX_NUMBER_OF_STUB_CLUSTERS_PS>> thePSEventStreamer("PSPhysics");
    ChannelContainerStream<Occupancy> theOccupancyStream("Physics2SOccupancy");
    ChannelContainerStream<float>     theStubStream("Physics2SStub");

    if(theOccupancyStream.attachBuffer(&dataBuffer))
    {
        std::cout << __PRETTY_FUNCTION__ << "attached Occupancy!!!" << std::endl;
        theOccupancyStream.decodeChipData(fDetectorData);
        fillOccupancy(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }

    if(theStubStream.attachBuffer(&dataBuffer))
    {
        std::cout << __PRETTY_FUNCTION__ << "attached Stub!!!" << std::endl;
        theStubStream.decodeChipData(fDetectorData);
        fillStub(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }
    return false;
}

void Physics2SHistograms::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    /*for(auto board: fOccupancy) // for on boards - begin
    {
        size_t boardIndex = board->getIndex();
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            size_t opticalGroupIndex = opticalGroup->getIndex();

            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                size_t hybridIndex = hybrid->getIndex();

                for(auto chip: *hybrid) // for on chip - begin
                {

                } // for on chip - end
            }     // for on hybrid - end
        }         // for on opticalGroup - end
    }             // for on boards - end*/
}
