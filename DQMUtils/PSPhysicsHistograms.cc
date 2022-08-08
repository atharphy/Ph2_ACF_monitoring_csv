/*!
  \file                  PSPhysicsHistograms.cc
  \brief                 Implementation of Physics histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "PSPhysicsHistograms.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ChannelContainerStream.h"
#include "../Utils/ChipContainerStream.h"
#include "../Utils/ContainerFactory.h"

#include "../Utils/ContainerStream.h"
#include "../Utils/PSSync.h"

using namespace Ph2_HwDescription;

void PSPhysicsHistograms::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    fDetectorContainer = &theDetectorStructure;
    for(auto board: *fDetectorContainer)
        for(auto optical: *board)
            for(auto hybrid: *optical)
                for(auto chip: *hybrid)
                {
                    if(chip->getFrontEndType() == FrontEndType::MPA) std::cout << "MPA" << std::endl;
                    if(chip->getFrontEndType() == FrontEndType::MPA2) std::cout << "MPA2" << std::endl;
                    if(chip->getFrontEndType() == FrontEndType::SSA) std::cout << "SSA" << std::endl;
                    if(chip->getFrontEndType() == FrontEndType::SSA2) std::cout << "SSA2" << std::endl;
                }

    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    HistContainer<TH1F> theSClusterTemplateHistogram = HistContainer<TH1F>("S clusters", "S clusters", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5);
    HistContainer<TH2F> thePClusterTemplateHistogram =
        HistContainer<TH2F>("P clusters", "P clusters", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5, NMPACHANNELS / NSSACHANNELS, -0.5, float(NMPACHANNELS / NSSACHANNELS) - 0.5);
    HistContainer<TH2F> theStubTemplateHistogram =
        HistContainer<TH2F>("Stubs", "Stubs", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5, NMPACHANNELS / NSSACHANNELS, -0.5, float(NMPACHANNELS / NSSACHANNELS) - 0.5);

    // auto mpaSelectFunction = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA); };
    // theDetectorStructure.setReadoutChipQueryFunction(mpaSelectFunction);
    RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fStubHistogramContainer, theStubTemplateHistogram);
    RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fOccupancyHistogramContainer, thePClusterTemplateHistogram);
    // theDetectorStructure.resetReadoutChipQueryFunction();

    // auto ssaSelectFunction = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA); };
    // theDetectorStructure.setReadoutChipQueryFunction(ssaSelectFunction);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fStripOccupancyHistogramContainer, theSClusterTemplateHistogram);
    // theDetectorStructure.resetReadoutChipQueryFunction();
}

// void PSPhysicsHistograms::fillSync(const DetectorDataContainer& DataContainer)
// {
//     for(const auto board: DataContainer)
// 	{
//         for(const auto opticalGroup: *board)
// 		{
//             for(const auto hybrid: *opticalGroup)
// 			{
//                 for(const auto chip: *hybrid)
//                 {
//                     TH1F* SClusterHistograms = fSClusterHistograms.at(board->getGlobalIndex())
//                                                         ->at(opticalGroup->getGlobalIndex())
//                                                         ->at(hybrid->getGlobalIndex())
//                                                         ->at(chip->getGlobalIndex())
//                                                         ->getSummary<HistContainer<TH1F>>()
//                                                         .fTheHistogram;
//                     TH2F* PClusterHistograms = fPClusterHistograms.at(board->getGlobalIndex())
//                                                         ->at(opticalGroup->getGlobalIndex())
//                                                         ->at(hybrid->getGlobalIndex())
//                                                         ->at(chip->getGlobalIndex())
//                                                         ->getSummary<HistContainer<TH2F>>()
//                                                         .fTheHistogram;
//                     TH2F* StubHistograms = fStubHistogramContainer.at(board->getGlobalIndex())
//                                                         ->at(opticalGroup->getGlobalIndex())
//                                                         ->at(hybrid->getGlobalIndex())
//                                                         ->at(chip->getGlobalIndex())
//                                                         ->getSummary<HistContainer<TH2F>>()
//                                                         .fTheHistogram;
//                     if(!chip->hasSummary()) continue;
//                     // else LOG(INFO) << BOLDBLUE << "Something received from chip " << chip->getId() << RESET;
// 					auto curPSSync = chip->getSummary<PSSync<MAX_NUMBER_OF_STRIP_CLUSTERS, MAX_NUMBER_OF_PIXEL_CLUSTERS,MAX_NUMBER_OF_STUB_CLUSTERS_PS>>();

//                     for(int pos=0; pos<MAX_NUMBER_OF_STUB_CLUSTERS_PS; ++pos)
//                     {
// 						StubHistograms->Fill(curPSSync.fStubs[pos].getPosition(),curPSSync.fStubs[pos].getRow());
// 		            }
//                     for(int pos=0; pos<MAX_NUMBER_OF_PIXEL_CLUSTERS; ++pos)
// 		            {
//                         if(curPSSync.fPClusters[pos].fAddress != 255u) std::cout<<"good pixel cluster"<<std::endl;
// 						PClusterHistograms->Fill(curPSSync.fPClusters[pos].fAddress ,curPSSync.fPClusters[pos].fZpos);
// 		            }
//                     for(int pos=0; pos<MAX_NUMBER_OF_STRIP_CLUSTERS; ++pos)
//                     {
//                         if(curPSSync.fSClusters[pos].fAddress != 255u) std::cout<<"good pixel cluster"<<std::endl;
// 						SClusterHistograms->Fill(curPSSync.fSClusters[pos].fAddress);
// 		            }

//                 }
//             }
//         }
//     }
// }

void PSPhysicsHistograms::fillOccupancy(const DetectorDataContainer& DataContainer)
{
    // std::cout<<__LINE__<<std::endl;
    for(const auto board: DataContainer)
    {
        // std::cout<<__LINE__<<std::endl;
        for(const auto opticalGroup: *board)
        {
            // std::cout<<__LINE__<<std::endl;
            for(const auto hybrid: *opticalGroup)
            {
                // std::cout<<__LINE__<<std::endl;
                for(const auto chip: *hybrid)
                {
                    // std::cout<<__LINE__<<std::endl;
                    if(chip->getChannelContainer<float>() == nullptr) continue;

                    // std::cout<<__LINE__<<std::endl;
                    // std::cout<<"board = "<<board->getGlobalIndex()<<std::endl;
                    // std::cout<<"opticalGroup = "<<opticalGroup->getGlobalIndex()<<std::endl;
                    // std::cout<<"hybrid = "<<hybrid->getGlobalIndex()<<std::endl;
                    // std::cout<<"chip = "<<chip->getGlobalIndex()<<std::endl;
                    FrontEndType theFrontEndType =
                        fDetectorContainer->at(board->getGlobalIndex())->at(opticalGroup->getGlobalIndex())->at(hybrid->getGlobalIndex())->at(chip->getGlobalIndex())->getFrontEndType();
                    // std::cout<<__LINE__<<std::endl;
                    if(theFrontEndType == FrontEndType::MPA || theFrontEndType == FrontEndType::MPA2)
                    {
                        // std::cout<<__LINE__<<std::endl;
                        TH2F* pixelClusterHistogram = fOccupancyHistogramContainer.at(board->getGlobalIndex())
                                                          ->at(opticalGroup->getGlobalIndex())
                                                          ->at(hybrid->getGlobalIndex())
                                                          ->at(chip->getGlobalIndex())
                                                          ->getSummary<HistContainer<TH2F>>()
                                                          .fTheHistogram;

                        // std::cout<<__LINE__<<std::endl;
                        for(int row = 0; row < NMPACHANNELS / NSSACHANNELS; ++row)
                        {
                            // std::cout<<__LINE__<<std::endl;
                            for(int col = 0; col < NSSACHANNELS; ++col)
                            {
                                // std::cout<<col<<" "<<row<<std::endl;
                                pixelClusterHistogram->Fill(col, row, chip->getChannel<float>(row, col));
                                // std::cout<<__LINE__<<std::endl;
                            }
                        }
                        // std::cout<<__LINE__<<std::endl;
                    }
                    else
                    {
                        // std::cout<<__LINE__<<std::endl;
                        // std::cout<<chip->getGlobalIndex()<<std::endl;
                        // std::cout<<hybrid->size()<<std::endl;

                        TH1F* stripClusterHistogram = fStripOccupancyHistogramContainer.at(board->getGlobalIndex())
                                                          ->at(opticalGroup->getGlobalIndex())
                                                          ->at(hybrid->getGlobalIndex())
                                                          ->at(chip->getGlobalIndex())
                                                          ->getSummary<HistContainer<TH1F>>()
                                                          .fTheHistogram;
                        // std::cout<<__LINE__<<std::endl;
                        for(int channel = 0; channel < NSSACHANNELS; ++channel) { stripClusterHistogram->Fill(channel, chip->getChannel<float>(channel)); }
                        // std::cout<<__LINE__<<std::endl;
                    }
                }
            }
        }
    }
}

void PSPhysicsHistograms::fillStub(const DetectorDataContainer& DataContainer)
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

                    FrontEndType theFrontEndType =
                        fDetectorContainer->at(board->getGlobalIndex())->at(opticalGroup->getGlobalIndex())->at(hybrid->getGlobalIndex())->at(chip->getGlobalIndex())->getFrontEndType();
                    if(theFrontEndType != FrontEndType::MPA && theFrontEndType != FrontEndType::MPA2) continue;

                    TH2F* stubHistogram = fStubHistogramContainer.at(board->getGlobalIndex())
                                              ->at(opticalGroup->getGlobalIndex())
                                              ->at(hybrid->getGlobalIndex())
                                              ->at(chip->getGlobalIndex())
                                              ->getSummary<HistContainer<TH2F>>()
                                              .fTheHistogram;

                    for(int row = 0; row < NMPACHANNELS / NSSACHANNELS; ++row)
                    {
                        for(int col = 0; col < NSSACHANNELS; ++col) { stubHistogram->Fill(col, row, chip->getChannel<float>(row, col)); }
                    }
                }
            }
        }
    }
}

bool PSPhysicsHistograms::fill(std::vector<char>& dataBuffer)
{
    // std::cout<<__PRETTY_FUNCTION__ << "Begin of function"<<std::endl;
    // ChipContainerStream<EmptyContainer, PSSync<MAX_NUMBER_OF_STRIP_CLUSTERS, MAX_NUMBER_OF_PIXEL_CLUSTERS,MAX_NUMBER_OF_STUB_CLUSTERS_PS>> thePSEventStreamer("PSPhysics");
    ChannelContainerStream<float> theOccupancyStream("PSPhysicsOccupancy");
    ChannelContainerStream<float> theStubStream("PSPhysicsStub");

    if(theOccupancyStream.attachBuffer(&dataBuffer))
    {
        std::cout << __PRETTY_FUNCTION__ << "attached Occupancy!!!" << std::endl;
        theOccupancyStream.decodeChipData(fDetectorData);
        std::cout << __LINE__ << std::endl;
        fillOccupancy(fDetectorData);
        std::cout << __LINE__ << std::endl;
        fDetectorData.cleanDataStored();
        std::cout << __LINE__ << std::endl;
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

void PSPhysicsHistograms::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    /*for(auto board: fOccupancy) // for on boards - begin
    {
        size_t boardIndex = board->getGlobalIndex();
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            size_t opticalGroupIndex = opticalGroup->getGlobalIndex();

            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                size_t hybridIndex = hybrid->getGlobalIndex();

                for(auto chip: *hybrid) // for on chip - begin
                {

                } // for on chip - end
            }     // for on hybrid - end
        }         // for on opticalGroup - end
    }             // for on boards - end*/
}
