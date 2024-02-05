#include "DQMUtils/DQMHistogramOTalignLpGBTinputs.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"

#include "TFile.h"
#include "TH1I.h"
#include "TH1F.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramOTalignLpGBTinputs::DQMHistogramOTalignLpGBTinputs() {}

//========================================================================================================================
DQMHistogramOTalignLpGBTinputs::~DQMHistogramOTalignLpGBTinputs() {}

//========================================================================================================================
void DQMHistogramOTalignLpGBTinputs::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    fGroupAndChannelToBinNumber.clear();
    const auto theGroupsAndChannels = theDetectorStructure.getFirstObject()->getFirstObject()->getLpGBTrxGroupsAndChannels();

    int numberOfBins = 0;
    for(const auto& groupAndChannels : theGroupsAndChannels)
    {
        for(const auto channel : groupAndChannels.second)
        {
            fGroupAndChannelToBinNumber[groupAndChannels.first][channel] = numberOfBins + 1;
            ++numberOfBins;
        }
    }

    auto setBinLabels = [this](TAxis* theHistogram)
    {
        for(const auto& group : this->fGroupAndChannelToBinNumber)
        {
            for(const auto& channelAndBin : group.second)
            {
                theHistogram->SetBinLabel(channelAndBin.second, Form("%d_%d", group.first, channelAndBin.first));
            }
        }
    };

    HistContainer<TH1F> alignmentSuccessHistogram("LpGBTinputAlignmentSuccess", "LpGBT input best phase", numberOfBins, -0.5, numberOfBins - 0.5);
    alignmentSuccessHistogram.fTheHistogram->GetXaxis()->SetTitle("group_channel");
    setBinLabels(alignmentSuccessHistogram.fTheHistogram->GetXaxis());
    alignmentSuccessHistogram.fTheHistogram->GetYaxis()->SetTitle("alignment efficiency");
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fAlignmentSuccessHistogramContainer, alignmentSuccessHistogram);

    HistContainer<TH1I> bestPhaseHistogram("LpGBTinputBestPhase", "LpGBT input best phase", numberOfBins, -0.5, numberOfBins - 0.5);
    bestPhaseHistogram.fTheHistogram->GetXaxis()->SetTitle("group_channel");
    setBinLabels(bestPhaseHistogram.fTheHistogram->GetXaxis());
    bestPhaseHistogram.fTheHistogram->GetYaxis()->SetTitle("best phase value");
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fBestPhaseHistogramContainer, bestPhaseHistogram);

    HistContainer<TH2F> foundPhasesDistributionHistogram("LpGBTinputFoundPhasesDistribution", "LpGBT input found phases distribution", numberOfBins, -0.5, numberOfBins - 0.5, 16, -0.5, 15.5);
    foundPhasesDistributionHistogram.fTheHistogram->GetXaxis()->SetTitle("group_channel");
    setBinLabels(foundPhasesDistributionHistogram.fTheHistogram->GetXaxis());
    foundPhasesDistributionHistogram.fTheHistogram->GetYaxis()->SetTitle("phase");
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fFoundPhasesDistributionHistogramContainer, foundPhasesDistributionHistogram);

}

//========================================================================================================================

void DQMHistogramOTalignLpGBTinputs::fillPhaseAlignmentResults(DetectorDataContainer& thePhaseAlignmentResultContainer)
{
    for(auto board: thePhaseAlignmentResultContainer)
    {
        for(auto opticalGroup: *board)
        {
            if(!opticalGroup->hasSummary()) continue;
            auto theHybridRetryNumberVector = opticalGroup->getSummary<std::map<uint8_t, std::map<uint8_t, std::tuple<float, uint8_t, std::array<float, 16>>>>>();

            TH1F* hybridAlignmentSuccessHistogram =
                fAlignmentSuccessHistogramContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
            TH1I* hybridBestPhaseHistogramHistogram =
                fBestPhaseHistogramContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH1I>>().fTheHistogram;
            TH2F* hybridFoundPhasesDistributionHistogram =
                fFoundPhasesDistributionHistogramContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;

            for(const auto& theGroupResult : theHybridRetryNumberVector)
            {
                for(const auto& theChannelResult : theGroupResult.second)
                {
                    float alignmentSuccessRate = std::get<0>(theChannelResult.second);
                    uint8_t bestPhaseValue = std::get<1>(theChannelResult.second);
                    std::array<float, 16> foundPhaseHistogram = std::get<2>(theChannelResult.second);
                    int currentBit = fGroupAndChannelToBinNumber[theGroupResult.first][theChannelResult.first];

                    hybridAlignmentSuccessHistogram->SetBinContent(currentBit, alignmentSuccessRate);
                    hybridBestPhaseHistogramHistogram->SetBinContent(currentBit, bestPhaseValue);
                    for(size_t phaseValue=0; phaseValue<foundPhaseHistogram.size(); ++phaseValue) hybridFoundPhasesDistributionHistogram->SetBinContent(currentBit, phaseValue+1, foundPhaseHistogram[phaseValue]);
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTalignLpGBTinputs::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTalignLpGBTinputs::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTalignLpGBTinputs::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    // ContainerSerialization theAlignmentResultsContainerSerialization("OTalignLpGBTinputsAlignmentResults");

    // if(theAlignmentResultsContainerSerialization.attachDeserializer(inputStream))
    // {
    //     std::cout << "Matched OTalignLpGBTinputs AlignmentResults!!!!\n";
    //     DetectorDataContainer theDetectorData =
    //         theAlignmentResultsContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, EmptyContainer, std::map<uint8_t, std::map<uint8_t, std::tuple<float, uint8_t, std::array<float, 16>>>>>(fDetectorContainer);
    //     fillPhaseAlignmentResults(theDetectorData);
    //     return true;
    // }
    
    return false;
    // SoC utilities only - END
}
