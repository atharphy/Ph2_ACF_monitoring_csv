#include "../Utils/SSA2ChannelGroupHandler.h"
#include "../HWDescription/Definition.h"

SSA2ChannelGroupHandler::SSA2ChannelGroupHandler()
{
    allChannelGroup_     = new ChannelGroup<NSSACHANNELS, 1>();
    currentChannelGroup_ = new ChannelGroup<NSSACHANNELS, 1>();
}

SSA2ChannelGroupHandler::~SSA2ChannelGroupHandler()
{
    delete allChannelGroup_;
    delete currentChannelGroup_;
}
