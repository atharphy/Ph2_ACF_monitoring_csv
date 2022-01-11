#include "../Utils/SSA2ChannelGroupHandler.h"
#include "../HWDescription/Definition.h"

SSA2ChannelGroupHandler::SSA2ChannelGroupHandler()
{
    allChannelGroup_     = std::make_shared<ChannelGroup<NSSACHANNELS, 1>>();
    currentChannelGroup_ = std::make_shared<ChannelGroup<NSSACHANNELS, 1>>();
}

SSA2ChannelGroupHandler::SSA2ChannelGroupHandler(std::bitset<NSSACHANNELS>&& inputChannelsBitset)
{
    allChannelGroup_     = std::make_shared<ChannelGroup<NSSACHANNELS, 1>>(std::move(inputChannelsBitset));
    currentChannelGroup_ = std::make_shared<ChannelGroup<NSSACHANNELS, 1>>(std::move(inputChannelsBitset));
}

SSA2ChannelGroupHandler::~SSA2ChannelGroupHandler() {}
