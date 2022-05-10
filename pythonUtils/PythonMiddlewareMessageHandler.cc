
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "pybind11/pybind11.h"
#pragma GCC diagnostic pop
#include "../miniDAQ/MiddlewareMessageHandler.cc"
#include "string"
#include "iostream"

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
    .def("status"    , &MiddlewareMessageHandler::status    )
    .def("print"     , &MiddlewareMessageHandler::print     );
}


// struct Pet {
//     Pet(const std::string &name) : name(name) { }
//     void setName(const std::string &name_) { name = name_; }
//     const std::string &getName() const { return name; }
//     void print() const { std::cout<<"printed"<<std::endl; }

//     std::string name;
// };


// namespace py = pybind11;

// PYBIND11_MODULE(Ph2_ACF_PythonInterface, m) {
//     py::class_<Pet>(m, "MiddlewareMessageHandler")
//         .def(py::init<const std::string &>())
//         .def("setName", &Pet::setName)
//         .def("getName", &Pet::getName)
//         .def("print", &Pet::print);
// }
