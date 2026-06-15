// do@Redlive

#include "world.h"

#include "scene.h"

#include "runtime/core/project/project.h"
#include "runtime/resource/res_type/scene_res.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/core/meta/serializer/serializer.h"

#include "systems/animation2d_system.h"
#include "systems/camera2d_system.h"
#include "systems/foliage_renderer_system.h"
#include "systems/light_system.h"
#include "systems/line_renderer_system.h"
#include "systems/sky_light_system.h"
#include "systems/mesh_renderer_system.h"
#include "systems/physics2d_system.h"
#include "systems/rect_renderer_system.h"
#include "systems/sprite_renderer_system.h"
#include "systems/mono_system.h"
#include "systems/tilemap_renderer_system.h"

namespace dodoe {

    bool World::initialize(const WorldCreateInfo& create_info) {
        m_name = create_info.name;
        if (!setupScenes()) return false;
        if (!setupSystems()) return false;
        return true;
    }

    void World::shutdown() {
        cleanupScenes();
        cleanupSystems();
        for (auto& scene : m_scenes) {
            Scene::Destroy(scene);
        }
    }

    Bool World::setupScenes() {
        const auto scene_assets = ResourceManager::Self().getAssets<SceneAsset>();
        for (const auto& scene_handle : scene_assets) {
            SceneAsset* scene_asset = scene_handle.get();
            if (!scene_asset) continue;
            const auto& scene_res = scene_asset->getSceneRes();
            const String scene_name = scene_res.m_name.empty()
                ? "Untitled"
                : scene_res.m_name;
            Scene* scene = createScene(scene_name);
            scene->deserialize(scene_res);
        }
        return true;
    }

    void World::cleanupScenes() {
        m_active_scenes.clear();
    }

    bool World::setupSystems() {
        auto mono = create_ref<MonoSystem>();
        auto camera2d = create_ref<Camera2dSystem>();
        auto light_system = create_ref<LightSystem>();
        auto sky_light = create_ref<SkyLightSystem>();
        auto physics2d = create_ref<Physics2dSystem>();
        auto animation2d = create_ref<Animation2dSystem>();
        auto foliage_renderer = create_ref<FoliageRendererSystem>();
        auto mesh_system = create_ref<MeshRendererSystem>();
        auto sprite_renderer = create_ref<SpriteRendererSystem>();
        auto tilemap_renderer = create_ref<TilemapRendererSystem>();
        auto rect_renderer = create_ref<RectRendererSystem>();
        auto line_renderer = create_ref<LineRendererSystem>();
        registerRuntimeSystem(mono);
        registerRuntimeSystem(camera2d);
        registerRuntimeSystem(light_system);
        registerRuntimeSystem(sky_light);
        registerRuntimeSystem(physics2d);
        registerRuntimeSystem(animation2d);
        registerRuntimeSystem(foliage_renderer);
        registerRuntimeSystem(mesh_system);
        registerRuntimeSystem(sprite_renderer);
        registerRuntimeSystem(tilemap_renderer);
        registerRuntimeSystem(rect_renderer);
        registerRuntimeSystem(line_renderer);

        registerSimulationSystem(camera2d);
        registerSimulationSystem(light_system);
        registerSimulationSystem(physics2d);
        registerSimulationSystem(animation2d);
        registerSimulationSystem(foliage_renderer);
        registerSimulationSystem(mesh_system);
        registerSimulationSystem(sprite_renderer);
        registerSimulationSystem(tilemap_renderer);
        registerSimulationSystem(rect_renderer);
        registerSimulationSystem(line_renderer);

        return true;
    }

    void World::cleanupSystems() {
        m_runtime_systems.clear();
        m_simulation_systems.clear();
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
            case WorldState::Pause:
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
            case WorldState::Pause:
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
            case WorldState::Pause:
            break;
        }
    }

    Scene* World::createScene(const std::string& name) { 
        for (const auto& existing_scene : m_scenes) {
            if (existing_scene && existing_scene->getName() == name) {
                return existing_scene.get();
            }
        }

        auto scene = Scene::Create({*this, name});
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

    bool World::activateStartScene() {
        const auto active_project = Project::ActiveProject();
        if (active_project->config().start_scene_name.empty()) {
            DO_ASSERT(false, "StartSceneName is empty!");
            return false;
        }

        Scene* start_scene = getScene(active_project->config().start_scene_name);
        if (!start_scene) {
            return false;
        }

        loadScene(start_scene->getName());
        setCurrentScene(start_scene);
        return true;
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
