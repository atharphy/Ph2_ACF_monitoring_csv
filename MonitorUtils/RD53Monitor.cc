/*!
  \file                  RD53Monitor.cc
  \brief                 Implementaion of monitoring process
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "MonitorUtils/RD53Monitor.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/Utilities.h"
#include "Utils/ValueAndTime.h"
#include <array>

RD53Monitor::RD53Monitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig)
{
#ifdef __USE_ROOT__
    fMonitorPlotDQM = new MonitorDQMPlotRD53();
    fMonitorDQM     = static_cast<MonitorDQMPlotRD53*>(fMonitorPlotDQM);
    fMonitorDQM->book(fOutputFile, *fTheSystemController->fDetectorContainer, fDetectorMonitorConfig);
#endif
}

void RD53Monitor::runMonitor()
{
    if(fDetectorMonitorConfig.getNumberOfMonitoredRegisters() == 0) return;

    for(const auto cBoard: *fTheSystemController->fDetectorContainer)
    {
        std::vector<std::string> listOfRegisters;
        for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("RD53"))
            if(registerName.second) listOfRegisters.push_back(registerName.first);
        fTheSystemController->ReadSystemMonitor(cBoard, listOfRegisters);

        for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("RD53"))
            if(registerName.second) runRD53RegisterMonitor(registerName.first);

        for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("LpGBT"))
            if(registerName.second) runLpGBTRegisterMonitor(registerName.first);
    }
}

void RD53Monitor::runRD53RegisterMonitor(const std::string& registerName)
{
    DetectorDataContainer theRegisterContainer;
    ContainerFactory::copyAndInitChip<ValueAndTime<float>>(*fTheSystemController->fDetectorContainer, theRegisterContainer);

    for(const auto cBoard: *fTheSystemController->fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    float registerValue;
                    try
                    {
                        registerValue = fTheSystemController->fBeBoardInterface->ReadChipMonitor(fTheSystemController->fReadoutChipInterface, cChip, registerName);

                        LOG(INFO) << GREEN << "Reading monitored data for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                                  << cHybrid->getId() << "/" << +cChip->getId() << RESET << GREEN << "]" << RESET;

                        theRegisterContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<ValueAndTime<float>>() =
                            ValueAndTime<float>(registerValue, getTimeStamp());
                    }
                    catch(...)
                    {
                        return;
                    }
                }

#ifdef __USE_ROOT__
    fMonitorDQM->fillRegisterPlots(theRegisterContainer, registerName);
#endif

    RD53Monitor::sendData(theRegisterContainer, registerName);
}

void RD53Monitor::runLpGBTRegisterMonitor(const std::string& registerName)
{
    DetectorDataContainer theRegisterContainer;
    ContainerFactory::copyAndInitChip<ValueAndTime<float>>(*fTheSystemController->fDetectorContainer, theRegisterContainer);

    for(const auto cBoard: *fTheSystemController->fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
        {
            if(cOpticalGroup->flpGBT == nullptr)
            {
                LOG(INFO) << BOLDRED << "[RD53Monitor::runLpGBTRegisterMonitor] No LpGBT chip found for [board/opticalGroup = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId()
                          << BOLDRED << "]" << RESET;
                continue;
            }

            float registerValue;
            try
            {
                LOG(INFO) << GREEN << "Reading monitored data for [board/opticalGroup = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << RESET << GREEN << "]" << RESET;

                if(fTheSystemController->flpGBTInterface->fADCInputMap.find(registerName) != fTheSystemController->flpGBTInterface->fADCInputMap.end())
                {
                    if(registerName.find("TEMP") != std::string::npos)
                        registerValue = fTheSystemController->flpGBTInterface->GetInternalTemperature(cOpticalGroup->flpGBT);
                    else
                        registerValue = fTheSystemController->flpGBTInterface->ReadADC(cOpticalGroup->flpGBT, registerName);
                }
                else
                    registerValue = fTheSystemController->flpGBTInterface->ReadChipReg(cOpticalGroup->flpGBT, registerName);

                theRegisterContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getSummary<ValueAndTime<float>>() = ValueAndTime<float>(registerValue, getTimeStamp());
            }
            catch(...)
            {
                theRegisterContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getSummary<ValueAndTime<float>>() = ValueAndTime<float>(-1., getTimeStamp());
                return;
            }
        }

#ifdef __USE_ROOT__
    fMonitorDQM->fillRegisterPlots(theRegisterContainer, registerName);
#endif

    RD53Monitor::sendData(theRegisterContainer, registerName);
}

void RD53Monitor::sendData(DetectorDataContainer& DataContainer, const std::string& registerName)
{
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("RD53MonitorRegister");
        theContainerSerialization.streamByChipContainer(fTheSystemController->fMonitorDQMStreamer, DataContainer, registerName);
    }
}
