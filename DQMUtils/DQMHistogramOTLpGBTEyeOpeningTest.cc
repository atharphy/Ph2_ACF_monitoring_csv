#include "DQMUtils/DQMHistogramOTLpGBTEyeOpeningTest.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH2I.h"

//========================================================================================================================
DQMHistogramOTLpGBTEyeOpeningTest::DQMHistogramOTLpGBTEyeOpeningTest() {}

//========================================================================================================================
DQMHistogramOTLpGBTEyeOpeningTest::~DQMHistogramOTLpGBTEyeOpeningTest() {}

//========================================================================================================================
void DQMHistogramOTLpGBTEyeOpeningTest::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    float numberOfTimePoints = 64;
    float numberOfVoltagePoints = 31;
    float timeStepSize = 6.1;
    float voltageStepSize = 40.;
    
    HistContainer<TH2I> theEyeOpeningHistogram("EyeOpeningScan", "Eye Opening Scan", numberOfTimePoints, -(timeStepSize/2), (numberOfTimePoints-0.5)*timeStepSize, numberOfVoltagePoints, -(voltageStepSize/2), (numberOfVoltagePoints-0.5)*voltageStepSize);
    theEyeOpeningHistogram.fTheHistogram->GetXaxis()->SetTitle("time [ps]");
    theEyeOpeningHistogram.fTheHistogram->GetYaxis()->SetTitle("voltage [mV]");
    theEyeOpeningHistogram.fTheHistogram->SetStats(false);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fEyeOpeningHystogramContainer, theEyeOpeningHistogram);
}

//========================================================================================================================
void DQMHistogramOTLpGBTEyeOpeningTest::fillEyeOpening(DetectorDataContainer& theEyeOpeningContainer)
{
    for(auto theBoard: theEyeOpeningContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            if(!theOpticalGroup->hasSummary()) continue;
            auto theEyeArray = theOpticalGroup->getSummary<GenericDataArray<uint16_t, 64, 31>>();
            TH2I* theEyeOpeningHistogram = fEyeOpeningHystogramContainer.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId())->getSummary<HistContainer<TH2I>>().fTheHistogram;
            for(uint8_t cVoltageStep = 0; cVoltageStep < 31; cVoltageStep++)
            {
                for(uint8_t cTimeStep = 0; cTimeStep < 64; cTimeStep++)
                {
                    theEyeOpeningHistogram->SetBinContent(cTimeStep+1, cVoltageStep+1, theEyeArray[cTimeStep][cVoltageStep]);
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTLpGBTEyeOpeningTest::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    
}

//========================================================================================================================
void DQMHistogramOTLpGBTEyeOpeningTest::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTLpGBTEyeOpeningTest::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    ContainerSerialization theLightYieldSerialization("OTLpGBTEyeOpeningTestEyeOpening");

    if(theLightYieldSerialization.attachDeserializer(inputStream))
    {
        DetectorDataContainer theDetectorData =
            theLightYieldSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray<uint16_t, 64, 31>>(fDetectorContainer);
        fillEyeOpening(theDetectorData);
        return true;
    }

    return false;
    // SoC utilities only - END
}
