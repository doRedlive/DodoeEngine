#ifndef DODOE_LUA_REGISTER_DETAIL_H
#define DODOE_LUA_REGISTER_DETAIL_H

#include "sol/sol.hpp"

namespace dodoe::lua_register_detail {

    void register_log_time(sol::state& lua, sol::table& dodoe_table);
    void register_resource_manager(sol::state& lua, sol::table& dodoe_table);
    void register_math_types(sol::state& lua, sol::table& dodoe_table);
    void register_components_entity(sol::state& lua, sol::table& dodoe_table);
    void register_world_system(sol::state& lua, sol::table& dodoe_table);

} // dodoe::lua_register_detail

#endif//DODOE_LUA_REGISTER_DETAIL_H
