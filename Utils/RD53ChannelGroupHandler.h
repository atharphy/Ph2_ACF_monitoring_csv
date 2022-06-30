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
constexpr uint8_t Groups = 1;
} // namespace RD53GroupType

class RD53ChannelGroup : public ChannelGroupBase {

public:
  RD53ChannelGroup(size_t nRows, size_t nCols, bool initialValue=false) 
    : ChannelGroupBase(nRows, nCols) 
    , storage(nRows * nCols, initialValue)
  {}

  uint32_t getNumberOfEnabledChannels(const std::shared_ptr<ChannelGroupBase> mask) const override {
    return getNumberOfEnabledChannels(mask.get());
  }
     bool     isChannelEnabled(uint16_t row, uint16_t col = 0) const override {
      return storage[col + numberOfCols_ * row];
    }
     void     enableChannel(uint16_t row, uint16_t col = 0) override {
      storage[col + numberOfCols_ * row] = true;
    }
     void     disableChannel(uint16_t row, uint16_t col = 0)   override {
      storage[col + numberOfCols_ * row] = false;
    }
     void     disableAllChannels(void) override {
      std::fill(storage.begin(), storage.end(), false);
    }
     void     enableAllChannels(void) override {
      std::fill(storage.begin(), storage.end(), true);
    }
     void     flipAllChannels(void) override {
      std::transform(storage.begin(), storage.end(), storage.begin(), std::logical_not<>{});
    }
     bool     areAllChannelsEnabled(void) const override {
      return std::all_of(storage.begin(), storage.end(), [] (auto x) { return x; });
    }
     void     setCustomPattern(const ChannelGroupBase& customChannelGroupBase) {}

  protected:
     uint32_t getNumberOfEnabledChannels(const ChannelGroupBase* mask) const override {
      std::vector<bool> conjuction(storage.size());
      std::transform(storage.begin(), storage.end(), static_cast<const RD53ChannelGroup*>(mask)->storage.begin(), conjuction.begin(), std::logical_and<>{});
      return std::count(conjuction.begin(), conjuction.end(), true);
    }

private:
  std::vector<bool> storage;
};

class RD53ChannelGroupHandler : public ChannelGroupHandler
{
  public:
    RD53ChannelGroupHandler(size_t rowStart,
                            size_t rowStop,
                            size_t colStart,
                            size_t colStop,
                            size_t nRows,
                            size_t nCols,
                            uint8_t           groupType,
                            size_t           hitPerCol   = 1,
                            size_t           onlyNGroups = 0);

    RD53ChannelGroup& getRegionOfInterest() { return regionOfInterest; }

    const std::shared_ptr<ChannelGroupBase> getTestGroup(int groupNumber) override;

    private:
    RD53ChannelGroup regionOfInterest;
    uint8_t groupType;
    size_t hitPerCol;
    size_t onlyNGroups;
};

#endif
