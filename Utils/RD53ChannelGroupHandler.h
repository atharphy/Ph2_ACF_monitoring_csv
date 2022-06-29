/*!
  \file                  RD53ChannelGroupHandler.h
  \brief                 Channel container handler
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53ChannelGroupHandler_H
#define RD53ChannelGroupHandler_H

#include "../HWDescription/RD53.h"
#include "ChannelGroupHandler.h"

namespace RD53GroupType
{
constexpr uint8_t AllPixels = 0;
constexpr uint8_t AllGroups = 1;
constexpr uint8_t OneGroup  = 2;
} // namespace RD53GroupType

class RD53ChannelGroupHandler : public ChannelGroupHandler
{
  public:
    RD53ChannelGroupHandler(ChannelGroupBase& customChannelGroup,
                            ChannelGroupBase* allChannelGroup,
                            ChannelGroupBase* currentChannelGroup,
                            uint8_t           groupType,
                            uint8_t           hitPerCol   = 1,
                            uint8_t           onlyNGroups = 0);

    template <size_t R, size_t C>
    class RD53ChannelGroupAll : public ChannelGroup<R, C>
    {
        void makeTestGroup(std::shared_ptr<ChannelGroupBase>& currentChannelGroup,
                           uint32_t                           groupNumber,
                           uint32_t                           numberOfClustersPerGroup,
                           uint16_t                           numberOfRowsPerCluster,
                           uint16_t                           numberOfColsPerCluster = 1) const override
        {
            currentChannelGroup->enableAllChannels();
        }
    };

    template <size_t R, size_t C>
    class RD53ChannelGroupPattern : public ChannelGroup<R, C>
    {
      public:
        RD53ChannelGroupPattern(uint8_t hitPerCol) : hitPerCol(hitPerCol){};

      private:
        void makeTestGroup(std::shared_ptr<ChannelGroupBase>& currentChannelGroup,
                           uint32_t                           groupNumber,
                           uint32_t                           numberOfClustersPerGroup,
                           uint16_t                           numberOfRowsPerCluster,
                           uint16_t                           numberOfColsPerCluster = 1) const override
        {
            currentChannelGroup->disableAllChannels();

            for(auto col = 0u; col < currentChannelGroup->getNumberOfCols(); col++)
                for(auto i = 0u; i < hitPerCol; i++)
                {
                    auto row = (RD53Constants::NROW_CORE * col + i * currentChannelGroup->getNumberOfRows() / hitPerCol) % currentChannelGroup->getNumberOfRows();
                    row += groupNumber;
                    row %= currentChannelGroup->getNumberOfRows();
                    currentChannelGroup->enableChannel(row, col);
                }
        }

        uint8_t hitPerCol;
    };
};

#endif
