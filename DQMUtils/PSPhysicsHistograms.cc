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
					PSSync curPSSync = chip->getSummary<PSSync>();

                    for(auto& st: curPSSync.fStubs)
		            {
						StubHistograms->Fill(st.getPosition(),st.getRow());
		            }
                    for(auto& pc: curPSSync.fPClusters)
		            {
						PClusterHistograms->Fill(pc.fAddress ,pc.fZpos);
		            }

                    for(auto& sc: curPSSync.fSClusters)
		            {
						SClusterHistograms->Fill(sc.fAddress);
		            }
                    
                }
            }
        }
    }
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
