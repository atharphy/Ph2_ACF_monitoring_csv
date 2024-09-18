#include "DQMUtils/DQMHistogramOTCICtoLpGBTecv.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"

#include "TFile.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramOTCICtoLpGBTecv::DQMHistogramOTCICtoLpGBTecv() {}

//========================================================================================================================
DQMHistogramOTCICtoLpGBTecv::~DQMHistogramOTCICtoLpGBTecv() {}

//========================================================================================================================
void DQMHistogramOTCICtoLpGBTecv::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;

    // Create a list of the possible CICSignalStrength and CICClockPolarity combinations. The left digit refers to the CICSignalStrength, the right digit refers to the CICClockPolarity
    std::vector<float> listOfLpGBTPhase    = convertStringToFloatList(findValueInSettings<std::string>(pSettingsMap, "OTCICtoLpGBTecv_LpGBTPhase", "0-14"));
    std::vector<float> listOfCICStrength   = convertStringToFloatList(findValueInSettings<std::string>(pSettingsMap, "OTCICtoLpGBTecv_CICStrength", "1, 3, 5"));
    std::vector<float> listOfClockPolarity = convertStringToFloatList(findValueInSettings<std::string>(pSettingsMap, "OTCICtoLpGBTecv_ClockPolarity", "0-1"));
    std::vector<float> listOfClockStrength = convertStringToFloatList(findValueInSettings<std::string>(pSettingsMap, "OTCICtoLpGBTecv_ClockStrength", "1, 4, 7"));
    size_t             numberOfLines       = (fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;

    // x-axis is line:clock strenghts and y-axis is LpGBT phase
    for(auto polarity: listOfClockPolarity)
    {
        for(auto CICStrength: listOfCICStrength)
        {
            // Declare histogram axes titles and number of bins
            size_t              numberOfYAxisBins = listOfLpGBTPhase.size();
            size_t              numberofXaxisBins = numberOfLines * listOfClockStrength.size();
            HistContainer<TH2F> ECVEfficiencyHistogram(Form("Efficiency_CIC_Clock_Polarity_%.0f-CIC_Signal_Strength_%.0f", polarity, CICStrength),
                                                       Form("Polarity %.0f CIC Strength %.0f", polarity, CICStrength),
                                                       numberofXaxisBins,
                                                       0,
                                                       numberofXaxisBins,
                                                       numberOfYAxisBins,
                                                       0,
                                                       numberOfYAxisBins);

            ECVEfficiencyHistogram.fTheHistogram->GetXaxis()->SetTitle("Line : Clock strenghts");
            ECVEfficiencyHistogram.fTheHistogram->GetYaxis()->SetTitle("lpGBT Phase");
            ECVEfficiencyHistogram.fTheHistogram->SetStats(false);
            ECVEfficiencyHistogram.fTheHistogram->GetYaxis()->SetLabelSize(0.04);
            ECVEfficiencyHistogram.fTheHistogram->GetXaxis()->SetLabelSize(0.02);

            // Label the y axis with the lpGBT phase
            int binNumber = 1;
            for(auto phase: listOfLpGBTPhase)
            {
                std::string s = convertToString(phase);
                ECVEfficiencyHistogram.fTheHistogram->GetYaxis()->SetBinLabel(binNumber, s.c_str());
                binNumber++;
            }

            // Label the x axis with the line and clock strength
            binNumber = 1;
            for(uint32_t channel = 1; channel <= numberOfLines; channel++)
            {
                for(auto hybridClockStrength: listOfClockStrength)
                {
                    std::string prefix = channel == 1 ? "L1" : "Stub" + convertToString(channel - 1);
                    std::string s      = prefix + ":" + convertToString(hybridClockStrength);
                    ECVEfficiencyHistogram.fTheHistogram->GetXaxis()->SetBinLabel(binNumber, s.c_str());
                    binNumber++;
                }
            }

            // Book the histograms
            auto CICStrengthPolarityCombination = (CICStrength * 10) + polarity;
            ECVEfficiencyHistogram.fTheHistogram->LabelsOption("v", "X");
            ECVEfficiencyHistogram.fTheHistogram->DrawCopy("text");
            RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fEfficiency[CICStrengthPolarityCombination], ECVEfficiencyHistogram);
        }
    }
}

//========================================================================================================================
void DQMHistogramOTCICtoLpGBTecv::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTCICtoLpGBTecv::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTCICtoLpGBTecv::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    // As example, I'm expecting to receive a data stream from an uint32_t contained from calibration "OTverifyECVlpGBTCIC"
    ContainerSerialization theECVlpGBTCICContainerSerialization("OTCICtoLpGBTecvEfficiencyHistogram");

    if(theECVlpGBTCICContainerSerialization.attachDeserializer(inputStream))
    {
        // It matched! Decoding data
        std::cout << "Matched OTCICtoLpGBTecv EfficiencyHistogram!!!!!\n";
        // Need to tell to the streamer what data are contained (in this case in every channel there is an object of type MyType)
        uint8_t               pClockStrengthLengthOfOptions, pClockPolarity, pClockStrengthIndex, pCicStrength, pPhaseIndex;
        DetectorDataContainer theDetectorData = theECVlpGBTCICContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, float, EmptyContainer>(
            fDetectorContainer, pClockStrengthLengthOfOptions, pClockPolarity, pClockStrengthIndex, pCicStrength, pPhaseIndex);

        // Filling the histograms
        fillEfficiency(pClockStrengthLengthOfOptions, pClockPolarity, pClockStrengthIndex, pCicStrength, pPhaseIndex, theDetectorData);
        return true;
    }

    // the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    //  for this stream)
    return false;
    // SoC utilities only - END
}

void DQMHistogramOTCICtoLpGBTecv::fillEfficiency(uint8_t                pClockStrengthLengthOfOptions,
                                                 uint8_t                pClockPolarity,
                                                 uint8_t                pClockStrengthIndex,
                                                 uint8_t                pCicStrength,
                                                 uint8_t                pPhaseIndex,
                                                 DetectorDataContainer& theEfficiencyContainer)
{
    for(auto board: theEfficiencyContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto theHybrid: *opticalGroup)
            {
                auto efficiencies = theEfficiencyContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<float>>();

                // Select the correct histogram given the pClockPolarity and the pCicStrength
                uint8_t CICStrengthPolarityCombination = (pCicStrength * 10) + pClockPolarity;
                auto    theEfficiencyHistogram         = fEfficiency[CICStrengthPolarityCombination]
                                                  .getObject(board->getId())
                                                  ->getObject(opticalGroup->getId())
                                                  ->getObject(theHybrid->getId())
                                                  ->getSummary<HistContainer<TH2F>>()
                                                  .fTheHistogram;

                // Fill the selected histogram with efficiency content
                uint8_t lineCounter = 0;
                for(auto efficiency: efficiencies)
                {
                    theEfficiencyHistogram->SetBinContent(lineCounter * pClockStrengthLengthOfOptions + pClockStrengthIndex, pPhaseIndex, efficiency);
                    lineCounter++;
                }
            }
        }
    }
}