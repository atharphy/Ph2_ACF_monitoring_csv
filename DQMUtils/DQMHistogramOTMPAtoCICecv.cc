#include "DQMUtils/DQMHistogramOTMPAtoCICecv.h"
#include "HWDescription/ReadoutChip.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH2F.h"

using namespace Ph2_HwDescription;

//========================================================================================================================
DQMHistogramOTMPAtoCICecv::DQMHistogramOTMPAtoCICecv() {}

//========================================================================================================================
DQMHistogramOTMPAtoCICecv::~DQMHistogramOTMPAtoCICecv() {}

//========================================================================================================================
void DQMHistogramOTMPAtoCICecv::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    auto        MPAqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string theMPAqueryFunctionString = "MPAqueryFunction";
    fDetectorContainer->addReadoutChipQueryFunction(MPAqueryFunction, theMPAqueryFunctionString);

    uint8_t numberOfMPA         = 8;
    uint8_t numberOfLinesPerMPA = 6;

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

    for(uint8_t slvsCurrent = 1; slvsCurrent < 8; ++slvsCurrent)
    {
        HistContainer<TH2F> phaseScanMatchingEfficiency(Form("MPAtoCICPhaseScan_SLVScurrent_%d", slvsCurrent),
                                                        Form("MPA to CIC Phase Scan Matching efficiency - SLVScurrent = %d", slvsCurrent),
                                                        15,
                                                        -0.5,
                                                        14.5,
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

    fDetectorContainer->removeReadoutChipQueryFunction(theMPAqueryFunctionString);
}

//========================================================================================================================
void DQMHistogramOTMPAtoCICecv::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTMPAtoCICecv::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
void DQMHistogramOTMPAtoCICecv::fillPhaseScanMatchingEfficiency(DetectorDataContainer& thePhaseMatchingEfficiency, uint8_t phase, uint8_t slvsCurrent)
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
bool DQMHistogramOTMPAtoCICecv::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization thePhaseScanMatchingEfficiencySerialization("OTMPAtoCICecvPhaseScanMatchingEfficiency");

    if(thePhaseScanMatchingEfficiencySerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched OTMPAtoCICecv PhaseScanMatchingEfficiency!!!!!\n";
        uint8_t               phase, slvsCurrent;
        DetectorDataContainer theDetectorData =
            thePhaseScanMatchingEfficiencySerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, GenericDataArray<float, 6>>(fDetectorContainer, phase, slvsCurrent);
        fillPhaseScanMatchingEfficiency(theDetectorData, phase, slvsCurrent);
        return true;
    }
    return false;
    // SoC utilities only - END
}
