//
// Created by GreenMuffin on 2026/2/6.
//

#pragma once

#include "dopch.h"

#include "registry.h"

#include "runtime/core/base.h"
#include "scene.h"
#include "systems/system.h"

namespace dodoe {

    struct WorldCreateInfo {
        std::string name;
    };

    enum class WorldState {
        Simulation,
        Runtime,
    };

    class World {
        friend class WorldManager;
        friend class Scene;

        std::string m_name;
        Uuid m_uuid{};

        WorldState m_state{WorldState::Runtime};

        Scene* m_current_scene{nullptr};
        std::vector<Scope<Scene>> m_scenes{};
        std::vector<Scene*> m_active_scenes{};

        std::vector<Ref<System>> m_runtime_systems{};
        std::vector<Ref<System>> m_simulation_systems{};
    public:
        static Scope<World> create(const WorldCreateInfo& create_info);
        static void destroy(Scope<World>& world);

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

        [[nodiscard]] Scene* getScene(const std::string& name) const;

        void setCurrentScene(Scene* current) { m_current_scene = current; }
        [[nodiscard]] Scene* getCurrentScene();

        void registerRuntimeSystem(Ref<System> system);
        void registerSimulationSystem(Ref<System> system);

    private:
        void initialize(const WorldCreateInfo& create_info);
        void shutdown();

        void onRuntimeStart(Registry& reg);
        void onRuntimeUpdate(Registry& reg, float dt);
        void onRuntimeFinalize(Registry& reg);

        void onSimulationStart(Registry& reg);
        void onSimulationUpdate(Registry& reg, float dt);
        void onSimulationFinalize(Registry& reg);
    };

} // dodoe
