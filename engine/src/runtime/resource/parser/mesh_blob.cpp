// do@Redlive

#include "mesh_blob.h"

#include "runtime/core/utils/common.h"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

namespace dodoe {

    namespace {

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

        void AddBoneWeight(MeshVertex& vertex, const UInt32 bone_index, const Float weight) {
            for (UInt32 i = 0; i < 4; ++i) {
                if (vertex.bone_weights[i] < weight) {
                    for (UInt32 j = 3; j > i; --j) {
                        vertex.bone_ids[j] = vertex.bone_ids[j - 1];
                        vertex.bone_weights[j] = vertex.bone_weights[j - 1];
                    }
                    vertex.bone_ids[i] = bone_index;
                    vertex.bone_weights[i] = weight;
                    break;
                }
            }
        }

        void LoadBoneWeights(const aiMesh& source_mesh,
                             const Skeleton& skeleton,
                             DynamicArray<MeshVertex>& vertices) {
            for (unsigned int b = 0; b < source_mesh.mNumBones; ++b) {
                const aiBone* bone = source_mesh.mBones[b];
                if (!bone) {
                    continue;
                }
                const Int32 bone_index = skeleton.findNode(bone->mName.C_Str());
                if (bone_index < 0) {
                    continue;
                }
                for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
                    const aiVertexWeight& weight = bone->mWeights[w];
                    if (weight.mVertexId >= vertices.size()) {
                        continue;
                    }
                    AddBoneWeight(vertices[weight.mVertexId], static_cast<UInt32>(bone_index), weight.mWeight);
                }
            }
        }

        void BakeNodeTransform(DynamicArray<MeshVertex>& vertices, const Matrix4f& node_world) {
            const glm::mat3 normal_matrix = glm::mat3(Math::Transpose(Math::Inverse(node_world)));
            for (auto& vertex : vertices) {
                vertex.position = glm::vec3(node_world * glm::vec4(vertex.position, 1.0f));
                vertex.normal = Math::Normalize(normal_matrix * vertex.normal);
                vertex.tangent = Math::Normalize(normal_matrix * vertex.tangent);
                vertex.bitangent = Math::Normalize(normal_matrix * vertex.bitangent);
            }
        }

        BoneBindPose MakeBindPose(const aiMatrix4x4& matrix) {
            aiVector3D scaling;
            aiQuaternion rotation;
            aiVector3D position;
            matrix.Decompose(scaling, rotation, position);
            BoneBindPose pose;
            pose.position = {position.x, position.y, position.z};
            pose.rotation = Quaternion(rotation.w, rotation.x, rotation.y, rotation.z);
            pose.scale = {scaling.x, scaling.y, scaling.z};
            return pose;
        }

        Int32 BuildSkeletonNode(const aiNode& node,
                                Skeleton& skeleton,
                                const Int32 parent,
                                UnorderedMap<String, Int32>& node_indices) {
            const Int32 index = skeleton.addNode(node.mName.C_Str(), parent, MakeBindPose(node.mTransformation));
            node_indices[node.mName.C_Str()] = index;
            for (unsigned int i = 0; i < node.mNumChildren; ++i) {
                if (node.mChildren[i]) {
                    BuildSkeletonNode(*node.mChildren[i], skeleton, index, node_indices);
                }
            }
            return index;
        }

        Skeleton* BuildSkeleton(const aiScene& scene, const UUID& asset_id) {
            Skeleton* skeleton = Skeleton::Create(ObjectID{asset_id, Skeleton::kLocalId});
            skeleton->clear();
            UnorderedMap<String, Int32> node_indices;
            BuildSkeletonNode(*scene.mRootNode, *skeleton, -1, node_indices);
            return skeleton;
        }

