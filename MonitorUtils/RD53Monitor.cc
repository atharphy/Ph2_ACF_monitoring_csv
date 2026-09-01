/*!
  \file                  RD53Monitor.cc
  \brief                 Implementaion of monitoring process
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "MonitorUtils/RD53Monitor.h"
#include "MonitorUtils/LpGBTMonitoringCsvWriter.h"
#include "MonitorUtils/MonitoringCsvWriter.h"

RD53Monitor::RD53Monitor(const Ph2_System::SystemController* theSystemController, const DetectorMonitorConfig& theDetectorMonitorConfig)
    : DetectorMonitor(theSystemController, theDetectorMonitorConfig)
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
    auto& csvWriter = MonitoringCsvWriter::getInstance();
    if(!csvWriter.shouldMonitorNow()) return;
    csvWriter.beginCycle();
    LpGBTMonitoringCsvWriter::getInstance().beginCycle();

    for(const auto cBoard: *fTheSystemController->fDetectorContainer)
    {
        std::vector<std::string> listOfRegisters;
        for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("RD53"))
            if(registerName.second) listOfRegisters.push_back(registerName.first);
        fTheSystemController->ReadSystemMonitor(cBoard, listOfRegisters, fDetectorMonitorConfig.fSilentRunning);

        for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("RD53"))
            if(registerName.second) runRD53RegisterMonitor(registerName.first);

        for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("LpGBT"))
            if(registerName.second) runLpGBTRegisterMonitor(registerName.first);

        for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("Board"))
            if(registerName.second) runBoardRegisterMonitor(registerName.first);
    }
    csvWriter.endCycle();
    LpGBTMonitoringCsvWriter::getInstance().endCycle();
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
                    float registerValue = -1;
                    if(fDetectorMonitorConfig.fSilentRunning == false)
                        LOG(INFO) << GREEN << "Reading monitored data for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                                  << cHybrid->getId() << "/" << +cChip->getId() << RESET << GREEN << "]" << RESET;
                    auto* readoutChipInterface = fTheSystemController->fReadoutChipInterface;

                    bool isCurrentNotVoltage = false;
                    const bool isAdcObservable = static_cast<Ph2_HwInterface::RD53Interface*>(readoutChipInterface)
                                                     ->getADCobservable(registerName, isCurrentNotVoltage, fDetectorMonitorConfig.fSilentRunning) != -1;
                    if(isAdcObservable)
                        // #######################
                        // # Monitor environment #
                        // #######################
                        registerValue =
                            fTheSystemController->fBeBoardInterface->ReadChipMonitor(fTheSystemController->fReadoutChipInterface, cChip, registerName, fDetectorMonitorConfig.fSilentRunning);
                    else
                    {
                        // #####################
                        // # Monitor registers #
                        // #####################
                        if((registerName.find("_AUTORA") != std::string::npos) || (registerName.find("_AUTORB") != std::string::npos))
                        {
                            auto FWInterface = static_cast<Ph2_HwInterface::RD53FWInterface*>(fTheSystemController->fBeBoardFWMap.find(cBoard->getId())->second);

                            std::string which = "B";
                            if(registerName.find("_AUTORA") != std::string::npos)
                                which = "A";
                            else if(registerName.find("_AUTORB") != std::string::npos)
                                which = "B";

                            registerValue = FWInterface->ReadAutoreadReg(cHybrid->getId(), static_cast<Ph2_HwDescription::RD53*>(cChip)->getChipLane(), which);

                            if(fDetectorMonitorConfig.fSilentRunning == false)
                                LOG(INFO) << BOLDBLUE << "\t--> " << BOLDYELLOW << registerName << BOLDBLUE << " = " << BOLDYELLOW << std::setprecision(0) << registerValue << BOLDBLUE << " (0x"
                                          << BOLDYELLOW << std::hex << registerValue << std::dec << BOLDBLUE << ")" << RESET;
                        }
                        else
                        {
                            try
                            {
                                registerValue = readoutChipInterface->ReadChipReg(cChip, registerName);
                                if(fDetectorMonitorConfig.fSilentRunning == false)
                                    LOG(INFO) << BOLDBLUE << "\t--> " << BOLDYELLOW << registerName << BOLDBLUE << " = " << BOLDYELLOW << std::setprecision(0) << registerValue << BOLDBLUE << " (0x"
                                              << BOLDYELLOW << std::hex << registerValue << std::dec << BOLDBLUE << ")" << RESET;
                            }
                            catch(const std::exception& e)
                            {
                                LOG(ERROR) << BOLDRED << e.what() << RESET;
                                registerValue = -1;
                            }
                        }
                    }

                    theRegisterContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<ValueAndTime<float>>() =
                        ValueAndTime<float>(registerValue, getTimeStampString());

                    MonitoringCsvWriter::getInstance().update(cBoard->getId(), cOpticalGroup->getId(), cHybrid->getId(), cChip->getId(),
                                                              static_cast<Ph2_HwDescription::RD53*>(cChip)->geteFuseCode(), registerName, registerValue,
                                                              isAdcObservable, isCurrentNotVoltage, cHybrid->getNChip());
                }

#ifdef __USE_ROOT__
    fMonitorDQM->fillChipPlots(theRegisterContainer, registerName);
#endif

    RD53Monitor::sendData(theRegisterContainer, registerName, "chip");
}

void RD53Monitor::runLpGBTRegisterMonitor(const std::string& registerName)
{
    DetectorDataContainer theRegisterContainer;
    ContainerFactory::copyAndInitOpticalGroup<ValueAndTime<float>>(*fTheSystemController->fDetectorContainer, theRegisterContainer);

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
            if(fDetectorMonitorConfig.fSilentRunning == false)
                LOG(INFO) << GREEN << "Reading monitored data for [board/opticalGroup = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << RESET << GREEN << "]" << RESET;
            auto* lpGBTInterface = fTheSystemController->flpGBTInterface;

            if(lpGBTInterface->fADCInputMap.find(registerName) != lpGBTInterface->fADCInputMap.end())
                // #######################
                // # Monitor environment #
                // #######################
                registerValue = lpGBTInterface->ReadChipMonitor(cOpticalGroup, registerName, fDetectorMonitorConfig.fSilentRunning);
            else if(registerName == "SFP")
            {
                // ###############
                // # Monitor SFP #
                // ###############
                auto FWInterface = static_cast<Ph2_HwInterface::RD53FWInterface*>(fTheSystemController->fBeBoardFWMap.find(cBoard->getId())->second);
                registerValue    = FWInterface->GetSFPParameter(cOpticalGroup, "RX", lpGBTInterface->GetSFPchannel(cOpticalGroup));
                if(fDetectorMonitorConfig.fSilentRunning == false)
                    LOG(INFO) << BOLDBLUE << "\t--> " << BOLDYELLOW << registerName << BOLDBLUE << " = " << BOLDYELLOW << std::setprecision(0) << registerValue << BOLDBLUE << " (0x" << BOLDYELLOW
                              << std::hex << registerValue << std::dec << BOLDBLUE << ")" << RESET;
            }
            else
            {
                // #####################
                // # Monitor registers #
                // #####################
                try
                {
                    registerValue = lpGBTInterface->ReadChipReg(cOpticalGroup->flpGBT, registerName);
                    if(fDetectorMonitorConfig.fSilentRunning == false)
                        LOG(INFO) << BOLDBLUE << "\t--> " << BOLDYELLOW << registerName << BOLDBLUE << " = " << BOLDYELLOW << std::setprecision(0) << registerValue << BOLDBLUE << " (0x" << BOLDYELLOW
                                  << std::hex << registerValue << std::dec << BOLDBLUE << ")" << RESET;
                }
                catch(const std::exception& e)
                {
                    LOG(ERROR) << BOLDRED << e.what() << RESET;
                    registerValue = -1;
                }
            }

            theRegisterContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getSummary<ValueAndTime<float>>() = ValueAndTime<float>(registerValue, getTimeStampString());
            auto& csvWriter = LpGBTMonitoringCsvWriter::getInstance();
            if(csvWriter.isRunning())
            {
                const std::pair<uint16_t, uint16_t> key{cBoard->getId(), cOpticalGroup->getId()};
                if(fLpGBTFuseIds.count(key) == 0) fLpGBTFuseIds[key] = lpGBTInterface->ReadChipFuseID(cOpticalGroup->flpGBT, cOpticalGroup->flpGBT->getVersion());
                csvWriter.update(cBoard->getId(), cOpticalGroup->getId(), cOpticalGroup->flpGBT->getId(), fLpGBTFuseIds[key], registerName, registerValue);
            }
        }

#ifdef __USE_ROOT__
    fMonitorDQM->fillOptoPlots(theRegisterContainer, registerName);
#endif

    RD53Monitor::sendData(theRegisterContainer, registerName, "opto");
}

void RD53Monitor::runBoardRegisterMonitor(const std::string& registerName)
{
    DetectorDataContainer theRegisterContainer;
    ContainerFactory::copyAndInitBoard<ValueAndTime<float>>(*fTheSystemController->fDetectorContainer, theRegisterContainer);

    for(const auto cBoard: *fTheSystemController->fDetectorContainer)
    {
        float registerValue;
        if(fDetectorMonitorConfig.fSilentRunning == false) LOG(INFO) << GREEN << "Reading monitored data for [board = " << BOLDYELLOW << cBoard->getId() << RESET << GREEN << "]" << RESET;
        auto FWInterface = fTheSystemController->fBeBoardInterface;

        // #####################
        // # Monitor registers #
        // #####################
        try
        {
            registerValue = FWInterface->ReadBoardReg(cBoard, registerName);
            if(fDetectorMonitorConfig.fSilentRunning == false)
                LOG(INFO) << BOLDBLUE << "\t--> " << BOLDYELLOW << registerName << BOLDBLUE << " = " << BOLDYELLOW << std::setprecision(0) << registerValue << BOLDBLUE << " (0x" << BOLDYELLOW
                          << std::hex << registerValue << std::dec << BOLDBLUE << ")" << RESET;
        }
        catch(const std::exception& e)
        {
            LOG(ERROR) << BOLDRED << e.what() << RESET;
            registerValue = -1;
        }

        theRegisterContainer.getObject(cBoard->getId())->getSummary<ValueAndTime<float>>() = ValueAndTime<float>(registerValue, getTimeStampString());
    }

#ifdef __USE_ROOT__
    fMonitorDQM->fillBoardPlots(theRegisterContainer, registerName);
#endif

    RD53Monitor::sendData(theRegisterContainer, registerName, "board");
}

void RD53Monitor::sendData(DetectorDataContainer& DataContainer, const std::string& registerName, const std::string& type)
{
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        if(type == "chip")
        {
            ContainerSerialization theContainerSerialization("ITMonitorChipRegister");
            theContainerSerialization.streamByChipContainer(fTheSystemController->fMonitorDQMStreamer, DataContainer, registerName);
        }
        else if(type == "opto")
        {
            ContainerSerialization theContainerSerialization("ITMonitorOptoRegister");
            theContainerSerialization.streamByOpticalGroupContainer(fTheSystemController->fMonitorDQMStreamer, DataContainer, registerName);
        }
        else if(type == "board")
        {
            ContainerSerialization theContainerSerialization("ITMonitorBoardRegister");
            theContainerSerialization.streamByOpticalGroupContainer(fTheSystemController->fMonitorDQMStreamer, DataContainer, registerName);
        }
    }
}
