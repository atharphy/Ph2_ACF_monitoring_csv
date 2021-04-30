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
#include "../Utils/Data2S.h"

using namespace Ph2_HwDescription;

void Physics2SHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    HistContainer<TH1F> theTopSensorClusterHistogram    = HistContainer<TH1F>("TopSensorClusters"   , "Top Sensor Clusters"   , NCHANNELS, -0.5, NCHANNELS/2. - 0.5);
    HistContainer<TH1F> theBottomSensorClusterHistogram = HistContainer<TH1F>("BottomSensorClusters", "Bottom Sensor Clusters", NCHANNELS, -0.5, NCHANNELS/2. - 0.5);
    HistContainer<TH1F> theStubPositionHistogram        = HistContainer<TH1F>("Stub Position"       , "Stub Position"         , NCHANNELS, -0.5, NCHANNELS/2. - 0.5);
    
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fTopClusterHistograms   , theTopSensorClusterHistogram   );
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fBottomClusterHistograms, theBottomSensorClusterHistogram);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fStubPositionHistograms , theStubPositionHistogram       );
   
}

void Physics2SHistograms::fillData(const DetectorDataContainer& DataContainer)
{
    for(const auto board: DataContainer)
	{
        for(const auto opticalGroup: *board)
		{
            for(const auto hybrid: *opticalGroup)
			{
                for(const auto chip: *hybrid)
                {
                    TH1F* topClusterHistograms = fTopClusterHistograms.at(board->getIndex())
                                                        ->at(opticalGroup->getIndex())
                                                        ->at(hybrid->getIndex())
                                                        ->at(chip->getIndex())
                                                        ->getSummary<HistContainer<TH1F>>()
                                                        .fTheHistogram;
                    TH1F* bottomClusterHistograms = fBottomClusterHistograms.at(board->getIndex())
                                                        ->at(opticalGroup->getIndex())
                                                        ->at(hybrid->getIndex())
                                                        ->at(chip->getIndex())
                                                        ->getSummary<HistContainer<TH1F>>()
                                                        .fTheHistogram;
                    TH1F* stubHistograms = fStubPositionHistograms.at(board->getIndex())
                                                        ->at(opticalGroup->getIndex())
                                                        ->at(hybrid->getIndex())
                                                        ->at(chip->getIndex())
                                                        ->getSummary<HistContainer<TH1F>>()
                                                        .fTheHistogram;
					auto data2S = chip->getSummary<Data2S<NCHANNELS, MAX_NUMBER_OF_STUB_CLUSTERS_2S>>();

                    for(int pos=0; pos<MAX_NUMBER_OF_STUB_CLUSTERS_2S; ++pos)
                    {
						stubHistograms->Fill(data2S.fStubs[pos].getPosition());
		            }
                    for(int pos=0; pos<NCHANNELS; ++pos)
		            {
						if(data2S.fClusters[pos].fSensor == 0)topClusterHistograms->Fill(data2S.fClusters[pos].getBaricentre());
						else bottomClusterHistograms->Fill(data2S.fClusters[pos].getBaricentre());
		            }
                    
                }
            }
        }
    }
}

bool Physics2SHistograms::fill(std::vector<char>& dataBuffer)
{
    ChipContainerStream<Data2S<NCHANNELS, MAX_NUMBER_OF_STUB_CLUSTERS_2S>, EmptyContainer> thePSEventStreamer("PSPhysics");

    if(thePSEventStreamer.attachBuffer(&dataBuffer))
    {
        thePSEventStreamer.decodeChipData(fDetectorData);
        fillData(fDetectorData);
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





