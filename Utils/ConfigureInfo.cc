#include "Utils/ConfigureInfo.h"
#include "Utils/Container.h"

ConfigureInfo::ConfigureInfo() {}

ConfigureInfo::~ConfigureInfo() {}

void ConfigureInfo::setEnabledObjects(DetectorContainer* theDetectorContainer) const
{
    auto getIdList = [this](const MessageUtils::ObjectType::ObjectTypeEnum theObjectType) -> std::set<int16_t> {
        std::set<int16_t> enabledIdList;
        if(this->fObjectList.find(theObjectType) != fObjectList.end())
        {
            for(const auto& entry: fObjectList.at(theObjectType)) enabledIdList.insert(entry.first);
        }
        return enabledIdList;
    };

    auto enabledBoardIds = getIdList(MessageUtils::ObjectType::BOARD);
    if(enabledBoardIds.size() > 0)
    {
        auto enableBoardListFunction = [enabledBoardIds](const BoardContainer* theBoard) { return enabledBoardIds.find(theBoard->getId()) != enabledBoardIds.end(); };
        theDetectorContainer->addBoardQueryFunction(enableBoardListFunction);
    }

    auto enabledOpticalGroupIds = getIdList(MessageUtils::ObjectType::OPTICALGROUP);
    if(enabledOpticalGroupIds.size() > 0)
    {
        auto enableOpticalGroupListFunction = [enabledOpticalGroupIds](const OpticalGroupContainer* theOpticalGroup) {
            return enabledOpticalGroupIds.find(theOpticalGroup->getId()) != enabledOpticalGroupIds.end();
        };
        theDetectorContainer->addOpticalGroupQueryFunction(enableOpticalGroupListFunction);
    }

    auto enabledHybridIds = getIdList(MessageUtils::ObjectType::HYBRID);
    if(enabledHybridIds.size() > 0)
    {
        auto enableHybridListFunction = [enabledHybridIds](const HybridContainer* theHybrid) { return enabledHybridIds.find(theHybrid->getId()) != enabledHybridIds.end(); };
        theDetectorContainer->addHybridQueryFunction(enableHybridListFunction);
    }

    auto enabledReadoutChipIds = getIdList(MessageUtils::ObjectType::CHIP);
    if(enabledReadoutChipIds.size() > 0)
    {
        auto enableReadoutChipListFunction = [enabledReadoutChipIds](const ChipContainer* theReadoutChip) {
            return enabledReadoutChipIds.find(theReadoutChip->getId()) != enabledReadoutChipIds.end();
        };
        theDetectorContainer->addReadoutChipQueryFunction(enableReadoutChipListFunction);
    }
}

void ConfigureInfo::parseProtobufMessage(const std::string& theConfigureInfoString)
{
    MessageUtils::ConfigurationMessage theConfigureMessage;
    theConfigureMessage.ParseFromString(theConfigureInfoString);

    fConfigurationFile = theConfigureMessage.data().configuration_file();
    fCalibrationName   = theConfigureMessage.data().calibration_name();

    for(const auto& objectInfo: theConfigureMessage.data().object_list()) { fObjectList[objectInfo.object_type().type()][uint16_t(objectInfo.id())] = objectInfo.name(); }
}

std::string ConfigureInfo::createProtobufMessage() const
{
    MessageUtils::ConfigurationMessage theConfigureMessage;
    theConfigureMessage.mutable_query_type()->set_type(MessageUtils::QueryType::CONFIGURE);
    theConfigureMessage.mutable_data()->set_calibration_name(fCalibrationName);
    theConfigureMessage.mutable_data()->set_configuration_file(fConfigurationFile);

    for(const auto& theObjectTypeMap: fObjectList)
    {
        for(const auto& theObjectIdAndName: theObjectTypeMap.second)
        {
            auto theObject = theConfigureMessage.mutable_data()->add_object_list();
            theObject->mutable_object_type()->set_type(theObjectTypeMap.first);
            theObject->set_id(theObjectIdAndName.first);
            theObject->set_name(theObjectIdAndName.second);
        }
    }

    std::string theConfigurationMessageString;
    theConfigureMessage.SerializeToString(&theConfigurationMessageString);
    return theConfigurationMessageString;
}

std::unordered_map<uint16_t, std::string> ConfigureInfo::getEnabledModulesList(bool isOT) const
{
    auto objectType = isOT ? MessageUtils::ObjectType::OPTICALGROUP : MessageUtils::ObjectType::HYBRID;
    if(this->fObjectList.find(objectType) != fObjectList.end())
        return fObjectList.at((isOT ? MessageUtils::ObjectType::OPTICALGROUP : MessageUtils::ObjectType::HYBRID));
    return std::unordered_map<uint16_t, std::string>();
}
