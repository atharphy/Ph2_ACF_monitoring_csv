/*!
        \file                MonitorDQMPlotCBC.cc
        \brief               DQM class for DQM example -> use it as a templare
        \author              Fabio Ravera
        \date                25/7/19
        Support :            mail to : fabio.ravera@cern.ch
*/

#include "MonitorDQM/MonitorDQMPlotCBC.h"
#include "RootUtils/GraphContainer.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/BoardContainerStream.h"
#include "Utils/CharArray.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerStream.h"
#include "TAxis.h"
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

    for(const auto& registerName: detectorMonitorConfig.fMonitorElementList.at("CBC")) bookCBCPlots(theOutputFile, theDetectorStructure, registerName);
    for(const auto& registerName: detectorMonitorConfig.fMonitorElementList.at("LpGBT")) bookLpGBTPlots(theOutputFile, theDetectorStructure, registerName);
}

//========================================================================================================================
void MonitorDQMPlotCBC::bookCBCPlots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName)
{
    // creating the histograms for all the chips:
    // create the GraphContainer<TGraph> as you would create a TGraph (it implements some feature needed to avoid memory
    // leaks in copying histograms like the move constructor)
    GraphContainer<TGraph> theTGraphPedestalContainer(0);
    theTGraphPedestalContainer.setNameTitle("CBC_DQM_" + registerName, "CBC_DQM_" + registerName);
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTimeDisplay(1);
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetNdivisions(503);
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTimeFormat("%Y-%m-%d %H:%M");
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTimeOffset(0, "gmt");
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTitle("time");
    theTGraphPedestalContainer.fTheGraph->GetYaxis()->SetTitle(registerName.c_str());
    theTGraphPedestalContainer.fTheGraph->SetMarkerStyle(20);
    theTGraphPedestalContainer.fTheGraph->SetMarkerSize(0.4);
    // create Histograms for all the chips, they will be automatically accosiated to the output file, no need to save
    // them, change the name for every chip or set their directory
    RootContainerFactory::bookChipHistograms<GraphContainer<TGraph>>(theOutputFile, theDetectorStructure, fCBCRegisterMonitorPlotMap[registerName], theTGraphPedestalContainer);
}

//========================================================================================================================
void MonitorDQMPlotCBC::bookLpGBTPlots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName)
{
    std::cout << __PRETTY_FUNCTION__ << "Booking plot for register = " << registerName << std::endl;
    // creating the histograms for all the chips:
    // create the GraphContainer<TGraph> as you would create a TGraph (it implements some feature needed to avoid memory
    // leaks in copying histograms like the move constructor)
    GraphContainer<TGraph> theTGraphPedestalContainer(0);
    theTGraphPedestalContainer.setNameTitle("LpGBT_DQM_" + registerName, "LpGBT_DQM_" + registerName);
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTimeDisplay(1);
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetNdivisions(503);
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTimeFormat("%Y-%m-%d %H:%M");
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTimeOffset(0, "gmt");
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTitle("time");
    theTGraphPedestalContainer.fTheGraph->GetYaxis()->SetTitle((registerName + " [V]").c_str());
    theTGraphPedestalContainer.fTheGraph->SetMarkerStyle(20);
    theTGraphPedestalContainer.fTheGraph->SetMarkerSize(0.4);
    // create Histograms for all the chips, they will be automatically accosiated to the output file, no need to save
    // them, change the name for every chip or set their directory
    RootContainerFactory::bookOpticalGroupHistograms<GraphContainer<TGraph>>(theOutputFile, theDetectorStructure, fLpGBTRegisterMonitorPlotMap[registerName], theTGraphPedestalContainer);
}

//========================================================================================================================
void MonitorDQMPlotCBC::fillCBCRegisterPlots(DetectorDataContainer& theThresholdContainer, const std::string& registerName)
{
    // std::cout <<  __PRETTY_FUNCTION__ << __LINE__ << std::endl;
    if(fCBCRegisterMonitorPlotMap.find(registerName) == fCBCRegisterMonitorPlotMap.end())
    {
        LOG(ERROR) << BOLDRED << "No plots for CBC register " << registerName << RESET;
        LOG(ERROR) << BOLDRED << "Check that DQM and Monitor register names matches" << RESET;
        std::string errorMessage = "No plots for CBC register " + registerName + " - Check that DQM and Monitor register names matches";
        throw std::runtime_error(errorMessage);
    }

    for(auto board: theThresholdContainer) // for on boards - begin
    {
        size_t boardIndex = board->getIndex();
        // std::cout <<  __PRETTY_FUNCTION__ << boardIndex << std::endl;
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            size_t opticalGroupIndex = opticalGroup->getIndex();
            // std::cout <<  __PRETTY_FUNCTION__ << opticalGroupIndex << std::endl;
            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                size_t hybridIndex = hybrid->getIndex();
                // std::cout <<  __PRETTY_FUNCTION__ << hybridIndex << std::endl;
                for(auto chip: *hybrid) // for on chip - begin
                {
                    size_t chipIndex = chip->getIndex();
                    // Retreive the corresponging chip histogram:
                    TGraph* chipDQMPlot =
                        fCBCRegisterMonitorPlotMap[registerName].at(boardIndex)->at(opticalGroupIndex)->at(hybridIndex)->at(chipIndex)->getSummary<GraphContainer<TGraph>>().fTheGraph;

                    // Check if the chip data are there (it is needed in the case of the SoC when data may be sent chip
                    // by chip and not in one shot)
                    if(!chip->hasSummary()) continue;
                    // std::cout <<  __PRETTY_FUNCTION__ << "has summary" << std::endl;
                    // // Get channel data and fill the histogram
                    // for(auto channel: *chip->getChannelContainer<uint32_t>())   // for on channel - begin
                    // std::cout <<  __PRETTY_FUNCTION__ << "Filling CBC plot with " << std::get<0>(chip->getSummary<std::tuple<time_t, uint16_t>>()) << " - " <<
                    // std::get<1>(chip->getSummary<std::tuple<time_t, uint16_t>>()) << std::endl;
                    chipDQMPlot->SetPoint(chipDQMPlot->GetN(),
                                          getTimeStampForRoot(std::get<0>(chip->getSummary<std::tuple<time_t, uint16_t>>())),
                                          std::get<1>(chip->getSummary<std::tuple<time_t, uint16_t>>())); // for on channel - end
                }                                                                                         // for on chip - end
            }                                                                                             // for on hybrid - end
        }                                                                                                 // for on opticalGroup - end
    }                                                                                                     // for on boards - end
}

