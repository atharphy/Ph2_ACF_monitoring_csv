#include "Utils/CBCChannelGroupHandler.h"
#include <memory>

CBCChannelGroupHandler::CBCChannelGroupHandler()
{
    allChannelGroup_     = std::make_shared<ChannelGroup<NCHANNELS, 1>>();
    currentChannelGroup_ = std::make_shared<ChannelGroup<NCHANNELS, 1>>();
}

CBCChannelGroupHandler::CBCChannelGroupHandler(std::bitset<NCHANNELS>&& inputChannelsBitset)
{
    allChannelGroup_     = std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(inputChannelsBitset));
    currentChannelGroup_ = std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(inputChannelsBitset));
}
CBCChannelGroupHandler::~CBCChannelGroupHandler() {}