#include "MonitorUtils/MonitoringCsvWriter.h"
#include "Utils/ConsoleColor.h"
#include "Utils/RD53Shared.h"
#include "Utils/RD53RunProgress.h"
#include "Utils/easylogging++.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{
std::string trim(const std::string& value)
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) return "";
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

bool parseBoolean(const std::string& value, const std::string& key, const std::string& configPath, size_t lineNumber)
{
    const std::string normalized = toLower(trim(value));
    if(normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") return true;
    if(normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") return false;
    throw std::runtime_error("[MonitoringCsvWriter] Invalid boolean for '" + key + "' at " + configPath + ":" + std::to_string(lineNumber));
}

unsigned int parseNonNegativeInteger(const std::string& value, const std::string& key, const std::string& configPath, size_t lineNumber)
{
    size_t parsedCharacters = 0;
    unsigned long parsedValue = 0;
    try
    {
        parsedValue = std::stoul(value, &parsedCharacters);
    }
    catch(const std::exception&)
    {
        throw std::runtime_error("[MonitoringCsvWriter] Invalid integer for '" + key + "' at " + configPath + ":" + std::to_string(lineNumber));
    }
    if(parsedCharacters != value.size() || parsedValue > std::numeric_limits<unsigned int>::max())
        throw std::runtime_error("[MonitoringCsvWriter] Invalid integer for '" + key + "' at " + configPath + ":" + std::to_string(lineNumber));
    return static_cast<unsigned int>(parsedValue);
}

double parseNonNegativeDouble(const std::string& value, const std::string& key, const std::string& configPath, size_t lineNumber)
{
    size_t parsedCharacters = 0;
    double parsedValue = 0;
    try
    {
        parsedValue = std::stod(value, &parsedCharacters);
    }
    catch(const std::exception&)
    {
        throw std::runtime_error("[MonitoringCsvWriter] Invalid number for '" + key + "' at " + configPath + ":" + std::to_string(lineNumber));
    }
    if(parsedCharacters != value.size() || parsedValue < 0 || !std::isfinite(parsedValue))
        throw std::runtime_error("[MonitoringCsvWriter] Invalid number for '" + key + "' at " + configPath + ":" + std::to_string(lineNumber));
    return parsedValue;
}

std::vector<std::string> split(const std::string& value, char delimiter, bool keepEmpty = true)
{
    std::vector<std::string> entries;
    size_t start = 0;
    while(start <= value.size())
    {
        const size_t separator = value.find(delimiter, start);
        const std::string entry = trim(value.substr(start, separator == std::string::npos ? std::string::npos : separator - start));
        if(keepEmpty || !entry.empty()) entries.push_back(entry);
        if(separator == std::string::npos) break;
        start = separator + 1;
    }
    return entries;
}

std::vector<std::pair<double, double>> parseWindows(const std::string& value, const std::string& key, const std::string& configPath, size_t lineNumber)
{
    std::vector<std::pair<double, double>> windows;
    for(const auto& entry: split(value, ',', false))
    {
        const auto separator = entry.find('-');
        if(separator == std::string::npos)
            throw std::runtime_error("[MonitoringCsvWriter] Expected START-END for '" + key + "' at " + configPath + ":" + std::to_string(lineNumber));
        const double start = parseNonNegativeDouble(trim(entry.substr(0, separator)), key, configPath, lineNumber);
        const double end = parseNonNegativeDouble(trim(entry.substr(separator + 1)), key, configPath, lineNumber);
        if(start >= end) throw std::runtime_error("[MonitoringCsvWriter] Window start must be smaller than end for '" + key + "'");
        windows.emplace_back(start, end);
    }
    return windows;
}

std::string baseDirectory()
{
    const char* base = std::getenv("PH2ACF_BASE_DIR");
    return base == nullptr ? std::string(".") : std::string(base);
}

std::string absolutePath(const std::string& value)
{
    if(value.empty() || value.front() == '/') return value;
    return baseDirectory() + "/" + value;
}

std::string workingDirectory()
{
    char path[4096];
    if(getcwd(path, sizeof(path)) == nullptr) throw std::runtime_error("[MonitoringCsvWriter] Cannot determine the working directory");
    return path;
}

std::string absoluteInputPath(const std::string& value)
{
    if(value.empty() || value.front() == '/') return value;
    return workingDirectory() + "/" + value;
}

std::string parentDirectory(const std::string& path)
{
    const size_t separator = path.find_last_of('/');
    if(separator == std::string::npos) return ".";
    return separator == 0 ? "/" : path.substr(0, separator);
}

std::string fileStem(const std::string& path)
{
    const size_t separator = path.find_last_of('/');
    const size_t start = separator == std::string::npos ? 0 : separator + 1;
    const size_t extension = path.find_last_of('.');
    return path.substr(start, extension == std::string::npos || extension < start ? std::string::npos : extension - start);
}

std::string outputPath(const std::string& value, const std::string& xmlDirectory)
{
    if(value.empty() || value.front() == '/') return value;
    return xmlDirectory + "/" + value;
}

bool fileExists(const std::string& path)
{
    struct stat status{};
    return stat(path.c_str(), &status) == 0;
}

void ensureDirectory(const std::string& path)
{
    std::string current;
    for(size_t index = 0; index < path.size(); ++index)
    {
        current += path[index];
        if(path[index] != '/' || current.size() == 1) continue;
        mkdir(current.c_str(), 0755);
    }
    if(mkdir(path.c_str(), 0755) != 0 && !fileExists(path)) throw std::runtime_error("Cannot create directory " + path);
}

std::string safeName(std::string value)
{
    for(char& character: value)
        if(!std::isalnum(static_cast<unsigned char>(character)) && character != '-' && character != '_') character = '_';
    return value.empty() ? "monitoring" : value;
}

std::string dateTimeStamp(const char* format)
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utcTime{};
    gmtime_r(&time, &utcTime);
    std::ostringstream output;
    output << std::put_time(&utcTime, format);
    return output.str();
}

class VirtualExpressionParser
{
  public:
    VirtualExpressionParser(const std::string& expression, const std::map<std::string, double>& values, size_t expectedWildcardValues = 0)
        : fExpression(expression), fValues(values), fExpectedWildcardValues(expectedWildcardValues)
    {
    }

    bool evaluate(double& result)
    {
        fPosition = 0;
        if(!parseExpression(result)) return false;
        skipWhitespace();
        return fPosition == fExpression.size() && std::isfinite(result);
    }

  private:
    bool parseExpression(double& result)
    {
        if(!parseTerm(result)) return false;

        while(true)
        {
            skipWhitespace();
            if(fPosition >= fExpression.size() || (fExpression[fPosition] != '+' && fExpression[fPosition] != '-')) return true;

            const char operation = fExpression[fPosition++];
            double     right     = 0;
            if(!parseTerm(right)) return false;
            result = (operation == '+' ? result + right : result - right);
        }
    }

    bool parseTerm(double& result)
    {
        if(!parseFactor(result)) return false;

        while(true)
        {
            skipWhitespace();
            if(fPosition >= fExpression.size() || (fExpression[fPosition] != '*' && fExpression[fPosition] != '/')) return true;

            const char operation = fExpression[fPosition++];
            double     right     = 0;
            if(!parseFactor(right)) return false;
            if(operation == '/' && right == 0) return false;
            result = (operation == '*' ? result * right : result / right);
        }
    }

    bool parseFactor(double& result)
    {
        skipWhitespace();
        if(fPosition >= fExpression.size()) return false;

        if(fExpression[fPosition] == '+' || fExpression[fPosition] == '-')
        {
            const bool negate = fExpression[fPosition++] == '-';
            if(!parseFactor(result)) return false;
            if(negate) result = -result;
            return true;
        }

        if(fExpression[fPosition] == '(')
        {
            ++fPosition;
            if(!parseExpression(result)) return false;
            skipWhitespace();
            if(fPosition >= fExpression.size() || fExpression[fPosition] != ')') return false;
            ++fPosition;
            return true;
        }

        if(fExpression[fPosition] == '"' || std::isalpha(static_cast<unsigned char>(fExpression[fPosition])) || fExpression[fPosition] == '_')
            return parseRegister(result);

        return parseNumber(result);
    }

    bool parseRegister(double& result)
    {
        std::string registerName;
        if(!parseRegisterName(registerName)) return false;

        skipWhitespace();
        if((registerName == "sum" || registerName == "avg") && fPosition < fExpression.size() && fExpression[fPosition] == '(')
            return parseAggregateFunction(registerName, result);

        if(fPosition < fExpression.size() && fExpression[fPosition] == '[')
        {
            ++fPosition;
            skipWhitespace();
            const size_t chipIdStart = fPosition;
            while(fPosition < fExpression.size() && std::isdigit(static_cast<unsigned char>(fExpression[fPosition]))) ++fPosition;
            if(chipIdStart == fPosition) return false;
            const std::string chipId = fExpression.substr(chipIdStart, fPosition - chipIdStart);
            skipWhitespace();
            if(fPosition >= fExpression.size() || fExpression[fPosition] != ']') return false;
            ++fPosition;
            registerName += "[" + chipId + "]";
        }

        const auto valueIt = fValues.find(registerName);
        if(valueIt == fValues.end()) return false;
        result = valueIt->second;
        return true;
    }

    bool parseRegisterName(std::string& registerName)
    {
        if(fPosition >= fExpression.size()) return false;
        if(fExpression[fPosition] == '"')
        {
            const size_t start = ++fPosition;
            while(fPosition < fExpression.size() && fExpression[fPosition] != '"') ++fPosition;
            if(fPosition >= fExpression.size()) return false;
            registerName = fExpression.substr(start, fPosition - start);
            ++fPosition;
            return true;
        }

        if(!std::isalpha(static_cast<unsigned char>(fExpression[fPosition])) && fExpression[fPosition] != '_') return false;
        const size_t start = fPosition;
        while(fPosition < fExpression.size() &&
              (std::isalnum(static_cast<unsigned char>(fExpression[fPosition])) || fExpression[fPosition] == '_'))
            ++fPosition;
        registerName = fExpression.substr(start, fPosition - start);
        return true;
    }

    bool parseAggregateFunction(const std::string& functionName, double& result)
    {
        ++fPosition;
        double valueSum   = 0;
        size_t valueCount = 0;

        while(true)
        {
            skipWhitespace();
            std::string registerName;
            if(!parseRegisterName(registerName)) return false;
            skipWhitespace();
            if(fPosition >= fExpression.size() || fExpression[fPosition] != '[') return false;
            ++fPosition;
            skipWhitespace();
            if(fPosition >= fExpression.size() || fExpression[fPosition] != '*') return false;
            ++fPosition;
            skipWhitespace();
            if(fPosition >= fExpression.size() || fExpression[fPosition] != ']') return false;
            ++fPosition;

            const std::string prefix = registerName + "[";
            size_t            registerValueCount = 0;
            for(const auto& value: fValues)
            {
                if(value.first.size() <= prefix.size() || value.first.compare(0, prefix.size(), prefix) != 0 || value.first.back() != ']') continue;
                valueSum += value.second;
                ++registerValueCount;
            }
            if(registerValueCount == 0 || (fExpectedWildcardValues != 0 && registerValueCount != fExpectedWildcardValues)) return false;
            valueCount += registerValueCount;

            skipWhitespace();
            if(fPosition >= fExpression.size()) return false;
            if(fExpression[fPosition] == ')')
            {
                ++fPosition;
                break;
            }
            if(fExpression[fPosition] != ',') return false;
            ++fPosition;
        }

        result = functionName == "avg" ? valueSum / static_cast<double>(valueCount) : valueSum;
        return std::isfinite(result);
    }

    bool parseNumber(double& result)
    {
        const char* start = fExpression.c_str() + fPosition;
        char*       end   = nullptr;
        result            = std::strtod(start, &end);
        if(end == start) return false;
        fPosition += static_cast<size_t>(end - start);
        return std::isfinite(result);
    }

    void skipWhitespace()
    {
        while(fPosition < fExpression.size() && std::isspace(static_cast<unsigned char>(fExpression[fPosition]))) ++fPosition;
    }

    const std::string&                   fExpression;
    const std::map<std::string, double>& fValues;
    size_t                          fPosition{0};
    size_t                          fExpectedWildcardValues{0};
};
}

