// do-redlive

#include "mesh_loader.h"

#include "runtime/core/utils/common.h"

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

    dodoe::Ref<dodoe::MeshData> MakeMesh(const aiMesh& source_mesh) {
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

            const dodoe::identifier mesh_id = loader.addMesh(MakeMesh(*source_mesh));
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

    Scope<MeshLoader> MeshLoader::create() {
        auto loader = create_scope<MeshLoader>();
        loader->initialize();
        return loader;
    }

    void MeshLoader::destroy(Scope<MeshLoader>& loader) {
        if (!loader) {
            return;
        }

        loader->shutdown();
        loader.reset();
    }

    void MeshLoader::initialize() {
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
            aiProcess_JoinIdenticalVertices
        );

        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !scene->mRootNode) {
            DoError("MeshLoader::loadModel: {}", importer.GetErrorString());
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
        return loadModel(String2Hash(id), path);
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
        return getModel(String2Hash(id));
    }

    ModelRes MeshLoader::getModel(const std::string& id, const std::string& path) {
        return getModel(String2Hash(id), path);
    }

    MeshRes MeshLoader::getMesh(identifier id) const {
        if (const auto it = mesh_umap_.find(id); it != mesh_umap_.end()) {
            return it->second;
        }
        return MeshRes{};
    }

} // dodoe
