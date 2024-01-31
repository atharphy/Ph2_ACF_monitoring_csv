#include "DQMUtils/DQMHistogramOTalignBoardDataWord.h"
#include "RootUtils/RootContainerFactory.h"
#include "TH1I.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"

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
    // make fDetectorData ready to receive the information fromm the stream
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    // SoC utilities only - END

    size_t              numberOfLines = (theDetectorStructure.getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
    HistContainer<TH1I> bitSlipHistogram("BitSlipValues", "Bit slip values", numberOfLines, -0.5, numberOfLines - 0.5);
    bitSlipHistogram.fTheHistogram->GetXaxis()->SetTitle("Line number");
    bitSlipHistogram.fTheHistogram->GetYaxis()->SetTitle("Bitslip value");
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fBitSlipHistogramContainer, bitSlipHistogram);
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
                auto theHybridBitSlipVector = hybrid->getSummary<std::vector<uint8_t>>();
                for(size_t lineId = 0; lineId < theHybridBitSlipVector.size(); ++lineId) { hybridBitSlipHistogram->SetBinContent(lineId + 1, theHybridBitSlipVector[lineId]); }
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
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    // As example, I'm expecting to receive a data stream from an uint32_t contained from calibration "OTalignBoardDataWord"
    // ContainerSerialization myStreamer("OTalignBoardDataWord");

    // if(myStreamer.attachDeserializer(inputStream))
    // {
    //     // It matched! Decoding data
    //     std::cout << "Matched OTalignBoardDataWord!!!!!\n";
    //     // Need to tell to the streamer what data are contained (in this case in every channel there is an object of type MyType)
    //     DetectorDataContainer theDetectorData = myStreamer.deserializeChannelContainer<MyType>(fDetectorContainer);
    //     // Filling the histograms
    //     myFillplotFunction(theDetectorData);
    //     return true;
    // }
    // the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    // for this stream)
    return false;
    // SoC utilities only - END
}
