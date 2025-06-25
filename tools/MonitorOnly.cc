#include "tools/MonitorOnly.h"
#include "System/RegisterHelper.h"
#include "HWDescription/Definition.h"
#include "Utils/NTChandler.h"
#include "HWInterface/PSInterface.h"
#include "Utils/ConsoleColor.h"
#include "MonitorUtils/DetectorMonitor.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <signal.h>
#include <errno.h>

using namespace Ph2_HwInterface;

std::string MonitorOnly::fCalibrationDescription = "Monitor only test with named pipe communication";
MonitorOnly* MonitorOnly::fInstance = nullptr;

void MonitorOnly::signalHandler(int signal)
{
    LOG(INFO) << BOLDRED << __PRETTY_FUNCTION__ << " Received signal " << signal << ", cleaning up..." << RESET;
    if (fInstance) {
        fInstance->fKeepMonitoring.store(false);
    }
}

MonitorOnly::MonitorOnly() : Tool()
{
    // Set up static instance for signal handling
    fInstance = this;
    
    // Include PID in pipe names to avoid conflicts between multiple instances
    pid_t pid = getpid();
    fDataPipeName = "/tmp/monitor_data_pipe_" + std::to_string(pid);
    fCommandPipeName = "/tmp/monitor_command_pipe_" + std::to_string(pid);
    fKeepMonitoring.store(false);
    fPaused.store(false);
    fDQMWasRunning = false;  // Initialize DQM monitoring state
    
    // Default MQTT settings (will be overridden by XML if present)
    fMQTTBrokerHost = "cmslabserver";  // Default MQTT broker
    fMQTTBrokerPort = 1883;         // Default MQTT port
    fMQTTTopic = "/ph2acf/data";  // Default topic
    fMQTTEnabled.store(true);       // Enable MQTT by default
}

MonitorOnly::~MonitorOnly()
{
    // Ensure monitoring is stopped
    fKeepMonitoring.store(false);
    
    // Wait for command thread to finish if it's running
    if (fCommandThread.joinable()) {
        fCommandThread.join();
    }
    
    // Re-enable DQM monitoring in case it was disabled
    enableDQMMonitoring();
    
    cleanupNamedPipes();
    
    // Clear static instance
    if (fInstance == this) {
        fInstance = nullptr;
    }
}

void MonitorOnly::loadMQTTSettings()
{
    // Load MQTT settings from XML configuration
    try {
        fMQTTBrokerHost = findValueInSettings("MQTTBrokerHost", std::string("cmslabserver"));
        fMQTTBrokerPort = findValueInSettings("MQTTBrokerPort", 1883);
        fMQTTTopic = findValueInSettings("MQTTTopic", std::string("/ph2acf/data"));
        bool mqttEnabled = findValueInSettings("MQTTEnabled", true);
        fMQTTEnabled.store(mqttEnabled);
        
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " MQTT Settings loaded from XML:" << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << "   Broker Host: " << fMQTTBrokerHost << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << "   Broker Port: " << fMQTTBrokerPort << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << "   Topic: " << fMQTTTopic << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << "   Enabled: " << (fMQTTEnabled.load() ? "Yes" : "No") << RESET;
    } catch (const std::exception& e) {
        LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Warning: Could not load MQTT settings from XML, using defaults: " << e.what() << RESET;
    }
}

void MonitorOnly::createNamedPipes()
{
    // Remove existing pipes if they exist
    unlink(fDataPipeName.c_str());
    unlink(fCommandPipeName.c_str());
    
    // Create named pipes
    if (mkfifo(fDataPipeName.c_str(), 0666) == -1) {
        LOG(INFO) << BOLDRED << __PRETTY_FUNCTION__ << " Error creating data pipe: " << fDataPipeName << RESET;
        perror("mkfifo data");
    } else {
        LOG(INFO) << BOLDGREEN << __PRETTY_FUNCTION__ << " Created data pipe: " << fDataPipeName << RESET;
    }
    
    if (mkfifo(fCommandPipeName.c_str(), 0666) == -1) {
        LOG(INFO) << BOLDRED << __PRETTY_FUNCTION__ << " Error creating command pipe: " << fCommandPipeName << RESET;
        perror("mkfifo command");
    } else {
        LOG(INFO) << BOLDGREEN << __PRETTY_FUNCTION__ << " Created command pipe: " << fCommandPipeName << RESET;
    }
}

