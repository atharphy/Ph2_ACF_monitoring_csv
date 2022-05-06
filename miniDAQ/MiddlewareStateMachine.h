#ifndef __MIDDLEWARE_STATE_MACHINE__
#define __MIDDLEWARE_STATE_MACHINE__

#include <string>

class Tool;

class MiddlewareStateMachine
{
  public:
    MiddlewareStateMachine();
    ~MiddlewareStateMachine();

    enum Status {RUNNING, DONE};
    // State machine commands
    void initialize();
    void configure (const std::string& calibrationName, const std::string& configurationFile);
    void start     (int runNumber);
    void stop      ();
    void halt      ();
    void pause     ();
    void resume    ();
    void abort     ();

    Status status();

    Tool* fTheTool;

  private:
    int currentRun_;


};

#endif