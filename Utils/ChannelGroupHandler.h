/*

        \file                          Container.h
        \brief                         containers for DAQ
        \author                        Fabio Ravera, Lorenzo Uplegger
        \version                       1.0
        \date                          26/04/19
        Support :                      mail to : fabio.ravera@cern.ch

 */

#ifndef __CHANNELGROUPBASE_H__
#define __CHANNELGROUPBASE_H__

#include <bitset>
#include <iostream>
#include <map>
#include <vector>

class ChannelGroupHandler;

class ChannelGroupBase
{
  public:
    ChannelGroupBase(){};
    ChannelGroupBase(uint16_t numberOfRows, uint16_t numberOfCols)
        : numberOfRows_(numberOfRows), numberOfCols_(numberOfCols), numberOfEnabledChannels_(numberOfRows * numberOfCols), customPatternSet_(false){};
    virtual ~ChannelGroupBase() { ; }
    virtual void
                     makeTestGroup(ChannelGroupBase* currentChannelGroup, uint32_t groupNumber, uint32_t numberOfClustersPerGroup, uint16_t numberOfRowsPerCluster, uint16_t numberOfColsPerCluster = 1) const = 0;
    uint32_t         getNumberOfEnabledChannels(void) const { return numberOfEnabledChannels_; }
    virtual uint32_t getNumberOfEnabledChannels(const ChannelGroupBase* mask) const = 0;
    virtual bool     isChannelEnabled(uint16_t row, uint16_t col = 0) const         = 0;
    virtual void     enableChannel(uint16_t row, uint16_t col = 0)                  = 0;
    virtual void     disableChannel(uint16_t row, uint16_t col = 0)                 = 0;
    virtual void     disableAllChannels(void)                                       = 0;
    virtual void     enableAllChannels(void)                                        = 0;
    virtual void     flipAllChannels(void)                                          = 0;
    virtual bool     areAllChannelsEnabled(void) const                              = 0;

  protected:
    uint16_t numberOfRows_;
    uint16_t numberOfCols_;
    uint32_t numberOfEnabledChannels_;
    bool     customPatternSet_;
};

template <size_t R, size_t C = 1>
class ChannelGroup : public ChannelGroupBase
{
  public:
    ChannelGroup() : ChannelGroupBase(R, C)
    {
        enableAllChannels();
        numberOfEnabledChannels_ = numberOfRows_ * numberOfCols_;
    };
    ChannelGroup(std::bitset<R * C>&& inputChannelsBitset) : ChannelGroupBase(R, C), channelsBitset_(inputChannelsBitset)
    {
        customPatternSet_        = true;
        numberOfEnabledChannels_ = channelsBitset_.count();
    };
    ChannelGroup(const ChannelGroup& theChannelGroup) : ChannelGroupBase(R, C)
    {
        channelsBitset_          = theChannelGroup.channelsBitset_;
        numberOfEnabledChannels_ = getNumberOfEnabledChannels(theChannelGroup);
    }

    virtual ~ChannelGroup() { ; }

    inline bool isChannelEnabled(uint16_t row, uint16_t col = 0) const override { return channelsBitset_[row + numberOfRows_ * col]; }
    inline void enableChannel(uint16_t row, uint16_t col = 0) override
    {
        channelsBitset_[row + numberOfRows_ * col] = true;
        numberOfEnabledChannels_                   = channelsBitset_.count();
    }
    inline void disableChannel(uint16_t row, uint16_t col = 0) override
    {
        channelsBitset_[row + numberOfRows_ * col] = false;
        numberOfEnabledChannels_                   = channelsBitset_.count();
    }
    inline void disableAllChannels(void) override
    {
        channelsBitset_.reset();
        numberOfEnabledChannels_ = channelsBitset_.count();
    }
    inline void enableAllChannels(void) override
    {
        channelsBitset_.set();
        numberOfEnabledChannels_ = channelsBitset_.count();
    }
    inline void flipAllChannels(void) override
    {
        channelsBitset_.flip();
        numberOfEnabledChannels_ = channelsBitset_.count();
    }
    inline bool areAllChannelsEnabled(void) const override { return channelsBitset_.all(); }

    inline uint32_t getNumberOfEnabledChannels(const ChannelGroupBase* mask) const
    {
        std::bitset<R * C> tmpBitset;
        tmpBitset = this->channelsBitset_ & static_cast<const ChannelGroup<R, C>*>(mask)->channelsBitset_;
        return tmpBitset.count();
    }

    inline std::bitset<R * C> getBitset(void) const { return channelsBitset_; }

    inline void setCustomPattern(const ChannelGroup<R, C>& customChannelGroup)
    {
        channelsBitset_          = customChannelGroup.channelsBitset_;
        customPatternSet_        = true;
        numberOfEnabledChannels_ = channelsBitset_.count();

        numberOfRows_ = customChannelGroup.numberOfRows_;
        numberOfCols_ = customChannelGroup.numberOfCols_;
    }

