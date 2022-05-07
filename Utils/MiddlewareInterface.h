#ifndef _MiddlewareInterface_h_
#define _MiddlewareInterface_h_

#include "../NetworkUtils/TCPClient.h"
#include "../MessageUtils/cpp/QueryMessage.pb.h"
#include <string>

class MiddlewareInterface : public TCPClient
{
  public:
    MiddlewareInterface(std::string serverIP, int serverPort);
    virtual ~MiddlewareInterface(void);
    void        initialize(void);
    void        configure(MessageUtils::ConfigurationInfo::CalibrationNameEnum theCalibrationEnum, std::string const& configurationFilePath);
    void        halt(void);
    void        pause(void);
    void        resume(void);
    void        start(int runNumber);
    void        stop(void);
    std::string status(void);

  protected:
    // std::string currentRun_ = "0";
    // bool        running_    = false;
    // bool        paused_     = false;

  private:
    std::string sendCommand(const std::string& command);
};

#endif
