#include "Utils/SSA2ChannelGroupHandler.h"
#include "HWDescription/Definition.h"

SSA2ChannelGroupHandler::SSA2ChannelGroupHandler()
{
    allChannelGroup_     = std::make_shared<ChannelGroup<1, NSSACHANNELS>>();
    currentChannelGroup_ = std::make_shared<ChannelGroup<1, NSSACHANNELS>>();
}

SSA2ChannelGroupHandler::SSA2ChannelGroupHandler(std::bitset<NSSACHANNELS>&& inputChannelsBitset)
{
    allChannelGroup_     = std::make_shared<ChannelGroup<1, NSSACHANNELS>>(std::move(inputChannelsBitset));
    currentChannelGroup_ = std::make_shared<ChannelGroup<1, NSSACHANNELS>>(std::move(inputChannelsBitset));
}

SSA2ChannelGroupHandler::~SSA2ChannelGroupHandler() {}
