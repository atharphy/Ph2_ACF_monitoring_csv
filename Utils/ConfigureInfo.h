#ifndef __CONFIGURATION_INFO__
#define __CONFIGURATION_INFO__

#include <string>
#include <vector>
#include <tuple>
#include <unordered_map>

class DetectorContainer;

class ConfigureInfo
{
  public:
    ConfigureInfo();
    ~ConfigureInfo();
    enum ObjectType {Board, OpticalGroup, Hybrid, ReadoutChip};

    void setConfigurationFile(const std::string& theConfigurationFile) {fConfigurationFile = theConfigurationFile;}
    std::string getConfigurationFile() const {return fConfigurationFile;}

    void setCalibrationName(const std::string& theCalibrationName) {fCalibrationName = theCalibrationName;}
    std::string getCalibrationName() const {return fCalibrationName;}

    void enableObject(ObjectType theObjectType, uint16_t objectId, const std::string& objectName="")
    {
        fObjectList[theObjectType][objectId] = objectName;
    }
    auto getModuleNameMap() const {return fObjectList;}

    void setEnabledObjects(DetectorContainer* theDetectorContainer) const;


  private:
    std::string fConfigurationFile {""};
    std::string fCalibrationName {""};

    std::unordered_map<ObjectType, std::unordered_map<uint16_t, std::string>> fObjectList;

};

#endif