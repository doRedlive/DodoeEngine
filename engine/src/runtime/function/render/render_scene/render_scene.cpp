// Created by Redlive on 2026/4/15.

#include "render_scene.h"
#include "runtime/core/math/math.h"
#include "glm/gtc/matrix_transform.hpp"

namespace dodoe {
	namespace {
		constexpr size_t kGeometryVertexStride = sizeof(Vector3f) + sizeof(ui32) + sizeof(Vector2f);
	}

    bool RenderScene::initialize(const RenderSceneCreateInfo& info) {
        reset();
        m_device = info.device;
        return true;
	}

    void RenderScene::shutdown() {
        reset();
    }

    void RenderScene::reset() {
		m_main_camera = {};
		m_device = nullptr;
		m_mesh_bounds_cache.clear();
        m_render_objects.clear();
        m_render_object_world_transforms.clear();
        m_light_objects.clear();
        m_light_world_transforms.clear();
		m_geometry_buffers_dirty = true;
        m_scene_data_dirty = true;
        m_primitive_indices.clear();
        m_point_light_indices.clear();
        m_pending_primitive_updates.clear();
        m_pending_light_updates.clear();
        m_primitives.clear();
        m_directional_lights.clear();
        m_point_lights.clear();
        m_skybox_texture = nullptr;
	}

	void RenderScene::setMainCameraViewProjection(const Matrix4f& view_proj_matrix, const Vector3f& position) {
		m_main_camera.view_projection = view_proj_matrix;
		m_main_camera.position = position;
        m_main_camera.valid = true;
	}

	void RenderScene::addRenderObject(Uuid entity_uuid, const Matrix4f& world_transform, Scope<RenderObject> render_object) {
        DO_ASSERT(render_object != nullptr, "RenderScene upsertRenderObject requires valid render object");

        PrimitiveUpdateType update_type = PrimitiveUpdateType::Added;
        const auto existing_it = m_render_objects.find(entity_uuid);
        const Ref<Mesh> next_mesh = render_object->getMesh();
        if (existing_it != m_render_objects.end()) {
            update_type = PrimitiveUpdateType::None;
            const auto* previous_render_object = existing_it->second.get();
            DO_ASSERT(previous_render_object != nullptr, "RenderScene existing render object is null");
            const RenderObjectDirtyFlags dirty_flags = render_object->diff(*previous_render_object);
            if (HasAnyFlags(dirty_flags, RenderObjectDirtyFlags::Mesh)) {
                update_type |= PrimitiveUpdateType::MeshChanged;
                if (previous_render_object->getMesh() != next_mesh) {
		            m_geometry_buffers_dirty = true;
                }
            }
            if (HasAnyFlags(dirty_flags, RenderObjectDirtyFlags::Materials)) {
                update_type |= PrimitiveUpdateType::MaterialChanged;
            }
            if (HasAnyFlags(dirty_flags, RenderObjectDirtyFlags::State)) {
                update_type |= PrimitiveUpdateType::StateChanged;
            }
            if (HasAnyFlags(dirty_flags, RenderObjectDirtyFlags::ProxyData)) {
                update_type |= PrimitiveUpdateType::ProxyChanged;
            }
        } else {
            if (next_mesh) {
		        m_geometry_buffers_dirty = true;
            }
        }

        const auto transform_it = m_render_object_world_transforms.find(entity_uuid);
        if (transform_it == m_render_object_world_transforms.end() || transform_it->second != world_transform) {
            update_type |= existing_it == m_render_objects.end() ? PrimitiveUpdateType::Added : PrimitiveUpdateType::TransformChanged;
        }

        m_render_objects[entity_uuid] = std::move(render_object);
        m_render_object_world_transforms[entity_uuid] = world_transform;
        if (update_type != PrimitiveUpdateType::None) {
            markPrimitiveDirty(entity_uuid, update_type);
        }
	}

    void RenderScene::updateRenderObjectTransform(const Uuid entity_uuid, const Matrix4f& world_transform) {
        if (m_render_objects.find(entity_uuid) == m_render_objects.end()) {
            return;
        }

        const auto transform_it = m_render_object_world_transforms.find(entity_uuid);
        if (transform_it != m_render_object_world_transforms.end() && transform_it->second == world_transform) {
            return;
        }

        m_render_object_world_transforms[entity_uuid] = world_transform;
        markPrimitiveDirty(entity_uuid, PrimitiveUpdateType::TransformChanged);
    }

	void RenderScene::removeRenderObject(Uuid entity_uuid) {
        m_render_objects.erase(entity_uuid);
        m_render_object_world_transforms.erase(entity_uuid);
        markPrimitiveDirty(entity_uuid, PrimitiveUpdateType::Removed);
	}

