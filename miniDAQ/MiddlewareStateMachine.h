#ifndef __MIDDLEWARE_STATE_MACHINE__
#define __MIDDLEWARE_STATE_MACHINE__

#include "../MessageUtils/cpp/ReplyMessage.pb.h"
#include "../MessageUtils/cpp/QueryMessage.pb.h"

class Tool;

class MiddlewareStateMachine
{
  public:
    MiddlewareStateMachine();
    virtual ~MiddlewareStateMachine();

    // State machine commands
    MessageUtils::ReplyMessage initialize();
    MessageUtils::ReplyMessage configure(const MessageUtils::ConfigurationInfo& configurationInfo);
    MessageUtils::ReplyMessage start(const MessageUtils::StartInfo& startInfo);
    MessageUtils::ReplyMessage stop();
    MessageUtils::ReplyMessage halt();
    MessageUtils::ReplyMessage pause();
    MessageUtils::ReplyMessage resume();
    MessageUtils::ReplyMessage abort();

    MessageUtils::ReplyMessage status();

    Tool* fTheTool;

  private:
    int currentRun_;

};

#endif