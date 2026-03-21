//
// Created by GreenMuffin on 2025/11/15.
//

#ifndef DODOE_SCENE_H
#define DODOE_SCENE_H
#include "dopch.h"

#include "registry.h"

#include "runtime/core/utils/uuid.h"

#include "entt/entt.hpp"

namespace dodoe {
    class Entity;
    class RenderSystem;
    class SceneManager;

    class Scene {
        friend class World;
        friend class Entity;
    public:
        Scene(World& world, const std::string& name);
        ~Scene();

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) = delete;
        Scene& operator=(Scene&&) = delete;

        void on_runtime_start();
        void on_runtime_stop();
        void on_runtime_update(float delta_time);

        void on_simulation_start();
        void on_simulation_stop();
        void on_simulation_update(float delta_time);

        void destroy();

        [[nodiscard]]
        const std::string& get_name() const { return name_; }
        void set_name(const std::string& name) { name_ = name; }
        Registry& registry() { return registry_; }
        const Registry& registry() const { return registry_; }

        Entity create_entity(const std::string& name);
        Entity create_entity(Uuid uuid, const std::string& name = std::string());
        void add_entity(Entity entity);
        void destroy_entity(Entity entity);

    private:
        World& world_;
        std::string name_;

        Registry reg_;
        Registry& registry_;
        std::unordered_map<Uuid, entt::entity> entity_umap_;

        template<typename T>
        void on_component_add_(Entity entity, T& component) { }
    };
} // dodoe


#endif //DODOE_SCENE_H
