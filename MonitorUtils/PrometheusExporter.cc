#include "MonitorUtils/PrometheusExporter.h"
#include "HWInterface/RD53Interface.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <netinet/in.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/select.h>
#include <sys/socket.h>
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

class VirtualExpressionParser
{
  public:
    VirtualExpressionParser(const std::string& expression, const std::map<std::string, double>& values) : fExpression(expression), fValues(values) {}

    bool evaluate(double& result, std::set<std::string>& usedRegisters)
    {
        fPosition = 0;
        fUsedRegisters.clear();
        if(!parseExpression(result)) return false;
        skipWhitespace();
        if(fPosition != fExpression.size() || !std::isfinite(result)) return false;
        usedRegisters = fUsedRegisters;
        return true;
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
        if(fExpression[fPosition] == '"')
        {
            const size_t start = ++fPosition;
            while(fPosition < fExpression.size() && fExpression[fPosition] != '"') ++fPosition;
            if(fPosition >= fExpression.size()) return false;
            registerName = fExpression.substr(start, fPosition - start);
            ++fPosition;
        }
        else
        {
            const size_t start = fPosition;
            while(fPosition < fExpression.size() &&
                  (std::isalnum(static_cast<unsigned char>(fExpression[fPosition])) || fExpression[fPosition] == '_'))
                ++fPosition;
            registerName = fExpression.substr(start, fPosition - start);
        }

        const auto valueIt = fValues.find(registerName);
        if(valueIt == fValues.end()) return false;
        result = valueIt->second;
        fUsedRegisters.insert(registerName);
        return true;
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

    const std::string&              fExpression;
    const std::map<std::string, double>& fValues;
    size_t                          fPosition{0};
    std::set<std::string>           fUsedRegisters;
};
}

PrometheusExporter& PrometheusExporter::getInstance()
{
    static PrometheusExporter instance;
    return instance;
}

PrometheusExporter::~PrometheusExporter() { stop(); }

void PrometheusExporter::start(uint16_t port)
{
    if(fRunning) return;

    loadVirtualRegisterDefinitions();

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket < 0) throw std::runtime_error("[PrometheusExporter::start] Failed to create socket: " + std::string(std::strerror(errno)));

    int reuseAddress = 1;
    if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress)) != 0)
    {
        close(serverSocket);
        throw std::runtime_error("[PrometheusExporter::start] Failed to configure socket: " + std::string(std::strerror(errno)));
    }

#ifdef SO_NOSIGPIPE
    int noSigPipe = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe));
#endif

    sockaddr_in address{};
    address.sin_family      = AF_INET;
    address.sin_port        = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        const std::string error = std::strerror(errno);
        close(serverSocket);
        throw std::runtime_error("[PrometheusExporter::start] Failed to bind port " + std::to_string(port) + ": " + error);
    }

    if(listen(serverSocket, 8) != 0)
    {
        const std::string error = std::strerror(errno);
        close(serverSocket);
        throw std::runtime_error("[PrometheusExporter::start] Failed to listen: " + error);
    }

    fServerSocket = serverSocket;
    fRunning      = true;
    fServerThread = std::thread(&PrometheusExporter::run, this);
}

void PrometheusExporter::loadVirtualRegisterDefinitions()
{
    std::string configPath;
    if(const char* configuredPath = std::getenv("CMSIT_VIRTUAL_REGISTER_CONFIG")) configPath = configuredPath;
    else if(const char* baseDirectory = std::getenv("PH2ACF_BASE_DIR"))
        configPath = std::string(baseDirectory) + "/settings/virtual_registers.conf";
    else
        return;

    std::ifstream configFile(configPath);
    if(!configFile.is_open()) return;

    std::vector<VirtualRegisterDefinition> definitions;
    std::set<std::string>                   names;
    std::string                             line;
    size_t                                  lineNumber = 0;
    while(std::getline(configFile, line))
    {
        ++lineNumber;
        line = trim(line);
        if(line.empty() || line[0] == '#') continue;

        const size_t firstSeparator  = line.find('|');
        const size_t secondSeparator = firstSeparator == std::string::npos ? std::string::npos : line.find('|', firstSeparator + 1);
        if(firstSeparator == std::string::npos || secondSeparator == std::string::npos)
            throw std::runtime_error("[PrometheusExporter] Invalid virtual-register definition at " + configPath + ":" + std::to_string(lineNumber));

        VirtualRegisterDefinition definition{
            trim(line.substr(0, firstSeparator)),
            trim(line.substr(firstSeparator + 1, secondSeparator - firstSeparator - 1)),
            trim(line.substr(secondSeparator + 1))};

        if(definition.name.empty() || definition.expression.empty())
            throw std::runtime_error("[PrometheusExporter] Empty virtual-register name or expression at " + configPath + ":" + std::to_string(lineNumber));
        if(!names.insert(definition.name).second)
            throw std::runtime_error("[PrometheusExporter] Duplicate virtual-register name '" + definition.name + "' in " + configPath);

        definitions.push_back(definition);
    }

    fVirtualRegisterDefinitions = definitions;
}

