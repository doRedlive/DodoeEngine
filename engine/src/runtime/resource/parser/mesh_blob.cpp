// do@Redlive

#include "mesh_blob.h"

#include "runtime/core/utils/common.h"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

namespace dodoe {

    namespace {

        MeshVertex makeMeshVertex(const aiMesh& mesh, unsigned int vertex_index) {
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

        void loadMaterialTextures(DynamicArray<FileID>& texture_ids,
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

        Ref<MeshData> processMesh(const aiMesh& source_mesh,
                                   const aiScene& scene,
                                   const FsPath& model_directory) {
            DynamicArray<MeshVertex> vertices;            vertices.reserve(source_mesh.mNumVertices);
            for (unsigned int i = 0; i < source_mesh.mNumVertices; ++i) {
                vertices.push_back(makeMeshVertex(source_mesh, i));
            }

            DynamicArray<ui32> indices;            for (unsigned int i = 0; i < source_mesh.mNumFaces; ++i) {
                const aiFace& face = source_mesh.mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                    indices.push_back(face.mIndices[j]);
                }
            }

            auto data = create_ref<MeshData>();
            data->vertices = std::move(vertices);
            data->indices = std::move(indices);
            if (source_mesh.mMaterialIndex < scene.mNumMaterials) {
                loadMaterialTextures(data->textures,
                                     scene.mMaterials[source_mesh.mMaterialIndex],
                                     model_directory);
            }
            return data;
        }

        void processAssimpNode(std::vector<Ref<MeshData>>& meshes,
                                aiNode& node,
                                const aiScene& scene,
                                const FsPath& model_directory) {
            for (unsigned int i = 0; i < node.mNumMeshes; ++i) {
                const aiMesh* source_mesh = scene.mMeshes[node.mMeshes[i]];
                if (!source_mesh) {
                    continue;
                }
                meshes.push_back(processMesh(*source_mesh, scene, model_directory));
            }
            for (unsigned int i = 0; i < node.mNumChildren; ++i) {
                aiNode* child = node.mChildren[i];
                if (!child) {
                    continue;
                }
                processAssimpNode(meshes, *child, scene, model_directory);
            }
        }

    } // namespace

    MeshBlob::MeshBlob(const String& path) {
        load(path);
    }

    MeshBlob::~MeshBlob() {
        if (isValid()) {
            free();
        }
    }

    void MeshBlob::load(const String& path) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            path.c_str(),
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices |
            aiProcess_PreTransformVertices);

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene->mRootNode) {
            DO_ERROR("MeshBlob::load: {}", importer.GetErrorString());
            return;
        }

        FsPath model_directory = FsPath(path).parent_path();
        std::vector<Ref<MeshData>> sub_meshes;
        processAssimpNode(sub_meshes, *scene->mRootNode, *scene, model_directory);

        if (sub_meshes.empty()) {
            return;
        }

        if (sub_meshes.size() == 1) {
            data = sub_meshes[0];
        } else {
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
    }

    void MeshBlob::free() {
        data.reset();
    }

} // dodoe
