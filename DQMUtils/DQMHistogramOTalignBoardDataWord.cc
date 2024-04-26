#include "DQMUtils/DQMHistogramOTalignBoardDataWord.h"
#include "RootUtils/RootContainerFactory.h"
#include "TH1I.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"

#include "TFile.h"

//========================================================================================================================
DQMHistogramOTalignBoardDataWord::DQMHistogramOTalignBoardDataWord() {}

//========================================================================================================================
DQMHistogramOTalignBoardDataWord::~DQMHistogramOTalignBoardDataWord() {}

//========================================================================================================================
void DQMHistogramOTalignBoardDataWord::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    size_t              numberOfLines = (theDetectorStructure.getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
    HistContainer<TH1I> bitSlipHistogram("BitSlipValues", "Bit slip values", numberOfLines, -0.5, numberOfLines - 0.5);
    bitSlipHistogram.fTheHistogram->GetXaxis()->SetTitle("Line number");
    bitSlipHistogram.fTheHistogram->GetYaxis()->SetTitle("Bitslip value");
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fBitSlipHistogramContainer, bitSlipHistogram);

    HistContainer<TH1I> alignmentRetryHistogram("WordAlignmentRetryNumbers", "Word alignment retry numbers", numberOfLines, -0.5, numberOfLines - 0.5);
    alignmentRetryHistogram.fTheHistogram->GetXaxis()->SetTitle("Line number");
    alignmentRetryHistogram.fTheHistogram->GetYaxis()->SetTitle("Retry number");
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fAlignmentRetryHistogramContainer, alignmentRetryHistogram);
}

//========================================================================================================================

void DQMHistogramOTalignBoardDataWord::fillBitSlipValues(DetectorDataContainer& theBitSlipContainer)
{
    for(auto board: theBitSlipContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;
                TH1I* hybridBitSlipHistogram =
                    fBitSlipHistogramContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH1I>>().fTheHistogram;
                auto theHybridRetryNumberVector = hybrid->getSummary<std::vector<uint8_t>>();
                for(size_t lineId = 0; lineId < theHybridRetryNumberVector.size(); ++lineId) { hybridBitSlipHistogram->SetBinContent(lineId + 1, theHybridRetryNumberVector[lineId]); }
            }
        }
    }
}

//========================================================================================================================

void DQMHistogramOTalignBoardDataWord::fillAlignmentRetryNumber(DetectorDataContainer& theAlignmentRetryContainer)
{
    for(auto board: theAlignmentRetryContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;
                TH1I* hybridRetryNumberHistogram =
                    fAlignmentRetryHistogramContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH1I>>().fTheHistogram;
                auto theHybridRetryNumberVector = hybrid->getSummary<std::vector<uint8_t>>();
                for(size_t lineId = 0; lineId < theHybridRetryNumberVector.size(); ++lineId) { hybridRetryNumberHistogram->SetBinContent(lineId + 1, theHybridRetryNumberVector[lineId]); }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTalignBoardDataWord::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTalignBoardDataWord::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTalignBoardDataWord::fill(std::string& inputStream)
{
    ContainerSerialization theBitSlipContainerSerialization("OTalignBoardDataWordBitSlip");
    ContainerSerialization theAlignmentRetryContainerSerialization("OTalignBoardDataWordAlignmentRetry");

    if(theBitSlipContainerSerialization.attachDeserializer(inputStream))
    {
        //std::cout << "Matched OTalignBoardDataWord BitSlip!!!!\n";
        DetectorDataContainer theDetectorData =
            theBitSlipContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, std::vector<uint8_t>, EmptyContainer>(fDetectorContainer);
        fillBitSlipValues(theDetectorData);
        return true;
    }
    if(theAlignmentRetryContainerSerialization.attachDeserializer(inputStream))
    {
        //std::cout << "Matched OTalignBoardDataWord AlignmentRetry!!!!!\n";
        DetectorDataContainer theDetectorData =
            theAlignmentRetryContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, std::vector<uint8_t>, EmptyContainer>(fDetectorContainer);
        fillAlignmentRetryNumber(theDetectorData);
        return true;
    }

    return false;
}
