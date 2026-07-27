// do@Redlive

#ifdef DODOE_PYTHON_ENABLED

#include "py_bindings.h"

#include "py_bindings_ui.h"

#include <pybind11/embed.h>

PYBIND11_EMBEDDED_MODULE(dodoe, m) {
    dodoe::py_bindings::RegisterAll(m);
}

namespace dodoe::py_bindings {

    void RegisterAll(py::module_& dodoe_module) {
        RegisterCore(dodoe_module);
        RegisterUI(dodoe_module);
        RegisterResource(dodoe_module);
        RegisterEntity(dodoe_module);
        RegisterWorld(dodoe_module);
        RegisterGeneratedComponents(dodoe_module);
    }

} // namespace dodoe::py_bindings

#endif // DODOE_PYTHON_ENABLED
