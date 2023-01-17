#include "MonitorUtils/CBCMonitor.h"
#include "HWDescription/Definition.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "HWInterface/D19clpGBTInterface.h"
#include "Utils/CharArray.h"
#include "Utils/ContainerFactory.h"

#ifdef __USE_ROOT__
#include "TFile.h"
#endif

using namespace Ph2_HwInterface;

CBCMonitor::CBCMonitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig)
{
#ifdef __USE_ROOT__
    fMonitorPlotDQM    = new MonitorDQMPlotCBC();
    fMonitorDQMPlotCBC = static_cast<MonitorDQMPlotCBC*>(fMonitorPlotDQM);
    fMonitorDQMPlotCBC->book(fOutputFile, *fTheSystemController->fDetectorContainer, fDetectorMonitorConfig);
#endif
}

void CBCMonitor::runMonitor()
{
    std::recursive_mutex                  theMutex;
    std::lock_guard<std::recursive_mutex> theGuard(theMutex);
    for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("CBC")) runCBCRegisterMonitor(registerName);
    for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("LpGBT")) runLpGBTRegisterMonitor(registerName);
}

void CBCMonitor::runCBCRegisterMonitor(std::string registerName)
{
    DetectorDataContainer theCBCRegisterContainer;
    ContainerFactory::copyAndInitChip<std::tuple<time_t, uint16_t>>(*fTheSystemController->fDetectorContainer, theCBCRegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            for(const auto& hybrid: *opticalGroup)
            {
                for(const auto& chip: *hybrid)
                {
                    uint16_t registerValue = fTheSystemController->fReadoutChipInterface->ReadChipReg(chip, registerName); // just to read something
                    LOG(DEBUG) << BOLDMAGENTA << "CBC " << hybrid->getId() << " - " << registerName << " = " << registerValue << RESET;
                    theCBCRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<std::tuple<time_t, uint16_t>>() =
                        std::make_tuple(getTimeStamp(), registerValue);
                }
            }
        }
    }

#ifdef __USE_ROOT__
    fMonitorDQMPlotCBC->fillCBCRegisterPlots(theCBCRegisterContainer, registerName);
#else
    auto theCBCRegisterStreamer = prepareBoardContainerStreamer<EmptyContainer, std::tuple<time_t, uint16_t>, EmptyContainer, EmptyContainer, EmptyContainer, CharArray>("CBCRegister");
    theCBCRegisterStreamer->setHeaderElement(CharArray(registerName));
    if(fTheSystemController->fDQMStreamerEnabled)
    {
        for(auto board: theCBCRegisterContainer) { theCBCRegisterStreamer->streamAndSendBoard(board, fTheSystemController->fMonitorDQMStreamer); }
    }
#endif
}

void CBCMonitor::runLpGBTRegisterMonitor(std::string registerName)
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
    fMonitorDQMPlotCBC->fillLpGBTRegisterPlots(theLpGBTRegisterContainer, registerName);
#else
    auto theLpGBTRegisterStreamer = prepareBoardContainerStreamer<EmptyContainer, EmptyContainer, EmptyContainer, std::tuple<time_t, uint16_t>, EmptyContainer, CharArray>("LpGBTRegister");
    theLpGBTRegisterStreamer->setHeaderElement(CharArray(registerName));
    if(fTheSystemController->fDQMStreamerEnabled)
    {
        for(auto board: theLpGBTRegisterContainer) theLpGBTRegisterStreamer->streamAndSendBoard(board, fTheSystemController->fMonitorDQMStreamer);
    }
#endif
}
