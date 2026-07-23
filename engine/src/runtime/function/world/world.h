//
// Created by GreenMuffin on 2026/2/6.
//

#pragma once

#include "dopch.h"

#include "registry.h"

#include "runtime/core/base.h"
#include "runtime/core/async/task_graph.h"
#include "scene.h"
#include "systems/system.h"
#include "world_commands.h"

namespace dodoe {

    struct WorldCreateInfo {
        std::string name;
    };

    enum class WorldState {
        Simulation,
        Runtime,
        Pause,
    };

    enum class LoadSceneMode {
        Single,
        Additive,
    };

    class DODOE_API World : public Managed<World, WorldCreateInfo> {
        friend class Managed<World, WorldCreateInfo>;
        friend class WorldManager;
        friend class Scene;

        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = default;
        World& operator=(World&&) = default;

        String m_name;
        Uuid m_uuid{};

        WorldState m_state{WorldState::Runtime};

        Scene* m_current_scene{nullptr};
        DynamicArray<Scope<Scene>> m_scenes{};
        DynamicArray<Scene*> m_active_scenes{};

        DynamicArray<Ref<System>> m_runtime_systems{};
        DynamicArray<Ref<System>> m_simulation_systems{};

        std::mutex m_async_mutex{};
        DynamicArray<std::function<void()>> m_async_completions{};

        TaskGraph m_runtime_task_graph{};
        TaskGraph m_simulation_task_graph{};
        WorldCommands m_command_buffer{};
        Bool m_task_graph_dirty{true};

        static Bool s_force_sequential;

    public:
        World() = default;

        [[nodiscard]] const String& getName() const { return m_name; }

        void start();
        void update(float dt);
        void finalize();

        void switchState() { m_state = m_state == WorldState::Runtime ? WorldState::Simulation : WorldState::Runtime; }
        void setState(WorldState state) { m_state = state; }
        WorldState getState() const { return m_state; }

        Scene* createScene(const std::string& name);
        void deleteScene(const std::string& name);

        Scene* loadScene(const std::string& name, LoadSceneMode mode = LoadSceneMode::Single);
        std::future<Scene*> loadSceneAsync(const std::string& name, LoadSceneMode mode = LoadSceneMode::Single);
        void drainAsyncCompletions();
        void unloadScene(const std::string& name);

        void activateScene(const std::string& name);
        void deactivateScene(const std::string& name);
        bool activateStartScene();

        [[nodiscard]] Scene* getScene(const String& name) const;

        void setActiveScene(Scene* scene) { m_current_scene = scene; }
        [[nodiscard]] Scene* getActiveScene();

        void registerRuntimeSystem(Ref<System> system);
        void registerSimulationSystem(Ref<System> system);

        static void SetForceSequential(bool v) { s_force_sequential = v; }
        [[nodiscard]] static bool IsForceSequential() { return s_force_sequential; }

    private:
        bool initialize(const WorldCreateInfo& create_info);
        void shutdown();

        bool setupScenes();
        void cleanupScenes();

        bool setupSystems();
        void cleanupSystems();

        void buildTaskGraphs();

        Scene* commitSceneAsync(const SceneRes& scene_res, const String& name, LoadSceneMode mode);
        void enqueueAsyncCompletion(std::function<void()> fn);

        void onRuntimeStart(Registry& reg);
        void onRuntimeUpdate(Registry& reg, float dt);
        void onRuntimeFinalize(Registry& reg);

        void onSimulationStart(Registry& reg);
        void onSimulationUpdate(Registry& reg, float dt);
        void onSimulationFinalize(Registry& reg);
    };

} // dodoe
