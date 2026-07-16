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

    class DODOE_API World : public Managed<World, WorldCreateInfo> {
        friend class Managed<World, WorldCreateInfo>;
        friend class WorldManager;
        friend class Scene;

        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = default;
        World& operator=(World&&) = default;

        std::string m_name;
        Uuid m_uuid{};

        WorldState m_state{WorldState::Runtime};

        Scene* m_current_scene{nullptr};
        std::vector<Scope<Scene>> m_scenes{};
        std::vector<Scene*> m_active_scenes{};

        std::vector<Ref<System>> m_runtime_systems{};
        std::vector<Ref<System>> m_simulation_systems{};

        TaskGraph m_runtime_task_graph{};
        TaskGraph m_simulation_task_graph{};
        WorldCommands m_command_buffer{};
        bool m_task_graph_dirty{true};

        static bool s_force_sequential;

    public:
        World() = default;

        [[nodiscard]] const std::string& getName() const { return m_name; }

        void start();
        void update(float dt);
        void finalize();

        void switchState() { m_state = m_state == WorldState::Runtime ? WorldState::Simulation : WorldState::Runtime; }
        void setState(WorldState state) { m_state = state; }
        WorldState getState() const { return m_state; }

        Scene* createScene(const std::string& name);
        void deleteScene(const std::string& name);

        void loadScene(const std::string& name);
        void unloadScene(const std::string& name);
        bool activateStartScene();

        [[nodiscard]] Scene* getScene(const std::string& name) const;

        void setCurrentScene(Scene* current) { m_current_scene = current; }
        [[nodiscard]] Scene* getCurrentScene();

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

        void onRuntimeStart(Registry& reg);
        void onRuntimeUpdate(Registry& reg, float dt);
        void onRuntimeFinalize(Registry& reg);

        void onSimulationStart(Registry& reg);
        void onSimulationUpdate(Registry& reg, float dt);
        void onSimulationFinalize(Registry& reg);
    };

} // dodoe
