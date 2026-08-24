// do@Redlive

#include "EditorDocumentModel.h"

#include "EditorDocumentSerializer.h"

#include <cstddef>
#include <random>
#include <utility>

namespace cakery {

namespace {

std::uint64_t GenerateUuid() {
    static std::mt19937_64 engine(std::random_device{}());
    return engine();
}

} // namespace

bool EditorDocumentModel::load(const std::filesystem::path& path) {
    EditorDocument document;
    if (!EditorDocumentSerializer::load(path, document)) {
        return false;
    }
    m_document = std::move(document);
    m_path = path;
    m_hasDocument = true;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::save(const std::filesystem::path& path) const {
    if (!m_hasDocument) {
        return false;
    }
    return EditorDocumentSerializer::save(m_document, path);
}

void EditorDocumentModel::close() {
    m_document = EditorDocument{};
    m_path.clear();
    m_hasDocument = false;
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
    entity.name = name.empty() ? std::string("Entity") : name;
    entity.nativeComponents.push_back(EditorComponent{"IDComponent", {{"id", uuid}, {"name", entity.name}}});
    entity.nativeComponents.push_back(EditorComponent{"TagComponent", {{"tag", "default"}}});
    entity.nativeComponents.push_back(EditorComponent{"TransformComponent",
        {{"position", {0.0, 0.0, 0.0}}, {"rotation", {0.0, 0.0, 0.0}}, {"scale", {1.0, 1.0, 1.0}}}});
    m_document.entities.push_back(std::move(entity));
    notifyChanged();
    return uuid;
}

bool EditorDocumentModel::deleteEntity(std::uint64_t uuid) {
    for (auto it = m_document.entities.begin(); it != m_document.entities.end(); ++it) {
        if (it->uuid == uuid) {
            m_document.entities.erase(it);
            notifyChanged();
            return true;
        }
    }
    return false;
}

bool EditorDocumentModel::reparentEntity(std::uint64_t uuid, std::uint64_t newParent) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity) {
        return false;
    }
    entity->parent = newParent;
    notifyChanged();
    return true;
}

bool EditorDocumentModel::renameEntity(std::uint64_t uuid, const std::string& name) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity) {
        return false;
    }
    entity->name = name.empty() ? std::string("Entity") : name;
    for (auto& component : entity->nativeComponents) {
        if (component.typeName == "IDComponent" && component.value.contains("name")) {
            component.value["name"] = entity->name;
        }
    }
    notifyChanged();
    return true;
}

bool EditorDocumentModel::insertEntity(std::size_t index, const EditorEntity& entity) {
    if (index > m_document.entities.size()) {
        return false;
    }
    m_document.entities.insert(m_document.entities.begin() + static_cast<std::ptrdiff_t>(index), entity);
    notifyChanged();
    return true;
}

void EditorDocumentModel::replaceDocument(const EditorDocument& document) {
    m_document = document;
    m_hasDocument = true;
    notifyChanged();
}

void EditorDocumentModel::newScene(const std::string& name) {
    m_document = EditorDocument{};
    m_document.name = name.empty() ? std::string("Untitled") : name;
    m_path.clear();
    m_hasDocument = true;
    notifyChanged();
}

bool EditorDocumentModel::addComponent(std::uint64_t uuid, const EditorComponent& component) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity) {
        return false;
    }
    entity->nativeComponents.push_back(component);
    notifyChanged();
    return true;
}

bool EditorDocumentModel::insertComponent(std::uint64_t uuid, std::size_t index, const EditorComponent& component) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || index > entity->nativeComponents.size()) {
        return false;
    }
    entity->nativeComponents.insert(entity->nativeComponents.begin() + static_cast<std::ptrdiff_t>(index), component);
    notifyChanged();
    return true;
}

bool EditorDocumentModel::removeComponent(std::uint64_t uuid, std::size_t nativeIndex) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || nativeIndex >= entity->nativeComponents.size()) {
        return false;
    }
    entity->nativeComponents.erase(entity->nativeComponents.begin() + static_cast<std::ptrdiff_t>(nativeIndex));
    notifyChanged();
    return true;
}

bool EditorDocumentModel::updateComponent(std::uint64_t uuid, std::size_t nativeIndex, const nlohmann::json& value) {
    EditorEntity* entity = findEntity(uuid);
    if (!entity || nativeIndex >= entity->nativeComponents.size()) {
        return false;
    }
    entity->nativeComponents[nativeIndex].value = value;
    notifyChanged();
    return true;
}

void EditorDocumentModel::subscribe(std::function<void()> onChange) {
    m_observers.push_back(std::move(onChange));
}

void EditorDocumentModel::notifyChanged() {
    for (const auto& observer : m_observers) {
        observer();
    }
}

} // namespace cakery
