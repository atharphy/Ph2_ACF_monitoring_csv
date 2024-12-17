#include "DQMUtils/DQMHistogramOTBitErrorRateTest.h"
#include "HWDescription/lpGBT.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"

#include "TFile.h"
#include "TH1F.h"

//========================================================================================================================
DQMHistogramOTBitErrorRateTest::DQMHistogramOTBitErrorRateTest() {}

//========================================================================================================================
DQMHistogramOTBitErrorRateTest::~DQMHistogramOTBitErrorRateTest() {}

//========================================================================================================================
void DQMHistogramOTBitErrorRateTest::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    uint32_t theAcquisitionDuration = findValueInSettings<double>(pSettingsMap, "OTBitErrorRateTest_AcquisitionDuration", 32);

    size_t numberOfLines = (theDetectorStructure.getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;

    auto setBitLabel = [numberOfLines](TH1F* theHistogram)
    {
        std::vector<std::string> hybridSide{"FEHR", "FEHL"};
        for(uint8_t hybridId = 0; hybridId < hybridSide.size(); ++hybridId)
        {
            theHistogram->GetXaxis()->SetBinLabel(1 + hybridId * numberOfLines, (std::string("L1_") + hybridSide[hybridId]).c_str());
            for(size_t stubLine = 0; stubLine < numberOfLines - 1; ++stubLine)
                theHistogram->GetXaxis()->SetBinLabel(stubLine + 2 + hybridId * numberOfLines, (std::string(Form("Stub%d_", int(stubLine))) + hybridSide[hybridId]).c_str());
        }
    };

    HistContainer<TH1F> errorCounterHistogram("BERTerrorCounter", Form("BERT error counter - acquisition duration %d s", theAcquisitionDuration), numberOfLines * 2, -0.5, numberOfLines * 2 - 0.5);
    errorCounterHistogram.fTheHistogram->GetXaxis()->SetTitle("Line number");
    errorCounterHistogram.fTheHistogram->GetYaxis()->SetTitle("NumberOfErrors");
    setBitLabel(errorCounterHistogram.fTheHistogram);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fBERTerrorCounterHistogram, errorCounterHistogram);
}

//========================================================================================================================
void DQMHistogramOTBitErrorRateTest::fillErrorCounter(DetectorDataContainer& theErrorCountainer)
{
    for(auto theBoard: theErrorCountainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            auto theErrorCounterHistogram = fBERTerrorCounterHistogram.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
            for(auto theHybrid: *theOpticalGroup)
            {
                auto    theErrorCounterVector = theHybrid->getSummary<std::vector<uint32_t>>();
                uint8_t numberOfLines         = theErrorCounterVector.size();
                for(uint8_t line = 0; line < numberOfLines; ++line) { theErrorCounterHistogram->SetBinContent(1 + line + (theHybrid->getId() % 2) * numberOfLines, theErrorCounterVector.at(line)); }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTBitErrorRateTest::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTBitErrorRateTest::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTBitErrorRateTest::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization theErrorCounterSerialization("OTBitErrorRateTestErrorCounter");

    if(theErrorCounterSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTBitErrorRateTest ErrorCounter!!!!\n";
        DetectorDataContainer theDetectorData =
            theErrorCounterSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, std::vector<uint32_t>, EmptyContainer>(fDetectorContainer);
        fillErrorCounter(theDetectorData);
        return true;
    }

    return false;
    // SoC utilities only - END
}
