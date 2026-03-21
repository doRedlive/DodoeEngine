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

        bool execute_file(const std::filesystem::path& script_file) override;
        bool invoke(const std::string& function_name) override;
        bool invoke(const std::string& module_name, const std::string& function_name) override;

    private:
        bool invoke_module_function(const std::string& module_name, const std::string& function_name);

        sol::state lua_{};
        std::unordered_map<std::string, sol::table> modules_{};
        std::optional<std::string> last_module_name_{};
    };

} // dodoe

#endif//DODOE_LUA_SCRIPT_RUNTIME_H
