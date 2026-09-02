#include "MonitorUtils/LpGBTMonitoringCsvWriter.h"
#include "Utils/easylogging++.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
std::string trim(const std::string& value)
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if(first == std::string::npos) return "";
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::vector<std::string> split(const std::string& value, char delimiter)
{
    std::vector<std::string> fields;
    size_t start = 0;
    while(start <= value.size())
    {
        const size_t separator = value.find(delimiter, start);
        fields.push_back(trim(value.substr(start, separator == std::string::npos ? std::string::npos : separator - start)));
        if(separator == std::string::npos) break;
        start = separator + 1;
    }
    return fields;
}

bool booleanValue(const std::string& value)
{
    const std::string normalized = lower(trim(value));
    if(normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") return true;
    if(normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") return false;
    throw std::runtime_error("Invalid boolean value '" + value + "'");
}

std::string baseDirectory()
{
    const char* base = std::getenv("PH2ACF_BASE_DIR");
    return base == nullptr ? "." : base;
}

std::string absoluteInputPath(const std::string& value)
{
    if(value.empty() || value.front() == '/') return value;
    char path[4096];
    if(getcwd(path, sizeof(path)) == nullptr) throw std::runtime_error("Cannot determine working directory");
    return std::string(path) + "/" + value;
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

std::string resolveFromXml(const std::string& value, const std::string& xmlDirectory)
{
    if(value.empty() || value.front() == '/') return value;
    return xmlDirectory + "/" + value;
}

bool exists(const std::string& path)
{
    struct stat status{};
    return stat(path.c_str(), &status) == 0;
}

void ensureDirectory(const std::string& path)
{
    std::string current;
    for(char character: path)
    {
        current += character;
        if(character == '/' && current.size() > 1) mkdir(current.c_str(), 0755);
    }
    if(mkdir(path.c_str(), 0755) != 0 && !exists(path)) throw std::runtime_error("Cannot create directory " + path);
}

std::string safeName(std::string value)
{
    for(char& character: value)
        if(!std::isalnum(static_cast<unsigned char>(character)) && character != '-' && character != '_') character = '_';
    return value.empty() ? "monitoring" : value;
}

std::string timestamp(const char* format)
{
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
    localtime_r(&now, &localTime);
    std::ostringstream output;
    output << std::put_time(&localTime, format);
    return output.str();
}

std::string csvEscape(const std::string& value)
{
    if(value.find_first_of(",\"\r\n") == std::string::npos) return value;
    std::string escaped = "\"";
    for(char character: value) escaped += character == '"' ? "\"\"" : std::string(1, character);
    return escaped + "\"";
}

std::string hexValue(uint32_t value)
{
    std::ostringstream output;
    output << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << value;
    return output.str();
}
}

LpGBTMonitoringCsvWriter& LpGBTMonitoringCsvWriter::getInstance()
{
    static LpGBTMonitoringCsvWriter instance;
    return instance;
}

LpGBTMonitoringCsvWriter::~LpGBTMonitoringCsvWriter() { stop(); }

LpGBTMonitoringCsvWriter::Configuration LpGBTMonitoringCsvWriter::loadConfiguration()
{
    Configuration configuration;
    bool globalEnabled = true;
    bool lpGBTEnabled = true;
    const char* configuredPath = std::getenv("CMSIT_MONITORING_CONFIG");
    const std::string path = configuredPath == nullptr ? baseDirectory() + "/settings/monitoring_settings.conf" : configuredPath;
    std::ifstream input(path);
    if(!input.is_open())
    {
        if(configuredPath != nullptr) throw std::runtime_error("[LpGBTMonitoringCsvWriter] Cannot open " + path);
        return configuration;
    }

    std::string line;
    while(std::getline(input, line))
    {
        const size_t comment = line.find('#');
        if(comment != std::string::npos) line.erase(comment);
        line = trim(line);
        if(line.empty()) continue;
        const size_t separator = line.find('=');
        if(separator == std::string::npos) continue;
        const std::string key = lower(trim(line.substr(0, separator)));
        const std::string value = trim(line.substr(separator + 1));
        if(key == "enabled") globalEnabled = booleanValue(value);
        else if(key == "csv_output_directory") configuration.outputDirectory = value;
        else if(key == "csv_rotate_size_mb") configuration.rotateSizeMb = std::stoul(value);
        else if(key == "csv_rotate_minutes") configuration.rotateMinutes = std::stoul(value);
        else if(key == "lpgbt_csv_enabled") lpGBTEnabled = booleanValue(value);
        else if(key == "lpgbt_csv_output_directory") configuration.outputDirectory = value;
        else if(key == "lpgbt_csv_rotate_size_mb") configuration.rotateSizeMb = std::stoul(value);
        else if(key == "lpgbt_csv_rotate_minutes") configuration.rotateMinutes = std::stoul(value);
        else if(key == "lpgbt_csv_register_allowlist")
        {
            configuration.registerAllowlist.clear();
            if(value != "*") for(const std::string& name: split(value, ',')) configuration.registerAllowlist.insert(name);
        }
        else if(key == "lpgbt_dca_mapping_file") configuration.mappingFile = value;
        else if(key == "lpgbt_dca_mapping_failure_policy") configuration.mappingFailurePolicy = lower(value);
    }

    configuration.enabled = globalEnabled && lpGBTEnabled;
    if(configuration.mappingFailurePolicy != "warn" && configuration.mappingFailurePolicy != "abort") throw std::runtime_error("[LpGBTMonitoringCsvWriter] mapping failure policy must be warn or abort");
    return configuration;
}

void LpGBTMonitoringCsvWriter::start(const std::string& calibrationName, const std::string& hardwareXml)
{
    const Configuration configuration = loadConfiguration();
    if(!configuration.enabled) return;
    std::lock_guard<std::mutex> lock(fMutex);
    if(fRunning) return;

    fConfiguration = configuration;
    const std::string resolvedXml = absoluteInputPath(hardwareXml);
    const std::string xmlDirectory = parentDirectory(resolvedXml);
    fConfiguration.outputDirectory = resolveFromXml(fConfiguration.outputDirectory, xmlDirectory);
    if(fConfiguration.mappingFile.empty() || lower(fConfiguration.mappingFile) == "auto")
        fConfiguration.mappingFile = xmlDirectory + "/" + fileStem(resolvedXml) + "_lpgbt_portcards.csv";
    else
        fConfiguration.mappingFile = resolveFromXml(fConfiguration.mappingFile, xmlDirectory);
    fCalibrationName = calibrationName;
    fRunNumber = -1;
    fMetadataReady = false;
    fFilePart = 1;
    fFileStem.clear();
    fValues.clear();
    fEfuses.clear();
    fColumns.clear();
    fWrittenColumns.clear();
    ensureDirectory(fConfiguration.outputDirectory);
    fMappingLoaded = false;
    fRunning = true;
}

void LpGBTMonitoringCsvWriter::setRunMetadata(const std::string& calibrationName, int runNumber)
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
}

void LpGBTMonitoringCsvWriter::stop()
{
    std::lock_guard<std::mutex> lock(fMutex);
    closeOutputFileLocked();
    fValues.clear();
    fEfuses.clear();
    fPortcards.clear();
    fColumns.clear();
    fRunning = false;
    fMappingLoaded = false;
    fCycleActive = false;
    fMetadataReady = false;
}

bool LpGBTMonitoringCsvWriter::isRunning() const
{
    std::lock_guard<std::mutex> lock(fMutex);
    return fRunning;
}

void LpGBTMonitoringCsvWriter::beginCycle()
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(!fRunning) return;
    fValues.clear();
    fColumns.clear();
    fCycleActive = true;
}

