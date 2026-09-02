#ifndef LPGBT_MONITORING_CSV_WRITER_H
#define LPGBT_MONITORING_CSV_WRITER_H

#include <chrono>
#include <cstdint>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <tuple>

class LpGBTMonitoringCsvWriter
{
  public:
    static LpGBTMonitoringCsvWriter& getInstance();

    void start(const std::string& calibrationName, const std::string& hardwareXml);
    void setRunMetadata(const std::string& calibrationName, int runNumber);
    void stop();

    bool isRunning() const;
    void beginCycle();
    void endCycle();
    void update(int boardId, int opticalGroupId, int lpGBTId, uint32_t lpGBTEfuse, const std::string& registerName, double value);

  private:
    struct Configuration
    {
        bool                  enabled{true};
        std::string           outputDirectory{"monitoring_csv"};
        unsigned int          rotateSizeMb{100};
        unsigned int          rotateMinutes{60};
        std::set<std::string> registerAllowlist;
        std::string           mappingFile{"auto"};
        std::string           mappingFailurePolicy{"warn"};
    };

    using LpGBTKey = std::tuple<int, int, int>;
    using OpticalKey = std::pair<int, int>;

    LpGBTMonitoringCsvWriter() = default;
    ~LpGBTMonitoringCsvWriter();

    LpGBTMonitoringCsvWriter(const LpGBTMonitoringCsvWriter&)            = delete;
    LpGBTMonitoringCsvWriter& operator=(const LpGBTMonitoringCsvWriter&) = delete;

    static Configuration loadConfiguration();
    void loadMappingLocked();
    void openOutputFileLocked();
    void rotateIfNeededLocked();
    void writeRowsLocked();
    void closeOutputFileLocked();

    mutable std::mutex                                  fMutex;
    Configuration                                       fConfiguration;
    std::map<LpGBTKey, std::map<std::string, double>>   fValues;
    std::map<LpGBTKey, uint32_t>                        fEfuses;
    std::map<OpticalKey, std::string>                   fPortcards;
    std::set<std::string>                               fColumns;
    std::set<std::string>                               fWrittenColumns;
    bool                                                fRunning{false};
    bool                                                fMappingLoaded{false};
    bool                                                fCycleActive{false};
    bool                                                fMetadataReady{false};
    std::string                                         fCalibrationName;
    int                                                 fRunNumber{-1};
    unsigned int                                        fFilePart{1};
    std::string                                         fFileStem;
    std::string                                         fCurrentFilePath;
    std::ofstream                                       fOutput;
    std::chrono::steady_clock::time_point               fFileOpenedAt;
};

#endif
