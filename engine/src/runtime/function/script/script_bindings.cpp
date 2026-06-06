// do@Redlive

#include "script_bindings.h"

#include <pybind11/embed.h>

PYBIND11_EMBEDDED_MODULE(dodoe, m) {
    dodoe::script_bindings::RegisterAll(m);
}

namespace dodoe::script_bindings {

    void RegisterAll(py::module_& dodoe_module) {
        RegisterCore(dodoe_module);
        RegisterResource(dodoe_module);
        RegisterEntity(dodoe_module);
        RegisterWorld(dodoe_module);
    }

} // dodoe::script_bindings
