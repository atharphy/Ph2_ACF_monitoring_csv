#include "DQMUtils/DQMHistogramOTalignLpGBTinputsForBypass.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH1I.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramOTalignLpGBTinputsForBypass::DQMHistogramOTalignLpGBTinputsForBypass() {}

//========================================================================================================================
DQMHistogramOTalignLpGBTinputsForBypass::~DQMHistogramOTalignLpGBTinputsForBypass() {}

//========================================================================================================================
void DQMHistogramOTalignLpGBTinputsForBypass::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    uint8_t numberOfLines = 4;
    for(uint8_t phyPort = 0; phyPort < 12; ++phyPort)
    {
        HistContainer<TH2F> phaseScanMatchingEfficiency(Form("LpGBTforCICbypassPhaseScan_phyPort%d", phyPort),
                                                        Form("LpGBT for CIC Bypass Phase Scan Matching Efficiency - phyPort %d", phyPort),
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

        HistContainer<TH1I> bestPhase(Form("LpGBTforCICbypassBestPhase_phyPort%d", phyPort), Form("LpGBT for CIC Bypass best phase - phyPort %d", phyPort), numberOfLines, -0.5, numberOfLines - 0.5);
        bestPhase.fTheHistogram->GetXaxis()->SetTitle("line");
        for(uint8_t line = 0; line < numberOfLines; ++line) bestPhase.fTheHistogram->GetXaxis()->SetBinLabel(line + 1, Form("Stub%d", line));
        bestPhase.fTheHistogram->SetStats(false);
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fBestPhase[phyPort], bestPhase);
    }
}

//========================================================================================================================
void DQMHistogramOTalignLpGBTinputsForBypass::fillMatchingEfficiency(DetectorDataContainer& matchingEfficiencyContainer, uint8_t phyPort)
{
    for(auto theBoard: matchingEfficiencyContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                if(!theHybrid->hasSummary()) continue;
                auto thePhaseScanHistogram =
                    fPhaseScanMatchingEfficiencies[phyPort].getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                auto theEfficiencyArray = theHybrid->getSummary<GenericDataArray<float, 4, 15>>();
                for(uint8_t lpgbtPhase = 0; lpgbtPhase < 15; ++lpgbtPhase)
                {
                    for(size_t line = 0; line < 4; ++line) { thePhaseScanHistogram->SetBinContent(lpgbtPhase + 1, line + 1, theEfficiencyArray[line][lpgbtPhase]); }
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTalignLpGBTinputsForBypass::fillBestPhase(DetectorDataContainer& bestPhaseContainer, uint8_t phyPort)
{
    for(auto theBoard: bestPhaseContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                if(!theHybrid->hasSummary()) continue;
                auto theBestPhaseHistogram = fBestPhase[phyPort].getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                auto theBestPhaseArray     = theHybrid->getSummary<GenericDataArray<uint8_t, 4>>();
                for(size_t line = 0; line < 4; ++line) { theBestPhaseHistogram->SetBinContent(line + 1, theBestPhaseArray[line]); }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTalignLpGBTinputsForBypass::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTalignLpGBTinputsForBypass::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTalignLpGBTinputsForBypass::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN

    ContainerSerialization thePhaseScanMatchingEfficiencySerialization("OTalignLpGBTinputsForBypassMatchingEfficiency");
    ContainerSerialization theBestPhaseSerialization("OTalignLpGBTinputsForBypassBestPhase");

    if(thePhaseScanMatchingEfficiencySerialization.attachDeserializer(inputStream))
    {
        uint8_t               phyPort;
        DetectorDataContainer theDetectorData =
            thePhaseScanMatchingEfficiencySerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, 4, 15>>(fDetectorContainer, phyPort);
        fillMatchingEfficiency(theDetectorData, phyPort);
        return true;
    }
    if(theBestPhaseSerialization.attachDeserializer(inputStream))
    {
        uint8_t               phyPort;
        DetectorDataContainer theDetectorData = theBestPhaseSerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<uint8_t, 4>>(fDetectorContainer, phyPort);
        fillBestPhase(theDetectorData, phyPort);
        return true;
    }

    return false;
    // SoC utilities only - END
}
