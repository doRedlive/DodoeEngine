#include "script_system.h"

#include "script_engine.h"
#include "script_glue.h"
#include "script_runtime.h"

namespace dodoe {

    Bool ScriptSystem::Execute(const FsPath& path) {
        if (m_tool_interp) {
            return m_tool_interp->Execute(path);
        }
        DO_ERROR("ScriptSystem::Execute: ToolInterpreter not available for '{}'", path.string());
        return false;
    }

    Bool ScriptSystem::reloadScripts() {
        if (!m_script_engine->onScriptSourcesChanged()) {
            DO_ERROR("Scripts sources don't changed");
            return false;
        }
        if (!m_script_engine->buildAppAssembly()) {
            DO_ERROR("Build App Assembly failed");
            return false;
        }

        m_script_runtime->snapshotFields();
        m_script_runtime->clearRuntimeState();
        m_script_engine->unloadAppAssembly();

        if (!m_script_engine->loadAppAssembly()) {
            DO_ERROR("load app assembly failed");
            return false;
        }
        ScriptGlue::Register();

        m_script_runtime->reloadAssemblyClasses();
        m_script_runtime->restoreFields();
        m_script_engine->commitScriptFingerprint();
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

        m_tool_interp = create_scope<ToolInterpreter>();
        if (m_tool_interp) {
            m_tool_interp->Initialize();
        }

        return true;
    }

    void ScriptSystem::shutdown() {
        if (m_tool_interp) {
            m_tool_interp->Shutdown();
            m_tool_interp.reset();
        }

        ScriptGlue::Shutdown();
        ScriptRuntime::Destroy(m_script_runtime);
        ScriptEngine::Destroy(m_script_engine);
    }

} // dodoe