    virtual void
    makeTestGroup(ChannelGroupBase* currentChannelGroup, uint32_t groupNumber, uint32_t numberOfClustersPerGroup, uint16_t numberOfRowsPerCluster, uint16_t numberOfColsPerCluster = 1) const override
    {
        // if(customPatternSet_ && (numberOfRowsPerCluster>1 || numberOfColsPerCluster>1))
        //     std::cout << __PRETTY_FUNCTION__ << " Warning, automatic group creation may not work when a custom
        //     pattern is set" << std::endl;
        if(numberOfClustersPerGroup * numberOfRowsPerCluster * numberOfColsPerCluster >= numberOfEnabledChannels_)
        {
            static_cast<ChannelGroup<R, C>*>(currentChannelGroup)->setCustomPattern(*this);
            return;
        }
        static_cast<ChannelGroup*>(currentChannelGroup)->disableAllChannels();

        uint32_t numberOfClusterToSkip = numberOfEnabledChannels_ / (numberOfRowsPerCluster * numberOfColsPerCluster * numberOfClustersPerGroup) - 1;
        if(numberOfEnabledChannels_ % (numberOfRowsPerCluster * numberOfColsPerCluster * numberOfClustersPerGroup) > 0) ++numberOfClusterToSkip;

        // std::cout << "numberOfClustersPerGroup = " << numberOfClustersPerGroup << "\n";

        // std::cout << "numberOfClusterToSkip = " << numberOfClusterToSkip << "\n";

        uint32_t clusterSkipped = numberOfClusterToSkip - groupNumber;
        for(uint16_t col = 0; col < numberOfCols_; col += numberOfColsPerCluster)
        {
            for(uint16_t row = 0; row < numberOfRows_; row += numberOfRowsPerCluster)
            {
                if(clusterSkipped == numberOfClusterToSkip)
                    clusterSkipped = 0;
                else
                {
                    ++clusterSkipped;
                    continue;
                }
                if(isChannelEnabled(row, col))
                {
                    for(uint16_t clusterRow = 0; clusterRow < numberOfRowsPerCluster; ++clusterRow)
                    {
                        for(uint16_t clusterCol = 0; clusterCol < numberOfColsPerCluster; ++clusterCol)
                        { static_cast<ChannelGroup<R, C>*>(currentChannelGroup)->enableChannel(row + clusterRow, col + clusterCol); }
                    }
                }
            }
        }
    }

  private:
    std::bitset<R * C> channelsBitset_;
};

class ChannelGroupHandler
{
  public:
    class ChannelGroupIterator : public std::iterator<std::output_iterator_tag, uint32_t>
    {
      public:
        explicit ChannelGroupIterator(ChannelGroupHandler& channelGroupHandler, uint32_t groupNumber) : channelGroupHandler_(channelGroupHandler), groupNumber_(groupNumber) { ; }
        const ChannelGroupBase* operator*() const { return channelGroupHandler_.getTestGroup(groupNumber_); }
        ChannelGroupIterator&   operator++()
        {
            ++groupNumber_;
            return *this;
        }
        ChannelGroupIterator& operator++(int i) { return ++(*this); }

        bool operator!=(const ChannelGroupIterator& rhs) const { return groupNumber_ != rhs.groupNumber_; }

      protected:
        ChannelGroupHandler& channelGroupHandler_;
        uint32_t             groupNumber_;
    };

    ChannelGroupHandler(){};
    virtual ~ChannelGroupHandler(){};

    virtual void setChannelGroupParameters(uint32_t numberOfClustersPerGroup, uint32_t numberOfRowsPerCluster, uint32_t numberOfColsPerCluster = 1);

    template <size_t R, size_t C>
    void setCustomChannelGroup(ChannelGroup<R, C>& customChannelGroup)
    {
        static_cast<ChannelGroup<R, C>*>(allChannelGroup_)->setCustomPattern(customChannelGroup);
    }

    virtual ChannelGroupIterator begin() { return ChannelGroupIterator(*this, 0); }

    virtual ChannelGroupIterator end() { return ChannelGroupIterator(*this, numberOfGroups_); }

    const ChannelGroupBase* allChannelGroup() const { return allChannelGroup_; }

    virtual ChannelGroupBase* getTestGroup(uint32_t groupNumber) const
    {
        allChannelGroup_->makeTestGroup(currentChannelGroup_, groupNumber, numberOfClustersPerGroup_, numberOfRowsPerCluster_, numberOfColsPerCluster_);
        return currentChannelGroup_;
    }

  protected:
    uint32_t          numberOfGroups_;
    uint32_t          numberOfClustersPerGroup_;
    uint32_t          numberOfRowsPerCluster_;
    uint32_t          numberOfColsPerCluster_;
    ChannelGroupBase* allChannelGroup_;
    ChannelGroupBase* currentChannelGroup_;
};

#endif
