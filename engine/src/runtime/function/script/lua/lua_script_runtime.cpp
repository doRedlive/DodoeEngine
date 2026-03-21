#include "lua_script_runtime.h"
#include "lua_register.h"

namespace dodoe {

    ScriptLanguage LuaScriptRuntime::language() const {
        return ScriptLanguage::Lua;
    }

    bool LuaScriptRuntime::initialize() {
        lua_.open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table,
            sol::lib::coroutine,
            sol::lib::utf8
        );
        LuaRegister::register_all(lua_);
        modules_.clear();
        last_module_name_.reset();
        return true;
    }

    void LuaScriptRuntime::shutdown() {
        modules_.clear();
        last_module_name_.reset();
        lua_ = sol::state{};
    }

    bool LuaScriptRuntime::execute_file(const std::filesystem::path& script_file) {
        const auto script_path = script_file.lexically_normal();
        auto result = lua_.safe_script_file(script_path.string(), &sol::script_pass_on_error);
        if (!result.valid()) {
            const sol::error err = result;
            DoError("LuaScriptRuntime: execute file failed: {}, error: {}", script_path.string(), err.what());
            return false;
        }

        if (result.get_type() == sol::type::table) {
            sol::table module_table = result;
            std::string module_name = script_path.stem().string();

            sol::object module_name_obj = module_table["moduleName"];
            if (module_name_obj.valid() && module_name_obj.get_type() == sol::type::string) {
                module_name = module_name_obj.as<std::string>();
            }

            modules_[module_name] = module_table;
            last_module_name_ = module_name;
        }

        return true;
    }

    bool LuaScriptRuntime::invoke(const std::string& function_name) {
        sol::protected_function global_fn = lua_[function_name];
        if (global_fn.valid()) {
            auto result = global_fn();
            if (!result.valid()) {
                const sol::error err = result;
                DoError("LuaScriptRuntime: invoke global failed: {}, error: {}", function_name, err.what());
                return false;
            }
            return true;
        }

        if (last_module_name_.has_value()) {
            return invoke_module_function(last_module_name_.value(), function_name);
        }

        DoError("LuaScriptRuntime: function not found: {}", function_name);
        return false;
    }

    bool LuaScriptRuntime::invoke(const std::string& module_name, const std::string& function_name) {
        return invoke_module_function(module_name, function_name);
    }

    bool LuaScriptRuntime::invoke_module_function(const std::string& module_name, const std::string& function_name) {
        sol::table module_table{};

        if (auto module_it = modules_.find(module_name); module_it != modules_.end()) {
            module_table = module_it->second;
        } else {
            sol::object module_obj = lua_[module_name];
            if (!module_obj.valid() || module_obj.get_type() != sol::type::table) {
                DoError("LuaScriptRuntime: module not found: {}", module_name);
                return false;
            }
            module_table = module_obj.as<sol::table>();
        }

        sol::object function_obj = module_table[function_name];
        if (!function_obj.valid() || function_obj.get_type() != sol::type::function) {
            DoError("LuaScriptRuntime: function {} not found in module {}", function_name, module_name);
            return false;
        }

        sol::protected_function fn = function_obj.as<sol::protected_function>();
        auto result = fn(module_table);
        if (!result.valid()) {
            const sol::error err = result;
            DoError("LuaScriptRuntime: invoke module function failed: {}.{}, error: {}", module_name, function_name, err.what());
            return false;
        }

        return true;
    }

} // dodoe
