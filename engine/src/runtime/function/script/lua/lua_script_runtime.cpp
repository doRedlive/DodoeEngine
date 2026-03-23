//
// Created by GreenMuffin on 2026/3/x.
//


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

    bool LuaScriptRuntime::execute(const std::filesystem::path& script_file) {
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

} // dodoe
