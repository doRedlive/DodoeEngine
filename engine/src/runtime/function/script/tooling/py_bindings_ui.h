// do@Redlive

#pragma once

#include "dopch.h"

#ifdef DODOE_PYTHON_ENABLED

#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace dodoe::py_bindings {

    void RegisterUI(py::module_& m);

} // namespace dodoe::py_bindings

#endif // DODOE_PYTHON_ENABLED
