#ifndef __MONITOR_OT_H__
#define __MONITOR_OT_H__

#include "MonitorUtils/DetectorMonitor.h"
#include "Utils/ValueAndTime.h"
#include <atomic>

namespace Ph2_HwDescription
{
class lpGBT;
}
class OTMonitor : public DetectorMonitor
{
  public:
    OTMonitor(const Ph2_System::SystemController* theSystemController, const DetectorMonitorConfig& theDetectorMonitorConfig);

  protected:
    virtual void                runMonitor() override;
    void                        runMonitorLpGBT(const std::string& monitorValueName);
    virtual ValueAndTime<float> readChipMonitorValue(const std::string& monitorValueName, Ph2_HwDescription::ReadoutChip* theChip, DetectorDataContainer& theDataContainer) = 0;
    DetectorDataContainer       getReadoutChipMonitorValues(const std::string& monitorValueName, FrontEndType theFrontEndType);
    void                        publishToMQTT(const std::string& payload);

  private:
    float readLpGBTmonitorValue(Ph2_HwDescription::OpticalGroup* theOpticalGroup, const std::string& monitorValueName);

    // MQTT settings
    std::string fMQTTBrokerHost;
    int         fMQTTBrokerPort;
    std::string fMQTTTopic;
    bool        fMQTTEnabled;
    // Cache for LpGBT fuse IDs to avoid repeated register reads
    std::map<uint32_t, uint32_t> fFuseIdCache; // optical group ID -> fuse ID
    // counter for MQTT payloads
    uint64_t fMQTTcounter = 0;
};

#endif