/*!
        \file                MonitorDQMPlotPS.cc
        \brief               DQM class for DQM example -> use it as a templare
        \author              Fabio Ravera
        \date                25/7/19
        Support :            mail to : fabio.ravera@cern.ch
*/

#include "MonitorDQM/MonitorDQMPlotPS.h"
#include "HWDescription/ReadoutChip.h"
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

using namespace Ph2_HwDescription;

//========================================================================================================================
MonitorDQMPlotPS::MonitorDQMPlotPS() {}

//========================================================================================================================
MonitorDQMPlotPS::~MonitorDQMPlotPS() {}

//========================================================================================================================
void MonitorDQMPlotPS::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const DetectorMonitorConfig& detectorMonitorConfig)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR DQM YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorData ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END
    auto        MPAqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string theMPAqueryFunctionString = "MPAqueryFunction";
    theDetectorStructure.addReadoutChipQueryFunction(MPAqueryFunction, theMPAqueryFunctionString);
    for(const auto& registerName: detectorMonitorConfig.fMonitorElementList.at("MPA2")) if(registerName.second) bookMPA2Plots(theOutputFile, theDetectorStructure, registerName.first);
    theDetectorStructure.removeReadoutChipQueryFunction(theMPAqueryFunctionString);
    auto        SSAqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2); };
    std::string theSSAqueryFunctionString = "SSAqueryFunction";
    theDetectorStructure.addReadoutChipQueryFunction(SSAqueryFunction, theSSAqueryFunctionString);
    for(const auto& registerName: detectorMonitorConfig.fMonitorElementList.at("SSA2")) if(registerName.second) bookSSA2Plots(theOutputFile, theDetectorStructure, registerName.first);
    theDetectorStructure.removeReadoutChipQueryFunction(theSSAqueryFunctionString);
    for(const auto& registerName: detectorMonitorConfig.fMonitorElementList.at("LpGBT")) if(registerName.second) bookLpGBTPlots(theOutputFile, theDetectorStructure, registerName.first);
}

