#ifndef __PYTHON_MIDDLEWARE_MESSAGE_HANDLER__
#define __PYTHON_MIDDLEWARE_MESSAGE_HANDLER__

#include "../miniDAQ/MiddlewareMessageHandler.h"
#include <string>

class PythonMiddlewareMessageHandler : public MiddlewareMessageHandler
{
  public:
    PythonMiddlewareMessageHandler() : MiddlewareMessageHandler() {};
    virtual ~PythonMiddlewareMessageHandler() {};
};

#endif