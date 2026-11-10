// do@Redlive

#include "dopch.h"

#include "EditorDocumentModel.h"

#include "EditorDocumentSerializer.h"

#include <algorithm>
#include <cstddef>
#include <random>
#include <unordered_set>
#include <unordered_map>
#include <utility>

namespace cakery {

namespace {

UInt64 GenerateUuid() {
    static std::mt19937_64 engine(std::random_device{}());
    return engine();
}

EditorComponent* FindHierarchyComponent(EditorEntity& entity)
{
    for (auto& component : entity.nativeComponents) {
        if (component.typeName == "HierarchyComponent") {
            return &component;
        }
    }
    return nullptr;
}

void SyncHierarchyComponents(EditorDocument& document)
{
    UnorderedMap<UInt64, Size_t> childCounts;
    for (const auto& entity : document.entities) {
        if (entity.parent != 0) {
            ++childCounts[entity.parent];
        }
    }

    for (auto& entity : document.entities) {
        const auto childCountIt = childCounts.find(entity.uuid);
        const Size_t childCount = childCountIt == childCounts.end() ? 0 : childCountIt->second;
        EditorComponent* hierarchy = FindHierarchyComponent(entity);
        if (entity.parent == 0 && childCount == 0 && !hierarchy) {
            continue;
        }
        if (!hierarchy) {
            entity.nativeComponents.push_back(EditorComponent{"HierarchyComponent", Json::object()});
            hierarchy = &entity.nativeComponents.back();
        }
        hierarchy->value["parent_uuid"] = entity.parent;
        hierarchy->value["child_count"] = childCount;
    }
}

} // namespace

bool EditorDocumentModel::load(const FsPath& path) {
    EditorDocument document;
    if (!EditorDocumentSerializer::load(path, document)) {
        return false;
    }
    m_document = std::move(document);
    SyncHierarchyComponents(m_document);
    m_path = path;
    m_hasDocument = true;
    m_dirty = false;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::save(const FsPath& path) {
    if (!m_hasDocument) {
        return false;
    }
    if (path.empty() || !EditorDocumentSerializer::save(m_document, path)) {
        return false;
    }
    m_path = path;
    m_dirty = false;
    notifyChanged();
    return true;
}

void EditorDocumentModel::close() {
    m_document = EditorDocument{};
    m_path.clear();
    m_hasDocument = false;
    m_dirty = false;
    notifyChanged();
}

const String& EditorDocumentModel::name() const {
    return m_document.name;
}

EditorEntity* EditorDocumentModel::findEntity(UInt64 uuid) {
    for (auto& entity : m_document.entities) {
        if (entity.uuid == uuid) {
            return &entity;
        }
    }
    return nullptr;
}

bool EditorDocumentModel::isDescendantOf(UInt64 uuid, UInt64 ancestorUuid) const {
    const EditorEntity* entity = findEntity(uuid);
    if (!entity || ancestorUuid == 0) {
        return false;
    }
    UInt64 parent = entity->parent;
    int depth = 0;
    while (parent != 0 && depth < 256) {
        if (parent == ancestorUuid) {
            return true;
        }
        const EditorEntity* parentEntity = findEntity(parent);
        if (!parentEntity) {
            return false;
        }
        parent = parentEntity->parent;
        ++depth;
    }
    return false;
}

const EditorEntity* EditorDocumentModel::findEntity(UInt64 uuid) const {
    for (const auto& entity : m_document.entities) {
        if (entity.uuid == uuid) {
            return &entity;
        }
    }
    return nullptr;
}

UInt64 EditorDocumentModel::generateEntityUuid() const {
    UInt64 uuid = GenerateUuid();
    while (findEntity(uuid) != nullptr) {
        uuid = GenerateUuid();
    }
    return uuid;
}

UInt64 EditorDocumentModel::createEntity(const String& name, UInt64 preferredUuid) {
    UInt64 uuid = preferredUuid != 0 ? preferredUuid : GenerateUuid();
    while (findEntity(uuid) != nullptr) {
        uuid = GenerateUuid();
    }

    EditorEntity entity;
    entity.uuid = uuid;
    entity.name = name.empty() ? String("GameObject") : name;
    entity.nativeComponents.push_back(EditorComponent{"IDComponent", {{"id", uuid}, {"name", entity.name}}});
    entity.nativeComponents.push_back(EditorComponent{"TagComponent", {{"tag", "default"}}});
    entity.nativeComponents.push_back(EditorComponent{"TransformComponent",
        {{"position", {0.0, 0.0, 0.0}}, {"rotation", {0.0, 0.0, 0.0}}, {"scale", {1.0, 1.0, 1.0}}}});
    entity.nativeComponents.push_back(EditorComponent{"ActiveComponent", {{"active_self", true}}});
    m_document.entities.push_back(std::move(entity));
    m_dirty = true;
    notifyChanged();
    return uuid;
}

bool EditorDocumentModel::deleteEntity(UInt64 uuid) {
    if (!findEntity(uuid)) {
        return false;
    }

    UnorderedSet<UInt64> subtree{uuid};
    bool expanded = true;
    while (expanded) {
        expanded = false;
        for (const auto& entity : m_document.entities) {
            if (subtree.contains(entity.parent) && subtree.insert(entity.uuid).second) {
                expanded = true;
            }
        }
    }

    const auto oldSize = m_document.entities.size();
    m_document.entities.erase(
        std::remove_if(m_document.entities.begin(), m_document.entities.end(),
                       [&subtree](const EditorEntity& entity) {
                           return subtree.contains(entity.uuid);
                       }),
        m_document.entities.end());
    if (m_document.entities.size() == oldSize) {
        return false;
    }
    SyncHierarchyComponents(m_document);
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::reparentEntity(UInt64 uuid, UInt64 newParent) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity) {
        return false;
    }
    if (newParent == uuid || (newParent != 0 && !findEntity(newParent))) {
        return false;
    }
    for (UInt64 ancestor = newParent; ancestor != 0;) {
        if (ancestor == uuid) {
            return false;
        }
        const EditorEntity* parent = findEntity(ancestor);
        if (!parent) {
            break;
        }
        ancestor = parent->parent;
    }
    if (entity->parent == newParent) {
        return true;
    }
    entity->parent = newParent;
    SyncHierarchyComponents(m_document);
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::renameEntity(UInt64 uuid, const String& name) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity) {
        return false;
    }
    entity->name = name.empty() ? String("GameObject") : name;
    for (auto& component : entity->nativeComponents) {
        if (component.typeName == "IDComponent" && component.value.contains("name")) {
            component.value["name"] = entity->name;
        }
    }
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::insertEntity(Size_t index, const EditorEntity& entity) {
    if (index > m_document.entities.size()) {
        return false;
    }
    m_document.entities.insert(m_document.entities.begin() + static_cast<std::ptrdiff_t>(index), entity);
    SyncHierarchyComponents(m_document);
    m_dirty = true;
    notifyChanged();
    return true;
}

void EditorDocumentModel::replaceDocument(const EditorDocument& document) {
    m_document = document;
    SyncHierarchyComponents(m_document);
    m_hasDocument = true;
    m_dirty = true;
    notifyChanged();
}

void EditorDocumentModel::restoreDocument(const EditorDocument& document, bool dirty) {
    m_document = document;
    SyncHierarchyComponents(m_document);
    m_hasDocument = true;
    m_dirty = dirty;
    notifyChanged();
}

void EditorDocumentModel::newScene(const String& name) {
    m_document = EditorDocument{};
    m_document.name = name.empty() ? String("Untitled") : name;
    m_path.clear();
    m_hasDocument = true;
    m_dirty = true;
    notifyChanged();
}

bool EditorDocumentModel::addComponent(UInt64 uuid, const EditorComponent& component) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity) {
        return false;
    }
    entity->nativeComponents.push_back(component);
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::insertComponent(UInt64 uuid, Size_t index, const EditorComponent& component) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || index > entity->nativeComponents.size()) {
        return false;
    }
    entity->nativeComponents.insert(entity->nativeComponents.begin() + static_cast<std::ptrdiff_t>(index), component);
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::moveComponent(UInt64 uuid, Size_t fromIndex, Size_t toIndex) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || fromIndex >= entity->nativeComponents.size() ||
        toIndex >= entity->nativeComponents.size() || fromIndex == toIndex) {
        return false;
    }
    std::swap(entity->nativeComponents[fromIndex], entity->nativeComponents[toIndex]);
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::removeComponent(UInt64 uuid, Size_t nativeIndex) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || nativeIndex >= entity->nativeComponents.size()) {
        return false;
    }
    entity->nativeComponents.erase(entity->nativeComponents.begin() + static_cast<std::ptrdiff_t>(nativeIndex));
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::updateComponent(UInt64 uuid, Size_t nativeIndex, const Json& value) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || nativeIndex >= entity->nativeComponents.size()) {
        return false;
    }
    entity->nativeComponents[nativeIndex].value = value;
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::insertManagedComponent(UInt64 uuid, Size_t index,
                                                  const EditorComponent& component) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || index > entity->managedComponents.size()) {
        return false;
    }
    entity->managedComponents.insert(
        entity->managedComponents.begin() + static_cast<std::ptrdiff_t>(index), component);
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::removeManagedComponent(UInt64 uuid, Size_t index) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || index >= entity->managedComponents.size()) {
        return false;
    }
    entity->managedComponents.erase(
        entity->managedComponents.begin() + static_cast<std::ptrdiff_t>(index));
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::updateManagedComponent(UInt64 uuid, Size_t index,
                                                  const Json& value) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || index >= entity->managedComponents.size()) {
        return false;
    }
    entity->managedComponents[index].value = value;
    m_dirty = true;
    notifyChanged();
    return true;
}

ScopedConnection EditorDocumentModel::subscribe(std::function<void()> onChange) {
    return ScopedConnection(m_changed, m_changed.connect(std::move(onChange)));
}

void EditorDocumentModel::notifyChanged() {
    m_changed.fire();
}

} // namespace cakery
