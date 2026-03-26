//
// Created by GreenMuffin on 2026/2/6.
//

#include "world.h"

#include "scene.h"
#include "systems.h"

#include "runtime/core/world/components.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/resource/resource_manager.h"

namespace dodoe {

    WorldProperty World::property = WorldProperty();

    Scope<World> World::create(WorldCreateInfo create_info) {
        auto context = Scope<World>(new World(std::string(create_info.name)));
        context->initialize();
        return context;
    }

    void World::destroy(Scope<World>& world) {
        world->shutdown();
        world.reset();
    }

    World::World(const std::string& in_name) : name_(in_name) { }

    void World::initialize() {
        add_start_system(systems::Physics2dStartSystem);
        add_update_system(systems::Physics2dUpdateSystem);
        add_update_system(systems::CameraUpdateSystem);
        add_update_system(systems::Animation2dUpdateSystem);
        add_update_system(systems::SpriteRendererUpdateSystem);

        // Read config
        auto* scene = active_scene();
        if (!scene) {
            scene = create_scene("default");
        }
    }

    void World::shutdown() {
        if (!scenes_.empty()) {
            destroy_all_scenes();
        }
        remove_all_systems();
    }

    void World::runtime_start() {
        for (auto& scene : scenes_) {
            scene->on_runtime_start();
        }
    }

    void World::runtime_update(const float delta_time) {
        for (auto& scene : scenes_) {
            scene->on_runtime_update(delta_time);
        }
    }

    void World::runtime_finalize() {
        for (auto& scene : scenes_) {
            scene->on_runtime_stop();
        }
    }

    void World::simulation_update(const float delta_time) {
        for (auto& scene : scenes_) {
            scene->on_simulation_update(delta_time);
        }
    }

    Scene* World::create_scene(const std::string& name) {
        for (const auto& scene : scenes_) {
            if (scene->get_name() == name) {
                DoError("Scene with name '{}' already exists. Returning existing Scene.", name);
                return scene.get();
            }
        }
        auto scene = create_scope<Scene>(*this, name);
        scenes_.push_back(std::move(scene));
        return get_scene(name);
    }

    Scene* World::get_scene(const std::string& name) const {
        for (const auto& scene : scenes_) {
            if (scene->get_name() == name) {
                return scene.get();
            }
        }
        DoError("Scene with name '{}' does not exist. Returning nullptr.", name);
        return nullptr;
    }

    Scene* World::active_scene() const {
        return scenes_.empty() ? nullptr : scenes_.back().get();
    }

    void World::load_scene(const std::string& name) {
        if (scenes_.back()->get_name() == name) {
            return;
        }
        for (size_t i = 0; i < scenes_.size(); ++i) {
            if (scenes_[i]->get_name() == name) {
                auto scene = std::move(scenes_[i]);
                scenes_.erase(scenes_.begin() + i);
                scenes_.push_back(std::move(scene));
                return;
            }
        }
        DoError("Scene with name '{}' does not exist. Cannot load.", name);
    }

    void World::destroy_scene(const std::string& name) {
        for (size_t i = 0; i < scenes_.size(); ++i) {
            if (scenes_[i]->get_name() == name) {
                scenes_[i]->destroy();
                scenes_.erase(scenes_.begin() + i);
                return;
            }
        }
        DoError("Scene with name '{}' does not exist. Cannot destroy.", name);
    }

    void World::destroy_all_scenes() {
        for (const auto& scene : scenes_) {
            scene->destroy();
        }
        scenes_.clear();
    }

    int World::add_start_system(StartSystem start) {
        start_systems_.push_back(start);
        return static_cast<int>(start_systems_.size() - 1);
    }

    int World::add_update_system(UpdateSystem update) {
        update_systems_.push_back(update);
        return static_cast<int>(update_systems_.size() - 1);
    }

    const std::vector<StartSystem>& World::load_start_systems() {
        return start_systems_;
    }

    const std::vector<UpdateSystem>& World::load_update_systems() {
        return update_systems_;
    }

    void World::remove_start_system(const int id) {
        start_systems_.erase(start_systems_.begin() + id);  // TODO, dirty flag and remove all at a time point.
    }

    void World::remove_update_system(const int id) {
        update_systems_.erase(update_systems_.begin() + id);
    }

    void World::remove_all_systems() {
        start_systems_.clear();
        update_systems_.clear();
    }

} // dodoe
