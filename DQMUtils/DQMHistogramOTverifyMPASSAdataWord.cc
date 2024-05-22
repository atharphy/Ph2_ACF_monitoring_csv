#include "DQMUtils/DQMHistogramOTverifyMPASSAdataWord.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramOTverifyMPASSAdataWord::DQMHistogramOTverifyMPASSAdataWord() {}

//========================================================================================================================
DQMHistogramOTverifyMPASSAdataWord::~DQMHistogramOTverifyMPASSAdataWord() {}

//========================================================================================================================
void DQMHistogramOTverifyMPASSAdataWord::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    HistContainer<TH2F> patternMatchingEfficiencyHistogram(
        "PatternMatchingEfficiencyMPA_SSA", "Pattern Matching Efficiency MPA-SSA", NUMBER_OF_CIC_PORTS, 8 - 0.5, 8 + NUMBER_OF_CIC_PORTS - 0.5, 9, -0.5, 8.5);
    patternMatchingEfficiencyHistogram.fTheHistogram->GetXaxis()->SetTitle("MPA Id");
    patternMatchingEfficiencyHistogram.fTheHistogram->GetYaxis()->SetTitle("Line");
    patternMatchingEfficiencyHistogram.fTheHistogram->GetYaxis()->SetBinLabel(1, "L1");
    for(size_t clusterLine = 0; clusterLine<8; ++clusterLine) patternMatchingEfficiencyHistogram.fTheHistogram->GetYaxis()->SetBinLabel(clusterLine+2, Form("Cluster line %d", int(clusterLine)));
    patternMatchingEfficiencyHistogram.fTheHistogram->SetMinimum(0);
    patternMatchingEfficiencyHistogram.fTheHistogram->SetMaximum(1);
    patternMatchingEfficiencyHistogram.fTheHistogram->SetStats(false);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fPatternMatchingEfficiencyHistogramContainer, patternMatchingEfficiencyHistogram);
}


//========================================================================================================================
void DQMHistogramOTverifyMPASSAdataWord::fillPatternMatchingEfficiencyResults(DetectorDataContainer& thePatternMatchingEfficiencyContainer)
{
    for(auto board: thePatternMatchingEfficiencyContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;

                auto thePatternMatchingEfficiencyVector = hybrid->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>();

                TH2F* patternMatchingEfficiencyHistogram = fPatternMatchingEfficiencyHistogramContainer.getObject(board->getId())
                                                               ->getObject(opticalGroup->getId())
                                                               ->getObject(hybrid->getId())
                                                               ->getSummary<HistContainer<TH2F>>()
                                                               .fTheHistogram;

                for(size_t chipId = 0; chipId < NUMBER_OF_CIC_PORTS; ++chipId) // not using the chipID because I want always to read all phases
                {
                    for(size_t cLineId = 0; cLineId < 9; cLineId++) { patternMatchingEfficiencyHistogram->SetBinContent(chipId + 1, cLineId + 1, thePatternMatchingEfficiencyVector[chipId][cLineId]); }
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTverifyMPASSAdataWord::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTverifyMPASSAdataWord::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTverifyMPASSAdataWord::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization thePatternMatchinEfficiencyContainerSerialization("OTverifyMPASSAdataWordPatternMatchingEfficiency");

    if(thePatternMatchinEfficiencyContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTverifyMPASSAdataWord PatternMatchingEfficiency!!!!\n";
        DetectorDataContainer theDetectorData =
            thePatternMatchinEfficiencyContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>, EmptyContainer>(
                fDetectorContainer);
        fillPatternMatchingEfficiencyResults(theDetectorData);
        return true;
    }

    return false;
    // SoC utilities only - END
}
