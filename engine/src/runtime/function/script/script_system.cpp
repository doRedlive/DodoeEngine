// do@Redlive

#include "script_system.h"

#include "script_engine.h"
#include "script_glue.h"
#include "script_runtime.h"

#include <pybind11/embed.h>
#include "script_bindings.h"

namespace py = pybind11;

namespace dodoe {

    struct ScriptSystem::ToolInterpreter {
        Scope<py::scoped_interpreter> guard;

        Bool Initialize() {
            guard = create_scope<py::scoped_interpreter>();
            py::module_::import("dodoe");
            return true;
        }

        void Shutdown() {
            guard.reset();
        }

        [[nodiscard]] Bool Execute(const std::filesystem::path& path) {
            DO_ASSERT(guard != nullptr, "ToolInterpreter not initialized");
            try {
                py::eval_file(path.string(), py::globals());
                return true;
            } catch (const py::error_already_set& e) {
                DO_ERROR("Script execute failed: {}, error: {}", path.string(), e.what());
                return false;
            }
        }
    };

    ScriptSystem::ScriptSystem() = default;

    ScriptSystem::~ScriptSystem() {
        delete m_tool_interp;
    }

    Bool ScriptSystem::Execute(const std::filesystem::path& path) {
        auto ext = path.extension().string();
        if (ext == ".py" && m_tool_interp) {
            return m_tool_interp->Execute(path);
        }
        DO_ERROR("ScriptSystem::Execute: unsupported extension '{}' for '{}'", ext, path.string());
        return false;
    }

    Bool ScriptSystem::reloadScripts() {
        const Bool reloaded = m_script_engine->reloadScripts();
        if (!reloaded) {
            return false;
        }

        m_script_runtime->reloadAssemblyClasses();
        ScriptGlue::Register();
        return true;
    }

    Bool ScriptSystem::initialize(const ScriptSystemCreateInfo& create_info) {
        m_script_engine = ScriptEngine::Create({});
        if (!m_script_engine) {
            DO_ASSERT(false, "ScriptEngine initialize failed!");
            return false;
        }
        m_script_runtime = ScriptRuntime::Create({m_script_engine.get()});
        if (!m_script_runtime) {
            DO_ASSERT(false, "ScriptRuntime initialize failed!");
            return false;
        }

        ScriptGlue::Initialize(m_script_engine.get());
        ScriptGlue::Register();

        m_script_runtime->reloadAssemblyClasses();

        m_tool_interp = new ToolInterpreter();
        m_tool_interp->Initialize();

        return m_tool_interp != nullptr;
    }

    void ScriptSystem::shutdown() {
        ScriptGlue::Shutdown();
        ScriptRuntime::Destroy(m_script_runtime);
        ScriptEngine::Destroy(m_script_engine);
        if (m_tool_interp) {
            m_tool_interp->Shutdown();
            delete m_tool_interp;
            m_tool_interp = nullptr;
        }
    }

} // dodoe
