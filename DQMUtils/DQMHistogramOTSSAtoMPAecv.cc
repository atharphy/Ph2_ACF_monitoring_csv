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

    uint8_t numberOfMPA             = 8;
    uint8_t numberOfStubLinesPerMPA = 8;

    auto setYaxisBinLabelForStubs = [numberOfMPA, numberOfStubLinesPerMPA](TH2F* theHistogram)
    {
        auto theAxis = theHistogram->GetYaxis();
        for(uint8_t mpaId = 0; mpaId < numberOfMPA; ++mpaId)
        {
            for(uint8_t lineId = 0; lineId < numberOfStubLinesPerMPA; ++lineId) { theAxis->SetBinLabel(mpaId * numberOfStubLinesPerMPA + lineId + 1, Form("MPA%d_Stub%d", mpaId + 8, lineId)); }
        }
    };

    auto setYaxisBinLabelForL1 = [numberOfMPA](TH2F* theHistogram)
    {
        auto theAxis = theHistogram->GetYaxis();
        for(uint8_t mpaId = 0; mpaId < numberOfMPA; ++mpaId) { theAxis->SetBinLabel(mpaId + 1, Form("MPA%d_L1", mpaId + 8)); }
    };

    auto setXaxisBinLabelForL1 = [numberOfMPA, this](TH2F* theHistogram)
    {
        std::vector<std::string> edgeLabel           = {"falling", "rising"};
        int                      totalNumberOfShifts = fMaximum320PhaseShift - fMinimum320PhaseShift + 1;
        auto                     theAxis             = theHistogram->GetXaxis();
        for(size_t labelIndex = 0; labelIndex < edgeLabel.size(); ++labelIndex)
        {
            for(int shift = fMinimum320PhaseShift; shift <= fMaximum320PhaseShift; ++shift)
            {
                theAxis->SetBinLabel(shift + totalNumberOfShifts * labelIndex - fMinimum320PhaseShift + 1, Form("%s : %s%d", edgeLabel[labelIndex].c_str(), (shift > 0 ? "+" : ""), shift));
            }
        }
    };
    int totalNumberOfShifts = fMaximum320PhaseShift - fMinimum320PhaseShift + 1;

    for(auto slvsCurrent: listOfMPAslvsCurrents)
    {
        HistContainer<TH2F> phaseScanStubMatchingEfficiency(Form("SSAtoMPAStubPhaseScan_SLVScurrent_%d", int(slvsCurrent)),
                                                            Form("SSA to MPA Stub Phase Scan Matching efficiency - SLVScurrent = %d", int(slvsCurrent)),
                                                            2,
                                                            -0.5,
                                                            1.5,
                                                            numberOfMPA * numberOfStubLinesPerMPA,
                                                            -0.5,
                                                            numberOfMPA * numberOfStubLinesPerMPA - 0.5);
        phaseScanStubMatchingEfficiency.fTheHistogram->GetXaxis()->SetTitle("sampling egde");
        phaseScanStubMatchingEfficiency.fTheHistogram->GetXaxis()->SetBinLabel(1, "falling");
        phaseScanStubMatchingEfficiency.fTheHistogram->GetXaxis()->SetBinLabel(2, "rising");
        setYaxisBinLabelForStubs(phaseScanStubMatchingEfficiency.fTheHistogram);
        phaseScanStubMatchingEfficiency.fTheHistogram->SetMinimum(0);
        phaseScanStubMatchingEfficiency.fTheHistogram->SetMaximum(1);
        phaseScanStubMatchingEfficiency.fTheHistogram->SetStats(false);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStubPhaseScanMatchingEfficiencies[slvsCurrent], phaseScanStubMatchingEfficiency);

        HistContainer<TH2F> phaseScanL1MatchingEfficiency(Form("SSAtoMPAL1PhaseScan_SLVScurrent_%d", int(slvsCurrent)),
                                                          Form("SSA to MPA L1 Phase Scan Matching efficiency - SLVScurrent = %d", int(slvsCurrent)),
                                                          2 * totalNumberOfShifts,
                                                          -0.5,
                                                          2 * totalNumberOfShifts - 1,
                                                          numberOfMPA,
                                                          -0.5,
                                                          numberOfMPA - 0.5);
        phaseScanL1MatchingEfficiency.fTheHistogram->GetXaxis()->SetTitle("sampling egde : 320MHz clock shift");
        setXaxisBinLabelForL1(phaseScanL1MatchingEfficiency.fTheHistogram);
        setYaxisBinLabelForL1(phaseScanL1MatchingEfficiency.fTheHistogram);
        phaseScanL1MatchingEfficiency.fTheHistogram->SetMinimum(0);
        phaseScanL1MatchingEfficiency.fTheHistogram->SetMaximum(1);
        phaseScanL1MatchingEfficiency.fTheHistogram->SetStats(false);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fL1PhaseScanMatchingEfficiencies[slvsCurrent], phaseScanL1MatchingEfficiency);
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
void DQMHistogramOTSSAtoMPAecv::fillStubPatternEfficiencyScan(DetectorDataContainer& thePhaseMatchingEfficiency, uint8_t clockEdge, uint8_t slvsCurrent)
{
    for(auto theBoard: thePhaseMatchingEfficiency)
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
                for(size_t chipId = 0; chipId < NUMBER_OF_CIC_PORTS; ++chipId) // not using the chipID because I want always to read all phases
                {
                    for(size_t line = 1; line < 9; line++) { patternMatchingEfficiencyHistogram->SetBinContent(clockEdge + 1, chipId * 8 + line, thePatternMatchingEfficiencyVector[chipId][line]); }
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTSSAtoMPAecv::fillL1PatternEfficiencyScan(DetectorDataContainer& thePhaseMatchingEfficiency, uint8_t clockEdge, uint8_t slvsCurrent, int samplingPhaseOffset)
{
    int totalNumberOfShifts = fMaximum320PhaseShift - fMinimum320PhaseShift + 1;

    for(auto theBoard: thePhaseMatchingEfficiency)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                if(!theHybrid->hasSummary()) continue;

                auto thePatternMatchingEfficiencyVector = theHybrid->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>();

                TH2F* patternMatchingEfficiencyHistogram = fL1PhaseScanMatchingEfficiencies[slvsCurrent]
                                                               .getObject(theBoard->getId())
                                                               ->getObject(theOpticalGroup->getId())
                                                               ->getObject(theHybrid->getId())
                                                               ->getSummary<HistContainer<TH2F>>()
                                                               .fTheHistogram;
                for(size_t chipId = 0; chipId < NUMBER_OF_CIC_PORTS; ++chipId) // not using the chipID because I want always to read all phases
                {
                    patternMatchingEfficiencyHistogram->SetBinContent(
                        clockEdge * totalNumberOfShifts + samplingPhaseOffset - fMinimum320PhaseShift + 1, chipId + 1, thePatternMatchingEfficiencyVector[chipId][0]);
                }
            }
        }
    }
}

