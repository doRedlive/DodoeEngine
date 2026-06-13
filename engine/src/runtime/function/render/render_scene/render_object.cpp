#include "render_object.h"

namespace dodoe {

    UInt32 RenderObject::getInstanceCount() const {
        return m_mesh ? 1u : 0u;
    }

    void RenderObject::appendInstanceSceneData(DynamicArray<InstanceSceneData>& out_instance_scene_data, const Matrix4f& world_transform) const {
        InstanceSceneData instance_scene_data{};
        instance_scene_data.model = world_transform;
        out_instance_scene_data.push_back(instance_scene_data);
    }

    RenderObjectDirtyFlags RenderObject::diff(const RenderObject& previous) const {
        if (getRenderObjectType() != previous.getRenderObjectType()) {
            return RenderObjectDirtyFlags::All;
        }

        RenderObjectDirtyFlags dirty_flags = RenderObjectDirtyFlags::None;
        if (m_mesh != previous.m_mesh) {
            dirty_flags |= RenderObjectDirtyFlags::Mesh;
        }

        const Bool same_material_count = m_override_materials.size() == previous.m_override_materials.size();
        Bool same_materials = same_material_count;
        if (same_materials) {
            for (Size_t material_index = 0; material_index < m_override_materials.size(); material_index++) {
                if (m_override_materials[material_index] != previous.m_override_materials[material_index]) {
                    same_materials = false;
                    break;
                }
            }
        }
        if (!same_materials) {
            dirty_flags |= RenderObjectDirtyFlags::Materials;
        }

        if (m_mobility != previous.m_mobility || m_visible != previous.m_visible || m_cast_shadow != previous.m_cast_shadow) {
            dirty_flags |= RenderObjectDirtyFlags::State;
        }

        return dirty_flags;
    }

    DynamicArray<Ref<Material>> RenderObject::resolveMaterials() const {
        DynamicArray<Ref<Material>> materials{};
        if (!m_mesh) {
            return materials;
        }

        materials.reserve(m_mesh->geometries.size());
        for (Size_t geometry_index = 0; geometry_index < m_mesh->geometries.size(); geometry_index++) {
            Ref<Material> material = m_mesh->geometries[geometry_index] ? m_mesh->geometries[geometry_index]->material : nullptr;
            if (geometry_index < m_override_materials.size() && m_override_materials[geometry_index]) {
                material = m_override_materials[geometry_index];
            }
            materials.push_back(material);
        }
        return materials;
    }

    DynamicArray<PrimitiveSceneInfo::Section> RenderObject::buildSections(const DynamicArray<Ref<Material>>& resolved_materials) const {
        DynamicArray<PrimitiveSceneInfo::Section> sections{};
        if (!m_mesh) {
            return sections;
        }

        sections.reserve(m_mesh->geometries.size());
        for (Size_t geometry_index = 0; geometry_index < m_mesh->geometries.size(); geometry_index++) {
            const auto& geometry = m_mesh->geometries[geometry_index];
            if (!geometry) {
                continue;
            }

            PrimitiveSceneInfo::Section section{};
            section.material = geometry_index < resolved_materials.size() ? resolved_materials[geometry_index] : geometry->material;
            section.index_offset = geometry->index_offset;
            section.vertex_offset = geometry->vertex_offset;
            section.index_count = geometry->index_count;
            section.vertex_count = geometry->vertex_count;
            section.geometry_index = geometry->geometry_index;
            section.primitive_type = geometry->type;
            sections.push_back(section);
        }

        return sections;
    }

    DynamicArray<MeshBatch> RenderObject::buildMeshBatches(
        const Identifier primitive_id,
        const DynamicArray<Ref<Material>>& resolved_materials,
        const UInt32 first_instance) const
    {
        DynamicArray<MeshBatch> mesh_batches{};
        if (!m_mesh || !m_mesh->buffers || !m_mesh->buffers->vertex_buffer || !m_mesh->buffers->index_buffer) {
            return mesh_batches;
        }

        mesh_batches.reserve(m_mesh->geometries.size());
        for (Size_t geometry_index = 0; geometry_index < m_mesh->geometries.size(); geometry_index++) {
            const auto& geometry = m_mesh->geometries[geometry_index];
            if (!geometry || geometry->index_count == 0) {
                continue;
            }

            MeshBatchElement element{};
            element.index_count = geometry->index_count;
            element.index_offset = geometry->index_offset;
            element.vertex_offset = geometry->vertex_offset;
            element.section_index = static_cast<UInt32>(geometry_index);
            element.uses_instance_range = true;
            element.first_instance = first_instance;
            element.instance_count = 1;
            element.vertex_buffer = m_mesh->buffers->vertex_buffer;
            element.index_buffer = m_mesh->buffers->index_buffer;

            MeshBatch batch{};
            batch.primitive_id = primitive_id;
            batch.material = geometry_index < resolved_materials.size() ? resolved_materials[geometry_index] : geometry->material;
            batch.pass_mask.setRelevant(MeshPassType::GBuffer, m_visible);
            batch.pass_mask.setRelevant(MeshPassType::DirectionalShadow, m_visible && m_cast_shadow);
            batch.elements.push_back(element);
            mesh_batches.push_back(std::move(batch));
        }

        return mesh_batches;
    }

    PrimitiveSceneInfo RenderObject::buildSceneInfo(
        const Identifier primitive_id,
        const Matrix4f& world_transform,
        const Vector3f& bounds_min,
        const Vector3f& bounds_max) const
    {
        PrimitiveSceneInfo primitive(primitive_id);
        const auto materials = resolveMaterials();
        primitive.setRenderObject(this);
        primitive.setMesh(m_mesh);
        primitive.setMaterials(materials);
        primitive.setSections(buildSections(materials));
        primitive.setMeshBatches(buildMeshBatches(primitive_id, materials, 0));
        primitive.setWorldTransform(world_transform);
        primitive.setBounds(bounds_min, bounds_max);
        primitive.setMobility(m_mobility);
        primitive.setVisible(m_visible);
        primitive.setCastShadow(m_cast_shadow);
        return primitive;
    }

} // dodoe
