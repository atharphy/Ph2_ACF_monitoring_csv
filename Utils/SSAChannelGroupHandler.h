#include "../Utils/ChannelGroupHandler.h"
#include "../HWDescription/Definition.h"

#ifndef SSAChannelGroupHandler_h
#define SSAChannelGroupHandler_h
class SSAChannelGroupHandler : public ChannelGroupHandler
{
  public:
    SSAChannelGroupHandler();
    SSAChannelGroupHandler(std::bitset<NSSACHANNELS>&& inputChannelsBitset);
    ~SSAChannelGroupHandler();
};

#endif