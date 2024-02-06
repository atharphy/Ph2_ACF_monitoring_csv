/*!
  Filename :                              OpticalGroup.cc
  Content :                               OpticalGroup Description class
  Programmer :                    Lorenzo BIDEGAIN
  Version :               1.0
  Date of Creation :              25/06/14
  Support :                               mail to : lorenzo.bidegain@gmail.com
*/

#include "OpticalGroup.h"

namespace Ph2_HwDescription
{
// Default C'tor
OpticalGroup::OpticalGroup() : FrontEndDescription(), OpticalGroupContainer(0) {}

OpticalGroup::OpticalGroup(const FrontEndDescription& pFeDesc, uint8_t pOpticalGroupId) : FrontEndDescription(pFeDesc), OpticalGroupContainer(pOpticalGroupId) {}

OpticalGroup::OpticalGroup(uint8_t pBeBoardId, uint8_t pFMCId, uint8_t pOpticalGroupId) : FrontEndDescription(pBeBoardId, pFMCId, pOpticalGroupId, 0), OpticalGroupContainer(pOpticalGroupId) {}

std::map<uint8_t, std::vector<uint8_t>> OpticalGroup::getLpGBTrxGroupsAndChannels() const
{
    std::map<uint8_t, std::vector<uint8_t>> groupsAndChannels;
    groupsAndChannels[0] = {0, 2};
    groupsAndChannels[1] = {0, 2};
    groupsAndChannels[2] = {0, 2};
    groupsAndChannels[4] = {0, 2};
    groupsAndChannels[5] = {0, 2};

    if(getFrontEndType() == FrontEndType::OuterTracker2S)
    {
        groupsAndChannels[3] = {2};
        groupsAndChannels[6] = {0};
    }
    else
    {
        groupsAndChannels[3] = {0, 2};
        groupsAndChannels[6] = {0, 2};
    }
    return groupsAndChannels;
}

} // namespace Ph2_HwDescription
