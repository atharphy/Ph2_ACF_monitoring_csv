#ifndef CABLING_DATABASE_READER_H
#define CABLING_DATABASE_READER_H

#include <cstdint>
#include <map>
#include <string>
#include <tuple>

struct CablingEntry
{
    uint16_t    board;
    uint16_t    opticalGroup;
    uint16_t    hybrid;
    uint16_t    hardwareChip;
    std::string subdetector;
    std::string sectionType;
    std::string sectionIndex;
    std::string elementType;
    std::string elementIndex;
    std::string moduleType;
    std::string moduleIndex;
    std::string chipType;
    std::string chipIndex;
    std::string side;
};

class CablingDatabaseReader
{
  public:
    using CablingKey = std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>;
    using CablingMap = std::map<CablingKey, CablingEntry>;

    CablingMap read(const std::string& endpoint, const std::string& bearerToken, unsigned int timeoutSeconds) const;

    static CablingMap parseJson(const std::string& jsonText);
};

#endif
