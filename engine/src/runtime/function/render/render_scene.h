// Created by Redlive on 2026/4/15.
#pragma once

#include "dopch.h"

#include "framework/scene_graph.h"

namespace dodoe {

	class RenderScene {
		struct Aabb {
			Vector3f min{0.0f};
			Vector3f max{0.0f};
		};

		Ref<SceneGraph> m_scene_graph;
		Ref<SceneCamera> m_main_camera;
		rhi::DeviceHandle m_device{};
		std::vector<Ref<MeshInstance>> m_visible_instances{};
		std::unordered_map<const Mesh*, Aabb> m_mesh_bounds_cache{};
		Matrix4f m_cached_view_projection{1.0f};
		Vector3f m_cached_camera_position{0.0f};
		bool m_has_cached_camera_state{false};
		bool m_geometry_buffers_dirty{true};
		bool m_instance_buffers_dirty{true};
		bool m_visibility_dirty{true};
	public:
		RenderScene();
		void initialize(rhi::DeviceHandle device);
		void reset();

		void setMainCamera(const Ref<SceneCamera>& camera);
		void setMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position);
		void upsertNode(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
		                const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling);
		void upsertPointLight(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
	                     const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling,
	                     const Ref<PointLight>& light);
		void upsertSpotLight(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
	                    const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling,
	                    const Ref<SpotLight>& light);
		void removeNode(Uuid entity_uuid);
		void upsertMeshInstance(Uuid entity_uuid, const Ref<Mesh>& mesh);
		void removeMeshInstance(Uuid entity_uuid);
		void rebuild();

		[[nodiscard]] const Ref<SceneCamera>& mainCamera() const { return m_main_camera; }
		[[nodiscard]] const std::vector<Ref<MeshInstance>>& mainCameraInstances() const { return m_visible_instances; }
		[[nodiscard]] Ref<SceneGraph> getSceneGraph() const { return m_scene_graph; }

		void prepareBuffers(const rhi::CommandListHandle& cmd_list);

		void createMeshBuffers(const rhi::CommandListHandle& cmd_list);
		void createMaterialBuffer();
		void createGeometryBuffer(const rhi::CommandListHandle& cmd_list);
		void createInstanceBuffer(const rhi::CommandListHandle& cmd_list);
		void createMaterialConstantBuffer();

	private:
		void createMeshBuffer(const rhi::CommandListHandle& cmd_list, const Ref<Mesh>& mesh, ui32 instance_count);
		void updateVisibleInstances();
		[[nodiscard]] const Aabb& getMeshBounds(const Ref<Mesh>& mesh);
		[[nodiscard]] bool isInstanceVisible(const Ref<MeshInstance>& instance, const Matrix4f& view_projection);
	};

} // dodoe
