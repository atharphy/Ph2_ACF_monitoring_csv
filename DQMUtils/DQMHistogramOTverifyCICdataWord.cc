#include "DQMUtils/DQMHistogramOTverifyCICdataWord.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramOTverifyCICdataWord::DQMHistogramOTverifyCICdataWord() {}

//========================================================================================================================
DQMHistogramOTverifyCICdataWord::~DQMHistogramOTverifyCICdataWord() {}

//========================================================================================================================
void DQMHistogramOTverifyCICdataWord::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    bool isPS = theDetectorStructure.getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS;

    std::string xAxisTitle = "CBC Id";
    int         idOffset   = 0;
    if(isPS)
    {
        xAxisTitle = "MPA Id";
        idOffset   = 8;
    }

    HistContainer<TH2F> patternMatchingEfficiencyHistogram(
        "PatternMatchingEfficiencyCIC", "Pattern Matching Efficiency CIC", NUMBER_OF_CIC_PORTS, idOffset - 0.5, idOffset + NUMBER_OF_CIC_PORTS - 0.5, 2, -0.5, 1.5);
    patternMatchingEfficiencyHistogram.fTheHistogram->GetXaxis()->SetTitle(xAxisTitle.c_str());
    patternMatchingEfficiencyHistogram.fTheHistogram->GetYaxis()->SetTitle("Line");
    patternMatchingEfficiencyHistogram.fTheHistogram->GetYaxis()->SetBinLabel(1, "L1");
    patternMatchingEfficiencyHistogram.fTheHistogram->GetYaxis()->SetBinLabel(2, "Stubs");
    patternMatchingEfficiencyHistogram.fTheHistogram->SetMinimum(0);
    patternMatchingEfficiencyHistogram.fTheHistogram->SetMaximum(1);
    patternMatchingEfficiencyHistogram.fTheHistogram->SetStats(false);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fPatternMatchingEfficiencyHistogramContainer, patternMatchingEfficiencyHistogram);
}

//========================================================================================================================
void DQMHistogramOTverifyCICdataWord::fillPatternMatchingEfficiencyResults(DetectorDataContainer& thePatternMatchingEfficiencyContainer)
{
    for(auto board: thePatternMatchingEfficiencyContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;

                auto thePatternMatchingEfficiencyVector = hybrid->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 2>>();

                TH2F* patternMatchingEfficiencyHistogram = fPatternMatchingEfficiencyHistogramContainer.getObject(board->getId())
                                                               ->getObject(opticalGroup->getId())
                                                               ->getObject(hybrid->getId())
                                                               ->getSummary<HistContainer<TH2F>>()
                                                               .fTheHistogram;

                for(size_t chipId = 0; chipId < NUMBER_OF_CIC_PORTS; ++chipId) // not using the chipID because I want always to read all phases
                {
                    for(size_t cLineId = 0; cLineId < 2; cLineId++) { patternMatchingEfficiencyHistogram->SetBinContent(chipId + 1, cLineId + 1, thePatternMatchingEfficiencyVector[chipId][cLineId]); }
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTverifyCICdataWord::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTverifyCICdataWord::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTverifyCICdataWord::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization thePatternMatchinEfficiencyContainerSerialization("OTverifyCICdataWordPatternMatchingEfficiency");

    if(thePatternMatchinEfficiencyContainerSerialization.attachDeserializer(inputStream))
    {
        //std::cout << "Matched OTverifyCICdataWord PatternMatchingEfficiency!!!!\n";
        DetectorDataContainer theDetectorData =
            thePatternMatchinEfficiencyContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, NUMBER_OF_CIC_PORTS, 2>, EmptyContainer>(
                fDetectorContainer);
        fillPatternMatchingEfficiencyResults(theDetectorData);
        return true;
    }

    return false;
    // SoC utilities only - END
}