//========================================================================================================================
void MonitorDQMPlotCBC::fillLpGBTRegisterPlots(DetectorDataContainer& theThresholdContainer, const std::string& registerName)
{
    if(fLpGBTRegisterMonitorPlotMap.find(registerName) == fLpGBTRegisterMonitorPlotMap.end())
    {
        LOG(ERROR) << BOLDRED << "No plots for LpGBT register " << registerName << RESET;
        LOG(ERROR) << BOLDRED << "Check that DQM and Monitor register names matches" << RESET;
        std::string errorMessage = "No plots for LpGBT register " + registerName + " - Check that DQM and Monitor register names matches";
        throw std::runtime_error(errorMessage);
    }

    for(auto board: theThresholdContainer) // for on boards - begin
    {
        size_t boardIndex = board->getIndex();
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            if(!opticalGroup->hasSummary()) continue;
            size_t  opticalGroupIndex = opticalGroup->getIndex();
            TGraph* LpGBTDQMPlot      = fLpGBTRegisterMonitorPlotMap[registerName].at(boardIndex)->at(opticalGroupIndex)->getSummary<GraphContainer<TGraph>>().fTheGraph;
            LpGBTDQMPlot->SetPoint(LpGBTDQMPlot->GetN(),
                                   getTimeStampForRoot(std::get<0>(opticalGroup->getSummary<std::tuple<time_t, uint16_t>>())),
                                   std::get<1>(opticalGroup->getSummary<std::tuple<time_t, uint16_t>>()) * CONVERSION_FACTOR);

            // std::cout << "Filling plot " << registerName << " at time " << std::get<0>(opticalGroup->getSummary<std::tuple<time_t, uint16_t>>()) << " with value " <<
            // (std::get<1>(opticalGroup->getSummary<std::tuple<time_t, uint16_t>>()) * CONVERSION_FACTOR) << std::endl;
        } // for on opticalGroup - end
    }     // for on boards - end
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
    BoardContainerStream<EmptyContainer, std::tuple<time_t, uint16_t>, EmptyContainer, EmptyContainer, EmptyContainer, CharArray> theCBCDQMStreamer("CBCMonitorCBCRegister");
    BoardContainerStream<EmptyContainer, EmptyContainer, EmptyContainer, std::tuple<time_t, uint16_t>, EmptyContainer, CharArray> theLpGBTDQMStreamer("CBCMonitorLpGBTRegister");

    // std::cout <<  __PRETTY_FUNCTION__ << __LINE__ << std::endl;

    if(theCBCDQMStreamer.attachBuffer(&dataBuffer))
    {
        // std::cout <<  __PRETTY_FUNCTION__ << "Matches CBC monitor" << std::endl;
        // It matched! Decoding chip data
        theCBCDQMStreamer.decodeData(fDetectorData);
        // Filling the histograms
        CharArray registerNameArray = theCBCDQMStreamer.getHeaderElement();
        // std::cout <<  __PRETTY_FUNCTION__ << "registerNameArray = " << registerNameArray.getString() << std::endl;

        fillCBCRegisterPlots(fDetectorData, registerNameArray.getString());
        // Cleaning the data container to be ready for the next TCP string
        fDetectorData.cleanDataStored();
        return true;
    }

    if(theLpGBTDQMStreamer.attachBuffer(&dataBuffer))
    {
        // It matched! Decoding chip data
        theLpGBTDQMStreamer.decodeData(fDetectorData);
        // Filling the histograms
        CharArray registerNameArray = theLpGBTDQMStreamer.getHeaderElement();

        fillLpGBTRegisterPlots(fDetectorData, registerNameArray.getString());
        // Cleaning the data container to be ready for the next TCP string
        fDetectorData.cleanDataStored();
        return true;
    }

    // the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    // for this stream)
    return false;
    // SoC utilities only - END
}
