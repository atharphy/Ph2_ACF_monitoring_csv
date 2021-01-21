#include "../Utils/CBCChannelGroupHandler.h"

CBCChannelGroupHandler::CBCChannelGroupHandler()
{
    allChannelGroup_     = new ChannelGroup<NCHANNELS, 1>();
    currentChannelGroup_ = new ChannelGroup<NCHANNELS, 1>();
}

CBCChannelGroupHandler::CBCChannelGroupHandler(std::bitset<NCHANNELS>&& inputChannelsBitset)
{
    allChannelGroup_     = new ChannelGroup<NCHANNELS, 1>(std::move(inputChannelsBitset));
    currentChannelGroup_ = new ChannelGroup<NCHANNELS, 1>(std::move(inputChannelsBitset));
}
CBCChannelGroupHandler::~CBCChannelGroupHandler()
{
    delete allChannelGroup_;
    delete currentChannelGroup_;
}