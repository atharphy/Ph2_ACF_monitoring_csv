#ifndef __DQM_CALIBRATION_FACTORY__
#define __DQM_CALIBRATION_FACTORY__

#include "DQMUtils/DQMHistogramBase.h"
#include "MessageUtils/cpp/QueryMessage.pb.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

// namespace detail 
// {
//     template<typename T, typename... Rest>
//     void addToList(std::vector<DQMHistogramBase*>& theVectorList)
//     {
//         theVectorList.emplace_back(new T());
//         addToList<Rest...>(theVectorList);
//     }

//     // template<typename T>
//     // void addToList(std::vector<DQMHistogramBase*>& theVectorList)
//     // {
//     //     theVectorList.emplace_back(new T());
//     // }

//     void addToList(std::vector<DQMHistogramBase*>& theVectorList) {}
// }

template<typename...>
struct Lister
{
    static void addToList(std::vector<DQMHistogramBase*>& theVectorList) {}
};


template<typename T, typename... Rest>
struct Lister<T, Rest...>
{
    static void addToList(std::vector<DQMHistogramBase*>& theVectorList)
    {
        theVectorList.emplace_back(new T());
        Lister<Rest...>::addToList(theVectorList);
    }
};

class DQMBaseCreator
{
  public:
    DQMBaseCreator() {}
    virtual ~DQMBaseCreator() {}
    virtual std::vector<DQMHistogramBase*> Create() const = 0;
};

template <typename... Args>
class DQMCreator : public DQMBaseCreator
{
  public:
    DQMCreator() {}
    virtual ~DQMCreator() {}

    std::vector<DQMHistogramBase*> Create() const override 
    {
        std::vector<DQMHistogramBase*> theVectorList;
        Lister<Args...>::addToList(theVectorList);
        return theVectorList;
    }
};

class DQMCalibrationFactory
{
  public:
    DQMCalibrationFactory();
    ~DQMCalibrationFactory();


    template <typename... Args>
    void Register(const std::string& calibrationTag, MessageUtils::CalibrationList::CalibrationNameEnum theCalibrationEnum)
    {
        if(fDQMInterfaceMap.count(calibrationTag))
        {
            std::cerr << "calibrationTag " << calibrationTag << " already exists, aborting..." << std::endl;
            abort();
        }
        if(fCalibrationNameToString.count(theCalibrationEnum))
        {
            std::cerr << "calibrationEnum for calibration tag " << calibrationTag << " already exists, aborting..." << std::endl;
            abort();
        }
        fDQMInterfaceMap[calibrationTag]              = new DQMCreator<Args...>;
        fCalibrationNameToString[theCalibrationEnum] = calibrationTag;
        fStringToCalibrationName[calibrationTag]     = theCalibrationEnum;
    }

    std::vector<DQMHistogramBase*> createDQMHistogrammerVector(const std::string& calibrationTag) const;

    std::vector<std::string>                                                  getAvailableCalibrations() const;
    std::map<MessageUtils::CalibrationList::CalibrationNameEnum, std::string> getAvailableCalibrationMap() const { return fCalibrationNameToString; }
    const std::string& getCalibrationName(MessageUtils::CalibrationList::CalibrationNameEnum theCalibrationEnum) const { return fCalibrationNameToString.at(theCalibrationEnum); }
    const MessageUtils::CalibrationList::CalibrationNameEnum& getCalibrationEnum(std::string theCalibrationName) const { return fStringToCalibrationName.at(theCalibrationName); }

  private:
    std::map<std::string, DQMBaseCreator*>                                    fDQMInterfaceMap;
    std::map<MessageUtils::CalibrationList::CalibrationNameEnum, std::string> fCalibrationNameToString;
    std::map<std::string, MessageUtils::CalibrationList::CalibrationNameEnum> fStringToCalibrationName;



};

#endif