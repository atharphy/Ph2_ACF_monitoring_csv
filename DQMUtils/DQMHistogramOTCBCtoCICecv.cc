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

    uint8_t numberOfStubs = 6; // Stub 1 to 5 and L1 is filled as Stub0
    uint8_t numberOfCBC = 8;
    uint8_t cbcStrengthCount = 16; //each cbc strength in different histogram 0 to 15 (0 to F)
    uint8_t numberOfPhases = 15; // 0 to 14

    for (uint8_t cbcStrength = 0; cbcStrength < cbcStrengthCount; ++cbcStrength)
    {
        HistContainer<TH2F> phaseScanMatchingEfficiency(
                Form("CBCtoCICecvEfficiency_CbcStrength%d", int(cbcStrength)),
                Form("CBC to CIC ecv Efficiency CbcStrength %d", int(cbcStrength)),
                numberOfPhases,
                -0.5,
                numberOfPhases - 0.5,
                numberOfCBC * numberOfStubs, //y-axis
                0,
                numberOfCBC * numberOfStubs
                                                        );
        phaseScanMatchingEfficiency.fTheHistogram->GetYaxis()->SetTitle("");
        phaseScanMatchingEfficiency.fTheHistogram->GetXaxis()->SetTitle("Phase");

        for(uint8_t cbcId = 1; cbcId <= numberOfCBC; ++cbcId) //itr from 1 to 8
            for (uint8_t stub = 0; stub < numberOfStubs; ++stub) //itr from 0 to 5
            {
                if(stub == 0)
                    phaseScanMatchingEfficiency.fTheHistogram->GetYaxis()->SetBinLabel( 6 * (cbcId - 1) + stub + 1, Form("CBC%d_L1", cbcId));
                else
                    phaseScanMatchingEfficiency.fTheHistogram->GetYaxis()->SetBinLabel( 6 * (cbcId - 1) + stub + 1, Form("CBC%d_Stub%d", cbcId, stub-1));
            }

        phaseScanMatchingEfficiency.fTheHistogram->SetMinimum(0);
        phaseScanMatchingEfficiency.fTheHistogram->SetMaximum(1);
        phaseScanMatchingEfficiency.fTheHistogram->SetStats(false);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fPhaseScanMatchingEfficiencies[cbcStrength], phaseScanMatchingEfficiency);
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
bool DQMHistogramOTCBCtoCICecv::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization theMatchingEfficiencySerialization("OTCBCtoCICecvMatchingEfficiency");

    if(theMatchingEfficiencySerialization.attachDeserializer(inputStream))
    {
        uint8_t               phyPort, cicCurrent, cicPhase, cbcStrength;
        std::map<std::pair<uint8_t, uint8_t>, std::pair<uint8_t, uint8_t>> phyPortAndlineToCbcIdAndStubMap;
        DetectorDataContainer theDetectorData =
            theMatchingEfficiencySerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, 6>>(fDetectorContainer, phyPort, cicCurrent, cicPhase, cbcStrength, phyPortAndlineToCbcIdAndStubMap);
        fillMatchingEfficiency(theDetectorData, phyPort, cicCurrent, cicPhase, cbcStrength, phyPortAndlineToCbcIdAndStubMap);
        return true;
    }
    return false;
    // SoC utilities only - END
}

//========================================================================================================================
void DQMHistogramOTCBCtoCICecv::fillMatchingEfficiency(DetectorDataContainer& matchingEfficiencyContainer, uint8_t phyPort, uint8_t cicCurrent, uint8_t cicPhase, uint8_t cbcStrength, std::map<std::pair<uint8_t, uint8_t>, std::pair<uint8_t, uint8_t>> phyPortAndlineToCbcIdAndStubMap)
{
    for(auto theBoard: matchingEfficiencyContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                if(!theHybrid->hasSummary()) continue;
                auto thePhaseScanHistogram =
                    fPhaseScanMatchingEfficiencies[cbcStrength].getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                auto theEfficiencyArray = theHybrid->getSummary<GenericDataArray<float, 4>>();
                for(uint8_t line = 0; line < 4; ++line)
                {
                    auto cbcId = phyPortAndlineToCbcIdAndStubMap[{phyPort, line}].first;
                    auto stub = phyPortAndlineToCbcIdAndStubMap[{phyPort, line}].second;
                    auto efficiency =  (int)(theEfficiencyArray[line] * 1000.0 + 0.5) / 1000.0;  // round to 3 decimals, helpful when plotting with "COLZ TEXT"
                    thePhaseScanHistogram->SetBinContent( cicPhase + 1, 6 * (cbcId -1) + stub + 1, efficiency);
                    //LOG(INFO) << "Setting X Bin " << cicPhase + 1 << ",  Y Bin " << 6 * (cbcId -1) + stub + 1 << " with Label " << thePhaseScanHistogram->GetYaxis()->GetBinLabel(6 * (cbcId -1) + stub + 1) << ", cbcStrength " << cbcStrength << " with efficiency " << theEfficiencyArray[line] << RESET;
                }
            }
        }
    }
}
