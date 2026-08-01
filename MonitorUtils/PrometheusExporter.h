#ifndef PROMETHEUS_EXPORTER_H
#define PROMETHEUS_EXPORTER_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace Ph2_HwInterface
{
class RD53Interface;
}

class PrometheusExporter
{
  public:
    struct Configuration
    {
        bool                  enabled{true};
        std::string           listenAddress{"127.0.0.1"};
        uint16_t              port{9101};
        std::string           metricsPath{"/metrics"};
        bool                  exportValue{true};
        bool                  exportError{true};
        bool                  exportLastUpdate{true};
        std::set<std::string> registerAllowlist;
    };

    static PrometheusExporter& getInstance();
    static Configuration       loadConfiguration();

    void start(const Configuration& configuration);
    void stop();

    void update(int                boardId,
                int                opticalGroupId,
                int                hybridId,
                int                chipId,
                const std::string& registerName,
                double             value,
                Ph2_HwInterface::RD53Interface* rd53Interface,
                std::size_t        moduleChipCount = 0);

  private:
    PrometheusExporter() = default;
    ~PrometheusExporter();

    PrometheusExporter(const PrometheusExporter&)            = delete;
    PrometheusExporter& operator=(const PrometheusExporter&) = delete;

    struct MetricValue
    {
        double                                value{0};
        bool                                  hasError{false};
        double                                error{0};
        std::chrono::system_clock::time_point updateTime;
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

    using MetricKey   = std::tuple<int, int, int, int, std::string, std::string>;
    using DetectorKey = std::tuple<int, int, int, int>;
    using ModuleKey   = std::tuple<int, int, int>;

    void        run();
    void        handleClient(int clientSocket) const;
    std::string renderMetrics() const;
    void        loadVirtualRegisterDefinitions();
    void        updateChipVirtualRegistersLocked(const DetectorKey& detectorKey);
    void        updateModuleVirtualRegistersLocked(const ModuleKey& moduleKey);

    static double      correctionFactor(const std::string& registerName);
    static std::string escapeLabelValue(const std::string& value);
    static std::string renderLabels(const MetricKey& key);
    static void        sendResponse(int clientSocket, int statusCode, const std::string& statusText, const std::string& contentType, const std::string& body);
    static void        sendAll(int clientSocket, const std::string& response);

    mutable std::mutex                                        fMetricMutex;
    std::map<MetricKey, MetricValue>                          fMetrics;
    std::map<DetectorKey, std::map<std::string, MetricValue>> fLatestRegisterValues;
    std::map<ModuleKey, std::size_t>                          fModuleChipCounts;
    std::vector<VirtualRegisterDefinition>                    fVirtualRegisterDefinitions;
    Configuration                                             fConfiguration;

    std::atomic<bool> fRunning{false};
    std::atomic<int>  fServerSocket{-1};
    std::thread       fServerThread;
};

#endif
