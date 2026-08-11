// do@Redlive

#include "scene_importer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/mesh_renderer_component.h"
#include "runtime/function/world/components/sprite_renderer_component.h"
#include "runtime/function/world/components/transform_component.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/core/object/pptr.h"
#include "runtime/service/sprite/sprite_loader.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "runtime/core/math/math.h"

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
            const Quaternion quaternion(rotation.w, rotation.x, rotation.y, rotation.z);
            return Math::Degrees(Math::EulerAngles(quaternion));
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

        struct MeshImportResult {
            MeshUploadData upload_data{};
            DynamicArray<MeshLODData> lods{};
        };

        MeshImportResult MakeMesh(
            const aiMesh& source_mesh,
            const aiScene* imported_scene,
            const FsPath& model_directory,
            const uint mesh_index,
            const String& fallback_name) {
            MeshImportResult result{};
            result.upload_data.name = source_mesh.mName.length > 0 ? source_mesh.mName.C_Str() : fallback_name;

            const uint vertex_count = source_mesh.mNumVertices;
            result.upload_data.position_data.reserve(vertex_count);
            result.upload_data.normal_data.reserve(vertex_count);

            uint index_count = 0;
            for (uint face_index = 0; face_index < source_mesh.mNumFaces; ++face_index) {
                index_count += source_mesh.mFaces[face_index].mNumIndices;
            }

            for (uint vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
                const auto& position = source_mesh.mVertices[vertex_index];
                result.upload_data.position_data.push_back({position.x, position.y, position.z});

                if (source_mesh.HasNormals()) {
                    const auto& normal = source_mesh.mNormals[vertex_index];
                    const auto packed_normal = Math::PackSnorm4x8(Vector4f(normal.x, normal.y, normal.z, 0.0f));
                    result.upload_data.normal_data.push_back(packed_normal);
                } else {
                    result.upload_data.normal_data.push_back(0);
                }

                if (source_mesh.HasTextureCoords(0)) {
                    const auto& uv = source_mesh.mTextureCoords[0][vertex_index];
                    result.upload_data.texcoord_data.push_back({uv.x, uv.y});
                } else {
                    result.upload_data.texcoord_data.push_back(Vector2f(0.0f));
                }
            }

            result.upload_data.index_data.reserve(index_count);
            for (uint face_index = 0; face_index < source_mesh.mNumFaces; ++face_index) {
                const auto& face = source_mesh.mFaces[face_index];
                for (uint index_offset = 0; index_offset < face.mNumIndices; ++index_offset) {
                    result.upload_data.index_data.push_back(face.mIndices[index_offset]);
                }
            }

            SubMesh section{};
            section.section_index = 0;
            section.vertex_count = vertex_count;
            section.index_count = index_count;
            section.primitive_type = MeshGeometryPrimitiveType::Triangles;
            section.material = MakeMaterial(imported_scene, source_mesh, model_directory);

            MeshLODData lod{};
            lod.sub_meshes.push_back(std::move(section));
            result.lods.push_back(std::move(lod));

            return result;
        }

        void AttachMeshComponent(
            Entity entity,
            const aiScene* imported_scene,
            const FsPath& model_directory,
            const uint mesh_index,
            const String& fallback_name) {
            if (!imported_scene || mesh_index >= imported_scene->mNumMeshes) {
                return;
            }

            const aiMesh* source_mesh = imported_scene->mMeshes[mesh_index];
            if (!source_mesh) { return; }

            auto result = MakeMesh(*source_mesh, imported_scene, model_directory, mesh_index, fallback_name);
            auto& mesh_component = entity.addComponent<MeshRendererComponent>();
            mesh_component.upload_data = std::move(result.upload_data);
            mesh_component.lods = std::move(result.lods);
        }

        void ProcessNode(
            Scene* scene,
            const aiScene* imported_scene,
            const FsPath& model_directory,
            aiNode* node,
            Entity parent_entity) {
            if (!scene || !imported_scene || !node || !parent_entity.valid()) {
                return;
            }

            const String node_name = node->mName.C_Str();
            auto node_entity = scene->createEntity(node_name);
            ApplyNodeTransform(node_entity, node->mTransformation);
            AttachChild(parent_entity, node_entity);

            if (node->mNumMeshes == 1) {
                AttachMeshComponent(node_entity, imported_scene, model_directory, node->mMeshes[0], node_name);
            } else {
                for (uint mesh_offset = 0; mesh_offset < node->mNumMeshes; ++mesh_offset) {
                    const uint mesh_index = node->mMeshes[mesh_offset];
                    const String mesh_entity_name(fmt::format("{}_Mesh{}", node_name, mesh_offset).c_str());

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

    void SceneImporter::ImportModel(const String& path) {
        auto cur_scene = Application::Self().context().getWorld()->getActiveScene();
        DO_ASSERT(cur_scene);

        Assimp::Importer importer;
        const aiScene* imported_scene = importer.ReadFile(
            path.c_str(),
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices
        );

        if (!imported_scene || (imported_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || !imported_scene->mRootNode) {
            DO_ERROR("SceneImporter::Import Model error! Info: {}", importer.GetErrorString());
            return;
        }

        const String root_name(FsPath(path).stem().string().c_str());
        const FsPath model_directory = FsPath(path).parent_path();
        auto root_entity = cur_scene->createEntity(root_name);

        ProcessNode(cur_scene, imported_scene, model_directory, imported_scene->mRootNode, root_entity);
    }

    void SceneImporter::ImportSprite(const String& path) {
        auto cur_scene = Application::Self().context().getWorld()->getActiveScene();
        DO_ASSERT(cur_scene);

        const String entity_name(FsPath(path).stem().string().c_str());
        auto entity = cur_scene->createEntity(entity_name);

        auto& sr = entity.addComponent<SpriteRendererComponent>();
        sr.sprite = PPtr<Sprite>(SpriteLoader::Load(path));
        sr.dirty = true;

        LOG_INFO("SceneImporter::ImportSprite: {}", path);
    }

    void SceneImporter::ImportAsset(const String& path) {
        const String ext(FsPath(path).extension().string().c_str());
        DO_DEBUG("Import Asset");

        if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" ||
            ext == ".OBJ" || ext == ".FBX" || ext == ".GLTF" || ext == ".GLB") {
            ImportModel(path);
        } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
                   ext == ".PNG" || ext == ".JPG" || ext == ".JPEG" || ext == ".BMP") {
            ImportSprite(path);
        } else {
            DO_ERROR("SceneImporter::ImportAsset: unsupported format '{}'", ext);
        }
    }

} // dodoe
