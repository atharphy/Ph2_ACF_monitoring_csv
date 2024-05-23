/*!
  \file                  MonitorDQMPlotRD53.cc
  \brief                 Implementaion of DQM monitoring process
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "MonitorDQM/MonitorDQMPlotRD53.h"

void MonitorDQMPlotRD53::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const DetectorMonitorConfig& fDetectorMonitorConfig)
{
    fDetectorContainer = &theDetectorStructure;

    for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("RD53"))
        if(registerName.second)
        {
            auto graphContainer = GraphContainer<TGraph>(0);
            bookImplementer(theOutputFile, theDetectorStructure, fRegisterMonitorPlotMap[registerName.first], graphContainer, "chip", "Time", registerName.first.c_str());
        }

    for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("LpGBT"))
        if(registerName.second)
        {
            auto graphContainer = GraphContainer<TGraph>(0);
            bookImplementer(theOutputFile, theDetectorStructure, fRegisterMonitorPlotMap[registerName.first], graphContainer, "opto", "Time", registerName.first.c_str());
        }
}

bool MonitorDQMPlotRD53::fill(std::string& inputStream)
{
    ContainerSerialization theContainerSerialization("ITMonitorRegister");

    if(theContainerSerialization.attachDeserializer(inputStream))
    {
        LOG(INFO) << GREEN << "Matched IT register" << RESET;
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
        std::string errorMessage = "No booked plots for IT register: ";
        LOG(ERROR) << BOLDRED << errorMessage << BOLDYELLOW << registerName << RESET;
        throw std::runtime_error(errorMessage + registerName);
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
