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

    uint8_t numberOfStubs = 6; //including L1 (as Stub6)
                               //using line would have been more accurate, but that would create confusion with phyPort's line
                               //so L1 is known as Stub6 from hereon
    uint8_t numberOfCBC = 8;
    uint8_t cbcStrengthCount = 16;
    for (uint8_t cicSlvsCurrent = 1; cicSlvsCurrent <= 5; cicSlvsCurrent++)
    {
        HistContainer<TH2F> phaseScanMatchingEfficiency(
                Form("CBCtoCICecvEfficiency_CICSLVScurrent%d", int(cicSlvsCurrent)),
                Form("CBC to CIC ecv Efficiency CICSLVScurrent %d", int(cicSlvsCurrent)),
                cbcStrengthCount,
                0.5,
                cbcStrengthCount + 0.5,
                numberOfCBC * numberOfStubs, //y-axis
                0,
                numberOfCBC * numberOfStubs
                                                        );
        phaseScanMatchingEfficiency.fTheHistogram->GetYaxis()->SetTitle("");
        phaseScanMatchingEfficiency.fTheHistogram->GetXaxis()->SetTitle("CBC strength");

        for(uint8_t cbcId = 1; cbcId <= numberOfCBC; ++cbcId) //itr from 1 to 8
            for (uint8_t stub = 1; stub <= numberOfStubs; ++stub) //itr from 1 to 6
            {
                if(stub == 6)
                    phaseScanMatchingEfficiency.fTheHistogram->GetYaxis()->SetBinLabel( 6 * (cbcId - 1) + stub, Form("CBC%d_L1", cbcId));
                else
                    phaseScanMatchingEfficiency.fTheHistogram->GetYaxis()->SetBinLabel( 6 * (cbcId - 1) + stub, Form("CBC%d_Stub%d", cbcId, stub));
            }

        phaseScanMatchingEfficiency.fTheHistogram->SetMinimum(0);
        phaseScanMatchingEfficiency.fTheHistogram->SetMaximum(1);
        phaseScanMatchingEfficiency.fTheHistogram->SetStats(false);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fPhaseScanMatchingEfficiencies[cicSlvsCurrent], phaseScanMatchingEfficiency);
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
                    thePhaseScanHistogram->SetBinContent( cbcStrength + 1, 6 * (cbcId -1) + stub, theEfficiencyArray[line]);
                    //LOG(INFO) << "Setting Y Bin No " << 6 * (cbcId -1) + stub << " with Label " << thePhaseScanHistogram->GetYaxis()->GetBinLabel(6 * (cbcId -1) + stub) << ", cbcStrength" << cbcStrength + 1 << " with efficiency" << theEfficiencyArray[line] << RESET;
                }
            }
        }
    }
}
