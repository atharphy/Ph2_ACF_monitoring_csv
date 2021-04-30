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
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/PSSync.h"

using namespace Ph2_HwDescription;

void PSPhysicsHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    HistContainer<TH1F> theSClusterContainer = HistContainer<TH1F>("S clusters", "S clusters", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5);
    HistContainer<TH2F> thePClusterContainer = HistContainer<TH2F>("P clusters", "P clusters", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5, NMPACHANNELS / 120 , -0.5, float(NMPACHANNELS / 120) - 0.5);
    HistContainer<TH2F> theStubContainer = HistContainer<TH2F>("Stubs", "Stubs", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5, NMPACHANNELS / 120 , -0.5, float(NMPACHANNELS / 120) - 0.5);




    RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fStubHistograms, theStubContainer);
    RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fPClusterHistograms, thePClusterContainer);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fSClusterHistograms, theSClusterContainer);
}

void PSPhysicsHistograms::fillSync(const DetectorDataContainer& DataContainer)
{
    for(const auto board: DataContainer)
	{
        for(const auto opticalGroup: *board)
		{
            for(const auto hybrid: *opticalGroup)
			{
                for(const auto chip: *hybrid)
                {
                    TH1F* SClusterHistograms = fSClusterHistograms.at(board->getIndex())
                                                        ->at(opticalGroup->getIndex())
                                                        ->at(hybrid->getIndex())
                                                        ->at(chip->getIndex())
                                                        ->getSummary<HistContainer<TH1F>>()
                                                        .fTheHistogram;
                    TH2F* PClusterHistograms = fPClusterHistograms.at(board->getIndex())
                                                        ->at(opticalGroup->getIndex())
                                                        ->at(hybrid->getIndex())
                                                        ->at(chip->getIndex())
                                                        ->getSummary<HistContainer<TH2F>>()
                                                        .fTheHistogram;
                    TH2F* StubHistograms = fStubHistograms.at(board->getIndex())
                                                        ->at(opticalGroup->getIndex())
                                                        ->at(hybrid->getIndex())
                                                        ->at(chip->getIndex())
                                                        ->getSummary<HistContainer<TH2F>>()
                                                        .fTheHistogram;
                    if(!chip->hasSummary()) continue;
                    // else LOG(INFO) << BOLDBLUE << "Something received from chip " << chip->getId() << RESET;
					auto curPSSync = chip->getSummary<PSSync<MAX_NUMBER_OF_STRIP_CLUSTERS, MAX_NUMBER_OF_PIXEL_CLUSTERS,MAX_NUMBER_OF_STUB_CLUSTERS_PS>>();

                    for(int pos=0; pos<MAX_NUMBER_OF_STUB_CLUSTERS_PS; ++pos)
                    {
						StubHistograms->Fill(curPSSync.fStubs[pos].getPosition(),curPSSync.fStubs[pos].getRow());
		            }
                    for(int pos=0; pos<MAX_NUMBER_OF_PIXEL_CLUSTERS; ++pos)
		            {
						PClusterHistograms->Fill(curPSSync.fPClusters[pos].fAddress ,curPSSync.fPClusters[pos].fZpos);
		            }
                    for(int pos=0; pos<MAX_NUMBER_OF_STRIP_CLUSTERS; ++pos)
                    {
						SClusterHistograms->Fill(curPSSync.fSClusters[pos].fAddress);
		            }
                    
                }
            }
        }
    }
}

bool PSPhysicsHistograms::fill(std::vector<char>& dataBuffer)
{
    // std::cout<<__PRETTY_FUNCTION__ << "Begin of function"<<std::endl;
    ChipContainerStream<EmptyContainer, PSSync<MAX_NUMBER_OF_STRIP_CLUSTERS, MAX_NUMBER_OF_PIXEL_CLUSTERS,MAX_NUMBER_OF_STUB_CLUSTERS_PS>> thePSEventStreamer("PSPhysics");

    if(thePSEventStreamer.attachBuffer(&dataBuffer))
    {
        // std::cout<<__PRETTY_FUNCTION__ << "attached!!"<<std::endl;
        thePSEventStreamer.decodeChipData(fDetectorData); 
        fillSync(fDetectorData);
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
