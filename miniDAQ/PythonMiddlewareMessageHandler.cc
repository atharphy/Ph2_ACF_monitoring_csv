#include "../miniDAQ/MiddlewareMessageHandler.cc"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "pybind11/pybind11.h"
#pragma GCC diagnostic pop

PYBIND11_MODULE(module_name, handle)
{
    handle.doc() = "Handle for MiddlewareMessageHandler";

    pybind11::class_<MiddlewareMessageHandler>(handle, "MiddlewareMessageHandler");
}

