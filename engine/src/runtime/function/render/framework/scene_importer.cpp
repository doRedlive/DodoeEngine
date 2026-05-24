// do@Redlive
#include "scene_importer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/mesh_renderer_component.h"
#include "runtime/function/world/components/transform_component.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/resource/resource_manager.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/euler_angles.hpp"

namespace dodoe {

    namespace {

        HierarchyComponent& EnsureHierarchyComponent(Entity entity) {
            if (entity.hasComponent<HierarchyComponent>()) {
                return entity.getComponent<HierarchyComponent>();
            }
            return entity.addComponent<HierarchyComponent>();
        }

        void AttachChild(Entity parent, Entity child) {
            auto& parent_hierarchy = EnsureHierarchyComponent(parent);
            auto& child_hierarchy = EnsureHierarchyComponent(child);

            child_hierarchy.parent = parent;
            child_hierarchy.parent_uuid = parent.uuid();
            child_hierarchy.dirty = true;

            const auto child_it = std::find(parent_hierarchy.children.begin(), parent_hierarchy.children.end(), child);
            if (child_it == parent_hierarchy.children.end()) {
                parent_hierarchy.children.push_back(child);
            }
            parent_hierarchy.child_count = static_cast<int>(parent_hierarchy.children.size());
            parent_hierarchy.dirty = true;
        }

        Vector3f ToVector3(const aiVector3D& value) {
            return {value.x, value.y, value.z};
        }

        Vector3f ToEulerDegrees(const aiQuaternion& rotation) {
            const glm::quat quaternion(rotation.w, rotation.x, rotation.y, rotation.z);
            return glm::degrees(glm::eulerAngles(quaternion));
        }

        void ApplyNodeTransform(Entity entity, const aiMatrix4x4& transform) {
            aiVector3D scaling{};
            aiVector3D translation{};
            aiQuaternion rotation{};
            transform.Decompose(scaling, rotation, translation);

            auto& transform_component = entity.getComponent<TransformComponent>();
            transform_component.position = ToVector3(translation);
            transform_component.rotation = ToEulerDegrees(rotation);
            transform_component.scale = ToVector3(scaling);
            transform_component.dirty = true;
        }

        identifier ImportTexture(const std::filesystem::path& model_directory, const aiString& texture_path) {
            if (texture_path.length == 0 || texture_path.C_Str()[0] == '*') {
                return 0;
            }

            std::filesystem::path resolved_path = std::filesystem::path(texture_path.C_Str());
            if (resolved_path.is_relative()) {
                resolved_path = model_directory / resolved_path;
            }
            resolved_path = resolved_path.lexically_normal();

            const auto texture_res = ResourceManager::Self().get_texture(resolved_path.string(), resolved_path.string());
            return texture_res.id;
        }

        identifier LoadMaterialTexture(
            const aiMaterial* material,
            const std::filesystem::path& model_directory,
            const aiTextureType primary_type,
            const aiTextureType fallback_type = aiTextureType_NONE) {
            if (!material) {
                return 0;
            }

            for (const aiTextureType type : {primary_type, fallback_type}) {
                if (type == aiTextureType_NONE || material->GetTextureCount(type) == 0) {
                    continue;
                }

                aiString texture_path{};
                if (material->GetTexture(type, 0, &texture_path) != aiReturn_SUCCESS) {
                    continue;
                }

                const identifier texture_id = ImportTexture(model_directory, texture_path);
                if (texture_id != 0) {
                    return texture_id;
                }
            }

            return 0;
        }

        Ref<Material> MakeMaterial(const aiScene* imported_scene, const aiMesh& source_mesh, const std::filesystem::path& model_directory) {
            if (!imported_scene || source_mesh.mMaterialIndex >= imported_scene->mNumMaterials) {
                return nullptr;
            }

            const aiMaterial* source_material = imported_scene->mMaterials[source_mesh.mMaterialIndex];
            if (!source_material) {
                return nullptr;
            }

            auto material = create_ref<Material>();

            aiColor4D base_color{};
            if (aiGetMaterialColor(source_material, AI_MATKEY_BASE_COLOR, &base_color) == aiReturn_SUCCESS ||
                aiGetMaterialColor(source_material, AI_MATKEY_COLOR_DIFFUSE, &base_color) == aiReturn_SUCCESS) {
                material->color = {base_color.r, base_color.g, base_color.b, base_color.a};
            }

            material->base_color_texture = LoadMaterialTexture(
                source_material,
                model_directory,
                aiTextureType_BASE_COLOR,
                aiTextureType_DIFFUSE);
            material->normal_texture = LoadMaterialTexture(
                source_material,
                model_directory,
                aiTextureType_NORMALS,
                aiTextureType_NORMAL_CAMERA);
            material->emissive_texture = LoadMaterialTexture(
                source_material,
                model_directory,
                aiTextureType_EMISSIVE);

            return material;
        }

