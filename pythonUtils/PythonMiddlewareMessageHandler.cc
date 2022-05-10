
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "pybind11/pybind11.h"
#pragma GCC diagnostic pop
#include "../miniDAQ/MiddlewareMessageHandler.cc"
#include "string"
#include "iostream"
#include "../Utils/easylogging++.h"

INITIALIZE_EASYLOGGINGPP

void configureLogger(std::string loggerConfigFile)
{
    el::Configurations conf(loggerConfigFile);
    el::Loggers::reconfigureAllLoggers(conf);
}

PYBIND11_MODULE(Ph2_ACF_PythonInterface, handle)
{
    handle.doc() = "Handle for MiddlewareMessageHandler";

    pybind11::class_<MiddlewareMessageHandler>(handle, "MiddlewareMessageHandler")
    .def(pybind11::init<>())
    .def("initialize", &MiddlewareMessageHandler::initialize)
    .def("configure" , &MiddlewareMessageHandler::configure )
    .def("start"     , &MiddlewareMessageHandler::start     )
    .def("stop"      , &MiddlewareMessageHandler::stop      )
    .def("halt"      , &MiddlewareMessageHandler::halt      )
    .def("pause"     , &MiddlewareMessageHandler::pause     )
    .def("resume"    , &MiddlewareMessageHandler::resume    )
    .def("abort"     , &MiddlewareMessageHandler::abort     )
    .def("status"    , &MiddlewareMessageHandler::status    );

    handle.def("configureLogger", &configureLogger);
}
