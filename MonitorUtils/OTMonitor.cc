#include "MonitorUtils/OTMonitor.h"
#include "HWInterface/D19clpGBTInterface.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ValueAndTime.h"

#ifdef __USE_ROOT__
#include "MonitorDQM/MonitorDQMPlotOT.h"
#endif

OTMonitor::OTMonitor(const Ph2_System::SystemController* theSystemController, const DetectorMonitorConfig& theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig) {}

void OTMonitor::runMonitorLpGBT(const std::string& monitorValueName)
{
    DetectorDataContainer theLpGBTmonitorValueContainer;
    ContainerFactory::copyAndInitOpticalGroup<ValueAndTime<uint16_t>>(*fTheSystemController->fDetectorContainer, theLpGBTmonitorValueContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            uint16_t monitorValue = 0;
            try
            {
                monitorValue = static_cast<Ph2_HwInterface::D19clpGBTInterface*>(fTheSystemController->flpGBTInterface)->ReadADC(opticalGroup->flpGBT, monitorValueName);
            }
            catch(const std::exception& e)
            {
                continue;
            }

            ValueAndTime<uint16_t> theMonitorValueAndTime(monitorValue, getTimeStamp());
            LOG(DEBUG) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - " << monitorValueName << " = " << monitorValue << RESET;
            theLpGBTmonitorValueContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<ValueAndTime<uint16_t>>() = theMonitorValueAndTime;
        }
    }

#ifdef __USE_ROOT__
    static_cast<MonitorDQMPlotOT*>(fMonitorPlotDQM)->fillLpGBTmonitorPlots(theLpGBTmonitorValueContainer, monitorValueName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("MonitorOTLpGBTMonitor");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, theLpGBTmonitorValueContainer, monitorValueName);
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
                    if(chip->getFrontEndType() == theFrontEndType) { readChipMonitorValue(monitorValueName, chip, theReadoutChipMonitorValueContainer); }
                }
            }
        }
    }
    return theReadoutChipMonitorValueContainer;
}
