//
// Created by GreenMuffin on 2026/2/6.
//

#ifndef DODOE_WORLD_H
#define DODOE_WORLD_H

#include "dopch.h"

#include "registry.h"
#include "world_context.h"

#include "runtime/core/base.h"
#include "runtime/core/world/scene.h"
#include "runtime/function/render/render_system.h"

namespace dodoe {

    using StartSystem  = std::function<void(Registry& reg)>;
    using UpdateSystem = std::function<void(Registry& reg, float dt)>;

    struct WorldProperty {
        float size{ 10.0f };
    };

    struct WorldCreateInfo {
        std::string_view name;
        WorldContext& context;
    };

    class World {
        friend class WorldManager;
        friend class Scene;
    public:
        static WorldProperty property;
        WorldContext& context;

        World(const std::string& in_name, WorldContext& in_context);
        ~World() = default;

        static Scope<World> create(WorldCreateInfo create_info);
        static void destroy(Scope<World>& world);

        void initialize();
        void shutdown();

        void runtime_update(float delta_time);
        void simulation_update(float delta_time);

        Scene* create_scene(const std::string& name);
        void load_scene(const std::string& name);
        void destroy_scene(const std::string& name);
        void destroy_all_scenes();
        [[nodiscard]] Scene* get_scene(const std::string& name) const;
        [[nodiscard]] Scene* active_scene() const;

        int add_start_system(StartSystem start);
        int add_update_system(UpdateSystem update);

        [[nodiscard]] const std::vector<StartSystem>&  load_start_systems();
        [[nodiscard]] const std::vector<UpdateSystem>& load_update_systems();

        void remove_start_system(int id);
        void remove_update_system(int id);
        void remove_all_systems();

    private:
        std::string name_;
        Uuid uuid_{};

        std::vector<Scope<Scene>> scenes_{};
        std::vector<StartSystem> start_systems_{};
        std::vector<UpdateSystem> update_systems_{};
    };

} // dodoe

#endif//DODOE_WORLD_H
