#include "DQMUtils/DQMHistogramOTverifyBoardDataWord.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"

#include "TFile.h"
#include "TH1F.h"

//========================================================================================================================
DQMHistogramOTverifyBoardDataWord::DQMHistogramOTverifyBoardDataWord() {}

//========================================================================================================================
DQMHistogramOTverifyBoardDataWord::~DQMHistogramOTverifyBoardDataWord() {}

//========================================================================================================================
void DQMHistogramOTverifyBoardDataWord::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    size_t numberOfLines = (theDetectorStructure.getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;

    auto setBitLabel = [numberOfLines](TH1F* theHistogram)
    {
        theHistogram->GetXaxis()->SetBinLabel(1, "L1");
        for(size_t stubLine = 0; stubLine < numberOfLines; ++stubLine) theHistogram->GetXaxis()->SetBinLabel(stubLine + 2, Form("Stub%d", int(stubLine)));
    };

    HistContainer<TH1F> bitSlipHistogram("PatternMatchingEfficiency", "Pattern Matching Efficiency", numberOfLines, -0.5, numberOfLines - 0.5);
    bitSlipHistogram.fTheHistogram->GetXaxis()->SetTitle("Line number");
    bitSlipHistogram.fTheHistogram->GetYaxis()->SetTitle("Efficiency");
    setBitLabel(bitSlipHistogram.fTheHistogram);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fMatchingEfficiencyHistogramContainer, bitSlipHistogram);
}

//========================================================================================================================
void DQMHistogramOTverifyBoardDataWord::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTverifyBoardDataWord::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================

void DQMHistogramOTverifyBoardDataWord::fillPatternMatchingEfficiency(DetectorDataContainer& thePatternMatchingEfficiencyContainer)
{
    for(auto board: thePatternMatchingEfficiencyContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;
                TH1I* hybridMatchingEfficiencyHistogram =
                    fMatchingEfficiencyHistogramContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH1I>>().fTheHistogram;
                auto theHybridMatchingEfficiencyVector = hybrid->getSummary<std::vector<float>>();
                for(size_t lineId = 0; lineId < theHybridMatchingEfficiencyVector.size(); ++lineId)
                {
                    hybridMatchingEfficiencyHistogram->SetBinContent(lineId + 1, theHybridMatchingEfficiencyVector[lineId]);
                }
            }
        }
    }
}

//========================================================================================================================
bool DQMHistogramOTverifyBoardDataWord::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization theMatchingEfficiencyContainerSerialization("OTverifyBoardDataWordMatchingEfficiency");

    if(theMatchingEfficiencyContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTverifyBoardDataWord MatchingEfficiency!!!!\n";
        DetectorDataContainer theDetectorData =
            theMatchingEfficiencyContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, std::vector<float>, EmptyContainer>(fDetectorContainer);
        fillPatternMatchingEfficiency(theDetectorData);
        return true;
    }

    return false;
    // SoC utilities only - END
}
