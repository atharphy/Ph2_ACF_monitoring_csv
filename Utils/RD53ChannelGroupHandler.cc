/*!
  \file                  RD53ChannelGroupHandler.cc
  \brief                 Channel container handler
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ChannelGroupHandler.h"

void RD53ChannelGroupHandler::RD53ChannelGroupAll::makeTestGroup(std::shared_ptr<ChannelGroupBase>& currentChannelGroup,
                                                                 uint32_t                           groupNumber,
                                                                 uint32_t                           numberOfClustersPerGroup,
                                                                 uint16_t                           numberOfRowsPerCluster,
                                                                 uint16_t                           numberOfColsPerCluster) const
{
    static_cast<ChannelGroup*>(currentChannelGroup.get())->disableAllChannels();

    for(auto row = 0u; row < Ph2_HwDescription::RD53::nRows; row++)
        for(auto col = 0u; col < Ph2_HwDescription::RD53::nCols; col++)
            if(isChannelEnabled(row, col)) static_cast<RD53ChannelGroupAll*>(currentChannelGroup.get())->enableChannel(row, col);
}

void RD53ChannelGroupHandler::RD53ChannelGroupPattern::makeTestGroup(std::shared_ptr<ChannelGroupBase>& currentChannelGroup,
                                                                     uint32_t                           groupNumber,
                                                                     uint32_t                           numberOfClustersPerGroup,
                                                                     uint16_t                           numberOfRowsPerCluster,
                                                                     uint16_t                           numberOfColsPerCluster) const
{
    static_cast<ChannelGroup*>(currentChannelGroup.get())->disableAllChannels();

    for(auto col = 0u; col < Ph2_HwDescription::RD53::nCols; col++)
        for(auto i = 0u; i < hitPerCol; i++)
        {
            auto row = (RD53Constants::NROW_CORE * col + i * Ph2_HwDescription::RD53::nRows / hitPerCol) % Ph2_HwDescription::RD53::nRows;
            row += groupNumber;
            row %= Ph2_HwDescription::RD53::nRows;
            if(isChannelEnabled(row, col) == true) static_cast<RD53ChannelGroupPattern*>(currentChannelGroup.get())->enableChannel(row, col);
        }
}

RD53ChannelGroupHandler::RD53ChannelGroupHandler(ChannelGroup<Ph2_HwDescription::RD53::nRows, Ph2_HwDescription::RD53::nCols>& customChannelGroup, uint8_t groupType, uint8_t hitPerCol)
{
    if(groupType == RD53GroupType::AllPixels)
    {
        allChannelGroup_     = std::make_shared<RD53ChannelGroupAll>();
        currentChannelGroup_ = std::make_shared<RD53ChannelGroupAll>();
    }
    else
    {
        allChannelGroup_     = std::make_shared<RD53ChannelGroupPattern>(hitPerCol);
        currentChannelGroup_ = std::make_shared<RD53ChannelGroupPattern>(hitPerCol);
    }

    numberOfGroups_ = getNumberOfGroups(groupType, hitPerCol);

    // ###############################
    // # Refine custom channel group #
    // ###############################
    this->setCustomChannelGroup(customChannelGroup);
    customChannelGroup.disableAllChannels();

    for(auto it = 0u; it < numberOfGroups_; it++)
    {
        allChannelGroup_->makeTestGroup(currentChannelGroup_, it, 1, 1, 1);

        for(auto row = 0u; row < Ph2_HwDescription::RD53::nRows; row++)
            for(auto col = 0u; col < Ph2_HwDescription::RD53::nCols; col++)
                if(static_cast<const ChannelGroup<Ph2_HwDescription::RD53::nRows, Ph2_HwDescription::RD53::nCols>*>(currentChannelGroup_.get())->isChannelEnabled(row, col) == true)
                    customChannelGroup.enableChannel(row, col);
    }
}

RD53ChannelGroupHandler::~RD53ChannelGroupHandler() {}
