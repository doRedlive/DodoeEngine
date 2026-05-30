// Created by Redlive on 2026/4/15.

#include "render_scene.h"
#include "runtime/core/math/math.h"

namespace dodoe {
	namespace {
		constexpr size_t kGeometryVertexStride = sizeof(Vector3f) + sizeof(ui32) + sizeof(Vector2f);

		struct FrustumPlane {
			Vector3f normal{0.0f};
			float distance{0.0f};
		};

		using FrustumPlanes = std::array<FrustumPlane, 6>;

		FrustumPlane NormalizePlane(const Vector4f& plane) {
			const Vector3f normal(plane.x, plane.y, plane.z);
			const float length = Math::Length(normal);
			if (length <= Math::Epsilon<float>()) {
				return {};
			}

			return {normal / length, plane.w / length};
		}

		FrustumPlanes ExtractFrustumPlanes(const Matrix4f& view_projection) {
			const Vector4f row0(view_projection[0][0], view_projection[1][0], view_projection[2][0], view_projection[3][0]);
			const Vector4f row1(view_projection[0][1], view_projection[1][1], view_projection[2][1], view_projection[3][1]);
			const Vector4f row2(view_projection[0][2], view_projection[1][2], view_projection[2][2], view_projection[3][2]);
			const Vector4f row3(view_projection[0][3], view_projection[1][3], view_projection[2][3], view_projection[3][3]);

			return {
				NormalizePlane(row3 + row0),
				NormalizePlane(row3 - row0),
				NormalizePlane(row3 + row1),
				NormalizePlane(row3 - row1),
				NormalizePlane(row3 + row2),
				NormalizePlane(row3 - row2)
			};
		}

		bool IntersectsFrustum(const FrustumPlanes& planes, const Vector3f& center, const Vector3f& extents) {
			for (const auto& plane : planes) {
				const float projected_radius =
					std::abs(plane.normal.x) * extents.x +
					std::abs(plane.normal.y) * extents.y +
					std::abs(plane.normal.z) * extents.z;
				const float signed_distance = Math::Dot(plane.normal, center) + plane.distance;
				if (signed_distance + projected_radius < 0.0f) {
					return false;
				}
			}

			return true;
		}
	}

	RenderScene::RenderScene() {
		reset();
	}

	void RenderScene::initialize(const rhi::DeviceHandle device) {
		reset();
		m_device = device;
	}

	void RenderScene::reset() {
		m_scene_graph = SceneGraph::Create();
		m_main_camera = nullptr;
		m_device = nullptr;
		m_visible_instances.clear();
		m_mesh_bounds_cache.clear();
		m_cached_view_projection = Matrix4f(1.0f);
		m_cached_camera_position = Vector3f(0.0f);
		m_has_cached_camera_state = false;
		m_geometry_buffers_dirty = true;
		m_instance_buffers_dirty = true;
		m_visibility_dirty = true;
	}

	void RenderScene::setMainCamera(const Ref<SceneCamera>& camera) {
		m_main_camera = camera ? m_scene_graph->setMainCamera(camera) : nullptr;
		m_visibility_dirty = true;
	}

