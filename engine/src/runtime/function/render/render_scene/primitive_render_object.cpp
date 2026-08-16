// do@Redlive

#include "primitive_render_object.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/mesh_draw/mesh.h"

namespace dodoe {

    const MeshLODData* PrimitiveRenderObject::activeLOD() const {
        if (!m_mesh || m_mesh->getLODData().empty()) {
            return nullptr;
        }
        return &m_mesh->getLODData()[0];
    }

    UInt32 PrimitiveRenderObject::getInstanceCount() const {
        const auto* lod = activeLOD();
        return (lod && lod->isValid()) ? 1u : 0u;
    }

    void PrimitiveRenderObject::appendInstanceSceneData(DynamicArray<InstanceSceneData>& out_instance_scene_data, const Matrix4f& world_transform) const {
        InstanceSceneData instance_scene_data{};
        instance_scene_data.model = world_transform;
        out_instance_scene_data.push_back(instance_scene_data);
    }

    void PrimitiveRenderObject::setMesh(const Mesh* mesh, const Int32 section_index) {
        m_mesh = mesh;
        m_section_index = section_index;
    }

    RenderObjectDirtyFlags PrimitiveRenderObject::diff(const RenderObject& previous) const {
        if (getRenderObjectType() != previous.getRenderObjectType()) {
            return RenderObjectDirtyFlags::All;
        }

        const auto& prev_prim = static_cast<const PrimitiveRenderObject&>(previous);
        RenderObjectDirtyFlags dirty_flags = RenderObjectDirtyFlags::None;

        if (m_mesh != prev_prim.m_mesh || m_section_index != prev_prim.m_section_index) {
            dirty_flags |= RenderObjectDirtyFlags::Mesh;
        }

        const Bool same_material_count = m_override_materials.size() == prev_prim.m_override_materials.size();
        Bool same_materials = same_material_count;
        if (same_materials) {
            for (Size_t material_index = 0; material_index < m_override_materials.size(); material_index++) {
                if (m_override_materials[material_index] != prev_prim.m_override_materials[material_index]) {
                    same_materials = false;
                    break;
                }
            }
        }
        if (!same_materials) {
            dirty_flags |= RenderObjectDirtyFlags::Materials;
        }

        if (m_mobility != prev_prim.m_mobility || m_visible != prev_prim.m_visible || m_cast_shadow != prev_prim.m_cast_shadow) {
            dirty_flags |= RenderObjectDirtyFlags::State;
        }

        return dirty_flags;
    }

    DynamicArray<PPtr<Material>> PrimitiveRenderObject::resolveMaterials() const {
        DynamicArray<PPtr<Material>> materials{};
        const auto* lod = activeLOD();
        if (!lod) {
            return materials;
        }

        materials.reserve(lod->sub_meshes.size());
        for (Size_t section_index = 0; section_index < lod->sub_meshes.size(); section_index++) {
            if (m_section_index >= 0 && static_cast<Int32>(section_index) != m_section_index) {
                continue;
            }

            PPtr<Material> material = lod->sub_meshes[section_index].material;
            const auto& override_material = section_index < m_override_materials.size() ? m_override_materials[section_index] : PPtr<Material>{};
            if (override_material.isValid() || !override_material.getLegacyPath().empty()) {
                material = override_material;
            }
            materials.push_back(material);
        }
        return materials;
    }

    DynamicArray<SubMesh> PrimitiveRenderObject::buildSections(const DynamicArray<PPtr<Material>>& resolved_materials) const {
        DynamicArray<SubMesh> sections{};
        const auto* lod = activeLOD();
        if (!lod) {
            return sections;
        }

        sections.reserve(lod->sub_meshes.size());
        for (Size_t section_index = 0; section_index < lod->sub_meshes.size(); section_index++) {
            if (m_section_index >= 0 && static_cast<Int32>(section_index) != m_section_index) {
                continue;
            }

            const auto& mesh_section = lod->sub_meshes[section_index];

            SubMesh section{};
            section.material = section_index < resolved_materials.size() ? resolved_materials[section_index] : mesh_section.material;
            section.index_offset = mesh_section.index_offset;
            section.vertex_offset = mesh_section.vertex_offset;
            section.index_count = mesh_section.index_count;
            section.vertex_count = mesh_section.vertex_count;
            section.section_index = mesh_section.section_index;
            section.primitive_type = mesh_section.primitive_type;
            sections.push_back(section);
        }

        return sections;
    }

    DynamicArray<MeshBatch> PrimitiveRenderObject::buildMeshBatches(
        const Identifier primitive_id,
        const DynamicArray<PPtr<Material>>& resolved_materials,
        const UInt32 first_instance) const
    {
        DynamicArray<MeshBatch> mesh_batches{};
        const auto* lod = activeLOD();
        if (!lod || !lod->isValid()) {
            return mesh_batches;
        }

        const auto& buffers = lod->buffers;
        mesh_batches.reserve(lod->sub_meshes.size());
        for (Size_t section_index = 0; section_index < lod->sub_meshes.size(); section_index++) {
            if (m_section_index >= 0 && static_cast<Int32>(section_index) != m_section_index) {
                continue;
            }

            const auto& mesh_section = lod->sub_meshes[section_index];
            if (mesh_section.index_count == 0) {
                continue;
            }

            MeshBatchElement element{};
            element.index_count = mesh_section.index_count;
            element.index_offset = mesh_section.index_offset;
            element.vertex_offset = mesh_section.vertex_offset;
            element.section_index = mesh_section.section_index;
            element.uses_instance_range = true;
            element.first_instance = first_instance;
            element.instance_count = 1;
            element.vertex_buffer = buffers.vertex_buffer;
            element.index_buffer = buffers.index_buffer;

            MeshBatch batch{};
            batch.primitive_id = primitive_id;
            batch.pass_mask.setRelevant(MeshPassType::GBuffer, m_visible);
            batch.pass_mask.setRelevant(MeshPassType::DirectionalShadow, m_visible && m_cast_shadow);
            batch.elements.push_back(element);
            mesh_batches.push_back(std::move(batch));
        }

        return mesh_batches;
    }

    PrimitiveSceneInfo PrimitiveRenderObject::buildSceneInfo(
        const Identifier primitive_id,
        const Matrix4f& world_transform,
        const Vector3f& bounds_min,
        const Vector3f& bounds_max) const
    {
        PrimitiveSceneInfo primitive(primitive_id);
        const auto materials = resolveMaterials();
        primitive.setSubMeshes(buildSections(materials));
        primitive.setMeshBatches(buildMeshBatches(primitive_id, materials, 0));
        primitive.setWorldTransform(world_transform);
        primitive.setBounds(bounds_min, bounds_max);
        primitive.setMobility(m_mobility);
        primitive.setVisible(m_visible);
        primitive.setCastShadow(m_cast_shadow);
        primitive.setInstanceCount(getInstanceCount());
        DynamicArray<InstanceSceneData> instance_data;
        appendInstanceSceneData(instance_data, world_transform);
        primitive.setInstanceSceneData(std::move(instance_data));
        return primitive;
    }

    bool PrimitiveRenderObject::createResources(DrawCommandList& cmd_list) {
        (void)cmd_list;
        return false;
    }

} // dodoe
