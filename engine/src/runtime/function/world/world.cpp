//
// Created by GreenMuffin on 2026/2/6.
//

#include "world.h"

#include "scene.h"

#include "systems/animation2d_system.h"
#include "systems/camera2d_system.h"
#include "systems/physics2d_system.h"
#include "systems/sprite_renderer_system.h"

namespace dodoe {

    Scope<World> World::create(const WorldCreateInfo& create_info) {
        auto context = create_scope<World>();
        context->initialize(create_info);
        return context;
    }

    void World::destroy(Scope<World>& world) {
        if (!world) return;
        world->shutdown();
        world.reset();
    }

    void World::initialize(const WorldCreateInfo& create_info) {
        name_ = create_info.name;

        systems_.clear();
        systems_.push_back(create_scope<Physics2dSystem>());
        systems_.push_back(create_scope<Camera2dSystem>());
        systems_.push_back(create_scope<Animation2dSystem>());
        systems_.push_back(create_scope<SpriteRendererSystem>());

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
        systems_.clear();
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

    void World::register_system(Scope<System> system) {
        if (!system) {
            return;
        }
        systems_.push_back(std::move(system));
    }

    void World::start_systems(Registry& reg) {
        for (auto& sys : systems_) {
            if (sys) {
                sys->start(reg);
            }
        }
    }

    void World::update_systems(Registry& reg, const float dt) {
        for (auto& sys : systems_) {
            if (sys) {
                sys->update(reg, dt);
            }
        }
    }

    void World::finalize_systems(Registry& reg) {
        for (auto& sys : systems_) {
            if (sys) {
                sys->finalize(reg);
            }
        }
    }

} // dodoe
