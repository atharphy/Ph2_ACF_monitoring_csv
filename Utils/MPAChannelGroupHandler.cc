#include "../Utils/MPAChannelGroupHandler.h"

// MPAChannelGroupHandler::MPAChannelGroupHandler()
// {
//     allChannelGroup_     = new ChannelGroup<120, 16>();
//     currentChannelGroup_ = new ChannelGroup<120, 16>();
// }

// MPAChannelGroupHandler::~MPAChannelGroupHandler()
// {
//     delete allChannelGroup_;
//     delete currentChannelGroup_;
// }
MPAChannelGroupHandler::MPAChannelGroupHandler()
{
    allChannelGroup_     = new ChannelGroup<NSSACHANNELS,NMPACOLS>();
    currentChannelGroup_ = new ChannelGroup<NSSACHANNELS,NMPACOLS>();

    allChannelGroup_->enableAllChannels();
}

MPAChannelGroupHandler::MPAChannelGroupHandler(std::bitset<NSSACHANNELS*NMPACOLS>&& inputChannelsBitset)
{
    allChannelGroup_     = new ChannelGroup<NSSACHANNELS,NMPACOLS>(std::move(inputChannelsBitset));
    currentChannelGroup_ = new ChannelGroup<NSSACHANNELS,NMPACOLS>(std::move(inputChannelsBitset));
}
MPAChannelGroupHandler::~MPAChannelGroupHandler()
{
    delete allChannelGroup_;
    delete currentChannelGroup_;
}