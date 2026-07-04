// do@Redlive

#include "primitive_render_object.h"
#include "runtime/function/graphics/draw_command_list.h"

namespace dodoe {

    const MeshLODData* PrimitiveRenderObject::activeLOD() const {
        if (m_lods.empty()) return nullptr;
        return &m_lods[0];
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

    RenderObjectDirtyFlags PrimitiveRenderObject::diff(const RenderObject& previous) const {
        if (getRenderObjectType() != previous.getRenderObjectType()) {
            return RenderObjectDirtyFlags::All;
        }

        const auto& prev_prim = static_cast<const PrimitiveRenderObject&>(previous);
        RenderObjectDirtyFlags dirty_flags = RenderObjectDirtyFlags::None;

        const Bool lods_changed = m_lods.size() != prev_prim.m_lods.size();
        if (lods_changed) {
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

    DynamicArray<Ref<Material>> PrimitiveRenderObject::resolveMaterials() const {
        DynamicArray<Ref<Material>> materials{};
        const auto* lod = activeLOD();
        if (!lod) {
            return materials;
        }

        materials.reserve(lod->sections.size());
        for (Size_t section_index = 0; section_index < lod->sections.size(); section_index++) {
            Ref<Material> material = lod->sections[section_index].material;
            if (section_index < m_override_materials.size() && m_override_materials[section_index]) {
                material = m_override_materials[section_index];
            }
            materials.push_back(material);
        }
        return materials;
    }

    DynamicArray<PrimitiveSceneInfo::Section> PrimitiveRenderObject::buildSections(const DynamicArray<Ref<Material>>& resolved_materials) const {
        DynamicArray<PrimitiveSceneInfo::Section> sections{};
        const auto* lod = activeLOD();
        if (!lod) {
            return sections;
        }

        sections.reserve(lod->sections.size());
        for (Size_t section_index = 0; section_index < lod->sections.size(); section_index++) {
            const auto& mesh_section = lod->sections[section_index];

            PrimitiveSceneInfo::Section section{};
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
        const DynamicArray<Ref<Material>>& resolved_materials,
        const UInt32 first_instance) const
    {
        DynamicArray<MeshBatch> mesh_batches{};
        const auto* lod = activeLOD();
        if (!lod || !lod->isValid()) {
            return mesh_batches;
        }

        const auto& buffers = lod->buffers;
        mesh_batches.reserve(lod->sections.size());
        for (Size_t section_index = 0; section_index < lod->sections.size(); section_index++) {
            const auto& mesh_section = lod->sections[section_index];
            if (mesh_section.index_count == 0) {
                continue;
            }

            MeshBatchElement element{};
            element.index_count = mesh_section.index_count;
            element.index_offset = mesh_section.index_offset;
            element.vertex_offset = mesh_section.vertex_offset;
            element.section_index = static_cast<UInt32>(section_index);
            element.uses_instance_range = true;
            element.first_instance = first_instance;
            element.instance_count = 1;
            element.vertex_buffer = buffers.vertex_buffer;
            element.index_buffer = buffers.index_buffer;

            MeshBatch batch{};
            batch.primitive_id = primitive_id;
            batch.material = section_index < resolved_materials.size() ? resolved_materials[section_index] : mesh_section.material;
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
        primitive.setSections(buildSections(materials));
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

    void PrimitiveRenderObject::createResources(DrawCommandList& cmd_list) {
        if (m_upload_data.position_data.empty() || m_upload_data.index_data.empty()) {
            return;
        }

        const auto vertex_count = m_upload_data.position_data.size();
        const auto index_count = m_upload_data.index_data.size();
        constexpr Size_t kVertexStride = sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f);
        const Size_t vertex_byte_size = kVertexStride * vertex_count;
        const Size_t index_byte_size = sizeof(UInt32) * index_count;

        GfxBufferHandle& vb = m_lods.empty() ? m_lods.emplace_back().buffers.vertex_buffer : m_lods[0].buffers.vertex_buffer;
        GfxBufferHandle& ib = m_lods.empty() ? m_lods.emplace_back().buffers.index_buffer : m_lods[0].buffers.index_buffer;

        if (vertex_count > 0) {
            auto vertex_buffer_desc = GfxBufferDesc()
                .setByteSize(vertex_byte_size)
                .setIsVertexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
                .setDebugName(fmt::format("Vertex Buffer {}", m_upload_data.name));

            DynamicArray<std::byte> vertex_bytes(vertex_byte_size);
            for (Size_t i = 0; i < vertex_count; ++i) {
                const Size_t base_offset = i * kVertexStride;
                std::memcpy(vertex_bytes.data() + base_offset, &m_upload_data.position_data[i], sizeof(Vector3f));

                const UInt32 normal = i < m_upload_data.normal_data.size() ?
                    m_upload_data.normal_data[i] : 0;
                std::memcpy(vertex_bytes.data() + base_offset + sizeof(Vector3f), &normal, sizeof(UInt32));

                const Vector2f uv = i < m_upload_data.texcoord_data.size() ?
                    m_upload_data.texcoord_data[i] : Vector2f(0.0f);
                std::memcpy(vertex_bytes.data() + base_offset + sizeof(Vector3f) + sizeof(UInt32), &uv, sizeof(Vector2f));
            }
            vb = cmd_list.createBuffer(vertex_buffer_desc, vertex_bytes.data(), vertex_byte_size);
        }

        if (index_count > 0) {
            auto index_buffer_desc = GfxBufferDesc()
                .setByteSize(index_byte_size)
                .setIsIndexBuffer(true)
                .enableAutomaticStateTracking(GfxResourceStates::IndexBuffer)
                .setDebugName(fmt::format("Index Buffer {}", m_upload_data.name));
            ib = cmd_list.createBuffer(index_buffer_desc, m_upload_data.index_data.data(), index_byte_size);
        }
    }

} // dodoe
