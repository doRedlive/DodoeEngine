// do@Redlive

#include "world.h"

#include "scene.h"

#include "runtime/core/project/project.h"
#include "runtime/resource/res_type/scene_res.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/async/task_scheduler.h"

#include "systems/animation2d_system.h"
#include "systems/camera_system.h"
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

    bool World::s_force_sequential = false;

    namespace {

        void AddEdgeHelper(
            DynamicArray<DynamicArray<Size_t>>& edges,
            DynamicArray<Int32>& indegree,
            Size_t from,
            Size_t to)
        {
            if (from == to) return;
            edges[from].push_back(to);
            indegree[to] += 1;
        }

        void BuildGraphForSystems(
            TaskGraph& graph,
            const std::vector<Ref<System>>& systems)
        {
            graph.reset();

            const Size_t count = systems.size();
            if (count == 0) return;

            DynamicArray<SystemAccess> accesses(count);
            for (Size_t i = 0; i < count; i++) {
                if (systems[i]) {
                    accesses[i] = systems[i]->getAccess();
                }
            }

            for (Size_t i = 0; i < count; i++) {
                graph.addNode(String{}, {});
            }

            UnorderedMap<entt::id_type, Int32> producer{};
            UnorderedMap<entt::id_type, DynamicArray<Size_t>> readers{};

            for (Size_t i = 0; i < count; i++) {
                const auto& access = accesses[i];

                for (const auto type_hash : access.writes) {
                    auto prod_it = producer.find(type_hash);
                    if (prod_it != producer.end() && prod_it->second >= 0) {
                        graph.addEdge(
                            static_cast<TaskGraph::NodeId>(prod_it->second),
                            static_cast<TaskGraph::NodeId>(i));
                    }

                    auto rd_it = readers.find(type_hash);
                    if (rd_it != readers.end()) {
                        for (const auto reader_idx : rd_it->second) {
                            graph.addEdge(
                                static_cast<TaskGraph::NodeId>(reader_idx),
                                static_cast<TaskGraph::NodeId>(i));
                        }
                        rd_it->second.clear();
                    }

                    producer[type_hash] = static_cast<Int32>(i);
                }

                for (const auto type_hash : access.reads) {
                    auto prod_it = producer.find(type_hash);
                    if (prod_it != producer.end() && prod_it->second >= 0) {
                        graph.addEdge(
                            static_cast<TaskGraph::NodeId>(prod_it->second),
                            static_cast<TaskGraph::NodeId>(i));
                    }
                    readers[type_hash].push_back(i);
                }
            }

            graph.compile();
        }

        void ExecuteSystemsParallel(
            const TaskGraph& graph,
            const std::vector<Ref<System>>& systems,
            Registry& reg,
            float dt,
            WorldCommands& cmd_buf)
        {
            if (World::IsForceSequential()) {
                for (auto& sys : systems) {
                    if (sys) {
                        sys->update(reg, dt);
                    }
                }
                cmd_buf.apply(reg);
                cmd_buf.reset();
                return;
            }

            const auto& levels = graph.getLevels();
            auto& scheduler = TaskScheduler::Self();

            for (const auto& level : levels) {
                if (level.size() == 1) {
                    const auto idx = level[0];
                    auto& sys = systems[idx];
                    if (sys) {
                        sys->update(reg, dt);
                    }
                }
                else {
                    std::atomic<Size_t> completed{0};

                    for (const auto idx : level) {
                        scheduler.submit([&systems, &reg, dt, idx, &completed]() {
                            auto& sys = systems[idx];
                            if (sys) {
                                sys->update(reg, dt);
                            }
                            completed.fetch_add(1, std::memory_order_relaxed);
                        });
                    }

                    while (completed.load(std::memory_order_relaxed) < level.size()) {
                        std::this_thread::yield();
                    }
                }
            }

            cmd_buf.apply(reg);
            cmd_buf.reset();
        }

    } // anonymous namespace

    bool World::initialize(const WorldCreateInfo& create_info) {
        m_name = create_info.name;
#ifdef DODOE_EDITOR_ENABLED
        m_state = WorldState::Simulation;
#endif
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
            const auto& file_id = scene_handle.getFileID();
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
        auto camera_system = create_ref<CameraSystem>();
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
        registerRuntimeSystem(camera_system);
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

        registerSimulationSystem(camera_system);
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
        m_task_graph_dirty = true;
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
        drainAsyncCompletions();

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

    Scene* World::createScene(const String& name) {
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

    void World::deleteScene(const String& name) {
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

    Scene* World::getScene(const String& name) const {
        for (const auto& scene : m_scenes) {
            if (scene->getName() == name) {
                return scene.get();
            }
        }
        return nullptr;
    }

    Scene* World::getActiveScene() {
        if (!m_current_scene && !m_active_scenes.empty()) {
            m_current_scene = m_active_scenes.front();
        }
        return m_current_scene;
    }

    Scene* World::loadScene(const String& name, LoadSceneMode mode) {
        if (mode == LoadSceneMode::Single) {
            auto scene_to_unload = m_active_scenes;
            for (auto* scene : scene_to_unload) {
                deactivateScene(scene->getName());
            }
        }

        auto* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            DO_ERROR("loadScene: AssetManager not available");
            return nullptr;
        }

        const std::string asset_url = (FsPath("Scenes") / (name + ".doscn")).generic_string();
        auto handle = asset_manager->loadSceneAsset(asset_url);
        if (!handle.isValid() || !handle.isLoaded()) {
            DO_ERROR("loadScene: failed to load '{}'", asset_url);
            return nullptr;
        }

        SceneAsset* scene_asset = handle.get();
        if (!scene_asset) {
            DO_ERROR("loadScene: scene asset is null for '{}'", name);
            return nullptr;
        }

        const auto& scene_res = scene_asset->getSceneRes();
        Scene* scene = createScene(scene_res.m_name.empty() ? name : scene_res.m_name);
        scene->deserialize(scene_res);
        activateScene(scene->getName());

        if (mode == LoadSceneMode::Single || m_current_scene == nullptr) {
            setActiveScene(scene);
        }

        return scene;
    }

    void World::unloadScene(const String& name) {
        auto scene = getScene(name);
        if (!scene) return;

        deactivateScene(name);

        if (m_current_scene == scene) {
            m_current_scene = m_active_scenes.empty() ? nullptr : m_active_scenes.front();
        }

        scene->onDelete();
        auto it = std::find_if(m_scenes.begin(), m_scenes.end(),
            [&name](const auto& s) { return s->getName() == name; });
        if (it != m_scenes.end()) {
            m_scenes.erase(it);
        }
    }

    void World::activateScene(const String& name) {
        auto scene = getScene(name);
        if (!scene) return;

        auto it = std::find_if(m_active_scenes.begin(), m_active_scenes.end(),
            [&name](const auto& s) { return s->getName() == name; });
        if (it != m_active_scenes.end()) {
            DO_ERROR("Scene '{}' is already active!", name);
            return;
        }

        m_active_scenes.push_back(scene);
    }

    void World::deactivateScene(const String& name) {
        auto it = std::find_if(m_active_scenes.begin(), m_active_scenes.end(),
            [&name](const auto& s) { return s->getName() == name; });
        if (it == m_active_scenes.end()) {
            DO_ERROR("Scene '{}' is not active!", name);
            return;
        }
        m_active_scenes.erase(it);
    }

    bool World::activateStartScene() {
        setupScenes();
        const auto active_project = Project::ActiveProject();
        if (active_project->config().start_scene_name.empty()) {
            DO_ASSERT(false, "StartSceneName is empty!");
            return false;
        }

        Scene* start_scene = getScene(active_project->config().start_scene_name);
        if (!start_scene) {
            DO_ERROR("World::activateStartScene: start scene '{}' not found. scene_count={}",
                active_project->config().start_scene_name,
                m_scenes.size());
            for (const auto& scene : m_scenes) {
                DO_ERROR("World::activateStartScene: available scene='{}'", scene ? scene->getName() : "<null>");
            }
            return false;
        }

        activateScene(start_scene->getName());
        setActiveScene(start_scene);
        return true;
    }

    void World::registerRuntimeSystem(Ref<System> system) {
        if (!system) return;
        m_runtime_systems.push_back(std::move(system));
        m_task_graph_dirty = true;
    }

    void World::registerSimulationSystem(Ref<System> system) {
        if (!system) return;
        m_simulation_systems.push_back(std::move(system));
        m_task_graph_dirty = true;
    }

    void World::buildTaskGraphs() {
        BuildGraphForSystems(m_runtime_task_graph, m_runtime_systems);
        BuildGraphForSystems(m_simulation_task_graph, m_simulation_systems);
        m_task_graph_dirty = false;
    }

    void World::onRuntimeStart(Registry& reg) {
        for (auto& sys : m_runtime_systems) {
            if (sys) {
                sys->start(reg);
            }
        }
    }

    void World::onRuntimeUpdate(Registry& reg, const float dt) {
        if (m_task_graph_dirty) {
            buildTaskGraphs();
        }
        ExecuteSystemsParallel(m_runtime_task_graph, m_runtime_systems, reg, dt, m_command_buffer);
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
        if (m_task_graph_dirty) {
            buildTaskGraphs();
        }
        ExecuteSystemsParallel(m_simulation_task_graph, m_simulation_systems, reg, dt, m_command_buffer);
    }

    void World::onSimulationFinalize(Registry& reg) {
        for (auto& sys : m_simulation_systems) {
            if (sys) {
                sys->finalize(reg);
            }
        }
    }

    std::future<Scene*> World::loadSceneAsync(const std::string& name, LoadSceneMode mode) {
        auto promise = create_ref<std::promise<Scene*>>();
        auto future = promise->get_future();

        auto* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            DO_ERROR("loadSceneAsync: AssetManager not available");
            promise->set_value(nullptr);
            return future;
        }

        const std::string asset_url = (FsPath("Scenes") / (name + ".doscn")).generic_string();

        auto& scheduler = TaskScheduler::Self();
        scheduler.async([this, asset_manager, asset_url, name, mode, promise]() {
            auto data_future = asset_manager->loadAssetFileAsync<SceneRes>(asset_url);
            SceneRes scene_res;
            try {
                scene_res = data_future.get();
            }
            catch (const std::exception& e) {
                DO_ERROR("loadSceneAsync: file load failed '{}', error={}", name, e.what());
                enqueueAsyncCompletion([promise]() {
                    promise->set_value(nullptr);
                });
                return;
            }

            enqueueAsyncCompletion([this, scene_res = std::move(scene_res), name, mode, promise]() {
                Scene* scene = commitSceneAsync(scene_res, name, mode);
                promise->set_value(scene);
            });
        });

        return future;
    }

    void World::drainAsyncCompletions() {
        DynamicArray<std::function<void()>> pending;
        {
            std::lock_guard<std::mutex> lock(m_async_mutex);
            pending.swap(m_async_completions);
        }
        for (auto& fn : pending) {
            fn();
        }
    }

    void World::enqueueAsyncCompletion(std::function<void()> fn) {
        std::lock_guard<std::mutex> lock(m_async_mutex);
        m_async_completions.push_back(std::move(fn));
    }

    Scene* World::commitSceneAsync(const SceneRes& scene_res, const String& name, LoadSceneMode mode) {
        if (mode == LoadSceneMode::Single) {
            auto scenes_to_unload = m_active_scenes;
            for (auto* scene : scenes_to_unload) {
                deactivateScene(scene->getName());
            }
        }

        const String scene_name = scene_res.m_name.empty() ? name : scene_res.m_name;
        Scene* scene = createScene(scene_name);
        scene->deserialize(scene_res);
        activateScene(scene->getName());

        if (mode == LoadSceneMode::Single || m_current_scene == nullptr) {
            setActiveScene(scene);
        }

        return scene;
    }
} // dodoe
