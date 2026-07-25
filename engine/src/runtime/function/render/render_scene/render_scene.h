// do@Redlive

#pragma once

#include "dopch.h"

#include "primitive_render_object.h"
#include "sprite_render_object.h"

#include "primitive_scene_info.h"
#include "render_scene_delta.h"
#include "sprite_scene_info.h"
#include "light_scene_info.h"
#include "runtime/function/render/gpu_driven/gpu_scene.h"

#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct RenderSceneCreateInfo {
        class SharedRenderService* shared_render_service{nullptr};
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

    enum class LightUpdateType : UInt32 {
        None = 0,
        Added = 1 << 0,
        Removed = 1 << 1,
        TransformChanged = 1 << 2,
        DataChanged = 1 << 3
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

    class RenderScene : public Managed<RenderScene, RenderSceneCreateInfo> {
        friend class Managed<RenderScene, RenderSceneCreateInfo>;
        struct Aabb {
            Vector3f min{0.0f};
            Vector3f max{0.0f};
        };

        UnorderedMap<Size_t, Aabb> m_mesh_bounds_cache{};
        UnorderedMap<UUID, Scope<PrimitiveRenderObject>> m_primitive_objects{};
        UnorderedMap<UUID, Scope<SpriteRenderObject>> m_sprite_objects{};
        UnorderedMap<UUID, GpuObjectHandle> m_cpu_to_gpu_map{};
        Bool m_scene_data_dirty{true};

        UnorderedMap<UUID, Size_t> m_primitive_scene_info_indices{};
        UnorderedMap<UUID, Size_t> m_light_scene_info_indices{};
        UnorderedMap<UUID, Size_t> m_sprite_scene_info_indices{};
        UnorderedMap<UUID, PrimitiveUpdateType> m_pending_primitive_updates{};
        UnorderedMap<UUID, SpriteUpdateType> m_pending_sprite_updates{};
        UnorderedMap<UUID, LightUpdateType> m_pending_light_updates{};

        DynamicArray<PrimitiveSceneInfo> m_primitive_scene_infos{};
        DynamicArray<SpriteSceneInfo> m_sprite_scene_infos{};
        DynamicArray<LightSceneInfo> m_light_scene_infos{};

        Scope<GpuScene> m_gpu_scene{};
        class SharedRenderService* m_shared_render_service{nullptr};

    public:
        void addPrimitive(Scope<PrimitiveRenderObject> primitive);
        void updatePrimitiveTransform(UUID id, const Matrix4f& world_transform);
        void removePrimitive(UUID id);

        void addLightSceneInfo(LightSceneInfo&& info);
        void updateLightSceneInfoTransform(UUID id, const Matrix4f& world_transform);
        void removeLightSceneInfo(UUID id);

        void addSprite(Scope<SpriteRenderObject> sprite);
        void updateSpriteTransform(UUID id, const Matrix4f& world_transform);
        void removeSprite(UUID id);

        void flushUpdates(DrawCommandList& cmd_list);

        [[nodiscard]] const Aabb& getMeshBounds(const MeshUploadData& upload_data);

        [[nodiscard]] const DynamicArray<PrimitiveSceneInfo>& getPrimitiveSceneInfos() const { return m_primitive_scene_infos; }
        [[nodiscard]] const DynamicArray<SpriteSceneInfo>& getSpriteSceneInfos() const { return m_sprite_scene_infos; }
        [[nodiscard]] const DynamicArray<LightSceneInfo>& getLightSceneInfos() const { return m_light_scene_infos; }
        [[nodiscard]] const LightSceneInfo* findLightSceneInfo(UUID id) const;

        [[nodiscard]] Bool hasPrimitive(UUID id) const { return m_primitive_objects.find(id) != m_primitive_objects.end(); }
        [[nodiscard]] Bool hasLight(UUID id) const { return m_light_scene_info_indices.find(id) != m_light_scene_info_indices.end(); }
        [[nodiscard]] Bool hasSprite(UUID id) const { return m_sprite_objects.find(id) != m_sprite_objects.end(); }

        [[nodiscard]] PrimitiveRenderObject* findPrimitive(UUID id);
        [[nodiscard]] const PrimitiveRenderObject* findPrimitive(UUID id) const;
        [[nodiscard]] const SpriteRenderObject* findSprite(UUID id) const;
        [[nodiscard]] const SpriteSceneInfo* findSpriteSceneInfo(UUID id) const;

        [[nodiscard]] GpuScene* getGpuScene() const { return m_gpu_scene.get(); }

    private:
        Bool initialize(const RenderSceneCreateInfo& info);
        void shutdown();

        void reset();
        void rebuildPipelineSceneData(DrawCommandList& cmd_list);
        void syncPrimitiveGpuScene(const RenderSceneDelta& delta);
        void syncSpriteGpuScene(const RenderSceneDelta& delta);
        void syncLightGpuScene(const RenderSceneDelta& delta);
        void markPrimitiveDirty(UUID id, PrimitiveUpdateType update_type);
        void markSpriteDirty(UUID id, SpriteUpdateType update_type);
        void markLightDirty(UUID id, LightUpdateType update_type);
        void upsertSpriteSceneInfo(UUID id);
        void removeSpriteSceneInfo(UUID id);
        void applySpriteTransform(UUID id);
        PrimitiveSceneInfo* findPrimitiveSceneInfo(UUID id);
        const PrimitiveSceneInfo* findPrimitiveSceneInfo(UUID id) const;
        void upsertPrimitiveSceneInfo(UUID id);
        void applyPrimitiveTransform(UUID id);
        void updatePrimitiveMaterials(UUID id);
        void updatePrimitiveState(UUID id);
        void removePrimitiveSceneInfo(UUID id);
        void resolveBatchMaterialInstances(PrimitiveSceneInfo& info);
    };

} // dodoe
