//
// Created by GreenMuffin on 2026/3/x.
//
#ifndef DODOE_LUA_REGISTER_DETAIL_H
#define DODOE_LUA_REGISTER_DETAIL_H

#include "sol/sol.hpp"

namespace dodoe::lua_register_detail {

    void register_core(sol::state& lua, sol::table& dodoe_table);
    void register_resource(sol::state& lua, sol::table& dodoe_table);
    void register_entity(sol::state& lua, sol::table& dodoe_table);
    void register_world(sol::state& lua, sol::table& dodoe_table);

} // dodoe::lua_register_detail

#endif//DODOE_LUA_REGISTER_DETAIL_H
