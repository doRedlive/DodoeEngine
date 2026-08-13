#include "mesh.h"

#include "runtime/core/math/math.h"
#include "runtime/function/graphics/draw_command_list.h"
#include "runtime/function/render/material/material.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/resource_manager.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

namespace dodoe {

    namespace {

        UnorderedMap<InstanceID, Scope<Mesh>> s_mesh_cache{};

        FileID ImportTexture(const FsPath& model_directory, const aiString& texture_path) {
            if (texture_path.length == 0 || texture_path.C_Str()[0] == '*') {
                return FileID();
            }

            FsPath resolved_path = FsPath(texture_path.C_Str());
            if (resolved_path.is_relative()) {
                resolved_path = model_directory / resolved_path;
            }
            resolved_path = resolved_path.lexically_normal();

            return FileID(String(resolved_path.string().c_str()));
        }

        FileID LoadMaterialTexture(
            const aiMaterial* material,
            const FsPath& model_directory,
            const aiTextureType primary_type,
            const aiTextureType fallback_type = aiTextureType_NONE) {
            if (!material) {
                return FileID();
            }

            for (const aiTextureType type : {primary_type, fallback_type}) {
                if (type == aiTextureType_NONE || material->GetTextureCount(type) == 0) {
                    continue;
                }

                aiString texture_path{};
                if (material->GetTexture(type, 0, &texture_path) != aiReturn_SUCCESS) {
                    continue;
                }

                const FileID texture_id = ImportTexture(model_directory, texture_path);
                if (texture_id.isValid()) {
                    return texture_id;
                }
            }

            return FileID();
        }

        MaterialProperties MakeMaterial(const aiScene* imported_scene, const aiMesh& source_mesh, const FsPath& model_directory) {
            MaterialProperties material{};

            if (!imported_scene || source_mesh.mMaterialIndex >= imported_scene->mNumMaterials) {
                return material;
            }

            const aiMaterial* source_material = imported_scene->mMaterials[source_mesh.mMaterialIndex];
            if (!source_material) {
                return material;
            }

            aiColor4D base_color{};
            if (aiGetMaterialColor(source_material, AI_MATKEY_BASE_COLOR, &base_color) == aiReturn_SUCCESS ||
                aiGetMaterialColor(source_material, AI_MATKEY_COLOR_DIFFUSE, &base_color) == aiReturn_SUCCESS) {
                material.color = {base_color.r, base_color.g, base_color.b, base_color.a};
            }

            material.base_color_texture = LoadMaterialTexture(
                source_material,
                model_directory,
                aiTextureType_BASE_COLOR,
                aiTextureType_DIFFUSE);
            material.normal_texture = LoadMaterialTexture(
                source_material,
                model_directory,
                aiTextureType_NORMALS,
                aiTextureType_NORMAL_CAMERA);
            material.emissive_texture = LoadMaterialTexture(
                source_material,
                model_directory,
                aiTextureType_EMISSIVE);

            return material;
        }

        struct MeshBuildResult {
            MeshUploadData upload_data{};
            DynamicArray<SubMesh> sub_meshes{};
            DynamicArray<MaterialProperties> material_props{};
        };

        MeshBuildResult BuildMeshData(const aiScene* imported_scene, const String& mesh_name, const FsPath& model_directory) {
            MeshBuildResult result{};
            if (!imported_scene) {
                return result;
            }

            result.upload_data.name = mesh_name;

            for (uint mesh_index = 0; mesh_index < imported_scene->mNumMeshes; ++mesh_index) {
                const aiMesh* source_mesh = imported_scene->mMeshes[mesh_index];
                if (!source_mesh) {
                    continue;
                }

                const uint vertex_base = static_cast<uint>(result.upload_data.position_data.size());

                const uint vertex_count = source_mesh->mNumVertices;
                uint index_count = 0;
                for (uint face_index = 0; face_index < source_mesh->mNumFaces; ++face_index) {
                    index_count += source_mesh->mFaces[face_index].mNumIndices;
                }

                result.upload_data.position_data.reserve(vertex_base + vertex_count);
                result.upload_data.normal_data.reserve(vertex_base + vertex_count);

                for (uint vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
                    const auto& position = source_mesh->mVertices[vertex_index];
                    result.upload_data.position_data.push_back({position.x, position.y, position.z});

                    if (source_mesh->HasNormals()) {
                        const auto& normal = source_mesh->mNormals[vertex_index];
                        const auto packed_normal = Math::PackSnorm4x8(Vector4f(normal.x, normal.y, normal.z, 0.0f));
                        result.upload_data.normal_data.push_back(packed_normal);
                    } else {
                        result.upload_data.normal_data.push_back(0);
                    }

                    if (source_mesh->HasTextureCoords(0)) {
                        const auto& uv = source_mesh->mTextureCoords[0][vertex_index];
                        result.upload_data.texcoord_data.push_back({uv.x, uv.y});
                    } else {
                        result.upload_data.texcoord_data.push_back(Vector2f(0.0f));
                    }
                }

                const uint index_base = static_cast<uint>(result.upload_data.index_data.size());
                result.upload_data.index_data.reserve(index_base + index_count);
                for (uint face_index = 0; face_index < source_mesh->mNumFaces; ++face_index) {
                    const auto& face = source_mesh->mFaces[face_index];
                    for (uint index_offset = 0; index_offset < face.mNumIndices; ++index_offset) {
                        result.upload_data.index_data.push_back(face.mIndices[index_offset] + vertex_base);
                    }
                }

                SubMesh section{};
                section.section_index = static_cast<Int32>(mesh_index);
                section.vertex_count = vertex_count;
                section.index_count = index_count;
                section.vertex_offset = vertex_base;
                section.index_offset = index_base;
                section.primitive_type = MeshGeometryPrimitiveType::Triangles;
                result.material_props.push_back(MakeMaterial(imported_scene, *source_mesh, model_directory));
                result.sub_meshes.push_back(std::move(section));
            }

            return result;
        }

    } // namespace

