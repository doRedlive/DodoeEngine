// do@GreenMuffin

#include "script_system.h"

#include "script_engine.h"
#include "script_glue.h"
#include "script_runtime.h"
#include "lua/lua_script_runtime.h"

namespace dodoe {

    Scope<ScriptSystem> ScriptSystem::create(const ScriptSystemCreateInfo& info) {
        if (auto context = create_scope<ScriptSystem>(); context->initialize(std::move(info)))
            return context;
        return nullptr;
    }

    void ScriptSystem::destroy(Scope<ScriptSystem>& system) {
        if (!system) return;
        system->shutdown();
        system.reset();
    }

    bool ScriptSystem::executeLua(const std::filesystem::path& script_file) const {
        if (m_lua_engine) {
            return m_lua_engine->execute(script_file);
        }
        return false;
    }

    bool ScriptSystem::initialize(const ScriptSystemCreateInfo& create_info) {
        m_script_engine = ScriptEngine::create({});
        m_script_runtime = ScriptRuntime::create({m_script_engine.get()});

        ScriptGlue::Initialize(m_script_engine.get());
        ScriptGlue::Register();

        if (m_script_runtime) {
            m_script_runtime->loadAssemblyClasses();
        }

        m_lua_engine = create_scope<LuaScriptEngine>();
        m_lua_engine->initialize();

        return m_script_engine && m_script_runtime;
    }

    void ScriptSystem::shutdown() {
        ScriptGlue::Shutdown();
        ScriptRuntime::destroy(m_script_runtime);
        ScriptEngine::destroy(m_script_engine);
        if (m_lua_engine) {
            m_lua_engine->shutdown();
            m_lua_engine.reset();
        }
    }

} // dodoe
