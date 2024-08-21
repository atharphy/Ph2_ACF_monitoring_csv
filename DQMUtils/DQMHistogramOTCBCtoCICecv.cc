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

    uint8_t numberOfStubs = 6; //including L1 (as Stub6)
                               //using line would have been more accurate, but that would create confusion with phyPort's line
                               //so L1 is known as Stub6 from hereon
    uint8_t numberOfCBC = 8;
    uint8_t cbcStrengthCount = 16;
    for (uint8_t cicSlvsCurrent = 1; cicSlvsCurrent <= 5; cicSlvsCurrent++)
    {
        HistContainer<TH2F> phaseScanMatchingEfficiency(Form("CBCtoCICecvEfficiency_CICSLVScurrent%d", int(cicSlvsCurrent)),
                                                        Form("CBC to CIC ecv Efficiency CICSLVScurrent %d", int(cicSlvsCurrent)),
                                                        numberOfCBC * numberOfStubs, //x-axis
                                                        0,
                                                        numberOfCBC * numberOfStubs,
                                                        cbcStrengthCount,
                                                        0.5,
                                                        cbcStrengthCount + 0.5);
        phaseScanMatchingEfficiency.fTheHistogram->GetXaxis()->SetTitle("");
        phaseScanMatchingEfficiency.fTheHistogram->GetYaxis()->SetTitle("CBC strength");

        for(uint8_t cbcId = 1; cbcId <= numberOfCBC; ++cbcId) //itr from 1 to 8
            for (uint8_t stub = 1; stub <= numberOfStubs; ++stub) //itr from 1 to 6
            {
                if(stub == 6)
                    phaseScanMatchingEfficiency.fTheHistogram->GetXaxis()->SetBinLabel( 6 * (cbcId - 1) + stub, Form("CBC%d_L1", cbcId));
                else
                    phaseScanMatchingEfficiency.fTheHistogram->GetXaxis()->SetBinLabel( 6 * (cbcId - 1) + stub, Form("CBC%d_Stub%d", cbcId, stub));
            }
        phaseScanMatchingEfficiency.fTheHistogram->LabelsOption("v", "X");

        phaseScanMatchingEfficiency.fTheHistogram->SetMinimum(0);
        phaseScanMatchingEfficiency.fTheHistogram->SetMaximum(1);
        phaseScanMatchingEfficiency.fTheHistogram->SetStats(false);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fPhaseScanMatchingEfficiencies[cicSlvsCurrent], phaseScanMatchingEfficiency);
    }

//        HistContainer<TH1I> bestPhase(Form("LpGBTforCICbypassBestPhase_phyPort%d", phyPort), Form("LpGBT for CIC Bypass best phase - phyPort %d", phyPort), numberOfLines, -0.5, numberOfLines - 0.5);
//        bestPhase.fTheHistogram->GetXaxis()->SetTitle("line");
//        for(uint8_t line = 0; line < numberOfLines; ++line) bestPhase.fTheHistogram->GetXaxis()->SetBinLabel(line + 1, Form("Stub%d", line));
//        bestPhase.fTheHistogram->SetStats(false);
//        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fBestPhase[phyPort], bestPhase);


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
void DQMHistogramOTCBCtoCICecv::fillMatchingEfficiency(DetectorDataContainer& matchingEfficiencyContainer, uint8_t phyPort, uint8_t cbcStrength, uint8_t cicSlvsCurrent, std::map<std::pair<uint8_t, uint8_t>, std::pair<uint8_t, uint8_t>> phyPortAndlineToCbcIdAndStubMap)
{
    for(auto theBoard: matchingEfficiencyContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                if(!theHybrid->hasSummary()) continue;
                auto thePhaseScanHistogram =
                    fPhaseScanMatchingEfficiencies[cicSlvsCurrent].getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                auto theEfficiencyArray = theHybrid->getSummary<GenericDataArray<float, 4>>();
                for(uint8_t line = 0; line < 4; ++line)
                {
                    auto cbcId = phyPortAndlineToCbcIdAndStubMap[{phyPort, line}].first;
                    auto stub = phyPortAndlineToCbcIdAndStubMap[{phyPort, line}].second;
                    thePhaseScanHistogram->SetBinContent( 6 * (cbcId -1) + stub, cbcStrength + 1, theEfficiencyArray[line]);
                    //LOG(INFO) << "Setting Bin No " << 6 * (cbcId -1) + stub << " with Label " << thePhaseScanHistogram->GetXaxis()->GetBinLabel(6 * (cbcId -1) + stub) << ", cbcStrength" << cbcStrength + 1 << " with efficiency" << theEfficiencyArray[line] << RESET;
                }
            }
        }
    }
}
