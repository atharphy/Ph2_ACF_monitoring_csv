#include "SEHMonitor.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/OuterTrackerHybrid.h"
#include "../HWInterface/D19clpGBTInterface.h"
#include "../Utils/CharArray.h"
#include "../Utils/ContainerFactory.h"

#ifdef __USE_ROOT__
#include "TFile.h"
#endif

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

SEHMonitor::SEHMonitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig)
{
#ifdef __USE_ROOT__
    fMonitorPlotDQM    = new MonitorDQMPlotCBC();
    fMonitorDQMPlotSEH = static_cast<MonitorDQMPlotCBC*>(fMonitorPlotDQM);
    fMonitorDQMPlotSEH->book(fOutputFile, *fTheSystemController->fDetectorContainer, fDetectorMonitorConfig);
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
    ContainerFactory::copyAndInitOpticalGroup<std::tuple<time_t, uint16_t>>(*fTheSystemController->fDetectorContainer, theLpGBTRegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        if(board->at(0)->flpGBT == nullptr)
        {
            for(const auto& opticalGroup: *board)
                theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<std::tuple<time_t, uint16_t>>() = std::make_tuple(getTimeStamp(), -1);
            continue;
        }
        for(const auto& opticalGroup: *board)
        {
            uint16_t registerValue = static_cast<D19clpGBTInterface*>(fTheSystemController->flpGBTInterface)->ReadADC(opticalGroup->flpGBT, registerName);
            LOG(DEBUG) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - " << registerName << " = " << registerValue << RESET;
            theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<std::tuple<time_t, uint16_t>>() = std::make_tuple(getTimeStamp(), registerValue);
        }
    }

#ifdef __USE_ROOT__
    fMonitorDQMPlotSEH->fillLpGBTRegisterPlots(theLpGBTRegisterContainer, registerName);
#else
    auto theLpGBTRegisterStreamer = prepareBoardContainerStreamer<EmptyContainer, EmptyContainer, EmptyContainer, std::tuple<time_t, uint16_t>, EmptyContainer, CharArray>("LpGBTRegister");
    theLpGBTRegisterStreamer->setHeaderElement(CharArray(registerName));
    if(fTheSystemController->fDQMStreamerEnabled)
    {
        for(auto board: theLpGBTRegisterContainer) theLpGBTRegisterStreamer->streamAndSendBoard(board, fTheSystemController->fMonitorDQMStreamer);
    }
#endif
}

void SEHMonitor::runPowerSupplyMonitor(std::string registerName)
{
    // LOG(INFO) << BOLDMAGENTA << "We pretend to be a measurement " << registerName<< RESET;
    std::string buffer = fTheSystemController->fPowerSupplyClient->sendAndReceivePacket("GetStatus");
    LOG(INFO) << BOLDMAGENTA << buffer << RESET;
    while(!(buffer.find("TimeStamp") != std::string::npos))
    {
        buffer = fTheSystemController->fPowerSupplyClient->sendAndReceivePacket("GetStatus");
        LOG(INFO) << BOLDMAGENTA << buffer << RESET;
    }
    float cValue = std::stof(getVariableValue(registerName, buffer));
    LOG(INFO) << BOLDMAGENTA << cValue << " " << registerName << RESET;
    if((registerName.find("HV") != std::string::npos) & (registerName.find("Current") != std::string::npos)) { cValue *= 1e9; }
    DetectorDataContainer thePowerSupplyContainer;
    ContainerFactory::copyAndInitDetector<std::tuple<time_t, float>>(*fTheSystemController->fDetectorContainer, thePowerSupplyContainer);
    thePowerSupplyContainer.getSummary<std::tuple<time_t, float>>() = std::make_tuple(getTimeStamp(), cValue);

#ifdef __USE_ROOT__
    fMonitorDQMPlotSEH->fillPowerSupplyPlots(thePowerSupplyContainer, registerName);
#else
    auto thePowerSupplyStreamer = prepareBoardContainerStreamer<EmptyContainer, EmptyContainer, EmptyContainer, std::tuple<time_t, float>, EmptyContainer, CharArray>("PowerSupply");
    thePowerSupplyStreamer->setHeaderElement(CharArray(registerName));
    if(fTheSystemController->fDQMStreamerEnabled)
    {
        for(auto board: thePowerSupplyContainer) thePowerSupplyStreamer->streamAndSendBoard(board, fTheSystemController->fMonitorDQMStreamer);
    }
#endif
}

void SEHMonitor::runTestCardMonitor(std::string registerName)
{
    LOG(INFO) << BOLDMAGENTA << "We pretend to be a measurement " << registerName << RESET;
    float cValue = 0;
    fTheSystemController->flpGBTInterface->getExternalController()->getInterface().read_hvmon(fTheSystemController->flpGBTInterface->getExternalController()->getInterface().HV_meas, cValue);
    LOG(INFO) << BOLDMAGENTA << cValue << " " << registerName << RESET;

    DetectorDataContainer theTestCardContainer;
    ContainerFactory::copyAndInitDetector<std::tuple<time_t, float>>(*fTheSystemController->fDetectorContainer, theTestCardContainer);
    theTestCardContainer.getSummary<std::tuple<time_t, float>>() = std::make_tuple(getTimeStamp(), cValue);

#ifdef __USE_ROOT__
    fMonitorDQMPlotSEH->fillTestCardPlots(theTestCardContainer, registerName);
#else
    auto theTestCardStreamer = prepareBoardContainerStreamer<EmptyContainer, EmptyContainer, EmptyContainer, std::tuple<time_t, float>, EmptyContainer, CharArray>("TestCard");
    thePowerSupplyStreamer->setHeaderElement(CharArray(registerName));
    if(fTheSystemController->fDQMStreamerEnabled)
    {
        for(auto board: theTestCardContainer) theTestCardStreamer->streamAndSendBoard(board, fTheSystemController->fMonitorDQMStreamer);
    }
#endif
}

void SEHMonitor::runInputCurrentMonitor(std::string registerName)
{
    LOG(INFO) << BOLDMAGENTA << "Running Input Current Monitor" << RESET;

    DetectorDataContainer theLpGBTRegisterContainer;
    ContainerFactory::copyAndInitOpticalGroup<std::tuple<time_t, uint16_t>>(*fTheSystemController->fDetectorContainer, theLpGBTRegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        if(board->at(0)->flpGBT == nullptr) continue;
        for(const auto& opticalGroup: *board)
        {
            uint16_t registerValue = (fTheSystemController->flpGBTInterface)->ReadADC(opticalGroup->flpGBT, "ADC1");
            LOG(INFO) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - "
                      << "ADC1"
                      << " = " << registerValue << RESET;
            // theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<std::tuple<time_t, uint16_t>>() = std::make_tuple(getTimeStamp(), registerValue);
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