void LpGBTMonitoringCsvWriter::endCycle()
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(!fRunning || !fCycleActive) return;
    fCycleActive = false;
    if(!fMetadataReady || fValues.empty()) return;
    if(!fWrittenColumns.empty() && fColumns != fWrittenColumns)
    {
        closeOutputFileLocked();
        ++fFilePart;
    }
    rotateIfNeededLocked();
    if(!fOutput.is_open()) openOutputFileLocked();
    writeRowsLocked();
}

void LpGBTMonitoringCsvWriter::update(int boardId, int opticalGroupId, int lpGBTId, uint32_t lpGBTEfuse, const std::string& registerName, double value)
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(!fRunning || !fCycleActive) return;
    if(!fConfiguration.registerAllowlist.empty() && fConfiguration.registerAllowlist.count(registerName) == 0) return;
    if(!fMappingLoaded)
    {
        loadMappingLocked();
        fMappingLoaded = true;
    }
    const LpGBTKey key{boardId, opticalGroupId, lpGBTId};
    fValues[key][registerName] = value;
    fEfuses[key] = lpGBTEfuse;
    fColumns.insert(registerName);
}

void LpGBTMonitoringCsvWriter::loadMappingLocked()
{
    fPortcards.clear();
    std::ifstream input(fConfiguration.mappingFile);
    if(!input.is_open())
    {
        const std::string message = "[LpGBTMonitoringCsvWriter] Cannot open " + fConfiguration.mappingFile;
        if(fConfiguration.mappingFailurePolicy == "abort") throw std::runtime_error(message);
        LOG(WARNING) << message;
        return;
    }
    std::string line;
    std::getline(input, line);
    while(std::getline(input, line))
    {
        const std::vector<std::string> fields = split(line, ',');
        if(fields.size() < 4) continue;
        try { fPortcards[{std::stoi(fields[0]), std::stoi(fields[1])}] = fields[3]; }
        catch(const std::exception&) { LOG(WARNING) << "[LpGBTMonitoringCsvWriter] Ignoring malformed mapping row: " << line; }
    }
}