        AnimClip* ParseAnimation(const aiAnimation& animation,
                                 const Skeleton& skeleton,
                                 const UUID& asset_id,
                                 const UInt32 clip_index) {
            AnimClip* clip = AnimClip::Create(ObjectID{asset_id, AnimClip::kLocalIdBase + clip_index});
            clip->clear();
            clip->name = animation.mName.C_Str();
            const Float tick_scale =
                animation.mTicksPerSecond > 0.0 ? static_cast<Float>(animation.mTicksPerSecond) : 1.0f;
            clip->duration = static_cast<Float>(animation.mDuration) / tick_scale;
            clip->loop = true;
            clip->channels.reserve(animation.mNumChannels);
            for (unsigned int c = 0; c < animation.mNumChannels; ++c) {
                const aiNodeAnim* channel = animation.mChannels[c];
                if (!channel) {
                    continue;
                }
                AnimBoneChannel3D bone_channel;
                bone_channel.bone = skeleton.findNode(channel->mNodeName.C_Str());
                if (bone_channel.bone < 0) {
                    continue;
                }
                for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k) {
                    const aiVectorKey& key = channel->mPositionKeys[k];
                    bone_channel.position_times.push_back(static_cast<Float>(key.mTime) / tick_scale);
                    bone_channel.positions.emplace_back(key.mValue.x, key.mValue.y, key.mValue.z);
                }
                for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k) {
                    const aiQuatKey& key = channel->mRotationKeys[k];
                    bone_channel.rotation_times.push_back(static_cast<Float>(key.mTime) / tick_scale);
                    bone_channel.rotations.emplace_back(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z);
                }
                for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k) {
                    const aiVectorKey& key = channel->mScalingKeys[k];
                    bone_channel.scale_times.push_back(static_cast<Float>(key.mTime) / tick_scale);
                    bone_channel.scales.emplace_back(key.mValue.x, key.mValue.y, key.mValue.z);
                }
                if (bone_channel.positions.empty() && bone_channel.rotations.empty() && bone_channel.scales.empty()) {
                    continue;
                }
                clip->channels.push_back(std::move(bone_channel));
            }
            return clip;
        }

        void LoadMaterialTextures(DynamicArray<FileID>& texture_ids,
                                   const aiMaterial* material,
                                   const FsPath& model_directory) {
            if (!material) {
                return;
            }
            auto loadType = [&](aiTextureType type) {
                unsigned int count = material->GetTextureCount(type);
                for (unsigned int i = 0; i < count; ++i) {
                    aiString texture_path{};
                    if (material->GetTexture(type, i, &texture_path) != aiReturn_SUCCESS) {
                        continue;
                    }
                    FsPath resolved = model_directory / FsPath(texture_path.C_Str());
                    resolved = resolved.lexically_normal();
                    texture_ids.emplace_back(resolved.string().c_str());
                }
            };
            loadType(aiTextureType_DIFFUSE);
            if (texture_ids.empty()) {
                loadType(aiTextureType_BASE_COLOR);
            }
        }

        Ref<MeshData> ProcessMesh(const aiMesh& source_mesh,
                                   const aiScene& scene,
                                   const FsPath& model_directory,
                                   const Skeleton* skeleton,
                                   const Matrix4f& node_world) {
            DynamicArray<MeshVertex> vertices;
            vertices.reserve(source_mesh.mNumVertices);
            for (unsigned int i = 0; i < source_mesh.mNumVertices; ++i) {
                vertices.push_back(MakeMeshVertex(source_mesh, i));
            }

            if (skeleton) {
                LoadBoneWeights(source_mesh, *skeleton, vertices);
            }
            else {
                BakeNodeTransform(vertices, node_world);
            }

            DynamicArray<UInt32> indices;
            for (unsigned int i = 0; i < source_mesh.mNumFaces; ++i) {
                const aiFace& face = source_mesh.mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                    indices.push_back(face.mIndices[j]);
                }
            }

            auto data = create_ref<MeshData>();
            data->vertices = std::move(vertices);
            data->indices = std::move(indices);
            if (source_mesh.mMaterialIndex < scene.mNumMaterials) {
                LoadMaterialTextures(data->textures,
                                     scene.mMaterials[source_mesh.mMaterialIndex],
                                     model_directory);
            }
            return data;
        }

        void ProcessAssimpNode(std::vector<Ref<MeshData>>& meshes,
                                aiNode& node,
                                const aiScene& scene,
                                const FsPath& model_directory,
                                const Skeleton* skeleton,
                                const Matrix4f& parent_world) {
            const Matrix4f node_world = parent_world * ToGlmMatrix(node.mTransformation);
            for (unsigned int i = 0; i < node.mNumMeshes; ++i) {
                const aiMesh* source_mesh = scene.mMeshes[node.mMeshes[i]];
                if (!source_mesh) {
                    continue;
                }
                meshes.push_back(ProcessMesh(*source_mesh, scene, model_directory, skeleton, node_world));
            }
            for (unsigned int i = 0; i < node.mNumChildren; ++i) {
                aiNode* child = node.mChildren[i];
                if (!child) {
                    continue;
                }
                ProcessAssimpNode(meshes, *child, scene, model_directory, skeleton, node_world);
            }
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

    } // namespace

    MeshBlob::~MeshBlob() {
        if (isValid()) {
            free();
        }
    }

    void MeshBlob::load(const String& path, const UUID& asset_id) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            path.c_str(),
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices);

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene->mRootNode) {
            DO_ERROR("MeshBlob::load: {}", importer.GetErrorString());
            return;
        }

        BuildHierarchyNode(*scene->mRootNode, -1, hierarchy);

        Bool has_bones = false;
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
            if (scene->mMeshes[i] && scene->mMeshes[i]->mNumBones > 0) {
                has_bones = true;
                break;
            }
        }

        Skeleton* skeleton = nullptr;
        if (has_bones) {
            skeleton = BuildSkeleton(*scene, asset_id);
        }

        FsPath model_directory = FsPath(path).parent_path();
        std::vector<Ref<MeshData>> sub_meshes;
        const Matrix4f root_world(1.0f);
        ProcessAssimpNode(sub_meshes, *scene->mRootNode, *scene, model_directory, skeleton, root_world);

        if (sub_meshes.empty()) {
            return;
        }

        if (sub_meshes.size() == 1) {
            data = sub_meshes[0];
        }
        else {
            data = create_ref<MeshData>();
            for (auto& sm : sub_meshes) {
                ui32 vertex_offset = static_cast<ui32>(data->vertices.size());
                data->vertices.insert(data->vertices.end(),
                                       sm->vertices.begin(), sm->vertices.end());
                for (ui32 idx : sm->indices) {
                    data->indices.push_back(vertex_offset + idx);
                }
                for (const auto& tex : sm->textures) {
                    data->textures.push_back(tex);
                }
            }
        }

        data->skeleton = PPtr<Skeleton>(skeleton);
        if (skeleton && scene->mNumAnimations > 0) {
            data->animations.reserve(scene->mNumAnimations);
            for (unsigned int a = 0; a < scene->mNumAnimations; ++a) {
                if (!scene->mAnimations[a]) {
                    continue;
                }
                AnimClip* clip = ParseAnimation(*scene->mAnimations[a], *skeleton, asset_id, a);
                if (!clip->channels.empty()) {
                    data->animations.push_back(PPtr<AnimClip>(clip));
                }
            }
        }
    }

    void MeshBlob::free() {
        data.reset();
        hierarchy.clear();
    }

} // dodoe
