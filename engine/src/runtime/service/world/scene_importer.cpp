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
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/types/mesh_asset.h"
#include "runtime/core/object/pptr.h"
#include "runtime/function/render/mesh_draw/mesh.h"
#include "runtime/function/render/pixel2d/sprite_manager.h"

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

    }

    void SceneImporter::ImportModel(const String& path) {
        auto cur_scene = Application::Self().context().getWorld()->getActiveScene();
        DO_ASSERT(cur_scene);

        AssetManager* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            DO_ERROR("SceneImporter::ImportModel: AssetManager unavailable");
            return;
        }

        const ObjectID ref = asset_manager->resolvePathToRef(FileID(path));
        if (!ref.isValid()) {
            DO_ERROR("SceneImporter::ImportModel: asset not found in database for '{}'", path);
            return;
        }

        MeshAsset* asset = asset_manager->loadAssetSync<MeshAsset>(ref.asset_id);
        if (!asset) {
            DO_ERROR("SceneImporter::ImportModel: failed to load MeshAsset for '{}'", path);
            return;
        }

        Mesh* mesh = ResourceManager::Self().loadObject<Mesh>(ref.asset_id, 0);
        if (!mesh) {
            DO_ERROR("SceneImporter::ImportModel: failed to load Mesh for '{}'", path);
            return;
        }

        const String root_name(FsPath(path).stem().string().c_str());
        auto root_entity = cur_scene->createEntity(root_name);

        const DynamicArray<MeshNode>& hierarchy = asset->getHierarchy();
        if (hierarchy.empty()) {
            auto& mc = root_entity.addComponent<MeshRendererComponent>();
            mc.mesh = PPtr<Mesh>(mesh);
            mc.mesh.setLegacyPath(path);
            mc.section_index = 0;
            mc.dirty = true;
            return;
        }

        DynamicArray<Entity> node_entities;
        node_entities.reserve(hierarchy.size());
        for (const auto& node : hierarchy) {
            auto node_entity = cur_scene->createEntity(node.name);

            auto& tc = node_entity.getComponent<TransformComponent>();
            tc.position = node.position;
            tc.rotation = node.rotation;
            tc.scale = node.scale;
            tc.dirty = true;

            if (node.parent_index >= 0) {
                AttachChild(node_entities[static_cast<Size_t>(node.parent_index)], node_entity);
            } else {
                AttachChild(root_entity, node_entity);
            }

            if (node.mesh_section_index >= 0) {
                auto& mc = node_entity.addComponent<MeshRendererComponent>();
                mc.mesh = PPtr<Mesh>(mesh);
                mc.mesh.setLegacyPath(path);
                mc.section_index = node.mesh_section_index;
                mc.dirty = true;
            }
            node_entities.push_back(node_entity);
        }
    }

    void SceneImporter::ImportSprite(const String& path) {
        auto cur_scene = Application::Self().context().getWorld()->getActiveScene();
        DO_ASSERT(cur_scene);

        AssetManager* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            DO_ERROR("SceneImporter::ImportSprite: AssetManager unavailable");
            return;
        }

        const ObjectID ref = asset_manager->ensureImported(path);
        if (!ref.isValid()) {
            DO_ERROR("SceneImporter::ImportSprite: failed to ensure import for '{}'", path);
            return;
        }

        Sprite* sprite = ResourceManager::Self().loadObjectByPath<Sprite>(FileID(path));
        if (!sprite) {
            DO_WARN("SceneImporter::ImportSprite: no sprite sub-asset for '{}'", path);
            return;
        }

        const String entity_name(FsPath(path).stem().string().c_str());
        auto entity = cur_scene->createEntity(entity_name);

        auto& sr = entity.addComponent<SpriteRendererComponent>();
        sr.sprite = PPtr<Sprite>(sprite);
        sr.dirty = true;

        LOG_INFO("SceneImporter::ImportSprite: {}", path);
    }

    void SceneImporter::ImportAsset(const String& path) {
        const String ext(FsPath(path).extension().string().c_str());

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
