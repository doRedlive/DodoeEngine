//
// Created by GreenMuffin on 2026/3/x.
//
#ifndef DODOE_LUA_SCRIPT_ENGINE_H
#define DODOE_LUA_SCRIPT_ENGINE_H

#include "dopch.h"
#include "sol/sol.hpp"

namespace dodoe {

    class LuaScriptEngine {
    public:
        bool initialize();
        void shutdown();

        bool execute(const std::filesystem::path& script_file);

    private:
        sol::state lua_{};
        std::unordered_map<std::string, sol::table> modules_{};
        std::optional<std::string> last_module_name_{};
    };

} // dodoe

#endif//DODOE_LUA_SCRIPT_ENGINE_H
