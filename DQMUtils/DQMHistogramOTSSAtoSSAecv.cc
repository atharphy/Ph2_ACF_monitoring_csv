#include "DQMUtils/DQMHistogramOTSSAtoSSAecv.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramOTSSAtoSSAecv::DQMHistogramOTSSAtoSSAecv() {}

//========================================================================================================================
DQMHistogramOTSSAtoSSAecv::~DQMHistogramOTSSAtoSSAecv() {}

//========================================================================================================================
void DQMHistogramOTSSAtoSSAecv::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END
    int numberOfSSA = 8;
    int numberOfDirections = 2;

    auto setYaxisBinLabelForStubs = [this, numberOfSSA, numberOfDirections](TH2F* theHistogram)
    {
        auto theAxis = theHistogram->GetYaxis();
        for(int direction = 0; direction < numberOfDirections; ++direction)
        {
            bool isLeftToRight = direction == 0;
            std::pair<int, int> ssaRange = getSSArange(isLeftToRight);
            for(int ssaId = ssaRange.first; ssaId < ssaRange.second; ++ssaId)
            {
                theAxis->SetBinLabel(direction * (ssaRange.second - ssaRange.first) + (ssaId - ssaRange.first)  + 1, Form("SSA%d#rightarrowSSA%d", ssaId + (isLeftToRight ? -1 : +1), ssaId)); 
            }
        }
    };

    std::vector<float> listOfSSAslvsCurrents = convertStringToFloatList(findValueInSettings<std::string>(pSettingsMap, "OTSSAtoSSAecv_ListOfSSAslvsCurrents", "1, 4, 7"));

    for(auto slvsCurrent: listOfSSAslvsCurrents)
    {
        HistContainer<TH2F> phaseScanStubMatchingEfficiency(Form("SSAtoMPAStubPhaseScan_SLVScurrent_%d", int(slvsCurrent)),
                                                            Form("SSA to MPA Stub Phase Scan Matching efficiency - SLVScurrent = %d", int(slvsCurrent)),
                                                            2,
                                                            -0.5,
                                                            1.5,
                                                            (numberOfSSA - 1) * numberOfDirections,
                                                            -0.5,
                                                            (numberOfSSA - 1) * numberOfDirections - 0.5);
        phaseScanStubMatchingEfficiency.fTheHistogram->GetXaxis()->SetTitle("sampling egde");
        phaseScanStubMatchingEfficiency.fTheHistogram->GetXaxis()->SetBinLabel(1, "falling");
        phaseScanStubMatchingEfficiency.fTheHistogram->GetXaxis()->SetBinLabel(2, "rising");
        setYaxisBinLabelForStubs(phaseScanStubMatchingEfficiency.fTheHistogram);
        phaseScanStubMatchingEfficiency.fTheHistogram->SetMinimum(0);
        phaseScanStubMatchingEfficiency.fTheHistogram->SetMaximum(1);
        phaseScanStubMatchingEfficiency.fTheHistogram->SetStats(false);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStubPhaseScanMatchingEfficiencies[slvsCurrent], phaseScanStubMatchingEfficiency);
    }
}

//========================================================================================================================
void DQMHistogramOTSSAtoSSAecv::fillStubPatternEfficiencyScan(DetectorDataContainer& thePatternMatchingEfficiency, uint8_t injectedStrip, uint8_t clockEdge, uint8_t slvsCurrent)
{
    for(auto theBoard: thePatternMatchingEfficiency)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                if(!theHybrid->hasSummary()) continue;

                auto thePatternMatchingEfficiencyVector = theHybrid->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>();

                TH2F* patternMatchingEfficiencyHistogram = fStubPhaseScanMatchingEfficiencies[slvsCurrent]
                                                               .getObject(theBoard->getId())
                                                               ->getObject(theOpticalGroup->getId())
                                                               ->getObject(theHybrid->getId())
                                                               ->getSummary<HistContainer<TH2F>>()
                                                               .fTheHistogram;
                bool isLeftToRight = injectedStrip == 1;
                std::pair<int, int> ssaRange = getSSArange(isLeftToRight);

                for(int chipId = ssaRange.first; chipId < ssaRange.second; ++chipId) // not using the chipID because I want always to read all phases
                {
                    // attention!!! this is the MPA address
                    patternMatchingEfficiencyHistogram->SetBinContent(clockEdge + 1, chipId - ssaRange.second + (isLeftToRight ? 0 : (ssaRange.second - ssaRange.first)) + 1, thePatternMatchingEfficiencyVector[chipId][1]);
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTSSAtoSSAecv::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTSSAtoSSAecv::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTSSAtoSSAecv::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    ContainerSerialization theStubPatternMatchinEfficiencyContainerSerialization("OTSSAtoSSAecvStubPatternMatchingEfficiency");

    if(theStubPatternMatchinEfficiencyContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTSSAtoSSAecv StubPatternMatchingEfficiency!!!!\n";
        uint8_t               injectedStrip, clockEdge, slvsCurrent;
        DetectorDataContainer theDetectorData =
            theStubPatternMatchinEfficiencyContainerSerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>(
                fDetectorContainer, injectedStrip, clockEdge, slvsCurrent);
        fillStubPatternEfficiencyScan(theDetectorData, injectedStrip, clockEdge, slvsCurrent);
        return true;
    }

    return false;
    // SoC utilities only - END
}

//========================================================================================================================
std::pair<int, int> DQMHistogramOTSSAtoSSAecv::getSSArange(bool isLeftToRight) const
{
    std::pair<int, int> ssaRange;
    if(isLeftToRight)
    {
        ssaRange.first = 0;
        ssaRange.second = 7;
    }
    else
    {
        ssaRange.first = 1;
        ssaRange.second = 8;
    }
    return ssaRange;
}

