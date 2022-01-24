/*!
  \file                  MonitorDQMPlotRD53.h
  \brief                 Implementaion of DQM monitoring process
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef MonitorDQMPlotRD53_H
#define MonitorDQMPlotRD53_H

#include "../MonitorDQM/MonitorDQMPlotBase.h"
#include "../RootUtils/GraphContainer.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../Utils/CharArray.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"

#include "TAxis.h"
#include "TCanvas.h"
#include "TFile.h"
#include "TGraph.h"

class MonitorDQMPlotRD53 : public MonitorDQMPlotBase
{
  public:
    MonitorDQMPlotRD53(){};
    ~MonitorDQMPlotRD53(){};

    void book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const DetectorMonitorConfig& detectorMonitorConfig) override;
    bool fill(std::vector<char>& dataBuffer) override;
    void process() override{};
    void reset(void) override{};

    void fillRegisterPlots(DetectorDataContainer& DataContainer, const std::string& registerName);

  private:
    DetectorDataContainer                        DetectorData;
    std::map<std::string, DetectorDataContainer> fRegisterMonitorPlotMap;

    void bookPlots(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, std::string registerName);
};
#endif