MonitoringCsvWriter& MonitoringCsvWriter::getInstance()
{
    static MonitoringCsvWriter instance;
    return instance;
}

MonitoringCsvWriter::~MonitoringCsvWriter() { stop(); }

MonitoringCsvWriter::Configuration MonitoringCsvWriter::loadConfiguration()
{
    Configuration configuration;
    std::vector<std::pair<double, double>> percentWindows;
    std::map<std::string, std::vector<std::pair<double, double>>> calibrationPercentWindows;
    const char* configuredPath = std::getenv("CMSIT_MONITORING_CONFIG");
    const std::string configPath = configuredPath == nullptr ? baseDirectory() + "/settings/monitoring_settings.conf" : configuredPath;
    std::ifstream configFile(configPath);
    if(!configFile.is_open())
    {
        if(configuredPath != nullptr) throw std::runtime_error("[MonitoringCsvWriter] Cannot open " + configPath);
        return configuration;
    }

    std::string line;
    size_t lineNumber = 0;
    while(std::getline(configFile, line))
    {
        ++lineNumber;
        const auto comment = line.find('#');
        if(comment != std::string::npos) line.erase(comment);
        line = trim(line);
        if(line.empty()) continue;
        const auto separator = line.find('=');
        if(separator == std::string::npos) throw std::runtime_error("[MonitoringCsvWriter] Expected KEY = VALUE at " + configPath + ":" + std::to_string(lineNumber));
        const std::string key = toLower(trim(line.substr(0, separator)));
        const std::string value = trim(line.substr(separator + 1));
        if(key.empty()) throw std::runtime_error("[MonitoringCsvWriter] Empty key at " + configPath + ":" + std::to_string(lineNumber));

        const std::string calibrationPrefix = "calibration.";
        if(key.compare(0, calibrationPrefix.size(), calibrationPrefix) == 0)
        {
            const size_t settingSeparator = key.find('.', calibrationPrefix.size());
            if(settingSeparator == std::string::npos)
                throw std::runtime_error("[MonitoringCsvWriter] Expected calibration.NAME.SETTING at " + configPath + ":" + std::to_string(lineNumber));
            const std::string calibration = key.substr(calibrationPrefix.size(), settingSeparator - calibrationPrefix.size());
            const std::string setting = key.substr(settingSeparator + 1);
            auto& schedule = configuration.calibrationSchedules[calibration];
            if(setting == "monitor_schedule") schedule.mode = toLower(value);
            else if(setting == "monitor_percent_windows") calibrationPercentWindows[calibration] = parseWindows(value, key, configPath, lineNumber);
            else throw std::runtime_error("[MonitoringCsvWriter] Unknown calibration setting '" + setting + "' at " + configPath + ":" + std::to_string(lineNumber));
        }
        else if(key == "enabled") configuration.enabled = parseBoolean(value, key, configPath, lineNumber);
        else if(key == "csv_output_directory") configuration.outputDirectory = value;
        else if(key == "csv_rotate_size_mb") configuration.rotateSizeMb = parseNonNegativeInteger(value, key, configPath, lineNumber);
        else if(key == "csv_rotate_minutes") configuration.rotateMinutes = parseNonNegativeInteger(value, key, configPath, lineNumber);
        else if(key == "csv_include_errors") configuration.includeErrors = parseBoolean(value, key, configPath, lineNumber);
        else if(key == "csv_register_allowlist")
        {
            configuration.registerAllowlist.clear();
            if(value != "*") for(const auto& name: split(value, ',', false)) configuration.registerAllowlist.insert(name);
        }
        else if(key == "lpgbt_csv_enabled") configuration.lpGBTEnabled = parseBoolean(value, key, configPath, lineNumber);
        else if(key == "lpgbt_csv_register_allowlist")
        {
            configuration.lpGBTRegisterAllowlist.clear();
            if(value != "*") for(const auto& name: split(value, ',', false)) configuration.lpGBTRegisterAllowlist.insert(name);
        }
        else if(key == "virtual_register_config") configuration.virtualRegisterConfig = value;
        else if(key == "dca_mapping_file") configuration.dcaMappingFile = value;
        else if(key == "dca_mapping_failure_policy") configuration.dcaMappingFailurePolicy = toLower(value);
        else if(key == "monitor_schedule") configuration.scheduleMode = toLower(value);
        else if(key == "monitor_percent_windows") percentWindows = parseWindows(value, key, configPath, lineNumber);
        else if(key.compare(0, 9, "exporter_") == 0 || key.compare(0, 6, "lpgbt_") == 0 || key == "csv_cleanup_mode" || key == "csv_archive_directory" || key == "csv_retention_hours" ||
                key == "csv_acknowledgement" || key == "prometheus_server_url" || key == "csv_min_confirmed_scrapes" || key == "csv_exporter_source")
            continue;
        else throw std::runtime_error("[MonitoringCsvWriter] Unknown setting '" + key + "' at " + configPath + ":" + std::to_string(lineNumber));
    }

    configuration.scheduleWindows = percentWindows;
    if(configuration.scheduleMode != "always" && configuration.scheduleMode != "percent")
        throw std::runtime_error("[MonitoringCsvWriter] monitor_schedule must be always or percent");
    if(configuration.scheduleMode != "always" && configuration.scheduleWindows.empty())
        throw std::runtime_error("[MonitoringCsvWriter] Scheduled monitoring requires at least one window");
    if(configuration.scheduleMode == "percent")
    {
        for(const auto& window: configuration.scheduleWindows)
            if(window.second > 100) throw std::runtime_error("[MonitoringCsvWriter] Percent windows must be within 0-100");
    }
    for(auto& entry: configuration.calibrationSchedules)
    {
        auto& schedule = entry.second;
        schedule.windows = calibrationPercentWindows[entry.first];
        if(schedule.mode != "always" && schedule.mode != "percent")
            throw std::runtime_error("[MonitoringCsvWriter] Calibration monitor_schedule must be always or percent");
        if(schedule.mode != "always" && schedule.windows.empty())
            throw std::runtime_error("[MonitoringCsvWriter] Scheduled monitoring for '" + entry.first + "' requires at least one window");
        if(schedule.mode == "percent")
            for(const auto& window: schedule.windows)
                if(window.second > 100) throw std::runtime_error("[MonitoringCsvWriter] Calibration percent windows must be within 0-100");
    }
    if(configuration.dcaMappingFailurePolicy != "warn" && configuration.dcaMappingFailurePolicy != "abort")
        throw std::runtime_error("[MonitoringCsvWriter] dca_mapping_failure_policy must be warn or abort");
    return configuration;
}

