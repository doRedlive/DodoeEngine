// do-redlive

#include "mesh_loader.h"

#include "runtime/core/utils/common.h"
#include "runtime/resource/resource_manager.h"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

namespace {

    dodoe::MeshVertex MakeVertex(const aiMesh& mesh, const unsigned int vertex_index) {
        dodoe::MeshVertex vertex{};

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

    std::vector<dodoe::identifier> LoadMaterialTextures(const aiMaterial* material, const std::filesystem::path& model_directory) {
        std::vector<dodoe::identifier> texture_ids{};
        if (!material) {
            return texture_ids;
        }

        auto load_type = [&](aiTextureType type) {
            const unsigned int count = material->GetTextureCount(type);
            for (unsigned int i = 0; i < count; ++i) {
                aiString texture_path{};
                if (material->GetTexture(type, i, &texture_path) != aiReturn_SUCCESS) {
                    continue;
                }

                std::filesystem::path resolved = model_directory / std::filesystem::path(texture_path.C_Str());
                resolved = resolved.lexically_normal();
                auto texture_res = dodoe::ResourceManager::Self().get_texture(resolved.string(), resolved.string());
                if (texture_res.id != 0) {
                    texture_ids.push_back(texture_res.id);
                }
            }
        };

        load_type(aiTextureType_DIFFUSE);
        if (texture_ids.empty()) {
            load_type(aiTextureType_BASE_COLOR);
        }

        return texture_ids;
    }

    dodoe::Ref<dodoe::MeshData> MakeMesh(const aiMesh& source_mesh, const aiScene& scene, const std::filesystem::path& model_directory) {
        std::vector<dodoe::MeshVertex> vertices;
        vertices.reserve(source_mesh.mNumVertices);

        for (unsigned int vertex_index = 0; vertex_index < source_mesh.mNumVertices; ++vertex_index) {
            vertices.push_back(MakeVertex(source_mesh, vertex_index));
        }

        std::vector<dodoe::ui32> indices;
        for (unsigned int face_index = 0; face_index < source_mesh.mNumFaces; ++face_index) {
            const aiFace& face = source_mesh.mFaces[face_index];
            indices.reserve(indices.size() + face.mNumIndices);
            for (unsigned int index = 0; index < face.mNumIndices; ++index) {
                indices.push_back(face.mIndices[index]);
            }
        }

        auto context = dodoe::create_ref<dodoe::MeshData>();
        context->vertices = std::move(vertices);
        context->indices = std::move(indices);
        if (source_mesh.mMaterialIndex < scene.mNumMaterials) {
            context->textures = LoadMaterialTextures(scene.mMaterials[source_mesh.mMaterialIndex], model_directory);
        }

        return context;
    }

    void ProcessNode(
        dodoe::MeshLoader& loader,
        dodoe::Ref<dodoe::ModelData>& model,
        aiNode& node,
        const aiScene& scene) {
        for (unsigned int mesh_index = 0; mesh_index < node.mNumMeshes; ++mesh_index) {
            const aiMesh* source_mesh = scene.mMeshes[node.mMeshes[mesh_index]];
            if (!source_mesh) {
                continue;
            }

            const std::filesystem::path model_directory = std::filesystem::path(model->directory);
            const dodoe::identifier mesh_id = loader.addMesh(MakeMesh(*source_mesh, scene, model_directory));
            model->meshes.push_back(mesh_id);
        }

        for (unsigned int child_index = 0; child_index < node.mNumChildren; ++child_index) {
            aiNode* child = node.mChildren[child_index];
            if (!child) {
                continue;
            }

            ProcessNode(loader, model, *child, scene);
        }
    }

}

namespace dodoe {

    bool MeshLoader::initialize(const MeshLoaderCreateInfo& info) {
        (void)info;
        return true;
    }

    void MeshLoader::shutdown() {
        model_umap_.clear();
        mesh_umap_.clear();
        next_mesh_id_ = 1;
    }

    identifier MeshLoader::allocateMeshId() {
        return next_mesh_id_++;
    }

    identifier MeshLoader::addMesh(const Ref<MeshData>& mesh_data) {
        if (!mesh_data) {
            return 0;
        }

        const identifier mesh_id = allocateMeshId();
        MeshRes mesh_res{};
        mesh_res.id = mesh_id;
        mesh_res.data = mesh_data;
        mesh_umap_[mesh_id] = mesh_res;
        return mesh_id;
    }

    ModelRes MeshLoader::loadModel(identifier id, const std::string& path) {
        if (const auto it = model_umap_.find(id); it != model_umap_.end()) {
            return it->second;
        }

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices |
            aiProcess_PreTransformVertices
        );

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene->mRootNode) {
            DO_ERROR("MeshLoader::loadModel: {}", importer.GetErrorString());
            return {};
        }

        auto data = create_ref<ModelData>();
        data->directory = std::filesystem::path(path).parent_path().string();
        ProcessNode(*this, data, *scene->mRootNode, *scene);

        ModelRes res;
        res.id = id;
        res.data = data;

        model_umap_[id] = res;
        return res;
    }

    ModelRes MeshLoader::loadModel(const std::string& id, const std::string& path) {
        return loadModel(string2hash(id), path);
    }

    ModelRes MeshLoader::getModel(identifier id) {
        if (const auto it = model_umap_.find(id); it != model_umap_.end()) {
            return it->second;
        }
        return ModelRes{};
    }

    ModelRes MeshLoader::getModel(identifier id, const std::string& path) {
        if (const auto it = model_umap_.find(id); it != model_umap_.end()) {
            return it->second;
        }
        return loadModel(id, path);
    }

    ModelRes MeshLoader::getModel(const std::string& id) {
        return getModel(string2hash(id));
    }

    ModelRes MeshLoader::getModel(const std::string& id, const std::string& path) {
        return getModel(string2hash(id), path);
    }

    MeshRes MeshLoader::getMesh(identifier id) const {
        if (const auto it = mesh_umap_.find(id); it != mesh_umap_.end()) {
            return it->second;
        }
        return MeshRes{};
    }

} // dodoe