//========================================================================================================================
void MonitorDQMPlotPS::bookPSPlots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName)
{
    // creating the histograms for all the chips:
    // create the GraphContainer<TGraph> as you would create a TGraph (it implements some feature needed to avoid memory
    // leaks in copying histograms like the move constructor)
    GraphContainer<TGraph> theTGraphPedestalContainer(0);
    theTGraphPedestalContainer.setNameTitle("PS_DQM_" + registerName, "PS_DQM_" + registerName);
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
    RootContainerFactory::bookChipHistograms<GraphContainer<TGraph>>(theOutputFile, theDetectorStructure, fPSRegisterMonitorPlotMap[registerName], theTGraphPedestalContainer);
}
//========================================================================================================================
void MonitorDQMPlotPS::bookSSA2Plots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName)
{
    // creating the histograms for all the chips:
    // create the GraphContainer<TGraph> as you would create a TGraph (it implements some feature needed to avoid memory
    // leaks in copying histograms like the move constructor)
    GraphContainer<TGraph> theTGraphPedestalContainer(0);
    theTGraphPedestalContainer.setNameTitle("SSA2_DQM_" + registerName, "SSA2_DQM_" + registerName);
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
    RootContainerFactory::bookChipHistograms<GraphContainer<TGraph>>(theOutputFile, theDetectorStructure, fSSA2RegisterMonitorPlotMap[registerName], theTGraphPedestalContainer);
}
//========================================================================================================================
void MonitorDQMPlotPS::bookMPA2Plots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName)
{
    // creating the histograms for all the chips:
    // create the GraphContainer<TGraph> as you would create a TGraph (it implements some feature needed to avoid memory
    // leaks in copying histograms like the move constructor)
    GraphContainer<TGraph> theTGraphPedestalContainer(0);
    theTGraphPedestalContainer.setNameTitle("MPA2_DQM_" + registerName, "MPA2_DQM_" + registerName);
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
    RootContainerFactory::bookChipHistograms<GraphContainer<TGraph>>(theOutputFile, theDetectorStructure, fMPA2RegisterMonitorPlotMap[registerName], theTGraphPedestalContainer);
}
//========================================================================================================================
void MonitorDQMPlotPS::bookLpGBTPlots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName)
{
    // std::cout << __PRETTY_FUNCTION__ << "Booking plot for register = " << registerName << std::endl;
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
void MonitorDQMPlotPS::fillPSRegisterPlots(DetectorDataContainer& theThresholdContainer, const std::string& registerName)
{
    // std::cout <<  __PRETTY_FUNCTION__ << __LINE__ << std::endl;
    if(fPSRegisterMonitorPlotMap.find(registerName) == fPSRegisterMonitorPlotMap.end())
    {
        LOG(ERROR) << BOLDRED << "No plots for PS register " << registerName << RESET;
        LOG(ERROR) << BOLDRED << "Check that DQM and Monitor register names matches" << RESET;
        std::string errorMePSge = "No plots for PS register " + registerName + " - Check that DQM and Monitor register names matches";
        throw std::runtime_error(errorMePSge);
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
                        fPSRegisterMonitorPlotMap[registerName].getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(chipId)->getSummary<GraphContainer<TGraph>>().fTheGraph;

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
void MonitorDQMPlotPS::fillSSA2RegisterPlots(DetectorDataContainer& theThresholdContainer, const std::string& registerName)
{
    // std::cout <<  __PRETTY_FUNCTION__ << " register "<< registerName  << std::endl;
    if(fSSA2RegisterMonitorPlotMap.find(registerName) == fSSA2RegisterMonitorPlotMap.end())
    {
        LOG(ERROR) << BOLDRED << "No plots for SSA2 register " << registerName << RESET;
        LOG(ERROR) << BOLDRED << "Check that DQM and Monitor register names matches" << RESET;
        std::string errorMePSge = "No plots for SSA2 register " + registerName + " - Check that DQM and Monitor register names matches";
        throw std::runtime_error(errorMePSge);
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
                    size_t chipId       = chip->getId();
                    auto theReadoutChip = static_cast<const Ph2_HwDescription::ReadoutChip*>(fDetectorContainer->getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(chipId));
                    if(theReadoutChip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        // Retreive the corresponging chip histogram:
                        TGraph* chipDQMPlot = fSSA2RegisterMonitorPlotMap[registerName]
                                                  .getObject(boardId)
                                                  ->getObject(opticalGroupId)
                                                  ->getObject(hybridId)
                                                  ->getObject(chipId)
                                                  ->getSummary<GraphContainer<TGraph>>()
                                                  .fTheGraph;

                        // Check if the chip data are there (it is needed in the case of the SoC when data may be sent chip
                        // by chip and not in one shot)
                        if(!chip->hasSummary()) continue;
                        auto theValueAndTime = chip->getSummary<ValueAndTime<uint16_t>>();
                        chipDQMPlot->SetPoint(chipDQMPlot->GetN(), getTimeStampForRoot(theValueAndTime.fTime), theValueAndTime.fValue); // for on channel - end
                    }                                                                                                                   // if on chip type
                }                                                                                                                       // for on chip - end
            }                                                                                                                           // for on hybrid - end
        }                                                                                                                               // for on opticalGroup - end
    }                                                                                                                                   // for on boards - end
}
//========================================================================================================================
void MonitorDQMPlotPS::fillMPA2RegisterPlots(DetectorDataContainer& theThresholdContainer, const std::string& registerName)
{
    // std::cout <<  __PRETTY_FUNCTION__ << " register "<< registerName  << std::endl;
    if(fMPA2RegisterMonitorPlotMap.find(registerName) == fMPA2RegisterMonitorPlotMap.end())
    {
        LOG(ERROR) << BOLDRED << "No plots for MPA2 register " << registerName << RESET;
        LOG(ERROR) << BOLDRED << "Check that DQM and Monitor register names matches" << RESET;
        std::string errorMePSge = "No plots for MPA2 register " + registerName + " - Check that DQM and Monitor register names matches";
        throw std::runtime_error(errorMePSge);
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
                    size_t chipId       = chip->getId();
                    auto theReadoutChip = static_cast<const Ph2_HwDescription::ReadoutChip*>(fDetectorContainer->getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(chipId));
                    if(theReadoutChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        // Retreive the corresponging chip histogram:
                        TGraph* chipDQMPlot = fMPA2RegisterMonitorPlotMap[registerName]
                                                  .getObject(boardId)
                                                  ->getObject(opticalGroupId)
                                                  ->getObject(hybridId)
                                                  ->getObject(chipId)
                                                  ->getSummary<GraphContainer<TGraph>>()
                                                  .fTheGraph;

                        // Check if the chip data are there (it is needed in the case of the SoC when data may be sent chip
                        // by chip and not in one shot)
                        if(!chip->hasSummary()) continue;
                        auto theValueAndTime = chip->getSummary<ValueAndTime<uint16_t>>();
                        chipDQMPlot->SetPoint(chipDQMPlot->GetN(), getTimeStampForRoot(theValueAndTime.fTime), theValueAndTime.fValue); // for on channel - end
                    }                                                                                                                   // if on chip type
                }                                                                                                                       // for on chip - end
            }                                                                                                                           // for on hybrid - end
        }                                                                                                                               // for on opticalGroup - end
    }                                                                                                                                   // for on boards - end
}
//========================================================================================================================
void MonitorDQMPlotPS::fillLpGBTRegisterPlots(DetectorDataContainer& theThresholdContainer, const std::string& registerName)
{
    // std::cout <<  __PRETTY_FUNCTION__ << __LINE__ << std::endl;
    if(fLpGBTRegisterMonitorPlotMap.find(registerName) == fLpGBTRegisterMonitorPlotMap.end())
    {
        LOG(ERROR) << BOLDRED << "No plots for LpGBT register " << registerName << RESET;
        LOG(ERROR) << BOLDRED << "Check that DQM and Monitor register names matches" << RESET;
        std::string errorMePSge = "No plots for LpGBT register " + registerName + " - Check that DQM and Monitor register names matches";
        throw std::runtime_error(errorMePSge);
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
void MonitorDQMPlotPS::process() {}

//========================================================================================================================
void MonitorDQMPlotPS::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool MonitorDQMPlotPS::fill(std::string& inputStream)
{
    // ContainerSerialization thePSRegisterSerialization("PSMonitorPSRegister");
    ContainerSerialization theSSA2RegisterSerialization("PSMonitorSSA2Register");
    ContainerSerialization theMPA2RegisterSerialization("PSMonitorMPA2Register");
    ContainerSerialization theLpGBTRegisterSerialization("PSMonitorLpGBTRegister");

    if(theSSA2RegisterSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched PSMonitor SSA2Register!!!!!\n";
        std::string           registerName;
        DetectorDataContainer fDetectorData =
            theSSA2RegisterSerialization.deserializeBoardContainer<EmptyContainer, ValueAndTime<uint16_t>, EmptyContainer, EmptyContainer, EmptyContainer>(fDetectorContainer, registerName);
        fillSSA2RegisterPlots(fDetectorData, registerName);
        return true;
    }
    if(theMPA2RegisterSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched PSMonitor MPA2Register!!!!!\n";
        std::string           registerName;
        DetectorDataContainer fDetectorData =
            theMPA2RegisterSerialization.deserializeBoardContainer<EmptyContainer, ValueAndTime<uint16_t>, EmptyContainer, EmptyContainer, EmptyContainer>(fDetectorContainer, registerName);
        fillMPA2RegisterPlots(fDetectorData, registerName);
        return true;
    }
    if(theLpGBTRegisterSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched PSMonitor LpGBTRegister!!!!!\n";
        std::string           registerName;
        DetectorDataContainer fDetectorData =
            theLpGBTRegisterSerialization.deserializeBoardContainer<EmptyContainer, EmptyContainer, EmptyContainer, ValueAndTime<uint16_t>, EmptyContainer>(fDetectorContainer, registerName);
        fillLpGBTRegisterPlots(fDetectorData, registerName);
        return true;
    }
    return false;
}