void MonitoringCsvWriter::start(const std::string& calibrationName, const std::string& hardwareXml)
{
    const Configuration configuration = loadConfiguration();
    if(!configuration.enabled) return;

    std::lock_guard<std::mutex> lock(fMutex);
    if(fRunning) return;
    fConfiguration = configuration;
    const auto schedule = fConfiguration.calibrationSchedules.find(toLower(calibrationName));
    if(schedule != fConfiguration.calibrationSchedules.end())
    {
        fConfiguration.scheduleMode = schedule->second.mode;
        fConfiguration.scheduleWindows = schedule->second.windows;
    }
    const std::string hardwareXmlPath = absoluteInputPath(hardwareXml);
    const std::string xmlDirectory = parentDirectory(hardwareXmlPath);
    fConfiguration.outputDirectory = outputPath(fConfiguration.outputDirectory, xmlDirectory);
    fConfiguration.virtualRegisterConfig = absolutePath(fConfiguration.virtualRegisterConfig);
    if(fConfiguration.dcaMappingFile.empty() || toLower(fConfiguration.dcaMappingFile) == "auto")
        fConfiguration.dcaMappingFile = xmlDirectory + "/" + fileStem(hardwareXmlPath) + ".csv";
    else
        fConfiguration.dcaMappingFile = outputPath(fConfiguration.dcaMappingFile, xmlDirectory);
    fCalibrationName = calibrationName;
    fRunNumber = -1;
    fRunStart = std::chrono::steady_clock::now();
    fMetadataReady = false;
    fValues.clear();
    fModuleValues.clear();
    fPortcardValues.clear();
    fIdentities.clear();
    fPortcardIdentities.clear();
    fModuleChipCounts.clear();
    fColumns.clear();
    fWrittenColumns.clear();
    ensureDirectory(fConfiguration.outputDirectory);
    loadVirtualRegisterDefinitions();
    loadDcaMapping();
    fRunning = true;
}

