// 
// Created by GreenMuffin on 2026/3/20.
//

#pragma once

#include "dopch.h"

#include "script_engine.h"
#include "script_runtime.h"

#include "lua/lua_script_runtime.h"

namespace dodoe {

    struct ScriptSystemCreateInfo {

    };

    class ScriptSystem : public Managed<ScriptSystem, ScriptSystemCreateInfo> {
        friend class Managed<ScriptSystem, ScriptSystemCreateInfo>;
        Scope<ScriptEngine> m_script_engine;
        Scope<ScriptRuntime> m_script_runtime;

        Scope<LuaScriptEngine> m_lua_engine;
    public:

        [[nodiscard]] ScriptEngine* getMonoEngine() const { return m_script_engine.get(); }
        [[nodiscard]] ScriptRuntime* getMonoRuntime() const { return m_script_runtime.get(); }
        [[nodiscard]] LuaScriptEngine* getLuaEngine() const { return m_lua_engine.get(); }
        [[nodiscard]] bool executeLua(const std::filesystem::path& script_file) const;
        bool reloadScripts();

    private:
        bool initialize(const ScriptSystemCreateInfo& create_info);
        void shutdown();
    };

} // dodoe