        Ref<Mesh> MakeMesh(
            const aiMesh& source_mesh,
            const aiScene* imported_scene,
            const std::filesystem::path& model_directory,
            const uint mesh_index,
            const std::string& fallback_name) {
            Ref<Mesh> mesh = create_ref<Mesh>();
            mesh->name = source_mesh.mName.length > 0 ? source_mesh.mName.C_Str() : fallback_name;
            mesh->type = MeshType::Triangles;
            mesh->mesh_index = static_cast<int>(mesh_index);
            mesh->vertex_count = source_mesh.mNumVertices;
            mesh->buffers = create_ref<BufferGroup>();

            ui32 index_count = 0;
            for (uint face_index = 0; face_index < source_mesh.mNumFaces; ++face_index) {
                index_count += source_mesh.mFaces[face_index].mNumIndices;
            }
            mesh->index_count = index_count;

            mesh->buffers->position_data.reserve(source_mesh.mNumVertices);
            mesh->buffers->texcoord1_data.reserve(source_mesh.mNumVertices);
            mesh->buffers->normal_data.reserve(source_mesh.mNumVertices);
            mesh->buffers->tangent_data.reserve(source_mesh.mNumVertices);
            for (uint vertex_index = 0; vertex_index < source_mesh.mNumVertices; ++vertex_index) {
                const auto& position = source_mesh.mVertices[vertex_index];
                mesh->buffers->position_data.push_back({position.x, position.y, position.z});

                if (source_mesh.HasNormals()) {
                    const auto& normal = source_mesh.mNormals[vertex_index];
                    const auto packed_normal = glm::packSnorm4x8(Vector4f(normal.x, normal.y, normal.z, 0.0f));
                    mesh->buffers->normal_data.push_back(packed_normal);
                } else {
                    mesh->buffers->normal_data.push_back(0);
                }

                if (source_mesh.HasTangentsAndBitangents()) {
                    const auto& tangent = source_mesh.mTangents[vertex_index];
                    const auto packed_tangent = glm::packSnorm4x8(Vector4f(tangent.x, tangent.y, tangent.z, 0.0f));
                    mesh->buffers->tangent_data.push_back(packed_tangent);
                } else {
                    mesh->buffers->tangent_data.push_back(0);
                }

                if (source_mesh.HasTextureCoords(0)) {
                    const auto& uv = source_mesh.mTextureCoords[0][vertex_index];
                    mesh->buffers->texcoord1_data.push_back({uv.x, uv.y});
                } else {
                    mesh->buffers->texcoord1_data.push_back(Vector2f(0.0f));
                }
            }

            mesh->buffers->index_data.reserve(index_count);
            for (uint face_index = 0; face_index < source_mesh.mNumFaces; ++face_index) {
                const auto& face = source_mesh.mFaces[face_index];
                for (uint index_offset = 0; index_offset < face.mNumIndices; ++index_offset) {
                    mesh->buffers->index_data.push_back(face.mIndices[index_offset]);
                }
            }

            auto geometry = create_ref<MeshGeometry>();
            geometry->geometry_index = 0;
            geometry->vertex_count = mesh->vertex_count;
            geometry->index_count = mesh->index_count;
            geometry->type = MeshGeometryPrimitiveType::Triangles;
            geometry->material = MakeMaterial(imported_scene, source_mesh, model_directory);
            mesh->geometries.push_back(std::move(geometry));

            return mesh;
        }

        void AttachMeshComponent(
            Entity entity,
            const aiScene* imported_scene,
            const std::filesystem::path& model_directory,
            const uint mesh_index,
            const std::string& fallback_name) {
            if (!imported_scene || mesh_index >= imported_scene->mNumMeshes) {
                return;
            }

            const aiMesh* source_mesh = imported_scene->mMeshes[mesh_index];
            if (!source_mesh) { return; }

            auto& mesh_component = entity.addComponent<MeshRendererComponent>();
            mesh_component.mesh = MakeMesh(*source_mesh, imported_scene, model_directory, mesh_index, fallback_name);
        }

        void ProcessNode(
            Scene* scene,
            const aiScene* imported_scene,
            const std::filesystem::path& model_directory,
            aiNode* node,
            Entity parent_entity) {
            if (!scene || !imported_scene || !node || !parent_entity.valid()) {
                return;
            }

            const std::string node_name = node->mName.C_Str();
            auto node_entity = scene->createEntity(node_name);
            ApplyNodeTransform(node_entity, node->mTransformation);
            AttachChild(parent_entity, node_entity);

            if (node->mNumMeshes == 1) {
                AttachMeshComponent(node_entity, imported_scene, model_directory, node->mMeshes[0], node_name);
            } else {
                for (uint mesh_offset = 0; mesh_offset < node->mNumMeshes; ++mesh_offset) {
                    const uint mesh_index = node->mMeshes[mesh_offset];
                    const std::string mesh_entity_name = fmt::format("{}_Mesh{}", node_name, mesh_offset);

                    auto mesh_entity = scene->createEntity(mesh_entity_name);
                    AttachChild(node_entity, mesh_entity);
                    AttachMeshComponent(mesh_entity, imported_scene, model_directory, mesh_index, mesh_entity_name);
                }
            }

            for (uint child_index = 0; child_index < node->mNumChildren; ++child_index) {
                ProcessNode(scene, imported_scene, model_directory, node->mChildren[child_index], node_entity);
            }
        }

    }

    void SceneImporter::ImportModel(const std::string& path) {
        auto cur_scene = Application::Self().context().world->getCurrentScene();
        DO_ASSERT(cur_scene);

        Assimp::Importer importer;
        const aiScene* imported_scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices
        );

        if (!imported_scene || (imported_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !imported_scene->mRootNode) {
            DO_ERROR("SceneImporter::Import Model error! Info: {}", importer.GetErrorString());
            return;
        }

        const std::string root_name = std::filesystem::path(path).stem().string();
        const std::filesystem::path model_directory = std::filesystem::path(path).parent_path();
        auto root_entity = cur_scene->createEntity(root_name);

        ProcessNode(cur_scene, imported_scene, model_directory, imported_scene->mRootNode, root_entity);
    }
    
} // dodoe
