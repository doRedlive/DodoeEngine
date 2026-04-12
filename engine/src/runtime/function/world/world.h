//
// Created by GreenMuffin on 2026/2/6.
//

#ifndef DODOE_WORLD_H
#define DODOE_WORLD_H

#include "dopch.h"

#include "registry.h"

#include "runtime/core/base.h"
#include "scene.h"
#include "systems/system.h"

namespace dodoe {

    struct WorldCreateInfo {
        std::string name;
    };

    class World {
        friend class WorldManager;
        friend class Scene;
    public:
        static Scope<World> create(const WorldCreateInfo& create_info);
        static void destroy(Scope<World>& world);

        void initialize(const WorldCreateInfo& create_info);
        void shutdown();
        [[nodiscard]]
        const std::string& get_name() const { return name_; }

        void runtime_start();
        void runtime_update(float delta_time);
        void runtime_finalize();
        void simulation_update(float delta_time);

        Scene* create_scene(const std::string& name);
        void load_scene(const std::string& name);
        void destroy_scene(const std::string& name);
        void destroy_all_scenes();
        [[nodiscard]] Scene* get_scene(const std::string& name) const;
        [[nodiscard]] Scene* active_scene() const;

        void register_system(Scope<System> system);

        void start_systems(Registry& reg);
        void update_systems(Registry& reg, float dt);
        void finalize_systems(Registry& reg);
    private:
        std::string name_;
        Uuid uuid_{};

        std::vector<Scope<Scene>> scenes_{};
        std::vector<Scope<System>> systems_{};
    };

} // dodoe

#endif//DODOE_WORLD_H
