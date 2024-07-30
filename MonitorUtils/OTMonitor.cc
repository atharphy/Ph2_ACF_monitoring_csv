#include "MonitorUtils/OTMonitor.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ValueAndTime.h"
#include "HWInterface/D19clpGBTInterface.h"

#ifdef __USE_ROOT__
#include "MonitorDQM/MonitorDQMPlotOT.h"
#endif

OTMonitor::OTMonitor(const Ph2_System::SystemController* theSystemController, const DetectorMonitorConfig& theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig)
{}

void OTMonitor::runMonitorLpGBT(const std::string& monitorValueName)
{
    DetectorDataContainer theLpGBTRegisterContainer;
    ContainerFactory::copyAndInitOpticalGroup<ValueAndTime<uint16_t>>(*fTheSystemController->fDetectorContainer, theLpGBTRegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            uint16_t registerValue = 0;
            try
            {
                registerValue = static_cast<Ph2_HwInterface::D19clpGBTInterface*>(fTheSystemController->flpGBTInterface)->ReadADC(opticalGroup->flpGBT, monitorValueName);
            }
            catch(const std::exception& e)
            {
                continue;
            }
            
            ValueAndTime<uint16_t> theRegisterAndTime(registerValue, getTimeStamp());
            LOG(DEBUG) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - " << monitorValueName << " = " << registerValue << RESET;
            theLpGBTRegisterContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<ValueAndTime<uint16_t>>() = theRegisterAndTime;
        }
    }

#ifdef __USE_ROOT__
    static_cast<MonitorDQMPlotOT*>(fMonitorPlotDQM)->fillLpGBTRegisterPlots(theLpGBTRegisterContainer, monitorValueName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("MonitorOTLpGBTRegister");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, theLpGBTRegisterContainer, monitorValueName);
    }
#endif
}

DetectorDataContainer OTMonitor::getReadoutChipMonitorValues(const std::string& monitorValueName, FrontEndType theFrontEndType)
{
    DetectorDataContainer theReadoutChipMonitorValueContainer;
    ContainerFactory::copyAndInitChip<ValueAndTime<uint16_t>>(*fTheSystemController->fDetectorContainer, theReadoutChipMonitorValueContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            for(const auto& hybrid: *opticalGroup)
            {
                for(const auto& chip: *hybrid)
                {
                    if(chip->getFrontEndType() == theFrontEndType)
                    {
                        readChipMonitorValue(monitorValueName, chip, theReadoutChipMonitorValueContainer);
                    }
                }
            }
        }
    }
    return theReadoutChipMonitorValueContainer;
}

