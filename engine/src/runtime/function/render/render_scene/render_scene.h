#pragma once

#include "dopch.h"

#include "../framework/primitive_scene_info.h"
#include "../framework/sprite_scene_info.h"
#include "primitive_render_object.h"
#include "light_render_object.h"
#include "sprite_render_object.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct RenderSceneCreateInfo {};

    struct RenderDirectionalLight {
        Vector3f direction{0.3f, -0.8f, -0.5f};
        Vector3f color{1.0f};
        Float irradiance{1.0f};
    };

    struct RenderPointLight {
        Vector3f position{0.0f};
        Float radius{1.0f};
        Vector3f color{1.0f};
        Float intensity{1.0f};
        Float range{10.0f};
    };

    struct RenderSceneCamera {
        Matrix4f view_projection{1.0f};
        Vector3f position{0.0f};
        Bool valid{false};
    };

    enum class PrimitiveUpdateType : UInt32 {
        None = 0,
        Added = 1 << 0,
        Removed = 1 << 1,
        TransformChanged = 1 << 2,
        MaterialChanged = 1 << 3,
        StateChanged = 1 << 4,
        MeshChanged = 1 << 5,
        ProxyChanged = 1 << 6
    };

    inline PrimitiveUpdateType operator|(const PrimitiveUpdateType lhs, const PrimitiveUpdateType rhs) {
        return static_cast<PrimitiveUpdateType>(static_cast<UInt32>(lhs) | static_cast<UInt32>(rhs));
    }

    inline PrimitiveUpdateType& operator|=(PrimitiveUpdateType& lhs, const PrimitiveUpdateType rhs) {
        lhs = lhs | rhs;
        return lhs;
    }

    inline Bool HasAnyFlags(const PrimitiveUpdateType lhs, const PrimitiveUpdateType rhs) {
        return (static_cast<UInt32>(lhs) & static_cast<UInt32>(rhs)) != 0;
    }

    enum class LightUpdateType : UInt32 {
        None = 0,
        Added = 1 << 0,
        Removed = 1 << 1,
        TransformChanged = 1 << 2,
        StateChanged = 1 << 3
    };

    inline LightUpdateType operator|(const LightUpdateType lhs, const LightUpdateType rhs) {
        return static_cast<LightUpdateType>(static_cast<UInt32>(lhs) | static_cast<UInt32>(rhs));
    }

    inline LightUpdateType& operator|=(LightUpdateType& lhs, const LightUpdateType rhs) {
        lhs = lhs | rhs;
        return lhs;
    }

    inline Bool HasAnyFlags(const LightUpdateType lhs, const LightUpdateType rhs) {
        return (static_cast<UInt32>(lhs) & static_cast<UInt32>(rhs)) != 0;
    }

    enum class SpriteUpdateType : UInt32 {
        None = 0,
        Added = 1 << 0,
        Removed = 1 << 1,
        TransformChanged = 1 << 2,
        TextureChanged = 1 << 3,
        MaterialChanged = 1 << 4,
        StateChanged = 1 << 5
    };

    inline SpriteUpdateType operator|(const SpriteUpdateType lhs, const SpriteUpdateType rhs) {
        return static_cast<SpriteUpdateType>(static_cast<UInt32>(lhs) | static_cast<UInt32>(rhs));
    }

    inline SpriteUpdateType& operator|=(SpriteUpdateType& lhs, const SpriteUpdateType rhs) {
        lhs = lhs | rhs;
        return lhs;
    }

    inline Bool HasAnyFlags(const SpriteUpdateType lhs, const SpriteUpdateType rhs) {
        return (static_cast<UInt32>(lhs) & static_cast<UInt32>(rhs)) != 0;
    }

    class RenderScene : public Managed<RenderScene, RenderSceneCreateInfo> {
        friend class Managed<RenderScene, RenderSceneCreateInfo>;
        struct Aabb {
            Vector3f min{0.0f};
            Vector3f max{0.0f};
        };

        RenderSceneCamera m_main_camera{};
        UnorderedMap<Size_t, Aabb> m_mesh_bounds_cache{};
        UnorderedMap<UUID, Scope<PrimitiveRenderObject>> m_primitive_objects{};
        UnorderedMap<UUID, Scope<LightRenderObject>> m_light_objects{};
        UnorderedMap<UUID, Scope<SpriteRenderObject>> m_sprite_objects{};
        Bool m_scene_data_dirty{true};
        UnorderedMap<UUID, Size_t> m_primitive_scene_info_indices{};
        UnorderedMap<UUID, Size_t> m_point_light_indices{};
        UnorderedMap<UUID, PrimitiveUpdateType> m_pending_primitive_updates{};
        UnorderedMap<UUID, LightUpdateType> m_pending_light_updates{};
        UnorderedMap<UUID, SpriteUpdateType> m_pending_sprite_updates{};
        DynamicArray<PrimitiveSceneInfo> m_primitive_scene_infos{};
        DynamicArray<RenderDirectionalLight> m_directional_lights{};
        DynamicArray<RenderPointLight> m_point_lights{};
        SpriteSceneInfo m_sprite_scene_info{};

    public:
        RenderScene() = default;
        ~RenderScene() = default;

        void setMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position);
        void addPrimitive(Scope<PrimitiveRenderObject> primitive);
        void updatePrimitiveTransform(UUID id, const Matrix4f& world_transform);
        void removePrimitive(UUID id);
        void addLight(Scope<LightRenderObject> light);
        void updateLightTransform(UUID id, const Matrix4f& world_transform);
        void removeLight(UUID id);
        void addSprite(Scope<SpriteRenderObject> sprite);
        void updateSpriteTransform(UUID id, const Matrix4f& world_transform);
        void removeSprite(UUID id);
        void flushUpdates();

        [[nodiscard]] const RenderSceneCamera& mainCamera() const { return m_main_camera; }
        [[nodiscard]] const DynamicArray<PrimitiveSceneInfo>& getPrimitiveSceneInfos() const { return m_primitive_scene_infos; }
        [[nodiscard]] const DynamicArray<RenderDirectionalLight>& getDirectionalLights() const { return m_directional_lights; }
        [[nodiscard]] const DynamicArray<RenderPointLight>& getPointLights() const { return m_point_lights; }
        [[nodiscard]] const SpriteSceneInfo& getSpriteSceneInfo() const { return m_sprite_scene_info; }
        [[nodiscard]] Bool hasPrimitive(UUID id) const { return m_primitive_objects.find(id) != m_primitive_objects.end(); }
        [[nodiscard]] Bool hasLight(UUID id) const { return m_light_objects.find(id) != m_light_objects.end(); }
        [[nodiscard]] Bool hasSprite(UUID id) const { return m_sprite_objects.find(id) != m_sprite_objects.end(); }
        [[nodiscard]] const PrimitiveRenderObject* findPrimitive(UUID id) const;
        [[nodiscard]] const LightRenderObject* findLight(UUID id) const;
        [[nodiscard]] const SpriteRenderObject* findSprite(UUID id) const;
        [[nodiscard]] const SkyLightRenderObject* findSkyLight() const;
        [[nodiscard]] const Aabb& getMeshBounds(const MeshUploadData& upload_data);

    private:
        Bool initialize(const RenderSceneCreateInfo& info);
        void shutdown();
        void reset();
        void rebuildPipelineSceneData();
        void markPrimitiveDirty(UUID id, PrimitiveUpdateType update_type);
        void markLightDirty(UUID id, LightUpdateType update_type);
        void markSpriteDirty(UUID id, SpriteUpdateType update_type);
        void rebuildSpriteSceneData();
        void upsertSpriteInstance(UUID id);
        void removeSpriteInstance(UUID id);
        PrimitiveSceneInfo* findPrimitiveSceneInfo(UUID id);
        const PrimitiveSceneInfo* findPrimitiveSceneInfo(UUID id) const;
        void upsertPrimitiveSceneInfo(UUID id);
        void applyPrimitiveTransform(UUID id);
        void updatePrimitiveMaterials(UUID id);
        void updatePrimitiveState(UUID id);
        void removePrimitiveSceneInfo(UUID id);
        void upsertPointLightInfo(UUID id);
        void removePointLightInfo(UUID id);
    };

} // dodoe