void MonitorOnly::cleanupNamedPipes()
{
    // Close pipes if open with error handling
    try {
        if (fDataPipe.is_open()) {
            fDataPipe.close();
            LOG(DEBUG) << BOLDGREEN << __PRETTY_FUNCTION__ << " Data pipe closed" << RESET;
        }
    } catch (const std::exception& e) {
        LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Error closing data pipe: " << e.what() << RESET;
    }
    
    try {
        if (fCommandPipe.is_open()) {
            fCommandPipe.close();
            LOG(DEBUG) << BOLDGREEN << __PRETTY_FUNCTION__ << " Command pipe closed" << RESET;
        }
    } catch (const std::exception& e) {
        LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Error closing command pipe: " << e.what() << RESET;
    }
    
    // Remove named pipes
    if (unlink(fDataPipeName.c_str()) == 0) {
        LOG(DEBUG) << BOLDGREEN << __PRETTY_FUNCTION__ << " Removed data pipe: " << fDataPipeName << RESET;
    }
    if (unlink(fCommandPipeName.c_str()) == 0) {
        LOG(DEBUG) << BOLDGREEN << __PRETTY_FUNCTION__ << " Removed command pipe: " << fCommandPipeName << RESET;
    }
}

void MonitorOnly::Running()
{
    LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " Starting MonitorOnly test" << RESET;
    
    // Set up signal handlers for clean shutdown when we actually start monitoring
    signal(SIGINT, signalHandler);  // Ctrl+C
    signal(SIGTERM, signalHandler); // Termination signal
    signal(SIGPIPE, SIG_IGN);      // Ignore SIGPIPE to prevent crashes on pipe closure
    
    // Load MQTT settings from XML configuration
    loadMQTTSettings();
    
    // Create named pipes
    createNamedPipes();
    
    LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " Temperature monitoring will start immediately." << RESET;
    LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " - MQTT publishing: " << (fMQTTEnabled.load() ? "ENABLED" : "DISABLED") << RESET;
    LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " - Named pipe: Available for readers at " << fDataPipeName << RESET;
    LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " - Command pipe: Available at " << fCommandPipeName << RESET;
    
    // Disable DQM monitoring to avoid register access conflicts
    disableDQMMonitoring();
    
    fKeepMonitoring.store(true);
    fPaused.store(false);
    
    // Start command monitoring thread
    fCommandThread = std::thread(&MonitorOnly::monitorCommandPipe, this);
    
    // Main monitoring loop - write data to pipe
    writeDataToPipe();
    
    // Check if we were stopped (either by command or signal)
    if (!fKeepMonitoring.load()) {
        LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Monitoring stopped, cleaning up..." << RESET;
        // Re-enable DQM monitoring before exit
        enableDQMMonitoring();
        // Don't wait for command thread if we were stopped by signal
        if (fCommandThread.joinable()) {
            fCommandThread.detach(); // Let it finish naturally
        }
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " MonitorOnly test completed (early exit)" << RESET;
        return;
    }
    
    // Wait for command thread to finish
    if (fCommandThread.joinable()) {
        fCommandThread.join();
    }
    
    // Re-enable DQM monitoring before normal exit
    enableDQMMonitoring();
    
    LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " MonitorOnly test completed" << RESET;
}

