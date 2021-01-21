/*!
        \file                DQMHistogramSignalScanFit.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
 */

#include "../DQMUtils/DQMHistogramSignalScanFit.h"
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

//========================================================================================================================
DQMHistogramSignalScanFit::DQMHistogramSignalScanFit() {}

//========================================================================================================================
DQMHistogramSignalScanFit::~DQMHistogramSignalScanFit() {}

//========================================================================================================================
void DQMHistogramSignalScanFit::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
}

//========================================================================================================================
bool DQMHistogramSignalScanFit::fill(std::vector<char>& dataBuffer) { return false; }

//========================================================================================================================
void DQMHistogramSignalScanFit::process() {}

//========================================================================================================================

void DQMHistogramSignalScanFit::reset(void) {}
