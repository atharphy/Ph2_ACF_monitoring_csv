#include "../HWDescription/Definition.h"
#include "../Utils/ChannelGroupHandler.h"

class CBCChannelGroupHandler : public ChannelGroupHandler
{
  public:
    CBCChannelGroupHandler();
    CBCChannelGroupHandler(std::bitset<NCHANNELS>&& inputChannelsBitset);
    ~CBCChannelGroupHandler();
};