void MonitoringCsvWriter::setRunMetadata(const std::string& calibrationName, int runNumber)
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(!fRunning) return;
    if(fMetadataReady && fCalibrationName == calibrationName && fRunNumber == runNumber) return;
    closeOutputFileLocked();
    fCalibrationName = calibrationName;
    fRunNumber = runNumber;
    fMetadataReady = true;
    fFilePart = 1;
    fFileStem.clear();
    fRunStart = std::chrono::steady_clock::now();
}

void MonitoringCsvWriter::stop()
{
    std::lock_guard<std::mutex> lock(fMutex);
    closeOutputFileLocked();
    fValues.clear();
    fModuleValues.clear();
    fPortcardValues.clear();
    fIdentities.clear();
    fPortcardIdentities.clear();
    fModuleChipCounts.clear();
    fColumns.clear();
    fRunning = false;
    fCycleActive = false;
    fMetadataReady = false;
}

bool MonitoringCsvWriter::shouldMonitorNow() const
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(!fRunning || fConfiguration.scheduleMode == "always") return true;
    const double progress = RD53RunProgress::fraction();
    if(progress < 0) return false;
    const double position = progress * 100;
    for(const auto& window: fConfiguration.scheduleWindows)
        if(position >= window.first && (position < window.second || (window.second == 100 && position == 100))) return true;
    return false;
}

