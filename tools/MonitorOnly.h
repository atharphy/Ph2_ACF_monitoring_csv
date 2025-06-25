#ifndef __MONITOR_ONLY__
#define __MONITOR_ONLY__

#include "tools/Tool.h"
#include <string>
#include <thread>
#include <atomic>
#include <fstream>

class MonitorOnly : public Tool
{
  public:
    MonitorOnly();
    ~MonitorOnly();

    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;

    static std::string fCalibrationDescription;
    
    // Signal handler for cleanup on termination
    static void signalHandler(int signal);
    static MonitorOnly* fInstance;

  private:
    void createNamedPipes();
    void cleanupNamedPipes();
    void monitorCommandPipe();
    void writeDataToPipe();
    bool processCommand(const std::string& command);
    void loadMQTTSettings();
    
    // MQTT functionality
    void publishToMQTT(const std::string& payload);

    std::string fDataPipeName;
    std::string fCommandPipeName;
    
    std::thread fCommandThread;
    std::atomic<bool> fKeepMonitoring;
    std::atomic<bool> fPaused;
    
    std::ofstream fDataPipe;
    std::ifstream fCommandPipe;
    
    // MQTT settings
    std::string fMQTTBrokerHost;
    int fMQTTBrokerPort;
    std::string fMQTTTopic;
    std::atomic<bool> fMQTTEnabled;
};

#endif
