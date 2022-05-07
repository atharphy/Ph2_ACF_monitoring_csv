#ifndef __COMBINED_CALIBRATION_FACTORY__
#define __COMBINED_CALIBRATION_FACTORY__

#include <string>
#include <map>
#include <iostream>
#include "../tools/Tool.h"
#include "../tools/CombinedCalibration.h"
#include "../MessageUtils/cpp/QueryMessage.pb.h"

template<typename... Args>
class CombineCalibration;

class BaseCreator
{
  public:
    BaseCreator(){}
    virtual ~BaseCreator(){}
    virtual Tool* Create() const = 0;
};

template<typename... Args>
class Creator : public BaseCreator
{
  public:
    Creator(){}
    virtual ~Creator(){}
    Tool* Create() const override {return new CombinedCalibration<Args...>();};
};


class CombinedCalibrationFactory
{
  public:
    CombinedCalibrationFactory();
    ~CombinedCalibrationFactory();

    template<typename... Args>
    void Register(const std::string& calibrationTag, MessageUtils::ConfigurationInfo::CalibrationNameEnum theCalibrationEnum)
    {
        if (fCalibrationMap.count(calibrationTag))
        {
            std::cerr << "calibrationTag " << calibrationTag << " already exists, aborting..." << std::endl;
            abort();
        }
        if (fCalibrationNameToString.count(theCalibrationEnum))
        {
            std::cerr << "calibrationEnum for calibration tag " << calibrationTag << " already exists, aborting..." << std::endl;
            abort();
        }
        fCalibrationMap[calibrationTag] = new Creator<Args...>;
        fCalibrationNameToString[theCalibrationEnum] = calibrationTag;
        fStringToCalibrationName[calibrationTag] = theCalibrationEnum;
    }

    Tool* CreateCombinedCalibration(const std::string& calibrationTag) const;
    
    std::vector<std::string> getAvailableCalibrations() const;
    const std::string& getCalibrationName(MessageUtils::ConfigurationInfo::CalibrationNameEnum theCalibrationEnum) const {return fCalibrationNameToString.at(theCalibrationEnum);}
    const MessageUtils::ConfigurationInfo::CalibrationNameEnum& getCalibrationEnum(std::string theCalibrationName) const {return fStringToCalibrationName.at(theCalibrationName);}

  private:
    std::map<std::string, BaseCreator*> fCalibrationMap;
    std::map<MessageUtils::ConfigurationInfo::CalibrationNameEnum, std::string> fCalibrationNameToString;
    std::map<std::string, MessageUtils::ConfigurationInfo::CalibrationNameEnum> fStringToCalibrationName;

};

#endif
