#include "MonitorUtils/CBCMonitor.h"
#include "HWDescription/Definition.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "HWInterface/D19clpGBTInterface.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/ValueAndTime.h"

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
    ContainerFactory::copyAndInitChip<ValueAndTime<uint16_t>>(*fTheSystemController->fDetectorContainer, theCBCRegisterContainer);

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
                    ValueAndTime<uint16_t> theRegisterAndTime(registerValue, getTimeStamp());
                    theCBCRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<ValueAndTime<uint16_t>>() =
                        theRegisterAndTime;
                }
            }
        }
    }

#ifdef __USE_ROOT__
    fMonitorDQMPlotCBC->fillCBCRegisterPlots(theCBCRegisterContainer, registerName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("CBCMonitorCBCRegister");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, theCBCRegisterContainer, registerName);
    }
#endif
}

void CBCMonitor::runLpGBTRegisterMonitor(std::string registerName)
{
    DetectorDataContainer theLpGBTRegisterContainer;
    ContainerFactory::copyAndInitOpticalGroup<ValueAndTime<uint16_t>>(*fTheSystemController->fDetectorContainer, theLpGBTRegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        if(board->at(0)->flpGBT == nullptr)
        {
            for(const auto& opticalGroup: *board)
            {
                ValueAndTime<uint16_t> theRegisterAndTime(0, getTimeStamp());
                theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<ValueAndTime<uint16_t>>() = theRegisterAndTime;
            }
            continue;
        }
        for(const auto& opticalGroup: *board)
        {
            uint16_t               registerValue = static_cast<D19clpGBTInterface*>(fTheSystemController->flpGBTInterface)->ReadADC(opticalGroup->flpGBT, registerName);
            ValueAndTime<uint16_t> theRegisterAndTime(registerValue, getTimeStamp());
            LOG(DEBUG) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - " << registerName << " = " << registerValue << RESET;
            theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<ValueAndTime<uint16_t>>() = theRegisterAndTime;
        }
    }

#ifdef __USE_ROOT__
    fMonitorDQMPlotCBC->fillLpGBTRegisterPlots(theLpGBTRegisterContainer, registerName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("CBCMonitorLpGBTRegister");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, theLpGBTRegisterContainer, registerName);
    }
#endif
}
