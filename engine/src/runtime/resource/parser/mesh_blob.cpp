// do@Redlive

#include "mesh_blob.h"

#include "runtime/core/utils/common.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/resource_manager.h"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include <fstream>

namespace dodoe {

    namespace {

        constexpr UInt32 kMeshCacheMagic = 0x48534D44;
        constexpr UInt32 kMeshCacheVersion = 1;
        constexpr UInt32 kMeshCacheMaxCount = 1u << 26;

        Matrix4f ToGlmMatrix(const aiMatrix4x4& matrix) {
            Matrix4f result;
            result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
            result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
            result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
            result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
            return result;
        }

        MeshVertex MakeMeshVertex(const aiMesh& mesh, unsigned int vertex_index) {
            MeshVertex vertex{};
            vertex.position = {
                mesh.mVertices[vertex_index].x,
                mesh.mVertices[vertex_index].y,
                mesh.mVertices[vertex_index].z,
            };
            if (mesh.HasNormals()) {
                vertex.normal = {
                    mesh.mNormals[vertex_index].x,
                    mesh.mNormals[vertex_index].y,
                    mesh.mNormals[vertex_index].z,
                };
            }
            if (mesh.mTextureCoords[0]) {
                vertex.tex_coords = {
                    mesh.mTextureCoords[0][vertex_index].x,
                    mesh.mTextureCoords[0][vertex_index].y,
                };
            }
            if (mesh.HasTangentsAndBitangents()) {
                vertex.tangent = {
                    mesh.mTangents[vertex_index].x,
                    mesh.mTangents[vertex_index].y,
                    mesh.mTangents[vertex_index].z,
                };
                vertex.bitangent = {
                    mesh.mBitangents[vertex_index].x,
                    mesh.mBitangents[vertex_index].y,
                    mesh.mBitangents[vertex_index].z,
                };
            }
            return vertex;
        }

        void BuildHierarchyNode(const aiNode& node, const Int32 parent_index, DynamicArray<MeshNode>& out) {
            const String node_name = node.mName.C_Str();
            MeshNode entry;
            entry.name = node_name;
            entry.parent_index = parent_index;

            aiVector3D scaling{};
            aiVector3D translation{};
            aiQuaternion rotation{};
            node.mTransformation.Decompose(scaling, rotation, translation);
            entry.position = {translation.x, translation.y, translation.z};
            const Quaternion quat(rotation.w, rotation.x, rotation.y, rotation.z);
            entry.rotation = Math::Degrees(Math::EulerAngles(quat));
            entry.scale = {scaling.x, scaling.y, scaling.z};
            entry.mesh_section_index = (node.mNumMeshes == 1) ? static_cast<Int32>(node.mMeshes[0]) : -1;

            const Int32 entry_index = static_cast<Int32>(out.size());
            out.push_back(std::move(entry));

            if (node.mNumMeshes > 1) {
                for (unsigned int i = 0; i < node.mNumMeshes; ++i) {
                    MeshNode sub;
                    sub.name = fmt::format("{}_Mesh{}", node_name, i);
                    sub.parent_index = entry_index;
                    sub.mesh_section_index = static_cast<Int32>(node.mMeshes[i]);
                    out.push_back(std::move(sub));
                }
            }

            for (unsigned int i = 0; i < node.mNumChildren; ++i) {
                if (node.mChildren[i]) {
                    BuildHierarchyNode(*node.mChildren[i], entry_index, out);
                }
            }
        }

        void CollectSectionWorlds(const aiNode& node, const Matrix4f& parent_world, UnorderedMap<UInt32, Matrix4f>& out_worlds) {
            const Matrix4f node_world = parent_world * ToGlmMatrix(node.mTransformation);
            for (unsigned int i = 0; i < node.mNumMeshes; ++i) {
                out_worlds[node.mMeshes[i]] = node_world;
            }
            for (unsigned int i = 0; i < node.mNumChildren; ++i) {
                if (node.mChildren[i]) {
                    CollectSectionWorlds(*node.mChildren[i], node_world, out_worlds);
                }
            }
        }

