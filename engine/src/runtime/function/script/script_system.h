// 
// Created by GreenMuffin on 2026/3/20.
//

#ifndef DODOE_SCRIPT_SYSTEM_H
#define DODOE_SCRIPT_SYSTEM_H

#include "dopch.h"
#include "lua/lua_script_runtime.h"

namespace dodoe {

    struct ScriptSystemCreateInfo {

    };

    class ScriptSystem {
    public:
        static Scope<ScriptSystem> create(const ScriptSystemCreateInfo& create_info);
        static void destroy(Scope<ScriptSystem>& script_system);

        bool execute_csharp(const std::filesystem::path& script_file);
        bool execute_lua(const std::filesystem::path& script_file);

        LuaScriptEngine* get_lua_engine() { return lua_engine_.get(); }

    private:
        bool initialize(const ScriptSystemCreateInfo& create_info);
        void shutdown();

        Scope<LuaScriptEngine> lua_engine_;
        bool csharp_initialized_{ false };
        bool enable_csharp_on_startup_{ false };
    };

} // dodoe

#endif//DODOE_SCRIPT_SYSTEM_H
