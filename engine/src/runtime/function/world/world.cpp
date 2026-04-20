//
// Created by GreenMuffin on 2026/2/6.
//

#include "world.h"

#include "scene.h"

#include "systems/animation2d_system.h"
#include "systems/camera2d_system.h"
#include "systems/model_renderer_system.h"
#include "systems/physics2d_system.h"
#include "systems/sprite_renderer_system.h"
#include "systems/mono_system.h"

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
        m_name = create_info.name;

        auto mono = create_ref<MonoSystem>();
        auto camera2d = create_ref<Camera2dSystem>();
        auto physics2d = create_ref<Physics2dSystem>();
        auto animation2d = create_ref<Animation2dSystem>();
        auto model_renderer = create_ref<ModelRendererSystem>();
        auto sprite_renderer = create_ref<SpriteRendererSystem>();

        registerRuntimeSystem(mono);
        registerRuntimeSystem(camera2d);
        registerRuntimeSystem(physics2d);
        registerRuntimeSystem(animation2d);
        registerRuntimeSystem(model_renderer);
        registerRuntimeSystem(sprite_renderer);

        registerSimulationSystem(camera2d);
        registerSimulationSystem(physics2d);
        registerSimulationSystem(animation2d);
        registerSimulationSystem(model_renderer);
        registerSimulationSystem(sprite_renderer);
        
        // Read config
        auto* scene = getCurrentScene();
        if (!scene) {
            scene = createScene("default");
            loadScene("default");
        }
    }

    void World::shutdown() {
        m_active_scenes.clear();
        m_runtime_systems.clear();
        m_simulation_systems.clear();

        for (auto& scene : m_scenes) {
            Scene::destroy(scene);
        }
    }

    void World::start() {
        switch (m_state) {
            case WorldState::Runtime:
                for (auto& scene : m_active_scenes) {
                    scene->onRuntimeStart();
                }
            break;
            case WorldState::Simulation:
                for (auto& scene : m_active_scenes) {
                    scene->onSimulationStart();
                }
            break;
        }
    }

    void World::update(const float dt) {
        switch (m_state) {
            case WorldState::Runtime:
                for (auto& scene : m_active_scenes) {
                    scene->onRuntimeUpdate(dt);
                }
            break;
            case WorldState::Simulation:
                for (auto& scene : m_active_scenes) {
                    scene->onSimulationUpdate(dt);
                }
            break;
        }
    }

    void World::finalize() {
        switch (m_state) {
            case WorldState::Runtime:
                for (auto& scene : m_active_scenes) {
                    scene->onRuntimeStop();
                }
            break;
            case WorldState::Simulation:
                for (auto& scene : m_active_scenes) {
                    scene->onSimulationStop();
                }
            break;
        }
    }

    Scene* World::createScene(const std::string& name) { 
        if (auto* existing_scene = getScene(name)) {
            return existing_scene;
        }

        auto scene = Scene::create({*this, name});
        scene->onCreate();
        Scene* created_scene = scene.get();
        m_scenes.push_back(std::move(scene));

        return created_scene;
    }

    void World::deleteScene(const std::string& name) {
        auto scene = getScene(name);
        if (!scene) return;

        auto active_it = std::find_if(m_active_scenes.begin(), m_active_scenes.end(),
            [&name](const auto& scene) {
                return scene->getName() == name; }
        );
        if (active_it != m_active_scenes.end()) {
            DO_ERROR("Scene with name '{}' has loaded! Can't delete!", name);
            return;
        }

        scene->onDelete();

        auto it = std::find_if(m_scenes.begin(), m_scenes.end(),
            [&name](const auto& scene) {
                return scene->getName() == name; }
        );
        m_scenes.erase(it);
    }

    Scene* World::getScene(const std::string& name) const {
        for (const auto& scene : m_scenes) {
            if (scene->getName() == name) {
                return scene.get();
            }
        }
        DoWarn("Scene with name '{}' does not exist. Returning nullptr.", name);
        return nullptr;
    }

    Scene* World::getCurrentScene() {
        if (!m_current_scene && !m_active_scenes.empty()) {
            m_current_scene = m_active_scenes.front();
        }
        return m_current_scene;
    }

    void World::loadScene(const std::string& name) {
        auto scene = getScene(name);
        if (!scene) return;

        auto it = std::find_if(m_active_scenes.begin(), m_active_scenes.end(),
            [&name](const auto& scene) {
                return scene->getName() == name; }
        );

        if (it != m_active_scenes.end()) {
            DO_ERROR("Scene with name '{}' has loaded!", name);
            return;
        }

        m_active_scenes.push_back(scene);
    }

    void World::unloadScene(const std::string& name) {
        auto scene = getScene(name);
        if (!scene) return;

        auto it = std::find_if(m_active_scenes.begin(), m_active_scenes.end(),
            [&name](const auto& scene) {
                return scene->getName() == name;
            });

        if (it == m_active_scenes.end()) {
            DO_ERROR("Scene with name '{}' does not loaded!", name);
            return;
        }

        m_active_scenes.erase(it);

    }

    void World::registerRuntimeSystem(Ref<System> system) {
        if (!system) return;
        m_runtime_systems.push_back(std::move(system));
    }

    void World::registerSimulationSystem(Ref<System> system) {
        if (!system) return;
        m_simulation_systems.push_back(std::move(system));
    }

    void World::onRuntimeStart(Registry& reg) {
        for (auto& sys : m_runtime_systems) {
            if (sys) {
                sys->start(reg);
            }
        }
    }

    void World::onRuntimeUpdate(Registry& reg, const float dt) {
        for (auto& sys : m_runtime_systems) {
            if (sys) {
                sys->update(reg, dt);
            }
        }
    }

    void World::onRuntimeFinalize(Registry& reg) {
        for (auto& sys : m_runtime_systems) {
            if (sys) {
                sys->finalize(reg);
            }
        }
    }

    void World::onSimulationStart(Registry& reg) {
        for (auto& sys : m_simulation_systems) {
            if (sys) {
                sys->start(reg);
            }
        }
    }

    void World::onSimulationUpdate(Registry& reg, const float dt) {
        for (auto& sys : m_simulation_systems) {
            if (sys) {
                sys->update(reg, dt);
            }
        }
    }

    void World::onSimulationFinalize(Registry& reg) {
        for (auto& sys : m_simulation_systems) {
            if (sys) {
                sys->finalize(reg);
            }
        }
    }
} // dodoe
