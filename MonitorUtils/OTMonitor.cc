#include "MonitorUtils/OTMonitor.h"
#include "HWDescription/lpGBT.h"
#include "HWInterface/D19clpGBTInterface.h"
#include "Utils/ContainerFactory.h"
#include "Utils/NTChandler.h"
#include "Utils/ValueAndTime.h"
#include <chrono>
#include <map>

#ifdef __USE_ROOT__
#include "MonitorDQM/MonitorDQMPlotOT.h"
#endif

OTMonitor::OTMonitor(const Ph2_System::SystemController* theSystemController, const DetectorMonitorConfig& theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig)
{
    fMQTTBrokerHost = fDetectorMonitorConfig.fMQTTBrokerHost;
    fMQTTBrokerPort = fDetectorMonitorConfig.fMQTTBrokerPort;
    fMQTTTopic      = fDetectorMonitorConfig.fMQTTTopic;
    fMQTTEnabled    = fDetectorMonitorConfig.fMQTTEnabled;

    if(fMQTTEnabled)
    {
        // try to query on config topic to check for mqtt broker availability, disable if not available
        std::string command = "mosquitto_sub -h " + fMQTTBrokerHost + " -p " + std::to_string(fMQTTBrokerPort) + " -t " + fMQTTTopic + "/config -C 1 -W 2 | grep 1 2>&1 > /dev/null";
        int         result  = system(command.c_str());
        if(result != 0)
        {
            LOG(WARNING) << "MQTT broker not available at " << fMQTTBrokerHost << ":" << fMQTTBrokerPort << ". Disabling MQTT publishing." << RESET;
            fMQTTEnabled = false;
        }
        else { LOG(INFO) << "MQTT broker available at " << fMQTTBrokerHost << ":" << fMQTTBrokerPort << ". MQTT publishing enabled." << RESET; }
    }
}

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

            ValueAndTime<float> theMonitorValueAndTime(monitorValue, getTimeStampString());
            // LOG(DEBUG) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - " << monitorValueName << " = " << monitorValue << RESET;
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

    std::string chipType = "";
    if(theFrontEndType == FrontEndType::SSA2) { chipType = "SSA"; }
    else if(theFrontEndType == FrontEndType::MPA2) { chipType = "MPA"; }

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            std::string json_payload = "{";
            json_payload += "\"counter\":" + std::to_string(fMQTTcounter) + ",";
            json_payload += "\"timestamp\":" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
            // add beboard
            json_payload += ",\"BeBoardId\":" + std::to_string(board->getId());
            // add optical group
            auto theLpGBT = static_cast<Ph2_HwDescription::lpGBT*>(opticalGroup->flpGBT);
            if(theLpGBT == nullptr) continue;
            // Read LpGBT fuse ID (cached to avoid repeated register reads)
            uint32_t ogId = opticalGroup->getId();
            if(fFuseIdCache.find(ogId) == fFuseIdCache.end()) { fFuseIdCache[ogId] = fTheSystemController->flpGBTInterface->ReadChipFuseID(theLpGBT); }
            json_payload += ",\"LpGBT_OG" + std::to_string(ogId) + "_fuseId\":" + std::to_string(fFuseIdCache[ogId]);

            for(const auto& hybrid: *opticalGroup)
            {
                for(const auto& chip: *hybrid)
                {
                    if(chip->getFrontEndType() == theFrontEndType)
                    {
                        // get chip temperature and time
                        ValueAndTime<float> theRegisterAndTime = readChipMonitorValue(monitorValueName, chip, theReadoutChipMonitorValueContainer);

                        std::string hybridId = std::to_string(chip->getHybridId());
                        std::string chipId   = std::to_string(chip->getId());
                        json_payload += ",\"" + chipType + "_H" + hybridId + "_C" + chipId + "_" + monitorValueName + "\":" + std::to_string(theRegisterAndTime.fValue);
                    }
                }
            }

            json_payload += "}";
            // publish JSON to MQTT
            publishToMQTT(json_payload);
        }
    }
    return theReadoutChipMonitorValueContainer;
}