bool MonitoringCsvWriter::isRunning() const
{
    std::lock_guard<std::mutex> lock(fMutex);
    return fRunning;
}

void MonitoringCsvWriter::beginCycle()
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(!fRunning) return;
    fValues.clear();
    fModuleValues.clear();
    fPortcardValues.clear();
    fColumns.clear();
    fCycleActive = true;
}

void MonitoringCsvWriter::endCycle()
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(!fRunning || !fCycleActive) return;
    fCycleActive = false;
    if(!fMetadataReady || (fValues.empty() && fPortcardValues.empty())) return;
    if(!fWrittenColumns.empty() && fColumns != fWrittenColumns)
    {
        closeOutputFileLocked();
        ++fFilePart;
    }
    rotateIfNeededLocked();
    if(!fOutput.is_open()) openOutputFileLocked();
    writeRowsLocked();
}

void MonitoringCsvWriter::update(int boardId,
                                 int opticalGroupId,
                                 int hybridId,
                                 int chipId,
                                 const std::string& registerName,
                                 double value,
                                 bool isAdcObservable,
                                 std::size_t moduleChipCount)
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(!fRunning || !fCycleActive) return;
    if(!fConfiguration.registerAllowlist.empty() && fConfiguration.registerAllowlist.count(registerName) == 0) return;

    const double factor = correctionFactor(registerName);
    const DetectorKey detectorKey{boardId, opticalGroupId, hybridId, chipId};
    const ModuleKey moduleKey{boardId, opticalGroupId, hybridId};
    MetricValue metric{value * factor, isAdcObservable, isAdcObservable ? value * 0.04 * factor : 0};
    fValues[detectorKey][registerName] = metric;
    fColumns.insert(registerName);
    if(moduleChipCount != 0) fModuleChipCounts[moduleKey] = moduleChipCount;
    updateChipVirtualRegistersLocked(detectorKey);
    updateModuleVirtualRegistersLocked(moduleKey);
}

