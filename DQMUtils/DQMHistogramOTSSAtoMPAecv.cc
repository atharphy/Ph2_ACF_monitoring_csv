#include "DQMUtils/DQMHistogramOTSSAtoMPAecv.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramOTSSAtoMPAecv::DQMHistogramOTSSAtoMPAecv() {}

//========================================================================================================================
DQMHistogramOTSSAtoMPAecv::~DQMHistogramOTSSAtoMPAecv() {}

//========================================================================================================================
void DQMHistogramOTSSAtoMPAecv::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    std::vector<float> listOfMPAslvsCurrents = convertStringToFloatList(findValueInSettings<std::string>(pSettingsMap, "OTSSAtoMPAecv_ListOfSSAslvsCurrents", "1, 4, 7"));

    uint8_t numberOfMPA         = 8;
    uint8_t numberOfLinesPerMPA = 9;

    auto setYaxisBinLable = [numberOfMPA, numberOfLinesPerMPA](TH2F* theHistogram)
    {
        auto theAxis = theHistogram->GetYaxis();
        for(uint8_t mpaId = 0; mpaId < numberOfMPA; ++mpaId)
        {
            for(uint8_t lineId = 0; lineId < numberOfLinesPerMPA; ++lineId)
            {
                std::string binLabel = Form("MPA%d_", mpaId + 8);
                if(lineId == 0)
                    binLabel += "L1";
                else
                    binLabel += Form("Stub%d", lineId - 1);
                theAxis->SetBinLabel(mpaId * numberOfLinesPerMPA + lineId + 1, binLabel.c_str());
            }
        }
    };

    for(auto slvsCurrent: listOfMPAslvsCurrents)
    {
        HistContainer<TH2F> phaseScanMatchingEfficiency(Form("MPAtoCICPhaseScan_SLVScurrent_%d", int(slvsCurrent)),
                                                        Form("MPA to CIC Phase Scan Matching efficiency - SLVScurrent = %d", int(slvsCurrent)),
                                                        8,
                                                        -0.5,
                                                        7.5,
                                                        numberOfMPA * numberOfLinesPerMPA,
                                                        -0.5,
                                                        numberOfMPA * numberOfLinesPerMPA - 0.5);
        phaseScanMatchingEfficiency.fTheHistogram->GetXaxis()->SetTitle("phase");
        setYaxisBinLable(phaseScanMatchingEfficiency.fTheHistogram);
        phaseScanMatchingEfficiency.fTheHistogram->SetMinimum(0);
        phaseScanMatchingEfficiency.fTheHistogram->SetMaximum(1);
        phaseScanMatchingEfficiency.fTheHistogram->SetStats(false);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fPhaseScanMatchingEfficiencies[slvsCurrent], phaseScanMatchingEfficiency);
    }
    
}

//========================================================================================================================
void DQMHistogramOTSSAtoMPAecv::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    
}

//========================================================================================================================
void DQMHistogramOTSSAtoMPAecv::reset(void)
{
    // Clear histograms if needed
}


//========================================================================================================================
void DQMHistogramOTSSAtoMPAecv::fillPatternEfficiencyScan(DetectorDataContainer& thePhaseMatchingEfficiency, uint8_t phase, uint8_t slvsCurrent)
{
    for(auto theBoard: thePhaseMatchingEfficiency)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                if(!theHybrid->hasSummary()) continue;

                auto thePatternMatchingEfficiencyVector = theHybrid->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>();

                TH2F* patternMatchingEfficiencyHistogram = fPhaseScanMatchingEfficiencies[slvsCurrent].getObject(theBoard->getId())
                                                               ->getObject(theOpticalGroup->getId())
                                                               ->getObject(theHybrid->getId())
                                                               ->getSummary<HistContainer<TH2F>>()
                                                               .fTheHistogram;

                for(size_t chipId = 0; chipId < NUMBER_OF_CIC_PORTS; ++chipId) // not using the chipID because I want always to read all phases
                {
                    for(size_t cLineId = 0; cLineId < 9; cLineId++) { patternMatchingEfficiencyHistogram->SetBinContent(chipId + 1, cLineId + 1, thePatternMatchingEfficiencyVector[chipId][cLineId]); }
                }
            }
        }
    }
}

//========================================================================================================================
bool DQMHistogramOTSSAtoMPAecv::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization thePatternMatchinEfficiencyContainerSerialization("OTSSAtoMPAecvPatternMatchingEfficiency");

    if(thePatternMatchinEfficiencyContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTverifyMPASSAdataWord PatternMatchingEfficiency!!!!\n";
        uint8_t phase, slvsCurrent;
        DetectorDataContainer theDetectorData =
            thePatternMatchinEfficiencyContainerSerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>(
                fDetectorContainer, phase, slvsCurrent);
        fillPatternEfficiencyScan(theDetectorData, phase, slvsCurrent);
        return true;
    }

    return false;
    // SoC utilities only - END
}
