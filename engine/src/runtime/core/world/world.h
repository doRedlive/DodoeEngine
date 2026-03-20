//
// Created by GreenMuffin on 2026/2/6.
//

#ifndef DODOE_WORLD_H
#define DODOE_WORLD_H

#include "dopch.h"

#include "registry.h"

#include "runtime/core/base.h"
#include "runtime/core/world/scene.h"
#include "runtime/function/render/render_system.h"

namespace dodoe {

    using StartSystem  = std::function<void(Registry& reg)>;
    using UpdateSystem = std::function<void(Registry& reg, float dt)>;

    struct WorldProperty {
        float size{ 10.0f };
    };

    class World {
        friend class WorldManager;
    public:
        static WorldProperty property;

        World(const std::string& name);
        ~World() = default;

        void initialize();
        void shutdown();

        Scene* create_scene(const std::string& name);
        [[nodiscard]]
        Scene* get_scene(const std::string& name) const;
        [[nodiscard]]
        Scene* active_scene() const;
        void load_scene(const std::string& name);
        void destroy_scene(const std::string& name);
        void destroy_all_scenes();

        int add_start_system(StartSystem start);
        int add_update_system(UpdateSystem update);

        const std::vector<StartSystem>&  load_start_systems();
        const std::vector<UpdateSystem>& load_update_systems();

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