void PrometheusExporter::stop()
{
    fRunning = false;
    const int serverSocket = fServerSocket.exchange(-1);
    if(serverSocket >= 0)
    {
        shutdown(serverSocket, SHUT_RDWR);
        close(serverSocket);
    }
    if(fServerThread.joinable()) fServerThread.join();
}

void PrometheusExporter::update(int                boardId,
                                int                opticalGroupId,
                                int                hybridId,
                                int                chipId,
                                const std::string& registerName,
                                double             value,
                                Ph2_HwInterface::RD53Interface* rd53Interface)
{
    if(!fRunning) return;

    bool       isCurrentNotVoltage = false;
    const bool isADCobservable =
        rd53Interface != nullptr && rd53Interface->getADCobservable(registerName, isCurrentNotVoltage, true) != -1;

    std::string unit;
    if(isADCobservable)
    {
        if((registerName.find("TEMPSENS") != std::string::npos) || (registerName.find("RADSENS") != std::string::npos) || (registerName.find("INTERNAL_NTC") != std::string::npos))
            unit = "C";
        else
            unit = (isCurrentNotVoltage ? "uA" : "V");
    }

    const double factor = correctionFactor(registerName);
    const MetricKey key{boardId, opticalGroupId, hybridId, chipId, registerName, unit};
    const MetricValue metric{value * factor, isADCobservable, (isADCobservable ? value * 0.04 * factor : 0), std::chrono::system_clock::now()};
    const DetectorKey detectorKey{boardId, opticalGroupId, hybridId, chipId};

    std::lock_guard<std::mutex> lock(fMetricMutex);
    fMetrics[key]                                    = metric;
    fLatestRegisterValues[detectorKey][registerName] = metric;
    updateVirtualRegistersLocked(detectorKey);
}

void PrometheusExporter::updateVirtualRegistersLocked(const DetectorKey& detectorKey)
{
    const auto detectorValuesIt = fLatestRegisterValues.find(detectorKey);
    if(detectorValuesIt == fLatestRegisterValues.end()) return;

    const auto& detectorValues = detectorValuesIt->second;
    std::map<std::string, double> correctedValues;
    for(const auto& value: detectorValues) correctedValues[value.first] = value.second.value;

    for(const auto& definition: fVirtualRegisterDefinitions)
    {
        double virtualValue = 0;
        std::set<std::string> usedRegisters;
        VirtualExpressionParser parser(definition.expression, correctedValues);
        if(!parser.evaluate(virtualValue, usedRegisters)) continue;

        auto updateTime = std::chrono::system_clock::time_point::min();
        for(const auto& registerName: usedRegisters) updateTime = std::max(updateTime, detectorValues.at(registerName).updateTime);

        const MetricKey virtualKey{
            std::get<0>(detectorKey), std::get<1>(detectorKey), std::get<2>(detectorKey), std::get<3>(detectorKey), definition.name, definition.unit};
        fMetrics[virtualKey] = MetricValue{virtualValue, false, 0, updateTime};
    }
}

void PrometheusExporter::run()
{
    while(fRunning)
    {
        const int serverSocket = fServerSocket.load();
        if(serverSocket < 0) break;

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverSocket, &readSet);
        timeval timeout{0, 200000};
        const int ready = select(serverSocket + 1, &readSet, nullptr, nullptr, &timeout);
        if(ready <= 0) continue;

        const int clientSocket = accept(serverSocket, nullptr, nullptr);
        if(clientSocket < 0)
        {
            if(fRunning) continue;
            break;
        }

#ifdef SO_NOSIGPIPE
        int noSigPipe = 1;
        setsockopt(clientSocket, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe));
#endif
        handleClient(clientSocket);
        shutdown(clientSocket, SHUT_RDWR);
        close(clientSocket);
    }
}