void MonitoringCsvWriter::updateLpGBT(int boardId, int opticalGroupId, const std::string& registerName, double value)
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(!fRunning || !fCycleActive || !fConfiguration.lpGBTEnabled) return;
    const std::string name = "LPGBT_" + registerName;
    if(!fConfiguration.lpGBTRegisterAllowlist.empty() && fConfiguration.lpGBTRegisterAllowlist.count(registerName) == 0) return;
    const OpticalKey key{boardId, opticalGroupId};
    fPortcardValues[key][name] = MetricValue{value, false, 0};
    fColumns.insert(name);
    updateLpGBTVirtualRegistersLocked(key);
}

void MonitoringCsvWriter::loadVirtualRegisterDefinitions()
{
    fVirtualRegisterDefinitions.clear();
    if(fConfiguration.virtualRegisterConfig.empty()) return;
    std::ifstream input(fConfiguration.virtualRegisterConfig);
    if(!input.is_open()) return;
    std::set<std::string> names;
    std::string line;
    size_t lineNumber = 0;
    while(std::getline(input, line))
    {
        ++lineNumber;
        line = trim(line);
        if(line.empty() || line.front() == '#') continue;
        const std::vector<std::string> fields = split(line, '|');
        if(fields.size() != 3 && fields.size() != 4)
            throw std::runtime_error("[MonitoringCsvWriter] Invalid virtual register at " + fConfiguration.virtualRegisterConfig + ":" + std::to_string(lineNumber));
        VirtualRegisterDefinition definition;
        definition.name = fields[0];
        definition.expression = fields.back();
        if(fields.size() == 4)
        {
            if(fields[2] == "module") definition.scope = VirtualRegisterDefinition::Scope::Module;
            else if(fields[2] == "lpgbt") definition.scope = VirtualRegisterDefinition::Scope::LpGBT;
            else if(fields[2] == "portcard") definition.scope = VirtualRegisterDefinition::Scope::Portcard;
            else if(fields[2] != "chip") throw std::runtime_error("[MonitoringCsvWriter] Virtual scope must be chip, module, lpgbt, or portcard");
        }
        if(definition.name.empty() || definition.expression.empty() || !names.insert(definition.name).second)
            throw std::runtime_error("[MonitoringCsvWriter] Invalid or duplicate virtual register at line " + std::to_string(lineNumber));
        fVirtualRegisterDefinitions.push_back(definition);
    }
}

void MonitoringCsvWriter::loadDcaMapping()
{
    if(fConfiguration.dcaMappingFile.empty()) return;
    std::ifstream input(fConfiguration.dcaMappingFile);
    if(!input.is_open())
    {
        const std::string message = "[MonitoringCsvWriter] Cannot open DCA mapping " + fConfiguration.dcaMappingFile + "; run the standalone DCA lookup before CMSITminiDAQ";
        if(fConfiguration.dcaMappingFailurePolicy == "abort") throw std::runtime_error(message);
        LOG(WARNING) << message;
        return;
    }
    std::string line;
    std::getline(input, line);
    while(std::getline(input, line))
    {
        const std::vector<std::string> fields = split(line, ',');
        if(fields.size() < 8) continue;
        try
        {
            const int board = std::stoi(fields[0]);
            const int optical = std::stoi(fields[1]);
            const DetectorKey key{board, optical, std::stoi(fields[4]), std::stoi(fields[5])};
            fIdentities[key] = Identity{fields[7].empty() ? 0 : std::stoll(fields[7]), fields[6]};
            auto& portcard = fPortcardIdentities[{board, optical}];
            portcard.portcard = fields[2].empty() ? "-1" : fields[2];
            portcard.efuse = fields[3].empty() ? "-1" : fields[3];
        }
        catch(const std::exception&) { LOG(WARNING) << "[MonitoringCsvWriter] Ignoring malformed DCA mapping row: " << line; }
    }
}