    Mesh* Mesh::Create(const ObjectID& ref, const String& path) {
        if (!ref.isValid() || path.empty()) {
            return nullptr;
        }

        AssetManager* asset_manager = ResourceManager::Self().getAssetManager();
        String absolute_path = path;
        if (asset_manager) {
            if (FsPath(path.c_str()).is_relative()) {
                absolute_path = String((asset_manager->getAssetDir() / FsPath(path.c_str())).generic_string().c_str());
            }
        }

        Assimp::Importer importer;
        const aiScene* imported_scene = importer.ReadFile(
            absolute_path.c_str(),
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices);

        if (!imported_scene || (imported_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !imported_scene->mRootNode) {
            return nullptr;
        }

        const String mesh_name(FsPath(path).stem().string().c_str());
        const FsPath model_directory = FsPath(absolute_path).parent_path();
        MeshBuildResult build = BuildMeshData(imported_scene, mesh_name, model_directory);

        if (build.upload_data.position_data.empty() || build.upload_data.index_data.empty()) {
            return nullptr;
        }

        Vector3f bounds_min = build.upload_data.position_data.front();
        Vector3f bounds_max = build.upload_data.position_data.front();
        for (const auto& position : build.upload_data.position_data) {
            bounds_min = Vector3f(
                (std::min)(bounds_min.x, position.x),
                (std::min)(bounds_min.y, position.y),
                (std::min)(bounds_min.z, position.z));
            bounds_max = Vector3f(
                (std::max)(bounds_max.x, position.x),
                (std::max)(bounds_max.y, position.y),
                (std::max)(bounds_max.z, position.z));
        }

        const Size_t vertex_count = build.upload_data.position_data.size();
        const Size_t index_count = build.upload_data.index_data.size();
        constexpr Size_t kVertexStride = sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f);
        const Size_t vertex_byte_size = kVertexStride * vertex_count;
        const Size_t index_byte_size = sizeof(UInt32) * index_count;

        DynamicArray<std::byte> vertex_bytes(vertex_byte_size);
        for (Size_t i = 0; i < vertex_count; ++i) {
            const Size_t base_offset = i * kVertexStride;
            std::memcpy(vertex_bytes.data() + base_offset, &build.upload_data.position_data[i], sizeof(Vector3f));

            const UInt32 normal = i < build.upload_data.normal_data.size() ? build.upload_data.normal_data[i] : 0;
            std::memcpy(vertex_bytes.data() + base_offset + sizeof(Vector3f), &normal, sizeof(UInt32));

            const Vector2f uv = i < build.upload_data.texcoord_data.size() ? build.upload_data.texcoord_data[i] : Vector2f(0.0f);
            std::memcpy(vertex_bytes.data() + base_offset + sizeof(Vector3f) + sizeof(UInt32), &uv, sizeof(Vector2f));
        }

        MeshLODData lod{};
        auto vertex_buffer_desc = GfxBufferDesc()
            .setByteSize(vertex_byte_size)
            .setIsVertexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::VertexBuffer)
            .setDebugName(fmt::format("Vertex Buffer {}", mesh_name));
        lod.buffers.vertex_buffer = GDrawCommandList.createBuffer(vertex_buffer_desc, vertex_bytes.data(), vertex_byte_size);

        auto index_buffer_desc = GfxBufferDesc()
            .setByteSize(index_byte_size)
            .setIsIndexBuffer(true)
            .enableAutomaticStateTracking(GfxResourceStates::IndexBuffer)
            .setDebugName(fmt::format("Index Buffer {}", mesh_name));
        lod.buffers.index_buffer = GDrawCommandList.createBuffer(index_buffer_desc, build.upload_data.index_data.data(), index_byte_size);

        lod.sub_meshes = std::move(build.sub_meshes);

        DynamicArray<MaterialProperties> unique_materials{};
        DynamicArray<UInt32> material_indices{};
        material_indices.reserve(build.material_props.size());
        for (const auto& props : build.material_props) {
            UInt32 material_index = static_cast<UInt32>(unique_materials.size());
            for (UInt32 i = 0; i < unique_materials.size(); ++i) {
                if (unique_materials[i] == props) {
                    material_index = i;
                    break;
                }
            }
            if (material_index == unique_materials.size()) {
                unique_materials.push_back(props);
            }
            material_indices.push_back(material_index);
        }

        for (Size_t section_index = 0; section_index < lod.sub_meshes.size(); section_index++) {
            const UInt32 material_index = section_index < material_indices.size() ? material_indices[section_index] : 0;
            const String file_name = String(fmt::format("{}_{}.domat", mesh_name, material_index).c_str());
            const String source_path = String(("materials/" + file_name).c_str());
            Material* material = ResourceManager::Self().loadObjectByPath<Material>(FileID(source_path));
            if (material) {
                lod.sub_meshes[section_index].material = PPtr<Material>(material);
            }
        }

        auto mesh = create_scope<Mesh>(ref);
        Mesh* raw = mesh.get();
        raw->setPath(absolute_path);
        raw->setName(mesh_name);
        DynamicArray<MeshLODData> lods{};
        lods.push_back(std::move(lod));
        raw->setLODData(lods);
        raw->setBounds(bounds_min, bounds_max);

        const InstanceID instance_id = raw->getInstanceID();
        s_mesh_cache.emplace(instance_id, std::move(mesh));
        return raw;
    }

    void Mesh::Shutdown() {
        s_mesh_cache.clear();
    }

} // namespace dodoe
