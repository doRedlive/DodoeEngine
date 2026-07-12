// do@Redlive

#include "tool_interpreter.h"

#ifdef DODOE_PYTHON_ENABLED
#include <pybind11/embed.h>
#include "py_bindings.h"
#endif

namespace dodoe {

#ifdef DODOE_PYTHON_ENABLED

    Bool ToolInterpreter::Initialize() {
        try {
            m_guard = create_scope<pybind11::scoped_interpreter>();
            pybind11::module_::import("dodoe");
            return true;
        } catch (const std::exception& e) {
            DO_ERROR("ToolInterpreter: failed to initialize Python: {}", e.what());
            return false;
        } catch (...) {
            DO_ERROR("ToolInterpreter: failed to initialize Python (unknown error)");
            return false;
        }
    }

    void ToolInterpreter::Shutdown() {
        m_guard.reset();
    }

    Bool ToolInterpreter::Execute(const std::filesystem::path& path) {
        DO_ASSERT(m_guard != nullptr, "ToolInterpreter: not initialized");
        try {
            pybind11::eval_file(path.string(), pybind11::globals());
            return true;
        } catch (const pybind11::error_already_set& e) {
            DO_ERROR("ToolInterpreter: script '{}' failed: {}", path.string(), e.what());
            return false;
        }
    }

#else

    Bool ToolInterpreter::Initialize() { return true; }
    void ToolInterpreter::Shutdown() {}
    Bool ToolInterpreter::Execute(const std::filesystem::path&) { return false; }

#endif

} // namespace dodoe
