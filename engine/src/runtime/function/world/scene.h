// do@Redlive
#pragma once

#include "dopch.h"

#include "registry.h"
#include "entity.h"

#include "runtime/core/utils/uuid.h"

namespace dodoe {
    class World;
    class SceneRes;

    struct SceneCreateInfo {
        World& world;
        std::string name;
    };

    class Scene : public Managed<Scene, SceneCreateInfo> {
        friend class Managed<Scene, SceneCreateInfo>;
        friend class World;
        friend class Entity;
        template<typename T>
        friend void OnComponentAdd_Help(Scene* scene, Entity entity, T& component);

        World& m_world;
        Registry m_reg;
        std::string m_name;
        std::unordered_map<Uuid, Entity> m_entity_umap{};
    public: 

        Scene(const SceneCreateInfo& info);
        Scene(World& world, const std::string& name);
        ~Scene() = default;
        Scene(const Scene&) = delete;
        Scene(Scene&&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene& operator=(Scene&&) = delete;

        void save();
        void onCreate();
        void onDelete();

        void onRuntimeStart();
        void onRuntimeStop();
        void onRuntimeUpdate(float delta_time);

        void onSimulationStart();
        void onSimulationStop();
        void onSimulationUpdate(float delta_time);
        [[nodiscard]] SceneRes serialize() const;
        void deserialize(const SceneRes& scene_res);

        [[nodiscard]] const std::string& getName() const { return m_name; }
        void setName(const std::string& name) { m_name = name; }

        [[nodiscard]] Registry& registry() { return m_reg; }
        [[nodiscard]] const Registry& registry() const { return m_reg; }

        Entity createEntity(const std::string& name);
        Entity createEntity(Uuid uuid, const std::string& name = std::string());
        void destroyEntity(Entity entity);
        void addEntity(Entity entity);
        [[nodiscard]] Entity getEntity(ui32 entity_id);
        [[nodiscard]] Entity getEntityByTag(const std::string& tag);
        [[nodiscard]] std::vector<Entity> getEntities();
        [[nodiscard]] Entity tryGetEntityByUUID(Uuid uuid) const;
        [[nodiscard]] Entity getEntityByUUID(Uuid uuid);

    private:
        template<typename T>
        void onComponentAdd(Entity entity, T& component) { }

        bool initialize(const SceneCreateInfo& info);
        void shutdown();
    };

} // dodoe

namespace dodoe {
    template<typename T>
    void OnComponentAdd_Help(Scene* scene, Entity entity, T& component) {
        DO_ASSERT(scene);
        scene->onComponentAdd<T>(entity, component);
    }
} // dodoe
