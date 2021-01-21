#include "../Utils/SSAChannelGroupHandler.h"
#include "../HWDescription/Definition.h"

SSAChannelGroupHandler::SSAChannelGroupHandler()
{
    allChannelGroup_     = new ChannelGroup<NSSACHANNELS, 1>();
    currentChannelGroup_ = new ChannelGroup<NSSACHANNELS, 1>();
}

SSAChannelGroupHandler::~SSAChannelGroupHandler()
{
    delete allChannelGroup_;
    delete currentChannelGroup_;
}
