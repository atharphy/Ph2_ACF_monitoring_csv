#include "HWDescription/Definition.h"
#include "Utils/ChannelGroupHandler.h"

#ifndef MPAChannelGroupHandler_h
#define MPAChannelGroupHandler_h
class MPAChannelGroupHandler : public ChannelGroupHandler
{
  public:
    MPAChannelGroupHandler();
    MPAChannelGroupHandler(std::bitset<NSSACHANNELS * NMPACOLS>&& inputChannelsBitset);
    ~MPAChannelGroupHandler();
};

#endif