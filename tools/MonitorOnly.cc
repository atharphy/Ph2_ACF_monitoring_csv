#include "tools/MonitorOnly.h"
#include "System/RegisterHelper.h"
#include "HWDescription/Definition.h"
#include "Utils/NTChandler.h"
#include "HWInterface/PSInterface.h"
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

using namespace Ph2_HwInterface;

std::string MonitorOnly::fCalibrationDescription = "Monitor only test with named pipe communication";

MonitorOnly::MonitorOnly() : Tool()
{
    fDataPipeName = "/tmp/monitor_data_pipe";
    fCommandPipeName = "/tmp/monitor_command_pipe";
    fKeepMonitoring.store(false);
    fPaused.store(false);
    
    // MQTT settings
    fMQTTBrokerHost = "cmslabserver";  // Default MQTT broker
    fMQTTBrokerPort = 1883;         // Default MQTT port
    fMQTTTopic = "/ph2acf/data";  // Default topic
    fMQTTEnabled.store(true);       // Enable MQTT by default
}

MonitorOnly::~MonitorOnly()
{
    cleanupNamedPipes();
}

void MonitorOnly::createNamedPipes()
{
    // Remove existing pipes if they exist
    unlink(fDataPipeName.c_str());
    unlink(fCommandPipeName.c_str());
    
    // Create named pipes
    if (mkfifo(fDataPipeName.c_str(), 0666) == -1) {
        std::cerr << "Error creating data pipe: " << fDataPipeName << std::endl;
        perror("mkfifo data");
    } else {
        std::cout << "Created data pipe: " << fDataPipeName << std::endl;
    }
    
    if (mkfifo(fCommandPipeName.c_str(), 0666) == -1) {
        std::cerr << "Error creating command pipe: " << fCommandPipeName << std::endl;
        perror("mkfifo command");
    } else {
        std::cout << "Created command pipe: " << fCommandPipeName << std::endl;
    }
}

void MonitorOnly::cleanupNamedPipes()
{
    // Close pipes if open
    if (fDataPipe.is_open()) {
        fDataPipe.close();
    }
    if (fCommandPipe.is_open()) {
        fCommandPipe.close();
    }
    
    // Remove named pipes
    unlink(fDataPipeName.c_str());
    unlink(fCommandPipeName.c_str());
}

void MonitorOnly::Running()
{
    std::cout << __PRETTY_FUNCTION__ << " Starting MonitorOnly test" << std::endl;
    
    // Create named pipes
    createNamedPipes();
    
    std::cout << "Temperature monitoring will start immediately." << std::endl;
    std::cout << "- MQTT publishing: " << (fMQTTEnabled.load() ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "- Named pipe: Available for readers at " << fDataPipeName << std::endl;
    std::cout << "- Command pipe: Available at " << fCommandPipeName << std::endl;
    
    fKeepMonitoring.store(true);
    fPaused.store(false);
    
    // Start command monitoring thread
    fCommandThread = std::thread(&MonitorOnly::monitorCommandPipe, this);
    
    // Main monitoring loop - write data to pipe
    writeDataToPipe();
    
    // Wait for command thread to finish
    if (fCommandThread.joinable()) {
        fCommandThread.join();
    }
    
    std::cout << __PRETTY_FUNCTION__ << " MonitorOnly test completed" << std::endl;
}

void MonitorOnly::writeDataToPipe()
{
    std::cout << "Starting temperature monitoring..." << std::endl;
    
    // Try to open data pipe for writing (non-blocking)
    bool pipe_available = false;
    
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
                        // Read LpGBT temperature
                        float lpgbtTemp = flpGBTInterface->MeasureTemperature(theLpGBT);
                        json_payload += ",\"LpGBT_OG" + std::to_string(opticalGroup->getId()) + "_temp\":" + std::to_string(lpgbtTemp);
                        
                        // Read LpGBT fuse ID
                        uint32_t lpgbtFuseId = flpGBTInterface->ReadChipFuseID(theLpGBT);
                        json_payload += ",\"LpGBT_OG" + std::to_string(opticalGroup->getId()) + "_fuseId\":" + std::to_string(lpgbtFuseId);
                        
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
                                                std::cout << "SSA chip " << chip->getId() << " temp: " << chipTemp << std::endl;
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
                                                std::cout << "MPA chip " << chip->getId() << " temp: " << chipTemp << std::endl;
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
            
            // For pipe output, use the old format for compatibility
            std::string data_line = "TEMP_DATA_" + std::to_string(counter) + 
                                  ": timestamp=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now().time_since_epoch()).count());
            
            // Convert JSON back to comma-separated format for pipe (legacy compatibility)
            std::string pipe_data = data_line;
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
                    fDataPipe.open(fDataPipeName, std::ios::out);
                    if (fDataPipe.is_open()) {
                        pipe_available = true;
                        std::cout << "Data pipe reader connected, enabling pipe output" << std::endl;
                    }
                }
            }
            
            // Write to pipe if available (using legacy format for compatibility)
            if (pipe_available && fDataPipe.is_open()) {
                fDataPipe << pipe_data;
                fDataPipe.flush();
                
                // Check if pipe is still open (reader disconnected)
                if (fDataPipe.fail()) {
                    fDataPipe.close();
                    pipe_available = false;
                    std::cout << "Data pipe reader disconnected, disabling pipe output" << std::endl;
                }
            }
            
            // Always publish JSON to MQTT
            publishToMQTT(json_payload);
            
            counter++;
            
            // Print periodic status
            if (counter % 10 == 0) {
                std::cout << "Sent " << counter << " temperature data packets" << std::endl;
            }
        }
        
        // Sleep for longer time for temperature monitoring
        usleep(1000000); // 1 second
    }
    
    std::cout << "Temperature monitoring stopped. Total packets sent: " << counter << std::endl;
    
    // Clean up pipe if it was opened
    if (fDataPipe.is_open()) {
        fDataPipe.close();
    }
}

