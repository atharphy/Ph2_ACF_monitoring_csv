#include "Utils/MPAChannelGroupHandler.h"

// MPAChannelGroupHandler::MPAChannelGroupHandler()
// {
//     allChannelGroup_     = new ChannelGroup<1, NMPAROWS * NSSACHANNELS>();
//     currentChannelGroup_ = new ChannelGroup<1, NMPAROWS * NSSACHANNELS>();
// }

// MPAChannelGroupHandler::~MPAChannelGroupHandler()
// {
//     delete allChannelGroup_;
//     delete currentChannelGroup_;
// }
MPAChannelGroupHandler::MPAChannelGroupHandler()
{
    allChannelGroup_     = std::make_shared<ChannelGroup<1, NMPAROWS * NSSACHANNELS>>();
    currentChannelGroup_ = std::make_shared<ChannelGroup<1, NMPAROWS * NSSACHANNELS>>();

    allChannelGroup_->enableAllChannels();
}

MPAChannelGroupHandler::MPAChannelGroupHandler(std::bitset<NSSACHANNELS * NMPAROWS>&& inputChannelsBitset)
{
    allChannelGroup_     = std::make_shared<ChannelGroup<1, NMPAROWS * NSSACHANNELS>>(std::move(inputChannelsBitset));
    currentChannelGroup_ = std::make_shared<ChannelGroup<1, NMPAROWS * NSSACHANNELS>>(std::move(inputChannelsBitset));
}

MPAChannelGroupHandler::~MPAChannelGroupHandler() {}