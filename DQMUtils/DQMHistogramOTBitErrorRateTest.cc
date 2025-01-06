#include "DQMUtils/DQMHistogramOTBitErrorRateTest.h"
#include "HWDescription/lpGBT.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH1F.h"
#include "TH1I.h"

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

    HistContainer<TH1F> errorRateHistogram("BERTerrorRate", "BERT error rate", numberOfLines * 2, -0.5, numberOfLines * 2 - 0.5);
    errorRateHistogram.fTheHistogram->GetXaxis()->SetTitle("Line number");
    errorRateHistogram.fTheHistogram->GetYaxis()->SetTitle("Error rate");
    setBitLabel(errorRateHistogram.fTheHistogram);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fBERTerrorRateHistogram, errorRateHistogram);

    HistContainer<TH1F> bitCounterHistogram("BERTtestedBitCounter", "BERT tested bit counter", numberOfLines * 2, -0.5, numberOfLines * 2 - 0.5);
    bitCounterHistogram.fTheHistogram->GetXaxis()->SetTitle("Line number");
    bitCounterHistogram.fTheHistogram->GetYaxis()->SetTitle("Number of tested bits");
    setBitLabel(bitCounterHistogram.fTheHistogram);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fBERTbitCounterHistogram, bitCounterHistogram);

    HistContainer<TH1I> fecCounterHistogram("FECerrorCounter", "FEC error counter", 1, -0.5, 0.5);
    fecCounterHistogram.fTheHistogram->GetXaxis()->SetTitle("FEC");
    fecCounterHistogram.fTheHistogram->GetYaxis()->SetTitle("FEC counter");
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fFECcounterHistogram, fecCounterHistogram);

}

//========================================================================================================================
void DQMHistogramOTBitErrorRateTest::fillErrorCounter(DetectorDataContainer& theErrorCountainer)
{
    for(auto theBoard: theErrorCountainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            auto theErrorCounterHistogram = fBERTerrorRateHistogram.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
            auto theBitCounterHistogram   = fBERTbitCounterHistogram.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
            for(auto theHybrid: *theOpticalGroup)
            {
                auto    theErrorCounterVector = theHybrid->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
                uint8_t numberOfLines         = theErrorCounterVector.size();
                for(uint8_t line = 0; line < numberOfLines; ++line)
                {
                    int binNumber = 1 + line + (theHybrid->getId() % 2) * numberOfLines;
                    theBitCounterHistogram->SetBinContent(binNumber, theErrorCounterVector.at(line).at(0));
                    theErrorCounterHistogram->SetBinContent(binNumber, float(theErrorCounterVector.at(line).at(1)) / float(theErrorCounterVector.at(line).at(0)));
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTBitErrorRateTest::fillFECcounter(DetectorDataContainer& theErrorCountainer)
{
    for(auto theBoard: theErrorCountainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            auto theFECCounterHistogram = fFECcounterHistogram.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId())->getSummary<HistContainer<TH1I>>().fTheHistogram;
            theFECCounterHistogram->SetBinContent(1, theOpticalGroup->getSummary<uint32_t>());
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
    ContainerSerialization theFECcounterSerialization("OTBitErrorRateTestFECcounter");

    if(theErrorCounterSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTBitErrorRateTest ErrorCounter!!!!\n";
        DetectorDataContainer theDetectorData =
            theErrorCounterSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, std::vector<GenericDataArray<uint64_t, 2>>, EmptyContainer>(fDetectorContainer);
        fillErrorCounter(theDetectorData);
        return true;
    }
    if(theFECcounterSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTBitErrorRateTest FECcounter!!!!\n";
        DetectorDataContainer theDetectorData =
            theErrorCounterSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, EmptyContainer, uint32_t>(fDetectorContainer);
        fillFECcounter(theDetectorData);
        return true;
    }

    return false;
    // SoC utilities only - END
}
