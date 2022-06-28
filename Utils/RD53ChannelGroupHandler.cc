/*!
  \file                  RD53ChannelGroupHandler.cc
  \brief                 Channel container handler
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ChannelGroupHandler.h"

RD53ChannelGroupHandler::RD53ChannelGroupHandler(ChannelGroupBase& customChannelGroup,
                                                 ChannelGroupBase* allChannelGroup,
                                                 ChannelGroupBase* currentChannelGroup,
                                                 uint8_t           groupType,
                                                 uint8_t           hitPerCol,
                                                 uint8_t           onlyNGroups)
{
    allChannelGroup_     = std::shared_ptr<ChannelGroupBase>(allChannelGroup);
    currentChannelGroup_ = std::shared_ptr<ChannelGroupBase>(currentChannelGroup);

    if(groupType == RD53GroupType::AllGroups)
        numberOfGroups_ = (onlyNGroups == 0 ? customChannelGroup.getNumberOfRows() : onlyNGroups) / hitPerCol;
    else
        numberOfGroups_ = 1;

    // ###############################
    // # Refine custom channel group #
    // ###############################
    this->setCustomChannelGroup(customChannelGroup);
    customChannelGroup.disableAllChannels();

    for(auto it = 0u; it < numberOfGroups_; it++)
    {
        allChannelGroup_->makeTestGroup(currentChannelGroup_, it, 1, 1, 1);

        for(auto row = 0u; row < customChannelGroup.getNumberOfRows(); row++)
            for(auto col = 0u; col < customChannelGroup.getNumberOfRows(); col++)
                if(currentChannelGroup_->isChannelEnabled(row, col) == true) customChannelGroup.enableChannel(row, col);
    }
}

RD53ChannelGroupHandler::~RD53ChannelGroupHandler() {}