void MonitoringCsvWriter::updateChipVirtualRegistersLocked(const DetectorKey& detectorKey)
{
    auto& values = fValues[detectorKey];
    std::map<std::string, double> corrected;
    for(const auto& entry: values) corrected[entry.first] = entry.second.value;
    updateVirtualRegistersLocked(VirtualRegisterDefinition::Scope::Chip, corrected, values);
}

void MonitoringCsvWriter::updateModuleVirtualRegistersLocked(const ModuleKey& moduleKey)
{
    std::map<std::string, double> corrected;
    for(const auto& detector: fValues)
    {
        if(std::get<0>(detector.first) != std::get<0>(moduleKey) || std::get<1>(detector.first) != std::get<1>(moduleKey) || std::get<2>(detector.first) != std::get<2>(moduleKey)) continue;
        for(const auto& value: detector.second) corrected[value.first + "[" + std::to_string(std::get<3>(detector.first)) + "]"] = value.second.value;
    }
    auto& moduleValues = fModuleValues[moduleKey];
    const auto count = fModuleChipCounts.find(moduleKey);
    const size_t expected = count == fModuleChipCounts.end() ? 0 : count->second;
    updateVirtualRegistersLocked(VirtualRegisterDefinition::Scope::Module, corrected, moduleValues, expected);
}

void MonitoringCsvWriter::updateLpGBTVirtualRegistersLocked(const OpticalKey& opticalKey)
{
    auto& values = fPortcardValues[opticalKey];
    std::map<std::string, double> corrected;
    for(const auto& entry: values) corrected[entry.first] = entry.second.value;
    updateVirtualRegistersLocked(VirtualRegisterDefinition::Scope::LpGBT, corrected, values);
    corrected.clear();
    for(const auto& entry: values) corrected[entry.first] = entry.second.value;
    updateVirtualRegistersLocked(VirtualRegisterDefinition::Scope::Portcard, corrected, values);
}

void MonitoringCsvWriter::updateVirtualRegistersLocked(VirtualRegisterDefinition::Scope scope, const std::map<std::string, double>& inputs,
                                                        std::map<std::string, MetricValue>& values, std::size_t expectedInputs)
{
    for(const auto& definition: fVirtualRegisterDefinitions)
    {
        if(definition.scope != scope) continue;
        double result = 0;
        VirtualExpressionParser parser(definition.expression, inputs, expectedInputs);
        if(!parser.evaluate(result)) continue;
        values[definition.name] = MetricValue{result, false, 0};
        fColumns.insert(definition.name);
    }
}

void MonitoringCsvWriter::openOutputFileLocked()
{
    if(fFileStem.empty())
    {
        fFileStem = safeName(fCalibrationName);
        if(fRunNumber >= 0) fFileStem += "_" + std::to_string(fRunNumber);
        fFileStem += "_" + dateTimeStamp("%Y%m%d_%H%M%S");
    }
    std::ostringstream name;
    name << fConfiguration.outputDirectory << "/" << fFileStem;
    if(fFilePart > 1) name << "_part" << std::setw(3) << std::setfill('0') << fFilePart;
    name << ".active.csv";
    fCurrentFilePath = name.str();
    fOutput.open(fCurrentFilePath, std::ios::out | std::ios::app);
    if(!fOutput.is_open()) throw std::runtime_error("[MonitoringCsvWriter] Cannot open " + fCurrentFilePath);
    fFileOpenedAt = std::chrono::steady_clock::now();
    writeHeaderLocked();
    LOG(INFO) << "Monitoring CSV output: " << fCurrentFilePath;
}

void MonitoringCsvWriter::rotateIfNeededLocked()
{
    if(!fOutput.is_open()) return;
    bool rotate = false;
    if(fConfiguration.rotateMinutes > 0)
        rotate = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - fFileOpenedAt).count() >= fConfiguration.rotateMinutes;
    if(!rotate && fConfiguration.rotateSizeMb > 0)
    {
        struct stat status{};
        if(stat(fCurrentFilePath.c_str(), &status) == 0)
            rotate = static_cast<unsigned long long>(status.st_size) >= static_cast<unsigned long long>(fConfiguration.rotateSizeMb) * 1024ULL * 1024ULL;
    }
    if(rotate)
    {
        closeOutputFileLocked();
        ++fFilePart;
    }
}

void MonitoringCsvWriter::writeHeaderLocked()
{
    fWrittenColumns = fColumns;
    fOutput << "board,optical,portcard_id,lpgbt_efuse,hybrid,chip,module_id,chip_efuse,date,time";
    for(const auto& column: fWrittenColumns)
    {
        fOutput << ',' << csvEscape(column + "_value");
        if(fConfiguration.includeErrors) fOutput << ',' << csvEscape(column + "__error");
    }
    fOutput << '\n';
    fOutput.flush();
}