void MonitorOnly::monitorCommandPipe()
{
    std::cout << "Starting command monitoring thread..." << std::endl;
    
    while (fKeepMonitoring.load()) {
        // Try to open command pipe for reading (non-blocking)
        fCommandPipe.open(fCommandPipeName, std::ios::in);
        
        if (fCommandPipe.is_open()) {
            std::cout << "Command pipe opened for reading" << std::endl;
            
            std::string command;
            while (fKeepMonitoring.load() && std::getline(fCommandPipe, command)) {
                if (!command.empty()) {
                    std::cout << "Received command: '" << command << "'" << std::endl;
                    
                    bool should_exit = processCommand(command);
                    if (should_exit) {
                        std::cout << "Exit command received, stopping monitoring..." << std::endl;
                        fKeepMonitoring.store(false);
                        break;
                    }
                }
            }
            
            fCommandPipe.close();
        }
        
        // Short sleep before trying to reopen
        usleep(100000); // 100ms
    }
    
    std::cout << "Command monitoring thread finished" << std::endl;
}

bool MonitorOnly::processCommand(const std::string& command)
{
    if (command == "exit" || command == "quit" || command == "stop") {
        return true; // Signal to exit
    }
    else if (command == "pause") {
        fPaused.store(true);
        std::cout << "Monitoring paused" << std::endl;
    }
    else if (command == "resume") {
        fPaused.store(false);
        std::cout << "Monitoring resumed" << std::endl;
    }
    else if (command == "status") {
        std::cout << "Status: " << (fPaused.load() ? "PAUSED" : "RUNNING") << std::endl;
        std::cout << "Named Pipe: " << (fDataPipe.is_open() ? "CONNECTED" : "DISCONNECTED") << std::endl;
        std::cout << "MQTT: " << (fMQTTEnabled.load() ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "MQTT Broker: " << fMQTTBrokerHost << ":" << fMQTTBrokerPort << std::endl;
        std::cout << "MQTT Topic: " << fMQTTTopic << std::endl;
    }
    else if (command == "mqtt_enable") {
        fMQTTEnabled.store(true);
        std::cout << "MQTT enabled" << std::endl;
    }
    else if (command == "mqtt_disable") {
        fMQTTEnabled.store(false);
        std::cout << "MQTT disabled" << std::endl;
    }
    else {
        std::cout << "Unknown command: " << command << std::endl;
        std::cout << "Available commands: exit, quit, stop, pause, resume, status, mqtt_enable, mqtt_disable" << std::endl;
    }
    
    return false; // Don't exit
}

void MonitorOnly::Stop()
{
    std::cout << __PRETTY_FUNCTION__ << " Stopping MonitorOnly" << std::endl;
    
    fKeepMonitoring.store(false);
    
    // Wait for command thread to finish
    if (fCommandThread.joinable()) {
        fCommandThread.join();
    }
    
    cleanupNamedPipes();
}

void MonitorOnly::Pause()
{
    std::cout << __PRETTY_FUNCTION__ << " Pausing MonitorOnly" << std::endl;
    fPaused.store(true);
}

void MonitorOnly::Resume()
{
    std::cout << __PRETTY_FUNCTION__ << " Resuming MonitorOnly" << std::endl;
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
            std::cerr << "MQTT publish failed (error count: " << error_count << ")" << std::endl;
        }
    }
}
