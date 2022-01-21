/*!
  \file                  RD53Monitor.cc
  \brief                 Implementaion of monitoring process
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Monitor.h"

RD53Monitor::RD53Monitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig)
{
#ifdef __USE_ROOT__
    fMonitorPlotDQM = new MonitorDQMPlotRD53();
    fMonitorDQMPlot = static_cast<MonitorDQMPlotRD53*>(fMonitorPlotDQM);
    fMonitorDQMPlot->book(fOutputFile, *fTheSystemController->fDetectorContainer, fDetectorMonitorConfig);
#endif
}

void RD53Monitor::runMonitor()
{
    if(fDetectorMonitorConfig.fMonitorElementList.empty()) return;

    for(const auto cBoard: *fTheSystemController->fDetectorContainer)
    {
        fTheSystemController->ReadSystemMonitor(cBoard, fDetectorMonitorConfig.fMonitorElementList);

        for(unsigned int i = 0; i < fDetectorMonitorConfig.fMonitorElementList.size(); i++)
            if(fDetectorMonitorConfig.isElementToMonitor(fDetectorMonitorConfig.fMonitorElementList[i]) == true) runRegisterMonitor(fDetectorMonitorConfig.fMonitorElementList[i]);
    }
}

void RD53Monitor::runRegisterMonitor(const std::string& registerName)
{
    DetectorDataContainer theRegisterContainer;
    ContainerFactory::copyAndInitChip<std::tuple<time_t, uint16_t>>(*fTheSystemController->fDetectorContainer, theRegisterContainer);

    for(const auto cBoard: *fTheSystemController->fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    uint16_t registerValue = fTheSystemController->fReadoutChipInterface->ReadChipReg(cChip, registerName);
                    theRegisterContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<std::tuple<time_t, uint16_t>>() =
                        std::make_tuple(getTimeStamp(), registerValue);
                }

#ifdef __USE_ROOT__
    fMonitorDQMPlot->fillRegisterPlots(theRegisterContainer, registerName);
#endif

    RD53Monitor::sendData(theRegisterContainer, registerName);
}

void RD53Monitor::sendData(DetectorDataContainer& theRegisterContainer, const std::string& registerName)
{
    auto theRegisterStreamer = prepareOpticalGroupContainerStreamer<EmptyContainer, std::tuple<time_t, uint16_t>, EmptyContainer, EmptyContainer, CharArray>("RD53register");
    theRegisterStreamer.setHeaderElement(CharArray(registerName));

    if(fTheSystemController->fDQMStreamerEnabled == true)
        for(auto board: theRegisterContainer) theRegisterStreamer.streamAndSendBoard(board, fTheSystemController->fMonitorDQMStreamer);
}
