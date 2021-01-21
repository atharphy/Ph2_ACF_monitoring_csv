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
    RD53ChannelGroupHandler(ChannelGroup<Ph2_HwDescription::RD53::nRows, Ph2_HwDescription::RD53::nCols>& customChannelGroup, uint8_t groupType, uint8_t hitPerCol = 1);
    ~RD53ChannelGroupHandler();

    static size_t getNumberOfGroups(uint8_t groupType, uint8_t hitPerCol)
    {
        if(groupType == RD53GroupType::AllGroups)
            return Ph2_HwDescription::RD53::nRows / hitPerCol;
        else
            return 1;
    };

  private:
    class RD53ChannelGroupAll : public ChannelGroup<Ph2_HwDescription::RD53::nRows, Ph2_HwDescription::RD53::nCols>
    {
        void makeTestGroup(ChannelGroupBase* currentChannelGroup,
                           uint32_t          groupNumber,
                           uint32_t          numberOfClustersPerGroup,
                           uint16_t          numberOfRowsPerCluster,
                           uint16_t          numberOfColsPerCluster = 1) const override;
    };

    class RD53ChannelGroupPattern : public ChannelGroup<Ph2_HwDescription::RD53::nRows, Ph2_HwDescription::RD53::nCols>
    {
      public:
        RD53ChannelGroupPattern(uint8_t hitPerCol) : hitPerCol(hitPerCol){};

      private:
        void    makeTestGroup(ChannelGroupBase* currentChannelGroup,
                              uint32_t          groupNumber,
                              uint32_t          numberOfClustersPerGroup,
                              uint16_t          numberOfRowsPerCluster,
                              uint16_t          numberOfColsPerCluster = 1) const override;
        uint8_t hitPerCol;
    };
};

#endif
