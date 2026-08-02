#include "MonitorUtils/CablingDatabaseReader.h"

#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#ifdef PH2_HAVE_CURL
#include <curl/curl.h>
#endif

namespace
{
using json = nlohmann::json;

uint16_t parseIdentifier(const json& entry, const std::string& name)
{
    if(!entry.contains(name)) throw std::runtime_error("Missing cabling field '" + name + "'");

    long long value = 0;
    if(entry[name].is_number_integer())
        value = entry[name].get<long long>();
    else if(entry[name].is_string())
    {
        const std::string text = entry[name].get<std::string>();
        std::size_t       parsedCharacters = 0;
        value = std::stoll(text, &parsedCharacters);
        if(parsedCharacters != text.size()) throw std::runtime_error("Invalid cabling identifier '" + name + "'");
    }
    else
        throw std::runtime_error("Invalid cabling identifier '" + name + "'");

    if(value < 0 || value > std::numeric_limits<uint16_t>::max()) throw std::runtime_error("Cabling identifier out of range: '" + name + "'");
    return static_cast<uint16_t>(value);
}

std::string parseText(const json& entry, const std::string& name)
{
    if(!entry.contains(name)) throw std::runtime_error("Missing cabling field '" + name + "'");
    const auto& value = entry[name];
    if(value.is_string()) return value.get<std::string>();
    if(value.is_number_integer()) return std::to_string(value.get<long long>());
    if(value.is_number_unsigned()) return std::to_string(value.get<unsigned long long>());
    throw std::runtime_error("Invalid cabling field '" + name + "'");
}

#ifdef PH2_HAVE_CURL
size_t writeResponse(char* data, size_t size, size_t count, void* output)
{
    const size_t bytes = size * count;
    static_cast<std::string*>(output)->append(data, bytes);
    return bytes;
}

struct CurlHandleDeleter
{
    void operator()(CURL* handle) const { curl_easy_cleanup(handle); }
};

struct CurlHeadersDeleter
{
    void operator()(curl_slist* headers) const { curl_slist_free_all(headers); }
};
#endif
}

CablingDatabaseReader::CablingMap CablingDatabaseReader::parseJson(const std::string& jsonText)
{
    const json document = json::parse(jsonText);
    const json* entries = nullptr;

    if(document.is_array())
        entries = &document;
    else if(document.is_object() && document.contains("chips") && document["chips"].is_array())
        entries = &document["chips"];
    else
        throw std::runtime_error("Cabling response must be an array or an object containing a 'chips' array");

    CablingMap result;
    for(const auto& jsonEntry: *entries)
    {
        if(!jsonEntry.is_object()) throw std::runtime_error("Each cabling entry must be a JSON object");

        const std::string chipField = jsonEntry.contains("hardware_chip") ? "hardware_chip" : "chip";
        CablingEntry entry{parseIdentifier(jsonEntry, "board"),
                           parseIdentifier(jsonEntry, "optical_group"),
                           parseIdentifier(jsonEntry, "hybrid"),
                           parseIdentifier(jsonEntry, chipField),
                           parseText(jsonEntry, "subdetector"),
                           parseText(jsonEntry, "section_type"),
                           parseText(jsonEntry, "section_index"),
                           parseText(jsonEntry, "element_type"),
                           parseText(jsonEntry, "element_index"),
                           parseText(jsonEntry, "module_type"),
                           parseText(jsonEntry, "module_index"),
                           parseText(jsonEntry, "chip_type"),
                           parseText(jsonEntry, "chip_index"),
                           parseText(jsonEntry, "side")};

        const CablingKey key{entry.board, entry.opticalGroup, entry.hybrid, entry.hardwareChip};
        if(!result.emplace(key, std::move(entry)).second) throw std::runtime_error("Duplicate hardware address in cabling response");
    }
    return result;
}

CablingDatabaseReader::CablingMap CablingDatabaseReader::read(const std::string& endpoint, const std::string& bearerToken, unsigned int timeoutSeconds) const
{
#ifndef PH2_HAVE_CURL
    (void)endpoint;
    (void)bearerToken;
    (void)timeoutSeconds;
    throw std::runtime_error("Ph2_ACF was built without libcurl support");
#else
    if(endpoint.empty()) throw std::runtime_error("The cabling database endpoint is empty");

    static const CURLcode curlInitialization = curl_global_init(CURL_GLOBAL_DEFAULT);
    if(curlInitialization != CURLE_OK) throw std::runtime_error("Could not initialize libcurl");

    std::unique_ptr<CURL, CurlHandleDeleter> handle(curl_easy_init());
    if(!handle) throw std::runtime_error("Could not create a libcurl handle");

    std::string response;
    curl_easy_setopt(handle.get(), CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(handle.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT, static_cast<long>(timeoutSeconds));
    curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, writeResponse);
    curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &response);

    std::unique_ptr<curl_slist, CurlHeadersDeleter> headers(nullptr);
    if(!bearerToken.empty())
    {
        const std::string authorization = "Authorization: Bearer " + bearerToken;
        headers.reset(curl_slist_append(nullptr, authorization.c_str()));
        if(!headers) throw std::runtime_error("Could not create the cabling database authorization header");
        curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, headers.get());
    }

    const CURLcode requestResult = curl_easy_perform(handle.get());
    if(requestResult != CURLE_OK) throw std::runtime_error(std::string("Cabling database request failed: ") + curl_easy_strerror(requestResult));

    long statusCode = 0;
    curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &statusCode);
    if(statusCode < 200 || statusCode >= 300) throw std::runtime_error("Cabling database returned HTTP status " + std::to_string(statusCode));

    return parseJson(response);
#endif
}