//========================================================================================================================
bool DQMHistogramOTSSAtoMPAecv::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization theStubPatternMatchinEfficiencyContainerSerialization("OTSSAtoMPAecvStubPatternMatchingEfficiency");
    ContainerSerialization theL1PatternMatchinEfficiencyContainerSerialization("OTSSAtoMPAecvL1PatternMatchingEfficiency");

    if(theStubPatternMatchinEfficiencyContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTverifyMPASSAdataWord PatternMatchingEfficiency!!!!\n";
        uint8_t               clockEdge, slvsCurrent;
        DetectorDataContainer theDetectorData =
            theStubPatternMatchinEfficiencyContainerSerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>(
                fDetectorContainer, clockEdge, slvsCurrent);
        fillStubPatternEfficiencyScan(theDetectorData, clockEdge, slvsCurrent);
        return true;
    }
    if(theL1PatternMatchinEfficiencyContainerSerialization.attachDeserializer(inputStream))
    {
        // std::cout << "Matched OTverifyMPASSAdataWord PatternMatchingEfficiency!!!!\n";
        uint8_t               clockEdge, slvsCurrent;
        int                   samplingPhaseOffset;
        DetectorDataContainer theDetectorData =
            theL1PatternMatchinEfficiencyContainerSerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>(
                fDetectorContainer, clockEdge, slvsCurrent, samplingPhaseOffset);
        fillL1PatternEfficiencyScan(theDetectorData, clockEdge, slvsCurrent, samplingPhaseOffset);
        return true;
    }

    return false;
    // SoC utilities only - END
}