void PrometheusExporter::handleClient(int clientSocket) const
{
    char          buffer[4096];
    const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if(bytesRead <= 0) return;
    buffer[bytesRead] = '\0';

    std::istringstream request(std::string(buffer, static_cast<size_t>(bytesRead)));
    std::string method;
    std::string target;
    std::string version;
    request >> method >> target >> version;

    const auto queryPosition = target.find('?');
    if(queryPosition != std::string::npos) target.erase(queryPosition);

    if(method.empty() || target.empty() || version.empty())
    {
        sendResponse(clientSocket, 400, "Bad Request", "text/plain; charset=utf-8", "Invalid HTTP request.\n");
        return;
    }

    if(method != "GET")
    {
        sendResponse(clientSocket, 405, "Method Not Allowed", "text/plain; charset=utf-8", "Only GET is supported.\n");
        return;
    }

    if(target == "/metrics")
    {
        sendResponse(clientSocket, 200, "OK", "text/plain; version=0.0.4; charset=utf-8", renderMetrics());
        return;
    }

    sendResponse(clientSocket, 404, "Not Found", "text/plain; charset=utf-8", "Metrics are available at /metrics.\n");
}

std::string PrometheusExporter::renderMetrics() const
{
    std::map<MetricKey, MetricValue> snapshot;
    {
        std::lock_guard<std::mutex> lock(fMetricMutex);
        snapshot = fMetrics;
    }

    std::ostringstream output;
    output << std::setprecision(12);
    output << "# HELP cmsit_monitor_value CMSITminiDAQ live monitoring corrected value.\n";
    output << "# TYPE cmsit_monitor_value gauge\n";
    for(const auto& metric: snapshot) output << "cmsit_monitor_value" << renderLabels(metric.first) << " " << metric.second.value << "\n";

    output << "# HELP cmsit_monitor_error CMSITminiDAQ live monitoring corrected uncertainty.\n";
    output << "# TYPE cmsit_monitor_error gauge\n";
    for(const auto& metric: snapshot)
        if(metric.second.hasError) output << "cmsit_monitor_error" << renderLabels(metric.first) << " " << metric.second.error << "\n";

    output << "# HELP cmsit_monitor_last_update_seconds Unix timestamp of last CMSITminiDAQ monitoring update.\n";
    output << "# TYPE cmsit_monitor_last_update_seconds gauge\n";
    for(const auto& metric: snapshot)
    {
        const auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(metric.second.updateTime.time_since_epoch()).count();
        output << "cmsit_monitor_last_update_seconds" << renderLabels(metric.first) << " " << timestamp << "\n";
    }

    return output.str();
}

double PrometheusExporter::correctionFactor(const std::string& registerName)
{
    if(registerName == "ANA_IN_CURR" || registerName == "DIG_IN_CURR") return 21000.0;
    if(registerName == "ANA_SHUNT_CURR" || registerName == "DIG_SHUNT_CURR") return 21520.0;
    if(registerName == "VDDA" || registerName == "VDDD") return 2.0;
    if(registerName == "VINA" || registerName == "VOFS" || registerName == "VIND") return 4.0;
    return 1.0;
}

std::string PrometheusExporter::escapeLabelValue(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for(const char character: value)
    {
        if(character == '\\')
            escaped += "\\\\";
        else if(character == '"')
            escaped += "\\\"";
        else if(character == '\n')
            escaped += "\\n";
        else
            escaped += character;
    }
    return escaped;
}

std::string PrometheusExporter::renderLabels(const MetricKey& key)
{
    std::ostringstream labels;
    labels << "{board=\"" << std::get<0>(key) << "\",optical_group=\"" << std::get<1>(key) << "\",hybrid=\"" << std::get<2>(key) << "\",chip=\"" << std::get<3>(key)
           << "\",register=\"" << escapeLabelValue(std::get<4>(key)) << "\",unit=\"" << escapeLabelValue(std::get<5>(key)) << "\"}";
    return labels.str();
}

void PrometheusExporter::sendResponse(int clientSocket, int statusCode, const std::string& statusText, const std::string& contentType, const std::string& body)
{
    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "Cache-Control: no-cache\r\n";
    response << "Connection: close\r\n\r\n";
    response << body;
    sendAll(clientSocket, response.str());
}

void PrometheusExporter::sendAll(int clientSocket, const std::string& response)
{
    size_t sentBytes = 0;
    while(sentBytes < response.size())
    {
#ifdef MSG_NOSIGNAL
        const ssize_t result = send(clientSocket, response.data() + sentBytes, response.size() - sentBytes, MSG_NOSIGNAL);
#else
        const ssize_t result = send(clientSocket, response.data() + sentBytes, response.size() - sentBytes, 0);
#endif
        if(result <= 0) return;
        sentBytes += static_cast<size_t>(result);
    }
}
