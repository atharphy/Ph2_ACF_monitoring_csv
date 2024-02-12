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

std::map< std::pair<uint8_t,uint8_t>, std::string> OpticalGroup::getLpGBTrxGroupsAndChannelsPerHybrid() const
{

    std::map< std::pair<uint8_t,uint8_t>, std::string> hybridsAndGroupsAndChannels;
    
    if(getFrontEndType() == FrontEndType::OuterTracker2S)
    {
        // Right CIC 
        hybridsAndGroupsAndChannels[std::make_pair(0,0)] = "FEHR_L1";
        hybridsAndGroupsAndChannels[std::make_pair(4,0)] = "FEHR_Stub0";
        hybridsAndGroupsAndChannels[std::make_pair(4,2)] = "FEHR_Stub1";
        hybridsAndGroupsAndChannels[std::make_pair(5,0)] = "FEHR_Stub2";
        hybridsAndGroupsAndChannels[std::make_pair(5,2)] = "FEHR_Stub3";
        hybridsAndGroupsAndChannels[std::make_pair(6,0)] = "FEHR_Stub4";
        // Left CIC 
        hybridsAndGroupsAndChannels[std::make_pair(0,2)] = "FEHL_Stub0";
        hybridsAndGroupsAndChannels[std::make_pair(1,0)] = "FEHL_Stub1";
        hybridsAndGroupsAndChannels[std::make_pair(1,2)] = "FEHL_Stub2";
        hybridsAndGroupsAndChannels[std::make_pair(2,0)] = "FEHL_Stub3";
        hybridsAndGroupsAndChannels[std::make_pair(2,2)] = "FEHL_Stub4";
        hybridsAndGroupsAndChannels[std::make_pair(3,2)] = "FEHL_L1";


    }
    else
    {
        // Right CIC 
        hybridsAndGroupsAndChannels[std::make_pair(0,0)] = "FEHR_Stub0";
        hybridsAndGroupsAndChannels[std::make_pair(4,0)] = "FEHR_Stub5";
        hybridsAndGroupsAndChannels[std::make_pair(4,2)] = "FEHR_L1";
        hybridsAndGroupsAndChannels[std::make_pair(5,0)] = "FEHR_Stub4";
        hybridsAndGroupsAndChannels[std::make_pair(5,2)] = "FEHR_Stub3";
        hybridsAndGroupsAndChannels[std::make_pair(6,0)] = "FEHR_Stub2";
        hybridsAndGroupsAndChannels[std::make_pair(6,2)] = "FEHR_Stub1";

        // Left CIC 
        hybridsAndGroupsAndChannels[std::make_pair(0,2)] = "FEHL_Stub5";
        hybridsAndGroupsAndChannels[std::make_pair(1,0)] = "FEHL_L1";
        hybridsAndGroupsAndChannels[std::make_pair(1,2)] = "FEHL_Stub4";
        hybridsAndGroupsAndChannels[std::make_pair(2,0)] = "FEHL_Stub3";
        hybridsAndGroupsAndChannels[std::make_pair(2,2)] = "FEHL_Stub2";
        hybridsAndGroupsAndChannels[std::make_pair(3,0)] = "FEHL_Stub1";
        hybridsAndGroupsAndChannels[std::make_pair(3,2)] = "FEHL_Stub0";


    }
    return hybridsAndGroupsAndChannels;
}



} // namespace Ph2_HwDescription