    void RenderScene::addLightObject(const Uuid entity_uuid, const Matrix4f& world_transform, const RenderLightObject& light) {
        LightUpdateType update_type = LightUpdateType::Added;
        const auto existing_it = m_light_objects.find(entity_uuid);
        if (existing_it != m_light_objects.end()) {
            update_type = LightUpdateType::None;
            if (std::memcmp(&existing_it->second, &light, sizeof(RenderLightObject)) != 0) {
                update_type |= LightUpdateType::StateChanged;
            }
        }

        const auto transform_it = m_light_world_transforms.find(entity_uuid);
        if (transform_it == m_light_world_transforms.end() || transform_it->second != world_transform) {
            update_type |= existing_it == m_light_objects.end() ? LightUpdateType::Added : LightUpdateType::TransformChanged;
        }

        m_light_objects[entity_uuid] = light;
        m_light_world_transforms[entity_uuid] = world_transform;
        if (update_type != LightUpdateType::None) {
            markLightDirty(entity_uuid, update_type);
        }
    }

    void RenderScene::updateLightTransform(const Uuid entity_uuid, const Matrix4f& world_transform) {
        if (m_light_objects.find(entity_uuid) == m_light_objects.end()) {
            return;
        }

        const auto transform_it = m_light_world_transforms.find(entity_uuid);
        if (transform_it != m_light_world_transforms.end() && transform_it->second == world_transform) {
            return;
        }

        m_light_world_transforms[entity_uuid] = world_transform;
        markLightDirty(entity_uuid, LightUpdateType::TransformChanged);
    }

    void RenderScene::removeLightObject(Uuid entity_uuid) {
        m_light_objects.erase(entity_uuid);
        m_light_world_transforms.erase(entity_uuid);
        markLightDirty(entity_uuid, LightUpdateType::Removed);
    }

	void RenderScene::flushUpdates() {
        if (!m_scene_data_dirty && m_pending_primitive_updates.empty() && m_pending_light_updates.empty()) {
            return;
        }
        rebuildPipelineSceneData();
	}

	void RenderScene::prepareBuffers(const gfx::CommandListHandle& cmd_list) {
		createGeometryBuffer(cmd_list);
	}

	void RenderScene::createMeshBuffers(const gfx::CommandListHandle& cmd_list) {
		createGeometryBuffer(cmd_list);
	}

	void RenderScene::createMaterialBuffer() {
	}

	void RenderScene::createGeometryBuffer(const gfx::CommandListHandle& cmd_list) {
		if (!m_device || !cmd_list || !m_geometry_buffers_dirty) {
			return;
		}

        std::unordered_set<const Mesh*> uploaded_meshes{};
        for (const auto& [entity_uuid, render_object] : m_render_objects) {
            (void)entity_uuid;
            if (!render_object) {
                continue;
            }
            const Ref<Mesh>& mesh = render_object->getMesh();
            if (!mesh) {
                continue;
            }

            if (uploaded_meshes.insert(mesh.get()).second) {
			    createMeshBuffer(cmd_list, mesh);
            }
        }

		m_geometry_buffers_dirty = false;
	}

	void RenderScene::createMaterialConstantBuffer() {
	}