void LpGBTMonitoringCsvWriter::openOutputFileLocked()
{
    if(fFileStem.empty())
    {
        fFileStem = safeName(fCalibrationName);
        if(fRunNumber >= 0) fFileStem += "_" + std::to_string(fRunNumber);
        fFileStem += "_" + timestamp("%Y%m%d_%H%M%S") + "_lpgbt";
    }
    std::ostringstream path;
    path << fConfiguration.outputDirectory << "/" << fFileStem;
    if(fFilePart > 1) path << "_part" << std::setw(3) << std::setfill('0') << fFilePart;
    path << ".active.csv";
    fCurrentFilePath = path.str();
    fOutput.open(fCurrentFilePath, std::ios::out | std::ios::app);
    if(!fOutput.is_open()) throw std::runtime_error("[LpGBTMonitoringCsvWriter] Cannot open " + fCurrentFilePath);
    fWrittenColumns = fColumns;
    fOutput << "board,optical,lpgbt_efuse,portcard_id,date,time";
    for(const std::string& column: fWrittenColumns) fOutput << ',' << csvEscape(column);
    fOutput << '\n';
    fOutput.flush();
    fFileOpenedAt = std::chrono::steady_clock::now();
    LOG(INFO) << "LpGBT monitoring CSV output: " << fCurrentFilePath;
}

void LpGBTMonitoringCsvWriter::rotateIfNeededLocked()
{
    if(!fOutput.is_open()) return;
    bool rotate = fConfiguration.rotateMinutes > 0 && std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - fFileOpenedAt).count() >= fConfiguration.rotateMinutes;
    if(!rotate && fConfiguration.rotateSizeMb > 0)
    {
        struct stat status{};
        rotate = stat(fCurrentFilePath.c_str(), &status) == 0 && static_cast<unsigned long long>(status.st_size) >= static_cast<unsigned long long>(fConfiguration.rotateSizeMb) * 1024ULL * 1024ULL;
    }
    if(rotate)
    {
        closeOutputFileLocked();
        ++fFilePart;
    }
}

void LpGBTMonitoringCsvWriter::writeRowsLocked()
{
    const std::string date = timestamp("%Y-%m-%d");
    const std::string time = timestamp("%H:%M:%S");
    for(const auto& entry: fValues)
    {
        const LpGBTKey& key = entry.first;
        const auto efuse = fEfuses.find(key);
        const auto portcard = fPortcards.find({std::get<0>(key), std::get<1>(key)});
        fOutput << std::get<0>(key) << ',' << std::get<1>(key) << ',' << (efuse == fEfuses.end() ? "" : hexValue(efuse->second)) << ',';
        if(portcard != fPortcards.end()) fOutput << csvEscape(portcard->second);
        fOutput << ',' << date << ',' << time;
        for(const std::string& column: fWrittenColumns)
        {
            fOutput << ',';
            const auto value = entry.second.find(column);
            if(value != entry.second.end()) fOutput << std::setprecision(12) << value->second;
        }
        fOutput << '\n';
    }
    fOutput.flush();
}

void LpGBTMonitoringCsvWriter::closeOutputFileLocked()
{
    if(fOutput.is_open())
    {
        fOutput.close();
        const std::string readyPath = fCurrentFilePath.substr(0, fCurrentFilePath.size() - 11) + ".ready.csv";
        if(std::rename(fCurrentFilePath.c_str(), readyPath.c_str()) != 0) LOG(ERROR) << "[LpGBTMonitoringCsvWriter] Cannot finalize " << fCurrentFilePath;
    }
    fWrittenColumns.clear();
}
