// do@GreenMuffin

#include "script_system.h"

#include "script_engine.h"
#include "script_glue.h"
#include "script_runtime.h"
#include "lua/lua_script_runtime.h"

namespace dodoe {

    bool ScriptSystem::executeLua(const std::filesystem::path& script_file) const {
        if (m_lua_engine) {
            return m_lua_engine->execute(script_file);
        }
        return false;
    }

    bool ScriptSystem::reloadScripts() {
        const bool reloaded = m_script_engine->reloadScripts();
        if (!reloaded) {
            return false;
        }

        m_script_runtime->reloadAssemblyClasses();
        ScriptGlue::Register();
        return true;
    }

    bool ScriptSystem::initialize(const ScriptSystemCreateInfo& create_info) {
        m_script_engine = ScriptEngine::Create({});
        if (!m_script_engine) {
            DO_ASSERT(false, "SriptEngine initialize failed!");
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

        m_lua_engine = create_scope<LuaScriptEngine>();
        m_lua_engine->initialize();

        return m_lua_engine != nullptr;
    }

    void ScriptSystem::shutdown() {
        ScriptGlue::Shutdown();
        ScriptRuntime::Destroy(m_script_runtime);
        ScriptEngine::Destroy(m_script_engine);
        if (m_lua_engine) {
            m_lua_engine->shutdown();
            m_lua_engine.reset();
        }
    }

} // dodoe
