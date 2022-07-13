/*!
  \file                  RD53ChannelGroupHandler.cc
  \brief                 Channel container handler
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ChannelGroupHandler.h"

RD53ChannelGroupHandler::RD53ChannelGroupHandler(size_t rowStart, size_t rowStop, size_t colStart, size_t colStop, size_t nRows, size_t nCols, uint8_t groupType, size_t hitPerCol, size_t onlyNGroups)
    : ChannelGroupHandler(), regionOfInterest(nRows, nCols), groupType(groupType), hitPerCol(hitPerCol), onlyNGroups(onlyNGroups)
{
    for(auto col = colStart; col <= colStop; col++)
        for(auto row = rowStart; row <= rowStop; ++row) regionOfInterest.enableChannel(row, col);

    allChannelGroup_ = std::shared_ptr<ChannelGroupBase>(&regionOfInterest, [](auto*) {});

    if(groupType == RD53GroupType::AllPixels)
        numberOfGroups_ = 1;
    else
        numberOfGroups_ = onlyNGroups == 0 ? (nRows / hitPerCol) : onlyNGroups;
}

const std::shared_ptr<ChannelGroupBase> RD53ChannelGroupHandler::getTestGroup(int groupNumber)
{
    size_t nRows        = regionOfInterest.getNumberOfRows();
    size_t nCols        = regionOfInterest.getNumberOfCols();
    auto   channelGroup = std::make_shared<RD53ChannelGroup>(nRows, nCols);

    if(groupType == RD53GroupType::AllPixels) { *channelGroup = regionOfInterest; }
    else
    {
        for(auto col = 0u; col < nCols; col++)
            for(auto i = 0u; i < hitPerCol; i++)
            {
                auto row = (RD53Constants::NROW_CORE * col + i * nRows / hitPerCol) % nRows;
                row += groupNumber;
                row %= nRows;
                if(regionOfInterest.isChannelEnabled(row, col) == true) channelGroup->enableChannel(row, col);
            }
    }

    return channelGroup;
}
