#ifndef PS_MONITOR_H
#define PS_MONITOR_H

#include "MonitorUtils/OTMonitor.h"
#include "Utils/ValueAndTime.h"

class PSMonitor : public OTMonitor
{
  public:
    PSMonitor(Ph2_System::SystemController* theSystemController, const DetectorMonitorConfig& theDetectorMonitorConfig);

  protected:
    void runMonitor() override;

  private:
    void                runMonitorSSA(const std::string& monitorValueName);
    void                runMonitorMPA(const std::string& monitorValueName);
    ValueAndTime<float> readChipMonitorValue(const std::string& monitorValueName, Ph2_HwDescription::ReadoutChip* theChip, DetectorDataContainer& theDataContainer) override;

    // // MQTT functionality
    // void publishToMQTT(const std::string& payload);
    // // settings for MQTT publishing of monitored values
    // // if it works, implement method to read settings from xml
    // std::string       fMQTTBrokerHost = "cmslabserver";
    // int               fMQTTBrokerPort = 1883;
    // std::string       fMQTTTopic = "/ph2acf/data";
    // std::atomic<bool> fMQTTEnabled = true;
    // int fCounter = 0;
    // // Cache for LpGBT fuse IDs to avoid repeated register reads
    // std::map<uint32_t, uint32_t> fFuseIdCache; // optical group ID -> fuse ID
};

#endif