	void RenderScene::setMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position) {
		auto camera = m_main_camera;
		if (!camera) {
			camera = create_ref<PerspectiveCamera>();
			setMainCamera(camera);
		}
		camera->setViewProjectionMatrix(view_proj_matrix);
		camera->setPosition(position);
		m_main_camera = m_scene_graph->setMainCamera(camera);

		if (!m_has_cached_camera_state ||
			m_cached_view_projection != view_proj_matrix ||
			m_cached_camera_position != position) {
			m_cached_view_projection = view_proj_matrix;
			m_cached_camera_position = position;
			m_has_cached_camera_state = true;
			m_visibility_dirty = true;
		}
	}

	void RenderScene::upsertNode(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
	                             const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling) {
		m_scene_graph->upsertNode(entity_uuid, name, parent_uuid, translation, rotation, scaling);
	}

	void RenderScene::upsertPointLight(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
	                                 const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling,
	                                 const Ref<PointLight>& light) {
		m_scene_graph->upsertPointLight(entity_uuid, name, parent_uuid, translation, rotation, scaling, light);
	}

	void RenderScene::upsertSpotLight(Uuid entity_uuid, const std::string& name, Uuid parent_uuid,
	                                const Vector3f& translation, const Vector3f& rotation, const Vector3f& scaling,
	                                const Ref<SpotLight>& light) {
		m_scene_graph->upsertSpotLight(entity_uuid, name, parent_uuid, translation, rotation, scaling, light);
	}

	void RenderScene::removeNode(Uuid entity_uuid) {
		m_scene_graph->removeNode(entity_uuid);
	}

	void RenderScene::upsertMeshInstance(Uuid entity_uuid, const Ref<Mesh>& mesh) {
		m_scene_graph->upsertMeshInstance(entity_uuid, mesh);
		m_geometry_buffers_dirty = true;
		m_instance_buffers_dirty = true;
		m_visibility_dirty = true;
	}

	void RenderScene::removeMeshInstance(Uuid entity_uuid) {
		m_scene_graph->removeMeshInstance(entity_uuid);
		m_instance_buffers_dirty = true;
		m_visibility_dirty = true;
	}

	void RenderScene::rebuild() {
		m_scene_graph->rebuild();
		m_main_camera = m_scene_graph->getMainCamera();
		m_instance_buffers_dirty = true;
		m_visibility_dirty = true;
	}

	void RenderScene::prepareBuffers(const rhi::CommandListHandle& cmd_list) {
		updateVisibleInstances();
		createMeshBuffers(cmd_list);
	}

	void RenderScene::createMeshBuffers(const rhi::CommandListHandle& cmd_list) {
		createGeometryBuffer(cmd_list);
		createInstanceBuffer(cmd_list);
	}

	void RenderScene::createMaterialBuffer() {
	}

	void RenderScene::createGeometryBuffer(const rhi::CommandListHandle& cmd_list) {
		if (!m_device || !cmd_list || !m_geometry_buffers_dirty) {
			return;
		}

		for (auto it = m_scene_graph->getMeshes().begin(); it != m_scene_graph->getMeshes().end(); ++it) {
			const auto& mesh = *it;
			createMeshBuffer(cmd_list, mesh, 0);
		}

		m_geometry_buffers_dirty = false;
	}

	void RenderScene::createInstanceBuffer(const rhi::CommandListHandle& cmd_list) {
		if (!m_device || !cmd_list || !m_instance_buffers_dirty) {
			return;
		}

		const auto& instances = m_visible_instances;
		for (size_t begin = 0; begin < instances.size();) {
			const auto& first_instance = instances[begin];
			if (!first_instance || !first_instance->getMesh()) {
				++begin;
				continue;
			}

			const auto& mesh = first_instance->getMesh();
			size_t end = begin + 1;
			while (end < instances.size()) {
				const auto& instance = instances[end];
				if (!instance || instance->getMesh() != mesh) {
					break;
				}
				++end;
			}

			createMeshBuffer(cmd_list, mesh, static_cast<ui32>(end - begin));
			if (!mesh || !mesh->buffers || !mesh->buffers->instance_buffer) {
				begin = end;
				continue;
			}

			std::vector<Matrix4f> instance_data;
			std::vector<ui32> instance_ids;
			instance_data.reserve(end - begin);
			instance_ids.reserve(end - begin);
			for (size_t i = begin; i < end; ++i) {
				const auto& instance = instances[i];
				if (!instance) {
					continue;
				}
				instance_data.push_back(instance->getModelMatrix());
				auto* node = instance->getNode();
				const uint64_t uuid_value = node ? static_cast<uint64_t>(node->getEntityUuid()) : 0;
				instance_ids.push_back(static_cast<ui32>(uuid_value & 0xFFFFFFFFu));
			}

			if (!instance_data.empty()) {
				cmd_list->writeBuffer(
					mesh->buffers->instance_buffer,
					instance_data.data(),
					sizeof(Matrix4f) * instance_data.size()
				);
			}
			if (!instance_ids.empty() && mesh->buffers->instance_id_buffer) {
				cmd_list->writeBuffer(
					mesh->buffers->instance_id_buffer,
					instance_ids.data(),
					sizeof(ui32) * instance_ids.size()
				);
			}

			begin = end;
		}

		m_instance_buffers_dirty = false;
	}

	void RenderScene::createMaterialConstantBuffer() {
	}

	void RenderScene::createMeshBuffer(const rhi::CommandListHandle& cmd_list, const Ref<Mesh>& mesh, const ui32 instance_count) {
		if (!m_device || !cmd_list || !mesh || !mesh->buffers) {
			return;
		}

		const auto vertex_count = mesh->buffers->position_data.size();
		const auto index_count = mesh->buffers->index_data.size();
		const size_t vertex_byte_size = kGeometryVertexStride * vertex_count;
		const size_t index_byte_size = sizeof(ui32) * index_count;
		const size_t instance_byte_size = sizeof(Matrix4f) * std::max<ui32>(instance_count, 1);

		if (vertex_count > 0) {
			const bool need_vertex_buffer = !mesh->buffers->vertex_buffer || mesh->buffers->vertex_buffer->getDesc().byteSize < vertex_byte_size;
			if (need_vertex_buffer) {
				auto vertex_buffer_desc = rhi::BufferDesc()
					.setByteSize(vertex_byte_size)
					.setIsVertexBuffer(true)
					.enableAutomaticStateTracking(rhi::ResourceStates::VertexBuffer)
					.setDebugName(fmt::format("RenderScene Vertex Buffer {}", mesh->name));
				mesh->buffers->vertex_buffer = m_device->createBuffer(vertex_buffer_desc);

				std::vector<std::byte> vertex_bytes(vertex_byte_size);
				for (size_t i = 0; i < vertex_count; ++i) {
					const size_t base_offset = i * kGeometryVertexStride;
					std::memcpy(vertex_bytes.data() + base_offset, &mesh->buffers->position_data[i], sizeof(Vector3f));

					const ui32 normal = i < mesh->buffers->normal_data.size() ? mesh->buffers->normal_data[i] : 0;
					std::memcpy(vertex_bytes.data() + base_offset + sizeof(Vector3f), &normal, sizeof(ui32));

					const Vector2f uv = i < mesh->buffers->texcoord1_data.size() ? mesh->buffers->texcoord1_data[i] : Vector2f(0.0f);
					std::memcpy(vertex_bytes.data() + base_offset + sizeof(Vector3f) + sizeof(ui32), &uv, sizeof(Vector2f));
				}
				cmd_list->writeBuffer(mesh->buffers->vertex_buffer, vertex_bytes.data(), vertex_byte_size);
			}
		}

		if (index_count > 0) {
			const bool need_index_buffer = !mesh->buffers->index_buffer || mesh->buffers->index_buffer->getDesc().byteSize < index_byte_size;
			if (need_index_buffer) {
				auto index_buffer_desc = rhi::BufferDesc()
					.setByteSize(index_byte_size)
					.setIsIndexBuffer(true)
					.enableAutomaticStateTracking(rhi::ResourceStates::IndexBuffer)
					.setDebugName(fmt::format("RenderScene Index Buffer {}", mesh->name));
				mesh->buffers->index_buffer = m_device->createBuffer(index_buffer_desc);
				cmd_list->writeBuffer(mesh->buffers->index_buffer, mesh->buffers->index_data.data(), index_byte_size);
			}
		}

		if (instance_count > 0) {
			const bool need_instance_buffer = !mesh->buffers->instance_buffer || mesh->buffers->instance_buffer->getDesc().byteSize < instance_byte_size;
			if (need_instance_buffer) {
				auto instance_buffer_desc = rhi::BufferDesc()
					.setByteSize(instance_byte_size)
					.setIsVertexBuffer(true)
					.enableAutomaticStateTracking(rhi::ResourceStates::VertexBuffer)
					.setDebugName(fmt::format("RenderScene Instance Buffer {}", mesh->name));
				mesh->buffers->instance_buffer = m_device->createBuffer(instance_buffer_desc);
			}
			const size_t instance_id_byte_size = sizeof(ui32) * std::max<ui32>(instance_count, 1);
			const bool need_instance_id_buffer = !mesh->buffers->instance_id_buffer || mesh->buffers->instance_id_buffer->getDesc().byteSize < instance_id_byte_size;
			if (need_instance_id_buffer) {
				auto instance_id_desc = rhi::BufferDesc()
					.setByteSize(instance_id_byte_size)
					.setIsVertexBuffer(true)
					.enableAutomaticStateTracking(rhi::ResourceStates::VertexBuffer)
					.setDebugName(fmt::format("RenderScene InstanceId Buffer {}", mesh->name));
				mesh->buffers->instance_id_buffer = m_device->createBuffer(instance_id_desc);
			}
		}
	}

	void RenderScene::updateVisibleInstances() {
		if (!m_visibility_dirty) {
			return;
		}

		const auto& all_instances = m_scene_graph->getMeshInstances();
		std::vector<Ref<MeshInstance>> visible_instances;
		visible_instances.reserve(all_instances.size());

		if (!m_main_camera || !m_main_camera->isValid()) {
			visible_instances = all_instances;
		} else {
			const Matrix4f view_projection = m_main_camera->getViewProjectionMatrix();
			for (const auto& instance : all_instances) {
				if (isInstanceVisible(instance, view_projection)) {
					visible_instances.push_back(instance);
				}
			}
		}

		const bool visibility_changed =
			visible_instances.size() != m_visible_instances.size() ||
			!std::equal(visible_instances.begin(), visible_instances.end(), m_visible_instances.begin());
		if (visibility_changed) {
			m_visible_instances = std::move(visible_instances);
			m_instance_buffers_dirty = true;
		} else {
			m_visible_instances = std::move(visible_instances);
		}

		m_visibility_dirty = false;
	}

	const RenderScene::Aabb& RenderScene::getMeshBounds(const Ref<Mesh>& mesh) {
		static const Aabb kDefaultBounds{
			Vector3f(-0.5f, -0.5f, -0.5f),
			Vector3f(0.5f, 0.5f, 0.5f)
		};

		if (!mesh || !mesh->buffers || mesh->buffers->position_data.empty()) {
			return kDefaultBounds;
		}

		const Mesh* mesh_key = mesh.get();
		const auto cached = m_mesh_bounds_cache.find(mesh_key);
		if (cached != m_mesh_bounds_cache.end()) {
			return cached->second;
		}

		Vector3f min_corner = mesh->buffers->position_data.front();
		Vector3f max_corner = mesh->buffers->position_data.front();
		for (const auto& position : mesh->buffers->position_data) {
			min_corner = Vector3f(
				(std::min)(min_corner.x, position.x),
				(std::min)(min_corner.y, position.y),
				(std::min)(min_corner.z, position.z)
			);
			max_corner = Vector3f(
				(std::max)(max_corner.x, position.x),
				(std::max)(max_corner.y, position.y),
				(std::max)(max_corner.z, position.z)
			);
		}

		return m_mesh_bounds_cache.emplace(mesh_key, Aabb{min_corner, max_corner}).first->second;
	}

	bool RenderScene::isInstanceVisible(const Ref<MeshInstance>& instance, const Matrix4f& view_projection) {
		if (!instance) {
			return false;
		}

		const auto& mesh = instance->getMesh();
		if (!mesh) {
			return false;
		}

		const auto& bounds = getMeshBounds(mesh);
		const Vector3f local_center = (bounds.min + bounds.max) * 0.5f;
		const Vector3f local_extents = (bounds.max - bounds.min) * 0.5f;
		const Matrix4f model = instance->getModelMatrix();
		const Vector3f world_center = Vector3f(model * Vector4f(local_center, 1.0f));

		const glm::mat3 linear = glm::mat3(model);
		const glm::mat3 abs_linear(glm::abs(linear[0]), glm::abs(linear[1]), glm::abs(linear[2]));
		const Vector3f world_extents = abs_linear * local_extents;

		return IntersectsFrustum(ExtractFrustumPlanes(view_projection), world_center, world_extents);
	}

} // dodoe