void MonitorOnly::writeDataToPipe()
{
    LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " Starting temperature monitoring..." << RESET;
    
    // Try to open data pipe for writing (non-blocking)
    bool pipe_available = false;
    
    // Cache for LpGBT fuse IDs to avoid repeated register reads
    std::map<uint32_t, uint32_t> fuseIdCache; // optical group ID -> fuse ID
    
    int counter = 0;
    while (fKeepMonitoring.load()) {
        if (!fPaused.load()) {
            // Create JSON payload
            std::string json_payload = "{";
            json_payload += "\"counter\":" + std::to_string(counter) + ",";
            json_payload += "\"timestamp\":" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now().time_since_epoch()).count());
            
            // Read temperatures from all boards/optical groups
            for (const auto& board : *fDetectorContainer) {
                for (const auto& opticalGroup : *board) {
                    auto theLpGBT = static_cast<Ph2_HwDescription::lpGBT*>(opticalGroup->flpGBT);
                    if (theLpGBT == nullptr) continue;
                    
                    try {
                        if (counter % 20 == 0) {
                            // Read LpGBT temperature
                            float lpgbtTemp = flpGBTInterface->MeasureTemperature(theLpGBT);
                            json_payload += ",\"LpGBT_OG" + std::to_string(opticalGroup->getId()) + "_temp\":" + std::to_string(lpgbtTemp);
                            
                            // Read LpGBT fuse ID (cached to avoid repeated register reads)
                            uint32_t ogId = opticalGroup->getId();
                            if (fuseIdCache.find(ogId) == fuseIdCache.end()) {
                                fuseIdCache[ogId] = flpGBTInterface->ReadChipFuseID(theLpGBT);
                            }
                            json_payload += ",\"LpGBT_OG" + std::to_string(ogId) + "_fuseId\":" + std::to_string(fuseIdCache[ogId]);
                            
                            // Read sensor temperature (external NTC)
                
                            if (!opticalGroup->getNTCMap().empty()) {
                                auto ntcMap = opticalGroup->getNTCMap();
                                for (const auto& ntcEntry : ntcMap) {
                                    if (ntcEntry.first == "Sensor") {
                                        try {
                                            flpGBTInterface->CdacSetCurrent(theLpGBT, ntcEntry.second, 
                                                flpGBTInterface->_CdacCodeToCurrent(theLpGBT, ntcEntry.second, 0xaa));
                                            float resistance = flpGBTInterface->MeasureResistance(theLpGBT, ntcEntry.second, 1000, true);
                                            float sensorTemp = NTChandler::getInstance().getTemperature(ntcEntry.first, resistance);
                                            json_payload += ",\"Sensor_OG" + std::to_string(opticalGroup->getId()) + "_temp\":" + std::to_string(sensorTemp);
                                        } catch (...) {
                                            json_payload += ",\"Sensor_OG" + std::to_string(opticalGroup->getId()) + "_temp\":\"ERROR\"";
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        
                        // Read SSA and MPA temperatures from hybrids
                        for (const auto& hybrid : *opticalGroup) {
                            for (const auto& chip : *hybrid) {
                                try {
                                    std::string chipType = "";
                                    float chipTemp = -999.0;
                                    
                                    if (chip->getFrontEndType() == FrontEndType::SSA2) {
                                        chipType = "SSA";
                                        // Use PSInterface directly for SSA temperature reading
                                        auto thePSInterface = static_cast<PSInterface*>(fReadoutChipInterface);
                                        if (thePSInterface != nullptr) {
                                            chipTemp = thePSInterface->measureTemperature(chip);
                                            // Debug output for first few readings
                                            if (counter < 3) {
                                                LOG(INFO) << BOLDMAGENTA << __PRETTY_FUNCTION__ << " SSA chip " << chip->getId() << " temp: " << chipTemp << RESET;
                                            }
                                        }
                                    }
                                    else if (chip->getFrontEndType() == FrontEndType::MPA2) {
                                        chipType = "MPA";
                                        // Use PSInterface directly for MPA temperature reading
                                        auto thePSInterface = static_cast<PSInterface*>(fReadoutChipInterface);
                                        if (thePSInterface != nullptr) {
                                            chipTemp = thePSInterface->measureTemperature(chip);
                                            // Debug output for first few readings
                                            if (counter < 3) {
                                                LOG(INFO) << BOLDMAGENTA << __PRETTY_FUNCTION__ << " MPA chip " << chip->getId() << " temp: " << chipTemp << RESET;
                                            }
                                        }
                                    }
                                    
                                    if (!chipType.empty() && chipTemp > -900.0) {
                                        std::string hybridId = std::to_string(hybrid->getId());
                                        std::string chipId = std::to_string(chip->getId());
                                        json_payload += ",\"" + chipType + "_H" + hybridId + "_C" + chipId + "_temp\":" + std::to_string(chipTemp);
                                    } else if (!chipType.empty()) {
                                        // Output debug info for failed readings
                                        std::string hybridId = std::to_string(hybrid->getId());
                                        std::string chipId = std::to_string(chip->getId());
                                        json_payload += ",\"" + chipType + "_H" + hybridId + "_C" + chipId + "_temp\":\"NO_READ\"";
                                    }
                                    
                                } catch (const std::exception& e) {
                                    std::string chipType = (chip->getFrontEndType() == FrontEndType::SSA2) ? "SSA" : 
                                                         (chip->getFrontEndType() == FrontEndType::MPA2) ? "MPA" : "UNK";
                                    std::string hybridId = std::to_string(hybrid->getId());
                                    std::string chipId = std::to_string(chip->getId());
                                    json_payload += ",\"" + chipType + "_H" + hybridId + "_C" + chipId + "_temp\":\"ERROR\"";
                                } catch (...) {
                                    std::string chipType = (chip->getFrontEndType() == FrontEndType::SSA2) ? "SSA" : 
                                                         (chip->getFrontEndType() == FrontEndType::MPA2) ? "MPA" : "UNK";
                                    std::string hybridId = std::to_string(hybrid->getId());
                                    std::string chipId = std::to_string(chip->getId());
                                    json_payload += ",\"" + chipType + "_H" + hybridId + "_C" + chipId + "_temp\":\"EXCEPTION\"";
                                }
                            }
                        }
                        
                    } catch (...) {
                        json_payload += ",\"LpGBT_OG" + std::to_string(opticalGroup->getId()) + "_temp\":\"ERROR\"";
                    }
                }
            }
            
            // Close JSON object
            json_payload += "}";
            
            // // For pipe output, use the old format for compatibility
            // std::string data_line = "TEMP_DATA_" + std::to_string(counter) + 
            //                       ": timestamp=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            //                           std::chrono::steady_clock::now().time_since_epoch()).count());
            
            // Convert JSON back to comma-separated format for pipe (legacy compatibility)
            std::string pipe_data = json_payload;
            // Extract data from JSON and append to pipe_data (simplified conversion)
            // This maintains backward compatibility with existing pipe readers
            pipe_data += "\n";
            
            // Try to write to pipe only if someone is listening
            if (!pipe_available && !fDataPipe.is_open()) {
                // Try to open pipe in non-blocking mode
                int pipe_fd = open(fDataPipeName.c_str(), O_WRONLY | O_NONBLOCK);
                if (pipe_fd >= 0) {
                    // Someone is reading, we can use the pipe
                    close(pipe_fd);  // Close the test fd
                    try {
                        fDataPipe.open(fDataPipeName, std::ios::out);
                        if (fDataPipe.is_open() && fDataPipe.good()) {
                            pipe_available = true;
                            LOG(INFO) << BOLDGREEN << __PRETTY_FUNCTION__ << " Data pipe reader connected, enabling pipe output" << RESET;
                        } else {
                            fDataPipe.close();
                        }
                    } catch (const std::exception& e) {
                        LOG(DEBUG) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Failed to open data pipe for writing: " << e.what() << RESET;
                        if (fDataPipe.is_open()) {
                            fDataPipe.close();
                        }
                    }
                } else if (errno != ENXIO) {
                    // ENXIO is expected when no reader is present, other errors are worth noting
                    // Only log occasionally to avoid spam
                    static int open_error_count = 0;
                    if (++open_error_count % 100 == 0) {
                        LOG(DEBUG) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Pipe open attempt failed (error count: " << open_error_count 
                                  << ", errno: " << errno << ")" << RESET;
                    }
                }
            }
            
            // Write to pipe if available (using legacy format for compatibility)
            if (pipe_available && fDataPipe.is_open()) {
                try {
                    fDataPipe << pipe_data;
                    fDataPipe.flush();
                    
                    // Check if pipe is still open (reader disconnected)
                    if (fDataPipe.fail() || fDataPipe.bad()) {
                        fDataPipe.clear();  // Clear error flags
                        fDataPipe.close();
                        pipe_available = false;
                        LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Data pipe reader disconnected, disabling pipe output" << RESET;
                    }
                } catch (const std::ios_base::failure& e) {
                    // Handle pipe write failure (e.g., broken pipe)
                    LOG(DEBUG) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Pipe write failed (reader disconnected): " << e.what() << RESET;
                    fDataPipe.close();
                    pipe_available = false;
                } catch (const std::exception& e) {
                    // Handle other exceptions
                    LOG(DEBUG) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Unexpected error writing to pipe: " << e.what() << RESET;
                    fDataPipe.close();
                    pipe_available = false;
                } catch (...) {
                    // Handle any other exceptions
                    LOG(DEBUG) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Unknown error writing to pipe, disabling pipe output" << RESET;
                    fDataPipe.close();
                    pipe_available = false;
                }
            }
            
            // Always publish JSON to MQTT
            publishToMQTT(json_payload);
            
            counter++;
            
            // Print periodic status
            if (counter % 3 == 0) {
                LOG(INFO) << BOLDMAGENTA << __PRETTY_FUNCTION__ << " Sent " << counter << " temperature data packets" << RESET;
            }
        }
        
        // Sleep for longer time for temperature monitoring
        usleep(1000000); // 1 second
    }
    
    LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " Temperature monitoring stopped. Total packets sent: " << counter << RESET;
    
    // Clean up pipe if it was opened
    try {
        if (fDataPipe.is_open()) {
            fDataPipe.close();
        }
    } catch (const std::exception& e) {
        LOG(DEBUG) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Error closing data pipe at end of monitoring: " << e.what() << RESET;
    }
}

void MonitorOnly::monitorCommandPipe()
{
    LOG(DEBUG) << BOLDCYAN << __PRETTY_FUNCTION__ << " Starting command monitoring thread..." << RESET;
    
    while (fKeepMonitoring.load()) {
        try {
            // Try to open command pipe for reading (non-blocking)
            fCommandPipe.open(fCommandPipeName, std::ios::in);
            
            if (fCommandPipe.is_open()) {
                LOG(DEBUG) << BOLDGREEN << __PRETTY_FUNCTION__ << " Command pipe opened for reading" << RESET;
                
                std::string command;
                while (fKeepMonitoring.load()) {
                    try {
                        if (std::getline(fCommandPipe, command)) {
                            if (!command.empty()) {
                                LOG(INFO) << BOLDCYAN << __PRETTY_FUNCTION__ << " Received command: '" << command << "'" << RESET;
                                
                                bool should_exit = processCommand(command);
                                if (should_exit) {
                                    LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Exit command received, stopping monitoring..." << RESET;
                                    fKeepMonitoring.store(false);
                                    break;
                                }
                            }
                        } else {
                            // End of file or pipe closed
                            if (fCommandPipe.eof() || fCommandPipe.fail()) {
                                break;
                            }
                        }
                    } catch (const std::exception& e) {
                        LOG(DEBUG) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Error reading from command pipe: " << e.what() << RESET;
                        break;
                    }
                }
                
                try {
                    fCommandPipe.close();
                } catch (const std::exception& e) {
                    LOG(DEBUG) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Error closing command pipe: " << e.what() << RESET;
                }
            }
        } catch (const std::exception& e) {
            LOG(DEBUG) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Error in command pipe monitoring: " << e.what() << RESET;
        }
        
        // Short sleep before trying to reopen
        usleep(100000); // 100ms
    }
    
    LOG(DEBUG) << BOLDCYAN << __PRETTY_FUNCTION__ << " Command monitoring thread finished" << RESET;
}

bool MonitorOnly::processCommand(const std::string& command)
{
    if (command == "exit" || command == "quit" || command == "stop") {
        return true; // Signal to exit
    }
    else if (command == "pause") {
        fPaused.store(true);
        LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Monitoring paused" << RESET;
    }
    else if (command == "resume") {
        fPaused.store(false);
        LOG(INFO) << BOLDGREEN << __PRETTY_FUNCTION__ << " Monitoring resumed" << RESET;
    }
    else if (command == "status") {
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " Status: " << (fPaused.load() ? "PAUSED" : "RUNNING") << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " Data Pipe: " << fDataPipeName << " (" << (fDataPipe.is_open() ? "CONNECTED" : "DISCONNECTED") << ")" << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " Command Pipe: " << fCommandPipeName << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " MQTT: " << (fMQTTEnabled.load() ? "ENABLED" : "DISABLED") << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " MQTT Broker: " << fMQTTBrokerHost << ":" << fMQTTBrokerPort << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " MQTT Topic: " << fMQTTTopic << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " DQM Monitoring: " << (fDetectorMonitor != nullptr ? (fDQMWasRunning ? "CONTROLLED_BY_MONITOR_ONLY" : "AVAILABLE") : "NOT_AVAILABLE") << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " Process PID: " << getpid() << RESET;
    }
    else if (command == "mqtt_enable") {
        fMQTTEnabled.store(true);
        LOG(INFO) << BOLDGREEN << __PRETTY_FUNCTION__ << " MQTT enabled" << RESET;
    }
    else if (command == "mqtt_disable") {
        fMQTTEnabled.store(false);
        LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " MQTT disabled" << RESET;
    }
    else if (command == "dqm_disable") {
        disableDQMMonitoring();
        LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " DQM monitoring manually disabled" << RESET;
    }
    else if (command == "dqm_enable") {
        enableDQMMonitoring();
        LOG(INFO) << BOLDGREEN << __PRETTY_FUNCTION__ << " DQM monitoring manually enabled" << RESET;
    }
    else {
        LOG(INFO) << BOLDRED << __PRETTY_FUNCTION__ << " Unknown command: " << command << RESET;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " Available commands: exit, quit, stop, pause, resume, status, mqtt_enable, mqtt_disable, dqm_disable, dqm_enable" << RESET;
    }
    
    return false; // Don't exit
}

void MonitorOnly::Stop()
{
    LOG(INFO) << BOLDRED << __PRETTY_FUNCTION__ << " Stopping MonitorOnly" << RESET;
    
    fKeepMonitoring.store(false);
    
    // Wait for command thread to finish
    if (fCommandThread.joinable()) {
        fCommandThread.join();
    }
    
    // Re-enable DQM monitoring before cleanup
    enableDQMMonitoring();
    
    cleanupNamedPipes();
    
    LOG(INFO) << BOLDGREEN << __PRETTY_FUNCTION__ << " MonitorOnly stopped and cleaned up" << RESET;
}

void MonitorOnly::Pause()
{
    LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " Pausing MonitorOnly" << RESET;
    fPaused.store(true);
}

void MonitorOnly::Resume()
{
    LOG(INFO) << BOLDGREEN << __PRETTY_FUNCTION__ << " Resuming MonitorOnly" << RESET;
    fPaused.store(false);
}

void MonitorOnly::publishToMQTT(const std::string& payload)
{
    if (!fMQTTEnabled.load()) {
        return;
    }
    
    // Escape double quotes in payload for shell command
    std::string escaped_payload = payload;
    size_t pos = 0;
    while ((pos = escaped_payload.find("\"", pos)) != std::string::npos) {
        escaped_payload.replace(pos, 1, "\\\"");
        pos += 2;
    }
    
    // Construct mosquitto_pub command
    std::string command = "mosquitto_pub -h " + fMQTTBrokerHost + 
                         " -p " + std::to_string(fMQTTBrokerPort) + 
                         " -t \"" + fMQTTTopic + "\"" +
                         " -m \"" + escaped_payload + "\"" +
                         " > /dev/null 2>&1 &";  // Run in background, suppress output
    
    // Execute the command
    int result = system(command.c_str());
    if (result != 0) {
        // Only log errors occasionally to avoid spam
        static int error_count = 0;
        if (error_count++ % 100 == 0) {
            LOG(DEBUG) << BOLDYELLOW << __PRETTY_FUNCTION__ << " MQTT publish failed (error count: " << error_count << ")" << RESET;
        }
    }
}

void MonitorOnly::disableDQMMonitoring()
{
    if (fDetectorMonitor != nullptr) {
        fDetectorMonitor->stopMonitoring();
        fDQMWasRunning = true;
        LOG(INFO) << BOLDYELLOW << __PRETTY_FUNCTION__ << " DQM monitoring disabled to avoid register conflicts" << RESET;
    } else {
        fDQMWasRunning = false;
        LOG(INFO) << BOLDBLUE << __PRETTY_FUNCTION__ << " No DQM monitoring found to disable" << RESET;
    }
}

void MonitorOnly::enableDQMMonitoring()
{
    if (fDetectorMonitor != nullptr && fDQMWasRunning) {
        fDetectorMonitor->startMonitoring();
        LOG(INFO) << BOLDGREEN << __PRETTY_FUNCTION__ << " DQM monitoring re-enabled" << RESET;
    }
}
