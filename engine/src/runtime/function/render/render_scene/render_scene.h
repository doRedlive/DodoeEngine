// Created by Redlive on 2026/4/15.
#pragma once

#include "dopch.h"

#include "framework/primitive_scene_info.h"
#include "render_object.h"
#include "runtime/function/graphics/gfx.h"

namespace dodoe {

    struct RenderSceneCreateInfo {
        gfx::DeviceHandle device{nullptr};
    };

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

    enum class RenderLightType : UInt8 {
        Point,
        Spot
    };

    struct RenderLightObject {
        RenderLightType type{RenderLightType::Point};
        Color color{Color::white()};
        Float intensity{1.0f};
        Float radius{0.0f};
        Float range{0.0f};
        Float inner_angle{180.0f};
        Float outer_angle{180.0f};
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

    class RenderScene : public Managed<RenderScene, RenderSceneCreateInfo> {
        friend class Managed<RenderScene, RenderSceneCreateInfo>;
		struct Aabb {
			Vector3f min{0.0f};
			Vector3f max{0.0f};
		};

		RenderSceneCamera m_main_camera{};
		gfx::DeviceHandle m_device{};
		std::unordered_map<const Mesh*, Aabb> m_mesh_bounds_cache{};
        UnorderedMap<Uuid, Scope<RenderObject>> m_render_objects{};
        UnorderedMap<Uuid, Matrix4f> m_render_object_world_transforms{};
        UnorderedMap<Uuid, RenderLightObject> m_light_objects{};
        UnorderedMap<Uuid, Matrix4f> m_light_world_transforms{};
		bool m_geometry_buffers_dirty{true};
        bool m_scene_data_dirty{true};
        std::unordered_map<Uuid, Size_t> m_primitive_indices{};
        std::unordered_map<Uuid, Size_t> m_point_light_indices{};
        std::unordered_map<Uuid, PrimitiveUpdateType> m_pending_primitive_updates{};
        std::unordered_map<Uuid, LightUpdateType> m_pending_light_updates{};
        DynamicArray<PrimitiveSceneInfo> m_primitives{};
        DynamicArray<RenderDirectionalLight> m_directional_lights{};
        DynamicArray<RenderPointLight> m_point_lights{};
        gfx::TextureHandle m_skybox_texture{};
	public:
        RenderScene() = default;
        ~RenderScene() = default;

		void setMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position);
		void addRenderObject(Uuid entity_uuid, const Matrix4f& world_transform, Scope<RenderObject> render_object);
        void updateRenderObjectTransform(Uuid entity_uuid, const Matrix4f& world_transform);
		void removeRenderObject(Uuid entity_uuid);
        void addLightObject(Uuid entity_uuid, const Matrix4f& world_transform, const RenderLightObject& light);
        void updateLightTransform(Uuid entity_uuid, const Matrix4f& world_transform);
        void removeLightObject(Uuid entity_uuid);
		void flushUpdates();

        [[nodiscard]] const RenderSceneCamera& mainCamera() const { return m_main_camera; }
        [[nodiscard]] const DynamicArray<PrimitiveSceneInfo>& getPrimitives() const { return m_primitives; }
        [[nodiscard]] const DynamicArray<RenderDirectionalLight>& getDirectionalLights() const { return m_directional_lights; }
        [[nodiscard]] const DynamicArray<RenderPointLight>& getPointLights() const { return m_point_lights; }
        [[nodiscard]] const gfx::TextureHandle& getSkyboxTexture() const { return m_skybox_texture; }
        void setSkyboxTexture(const gfx::TextureHandle& skybox_texture) { m_skybox_texture = skybox_texture; }
        [[nodiscard]] Bool hasRenderObject(Uuid entity_uuid) const { return m_render_objects.find(entity_uuid) != m_render_objects.end(); }
        [[nodiscard]] Bool hasLightObject(Uuid entity_uuid) const { return m_light_objects.find(entity_uuid) != m_light_objects.end(); }
        [[nodiscard]] const RenderObject* findRenderObject(Uuid entity_uuid) const;
        [[nodiscard]] const RenderLightObject* findLightObject(Uuid entity_uuid) const;

		void prepareBuffers(const gfx::CommandListHandle& cmd_list);

		void createMeshBuffers(const gfx::CommandListHandle& cmd_list);
		void createMaterialBuffer();
		void createGeometryBuffer(const gfx::CommandListHandle& cmd_list);
		void createMaterialConstantBuffer();

	private:
        bool initialize(const RenderSceneCreateInfo& info);
        void shutdown();
        void reset();
        void rebuildPipelineSceneData();
        void markPrimitiveDirty(Uuid entity_uuid, PrimitiveUpdateType update_type);
        void markLightDirty(Uuid entity_uuid, LightUpdateType update_type);
        PrimitiveSceneInfo* findPrimitiveSceneInfo(Uuid entity_uuid);
        const PrimitiveSceneInfo* findPrimitiveSceneInfo(Uuid entity_uuid) const;
        void upsertPrimitiveSceneInfo(Uuid entity_uuid);
        void applyPrimitiveTransform(Uuid entity_uuid);
        void updatePrimitiveMaterials(Uuid entity_uuid);
        void updatePrimitiveState(Uuid entity_uuid);
        void removePrimitiveSceneInfo(Uuid entity_uuid);
        void upsertPointLightInfo(Uuid entity_uuid);
        void removePointLightInfo(Uuid entity_uuid);
		void createMeshBuffer(const gfx::CommandListHandle& cmd_list, const Ref<Mesh>& mesh);
		[[nodiscard]] const Aabb& getMeshBounds(const Ref<Mesh>& mesh);
        [[nodiscard]] Matrix4f getRenderObjectWorldTransform(Uuid entity_uuid) const;
        [[nodiscard]] Matrix4f getLightWorldTransform(Uuid entity_uuid) const;
	};

} // dodoe
