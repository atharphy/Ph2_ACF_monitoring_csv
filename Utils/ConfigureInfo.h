#ifndef __CONFIGURATION_INFO__
#define __CONFIGURATION_INFO__

#include "MessageUtils/cpp/QueryMessage.pb.h"
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

class DetectorContainer;

class ConfigureInfo
{
  public:
    ConfigureInfo();
    ~ConfigureInfo();

    void        setConfigurationFile(const std::string& theConfigurationFile) { fConfigurationFile = theConfigurationFile; }
    std::string getConfigurationFile() const { return fConfigurationFile; }

    void        setCalibrationName(const std::string& theCalibrationName) { fCalibrationName = theCalibrationName; }
    std::string getCalibrationName() const { return fCalibrationName; }

    void setEnabledObjects(DetectorContainer* theDetectorContainer) const;

    void parseProtobufMessage(const std::string& theConfigureInfoString);

    std::string createProtobufMessage() const;

    void enableBoard(uint16_t id, const std::string name) { enableObject(MessageUtils::ObjectType::BOARD, id, name); }
    void enableOpticalGroup(uint16_t id, const std::string name) { enableObject(MessageUtils::ObjectType::OPTICALGROUP, id, name); }
    void enableHybrid(uint16_t id, const std::string name) { enableObject(MessageUtils::ObjectType::HYBRID, id, name); }
    void enableChip(uint16_t id, const std::string name) { enableObject(MessageUtils::ObjectType::CHIP, id, name); }

  private:
    std::string fConfigurationFile{""};
    std::string fCalibrationName{""};

    std::unordered_map<MessageUtils::ObjectType::ObjectTypeEnum, std::unordered_map<uint16_t, std::string>> fObjectList;

    void enableObject(MessageUtils::ObjectType::ObjectTypeEnum theObjectType, uint16_t objectId, const std::string& objectName = "") { fObjectList[theObjectType][objectId] = objectName; }
};

#endif