// do@Redlive

#include "mesh.h"
#include "runtime/function/render/render_command_queue.h"

#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/material/material.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/types/mesh_asset.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/resource_manager.h"

#include <algorithm>

namespace dodoe {

    namespace {

        UnorderedMap<InstanceID, Scope<Mesh>> s_mesh_cache{};

    } // namespace

    Mesh::~Mesh() = default;

    Mesh* Mesh::Create(const ObjectID& ref, MeshAsset& asset) {
        if (!ref.isValid()) {
            return nullptr;
        }

        const Ref<MeshData> data = asset.getData();
        const MeshBlob& blob = asset.getBlob();
        if (!data || data->vertices.empty() || data->indices.empty() || data->sections.empty()) {
            DO_ERROR("Mesh::Create: no geometry parsed from '{}'", asset.getSourcePath());
            return nullptr;
        }

        const String mesh_name = asset.getName();

        Vector3f bounds_min = data->vertices.front().position;
        Vector3f bounds_max = data->vertices.front().position;
        for (const MeshVertex& vertex : data->vertices) {
            bounds_min = Vector3f(
                (std::min)(bounds_min.x, vertex.position.x),
                (std::min)(bounds_min.y, vertex.position.y),
                (std::min)(bounds_min.z, vertex.position.z));
            bounds_max = Vector3f(
                (std::max)(bounds_max.x, vertex.position.x),
                (std::max)(bounds_max.y, vertex.position.y),
                (std::max)(bounds_max.z, vertex.position.z));
        }

        const Size_t index_count = data->indices.size();
        const Size_t index_byte_size = sizeof(UInt32) * index_count;

        DynamicArray<UInt8> vertex_bytes{};
        PackVertexBytes(data->vertices, vertex_bytes);
        const Size_t vertex_byte_size = vertex_bytes.size();

        MeshLODData lod{};
        auto vertex_buffer_desc = GfxBufferDesc()
            .setByteSize(vertex_byte_size)
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
            .setDebugName(fmt::format("Vertex Buffer {}", mesh_name));
        lod.buffers.vertex_buffer = RenderResourceQueue::CreateBuffer(vertex_buffer_desc, vertex_bytes.data(), vertex_byte_size);

        auto index_buffer_desc = GfxBufferDesc()
            .setByteSize(index_byte_size)
            .setIsIndexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::IndexBuffer)
            .setDebugName(fmt::format("Index Buffer {}", mesh_name));
        lod.buffers.index_buffer = RenderResourceQueue::CreateBuffer(index_buffer_desc, data->indices.data(), index_byte_size);

        lod.sub_meshes.reserve(data->sections.size());
        for (Size_t section_index = 0; section_index < data->sections.size(); ++section_index) {
            const MeshSection& source_section = data->sections[section_index];

            SubMesh section{};
            section.section_index = static_cast<Int32>(section_index);
            section.vertex_offset = source_section.vertex_base;
            section.vertex_count = source_section.vertex_count;
            section.index_offset = source_section.index_base;
            section.index_count = source_section.index_count;
            section.primitive_type = MeshGeometryPrimitiveType::Triangles;

            if (source_section.material_index < blob.material_paths.size()) {
                const String& material_path = blob.material_paths[source_section.material_index];
                if (Material* material = ResourceManager::Self().loadObjectByPath<Material>(FileID(material_path))) {
                    section.material = PPtr<Material>(material);
                } else {
                    DO_WARN("Mesh: material '{}' not loadable for section {}", material_path, section_index);
                }
            }

            lod.sub_meshes.push_back(std::move(section));
        }

        auto mesh = create_scope<Mesh>(ref);
        Mesh* raw = mesh.get();
        raw->setPath(asset.getSourcePath());
        raw->setName(mesh_name);
        DynamicArray<MeshLODData> lods{};
        lods.push_back(std::move(lod));
        raw->setLODData(lods);
        raw->setBounds(bounds_min, bounds_max);
        raw->setSourceData(data);
        raw->setSkeleton(data->skeleton);
        for (const MeshVertex& vertex : data->vertices) {
            if (vertex.bone_weights[0] > 0.0f || vertex.bone_weights[1] > 0.0f ||
                vertex.bone_weights[2] > 0.0f || vertex.bone_weights[3] > 0.0f) {
                raw->setSkinned(true);
                break;
            }
        }

        const InstanceID instance_id = raw->getInstanceID();
        s_mesh_cache.emplace(instance_id, std::move(mesh));
        return raw;
    }

    void Mesh::Shutdown() {
        s_mesh_cache.clear();
    }

} // namespace dodoe
