#include "../Utils/SSAChannelGroupHandler.h"
#include "../HWDescription/Definition.h"

SSAChannelGroupHandler::SSAChannelGroupHandler()
{
    allChannelGroup_     = std::make_shared<ChannelGroup<NSSACHANNELS, 1>>();
    currentChannelGroup_ = std::make_shared<ChannelGroup<NSSACHANNELS, 1>>();
}

SSAChannelGroupHandler::SSAChannelGroupHandler(std::bitset<NSSACHANNELS>&& inputChannelsBitset)
{
    allChannelGroup_     = std::make_shared<ChannelGroup<NSSACHANNELS, 1>>(std::move(inputChannelsBitset));
    currentChannelGroup_ = std::make_shared<ChannelGroup<NSSACHANNELS, 1>>(std::move(inputChannelsBitset));
}

SSAChannelGroupHandler::~SSAChannelGroupHandler()
{
}
