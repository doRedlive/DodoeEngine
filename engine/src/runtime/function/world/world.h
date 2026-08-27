//
// Created by GreenMuffin on 2026/2/6.
//

#pragma once

#include "dopch.h"

#include <future>
#include <typeindex>

#include "registry.h"

#include "runtime/core/base.h"
#include "runtime/core/async/task_graph.h"
#include "scene.h"
#include "systems/system.h"
#include "world_commands.h"

namespace dodoe {

    struct WorldCreateInfo {
        String name;
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
        UUID m_uuid{};

        WorldState m_state{WorldState::Runtime};

        Scene* m_current_scene{nullptr};
        DynamicArray<Scope<Scene>> m_scenes{};
        DynamicArray<Scene*> m_active_scenes{};

        DynamicArray<Ref<System>> m_runtime_systems{};
        DynamicArray<Ref<System>> m_simulation_systems{};
        DynamicArray<Ref<System>> m_gameplay_systems{};

        using SystemIndex = UnorderedMap<std::type_index, System*>;
        SystemIndex m_runtime_system_index{};
        SystemIndex m_simulation_system_index{};

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

        void switchState();
        void setState(WorldState state);
        WorldState getState() const { return m_state; }

        Scene* createScene(const String& name);
        void deleteScene(const String& name);

        Scene* loadScene(const String& name, LoadSceneMode mode = LoadSceneMode::Single);
        std::future<Scene*> loadSceneAsync(const String& name, LoadSceneMode mode = LoadSceneMode::Single);
        void drainAsyncCompletions();
        void unloadScene(const String& name);

        void activateScene(const String& name);
        void deactivateScene(const String& name);

        [[nodiscard]] Scene* getScene(const String& name) const;

        void setActiveScene(Scene* scene) { m_current_scene = scene; }
        [[nodiscard]] Scene* getActiveScene();

        void registerRuntimeSystem(Ref<System> system);
        void registerSimulationSystem(Ref<System> system);
        void registerGameplaySystem(Ref<System> system);

        template<typename T>
        [[nodiscard]] T* findRuntimeSystem() {
            return findSystemIn<T>(m_runtime_system_index);
        }
        template<typename T>
        [[nodiscard]] const T* findRuntimeSystem() const {
            return findSystemIn<const T>(m_runtime_system_index);
        }

        template<typename T>
        [[nodiscard]] T* findSimulationSystem() {
            return findSystemIn<T>(m_simulation_system_index);
        }
        template<typename T>
        [[nodiscard]] const T* findSimulationSystem() const {
            return findSystemIn<const T>(m_simulation_system_index);
        }

        [[nodiscard]] WorldCommands& getCommandBuffer() { return m_command_buffer; }
        void flushCommandBuffer();

        static void SetForceSequential(bool v) { s_force_sequential = v; }
        [[nodiscard]] static bool IsForceSequential() { return s_force_sequential; }

    private:
        template<typename T>
        [[nodiscard]] T* findSystemIn(const SystemIndex& index) const {
            const auto it = index.find(typeid(T));
            return it == index.end() ? nullptr : static_cast<T*>(it->second);
        }

        void enterState();
        void leaveState();
        void notifyFixedUpdate();
        void syncFixedUpdateCallback();

        bool initialize(const WorldCreateInfo& create_info);
        void shutdown();

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