void MonitoringCsvWriter::writeRowsLocked()
{
    const std::string date = dateTimeStamp("%Y-%m-%d");
    const std::string time = dateTimeStamp("%H:%M:%S");
    std::set<DetectorKey> detectorKeys;
    for(const auto& entry: fValues) detectorKeys.insert(entry.first);
    for(const auto& entry: fIdentities) detectorKeys.insert(entry.first);
    for(const auto& entry: fPortcardValues)
    {
        const bool hasDetector = std::any_of(detectorKeys.begin(), detectorKeys.end(), [&entry](const DetectorKey& key) {
            return std::get<0>(key) == entry.first.first && std::get<1>(key) == entry.first.second;
        });
        if(!hasDetector) detectorKeys.insert({entry.first.first, entry.first.second, -1, -1});
    }
    for(const auto& key: detectorKeys)
    {
        const auto identity = fIdentities.find(key);
        const std::string efuse = identity == fIdentities.end() || identity->second.efuse == 0 ? "-1" : std::to_string(identity->second.efuse);
        const std::string module = identity == fIdentities.end() || identity->second.module.empty() ? "-1" : identity->second.module;
        const OpticalKey opticalKey{std::get<0>(key), std::get<1>(key)};
        const auto portcardIdentity = fPortcardIdentities.find(opticalKey);
        const std::string portcard = portcardIdentity == fPortcardIdentities.end() ? "-1" : portcardIdentity->second.portcard;
        const std::string lpgbtEfuse = portcardIdentity == fPortcardIdentities.end() ? "-1" : portcardIdentity->second.efuse;
        fOutput << std::get<0>(key) << ',' << std::get<1>(key) << ',' << csvEscape(portcard) << ',' << lpgbtEfuse << ',' << std::get<2>(key) << ',' << std::get<3>(key) << ','
                << csvEscape(module) << ',' << efuse << ',' << date << ',' << time;
        const auto detector = fValues.find(key);
        const ModuleKey moduleKey{std::get<0>(key), std::get<1>(key), std::get<2>(key)};
        const auto moduleValues = fModuleValues.find(moduleKey);
        const auto portcardValues = fPortcardValues.find(opticalKey);
        for(const auto& column: fWrittenColumns)
        {
            const MetricValue* value = nullptr;
            if(detector != fValues.end())
            {
                const auto chipValue = detector->second.find(column);
                if(chipValue != detector->second.end()) value = &chipValue->second;
            }
            if(value == nullptr && moduleValues != fModuleValues.end())
            {
                const auto moduleValue = moduleValues->second.find(column);
                if(moduleValue != moduleValues->second.end()) value = &moduleValue->second;
            }
            if(value == nullptr && portcardValues != fPortcardValues.end())
            {
                const auto portcardValue = portcardValues->second.find(column);
                if(portcardValue != portcardValues->second.end()) value = &portcardValue->second;
            }
            fOutput << ',';
            if(value != nullptr) fOutput << std::setprecision(12) << value->value;
            if(fConfiguration.includeErrors)
            {
                fOutput << ',';
                if(value != nullptr && value->hasError) fOutput << std::setprecision(12) << value->error;
            }
        }
        fOutput << '\n';
    }
    fOutput.flush();
}

void MonitoringCsvWriter::closeOutputFileLocked()
{
    if(fOutput.is_open())
    {
        fOutput.close();
        const std::string readyPath = fCurrentFilePath.substr(0, fCurrentFilePath.size() - 11) + ".ready.csv";
        if(std::rename(fCurrentFilePath.c_str(), readyPath.c_str()) != 0) LOG(ERROR) << "[MonitoringCsvWriter] Cannot finalize " << fCurrentFilePath;
    }
    fWrittenColumns.clear();
}

double MonitoringCsvWriter::correctionFactor(const std::string& registerName)
{
    if(registerName == "ANA_IN_CURR" || registerName == "DIG_IN_CURR") return 21000.0;
    if(registerName == "ANA_SHUNT_CURR" || registerName == "DIG_SHUNT_CURR") return 21520.0;
    if(registerName == "VDDA" || registerName == "VDDD") return 2.0;
    if(registerName == "VINA" || registerName == "VOFS" || registerName == "VIND") return 4.0;
    return 1.0;
}

std::string MonitoringCsvWriter::csvEscape(const std::string& value)
{
    if(value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string escaped = "\"";
    for(char character: value) escaped += character == '"' ? "\"\"" : std::string(1, character);
    return escaped + "\"";
}
