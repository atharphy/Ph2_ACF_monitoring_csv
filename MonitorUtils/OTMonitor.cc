#include "MonitorUtils/OTMonitor.h"
#include "HWDescription/lpGBT.h"
#include "HWInterface/D19clpGBTInterface.h"
#include "Utils/ContainerFactory.h"
#include "Utils/NTChandler.h"
#include "Utils/ValueAndTime.h"

#ifdef __USE_ROOT__
#include "MonitorDQM/MonitorDQMPlotOT.h"
#endif

OTMonitor::OTMonitor(const Ph2_System::SystemController* theSystemController, const DetectorMonitorConfig& theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig) {}

void OTMonitor::runMonitor()
{
    // measure temperature for correct ADC calibration
    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            auto  theLpGBT    = static_cast<Ph2_HwDescription::lpGBT*>(opticalGroup->flpGBT);
            float temperature = fTheSystemController->flpGBTInterface->MeasureTemperature(theLpGBT);
            theLpGBT->setTemperature(temperature);
        }
    }
    std::recursive_mutex                  theMutex;
    std::lock_guard<std::recursive_mutex> theGuard(theMutex);
    for(const auto& monitorValueName: fDetectorMonitorConfig.fMonitorElementList.at("LpGBT"))
        if(monitorValueName.second) runMonitorLpGBT(monitorValueName.first);
}

void OTMonitor::runMonitorLpGBT(const std::string& monitorValueName)
{
    DetectorDataContainer theLpGBTmonitorValueContainer;
    ContainerFactory::copyAndInitOpticalGroup<ValueAndTime<float>>(*fTheSystemController->fDetectorContainer, theLpGBTmonitorValueContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            float monitorValue = 0;
            try
            {
                monitorValue = readLpGBTmonitorValue(opticalGroup, monitorValueName);
            }
            catch(const std::exception& e)
            {
                continue;
            }

            ValueAndTime<float> theMonitorValueAndTime(monitorValue, getTimeStamp());
            LOG(DEBUG) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - " << monitorValueName << " = " << monitorValue << RESET;
            theLpGBTmonitorValueContainer.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<ValueAndTime<float>>() = theMonitorValueAndTime;
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
    ContainerFactory::copyAndInitChip<ValueAndTime<float>>(*fTheSystemController->fDetectorContainer, theReadoutChipMonitorValueContainer);

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

float OTMonitor::readLpGBTmonitorValue(Ph2_HwDescription::OpticalGroup* theOpticalGroup, const std::string& monitorValueName)
{
    auto theLpGBT          = static_cast<Ph2_HwDescription::lpGBT*>(theOpticalGroup->flpGBT);
    auto theLpGBRInterface = fTheSystemController->flpGBTInterface;

    float monitorValue = -999.;
    if(std::regex_match(monitorValueName, std::regex("^ADC[0-7]$"))) { monitorValue = theLpGBRInterface->AdcGetVin(theLpGBT, monitorValueName, "VREF/2", 0); }
    else if(std::regex_match(monitorValueName, std::regex("^VDD.*"))) { monitorValue = theLpGBRInterface->MeasurePowerSupplyVoltage(theLpGBT, monitorValueName); }
    else if(monitorValueName == "LpGBTtemp") { monitorValue = theLpGBRInterface->MeasureTemperature(theLpGBT); }
    else if(monitorValueName == "SensorTemp")
    {
        std::string theNTCtype           = "Sensor";
        std::string sensorTemperatureADC = theOpticalGroup->getNTCMap()[theNTCtype];
        theLpGBRInterface->CdacSetCurrent(theLpGBT, sensorTemperatureADC, theLpGBRInterface->_CdacCodeToCurrent(theLpGBT, sensorTemperatureADC, 0xaa));
        float resistance = theLpGBRInterface->MeasureResistance(theLpGBT, sensorTemperatureADC, 1000, false);

        monitorValue = NTChandler::getInstance().getTemperature("Sensor", resistance);
    }
    else if(monitorValueName == "1V25_Left") { monitorValue = theLpGBRInterface->AdcGetVin(theLpGBT, "ADC1", "VREF/2", 0) * (310. / 200.); }
    else if(monitorValueName == "VIN") { monitorValue = theLpGBRInterface->AdcGetVin(theLpGBT, "ADC2", "VREF/2", 0) * (95.6 / 4.7); }
    else if(monitorValueName == "VTRxLeakageCurr") { monitorValue = 2.5 - theLpGBRInterface->AdcGetVin(theLpGBT, "ADC5", "VREF/2", 0) * (146 / 47); }
    else if(monitorValueName == "BPOL2V5temp") { monitorValue = (theLpGBRInterface->AdcGetVin(theLpGBT, "ADC6", "VREF/2", 0) - 0.285) / 0.004; }
    else if(monitorValueName == "BPOL12Vtemp") { monitorValue = (theLpGBRInterface->AdcGetVin(theLpGBT, "ADC7", "VREF/2", 0) - 0.6976) / 0.00302; }

    return monitorValue;
}
