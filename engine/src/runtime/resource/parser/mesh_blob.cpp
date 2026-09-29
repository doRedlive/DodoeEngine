// do@Redlive

#include "mesh_blob.h"

#include "runtime/core/utils/common.h"
#include "runtime/core/math/math.h"
#include "runtime/resource/file/file_system.h"
#include "runtime/resource/resource_manager.h"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include <cstring>
#include <fstream>

namespace dodoe {

    namespace {

        constexpr UInt32 kMeshCacheMagic = 0x48534D44;
        constexpr UInt32 kMeshCacheVersion = 2;
        constexpr UInt32 kMeshCacheMaxCount = 1u << 26;
        constexpr Size_t kVertexStride = sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f) + sizeof(UInt32) * 4 + sizeof(Vector4f);

    } // namespace

    void MeshBlob::VertexBoneData::add(const UInt32 bone_index, const Float weight) {
        if (weight <= 0.0f) {
            return;
        }
        for (UInt32 i = 0; i < kMaxInfluences; ++i) {
            if (weights[i] <= 0.0f) {
                ids[i] = bone_index;
                weights[i] = weight;
                return;
            }
        }
        UInt32 min_index = 0;
        for (UInt32 i = 1; i < kMaxInfluences; ++i) {
            if (weights[i] < weights[min_index]) {
                min_index = i;
            }
        }
        if (weight > weights[min_index]) {
            ids[min_index] = bone_index;
            weights[min_index] = weight;
        }
    }

    void MeshBlob::VertexBoneData::normalize() {
        Float total = 0.0f;
        for (UInt32 i = 0; i < kMaxInfluences; ++i) {
            total += weights[i];
        }
        if (total <= 0.0f) {
            return;
        }
        for (UInt32 i = 0; i < kMaxInfluences; ++i) {
            weights[i] /= total;
        }
    }

    MeshVertex MeshBlob::MakeMeshVertex(const aiMesh& mesh, const unsigned int vertex_index, const DynamicArray<VertexBoneData>& bone_data) {
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
        if (vertex_index < bone_data.size()) {
            const VertexBoneData& bones = bone_data[vertex_index];
            for (UInt32 i = 0; i < VertexBoneData::kMaxInfluences; ++i) {
                vertex.bone_ids[i] = bones.ids[i];
                vertex.bone_weights[i] = bones.weights[i];
            }
        }
        return vertex;
    }

    void MeshBlob::BuildHierarchyNode(const aiNode& node, const Int32 parent_index, DynamicArray<MeshNode>& out) {
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

    void MeshBlob::CollectSectionWorlds(const aiNode& node, const Matrix4f& parent_world, UnorderedMap<UInt32, Matrix4f>& out_worlds) {
        const aiMatrix4x4& source = node.mTransformation;
        Matrix4f node_transform;
        node_transform[0][0] = source.a1; node_transform[1][0] = source.a2; node_transform[2][0] = source.a3; node_transform[3][0] = source.a4;
        node_transform[0][1] = source.b1; node_transform[1][1] = source.b2; node_transform[2][1] = source.b3; node_transform[3][1] = source.b4;
        node_transform[0][2] = source.c1; node_transform[1][2] = source.c2; node_transform[2][2] = source.c3; node_transform[3][2] = source.c4;
        node_transform[0][3] = source.d1; node_transform[1][3] = source.d2; node_transform[2][3] = source.d3; node_transform[3][3] = source.d4;

        const Matrix4f node_world = parent_world * node_transform;
        for (unsigned int i = 0; i < node.mNumMeshes; ++i) {
            out_worlds[node.mMeshes[i]] = node_world;
        }
        for (unsigned int i = 0; i < node.mNumChildren; ++i) {
            if (node.mChildren[i]) {
                CollectSectionWorlds(*node.mChildren[i], node_world, out_worlds);
            }
        }
    }

    MaterialProperties MeshBlob::MakeMaterial(const aiScene* imported_scene, const aiMesh& source_mesh, const FsPath& model_directory, const FsPath& asset_dir) {
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

        auto load_texture = [&](const aiTextureType primary_type, const aiTextureType fallback_type) -> FileID {
            for (const aiTextureType type : {primary_type, fallback_type}) {
                if (type == aiTextureType_NONE || source_material->GetTextureCount(type) == 0) {
                    continue;
                }
                aiString texture_path{};
                if (source_material->GetTexture(type, 0, &texture_path) != aiReturn_SUCCESS) {
                    continue;
                }
                if (texture_path.length == 0 || texture_path.C_Str()[0] == '*') {
                    continue;
                }

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
                        return FileID(String(relative_path.generic_string().c_str()));
                    }
                }
                return FileID(String(resolved_path.generic_string().c_str()));
            }
            return FileID();
        };

        material.base_color_texture = load_texture(aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE);
        material.normal_texture = load_texture(aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA);
        material.emissive_texture = load_texture(aiTextureType_EMISSIVE, aiTextureType_NONE);

        return material;
    }

    Int32 MeshBlob::BuildSkeletonNodes(const aiNode& node, const Int32 parent_index, const UnorderedSet<String>& bone_names, DynamicArray<SkeletonNode>& out_nodes) {
        const String node_name = node.mName.C_Str();
        const bool is_bone = bone_names.find(node_name) != bone_names.end();
        const bool has_bone_descendant = [&node, &bone_names]() {
            DynamicArray<const aiNode*> stack{};
            for (unsigned int i = 0; i < node.mNumChildren; ++i) {
                if (node.mChildren[i]) {
                    stack.push_back(node.mChildren[i]);
                }
            }
            while (!stack.empty()) {
                const aiNode* current = stack.back();
                stack.pop_back();
                if (bone_names.find(String(current->mName.C_Str())) != bone_names.end()) {
                    return true;
                }
                for (unsigned int i = 0; i < current->mNumChildren; ++i) {
                    if (current->mChildren[i]) {
                        stack.push_back(current->mChildren[i]);
                    }
                }
            }
            return false;
        }();

        Int32 node_index = -1;
        if (is_bone || has_bone_descendant || out_nodes.empty()) {
            SkeletonNode skeleton_node;
            skeleton_node.name = node_name;
            skeleton_node.parent = parent_index;

            aiVector3D scaling{};
            aiVector3D translation{};
            aiQuaternion rotation{};
            node.mTransformation.Decompose(scaling, rotation, translation);
            skeleton_node.bind_pose.position = {translation.x, translation.y, translation.z};
            skeleton_node.bind_pose.rotation = Quaternion(rotation.w, rotation.x, rotation.y, rotation.z);
            skeleton_node.bind_pose.scale = {scaling.x, scaling.y, scaling.z};

            node_index = static_cast<Int32>(out_nodes.size());
            out_nodes.push_back(std::move(skeleton_node));
        }

        for (unsigned int i = 0; i < node.mNumChildren; ++i) {
            if (node.mChildren[i]) {
                BuildSkeletonNodes(*node.mChildren[i], node_index, bone_names, out_nodes);
            }
        }
        return node_index;
    }

    void MeshBlob::ImportAnimations(const aiScene& scene, const UnorderedMap<String, Int32>& node_indices, DynamicArray<MeshAnimClipData>& out_clips) {
        out_clips.reserve(out_clips.size() + scene.mNumAnimations);
        for (unsigned int anim_index = 0; anim_index < scene.mNumAnimations; ++anim_index) {
            const aiAnimation* source_animation = scene.mAnimations[anim_index];
            if (!source_animation) {
                continue;
            }

            const double ticks_per_second = source_animation->mTicksPerSecond > 0.0
                ? source_animation->mTicksPerSecond
                : 24.0;

            MeshAnimClipData clip_data;
            clip_data.name = source_animation->mName.length > 0
                ? String(source_animation->mName.C_Str())
                : String(fmt::format("clip_{}", anim_index).c_str());
            clip_data.duration = static_cast<Float>(source_animation->mDuration / ticks_per_second);
            clip_data.loop = true;

            for (unsigned int channel_index = 0; channel_index < source_animation->mNumChannels; ++channel_index) {
                const aiNodeAnim* source_channel = source_animation->mChannels[channel_index];
                if (!source_channel) {
                    continue;
                }
                const auto bone_it = node_indices.find(String(source_channel->mNodeName.C_Str()));
                if (bone_it == node_indices.end() || bone_it->second < 0) {
                    continue;
                }

                AnimBoneChannel3D channel{};
                channel.bone = bone_it->second;

                const Float inverse_ticks = static_cast<Float>(1.0 / ticks_per_second);
                for (unsigned int key_index = 0; key_index < source_channel->mNumPositionKeys; ++key_index) {
                    const aiVectorKey& key = source_channel->mPositionKeys[key_index];
                    channel.position_times.push_back(static_cast<Float>(key.mTime) * inverse_ticks);
                    channel.positions.emplace_back(key.mValue.x, key.mValue.y, key.mValue.z);
                }
                for (unsigned int key_index = 0; key_index < source_channel->mNumRotationKeys; ++key_index) {
                    const aiQuatKey& key = source_channel->mRotationKeys[key_index];
                    channel.rotation_times.push_back(static_cast<Float>(key.mTime) * inverse_ticks);
                    channel.rotations.emplace_back(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z);
                }
                for (unsigned int key_index = 0; key_index < source_channel->mNumScalingKeys; ++key_index) {
                    const aiVectorKey& key = source_channel->mScalingKeys[key_index];
                    channel.scale_times.push_back(static_cast<Float>(key.mTime) * inverse_ticks);
                    channel.scales.emplace_back(key.mValue.x, key.mValue.y, key.mValue.z);
                }

                if (!channel.position_times.empty() || !channel.rotation_times.empty() || !channel.scale_times.empty()) {
                    clip_data.channels.push_back(std::move(channel));
                }
            }

            if (!clip_data.channels.empty()) {
                out_clips.push_back(std::move(clip_data));
            }
        }
    }

    void MeshBlob::bindSkeletalObjects(const UUID& asset_id) {
        if (!data || skeleton_nodes.empty()) {
            return;
        }

        if (Skeleton* skeleton = Skeleton::Create(ObjectID{asset_id, Skeleton::kLocalId})) {
            skeleton->clear();
            for (const SkeletonNode& node : skeleton_nodes) {
                skeleton->addNode(node.name, node.parent, node.bind_pose);
            }
            data->skeleton = PPtr<Skeleton>(skeleton);
        }

        for (Size_t i = 0; i < clips.size(); ++i) {
            AnimClip* clip = AnimClip::Create(ObjectID{asset_id, AnimClip::kLocalIdBase + static_cast<UInt32>(i)});
            if (!clip) {
                continue;
            }
            clip->clear();
            clip->name = clips[i].name;
            clip->duration = clips[i].duration;
            clip->loop = clips[i].loop;
            clip->channels = clips[i].channels;
            data->animations.push_back(PPtr<AnimClip>(clip));
        }
    }

    Bool MeshBlob::BuildMeshImport(
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

        out_blob.BuildHierarchyNode(*scene->mRootNode, -1, out_blob.hierarchy);

        UnorderedMap<UInt32, Matrix4f> section_worlds{};
        out_blob.CollectSectionWorlds(*scene->mRootNode, Matrix4f(1.0f), section_worlds);

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

        UnorderedSet<String> bone_names{};
        for (UInt32 mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index) {
            const aiMesh* source_mesh = scene->mMeshes[mesh_index];
            if (!source_mesh) {
                continue;
            }
            for (unsigned int i = 0; i < source_mesh->mNumBones; ++i) {
                const aiBone* bone = source_mesh->mBones[i];
                if (bone) {
                    bone_names.emplace(String(bone->mName.C_Str()));
                }
            }
        }

        UnorderedMap<String, Int32> node_indices{};
        if (!bone_names.empty() && scene->mRootNode) {
            BuildSkeletonNodes(*scene->mRootNode, -1, bone_names, out_blob.skeleton_nodes);
            for (Size_t i = 0; i < out_blob.skeleton_nodes.size(); ++i) {
                node_indices.emplace(out_blob.skeleton_nodes[i].name, static_cast<Int32>(i));
            }
            ImportAnimations(*scene, node_indices, out_blob.clips);
        }

        auto mesh_data = create_ref<MeshData>();
        for (UInt32 mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index) {
            const aiMesh* source_mesh = scene->mMeshes[mesh_index];
            if (!source_mesh) {
                continue;
            }

            DynamicArray<VertexBoneData> bone_data;
            if (!node_indices.empty() && source_mesh->mNumBones > 0) {
                bone_data.assign(source_mesh->mNumVertices, VertexBoneData{});
                for (unsigned int bone_index = 0; bone_index < source_mesh->mNumBones; ++bone_index) {
                    const aiBone* bone = source_mesh->mBones[bone_index];
                    if (!bone) {
                        continue;
                    }
                    const auto node_it = node_indices.find(String(bone->mName.C_Str()));
                    if (node_it == node_indices.end() || node_it->second < 0) {
                        continue;
                    }
                    const UInt32 skeleton_index = static_cast<UInt32>(node_it->second);
                    for (unsigned int weight_index = 0; weight_index < bone->mNumWeights; ++weight_index) {
                        const aiVertexWeight& vertex_weight = bone->mWeights[weight_index];
                        if (vertex_weight.mVertexId >= source_mesh->mNumVertices) {
                            continue;
                        }
                        bone_data[vertex_weight.mVertexId].add(skeleton_index, vertex_weight.mWeight);
                    }
                }
                for (VertexBoneData& vertex_bones : bone_data) {
                    vertex_bones.normalize();
                }
            }

            MeshSection section{};
            section.vertex_base = static_cast<UInt32>(mesh_data->vertices.size());
            section.vertex_count = source_mesh->mNumVertices;
            for (unsigned int vertex_index = 0; vertex_index < source_mesh->mNumVertices; ++vertex_index) {
                mesh_data->vertices.push_back(MakeMeshVertex(*source_mesh, vertex_index, bone_data));
            }

            section.index_base = static_cast<UInt32>(mesh_data->indices.size());
            for (unsigned int face_index = 0; face_index < source_mesh->mNumFaces; ++face_index) {
                const aiFace& face = source_mesh->mFaces[face_index];
                for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                    mesh_data->indices.push_back(face.mIndices[j]);
                }
            }
            section.index_count = static_cast<UInt32>(mesh_data->indices.size()) - section.index_base;

            section.material_index = mesh_material[mesh_index];
            const auto world_it = section_worlds.find(mesh_index);
            if (world_it != section_worlds.end()) {
                section.world = world_it->second;
            }
            mesh_data->sections.push_back(section);
        }

        if (mesh_data->vertices.empty() || mesh_data->indices.empty() || mesh_data->sections.empty()) {
            return false;
        }

        out_blob.data = std::move(mesh_data);
        out_blob.material_paths.resize(materials.size());
        for (Size_t i = 0; i < materials.size(); ++i) {
            out_blob.material_paths[i] = MakeMaterialAssetPath(model_stem, static_cast<UInt32>(i));
        }
        out_materials = std::move(materials);
        return true;
    }

    MeshBlob::~MeshBlob() {
        if (isValid()) {
            free();
        }
    }

    void MeshBlob::load(const String& path, const UUID& asset_id) {
        free();

        const String cache_path = String((path + ".domesh").c_str());
        if (!LoadMeshCache(cache_path, *this)) {
            FsPath asset_dir{};
            if (AssetManager* asset_manager = ResourceManager::Self().getAssetManager()) {
                asset_dir = asset_manager->getAssetDir();
            }

            DynamicArray<MaterialProperties> materials{};
            if (!BuildMeshImport(path, asset_dir, *this, materials)) {
                free();
                return;
            }
        }

        if (asset_id.isValid()) {
            bindSkeletalObjects(asset_id);
        }
    }

    void MeshBlob::free() {
        data.reset();
        hierarchy.clear();
        material_paths.clear();
        skeleton_nodes.clear();
        clips.clear();
    }

    String MakeMaterialAssetPath(const String& model_stem, const UInt32 material_index) {
        return String(fmt::format("materials/{}_{}.domat", model_stem, material_index).c_str());
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

        write_u32(static_cast<UInt32>(blob.skeleton_nodes.size()));
        for (const SkeletonNode& node : blob.skeleton_nodes) {
            write_string(node.name);
            write_i32(node.parent);
            write_f32(node.bind_pose.position.x);
            write_f32(node.bind_pose.position.y);
            write_f32(node.bind_pose.position.z);
            write_f32(node.bind_pose.rotation.w);
            write_f32(node.bind_pose.rotation.x);
            write_f32(node.bind_pose.rotation.y);
            write_f32(node.bind_pose.rotation.z);
            write_f32(node.bind_pose.scale.x);
            write_f32(node.bind_pose.scale.y);
            write_f32(node.bind_pose.scale.z);
        }

        write_u32(static_cast<UInt32>(blob.clips.size()));
        for (const MeshAnimClipData& clip : blob.clips) {
            write_string(clip.name);
            write_f32(clip.duration);
            write_u32(clip.loop ? 1u : 0u);
            write_u32(static_cast<UInt32>(clip.channels.size()));
            for (const AnimBoneChannel3D& channel : clip.channels) {
                write_i32(channel.bone);
                write_u32(static_cast<UInt32>(channel.position_times.size()));
                for (Size_t i = 0; i < channel.position_times.size(); ++i) {
                    write_f32(channel.position_times[i]);
                    write_f32(channel.positions[i].x);
                    write_f32(channel.positions[i].y);
                    write_f32(channel.positions[i].z);
                }
                write_u32(static_cast<UInt32>(channel.rotation_times.size()));
                for (Size_t i = 0; i < channel.rotation_times.size(); ++i) {
                    write_f32(channel.rotation_times[i]);
                    write_f32(channel.rotations[i].w);
                    write_f32(channel.rotations[i].x);
                    write_f32(channel.rotations[i].y);
                    write_f32(channel.rotations[i].z);
                }
                write_u32(static_cast<UInt32>(channel.scale_times.size()));
                for (Size_t i = 0; i < channel.scale_times.size(); ++i) {
                    write_f32(channel.scale_times[i]);
                    write_f32(channel.scales[i].x);
                    write_f32(channel.scales[i].y);
                    write_f32(channel.scales[i].z);
                }
            }
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

        auto mesh_data = create_ref<MeshData>();
        mesh_data->vertices.resize(vertex_count);
        if (vertex_count > 0 &&
            !stream.read(
                reinterpret_cast<char*>(mesh_data->vertices.data()),
                static_cast<std::streamsize>(vertex_count * sizeof(MeshVertex)))) {
            return false;
        }
        mesh_data->indices.resize(index_count);
        if (index_count > 0 &&
            !stream.read(
                reinterpret_cast<char*>(mesh_data->indices.data()),
                static_cast<std::streamsize>(index_count * sizeof(UInt32)))) {
            return false;
        }

        mesh_data->sections.resize(section_count);
        for (MeshSection& section : mesh_data->sections) {
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

        UInt32 skeleton_node_count = 0;
        UInt32 clip_count = 0;
        if (!read_value(skeleton_node_count) || !read_value(clip_count)) {
            return false;
        }
        if (skeleton_node_count > kMeshCacheMaxCount || clip_count > kMeshCacheMaxCount) {
            return false;
        }

        out_blob.skeleton_nodes.resize(skeleton_node_count);
        for (SkeletonNode& node : out_blob.skeleton_nodes) {
            if (!read_string(node.name) || !read_value(node.parent)) {
                return false;
            }
            if (!read_value(node.bind_pose.position.x) || !read_value(node.bind_pose.position.y) ||
                !read_value(node.bind_pose.position.z)) {
                return false;
            }
            if (!read_value(node.bind_pose.rotation.w) || !read_value(node.bind_pose.rotation.x) ||
                !read_value(node.bind_pose.rotation.y) || !read_value(node.bind_pose.rotation.z)) {
                return false;
            }
            if (!read_value(node.bind_pose.scale.x) || !read_value(node.bind_pose.scale.y) ||
                !read_value(node.bind_pose.scale.z)) {
                return false;
            }
        }

        out_blob.clips.resize(clip_count);
        for (MeshAnimClipData& clip : out_blob.clips) {
            UInt32 loop_flag = 0;
            UInt32 channel_count = 0;
            if (!read_string(clip.name) || !read_value(clip.duration) || !read_value(loop_flag) ||
                !read_value(channel_count)) {
                return false;
            }
            clip.loop = loop_flag != 0;
            if (channel_count > kMeshCacheMaxCount) {
                return false;
            }
            clip.channels.resize(channel_count);
            for (AnimBoneChannel3D& channel : clip.channels) {
                UInt32 position_count = 0;
                UInt32 rotation_count = 0;
                UInt32 scale_count = 0;
                if (!read_value(channel.bone) ||
                    !read_value(position_count) || !read_value(rotation_count) || !read_value(scale_count)) {
                    return false;
                }
                if (position_count > kMeshCacheMaxCount || rotation_count > kMeshCacheMaxCount ||
                    scale_count > kMeshCacheMaxCount) {
                    return false;
                }
                channel.position_times.resize(position_count);
                channel.positions.resize(position_count);
                for (UInt32 i = 0; i < position_count; ++i) {
                    if (!read_value(channel.position_times[i]) ||
                        !read_value(channel.positions[i].x) || !read_value(channel.positions[i].y) ||
                        !read_value(channel.positions[i].z)) {
                        return false;
                    }
                }
                channel.rotation_times.resize(rotation_count);
                channel.rotations.resize(rotation_count);
                for (UInt32 i = 0; i < rotation_count; ++i) {
                    if (!read_value(channel.rotation_times[i]) ||
                        !read_value(channel.rotations[i].w) || !read_value(channel.rotations[i].x) ||
                        !read_value(channel.rotations[i].y) || !read_value(channel.rotations[i].z)) {
                        return false;
                    }
                }
                channel.scale_times.resize(scale_count);
                channel.scales.resize(scale_count);
                for (UInt32 i = 0; i < scale_count; ++i) {
                    if (!read_value(channel.scale_times[i]) ||
                        !read_value(channel.scales[i].x) || !read_value(channel.scales[i].y) ||
                        !read_value(channel.scales[i].z)) {
                        return false;
                    }
                }
            }
        }

        if (!stream) {
            return false;
        }

        out_blob.data = std::move(mesh_data);
        return true;
    }

    void PackVertexBytes(const DynamicArray<MeshVertex>& vertices,
                         DynamicArray<UInt8>& out_bytes) {
        out_bytes.assign(vertices.size() * kVertexStride, UInt8(0));

        for (Size_t i = 0; i < vertices.size(); ++i) {
            const Size_t base_offset = i * kVertexStride;
            const MeshVertex& vertex = vertices[i];

            std::memcpy(out_bytes.data() + base_offset, &vertex.position, sizeof(Vector3f));

            const Vector4f unpacked_normal(vertex.normal, 0.0f);
            const UInt32 packed_normal = Math::PackSnorm4x8(unpacked_normal);
            std::memcpy(out_bytes.data() + base_offset + sizeof(Vector3f), &packed_normal, sizeof(UInt32));

            std::memcpy(out_bytes.data() + base_offset + sizeof(Vector3f) + sizeof(UInt32), &vertex.tex_coords, sizeof(Vector2f));

            UInt32 bone_ids[4];
            Float bone_weights[4];
            for (UInt32 b = 0; b < 4; ++b) {
                bone_ids[b] = vertex.bone_ids[b];
                bone_weights[b] = vertex.bone_weights[b];
            }
            std::memcpy(out_bytes.data() + base_offset + sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f), bone_ids, sizeof(bone_ids));
            std::memcpy(out_bytes.data() + base_offset + sizeof(Vector3f) + sizeof(UInt32) + sizeof(Vector2f) + sizeof(bone_ids), bone_weights, sizeof(bone_weights));
        }
    }

} // namespace dodoe
