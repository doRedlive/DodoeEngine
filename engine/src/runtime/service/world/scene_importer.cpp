// do@Redlive

#include "scene_importer.h"

#include "runtime/core/application.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/function/script/script_runtime.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/id_component.h"
#include "runtime/function/world/components/mesh_renderer_component.h"
#include "runtime/function/world/components/prefab_instance_component.h"
#include "runtime/function/world/components/sprite_renderer_component.h"
#include "runtime/function/world/components/transform_component.h"
#include "runtime/function/world/prefab.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/resource/resource_manager.h"
#include "runtime/resource/file/file_id.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/types/mesh_asset.h"
#include "runtime/resource/asset/types/prefab_asset.h"
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

        ScriptRuntime* GetScriptRuntime() {
            if (!GetScriptSystem()) {
                return nullptr;
            }
            return GetScriptSystem()->getScriptRuntime();
        }

        bool ParseJsonText(const String& json_text, Json& out_json) {
            try {
                out_json = Json::parse(json_text);
                return true;
            }
            catch (const Json::exception&) {
                return false;
            }
        }

        void DeserializePrefabComponents(const std::vector<ComponentRes>& components, Entity entity) {
            auto& component_db = ComponentDB::self();
            for (const auto& component_res : components) {
                if (component_res.m_type_name == "IDComponent") {
                    continue;
                }

                const auto* entry = component_db.find(component_res.m_type_name);
                if (!entry || !entry->readJson) {
                    continue;
                }

                if (!entry->contains(entity) && entry->add) {
                    entry->add(entity);
                }

                void* component_ptr = entry->get(entity);
                if (!component_ptr) {
                    continue;
                }

                Json component_json;
                if (!ParseJsonText(component_res.m_component, component_json)) {
                    continue;
                }

                (void)entry->readJson(component_ptr, component_json);
            }
        }

        void DeserializePrefabManagedComponents(const std::vector<ComponentRes>& components, Entity entity) {
            ScriptRuntime* runtime = GetScriptRuntime();
            if (!runtime) {
                return;
            }

            const uint64_t uuid = static_cast<uint64_t>(entity.uuid());
            for (const auto& component_res : components) {
                runtime->addEntityManagedComponentFromManaged(uuid, component_res.m_type_name);

                if (component_res.m_component.empty()) {
                    continue;
                }
                Json fields;
                if (!ParseJsonText(component_res.m_component, fields)) {
                    continue;
                }
                runtime->setEntityManagedComponentFields(uuid, component_res.m_type_name, fields);
            }
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

    Entity SceneImporter::InstantiatePrefab(const PPtr<Prefab>& prefab_ref) {
        auto cur_scene = Application::Self().context().getWorld()->getActiveScene();
        if (!cur_scene) {
            DO_ERROR("SceneImporter::InstantiatePrefab: no active scene");
            return Entity::NullEntity();
        }

        Prefab* prefab = prefab_ref.get();
        if (!prefab && prefab_ref.isAssigned()) {
            const ObjectID& id = prefab_ref.getObjectID();
            prefab = ResourceManager::Self().loadObject<Prefab>(id.asset_id, id.local_id);
        }
        if (!prefab) {
            const String& legacy_path = prefab_ref.getLegacyPath();
            if (legacy_path.empty()) {
                return Entity::NullEntity();
            }
            return ImportPrefab(legacy_path);
        }

        const SceneRes& res = prefab->getSceneRes();
        if (res.m_entities.empty()) {
            return Entity::NullEntity();
        }

        const String root_name(FsPath(prefab->getPath().c_str()).stem().string().c_str());

        std::unordered_map<UUID, Entity> created;
        created.reserve(res.m_entities.size());
        for (const auto& entity_res : res.m_entities) {
            Entity entity = cur_scene->createEntity(UUID(), entity_res.m_name);
            DeserializePrefabComponents(entity_res.m_native_components, entity);
            DeserializePrefabManagedComponents(entity_res.m_managed_components, entity);
            created[entity_res.m_uuid] = entity;
        }

        Entity root = Entity::NullEntity();
        Size_t root_count = 0;
        for (const auto& entity_res : res.m_entities) {
            const auto entity_it = created.find(entity_res.m_uuid);
            if (entity_it == created.end()) {
                continue;
            }
            Entity entity = entity_it->second;

            UUID parent_uuid{};
            if (entity.hasComponent<HierarchyComponent>()) {
                parent_uuid = entity.getComponent<HierarchyComponent>().parent_uuid;
            }

            const auto parent_it = parent_uuid.isValid() ? created.find(parent_uuid) : created.end();
            if (parent_it != created.end()) {
                AttachChild(parent_it->second, entity);
                continue;
            }

            if (entity.hasComponent<HierarchyComponent>()) {
                auto& hier = entity.getComponent<HierarchyComponent>();
                hier.parent = Entity::NullEntity();
                hier.parent_uuid = UUID{};
                hier.children.clear();
                hier.child_count = 0;
                hier.dirty = true;
            }
            root = entity;
            ++root_count;
        }

        if (root_count == 1 && root.valid()) {
            root.getComponent<IDComponent>().setName(root_name);

            auto& inst = root.addComponent<PrefabInstanceComponent>();
            inst.prefab = PPtr<Prefab>(prefab);
            inst.prefab.setLegacyPath(prefab_ref.getLegacyPath().empty() ? prefab->getPath() : prefab_ref.getLegacyPath());
            if (root.hasComponent<TransformComponent>()) {
                const auto& tc = root.getComponent<TransformComponent>();
                inst.position = tc.position;
                inst.rotation = tc.rotation;
                inst.scale = tc.scale;
            }
        }

        LOG_INFO("SceneImporter::InstantiatePrefab: {} ({} entities)", root_name, res.m_entities.size());
        return root_count == 1 ? root : Entity::NullEntity();
    }

    Entity SceneImporter::ImportPrefab(const String& path) {
        AssetManager* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            DO_ERROR("SceneImporter::ImportPrefab: AssetManager unavailable");
            return Entity::NullEntity();
        }

        FsPath prefab_fs_path(path.c_str());
        if (prefab_fs_path.is_relative()) {
            prefab_fs_path = asset_manager->getAssetDir() / prefab_fs_path;
        }

        const ObjectID ref = asset_manager->ensureImported(String(prefab_fs_path.generic_string().c_str()));
        if (!ref.isValid()) {
            DO_ERROR("SceneImporter::ImportPrefab: failed to ensure import for '{}'", path);
            return Entity::NullEntity();
        }

        PPtr<Prefab> prefab_ref(ref);
        prefab_ref.setLegacyPath(path);
        Entity root = InstantiatePrefab(prefab_ref);
        if (!root.valid()) {
            DO_ERROR("SceneImporter::ImportPrefab: failed to load Prefab for '{}'", path);
        }
        return root;
    }

    ObjectID SceneImporter::ExportPrefab(const String& path, Entity root) {
        if (!root.valid() || !root.getScene()) {
            DO_ERROR("SceneImporter::ExportPrefab: invalid root entity");
            return {};
        }

        AssetManager* asset_manager = ResourceManager::Self().getAssetManager();
        if (!asset_manager) {
            DO_ERROR("SceneImporter::ExportPrefab: AssetManager unavailable");
            return {};
        }

        const SceneRes res = root.getScene()->serializeSubtree(root);
        if (res.m_entities.empty()) {
            DO_ERROR("SceneImporter::ExportPrefab: subtree is empty");
            return {};
        }

        if (!asset_manager->saveAssetFile(res, path)) {
            return {};
        }

        const FsPath absolute_path = asset_manager->getAssetDir() / FsPath(path);
        const ObjectID ref = asset_manager->ensureImported(String(absolute_path.generic_string().c_str()));
        if (!ref.isValid()) {
            return {};
        }

        if (PrefabAsset* existing = asset_manager->findAsset<PrefabAsset>(ref.asset_id)) {
            (void)existing->loadFromSource(absolute_path.generic_string().c_str());
            if (Prefab* prefab_obj = ResourceManager::Self().findLoaded<Prefab>(ref.asset_id, ref.local_id)) {
                prefab_obj->setSceneRes(existing->getSceneRes());
            }
        }

        LOG_INFO("SceneImporter::ExportPrefab: {} ({} entities)", path, res.m_entities.size());
        return ref;
    }

    void SceneImporter::ImportAsset(const String& path) {
        const String ext(FsPath(path).extension().string().c_str());

        if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" ||
            ext == ".OBJ" || ext == ".FBX" || ext == ".GLTF" || ext == ".GLB") {
            ImportModel(path);
        } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
                   ext == ".PNG" || ext == ".JPG" || ext == ".JPEG" || ext == ".BMP") {
            ImportSprite(path);
        } else if (ext == ".prefab" || ext == ".PREFAB") {
            ImportPrefab(path);
        } else {
            DO_ERROR("SceneImporter::ImportAsset: unsupported format '{}'", ext);
        }
    }

} // dodoe
