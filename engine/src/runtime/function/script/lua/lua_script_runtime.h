//
// Created by GreenMuffin on 2026/3/x.
//
#ifndef DODOE_LUA_SCRIPT_RUNTIME_H
#define DODOE_LUA_SCRIPT_RUNTIME_H

#include "runtime/function/script/script_runtime.h"

#include "sol/sol.hpp"

namespace dodoe {

    class LuaScriptRuntime final : public IScriptRuntime {
    public:
        ScriptLanguage language() const override;
        bool initialize() override;
        void shutdown() override;

        bool execute(const std::filesystem::path& script_file) override;

    private:
        sol::state lua_{};
        std::unordered_map<std::string, sol::table> modules_{};
        std::optional<std::string> last_module_name_{};
    };

} // dodoe

#endif//DODOE_LUA_SCRIPT_RUNTIME_H
