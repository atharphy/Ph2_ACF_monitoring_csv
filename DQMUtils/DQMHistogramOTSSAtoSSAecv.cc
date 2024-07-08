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
            std::pair<int, int> mpaRange = getMPArange(isLeftToRight);
            for(int mpaId = mpaRange.first; mpaId < mpaRange.second; ++mpaId)
            {
                int ssaId = mpaId + (isLeftToRight ? +1 : -1);
                theAxis->SetBinLabel(direction * (mpaRange.second - mpaRange.first) + (mpaId - mpaRange.first)  + 1, Form("SSA%d#rightarrowSSA%d", ssaId, mpaId)); 
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
                int direction = isLeftToRight ? 0 : 1;
                std::pair<int, int> mpaRange = getMPArange(isLeftToRight);

                for(int mpaId = mpaRange.first; mpaId < mpaRange.second; ++mpaId) 
                {
                    patternMatchingEfficiencyHistogram->SetBinContent(clockEdge + 1, direction * (mpaRange.second - mpaRange.first) + (mpaId - mpaRange.first)  + 1, thePatternMatchingEfficiencyVector[mpaId][1]);
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
std::pair<int, int> DQMHistogramOTSSAtoSSAecv::getMPArange(bool isLeftToRight) const
{
    std::pair<int, int> mpaRange;
    if(isLeftToRight)
    {
        mpaRange.first = 0;
        mpaRange.second = 7;
    }
    else
    {
        mpaRange.first = 1;
        mpaRange.second = 8;
    }
    return mpaRange;
}

