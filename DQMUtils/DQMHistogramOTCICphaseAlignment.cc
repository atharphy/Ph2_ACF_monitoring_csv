#include "DQMUtils/DQMHistogramOTCICphaseAlignment.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "HWDescription/Definition.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH2I.h"

//========================================================================================================================
DQMHistogramOTCICphaseAlignment::DQMHistogramOTCICphaseAlignment() {}

//========================================================================================================================
DQMHistogramOTCICphaseAlignment::~DQMHistogramOTCICphaseAlignment() {}

//========================================================================================================================
void DQMHistogramOTCICphaseAlignment::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    std::string xAxisTitle = "CBC Id";
    int idOffset = 0;
    if(theDetectorStructure.getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS)
    {
        xAxisTitle = "MPA Id";
        idOffset = 8;
    }

    auto setBinLabels = [](TAxis* theHistogramAxis) {

        theHistogramAxis->SetBinLabel(1, "L1");
        for(int port=1; port<NUMBER_OF_LINES_PER_CIC_PORTS; ++port)
        {
            theHistogramAxis->SetBinLabel(port+1, Form("Stub%d",port-1));
        }
    };

    HistContainer<TH2I> bestPhaseHistogram("BestCICinputPhases", "Best CIC Input Phases", NUMBER_OF_CIC_PORTS, idOffset - 0.5, idOffset + NUMBER_OF_CIC_PORTS - 0.5, NUMBER_OF_LINES_PER_CIC_PORTS, -0.5, NUMBER_OF_LINES_PER_CIC_PORTS - 0.5);
    bestPhaseHistogram.fTheHistogram->GetXaxis()->SetTitle(xAxisTitle.c_str());
    bestPhaseHistogram.fTheHistogram->GetYaxis()->SetTitle("Line");
    setBinLabels(bestPhaseHistogram.fTheHistogram->GetYaxis());
    bestPhaseHistogram.fTheHistogram->SetMinimum(0);
    bestPhaseHistogram.fTheHistogram->SetMaximum(15);
    bestPhaseHistogram.fTheHistogram->SetStats(false);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fBestPhaseHistogramContainer, bestPhaseHistogram);
}


//========================================================================================================================
void DQMHistogramOTCICphaseAlignment::fillBestPhasePhaseResults(DetectorDataContainer& thePhaseAlignmentResultContainer)
{
    for(auto board: thePhaseAlignmentResultContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;

                auto theBestPhaseVector = hybrid->getSummary<GenericDataArray_2D<NUMBER_OF_CIC_PORTS, NUMBER_OF_LINES_PER_CIC_PORTS, int>>();

                TH2I* hybridAlignmentSuccessHistogram   = fBestPhaseHistogramContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH2I>>().fTheHistogram;
                
                for(size_t chipId = 0; chipId < NUMBER_OF_CIC_PORTS; ++chipId) // not using the chipID because I want always to read all phases
                {
                    for(size_t cLineId = 0; cLineId < NUMBER_OF_LINES_PER_CIC_PORTS; cLineId++)
                    {
                        hybridAlignmentSuccessHistogram->SetBinContent(chipId+1, cLineId+1, theBestPhaseVector(chipId, cLineId));
                    }
                }
            }
        }
    }
}

//========================================================================================================================
void DQMHistogramOTCICphaseAlignment::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramOTCICphaseAlignment::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTCICphaseAlignment::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization theBestPhasesContainerSerialization("OTCICphaseAlignmentBestPhase");

    if(theBestPhasesContainerSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched OTCICphaseAlignment BestPhase!!!!\n";
        DetectorDataContainer theDetectorData =
            theBestPhasesContainerSerialization.deserializeOpticalGroupContainer<EmptyContainer, EmptyContainer, GenericDataArray_2D<NUMBER_OF_CIC_PORTS, NUMBER_OF_LINES_PER_CIC_PORTS, int>, EmptyContainer>(fDetectorContainer);
        fillBestPhasePhaseResults(theDetectorData);
        return true;
    }

    return false;
    // SoC utilities only - END
}

