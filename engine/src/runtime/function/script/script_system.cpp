// do@GreenMuffin

#include "script_system.h"

#include "script_engine.h"
#include "script_runtime.h"
#include "lua/lua_script_runtime.h"

namespace dodoe {

    Scope<ScriptSystem> ScriptSystem::create(const ScriptSystemCreateInfo& create_info) {
        if (auto context = create_scope<ScriptSystem>(); context->initialize(std::move(create_info))) 
            return context;
        return nullptr;
    }

    void ScriptSystem::destroy(Scope<ScriptSystem>& script_system) {
        if (!script_system) return;
        script_system->shutdown();
        script_system.reset();
    }

    bool ScriptSystem::execute_csharp(const std::filesystem::path& script_file) {
        return true;
    }

    bool ScriptSystem::execute_lua(const std::filesystem::path& script_file) {
        if (lua_engine_) {
            return lua_engine_->execute(script_file);
        }
        return false;
    }

    bool ScriptSystem::initialize(const ScriptSystemCreateInfo& create_info) {
        lua_engine_ = create_scope<LuaScriptEngine>();
        lua_engine_->initialize();

        ScriptRuntime::initialize();

        return true;
    }

    void ScriptSystem::shutdown() {
        if (lua_engine_) {
            lua_engine_->shutdown();
            lua_engine_.reset();
        }
    }

} // dodoe
