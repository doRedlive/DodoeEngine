#include "lua_register_detail.h"

#include "runtime/resource/resource_manager.h"

namespace dodoe::lua_register_detail {

    void register_resource_manager(sol::state& lua, sol::table& dodoe_table) {
        sol::table resource_manager_table = dodoe_table["resourceManager"];
        if (!resource_manager_table.valid()) {
            resource_manager_table = lua.create_table();
            dodoe_table["resourceManager"] = resource_manager_table;
        }

        resource_manager_table.set_function("loadTexture", [](const std::string& path) -> bool {
            return ResourceManager::self().load_texture(path).texture != nullptr;
        });
        resource_manager_table.set_function("getTexture", [](const std::string& path) -> bool {
            return ResourceManager::self().get_texture(path).texture != nullptr;
        });
        resource_manager_table.set_function("loadShader", [](const std::string& name, const std::string& vertPath, const std::string& fragPath) -> bool {
            return ResourceManager::self().load_shader(name, vertPath, fragPath) != nullptr;
        });
        resource_manager_table.set_function("getShader", [](const std::string& name) -> bool {
            return ResourceManager::self().get_shader(name) != nullptr;
        });
    }

} // dodoe::lua_register_detail
