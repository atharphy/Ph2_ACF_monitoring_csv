/*!
  \file                  MonitorDQMPlotRD53.cc
  \brief                 Implementaion of DQM monitoring process
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "MonitorDQMPlotRD53.h"

void MonitorDQMPlotRD53::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const DetectorMonitorConfig& fDetectorMonitorConfig)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    for(unsigned int i = 0; i < fDetectorMonitorConfig.fMonitorElementList.size(); i++)
        if(fDetectorMonitorConfig.isElementToMonitor(fDetectorMonitorConfig.fMonitorElementList[i]) == true)
            bookPlots(theOutputFile, theDetectorStructure, fDetectorMonitorConfig.fMonitorElementList[i]);
}

void MonitorDQMPlotRD53::bookPlots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName)
{
    auto graphContainer = GraphContainer<TGraph>(0);
    bookImplementer(theOutputFile, theDetectorStructure, fRegisterMonitorPlotMap[registerName], graphContainer, "Time", registerName.c_str());
}

bool MonitorDQMPlotRD53::fill(std::vector<char>& dataBuffer)
{
    ChipContainerStream<EmptyContainer, std::tuple<time_t, float>, CharArray> theDQMStreamer("RD533MonitorRegister");

    if(theDQMStreamer.attachBuffer(&dataBuffer))
    {
        theDQMStreamer.decodeChipData(DetectorData);
        fillRegisterPlots(DetectorData, theDQMStreamer.getHeaderElement().getString());
        DetectorData.cleanDataStored();
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
                    chipDQMPlot->SetPoint(
                        chipDQMPlot->GetN(), this->getTimeStampForRoot(std::get<0>(cChip->getSummary<std::tuple<time_t, float>>())), std::get<1>(cChip->getSummary<std::tuple<time_t, float>>()));
                }
}
