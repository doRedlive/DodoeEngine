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
        String name;
    };

    class DODOE_API Scene : public Managed<Scene, SceneCreateInfo> {
        friend class Managed<Scene, SceneCreateInfo>;
        friend class World;
        friend class Entity;
        template<typename T>
        friend void OnComponentAdd_Help(Scene* scene, Entity entity, T& component);

        World& m_world;
        Registry m_reg;
        String m_name;
        std::unordered_map<UUID, Entity> m_entity_umap{};
    public: 

        Scene(const SceneCreateInfo& info);
        Scene(World& world, const String& name);
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
        [[nodiscard]] SceneRes serializeSubtree(Entity root) const;
        void deserialize(const SceneRes& scene_res);

        [[nodiscard]] const String& getName() const { return m_name; }
        void setName(const String& name) { m_name = name; }

        [[nodiscard]] Registry& registry() { return m_reg; }
        [[nodiscard]] const Registry& registry() const { return m_reg; }

        Entity createEntity(const String& name);
        Entity createEntity(UUID uuid, const String& name = String());
        void destroyEntity(Entity entity);
        void addEntity(Entity entity);
        [[nodiscard]] Entity getEntity(ui32 entity_id);
        [[nodiscard]] Entity getEntityByTag(const String& tag);
        [[nodiscard]] DynamicArray<Entity> getEntitiesByTag(const String& tag);
        [[nodiscard]] DynamicArray<Entity> getEntities();
        [[nodiscard]] Entity tryGetEntityByUUID(UUID uuid) const;
        [[nodiscard]] Entity getEntityByUUID(UUID uuid);

    private:
        template<typename T>
        void onComponentAdd(Entity entity, T& component) { }

        bool initialize(const SceneCreateInfo& info);
        void shutdown();
    };

} // dodoe

namespace dodoe {
    inline entt::entity GetEntityHandle_Help(const Entity& entity) { return entity.handle_; }
    inline Entity CreateEntityByScene_Help(Scene* scene, entt::entity handle) { return Entity(scene, handle); }
    inline Registry& GetSceneRegitry_Help(Scene* scene) { return scene->registry(); }

    template<typename T>
    void OnComponentAdd_Help(Scene* scene, Entity entity, T& component) {
        DO_ASSERT(scene);
        scene->onComponentAdd<T>(entity, component);
    }
} // dodoe
