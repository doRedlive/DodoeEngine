#include "lua_register_detail.h"

#include "runtime/core/world/world_manager.h"
#include "runtime/core/world/world.h"
#include "runtime/core/world/scene.h"
#include "runtime/core/world/registry.h"
#include "runtime/core/world/entity.h"

namespace dodoe::lua_register_detail {

    void register_world_system(sol::state& lua, sol::table& dodoe_table) {
        lua.new_usertype<Registry>("Registry",
            sol::no_constructor,
            "createEntity", &Registry::create,
            "destroyEntity", [](Registry& reg, const Entity& entity) { reg.destroy(entity); },
            "validEntity", [](Registry& reg, const Entity& entity) { return reg.valid(entity); },
            "clear", &Registry::clear
        );

        lua.new_usertype<Scene>("Scene",
            sol::no_constructor,
            "getName", &Scene::get_name,
            "setName", &Scene::set_name,
            "createEntity", [](Scene& scene, const std::string& name) { return scene.create_entity(name); },
            "destroyEntity", &Scene::destroy_entity,
            "getRegistry", [](Scene& scene) { return &scene.registry(); }
        );

        lua.new_usertype<World>("World",
            sol::no_constructor,
            "getName", &World::get_name,
            "createScene", &World::create_scene,
            "getScene", &World::get_scene,
            "getActiveScene", &World::active_scene,
            "loadScene", &World::load_scene,
            "destroyScene", &World::destroy_scene,
            "destroyAllScenes", &World::destroy_all_scenes,
            "addStartSystem", [](World& world, const sol::function& callback) {
                return world.add_start_system([callback](Registry& reg) {
                    sol::protected_function pf = callback;
                    auto result = pf(std::ref(reg));
                    if (!result.valid()) {
                        sol::error err = result;
                        DoError("Lua World addStartSystem callback failed: {}", err.what());
                    }
                });
            },
            "addUpdateSystem", [](World& world, const sol::function& callback) {
                return world.add_update_system([callback](Registry& reg, const float dt) {
                    sol::protected_function pf = callback;
                    auto result = pf(std::ref(reg), dt);
                    if (!result.valid()) {
                        sol::error err = result;
                        DoError("Lua World addUpdateSystem callback failed: {}", err.what());
                    }
                });
            },
            "startSystemCount", [](World& world) { return static_cast<int>(world.load_start_systems().size()); },
            "updateSystemCount", [](World& world) { return static_cast<int>(world.load_update_systems().size()); },
            "removeStartSystem", &World::remove_start_system,
            "removeUpdateSystem", &World::remove_update_system,
            "removeAllSystems", &World::remove_all_systems
        );

        sol::table world_manager_table = dodoe_table["worldManager"];
        if (!world_manager_table.valid()) {
            world_manager_table = lua.create_table();
            dodoe_table["worldManager"] = world_manager_table;
        }

        world_manager_table.set_function("worldCount", []() { return WorldManager::self().world_count(); });
        world_manager_table.set_function("createWorld",
                                         [](const std::string& name) -> World* { return &WorldManager::self().create_world(name); });
        world_manager_table.set_function("getWorld",
                                         [](const std::string& name) -> World* { return &WorldManager::self().get_world(name); });
        world_manager_table.set_function("activeWorld", []() -> World* { return &WorldManager::self().active_world(); });
        world_manager_table.set_function("destroyWorlds", []() { WorldManager::self().destory_worlds(); });
    }

} // dodoe::lua_register_detail
