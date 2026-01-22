#include "DQMUtils/DQMHistogramOTcountSSASpuriousClusters.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramOTcountSSASpuriousClusters::DQMHistogramOTcountSSASpuriousClusters() {}

//========================================================================================================================
DQMHistogramOTcountSSASpuriousClusters::~DQMHistogramOTcountSSASpuriousClusters() {}

//========================================================================================================================
void DQMHistogramOTcountSSASpuriousClusters::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    HistContainer<TH1F> missingClusterHistogram("SSA0toMPA8_MissingClusters", "SSA0 to MPA0 missing clusters", 8, -0.5, 7.5);
    missingClusterHistogram.fTheHistogram->GetXaxis()->SetTitle("Cluster line");
    missingClusterHistogram.fTheHistogram->GetYaxis()->SetTitle("Frequency");
    missingClusterHistogram.fTheHistogram->SetStats(false);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStubMissingCountContainer, missingClusterHistogram);
}

//========================================================================================================================
void DQMHistogramOTcountSSASpuriousClusters::fillStubMissingCountContainer(DetectorDataContainer& theStubMissingCountContainer)
{
    for(auto board: theStubMissingCountContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;

                auto thePatternMatchingEfficiencyVector = hybrid->getSummary<GenericDataArray<float, 8>>();

                TH1F* missingStubHistogram = fStubMissingCountContainer.getObject(board->getId())
                                                 ->getObject(opticalGroup->getId())
                                                 ->getObject(hybrid->getId())
                                                 ->getSummary<HistContainer<TH1F>>()
                                                 .fTheHistogram;


                for(size_t cLineId = 0; cLineId < 8; cLineId++)
                {
                    missingStubHistogram->SetBinContent(cLineId + 1, thePatternMatchingEfficiencyVector.at(cLineId));
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTcountSSASpuriousClusters::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTcountSSASpuriousClusters::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTcountSSASpuriousClusters::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization thePatternMatchinEfficiencyContainerSerialization("OTcountSSASpuriousClusterStubMissingCountContainer");

    if(thePatternMatchinEfficiencyContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTverifyMPASSAdataWord PatternMatchingEfficiency!!!!\n";
        DetectorDataContainer theDetectorData =
            thePatternMatchinEfficiencyContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, 8>, EmptyContainer>(
                fDetectorContainer);
        fillStubMissingCountContainer(theDetectorData);
        return true;
    }

    return false;
    // SoC utilities only - END
}
