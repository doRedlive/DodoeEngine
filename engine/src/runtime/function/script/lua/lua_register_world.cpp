#include "lua_register_detail.h"

#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/world/registry.h"
#include "runtime/function/world/entity.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/world/systems/lua_system.h"

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
            "registerSystem", [](World& self, const sol::table& system_table) {
                self.register_system(create_scope<LuaSystem>(system_table));
            }
        );

        dodoe_table.set_function("getWorld", []() -> World* {
            return Application::self().context().world.get();
        });
    }

} // dodoe::lua_register_detail
