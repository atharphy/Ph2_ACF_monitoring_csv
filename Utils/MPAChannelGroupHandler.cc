#include "../Utils/MPAChannelGroupHandler.h"
#include "../HWDescription/Definition.h"

MPAChannelGroupHandler::MPAChannelGroupHandler()
{
    allChannelGroup_     = new ChannelGroup<120, 16>();
    currentChannelGroup_ = new ChannelGroup<120, 16>();
}

MPAChannelGroupHandler::~MPAChannelGroupHandler()
{
    delete allChannelGroup_;
    delete currentChannelGroup_;
}
