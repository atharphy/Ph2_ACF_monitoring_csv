/*!
  \file                  MonitorDQMPlotRD53.cc
  \brief                 Implementaion of DQM monitoring process
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "MonitorDQM/MonitorDQMPlotRD53.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/ValueAndTime.h"

void MonitorDQMPlotRD53::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const DetectorMonitorConfig& fDetectorMonitorConfig)
{
    fDetectorContainer = &theDetectorStructure;

    for(unsigned int i = 0; i < fDetectorMonitorConfig.fMonitorElementList.at("RD53").size(); i++)
        bookPlots(theOutputFile, theDetectorStructure, fDetectorMonitorConfig.fMonitorElementList.at("RD53")[i]);
}

void MonitorDQMPlotRD53::bookPlots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName)
{
    auto graphContainer = GraphContainer<TGraph>(0);
    bookImplementer(theOutputFile, theDetectorStructure, fRegisterMonitorPlotMap[registerName], graphContainer, "Time", registerName.c_str());
}

bool MonitorDQMPlotRD53::fill(std::string& inputStream)
{
    
    ContainerSerialization theContainerSerialization("RD53MonitorRegister");

    if(theContainerSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched RD53Monitor Register!!!!!\n";
        std::string           registerName;
        DetectorDataContainer fDetectorData = theContainerSerialization.deserializeChipContainer<EmptyContainer, ValueAndTime<float>>(fDetectorContainer, registerName);
        fillRegisterPlots(fDetectorData, registerName);
        return true;
    }
    return false;
}

void MonitorDQMPlotRD53::fillRegisterPlots(DetectorDataContainer& DataContainer, const std::string& registerName)
{
    if(fRegisterMonitorPlotMap.find(registerName) == fRegisterMonitorPlotMap.end())
    {
        std::string errorMessage = "No booked plots for RD53 register: " + registerName;
        LOG(ERROR) << BOLDRED << errorMessage << RESET;
        throw std::runtime_error(errorMessage);
    }

    for(const auto cBoard: DataContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    TGraph* chipDQMPlot = fRegisterMonitorPlotMap[registerName]
                                              .getObject(cBoard->getId())
                                              ->getObject(cOpticalGroup->getId())
                                              ->getObject(cHybrid->getId())
                                              ->getObject(cChip->getId())
                                              ->getSummary<GraphContainer<TGraph>>()
                                              .fTheGraph;

                    if(cChip->hasSummary() == false) continue;
                    chipDQMPlot->SetPoint(chipDQMPlot->GetN(), this->getTimeStampForRoot(cChip->getSummary<ValueAndTime<float>>().fTime), cChip->getSummary<ValueAndTime<float>>().fValue);
                }
}
