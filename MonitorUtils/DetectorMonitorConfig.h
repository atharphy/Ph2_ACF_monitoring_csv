#ifndef DETECTOR_MONITOR_CONFIG_H
#define DETECTOR_MONITOR_CONFIG_H

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

struct DetectorMonitorConfig
{
    DetectorMonitorConfig()
    {
        fMonitorElementList["Board"]       = {};
        fMonitorElementList["LpGBT"]       = {};
        fMonitorElementList["CIC"]         = {};
        fMonitorElementList["MPA"]         = {};
        fMonitorElementList["SSA"]         = {};
        fMonitorElementList["CBC"]         = {};
        fMonitorElementList["RD53"]        = {};
        fMonitorElementList["CROC"]        = {};
        fMonitorElementList["PowerSupply"] = {};
        fMonitorElementList["TestCard"]    = {};
    }
    int fSleepTimeMs;

    void addElementToMonitor(const std::string& chipName, const std::string& registerName)
    {
        if(fMonitorElementList.find(chipName) == fMonitorElementList.end())
        {
            std::string exceptionMessage = "Error: cannot monitor chip type " + chipName + ". Chip type allowed: ";
            for(const auto& monitor: fMonitorElementList) exceptionMessage += (monitor.first + " ");
            exceptionMessage += "\n";
            throw std::runtime_error(exceptionMessage);
        }
        fMonitorElementList.at(chipName).emplace_back(registerName);
        ++fNumberOfMonitoredRegister;
    }

    uint16_t getNumberOfMonitoredRegisters() const { return fNumberOfMonitoredRegister; }

    std::map<std::string, std::vector<std::string>> fMonitorElementList;
    uint16_t                                        fNumberOfMonitoredRegister = 0;
};

#endif
