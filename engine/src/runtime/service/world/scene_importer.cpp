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
#include "runtime/function/render/mesh/mesh.h"
#include "runtime/function/render/texture/sprite_manager.h"

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

        void AttachMeshComponent(Entity entity, const String& model_path, const uint mesh_index) {
            auto& mesh_component = entity.addComponent<MeshRendererComponent>();
            mesh_component.mesh = PPtr<Mesh>(ResourceManager::Self().loadObjectByPath<Mesh>(FileID(model_path)));
            mesh_component.section_index = static_cast<Int32>(mesh_index);
            mesh_component.mesh.setLegacyPath(model_path);
            mesh_component.dirty = true;
        }

        void ProcessNode(
            Scene* scene,
            aiNode* node,
            Entity parent_entity,
            const String& model_path) {
            if (!scene || !node || !parent_entity.valid()) {
                return;
            }

            const String node_name = node->mName.C_Str();
            auto node_entity = scene->createEntity(node_name);
            ApplyNodeTransform(node_entity, node->mTransformation);
            AttachChild(parent_entity, node_entity);

            if (node->mNumMeshes == 1) {
                AttachMeshComponent(node_entity, model_path, node->mMeshes[0]);
            } else {
                for (uint mesh_offset = 0; mesh_offset < node->mNumMeshes; ++mesh_offset) {
                    const uint mesh_index = node->mMeshes[mesh_offset];
                    const String mesh_entity_name(fmt::format("{}_Mesh{}", node_name, mesh_offset).c_str());

                    auto mesh_entity = scene->createEntity(mesh_entity_name);
                    AttachChild(node_entity, mesh_entity);
                    AttachMeshComponent(mesh_entity, model_path, mesh_index);
                }
            }

            for (uint child_index = 0; child_index < node->mNumChildren; ++child_index) {
                ProcessNode(scene, node->mChildren[child_index], node_entity, model_path);
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
        auto root_entity = cur_scene->createEntity(root_name);

        ProcessNode(cur_scene, imported_scene->mRootNode, root_entity, path);
    }

    void SceneImporter::ImportSprite(const String& path) {
        auto cur_scene = Application::Self().context().getWorld()->getActiveScene();
        DO_ASSERT(cur_scene);

        const String entity_name(FsPath(path).stem().string().c_str());
        auto entity = cur_scene->createEntity(entity_name);

        auto& sr = entity.addComponent<SpriteRendererComponent>();
        sr.sprite = PPtr<Sprite>(ResourceManager::Self().loadObjectByPath<Sprite>(FileID(path)));
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
