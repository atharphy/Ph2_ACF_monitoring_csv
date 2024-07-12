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
    std::vector<float> listOfCICSignalStrengthAndCICClockPolarity = convertStringToFloatList(findValueInSettings<std::string>(pSettingsMap, "OTCICtoLpGBTecv_ListOfCICSignalStrengthsCICClockPolarities", "10, 20, 30, 40, 50, 11, 21, 31, 41, 51"));

    // x-axis is line:clock strenghts and y-axis is LpGBT phase
    int polarity = 0;
    int cicSignalStrength = 1;
    for (auto CICStrengthCombination: listOfCICSignalStrengthAndCICClockPolarity) {
        // Declare histogram axes titles and number of bins
        HistContainer<TH2F> ECVEfficiencyHistogram(Form("Efficiency_CIC_Clock_Polarity_%d-CIC_Signal_Strength_%d", polarity, cicSignalStrength),
                                                   Form("Polarity %d CIC Strength %d", polarity, cicSignalStrength),
                                                   49, 0, 49, 15, 0, 15);

        ECVEfficiencyHistogram.fTheHistogram->GetXaxis()->SetTitle("Line : Clock strenghts");
        ECVEfficiencyHistogram.fTheHistogram->GetYaxis()->SetTitle("lpGBT Phase");
        ECVEfficiencyHistogram.fTheHistogram->SetStats(false);
        ECVEfficiencyHistogram.fTheHistogram->GetYaxis()->SetLabelSize(0.04);
        ECVEfficiencyHistogram.fTheHistogram->GetXaxis()->SetLabelSize(0.02);

        // Label the y axis with the lpGBT phase
        int binNumber = 1;
        for(uint32_t phase = 0; phase < 15; phase++) {
            std::string s = convertToString(phase);
            ECVEfficiencyHistogram.fTheHistogram->GetYaxis()->SetBinLabel(binNumber, s.c_str());
            binNumber++;
        }

        // Label the x axis with the line and clock strength
        binNumber = 1;
        for(uint32_t channel = 1; channel <= 7; channel++) {
            for(uint32_t hybridClockStrength = 1; hybridClockStrength <= 7; hybridClockStrength++) {
                std::string prefix = channel == 1 ? "L1" : "Stub" + convertToString(channel - 1);
                std::string s      = prefix + ":" + convertToString(hybridClockStrength);
                ECVEfficiencyHistogram.fTheHistogram->GetXaxis()->SetBinLabel(binNumber, s.c_str());
                binNumber++;
            }
        }

        // Book the histograms
        ECVEfficiencyHistogram.fTheHistogram->LabelsOption("v", "X");
        ECVEfficiencyHistogram.fTheHistogram->DrawCopy("text");
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fEfficiency[CICStrengthCombination], ECVEfficiencyHistogram);

        // Iterate to the next cicSignalStrength/Polarity
        cicSignalStrength++;
        if (cicSignalStrength > 5){
            polarity = 1;
            cicSignalStrength = 1;
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

void DQMHistogramOTCICtoLpGBTecv::fillEfficiency(uint8_t pClockPolarity, uint8_t pClockStrength, uint8_t pCicStrength, uint8_t pPhase, DetectorDataContainer& theEfficiencyContainer)
{
    for(auto board: theEfficiencyContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto theHybrid: *opticalGroup)
            {
                auto  efficiencies = theEfficiencyContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<float>>();

                // Select the correct histogram given the pClockPolarity and the pCicStrength
                uint8_t CICStrengthCombination = (pCicStrength * 10) + pClockPolarity;
                auto theEfficiencyHistogram = fEfficiency[CICStrengthCombination].getObject(board->getId())->getObject(opticalGroup->getId())->getObject(theHybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;

                // Fill the selected histogram with efficiency content
                uint8_t lineCounter = 0;
                for(auto efficiency: efficiencies)
                {
                    theEfficiencyHistogram->SetBinContent(lineCounter * 7 + pClockStrength, pPhase + 1, efficiency);
                    lineCounter++;
                }
            }
        }
    }
}