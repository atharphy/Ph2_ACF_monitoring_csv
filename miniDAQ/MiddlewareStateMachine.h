#ifndef __MIDDLEWARE_STATE_MACHINE__
#define __MIDDLEWARE_STATE_MACHINE__

#include "../MessageUtils/Message.h"
#include "../MessageUtils/ConfigurationMessage.h"

class Tool;

class MiddlewareStateMachine
{
  public:
    MiddlewareStateMachine();
    virtual ~MiddlewareStateMachine();

    // State machine commands
    Message initialize();
    Message configure(const ConfigurationMessage& configurationMessage);
    Message start();
    Message stop();
    Message halt();
    Message pause();
    Message resume();

    Tool* fTheTool;


};

#endif