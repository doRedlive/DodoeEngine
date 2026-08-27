// do@Redlive

#include "world.h"

#include "scene.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/project/project.h"
#include "runtime/resource/res_type/scene_res.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/core/meta/serializer/serializer.h"
#include "runtime/core/async/task_scheduler.h"

#include "systems/animator_system.h"
#include "systems/audio_play_system.h"
#include "systems/camera_system.h"
#include "systems/foliage_renderer_system.h"
#include "systems/light_system.h"
#include "systems/line_renderer_system.h"
#include "systems/sky_light_system.h"
#include "systems/mesh_renderer_system.h"
#include "systems/physics2d_system.h"
#include "systems/physics3d_system.h"
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
            const DynamicArray<Ref<System>>& systems)
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
                    if (prod_it != producer.end() && prod_it->second >= 0 && static_cast<Size_t>(prod_it->second) != i) {
                        graph.addEdge(
                            static_cast<TaskGraph::NodeId>(prod_it->second),
                            static_cast<TaskGraph::NodeId>(i));
                    }
                    readers[type_hash].push_back(i);
                }
            }

            graph.compile();
        }

        void WarmupComponentsPools(Registry& reg) {
            reg.ensurePoolExists<CameraComponent>();
            reg.ensurePoolExists<CircleRendererComponent>();
            reg.ensurePoolExists<AnimationPoseComponent>();
            reg.ensurePoolExists<AnimationDriveModeComponent>();
            reg.ensurePoolExists<AudioSourceComponent>();
            reg.ensurePoolExists<AudioListenerComponent>();
            reg.ensurePoolExists<BoneAttachmentComponent>();
            reg.ensurePoolExists<BoxCollider2dComponent>();
            reg.ensurePoolExists<CircleCollider2dComponent>();
            reg.ensurePoolExists<BoxColliderComponent>();
            reg.ensurePoolExists<SphereColliderComponent>();
            reg.ensurePoolExists<CapsuleColliderComponent>();
            reg.ensurePoolExists<FoliageRendererComponent>();
            reg.ensurePoolExists<IDComponent>();
            reg.ensurePoolExists<MeshRendererComponent>();
            reg.ensurePoolExists<RectRendererComponent>();
            reg.ensurePoolExists<Rigidbody2dComponent>();
            reg.ensurePoolExists<RigidbodyComponent>();
            reg.ensurePoolExists<PointLightComponent>();
            reg.ensurePoolExists<SpotLightComponent>();
            reg.ensurePoolExists<LineRendererComponent>();
            reg.ensurePoolExists<SkyLightComponent>();
            reg.ensurePoolExists<SpriteRendererComponent>();
            reg.ensurePoolExists<TagComponent>();
            reg.ensurePoolExists<TransformComponent>();
            reg.ensurePoolExists<HierarchyComponent>();
            reg.ensurePoolExists<TilemapComponent>();
            reg.ensurePoolExists<TileLayerComponent>();
            reg.ensurePoolExists<SetVelocity2dRequest>();
            reg.ensurePoolExists<ApplyForce2dRequest>();
            reg.ensurePoolExists<ApplyImpulse2dRequest>();
            reg.ensurePoolExists<SetVelocityRequest>();
            reg.ensurePoolExists<ApplyForceRequest>();
            reg.ensurePoolExists<ApplyImpulseRequest>();
            reg.ensurePoolExists<TeleportRequest>();
            reg.ensurePoolExists<PlayAnimationRequest>();
            reg.ensurePoolExists<StopAnimationRequest>();
            reg.ensurePoolExists<ResumeAnimationRequest>();
        }

        CommandContext MakeCommandContext(Registry& reg) {
            CommandContext context;
            context.scene = reg.scene_context;
            context.registry = &reg;
            context.add_managed = [](uint64_t uuid, const String& type_name) {
                ScriptSystem* script_system = GetScriptSystem();
                if (script_system && script_system->getScriptRuntime()) {
                    script_system->getScriptRuntime()->addEntityManagedComponentFromManaged(uuid, type_name);
                }
            };
            context.remove_managed = [](uint64_t uuid, const String& type_name) {
                ScriptSystem* script_system = GetScriptSystem();
                if (script_system && script_system->getScriptRuntime()) {
                    script_system->getScriptRuntime()->removeEntityManagedComponentFromManaged(uuid, type_name);
                }
            };
            return context;
        }

        void ExecuteSystemsParallel(
            const TaskGraph& graph,
            const DynamicArray<Ref<System>>& systems,
            Registry& reg,
            float dt,
            WorldCommands& cmd_buf)
        {
            WarmupComponentsPools(reg);

            if (World::IsForceSequential()) {
                for (auto& sys : systems) {
                    if (sys) {
                        sys->update(reg, dt);
                    }
                }
                cmd_buf.apply(MakeCommandContext(reg));
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

            cmd_buf.apply(MakeCommandContext(reg));
            cmd_buf.reset();
        }

    } // anonymous namespace

    Bool World::initialize(const WorldCreateInfo& create_info) {
        DO_PROFILE_SCOPE_CATEGORY("World::initialize", "startup");
        m_name = create_info.name;
        const auto app_mode = Application::Self().getAppMode();
        if (app_mode != AppMode::Game && app_mode != AppMode::Server) {
            m_state = WorldState::Simulation;
        }
        if (!setupSystems()) return false;
        return true;
    }

    void World::shutdown() {
        DO_PROFILE_SCOPE_CATEGORY("World::shutdown", "shutdown");
        cleanupScenes();
        cleanupSystems();
        for (auto& scene : m_scenes) {
            Scene::Destroy(scene);
        }
    }

    void World::cleanupScenes() {
        m_active_scenes.clear();
    }

    bool World::setupSystems() {
        DO_PROFILE_SCOPE_CATEGORY("World::setupSystems", "startup");
        const bool is_2d_only = Application::Self().getEngineMode() == EngineMode::TwoD;

        Ref<System> mono = create_ref<MonoSystem>();
        Ref<System> animator = create_ref<AnimatorSystem>();
        Ref<System> camera_system = create_ref<CameraSystem>();
        Ref<System> light_system = create_ref<LightSystem>();
        Ref<System> sky_light = is_2d_only ? nullptr : create_ref<SkyLightSystem>();
        Ref<System> physics2d = create_ref<Physics2dSystem>();
        Ref<System> physics3d = is_2d_only ? nullptr : create_ref<Physics3dSystem>();
        Ref<System> audio_play = create_ref<AudioPlaySystem>();
        Ref<System> foliage_renderer = is_2d_only ? nullptr : create_ref<FoliageRendererSystem>();
        Ref<System> mesh_system = is_2d_only ? nullptr : create_ref<MeshRendererSystem>();
        Ref<System> sprite_renderer = create_ref<SpriteRendererSystem>();
        Ref<System> tilemap_renderer = create_ref<TilemapRendererSystem>();
        Ref<System> rect_renderer = create_ref<RectRendererSystem>();
        Ref<System> line_renderer = create_ref<LineRendererSystem>();

        registerGameplaySystem(mono);
        registerRuntimeSystem(animator);
        registerRuntimeSystem(camera_system);
        registerRuntimeSystem(light_system);
        registerRuntimeSystem(sky_light);
        registerRuntimeSystem(physics2d);
        registerRuntimeSystem(physics3d);
        registerRuntimeSystem(audio_play);
        registerRuntimeSystem(foliage_renderer);
        registerRuntimeSystem(mesh_system);
        registerRuntimeSystem(sprite_renderer);
        registerRuntimeSystem(tilemap_renderer);
        registerRuntimeSystem(rect_renderer);
        registerRuntimeSystem(line_renderer);

        registerSimulationSystem(animator);
        registerSimulationSystem(camera_system);
        registerSimulationSystem(light_system);
        registerSimulationSystem(physics2d);
        registerSimulationSystem(physics3d);
        registerSimulationSystem(audio_play);
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
        m_runtime_system_index.clear();
        m_simulation_system_index.clear();
        m_task_graph_dirty = true;
    }

    void World::start() {
        DO_PROFILE_SCOPE_CATEGORY("World::start", "startup");
        enterState();
        syncFixedUpdateCallback();
    }

    void World::setState(WorldState state) {
        if (m_state == state) {
            return;
        }
        const bool transition = m_state != WorldState::Pause && state != WorldState::Pause;
        if (transition) {
            leaveState();
            m_state = state;
            enterState();
        } else {
            m_state = state;
        }
        syncFixedUpdateCallback();
    }

    void World::syncFixedUpdateCallback() {
        PhysicsSystem* physics_system = GetPhysicsSystem();
        if (!physics_system) return;
        if (m_state == WorldState::Runtime) {
            physics_system->setFixedUpdateCallback([this]() { notifyFixedUpdate(); });
        } else {
            physics_system->setFixedUpdateCallback(nullptr);
        }
    }

    void World::switchState() {
        setState(m_state == WorldState::Runtime ? WorldState::Simulation : WorldState::Runtime);
    }

    void World::enterState() {
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

    void World::leaveState() {
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

    void World::update(const float dt) {
        DO_PROFILE_SCOPE_CATEGORY("World::update", "frame");
        drainAsyncCompletions();

        if (m_state != WorldState::Pause && dt > 0.0f) {
            auto* physics_system = GetPhysicsSystem();
            if (physics_system) {
                physics_system->step(dt);
            }
        }

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
        leaveState();
    }

    void World::notifyFixedUpdate() {
        DO_PROFILE_SCOPE_CATEGORY("World::notifyFixedUpdate", "frame");
        ScriptSystem* script_system = GetScriptSystem();
        if (!script_system) return;
        ScriptRuntime* runtime = script_system->getScriptRuntime();
        if (runtime) {
            runtime->onRuntimeFixedUpdate();
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
        DO_PROFILE_SCOPE_CATEGORY("World::loadScene", "startup");
        auto* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            DO_ERROR("loadScene: AssetManager not available");
            return nullptr;
        }

        const String asset_url((FsPath("Scenes") / (name + ".doscn")).generic_string().c_str());
        DO_PROFILE_MARK("World::loadScene.resolveAsset", "startup");
        auto handle = asset_manager->getHandleByPath<SceneAsset>(asset_url);
        SceneRes scene_res;
        if (handle.isValid()) {
            SceneAsset* scene_asset = asset_manager->loadAssetSync<SceneAsset>(handle.getObjectID().asset_id);
            if (!scene_asset) {
                DO_ERROR("loadScene: failed to load '{}'", asset_url);
                return nullptr;
            }
            scene_res = scene_asset->getSceneRes();
        } else if (!asset_manager->loadAssetFile(asset_url, scene_res)) {
            DO_ERROR("loadScene: failed to load '{}'", asset_url);
            return nullptr;
        }
        const String scene_name = scene_res.m_name.empty() ? name : scene_res.m_name;
        Scene* scene = getScene(scene_name);
        if (!scene) {
            scene = createScene(scene_name);
            scene->deserialize(scene_res);
            DO_PROFILE_MARK("World::loadScene.deserialize", "startup");
        }

        const Bool is_single = mode == LoadSceneMode::Single;
        if (is_single) {
            const DynamicArray<Scene*> scenes_to_deactivate = m_active_scenes;
            for (Scene* active_scene : scenes_to_deactivate) {
                deactivateScene(active_scene->getName());
            }
        }

        const auto active_it = std::find(m_active_scenes.begin(), m_active_scenes.end(), scene);
        if (active_it == m_active_scenes.end()) {
            activateScene(scene->getName());
            DO_PROFILE_MARK("World::loadScene.activate", "startup");
        }

        if (is_single || m_current_scene == nullptr) {
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

    void World::registerRuntimeSystem(Ref<System> system) {
        if (!system) return;
        m_runtime_system_index[typeid(*system)] = system.get();
        m_runtime_systems.push_back(std::move(system));
        m_task_graph_dirty = true;
    }

    void World::registerSimulationSystem(Ref<System> system) {
        if (!system) return;
        m_simulation_system_index[typeid(*system)] = system.get();
        m_simulation_systems.push_back(std::move(system));
        m_task_graph_dirty = true;
    }

    void World::registerGameplaySystem(Ref<System> system) {
        if (!system) return;
        m_gameplay_systems.push_back(std::move(system));
    }

    void World::buildTaskGraphs() {
        BuildGraphForSystems(m_runtime_task_graph, m_runtime_systems);
        BuildGraphForSystems(m_simulation_task_graph, m_simulation_systems);
        m_task_graph_dirty = false;
    }

    void World::onRuntimeStart(Registry& reg) {
        DO_PROFILE_SCOPE_CATEGORY("World::onRuntimeStart", "startup");
        for (auto& sys : m_gameplay_systems) {
            if (sys) {
                sys->start(reg);
            }
        }
        for (auto& sys : m_runtime_systems) {
            if (sys) {
                sys->start(reg);
            }
        }
    }

    void World::onRuntimeUpdate(Registry& reg, const float dt) {
        DO_PROFILE_SCOPE_CATEGORY("World::onRuntimeUpdate", "frame");
        for (auto& sys : m_gameplay_systems) {
            if (sys) {
                sys->update(reg, dt);
            }
        }

        m_command_buffer.apply(MakeCommandContext(reg));
        m_command_buffer.reset();

        if (m_task_graph_dirty) {
            buildTaskGraphs();
        }
        ExecuteSystemsParallel(m_runtime_task_graph, m_runtime_systems, reg, dt, m_command_buffer);
    }

    void World::flushCommandBuffer() {
        Scene* scene = getActiveScene();
        if (!scene) return;
        m_command_buffer.apply(MakeCommandContext(scene->registry()));
        m_command_buffer.reset();
    }

    void World::onRuntimeFinalize(Registry& reg) {
        DO_PROFILE_SCOPE_CATEGORY("World::onRuntimeFinalize", "shutdown");
        for (auto& sys : m_gameplay_systems) {
            if (sys) {
                sys->finalize(reg);
            }
        }
        for (auto& sys : m_runtime_systems) {
            if (sys) {
                sys->finalize(reg);
            }
        }
    }

    void World::onSimulationStart(Registry& reg) {
        DO_PROFILE_SCOPE_CATEGORY("World::onSimulationStart", "startup");
        for (auto& sys : m_simulation_systems) {
            if (sys) {
                sys->start(reg);
            }
        }
    }

    void World::onSimulationUpdate(Registry& reg, const float dt) {
        DO_PROFILE_SCOPE_CATEGORY("World::onSimulationUpdate", "frame");
        if (m_task_graph_dirty) {
            buildTaskGraphs();
        }
        ExecuteSystemsParallel(m_simulation_task_graph, m_simulation_systems, reg, dt, m_command_buffer);
    }

    void World::onSimulationFinalize(Registry& reg) {
        DO_PROFILE_SCOPE_CATEGORY("World::onSimulationFinalize", "shutdown");
        for (auto& sys : m_simulation_systems) {
            if (sys) {
                sys->finalize(reg);
            }
        }
    }

    std::future<Scene*> World::loadSceneAsync(const String& name, LoadSceneMode mode) {
        DO_PROFILE_SCOPE_CATEGORY("World::loadSceneAsync", "startup");
        auto promise = create_ref<std::promise<Scene*>>();
        auto future = promise->get_future();

        auto* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            DO_ERROR("loadSceneAsync: AssetManager not available");
            promise->set_value(nullptr);
            return future;
        }

        const String asset_url((FsPath("Scenes") / (name + ".doscn")).generic_string().c_str());

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
        DO_PROFILE_SCOPE_CATEGORY("World::drainAsyncCompletions", "frame");
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
        DO_PROFILE_SCOPE_CATEGORY("World::commitSceneAsync", "startup");
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
