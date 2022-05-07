#ifndef __COMBINED_CALIBRATION_FACTORY__
#define __COMBINED_CALIBRATION_FACTORY__

#include <string>
#include <map>
#include <iostream>
#include "../tools/Tool.h"
#include "../tools/CombinedCalibration.h"

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
    void Register(const std::string& calibrationTag)
    {
        if (fCalibrationMap.count(calibrationTag))
        {
            std::cerr << "calibrationTag " << calibrationTag << " already exists, aborting..." << std::endl;
            abort();
        }
        fCalibrationMap[calibrationTag] = new Creator<Args...>;
    }

    Tool* CreateCombinedCalibration(const std::string& calibrationTag) const;
    
    std::vector<std::string> getAvailableCalibrations() const;

  private:
    std::map<std::string, BaseCreator*> fCalibrationMap;

};

#endif
