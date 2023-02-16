#include "MonitorUtils/SEHMonitor.h"
#include "HWDescription/Definition.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "HWInterface/D19clpGBTInterface.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ValueAndTime.h"

#ifdef __USE_ROOT__
#include "TFile.h"
#endif

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

SEHMonitor::SEHMonitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig)
{
    // Add a new TCP Client to avoid conflicts in parallel process
    LOG(INFO) << BOLDYELLOW << "Trying to connect to the Power Supply Server..." << RESET;
    fPowerSupplyClient = new TCPClient("127.0.0.1", 7000);
    if(!fPowerSupplyClient->connect(1))
    {
        LOG(INFO) << BOLDYELLOW << "Cannot connect to the Power Supply Server, power supplies will need to be controlled manually" << RESET;
        delete fPowerSupplyClient;
        fPowerSupplyClient = nullptr;
    }
    else
    {
        LOG(INFO) << BOLDYELLOW << "Connected to the Power Supply Server!" << RESET;
    }

#ifdef __USE_ROOT__
    fMonitorPlotDQMSEH = new MonitorDQMPlotSEH();
    fMonitorPlotDQMSEH->book(fOutputFile, *fTheSystemController->fDetectorContainer, fDetectorMonitorConfig);
    fMonitorPlotDQM    = new MonitorDQMPlotCBC();
    fMonitorDQMPlotCBC = static_cast<MonitorDQMPlotCBC*>(fMonitorPlotDQM);
    fMonitorDQMPlotCBC->book(fOutputFile, *fTheSystemController->fDetectorContainer, fDetectorMonitorConfig);
#endif
}
// Maybe not ideal here (but needed to avoid memory leak)?? Could be moved to ~DetectorMonitor() if fPowerSupplyClient is also used for other devices?
SEHMonitor::~SEHMonitor()
{
    if(fPowerSupplyClient != nullptr)
    {
        delete fPowerSupplyClient;
        fPowerSupplyClient = nullptr;
    }
#ifdef __USE_ROOT__
    if(fMonitorPlotDQMSEH != nullptr)
    {
        delete fMonitorPlotDQMSEH;
        fMonitorPlotDQMSEH = nullptr;
    }
#endif
}

void SEHMonitor::runMonitor()
{
    std::recursive_mutex                  theMutex;
    std::lock_guard<std::recursive_mutex> theGuard(theMutex);
    for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("LpGBT")) runLpGBTRegisterMonitor(registerName);
    for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("PowerSupply")) runPowerSupplyMonitor(registerName);
    for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("TestCard")) runTestCardMonitor(registerName);
}

void SEHMonitor::runLpGBTRegisterMonitor(std::string registerName)
{
    DetectorDataContainer theLpGBTRegisterContainer;
    ContainerFactory::copyAndInitOpticalGroup<ValueAndTime<uint16_t>>(*fTheSystemController->fDetectorContainer, theLpGBTRegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        if(board->at(0)->flpGBT == nullptr)
        {
            for(const auto& opticalGroup: *board)
                theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<ValueAndTime<uint16_t>>() = ValueAndTime<uint16_t>(0, getTimeStamp());
            continue;
        }
        for(const auto& opticalGroup: *board)
        {
            uint16_t registerValue = static_cast<D19clpGBTInterface*>(fTheSystemController->flpGBTInterface)->ReadADC(opticalGroup->flpGBT, registerName);
            LOG(DEBUG) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - " << registerName << " = " << registerValue << RESET;
            theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<ValueAndTime<uint16_t>>() = ValueAndTime<uint16_t>(registerValue, getTimeStamp());
        }
    }

#ifdef __USE_ROOT__
    fMonitorDQMPlotCBC->fillLpGBTRegisterPlots(theLpGBTRegisterContainer, registerName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("SEHMonitorLpGBTRegister");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, theLpGBTRegisterContainer, registerName);
    }
#endif
}