        FsPath ResolveTexturePath(const FsPath& model_directory, const aiString& texture_path, const FsPath& asset_dir) {
            FsPath resolved_path = FsPath(texture_path.C_Str());
            if (resolved_path.is_relative()) {
                resolved_path = model_directory / resolved_path;
            }
            resolved_path = resolved_path.lexically_normal();

            if (!asset_dir.empty()) {
                std::error_code ec;
                const FsPath relative_path = std::filesystem::relative(resolved_path, asset_dir, ec);
                const String relative_str = String(relative_path.generic_string().c_str());
                if (!ec && !relative_path.empty() && !relative_str.starts_with("..")) {
                    return relative_path;
                }
            }
            return resolved_path;
        }

        FileID ImportTexture(const FsPath& model_directory, const aiString& texture_path, const FsPath& asset_dir) {
            if (texture_path.length == 0 || texture_path.C_Str()[0] == '*') {
                return FileID();
            }

            const FsPath resolved_path = ResolveTexturePath(model_directory, texture_path, asset_dir);
            return FileID(String(resolved_path.generic_string().c_str()));
        }

        FileID LoadMaterialTexture(
            const aiMaterial* material,
            const FsPath& model_directory,
            const FsPath& asset_dir,
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

                const FileID texture_id = ImportTexture(model_directory, texture_path, asset_dir);
                if (texture_id.isValid()) {
                    return texture_id;
                }
            }

            return FileID();
        }

        MaterialProperties MakeMaterial(const aiScene* imported_scene, const aiMesh& source_mesh, const FsPath& model_directory, const FsPath& asset_dir) {
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
                asset_dir,
                aiTextureType_BASE_COLOR,
                aiTextureType_DIFFUSE);
            material.normal_texture = LoadMaterialTexture(
                source_material,
                model_directory,
                asset_dir,
                aiTextureType_NORMALS,
                aiTextureType_NORMAL_CAMERA);
            material.emissive_texture = LoadMaterialTexture(
                source_material,
                model_directory,
                asset_dir,
                aiTextureType_EMISSIVE);

