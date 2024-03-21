#include "DQMUtils/DQMHistogramOTverifyMPASSAdataWord.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramOTverifyMPASSAdataWord::DQMHistogramOTverifyMPASSAdataWord() : DQMHistogramOTverifyCICdataWord() {}

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
        "PatternMatchingEfficiencyMPA_SSA", "Pattern Matching Efficiency MPA-SSA", NUMBER_OF_CIC_PORTS, 8 - 0.5, 8 + NUMBER_OF_CIC_PORTS - 0.5, 2, -0.5, 1.5);
    patternMatchingEfficiencyHistogram.fTheHistogram->GetXaxis()->SetTitle("MPA Id");
    patternMatchingEfficiencyHistogram.fTheHistogram->GetYaxis()->SetTitle("Line");
    patternMatchingEfficiencyHistogram.fTheHistogram->GetYaxis()->SetBinLabel(1, "L1");
    patternMatchingEfficiencyHistogram.fTheHistogram->GetYaxis()->SetBinLabel(2, "Stubs");
    patternMatchingEfficiencyHistogram.fTheHistogram->SetMinimum(0);
    patternMatchingEfficiencyHistogram.fTheHistogram->SetMaximum(1);
    patternMatchingEfficiencyHistogram.fTheHistogram->SetStats(false);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fPatternMatchingEfficiencyHistogramContainer, patternMatchingEfficiencyHistogram);
}

//========================================================================================================================
bool DQMHistogramOTverifyMPASSAdataWord::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization thePatternMatchinEfficiencyContainerSerialization("OTverifyMPASSAdataWordPatternMatchingEfficiency");

    if(thePatternMatchinEfficiencyContainerSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched OTverifyMPASSAdataWord PatternMatchingEfficiency!!!!\n";
        DetectorDataContainer theDetectorData =
            thePatternMatchinEfficiencyContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, NUMBER_OF_CIC_PORTS, 2>, EmptyContainer>(
                fDetectorContainer);
        fillPatternMatchingEfficiencyResults(theDetectorData);
        return true;
    }

    return false;
    // SoC utilities only - END
}
