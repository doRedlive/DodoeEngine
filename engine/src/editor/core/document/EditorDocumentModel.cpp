// do@Redlive

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

std::uint64_t GenerateUuid() {
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
    std::unordered_map<std::uint64_t, std::size_t> childCounts;
    for (const auto& entity : document.entities) {
        if (entity.parent != 0) {
            ++childCounts[entity.parent];
        }
    }

    for (auto& entity : document.entities) {
        const auto childCountIt = childCounts.find(entity.uuid);
        const std::size_t childCount = childCountIt == childCounts.end() ? 0 : childCountIt->second;
        EditorComponent* hierarchy = FindHierarchyComponent(entity);
        if (entity.parent == 0 && childCount == 0 && !hierarchy) {
            continue;
        }
        if (!hierarchy) {
            entity.nativeComponents.push_back(EditorComponent{"HierarchyComponent", nlohmann::json::object()});
            hierarchy = &entity.nativeComponents.back();
        }
        hierarchy->value["parent_uuid"] = entity.parent;
        hierarchy->value["child_count"] = childCount;
    }
}

} // namespace

bool EditorDocumentModel::load(const std::filesystem::path& path) {
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

bool EditorDocumentModel::save(const std::filesystem::path& path) {
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

const std::string& EditorDocumentModel::name() const {
    return m_document.name;
}

EditorEntity* EditorDocumentModel::findEntity(std::uint64_t uuid) {
    for (auto& entity : m_document.entities) {
        if (entity.uuid == uuid) {
            return &entity;
        }
    }
    return nullptr;
}

const EditorEntity* EditorDocumentModel::findEntity(std::uint64_t uuid) const {
    for (const auto& entity : m_document.entities) {
        if (entity.uuid == uuid) {
            return &entity;
        }
    }
    return nullptr;
}

std::uint64_t EditorDocumentModel::createEntity(const std::string& name, std::uint64_t preferredUuid) {
    std::uint64_t uuid = preferredUuid != 0 ? preferredUuid : GenerateUuid();
    while (findEntity(uuid) != nullptr) {
        uuid = GenerateUuid();
    }

    EditorEntity entity;
    entity.uuid = uuid;
    entity.name = name.empty() ? std::string("GameObject") : name;
    entity.nativeComponents.push_back(EditorComponent{"IDComponent", {{"id", uuid}, {"name", entity.name}}});
    entity.nativeComponents.push_back(EditorComponent{"TagComponent", {{"tag", "default"}}});
    entity.nativeComponents.push_back(EditorComponent{"TransformComponent",
        {{"position", {0.0, 0.0, 0.0}}, {"rotation", {0.0, 0.0, 0.0}}, {"scale", {1.0, 1.0, 1.0}}}});
    m_document.entities.push_back(std::move(entity));
    m_dirty = true;
    notifyChanged();
    return uuid;
}

bool EditorDocumentModel::deleteEntity(std::uint64_t uuid) {
    if (!findEntity(uuid)) {
        return false;
    }

    std::unordered_set<std::uint64_t> subtree{uuid};
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

bool EditorDocumentModel::reparentEntity(std::uint64_t uuid, std::uint64_t newParent) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity) {
        return false;
    }
    if (newParent == uuid || (newParent != 0 && !findEntity(newParent))) {
        return false;
    }
    for (std::uint64_t ancestor = newParent; ancestor != 0;) {
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

bool EditorDocumentModel::renameEntity(std::uint64_t uuid, const std::string& name) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity) {
        return false;
    }
    entity->name = name.empty() ? std::string("GameObject") : name;
    for (auto& component : entity->nativeComponents) {
        if (component.typeName == "IDComponent" && component.value.contains("name")) {
            component.value["name"] = entity->name;
        }
    }
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::insertEntity(std::size_t index, const EditorEntity& entity) {
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

void EditorDocumentModel::newScene(const std::string& name) {
    m_document = EditorDocument{};
    m_document.name = name.empty() ? std::string("Untitled") : name;
    m_path.clear();
    m_hasDocument = true;
    m_dirty = true;
    notifyChanged();
}

bool EditorDocumentModel::addComponent(std::uint64_t uuid, const EditorComponent& component) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity) {
        return false;
    }
    entity->nativeComponents.push_back(component);
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::insertComponent(std::uint64_t uuid, std::size_t index, const EditorComponent& component) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || index > entity->nativeComponents.size()) {
        return false;
    }
    entity->nativeComponents.insert(entity->nativeComponents.begin() + static_cast<std::ptrdiff_t>(index), component);
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::removeComponent(std::uint64_t uuid, std::size_t nativeIndex) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || nativeIndex >= entity->nativeComponents.size()) {
        return false;
    }
    entity->nativeComponents.erase(entity->nativeComponents.begin() + static_cast<std::ptrdiff_t>(nativeIndex));
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::updateComponent(std::uint64_t uuid, std::size_t nativeIndex, const nlohmann::json& value) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || nativeIndex >= entity->nativeComponents.size()) {
        return false;
    }
    entity->nativeComponents[nativeIndex].value = value;
    m_dirty = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::insertManagedComponent(std::uint64_t uuid, std::size_t index,
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

bool EditorDocumentModel::removeManagedComponent(std::uint64_t uuid, std::size_t index) {
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

bool EditorDocumentModel::updateManagedComponent(std::uint64_t uuid, std::size_t index,
                                                  const nlohmann::json& value) {
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
