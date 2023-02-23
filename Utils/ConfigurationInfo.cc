#include "ConfigureInfo.h"
#include "Container.h"

ConfigureInfo::ConfigureInfo()
{
}

ConfigureInfo::~ConfigureInfo()
{
}

void ConfigureInfo::setEnabledObjects(DetectorContainer* theDetectorContainer) const
{
    auto getIdList = [this](const ObjectType theObjectType) -> std::set<int16_t>
    {
        std::set<int16_t> enabledIdList;
        if(this->fObjectList.find(theObjectType) != fObjectList.end())
        {
            for(const auto& entry : fObjectList.at(theObjectType)) enabledIdList.insert(entry.first);
        }
        return enabledIdList;
    };
    
    auto enabledBoardIds = getIdList(ObjectType::Board);
    if(enabledBoardIds.size()>0)
    {
        auto enableBoardListFunction = [enabledBoardIds](const BoardContainer* theBoard)
        {
            return enabledBoardIds.find(theBoard->getId()) != enabledBoardIds.end();
        };
        theDetectorContainer->addBoardQueryFunction(enableBoardListFunction);
    }

    auto enabledOpticalGroupIds = getIdList(ObjectType::OpticalGroup);
    if(enabledOpticalGroupIds.size()>0)
    {
        auto enableOpticalGroupListFunction = [enabledOpticalGroupIds](const OpticalGroupContainer* theOpticalGroup)
        {
            return enabledOpticalGroupIds.find(theOpticalGroup->getId()) != enabledOpticalGroupIds.end();
        };
        theDetectorContainer->addOpticalGroupQueryFunction(enableOpticalGroupListFunction);
    }

    auto enabledHybridIds = getIdList(ObjectType::Hybrid);
    if(enabledHybridIds.size()>0)
    {
        auto enableHybridListFunction = [enabledHybridIds](const HybridContainer* theHybrid)
        {
            return enabledHybridIds.find(theHybrid->getId()) != enabledHybridIds.end();
        };
        theDetectorContainer->addHybridQueryFunction(enableHybridListFunction);
    }

    auto enabledReadoutChipIds = getIdList(ObjectType::ReadoutChip);
    if(enabledReadoutChipIds.size()>0)
    {
        auto enableReadoutChipListFunction = [enabledReadoutChipIds](const ChipContainer* theReadoutChip)
        {
            return enabledReadoutChipIds.find(theReadoutChip->getId()) != enabledReadoutChipIds.end();
        };
        theDetectorContainer->addReadoutChipQueryFunction(enableReadoutChipListFunction);
    }
}