float OTMonitor::readLpGBTmonitorValue(Ph2_HwDescription::OpticalGroup* theOpticalGroup, const std::string& monitorValueName)
{
    auto theLpGBT          = static_cast<Ph2_HwDescription::lpGBT*>(theOpticalGroup->flpGBT);
    auto theLpGBRInterface = fTheSystemController->flpGBTInterface;

    auto readTemperature = [theLpGBRInterface, theOpticalGroup, theLpGBT](const std::string& theNTCtype)
    {
        std::string sensorTemperatureADC = theOpticalGroup->getNTCMap().at(theNTCtype);
        // theLpGBRInterface->CdacSetCurrent(theLpGBT, sensorTemperatureADC, theLpGBRInterface->_CdacCodeToCurrent(theLpGBT, sensorTemperatureADC, 0xaa));
        float expectedROhm = theLpGBRInterface->GetLastNTCResistance(theLpGBT, theNTCtype);
        float resistance   = theLpGBRInterface->MeasureResistance(theLpGBT, sensorTemperatureADC, expectedROhm, false);
        theLpGBRInterface->SetLastNTCResistance(theLpGBT, theNTCtype, resistance);
        return NTChandler::getInstance().getTemperature(theNTCtype, resistance);
    };

    float monitorValue = -999.;
    if(std::regex_match(monitorValueName, std::regex("^ADC[0-7]$"))) { monitorValue = theLpGBRInterface->AdcGetVin(theLpGBT, monitorValueName, "VREF/2", 0); }
    else if(std::regex_match(monitorValueName, std::regex("^VDD.*"))) { monitorValue = theLpGBRInterface->MeasurePowerSupplyVoltage(theLpGBT, monitorValueName); }
    else if(monitorValueName == "LpGBTtemp") { monitorValue = theLpGBRInterface->MeasureTemperature(theLpGBT); }
    else if(monitorValueName == "SensorTemp") { monitorValue = readTemperature("Sensor"); }
    else if(monitorValueName == "VTRxTemp") { monitorValue = readTemperature("VTRx+"); }
    else if(monitorValueName == "1V25_Left") { monitorValue = theLpGBRInterface->AdcGetVin(theLpGBT, "ADC1", "VREF/2", 0) * (310. / 200.); }
    else if(monitorValueName == "VIN") { monitorValue = theLpGBRInterface->AdcGetVin(theLpGBT, "ADC2", "VREF/2", 0) * (95.6 / 4.7); }
    else if(monitorValueName == "VTRxLeakageCurr") { monitorValue = 2.5 - theLpGBRInterface->AdcGetVin(theLpGBT, "ADC5", "VREF/2", 0) * (146 / 47); }
    else if(monitorValueName == "BPOL2V5temp") { monitorValue = (theLpGBRInterface->AdcGetVin(theLpGBT, "ADC6", "VREF/2", 0) - 0.285) / 0.004; }
    else if(monitorValueName == "BPOL12Vtemp") { monitorValue = (theLpGBRInterface->AdcGetVin(theLpGBT, "ADC7", "VREF/2", 0) - 0.6976) / 0.00302; }
    else if(monitorValueName == "2V55") { monitorValue = theLpGBRInterface->AdcGetVin(theLpGBT, "ADC7", "VREF/2", 0) * (161. / 51.); }

    return monitorValue;
}

void OTMonitor::publishToMQTT(const std::string& payload)
{
    if(!fMQTTEnabled) { return; }

    // Escape double quotes in payload for shell command
    std::string escaped_payload = payload;
    size_t      pos             = 0;
    while((pos = escaped_payload.find("\"", pos)) != std::string::npos)
    {
        escaped_payload.replace(pos, 1, "\\\"");
        pos += 2;
    }

    // Construct mosquitto_pub command
    std::string command = "mosquitto_pub -h " + fMQTTBrokerHost + " -p " + std::to_string(fMQTTBrokerPort) + " -t \"" + fMQTTTopic + "\"" + " -m \"" + escaped_payload + "\"" +
                          " > /dev/null 2>&1 &"; // Run in background, suppress output
    // Execute the command
    int result = system(command.c_str());
    if(result != 0)
    {
        // Only log errors occasionally to avoid spam
        static int error_count = 0;
        if(error_count++ % 100 == 0) { LOG(DEBUG) << BOLDYELLOW << __PRETTY_FUNCTION__ << " MQTT publish failed (error count: " << error_count << ")" << RESET; }
    }
    fMQTTcounter++;
}
