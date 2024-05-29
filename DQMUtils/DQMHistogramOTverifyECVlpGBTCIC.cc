#include "DQMUtils/DQMHistogramOTverifyECVlpGBTCIC.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"

#include "TFile.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramOTverifyECVlpGBTCIC::DQMHistogramOTverifyECVlpGBTCIC() {}

//========================================================================================================================
DQMHistogramOTverifyECVlpGBTCIC::~DQMHistogramOTverifyECVlpGBTCIC() {}

//========================================================================================================================
void DQMHistogramOTverifyECVlpGBTCIC::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;

    // x-axis is line:clock strenghts and y-axis is LpGBT phase:CIC strenght
    HistContainer<TH2F> ECVEfficiencyPolarity0Histogram("Efficiency_CIC_Clock_Polarity_0", "Polarity 0", 49, 0, 49, 75, 0, 75);
    auto                EfficiencyPolarity0Hist = ECVEfficiencyPolarity0Histogram.fTheHistogram;
    EfficiencyPolarity0Hist->GetXaxis()->SetTitle("Line : Clock strenghts");
    EfficiencyPolarity0Hist->GetYaxis()->SetTitle("lpGBT Phase : CIC Strength");
    EfficiencyPolarity0Hist->SetStats(false);
    EfficiencyPolarity0Hist->GetYaxis()->SetLabelSize(0.02);
    EfficiencyPolarity0Hist->GetXaxis()->SetLabelSize(0.02);

    HistContainer<TH2F> ECVEfficiencyPolarity1Histogram("Efficiency_CIC_Clock_Polarity_1", "Polarity 1", 49, 0, 49, 75, 0, 75);
    auto                EfficiencyPolarity1Hist = ECVEfficiencyPolarity1Histogram.fTheHistogram;
    EfficiencyPolarity1Hist->GetXaxis()->SetTitle("Line : Clock strenghts");
    EfficiencyPolarity1Hist->GetYaxis()->SetTitle("lpGBT Phase : CIC Strength");
    EfficiencyPolarity1Hist->SetStats(false);
    EfficiencyPolarity1Hist->GetYaxis()->SetLabelSize(0.02);
    EfficiencyPolarity1Hist->GetXaxis()->SetLabelSize(0.02);

    int binNumber = 1;
    for(uint32_t phase = 0; phase < 15; phase++)
    {
        for(uint32_t cicSignalStrength = 1; cicSignalStrength <= 5; cicSignalStrength++)
        {
            std::string s = convertToString(phase) + ":" + convertToString(cicSignalStrength);
            EfficiencyPolarity0Hist->GetYaxis()->SetBinLabel(binNumber, s.c_str());
            EfficiencyPolarity1Hist->GetYaxis()->SetBinLabel(binNumber, s.c_str());
            binNumber++;
        }
    }
    binNumber = 1;

    for(uint32_t channel = 1; channel <= 7; channel++)
    {
        for(uint32_t hybridClockStrength = 1; hybridClockStrength <= 7; hybridClockStrength++)
        {
            std::string prefix = channel == 1 ? "L1" : "Stub" + convertToString(channel - 1);
            std::string s      = prefix + ":" + convertToString(hybridClockStrength);
            EfficiencyPolarity0Hist->GetXaxis()->SetBinLabel(binNumber, s.c_str());
            EfficiencyPolarity1Hist->GetXaxis()->SetBinLabel(binNumber, s.c_str());
            binNumber++;
        }
    }
    EfficiencyPolarity0Hist->LabelsOption("v", "X");
    EfficiencyPolarity0Hist->DrawCopy("text");
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fEfficiencyPolarity0, ECVEfficiencyPolarity0Histogram);

    EfficiencyPolarity1Hist->LabelsOption("v", "X");
    EfficiencyPolarity1Hist->DrawCopy("text");
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fEfficiencyPolarity1, ECVEfficiencyPolarity1Histogram);
}

//========================================================================================================================
void DQMHistogramOTverifyECVlpGBTCIC::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTverifyECVlpGBTCIC::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTverifyECVlpGBTCIC::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    // As example, I'm expecting to receive a data stream from an uint32_t contained from calibration "OTverifyECVlpGBTCIC"
    ContainerSerialization theECVlpGBTCICContainerSerialization("OTverifyECVlpGBTCICEfficiencyHistogram");

    if(theECVlpGBTCICContainerSerialization.attachDeserializer(inputStream))
    {
        // It matched! Decoding data
        std::cout << "Matched OTverifyECVlpGBTCIC!!!!!\n";
        // Need to tell to the streamer what data are contained (in this case in every channel there is an object of type MyType)
        uint8_t               pClockPolarity, pClockStrength, pCicStrength, pPhase;
        DetectorDataContainer theDetectorData = theECVlpGBTCICContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, float, EmptyContainer>(
            fDetectorContainer, pClockPolarity, pClockStrength, pCicStrength, pPhase);

        // Filling the histograms
        fillEfficiency(pClockPolarity, pClockStrength, pCicStrength, pPhase, theDetectorData);
        return true;
    }


    // the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    //  for this stream)
    return false;
    // SoC utilities only - END
}

void DQMHistogramOTverifyECVlpGBTCIC::fillEfficiency(uint8_t pClockPolarity, uint8_t pClockStrength, uint8_t pCicStrength, uint8_t pPhase, DetectorDataContainer& theEfficiencyContainer)
{
    for(auto board: theEfficiencyContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto theHybrid: *opticalGroup)
            {
                auto  efficiencies = theEfficiencyContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<float>>();
                TH2F* theEfficiencyHistogram;
                if(pClockPolarity == 0)
                    theEfficiencyHistogram =
                        fEfficiencyPolarity0.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(theHybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                else
                    theEfficiencyHistogram =
                        fEfficiencyPolarity1.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(theHybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                uint8_t lineCounter = 0;
                for(auto efficiency: efficiencies)
                {
                    theEfficiencyHistogram->SetBinContent(lineCounter * 7 + pClockStrength, pPhase * 5 + pCicStrength, efficiency);
                    lineCounter++;
                }
            }
        }
    }
}