            return material;
        }

    } // namespace

    String MakeMaterialAssetPath(const String& model_stem, const UInt32 material_index) {
        return String(fmt::format("materials/{}_{}.domat", model_stem, material_index).c_str());
    }

    Bool BuildMeshImport(
        const String& absolute_source_path,
        const FsPath& asset_dir,
        MeshBlob& out_blob,
        DynamicArray<MaterialProperties>& out_materials) {
        out_blob.free();
        out_materials.clear();

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            absolute_source_path.c_str(),
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices);

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene->mRootNode) {
            DO_ERROR("BuildMeshImport: {}", importer.GetErrorString());
            return false;
        }

        const FsPath model_directory = FsPath(absolute_source_path.c_str()).parent_path();
        const String model_stem = FileSystem::PathToNameNoExt(String(absolute_source_path.c_str()));

        BuildHierarchyNode(*scene->mRootNode, -1, out_blob.hierarchy);

        UnorderedMap<UInt32, Matrix4f> section_worlds{};
        CollectSectionWorlds(*scene->mRootNode, Matrix4f(1.0f), section_worlds);

        DynamicArray<MaterialProperties> materials{};
        DynamicArray<UInt32> mesh_material(static_cast<Size_t>(scene->mNumMeshes), 0);
        for (UInt32 mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index) {
            const aiMesh* source_mesh = scene->mMeshes[mesh_index];
            if (!source_mesh) {
                continue;
            }

            const MaterialProperties props = MakeMaterial(scene, *source_mesh, model_directory, asset_dir);
            UInt32 baked_index = static_cast<UInt32>(materials.size());
            for (Size_t i = 0; i < materials.size(); ++i) {
                if (materials[i] == props) {
                    baked_index = static_cast<UInt32>(i);
                    break;
                }
            }
            if (baked_index == materials.size()) {
                materials.push_back(props);
            }
            mesh_material[mesh_index] = baked_index;
        }

        auto data = create_ref<MeshData>();
        for (UInt32 mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index) {
            const aiMesh* source_mesh = scene->mMeshes[mesh_index];
            if (!source_mesh) {
                continue;
            }

            MeshSection section{};
            section.vertex_base = static_cast<UInt32>(data->vertices.size());
            section.vertex_count = source_mesh->mNumVertices;
            for (unsigned int vertex_index = 0; vertex_index < source_mesh->mNumVertices; ++vertex_index) {
                data->vertices.push_back(MakeMeshVertex(*source_mesh, vertex_index));
            }

            section.index_base = static_cast<UInt32>(data->indices.size());
            for (unsigned int face_index = 0; face_index < source_mesh->mNumFaces; ++face_index) {
                const aiFace& face = source_mesh->mFaces[face_index];
                for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                    data->indices.push_back(face.mIndices[j]);
                }
            }
            section.index_count = static_cast<UInt32>(data->indices.size()) - section.index_base;

            section.material_index = mesh_material[mesh_index];
            const auto world_it = section_worlds.find(mesh_index);
            if (world_it != section_worlds.end()) {
                section.world = world_it->second;
            }
            data->sections.push_back(section);
        }

        if (data->vertices.empty() || data->indices.empty() || data->sections.empty()) {
            return false;
        }

        out_blob.data = std::move(data);
        out_blob.material_paths.resize(materials.size());
        for (Size_t i = 0; i < materials.size(); ++i) {
            out_blob.material_paths[i] = MakeMaterialAssetPath(model_stem, static_cast<UInt32>(i));
        }
        out_materials = std::move(materials);
        return true;
    }

    Bool SaveMeshCache(const String& absolute_cache_path, const MeshBlob& blob) {
        if (!blob.isValid()) {
            return false;
        }

        std::ofstream stream(absolute_cache_path.c_str(), std::ios::binary | std::ios::trunc);
        if (!stream) {
            return false;
        }

        const auto write_u32 = [&stream](UInt32 value) {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(UInt32));
        };
        const auto write_i32 = [&stream](Int32 value) {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(Int32));
        };
        const auto write_f32 = [&stream](Float value) {
            stream.write(reinterpret_cast<const char*>(&value), sizeof(Float));
        };
        const auto write_string = [&stream, &write_u32](const String& value) {
            write_u32(static_cast<UInt32>(value.size()));
            stream.write(value.data(), static_cast<std::streamsize>(value.size()));
        };

        write_u32(kMeshCacheMagic);
        write_u32(kMeshCacheVersion);
        write_u32(static_cast<UInt32>(blob.data->vertices.size()));
        write_u32(static_cast<UInt32>(blob.data->indices.size()));
        write_u32(static_cast<UInt32>(blob.data->sections.size()));
        write_u32(static_cast<UInt32>(blob.material_paths.size()));
        write_u32(static_cast<UInt32>(blob.hierarchy.size()));

        if (!blob.data->vertices.empty()) {
            stream.write(
                reinterpret_cast<const char*>(blob.data->vertices.data()),
                static_cast<std::streamsize>(blob.data->vertices.size() * sizeof(MeshVertex)));
        }
        if (!blob.data->indices.empty()) {
            stream.write(
                reinterpret_cast<const char*>(blob.data->indices.data()),
                static_cast<std::streamsize>(blob.data->indices.size() * sizeof(UInt32)));
        }

        for (const MeshSection& section : blob.data->sections) {
            write_u32(section.vertex_base);
            write_u32(section.vertex_count);
            write_u32(section.index_base);
            write_u32(section.index_count);
            write_u32(section.material_index);
            for (UInt32 column = 0; column < 4; ++column) {
                for (UInt32 row = 0; row < 4; ++row) {
                    write_f32(section.world[column][row]);
                }
            }
        }

        for (const String& material_path : blob.material_paths) {
            write_string(material_path);
        }

        for (const MeshNode& node : blob.hierarchy) {
            write_string(node.name);
            write_f32(node.position.x);
            write_f32(node.position.y);
            write_f32(node.position.z);
            write_f32(node.rotation.x);
            write_f32(node.rotation.y);
            write_f32(node.rotation.z);
            write_f32(node.scale.x);
            write_f32(node.scale.y);
            write_f32(node.scale.z);
            write_i32(node.parent_index);
            write_i32(node.mesh_section_index);
        }

        return stream.good();
    }

    Bool LoadMeshCache(const String& absolute_cache_path, MeshBlob& out_blob) {
        out_blob.free();

        std::ifstream stream(absolute_cache_path.c_str(), std::ios::binary);
        if (!stream) {
            return false;
        }

        const auto read_value = [&stream](auto& value) -> Bool {
            return static_cast<Bool>(stream.read(reinterpret_cast<char*>(&value), sizeof(value)));
        };
        const auto read_string = [&stream, &read_value](String& value) -> Bool {
            UInt32 length = 0;
            if (!read_value(length) || length > kMeshCacheMaxCount) {
                return false;
            }
            value.resize(length);
            return static_cast<Bool>(stream.read(value.data(), static_cast<std::streamsize>(length)));
        };

        UInt32 magic = 0;
        UInt32 version = 0;
        UInt32 vertex_count = 0;
        UInt32 index_count = 0;
        UInt32 section_count = 0;
        UInt32 material_count = 0;
        UInt32 hierarchy_count = 0;
        if (!read_value(magic) || !read_value(version) ||
            !read_value(vertex_count) || !read_value(index_count) ||
            !read_value(section_count) || !read_value(material_count) || !read_value(hierarchy_count)) {
            return false;
        }
        if (magic != kMeshCacheMagic || version != kMeshCacheVersion) {
            return false;
        }
        if (vertex_count > kMeshCacheMaxCount || index_count > kMeshCacheMaxCount ||
            section_count > kMeshCacheMaxCount || material_count > kMeshCacheMaxCount ||
            hierarchy_count > kMeshCacheMaxCount) {
            return false;
        }

        auto data = create_ref<MeshData>();
        data->vertices.resize(vertex_count);
        if (vertex_count > 0 &&
            !stream.read(
                reinterpret_cast<char*>(data->vertices.data()),
                static_cast<std::streamsize>(vertex_count * sizeof(MeshVertex)))) {
            return false;
        }
        data->indices.resize(index_count);
        if (index_count > 0 &&
            !stream.read(
                reinterpret_cast<char*>(data->indices.data()),
                static_cast<std::streamsize>(index_count * sizeof(UInt32)))) {
            return false;
        }

        data->sections.resize(section_count);
        for (MeshSection& section : data->sections) {
            if (!read_value(section.vertex_base) || !read_value(section.vertex_count) ||
                !read_value(section.index_base) || !read_value(section.index_count) ||
                !read_value(section.material_index)) {
                return false;
            }
            for (UInt32 column = 0; column < 4; ++column) {
                for (UInt32 row = 0; row < 4; ++row) {
                    if (!read_value(section.world[column][row])) {
                        return false;
                    }
                }
            }
        }

        out_blob.material_paths.resize(material_count);
        for (String& material_path : out_blob.material_paths) {
            if (!read_string(material_path)) {
                return false;
            }
        }

        out_blob.hierarchy.resize(hierarchy_count);
        for (MeshNode& node : out_blob.hierarchy) {
            if (!read_string(node.name) ||
                !read_value(node.position.x) || !read_value(node.position.y) || !read_value(node.position.z) ||
                !read_value(node.rotation.x) || !read_value(node.rotation.y) || !read_value(node.rotation.z) ||
                !read_value(node.scale.x) || !read_value(node.scale.y) || !read_value(node.scale.z) ||
                !read_value(node.parent_index) || !read_value(node.mesh_section_index)) {
                return false;
            }
        }

        if (!stream) {
            return false;
        }

        out_blob.data = std::move(data);
        return true;
    }

    MeshBlob::~MeshBlob() {
        if (isValid()) {
            free();
        }
    }

    void MeshBlob::load(const String& path, const UUID& asset_id) {
        free();
        (void)asset_id;

        const String cache_path = String((path + ".domesh").c_str());
        if (LoadMeshCache(cache_path, *this)) {
            return;
        }

        FsPath asset_dir{};
        if (AssetManager* asset_manager = ResourceManager::Self().getAssetManager()) {
            asset_dir = asset_manager->getAssetDir();
        }

        DynamicArray<MaterialProperties> materials{};
        if (!BuildMeshImport(path, asset_dir, *this, materials)) {
            free();
        }
    }

    void MeshBlob::free() {
        data.reset();
        hierarchy.clear();
        material_paths.clear();
    }

} // namespace dodoe