	void RenderScene::createMeshBuffer(const gfx::CommandListHandle& cmd_list, const Ref<Mesh>& mesh) {
		if (!m_device || !cmd_list || !mesh || !mesh->buffers) {
			return;
		}

		const auto vertex_count = mesh->buffers->position_data.size();
		const auto index_count = mesh->buffers->index_data.size();
		const size_t vertex_byte_size = kGeometryVertexStride * vertex_count;
		const size_t index_byte_size = sizeof(ui32) * index_count;

		if (vertex_count > 0) {
			const bool need_vertex_buffer = !mesh->buffers->vertex_buffer || mesh->buffers->vertex_buffer->getDesc().byteSize < vertex_byte_size;
			if (need_vertex_buffer) {
				auto vertex_buffer_desc = gfx::BufferDesc()
					.setByteSize(vertex_byte_size)
					.setIsVertexBuffer(true)
					.enableAutomaticStateTracking(gfx::ResourceStates::VertexBuffer)
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
				auto index_buffer_desc = gfx::BufferDesc()
					.setByteSize(index_byte_size)
					.setIsIndexBuffer(true)
					.enableAutomaticStateTracking(gfx::ResourceStates::IndexBuffer)
					.setDebugName(fmt::format("RenderScene Index Buffer {}", mesh->name));
				mesh->buffers->index_buffer = m_device->createBuffer(index_buffer_desc);
				cmd_list->writeBuffer(mesh->buffers->index_buffer, mesh->buffers->index_data.data(), index_byte_size);
			}
		}

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

    const RenderObject* RenderScene::findRenderObject(const Uuid entity_uuid) const {
        const auto it = m_render_objects.find(entity_uuid);
        return it != m_render_objects.end() ? it->second.get() : nullptr;
    }

    PrimitiveSceneInfo* RenderScene::findPrimitiveSceneInfo(const Uuid entity_uuid) {
        const auto primitive_it = m_primitive_indices.find(entity_uuid);
        return primitive_it != m_primitive_indices.end() ? &m_primitives[primitive_it->second] : nullptr;
    }

    const PrimitiveSceneInfo* RenderScene::findPrimitiveSceneInfo(const Uuid entity_uuid) const {
        const auto primitive_it = m_primitive_indices.find(entity_uuid);
        return primitive_it != m_primitive_indices.end() ? &m_primitives[primitive_it->second] : nullptr;
    }

    const RenderLightObject* RenderScene::findLightObject(const Uuid entity_uuid) const {
        const auto it = m_light_objects.find(entity_uuid);
        return it != m_light_objects.end() ? &it->second : nullptr;
    }

    void RenderScene::markPrimitiveDirty(const Uuid entity_uuid, const PrimitiveUpdateType update_type) {
        m_pending_primitive_updates[entity_uuid] |= update_type;
        m_scene_data_dirty = true;
    }

    void RenderScene::markLightDirty(const Uuid entity_uuid, const LightUpdateType update_type) {
        m_pending_light_updates[entity_uuid] |= update_type;
        m_scene_data_dirty = true;
    }

    void RenderScene::upsertPrimitiveSceneInfo(const Uuid entity_uuid) {
        const RenderObject* render_object = findRenderObject(entity_uuid);
        if (render_object == nullptr || !render_object->getMesh()) {
            removePrimitiveSceneInfo(entity_uuid);
            return;
        }

        const auto& bounds = getMeshBounds(render_object->getMesh());
        PrimitiveSceneInfo primitive = render_object->buildSceneInfo(
            static_cast<Identifier>(static_cast<uint64_t>(entity_uuid)),
            getRenderObjectWorldTransform(entity_uuid),
            bounds.min,
            bounds.max);

        const auto primitive_it = m_primitive_indices.find(entity_uuid);
        if (primitive_it != m_primitive_indices.end()) {
            m_primitives[primitive_it->second] = std::move(primitive);
            return;
        }

        m_primitive_indices[entity_uuid] = m_primitives.size();
        m_primitives.push_back(std::move(primitive));
    }

    void RenderScene::applyPrimitiveTransform(const Uuid entity_uuid) {
        PrimitiveSceneInfo* primitive = findPrimitiveSceneInfo(entity_uuid);
        if (primitive == nullptr) {
            upsertPrimitiveSceneInfo(entity_uuid);
            return;
        }

        primitive->setWorldTransform(getRenderObjectWorldTransform(entity_uuid));
    }

    void RenderScene::updatePrimitiveMaterials(const Uuid entity_uuid) {
        const RenderObject* render_object = findRenderObject(entity_uuid);
        PrimitiveSceneInfo* primitive = findPrimitiveSceneInfo(entity_uuid);
        if (render_object == nullptr || primitive == nullptr) {
            upsertPrimitiveSceneInfo(entity_uuid);
            return;
        }

        const auto materials = render_object->resolveMaterials();
        primitive->setMaterials(materials);
        primitive->setSections(render_object->buildSections(materials));
        primitive->setMeshBatches(render_object->buildMeshBatches(primitive->getId(), materials, 0));
    }

    void RenderScene::updatePrimitiveState(const Uuid entity_uuid) {
        const RenderObject* render_object = findRenderObject(entity_uuid);
        PrimitiveSceneInfo* primitive = findPrimitiveSceneInfo(entity_uuid);
        if (render_object == nullptr || primitive == nullptr) {
            upsertPrimitiveSceneInfo(entity_uuid);
            return;
        }

        primitive->setMobility(render_object->getMobility());
        primitive->setVisible(render_object->isVisible());
        primitive->setCastShadow(render_object->castsShadow());
    }

    void RenderScene::removePrimitiveSceneInfo(const Uuid entity_uuid) {
        const auto primitive_it = m_primitive_indices.find(entity_uuid);
        if (primitive_it == m_primitive_indices.end()) {
            return;
        }

        const Size_t remove_index = primitive_it->second;
        const Size_t last_index = m_primitives.size() - 1;
        if (remove_index != last_index) {
            PrimitiveSceneInfo& moved_primitive = m_primitives[last_index];
            const Uuid moved_uuid = static_cast<Uuid>(static_cast<uint64_t>(moved_primitive.getId()));
            m_primitives[remove_index] = std::move(moved_primitive);
            m_primitive_indices[moved_uuid] = remove_index;
        }
        m_primitives.pop_back();
        m_primitive_indices.erase(primitive_it);
    }

    void RenderScene::upsertPointLightInfo(const Uuid entity_uuid) {
        const RenderLightObject* light_object = findLightObject(entity_uuid);
        if (light_object == nullptr || light_object->type != RenderLightType::Point) {
            removePointLightInfo(entity_uuid);
            return;
        }

        RenderPointLight render_light{};
        render_light.color = Vector3f(light_object->color.r, light_object->color.g, light_object->color.b);
        render_light.intensity = light_object->intensity;
        render_light.radius = light_object->radius;
        render_light.range = light_object->range;
        render_light.position = Vector3f(getLightWorldTransform(entity_uuid)[3]);

        const auto light_it = m_point_light_indices.find(entity_uuid);
        if (light_it != m_point_light_indices.end()) {
            m_point_lights[light_it->second] = render_light;
            return;
        }

        m_point_light_indices[entity_uuid] = m_point_lights.size();
        m_point_lights.push_back(render_light);
    }

    void RenderScene::removePointLightInfo(const Uuid entity_uuid) {
        const auto light_it = m_point_light_indices.find(entity_uuid);
        if (light_it == m_point_light_indices.end()) {
            return;
        }

        const Size_t remove_index = light_it->second;
        const Size_t last_index = m_point_lights.size() - 1;
        if (remove_index != last_index) {
            m_point_lights[remove_index] = std::move(m_point_lights[last_index]);
            for (auto& [other_uuid, other_index] : m_point_light_indices) {
                if (other_index == last_index) {
                    other_index = remove_index;
                    break;
                }
            }
        }
        m_point_lights.pop_back();
        m_point_light_indices.erase(light_it);
    }

    Matrix4f RenderScene::getRenderObjectWorldTransform(const Uuid entity_uuid) const {
        const auto it = m_render_object_world_transforms.find(entity_uuid);
        return it != m_render_object_world_transforms.end() ? it->second : Matrix4f(1.0f);
    }

    Matrix4f RenderScene::getLightWorldTransform(const Uuid entity_uuid) const {
        const auto it = m_light_world_transforms.find(entity_uuid);
        return it != m_light_world_transforms.end() ? it->second : Matrix4f(1.0f);
    }

    void RenderScene::rebuildPipelineSceneData() {
        for (const auto& [entity_uuid, update_type] : m_pending_primitive_updates) {
            if (HasAnyFlags(update_type, PrimitiveUpdateType::Removed) && m_render_objects.find(entity_uuid) == m_render_objects.end()) {
                removePrimitiveSceneInfo(entity_uuid);
                continue;
            }

            if (HasAnyFlags(update_type, PrimitiveUpdateType::Added | PrimitiveUpdateType::MeshChanged | PrimitiveUpdateType::ProxyChanged)) {
                upsertPrimitiveSceneInfo(entity_uuid);
                continue;
            }
            if (HasAnyFlags(update_type, PrimitiveUpdateType::TransformChanged)) {
                applyPrimitiveTransform(entity_uuid);
            }
            if (HasAnyFlags(update_type, PrimitiveUpdateType::MaterialChanged)) {
                updatePrimitiveMaterials(entity_uuid);
            }
            if (HasAnyFlags(update_type, PrimitiveUpdateType::StateChanged)) {
                updatePrimitiveState(entity_uuid);
            }
        }
        for (const auto& [entity_uuid, update_type] : m_pending_light_updates) {
            if (HasAnyFlags(update_type, LightUpdateType::Removed) && m_light_objects.find(entity_uuid) == m_light_objects.end()) {
                removePointLightInfo(entity_uuid);
                continue;
            }
            if (HasAnyFlags(update_type, LightUpdateType::Added | LightUpdateType::TransformChanged | LightUpdateType::StateChanged)) {
                upsertPointLightInfo(entity_uuid);
            }
        }
        for (auto it = m_point_light_indices.begin(); it != m_point_light_indices.end();) {
            const auto light_it = m_light_objects.find(it->first);
            if (light_it == m_light_objects.end() || light_it->second.type != RenderLightType::Point) {
                const Uuid entity_uuid = it->first;
                ++it;
                removePointLightInfo(entity_uuid);
            } else {
                ++it;
            }
        }

        m_directional_lights.clear();
        m_pending_primitive_updates.clear();
        m_pending_light_updates.clear();
        m_scene_data_dirty = false;
    }

} // dodoe
