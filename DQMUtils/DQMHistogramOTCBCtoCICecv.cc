#include "DQMUtils/DQMHistogramOTCBCtoCICecv.h"
#include "HWDescription/ReadoutChip.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH2F.h"
#include "TH1F.h"

using namespace Ph2_HwDescription;

//========================================================================================================================
DQMHistogramOTCBCtoCICecv::DQMHistogramOTCBCtoCICecv() {}

//========================================================================================================================
DQMHistogramOTCBCtoCICecv::~DQMHistogramOTCBCtoCICecv() {}

//========================================================================================================================
void DQMHistogramOTCBCtoCICecv::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

//    std::vector<float> listOfCBCslvsCurrents = convertStringToFloatList(findValueInSettings<std::string>(pSettingsMap, "OTCBCtoCICecv_ListOfCBCslvsCurrents", "1, 4, 7"));
//
//    uint8_t numberOfCBC         = 8;
//    uint8_t numberOfLinesPerCBC = 6;
//
//    auto setYaxisBinLable = [numberOfCBC, numberOfLinesPerCBC](TH2F* theHistogram)
//    {
//        auto theAxis = theHistogram->GetYaxis();
//        for(uint8_t cbcId = 0; cbcId < numberOfCBC; ++cbcId)
//        {
//            for(uint8_t lineId = 0; lineId < numberOfLinesPerCBC; ++lineId)
//            {
//                std::string binLabel = Form("CBC%d_", cbcId + 8);
//                if(lineId == 0)
//                    binLabel += "L1";
//                else
//                    binLabel += Form("Stub%d", lineId - 1);
//                theAxis->SetBinLabel(cbcId * numberOfLinesPerCBC + lineId + 1, binLabel.c_str());
//            }
//        }
//    };
//
//    for(auto slvsCurrent: listOfCBCslvsCurrents)
//    {
//        HistContainer<TH2F> phaseScanMatchingEfficiency(Form("CBCtoCICPhaseScan_SLVScurrent_%d", int(slvsCurrent)),
//                                                        Form("CBC to CIC Phase Scan Matching efficiency - SLVScurrent = %d", int(slvsCurrent)),
//                                                        15,
//                                                        -0.5,
//                                                        14.5,
//                                                        numberOfCBC * numberOfLinesPerCBC,
//                                                        -0.5,
//                                                        numberOfCBC * numberOfLinesPerCBC - 0.5);
//        phaseScanMatchingEfficiency.fTheHistogram->GetXaxis()->SetTitle("phase");
//        setYaxisBinLable(phaseScanMatchingEfficiency.fTheHistogram);
//        phaseScanMatchingEfficiency.fTheHistogram->SetMinimum(0);
//        phaseScanMatchingEfficiency.fTheHistogram->SetMaximum(1);
//        phaseScanMatchingEfficiency.fTheHistogram->SetStats(false);
//        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fPhaseScanMatchingEfficiencies[slvsCurrent], phaseScanMatchingEfficiency);
//    }

    uint8_t numberOfLines = 4;
    for(uint8_t phyPort = 0; phyPort < 12; ++phyPort)
    {
        HistContainer<TH2F> phaseScanMatchingEfficiency(Form("OTCBCtoCICecv_efficiency_phyPort%d", phyPort),
                                                        Form("CBC to CIC Bypass Phase Scan Matching Efficiency - phyPort %d", phyPort),
                                                        15,
                                                        -0.5,
                                                        14.5,
                                                        numberOfLines,
                                                        -0.5,
                                                        numberOfLines - 0.5);
        phaseScanMatchingEfficiency.fTheHistogram->GetXaxis()->SetTitle("phase");
        for(uint8_t line = 0; line < numberOfLines; ++line) phaseScanMatchingEfficiency.fTheHistogram->GetYaxis()->SetBinLabel(line + 1, Form("Stub%d", line));
        phaseScanMatchingEfficiency.fTheHistogram->SetMinimum(0);
        phaseScanMatchingEfficiency.fTheHistogram->SetMaximum(1);
        phaseScanMatchingEfficiency.fTheHistogram->SetStats(false);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fPhaseScanMatchingEfficiencies[phyPort], phaseScanMatchingEfficiency);

//        HistContainer<TH1I> bestPhase(Form("LpGBTforCICbypassBestPhase_phyPort%d", phyPort), Form("LpGBT for CIC Bypass best phase - phyPort %d", phyPort), numberOfLines, -0.5, numberOfLines - 0.5);
//        bestPhase.fTheHistogram->GetXaxis()->SetTitle("line");
//        for(uint8_t line = 0; line < numberOfLines; ++line) bestPhase.fTheHistogram->GetXaxis()->SetBinLabel(line + 1, Form("Stub%d", line));
//        bestPhase.fTheHistogram->SetStats(false);
//        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fBestPhase[phyPort], bestPhase);
    }


}

//========================================================================================================================
void DQMHistogramOTCBCtoCICecv::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTCBCtoCICecv::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
void DQMHistogramOTCBCtoCICecv::fillPhaseScanMatchingEfficiency(DetectorDataContainer& thePhaseMatchingEfficiency, uint8_t phase, uint8_t slvsCurrent)
{
    for(auto theBoard: thePhaseMatchingEfficiency)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                auto thePhaseMatchingEfficiencyPlot =
                    fPhaseScanMatchingEfficiencies[slvsCurrent].getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;

                for(auto theChip: *theHybrid)
                {
                    if(!theChip->hasSummary()) continue;
                    auto theChipLineMatchingEfficiency = theChip->getSummary<GenericDataArray<float, 6>>();
                    for(int line = 0; line < 6; ++line) { thePhaseMatchingEfficiencyPlot->SetBinContent(phase + 1, theChip->getId() % 8 * 6 + line + 1, theChipLineMatchingEfficiency[line]); }
                }
            }
        }
    }
}

//========================================================================================================================
bool DQMHistogramOTCBCtoCICecv::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization thePhaseScanMatchingEfficiencySerialization("OTCBCtoCICecvPhaseScanMatchingEfficiency");

    if(thePhaseScanMatchingEfficiencySerialization.attachDeserializer(inputStream))
    {
        uint8_t               phase, slvsCurrent;
        DetectorDataContainer theDetectorData =
            thePhaseScanMatchingEfficiencySerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, 6>>(fDetectorContainer, phase, slvsCurrent);
        fillPhaseScanMatchingEfficiency(theDetectorData, phase, slvsCurrent);
        return true;
    }
    return false;
    // SoC utilities only - END
}

//========================================================================================================================
void DQMHistogramOTCBCtoCICecv::fillMatchingEfficiency(DetectorDataContainer& matchingEfficiencyContainer, uint8_t phyPort)
{
    for(auto theBoard: matchingEfficiencyContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                if(!theHybrid->hasSummary()) continue;
                auto thePhaseScanHistogram =
                    fPhaseScanMatchingEfficiencies[phyPort].getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                auto theEfficiencyArray = theHybrid->getSummary<GenericDataArray<float, 4>>();
                for(size_t line = 0; line < 4; ++line) { thePhaseScanHistogram->SetBinContent(line + 1, theEfficiencyArray[line]); }
            }
        }
    }
}
