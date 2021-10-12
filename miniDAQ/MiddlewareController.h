#ifndef _MiddlewareController_h_
#define _MiddlewareController_h_

#include "../NetworkUtils/TCPServer.h"
#include "../System/SystemController.h"
#include "../tools/Tool.h"

#include <string>

class MiddlewareController : public TCPServer
{
  public:
    MiddlewareController(int serverPort);
    virtual ~MiddlewareController(void);

    // The MiddlewareController only has 1 client so send is more appropriate than broadcast
    // void send(const std::string& message){broadcast(message);}

    std::string interpretMessage(const std::string& buffer) override;

  protected:
    std::string getVariableValue(std::string variable, std::string buffer)
    {
        size_t begin = buffer.find(variable) + variable.size() + 1;
        size_t end   = buffer.find(',', begin);
        if(end == std::string::npos) end = buffer.size();
        return buffer.substr(begin, end - begin);
    }
    std::string currentRun_ = "0";
    bool        running_    = false;
    bool        paused_     = false;

  private:
    Tool* theSystemController_;
};

#endif
