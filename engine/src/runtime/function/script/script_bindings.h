// do@Redlive

#pragma once

#include "dopch.h"

#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace dodoe::script_bindings {

    void RegisterAll(py::module_& dodoe_module);

    void RegisterCore(py::module_& dodoe_module);
    void RegisterResource(py::module_& dodoe_module);
    void RegisterEntity(py::module_& dodoe_module);
    void RegisterWorld(py::module_& dodoe_module);

} // dodoe::script_bindings
