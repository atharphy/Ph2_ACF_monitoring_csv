#ifndef PROMETHEUS_EXPORTER_H
#define PROMETHEUS_EXPORTER_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>

namespace Ph2_HwInterface
{
class RD53Interface;
}

class PrometheusExporter
{
  public:
    static PrometheusExporter& getInstance();

    void start(uint16_t port = 9101);
    void stop();

    void update(int                boardId,
                int                opticalGroupId,
                int                hybridId,
                int                chipId,
                const std::string& registerName,
                double             value,
                Ph2_HwInterface::RD53Interface* rd53Interface);

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

    using MetricKey = std::tuple<int, int, int, int, std::string, std::string>;

    void        run();
    void        handleClient(int clientSocket) const;
    std::string renderMetrics() const;

    static double      correctionFactor(const std::string& registerName);
    static std::string escapeLabelValue(const std::string& value);
    static std::string renderLabels(const MetricKey& key);
    static void        sendResponse(int clientSocket, int statusCode, const std::string& statusText, const std::string& contentType, const std::string& body);
    static void        sendAll(int clientSocket, const std::string& response);

    mutable std::mutex               fMetricMutex;
    std::map<MetricKey, MetricValue> fMetrics;

    std::atomic<bool> fRunning{false};
    std::atomic<int>  fServerSocket{-1};
    std::thread       fServerThread;
};

#endif
