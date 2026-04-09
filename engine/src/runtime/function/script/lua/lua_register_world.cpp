#include "lua_register_detail.h"

#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/registry.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/systems.h"

namespace dodoe::lua_register_detail {

    void register_world(sol::state& lua, sol::table& dodoe_table) {
        dodoe_table.new_usertype<Registry>("Registry",
            sol::no_constructor,
            "createEntity", &Registry::create,
            "destroyEntity", [](Registry& reg, const Entity& entity) { reg.destroy(entity); },
            "validEntity", [](Registry& reg, const Entity& entity) { return reg.valid(entity); },
            "clear", &Registry::clear
        );

        dodoe_table.new_usertype<Scene>("Scene",
            sol::no_constructor,
            "getName", &Scene::get_name,
            "setName", &Scene::set_name,
            "createEntity", [](Scene& scene, const std::string& name) { return scene.create_entity(name); },
            "destroyEntity", &Scene::destroy_entity,
            "getRegistry", [](Scene& scene) { return &scene.registry(); }
        );

        dodoe_table.new_usertype<World>("World",
            sol::no_constructor,
            "getName", &World::get_name,
            "createScene", &World::create_scene,
            "getScene", &World::get_scene,
            "activeScene", &World::active_scene,
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
            "removeStartSystem", &World::remove_start_system,
            "removeUpdateSystem", &World::remove_update_system,
            "removeAllSystems", &World::remove_all_systems
        );
    }

} // dodoe::lua_register_detail
