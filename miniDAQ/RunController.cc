#include "MiddlewareController.h"

INITIALIZE_EASYLOGGINGPP

#define PORT 5000 // The server listening port

int main(int argc, char** argv)
{
    std::string loggerConfigFile;
    if(std::getenv("PH2ACF_BASE_DIR") == nullptr) throw std::runtime_error("No PH2ACF_BASE_DIR environment variables have been set. You need to source some settings file!");

    loggerConfigFile = std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf";
    el::Configurations conf(loggerConfigFile);
    el::Loggers::reconfigureAllLoggers(conf);

    MiddlewareController theMiddlewareController(PORT);
    theMiddlewareController.startAccept();

    while(true) { std::this_thread::sleep_for(std::chrono::milliseconds(1000)); }

    return EXIT_SUCCESS;
}
