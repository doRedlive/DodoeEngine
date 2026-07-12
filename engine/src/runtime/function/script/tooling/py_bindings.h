// do@Redlive

#pragma once

#include "dopch.h"

#ifdef DODOE_PYTHON_ENABLED

#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace dodoe::py_bindings {

    void RegisterAll(py::module_& dodoe_module);

    void RegisterCore(py::module_& dodoe_module);
    void RegisterResource(py::module_& dodoe_module);
    void RegisterEntity(py::module_& dodoe_module);
    void RegisterWorld(py::module_& dodoe_module);
    void RegisterGeneratedComponents(py::module_& m);

} // namespace dodoe::py_bindings

#endif // DODOE_PYTHON_ENABLED