void SEHMonitor::runPowerSupplyMonitor(std::string registerName)
{
    // LOG(INFO) << BOLDMAGENTA << "We pretend to be a measurement " << registerName<< RESET;
    std::string buffer = fPowerSupplyClient->sendAndReceivePacket("GetStatus");
    LOG(INFO) << BOLDMAGENTA << buffer << RESET;
    // while(!(buffer.find("TimeStamp") != std::string::npos))
    // {
    //     buffer = fPowerSupplyClient->sendAndReceivePacket("GetStatus");
    //     LOG(INFO) << BOLDMAGENTA << buffer << RESET;
    // }
    float cValue = std::stof(getVariableValue(registerName, buffer));
    LOG(INFO) << BOLDMAGENTA << cValue << " " << registerName << RESET;
    if((registerName.find("HV") != std::string::npos) & (registerName.find("Current") != std::string::npos)) { cValue *= 1e9; }
    DetectorDataContainer thePowerSupplyContainer;
    ContainerFactory::copyAndInitDetector<ValueAndTime<float>>(*fTheSystemController->fDetectorContainer, thePowerSupplyContainer);
    thePowerSupplyContainer.getSummary<ValueAndTime<float>>() = ValueAndTime<float>(cValue, getTimeStamp());

#ifdef __USE_ROOT__
    fMonitorPlotDQMSEH->fillPowerSupplyPlots(thePowerSupplyContainer, registerName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("SEHMonitorPowerSupply");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, thePowerSupplyContainer, registerName);
    }
#endif
}

void SEHMonitor::runTestCardMonitor(std::string registerName)
{
    LOG(INFO) << BOLDMAGENTA << "We pretend to be a measurement " << registerName << RESET;
    float cValue = 0;
#if defined(__TCUSB__) && defined(__SEH_USB__) && defined(__USE_ROOT__)
#ifdef __TCP_SERVER__
    fTheSystemController->fTestcardClient->sendAndReceivePacket("read_hvmon:HV_meas");
#else
    fTheSystemController->flpGBTInterface->GetExternalController()->getInterface().read_hvmon(fTheSystemController->flpGBTInterface->GetExternalController()->getInterface().HV_meas, cValue);
#endif
    LOG(INFO) << BOLDMAGENTA << cValue << " " << registerName << RESET;
#endif
    DetectorDataContainer theTestCardContainer;
    ContainerFactory::copyAndInitDetector<ValueAndTime<float>>(*fTheSystemController->fDetectorContainer, theTestCardContainer);
    theTestCardContainer.getSummary<ValueAndTime<float>>() = ValueAndTime<float>(cValue, getTimeStamp());

#ifdef __USE_ROOT__
    fMonitorPlotDQMSEH->fillTestCardPlots(theTestCardContainer, registerName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("SEHMonitorTestCard");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, theTestCardContainer, registerName);
    }
#endif
}

void SEHMonitor::runInputCurrentMonitor(std::string registerName)
{
    LOG(INFO) << BOLDMAGENTA << "Running Input Current Monitor" << RESET;

    DetectorDataContainer theLpGBTRegisterContainer;
    ContainerFactory::copyAndInitOpticalGroup<ValueAndTime<uint16_t>>(*fTheSystemController->fDetectorContainer, theLpGBTRegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        if(board->at(0)->flpGBT == nullptr) continue;
        for(const auto& opticalGroup: *board)
        {
            uint16_t registerValue = (fTheSystemController->flpGBTInterface)->ReadADC(opticalGroup->flpGBT, "ADC1");
            LOG(INFO) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - "
                      << "ADC1"
                      << " = " << registerValue << RESET;
            // theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<ValueAndTime<uint16_t>>() = ValueAndTime<uint16_t>(registerValue, getTimeStamp());
        }
    }
    LOG(INFO) << BOLDMAGENTA << "We pretend to be a measurement" << RESET;
}

std::string SEHMonitor::getVariableValue(std::string variable, std::string buffer)
{
    size_t begin = buffer.find(variable) + variable.size() + 1;
    size_t end   = buffer.find(',', begin);
    if(end == std::string::npos) end = buffer.size();
    return buffer.substr(begin, end - begin);
}
