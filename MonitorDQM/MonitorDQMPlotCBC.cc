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
#include "TAxis.h"
#include "TCanvas.h"
#include "TFile.h"
#include "TGraph.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/ValueAndTime.h"

//========================================================================================================================
MonitorDQMPlotCBC::MonitorDQMPlotCBC() {}

//========================================================================================================================
MonitorDQMPlotCBC::~MonitorDQMPlotCBC() {}

//========================================================================================================================
void MonitorDQMPlotCBC::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const DetectorMonitorConfig& detectorMonitorConfig)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR DQM YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorData ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    for(const auto& registerName: detectorMonitorConfig.fMonitorElementList.at("CBC"))
        if(registerName.second) bookCBCPlots(theOutputFile, theDetectorStructure, registerName.first);
    for(const auto& registerName: detectorMonitorConfig.fMonitorElementList.at("LpGBT"))
        if(registerName.second) bookLpGBTPlots(theOutputFile, theDetectorStructure, registerName.first);
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
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTimeFormat(TIME_FORMAT);
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTimeOffset(0);
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
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTimeFormat(TIME_FORMAT);
    theTGraphPedestalContainer.fTheGraph->GetXaxis()->SetTimeOffset(0);
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
        size_t boardId = board->getId();
        // std::cout <<  __PRETTY_FUNCTION__ << boardId << std::endl;
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            size_t opticalGroupId = opticalGroup->getId();
            // std::cout <<  __PRETTY_FUNCTION__ << opticalGroupId << std::endl;
            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                size_t hybridId = hybrid->getId();
                // std::cout <<  __PRETTY_FUNCTION__ << hybridId << std::endl;
                for(auto chip: *hybrid) // for on chip - begin
                {
                    size_t chipId = chip->getId();
                    // Retreive the corresponging chip histogram:
                    TGraph* chipDQMPlot =
                        fCBCRegisterMonitorPlotMap[registerName].getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(chipId)->getSummary<GraphContainer<TGraph>>().fTheGraph;

                    // Check if the chip data are there (it is needed in the case of the SoC when data may be sent chip
                    // by chip and not in one shot)
                    if(!chip->hasSummary()) continue;
                    auto theValueAndTime = chip->getSummary<ValueAndTime<uint16_t>>();
                    chipDQMPlot->SetPoint(chipDQMPlot->GetN(), getTimeStampForRoot(theValueAndTime.fTime), theValueAndTime.fValue); // for on channel - end
                }                                                                                                                   // for on chip - end
            }                                                                                                                       // for on hybrid - end
        }                                                                                                                           // for on opticalGroup - end
    }                                                                                                                               // for on boards - end
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
        size_t boardId = board->getId();
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            if(!opticalGroup->hasSummary()) continue;
            size_t  opticalGroupId  = opticalGroup->getId();
            TGraph* LpGBTDQMPlot    = fLpGBTRegisterMonitorPlotMap[registerName].getObject(boardId)->getObject(opticalGroupId)->getSummary<GraphContainer<TGraph>>().fTheGraph;
            auto    theValueAndTime = opticalGroup->getSummary<ValueAndTime<uint16_t>>();
            LpGBTDQMPlot->SetPoint(LpGBTDQMPlot->GetN(), getTimeStampForRoot(theValueAndTime.fTime), theValueAndTime.fValue * CONVERSION_FACTOR);
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
bool MonitorDQMPlotCBC::fill(std::string& inputStream)
{
    ContainerSerialization theCBCRegisterSerialization("CBCMonitorCBCRegister");
    ContainerSerialization theLpGBTRegisterSerialization("CBCMonitorLpGBTRegister");

    if(theCBCRegisterSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched CBCMonitor CBCRegister!!!!!\n";
        std::string           registerName;
        DetectorDataContainer fDetectorData =
            theCBCRegisterSerialization.deserializeBoardContainer<EmptyContainer, ValueAndTime<uint16_t>, EmptyContainer, EmptyContainer, EmptyContainer>(fDetectorContainer, registerName);
        fillCBCRegisterPlots(fDetectorData, registerName);
        return true;
    }
    if(theLpGBTRegisterSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched CBCMonitor LpGBTRegister!!!!!\n";
        std::string           registerName;
        DetectorDataContainer fDetectorData =
            theLpGBTRegisterSerialization.deserializeBoardContainer<EmptyContainer, EmptyContainer, EmptyContainer, ValueAndTime<uint16_t>, EmptyContainer>(fDetectorContainer, registerName);
        fillLpGBTRegisterPlots(fDetectorData, registerName);
        return true;
    }
    return false;
}
