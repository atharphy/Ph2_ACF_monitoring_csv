#ifndef MONITORING_CSV_WRITER_H
#define MONITORING_CSV_WRITER_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

class MonitoringCsvWriter
{
  public:
    struct Configuration
    {
        struct Schedule
        {
            std::string                            mode{"always"};
            std::vector<std::pair<double, double>> windows;
        };

        bool                  enabled{true};
        std::string           outputDirectory{"monitoring_csv"};
        unsigned int          rotateSizeMb{100};
        unsigned int          rotateMinutes{60};
        bool                  includeErrors{true};
        std::set<std::string> registerAllowlist;
        std::string           virtualRegisterConfig;

        std::string dcaMappingFile;
        std::string dcaMappingFailurePolicy{"warn"};

        std::string                            scheduleMode{"always"};
        std::vector<std::pair<double, double>> scheduleWindows;
        std::map<std::string, Schedule>         calibrationSchedules;
    };

    static MonitoringCsvWriter& getInstance();

    void start(const std::string& calibrationName, const std::string& hardwareXml);
    void setRunMetadata(const std::string& calibrationName, int runNumber);
    void stop();

    bool shouldMonitorNow() const;
    void beginCycle();
    void endCycle();
    void update(int boardId, int opticalGroupId, int hybridId, int chipId, int64_t efuseCode, const std::string& registerName,
                double value, bool isAdcObservable, bool isCurrent, std::size_t moduleChipCount = 0);

  private:
    MonitoringCsvWriter() = default;
    ~MonitoringCsvWriter();

    MonitoringCsvWriter(const MonitoringCsvWriter&)            = delete;
    MonitoringCsvWriter& operator=(const MonitoringCsvWriter&) = delete;

    static Configuration loadConfiguration();

    struct MetricValue
    {
        double value{0};
        bool   hasError{false};
        double error{0};
        std::string unit;
    };

    struct VirtualRegisterDefinition
    {
        enum class Scope
        {
            Chip,
            Module
        };

        std::string name;
        std::string unit;
        Scope       scope{Scope::Chip};
        std::string expression;
    };

    struct Identity
    {
        int64_t     efuse{0};
        std::string module;
    };

    using DetectorKey = std::tuple<int, int, int, int>;
    using ModuleKey   = std::tuple<int, int, int>;

    void loadVirtualRegisterDefinitions();
    void loadDcaMapping();
    void updateChipVirtualRegistersLocked(const DetectorKey& detectorKey);
    void updateModuleVirtualRegistersLocked(const ModuleKey& moduleKey);
    void openOutputFileLocked();
    void rotateIfNeededLocked();
    void writeHeaderLocked();
    void writeRowsLocked();
    void closeOutputFileLocked();

    static double      correctionFactor(const std::string& registerName);
    static std::string csvEscape(const std::string& value);

    mutable std::mutex                                        fMutex;
    Configuration                                             fConfiguration;
    std::map<DetectorKey, std::map<std::string, MetricValue>> fValues;
    std::map<ModuleKey, std::map<std::string, MetricValue>>   fModuleValues;
    std::map<DetectorKey, Identity>                           fIdentities;
    std::map<ModuleKey, std::size_t>                          fModuleChipCounts;
    std::vector<VirtualRegisterDefinition>                    fVirtualRegisterDefinitions;
    std::set<std::string>                                     fColumns;
    std::set<std::string>                                     fWrittenColumns;

    bool                                      fRunning{false};
    bool                                      fCycleActive{false};
    bool                                      fMetadataReady{false};
    std::string                               fCalibrationName;
    std::string                               fHardwareXml;
    int                                       fRunNumber{-1};
    unsigned int                              fFilePart{1};
    std::string                               fFileStem;
    std::string                               fCurrentFilePath;
    std::ofstream                             fOutput;
    std::chrono::steady_clock::time_point     fRunStart;
    std::chrono::steady_clock::time_point     fFileOpenedAt;
};

#endif
