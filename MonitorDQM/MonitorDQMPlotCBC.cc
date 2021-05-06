/*!
        \file                MonitorDQMPlotCBC.cc
        \brief               DQM class for DQM example -> use it as a templare
        \author              Fabio Ravera
        \date                25/7/19
        Support :            mail to : fabio.ravera@cern.ch
*/

#include "../MonitorDQM/MonitorDQMPlotCBC.h"
#include "../RootUtils/GraphContainer.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "TCanvas.h"
#include "TFile.h"
#include "TGraph.h"

//========================================================================================================================
MonitorDQMPlotCBC::MonitorDQMPlotCBC() {}

//========================================================================================================================
MonitorDQMPlotCBC::~MonitorDQMPlotCBC() {}

//========================================================================================================================
void MonitorDQMPlotCBC::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const DetectorMonitorConfig& detectorMonitorConfig)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR DQM YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorData ready to receive the information fromm the stream
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    // SoC utilities only - END

    fDoMonitorThreshold = detectorMonitorConfig.isElementToMonitor("CBCThreshold");
    // creating the histograms fo all the chips:
    // create the GraphContainer<TGraph> as you would create a TGraph (it implements some feature needed to avoid memory
    // leaks in copying histograms like the move constructor)
    if(fDoMonitorThreshold)
    {
        GraphContainer<TGraph> theTGraphPedestalContainer(0);
        theTGraphPedestalContainer.setNameTitle("ChipDQM", "ChipDQM");
        // create Histograms for all the chips, they will be automatically accosiated to the output file, no need to save
        // them, change the name for every chip or set their directory
        RootContainerFactory::bookChipHistograms<GraphContainer<TGraph>>(theOutputFile, theDetectorStructure, fDetectorMonitorPlots, theTGraphPedestalContainer);
    }
}

//========================================================================================================================
void MonitorDQMPlotCBC::fillDQMThresholdPlots(DetectorDataContainer& theThresholdContainer, time_t rawTime)
{
    uint32_t timeStampForRoot = getTimeStampForRoot(rawTime);

    for(auto board: theThresholdContainer) // for on boards - begin
    {
        size_t boardIndex = board->getIndex();
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            size_t opticalGroupIndex = opticalGroup->getIndex();
            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                size_t hybridIndex = hybrid->getIndex();
                for(auto chip: *hybrid) // for on chip - begin
                {
                    size_t chipIndex = chip->getIndex();
                    // Retreive the corresponging chip histogram:
                    TGraph* chipDQMPlot = fDetectorMonitorPlots.at(boardIndex)->at(opticalGroupIndex)->at(hybridIndex)->at(chipIndex)->getSummary<GraphContainer<TGraph>>().fTheGraph;

                    // Check if the chip data are there (it is needed in the case of the SoC when data may be sent chip
                    // by chip and not in one shot)
                    if(chip == nullptr) continue;
                    // // Get channel data and fill the histogram
                    // for(auto channel: *chip->getChannelContainer<uint32_t>())   // for on channel - begin
                    chipDQMPlot->SetPoint(chipDQMPlot->GetN(), timeStampForRoot, chip->getSummary<uint16_t>()); // for on channel - end
                }                                                                                               // for on chip - end
            }                                                                                                   // for on hybrid - end
        }                                                                                                       // for on opticalGroup - end
    }                                                                                                           // for on boards - end
}

//========================================================================================================================
void MonitorDQMPlotCBC::process() {}

//========================================================================================================================
void MonitorDQMPlotCBC::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool MonitorDQMPlotCBC::fill(std::vector<char>& dataBuffer)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR DQM YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    // I'm expecting to receive a data stream from an uint16_t contained from DQM "DQMExample"
    ChipContainerStream<uint16_t, EmptyContainer, time_t> theDQMStreamer("CBCMonitor");

    // Try to see if the char buffer matched what I'm expection (container of uint16_t from DQMExample
    // procedure)
    if(fDoMonitorThreshold)
    {
        if(theDQMStreamer.attachBuffer(&dataBuffer))
        {
            // It matched! Decoding chip data
            theDQMStreamer.decodeChipData(fDetectorData);
            // Filling the histograms
            fillDQMThresholdPlots(fDetectorData, theDQMStreamer.getHeaderElement());
            // Cleaning the data container to be ready for the next TCP string
            fDetectorData.cleanDataStored();
            return true;
        }
    }
    // the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    // for this stream)
    return false;
    // SoC utilities only - END
}
