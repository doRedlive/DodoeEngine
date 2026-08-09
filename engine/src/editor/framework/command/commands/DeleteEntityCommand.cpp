// do@Redlive

#include "DeleteEntityCommand.h"
#include "framework/EditorContext.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/resource/res_type/scene_res.h"
#include "runtime/core/context/system_context.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/function/script/script_runtime.h"
#include "runtime/function/log/log_system.h"

#include <algorithm>

namespace cakery {

namespace {

    dodoe::ScriptRuntime* GetScriptRuntime() {
        if (!dodoe::GetScriptSystem()) {
            return nullptr;
        }
        return dodoe::GetScriptSystem()->getScriptRuntime();
    }

    bool ParseJsonText(const dodoe::String& json_text, dodoe::Json& out_json) {
        try {
            out_json = dodoe::Json::parse(json_text);
            return true;
        }
        catch (const dodoe::Json::exception&) {
            return false;
        }
    }

    void CollectSubtree(dodoe::Entity entity, dodoe::DynamicArray<dodoe::Entity>& out) {
        out.push_back(entity);
        if (!entity.hasComponent<dodoe::HierarchyComponent>()) {
            return;
        }
        for (dodoe::Entity child : entity.getComponent<dodoe::HierarchyComponent>().children) {
            if (child.valid()) {
                CollectSubtree(child, out);
            }
        }
    }

    void DetachFromParent(dodoe::Entity entity) {
        if (!entity.hasComponent<dodoe::HierarchyComponent>()) {
            return;
        }
        auto& hierarchy = entity.getComponent<dodoe::HierarchyComponent>();
        if (!hierarchy.parent.valid() || !hierarchy.parent.hasComponent<dodoe::HierarchyComponent>()) {
            return;
        }
        auto& parent_hierarchy = hierarchy.parent.getComponent<dodoe::HierarchyComponent>();
        auto& children = parent_hierarchy.children;
        children.erase(std::remove(children.begin(), children.end(), entity), children.end());
        parent_hierarchy.child_count = static_cast<int>(children.size());
        parent_hierarchy.dirty = true;
    }

    void DeserializeNativeComponents(const std::vector<dodoe::ComponentRes>& components, dodoe::Entity entity) {
        auto& component_db = dodoe::ComponentDB::self();
        for (const auto& component_res : components) {
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

            dodoe::Json component_json;
            if (!ParseJsonText(component_res.m_component, component_json)) {
                continue;
            }

            (void)entry->readJson(component_ptr, component_json);
        }
    }

    void DeserializeManagedComponents(const std::vector<dodoe::ComponentRes>& components, dodoe::Entity entity) {
        dodoe::ScriptRuntime* runtime = GetScriptRuntime();
        if (!runtime) {
            return;
        }

        const uint64_t uuid = static_cast<uint64_t>(entity.uuid());
        for (const auto& component_res : components) {
            runtime->addEntityManagedComponentFromManaged(uuid, component_res.m_type_name);

            if (component_res.m_component.empty()) {
                continue;
            }
            dodoe::Json fields;
            if (!ParseJsonText(component_res.m_component, fields)) {
                continue;
            }
            runtime->setEntityManagedComponentFields(uuid, component_res.m_type_name, fields);
        }
    }

} // namespace

DeleteEntityCommand::DeleteEntityCommand(dodoe::UUID uuid)
    : m_uuid(uuid)
{}

bool DeleteEntityCommand::execute(EditorContext& ctx)
{
    auto* scene = ctx.activeScene();
    if (!scene) return false;

    auto entity = ResolveEntity(scene, m_uuid);
    if (!entity.valid()) return false;

    // 收集目标实体及其所有后代
    dodoe::DynamicArray<dodoe::Entity> subtree;
    CollectSubtree(entity, subtree);

    // 快照整场景，并筛选出子树对应的 EntityRes
    dodoe::SceneRes full_snapshot = scene->serialize();
    dodoe::UnorderedSet<dodoe::UUID> subtree_uuids;
    for (dodoe::Entity subtree_entity : subtree) {
        subtree_uuids.insert(subtree_entity.uuid());
    }

    m_serializedSubtree = std::make_unique<dodoe::SceneRes>();
    m_serializedSubtree->m_name = full_snapshot.m_name;
    for (auto& entity_res : full_snapshot.m_entities) {
        if (subtree_uuids.count(entity_res.m_uuid)) {
            m_serializedSubtree->m_entities.push_back(std::move(entity_res));
        }
    }

    // 从父的 children 中移除目标实体，并同步 child_count
    DetachFromParent(entity);

    // 从叶子到根销毁，避免父销毁后子仍引用父
    for (auto it = subtree.rbegin(); it != subtree.rend(); ++it) {
        if (it->valid()) {
            scene->destroyEntity(*it);
        }
    }

    LOG_INFO("[DeleteEntity] {}", static_cast<uint64_t>(m_uuid));
    return true;
}

void DeleteEntityCommand::undo(EditorContext& ctx)
{
    if (!m_serializedSubtree) return;

    auto* scene = ctx.activeScene();
    if (!scene) return;

    // 重建实体与组件
    dodoe::UnorderedMap<dodoe::UUID, dodoe::Entity> rebuilt;
    for (const auto& entity_res : m_serializedSubtree->m_entities) {
        dodoe::Entity entity = scene->createEntity(entity_res.m_uuid, entity_res.m_name);
        if (!entity.valid()) continue;

        DeserializeNativeComponents(entity_res.m_native_components, entity);
        DeserializeManagedComponents(entity_res.m_managed_components, entity);
        rebuilt[entity_res.m_uuid] = entity;
    }

    // 恢复层级关系：子树内部的父用重建实体，外部父（原根实体的父）从场景查询
    for (const auto& entity_res : m_serializedSubtree->m_entities) {
        auto rebuilt_it = rebuilt.find(entity_res.m_uuid);
        if (rebuilt_it == rebuilt.end()) continue;

        dodoe::Entity child = rebuilt_it->second;
        if (!child.hasComponent<dodoe::HierarchyComponent>()) continue;

        auto& child_hierarchy = child.getComponent<dodoe::HierarchyComponent>();

        dodoe::Entity parent;
        auto parent_it = rebuilt.find(child_hierarchy.parent_uuid);
        if (parent_it != rebuilt.end()) {
            parent = parent_it->second;
        } else {
            parent = scene->tryGetEntityByUUID(child_hierarchy.parent_uuid);
        }
        if (!parent.valid()) continue;

        child_hierarchy.parent = parent;
        child_hierarchy.dirty = true;

        if (!parent.hasComponent<dodoe::HierarchyComponent>()) {
            parent.addComponent<dodoe::HierarchyComponent>();
        }
        auto& parent_hierarchy = parent.getComponent<dodoe::HierarchyComponent>();
        const auto child_it = std::find(parent_hierarchy.children.begin(), parent_hierarchy.children.end(), child);
        if (child_it == parent_hierarchy.children.end()) {
            parent_hierarchy.children.push_back(child);
        }
        parent_hierarchy.child_count = static_cast<int>(parent_hierarchy.children.size());
        parent_hierarchy.dirty = true;
    }
}

std::string DeleteEntityCommand::label() const
{
    return "Delete Entity";
}

} // namespace cakery
