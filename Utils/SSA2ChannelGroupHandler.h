#include "../HWDescription/Definition.h"
#include "../Utils/ChannelGroupHandler.h"

#ifndef SSA2ChannelGroupHandler_h
#define SSA2ChannelGroupHandler_h
class SSA2ChannelGroupHandler : public ChannelGroupHandler
{
  public:
    SSA2ChannelGroupHandler();
    SSA2ChannelGroupHandler(std::bitset<NSSACHANNELS>&& inputChannelsBitset);
    ~SSA2ChannelGroupHandler();
};
#endif