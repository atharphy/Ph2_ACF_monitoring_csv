#ifndef __MIDDLEWARE_STATE_MACHINE__
#define __MIDDLEWARE_STATE_MACHINE__

#include "../MessageUtils/cpp/Message.pb.h"

class Tool;

class MiddlewareStateMachine
{
  public:
    MiddlewareStateMachine();
    virtual ~MiddlewareStateMachine();

    // State machine commands
    MessageUtils::Message initialize(MessageUtils::Message);
    MessageUtils::Message configure(const MessageUtils::ConfigurationMessage& configurationMessage);
    MessageUtils::Message start(MessageUtils::Message);
    MessageUtils::Message stop(MessageUtils::Message);
    MessageUtils::Message halt(MessageUtils::Message);
    MessageUtils::Message pause(MessageUtils::Message);
    MessageUtils::Message resume(MessageUtils::Message);

    Tool* fTheTool;


};

#endif