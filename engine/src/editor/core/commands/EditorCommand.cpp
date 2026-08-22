// do@Redlive

#include "EditorCommand.h"

#include "core/document/EditorDocumentModel.h"

#include <sstream>
#include <utility>

namespace cakery {

CreateEntityCommand::CreateEntityCommand(std::string name)
    : m_name(std::move(name))
{
}

void CreateEntityCommand::execute(EditorDocumentModel& model) {
    m_createdUuid = model.createEntity(m_name, m_createdUuid);
}

void CreateEntityCommand::revert(EditorDocumentModel& model) {
    model.deleteEntity(m_createdUuid);
}

std::string CreateEntityCommand::label() const {
    return "Create Entity";
}

DeleteEntityCommand::DeleteEntityCommand(std::uint64_t uuid)
    : m_uuid(uuid)
{
}

void DeleteEntityCommand::execute(EditorDocumentModel& model) {
    const std::vector<EditorEntity>& entities = model.entities();
    for (std::size_t i = 0; i < entities.size(); ++i) {
        if (entities[i].uuid == m_uuid) {
            m_index = i;
            m_savedEntity = entities[i];
            break;
        }
    }
    model.deleteEntity(m_uuid);
}

void DeleteEntityCommand::revert(EditorDocumentModel& model) {
    model.insertEntity(m_index, m_savedEntity);
}

std::string DeleteEntityCommand::label() const {
    return "Delete Entity";
}

RenameEntityCommand::RenameEntityCommand(std::uint64_t uuid, std::string newName)
    : m_uuid(uuid), m_newName(std::move(newName))
{
}

void RenameEntityCommand::execute(EditorDocumentModel& model) {
    const EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity) {
        return;
    }
    if (m_oldName.empty()) {
        m_oldName = entity->name;
    }
    model.renameEntity(m_uuid, m_newName);
}

void RenameEntityCommand::revert(EditorDocumentModel& model) {
    model.renameEntity(m_uuid, m_oldName);
}

std::string RenameEntityCommand::label() const {
    return "Rename Entity";
}

AddComponentCommand::AddComponentCommand(std::uint64_t uuid, EditorComponent component)
    : m_uuid(uuid), m_component(std::move(component))
{
}

void AddComponentCommand::execute(EditorDocumentModel& model) {
    const EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity) {
        return;
    }
    m_index = entity->nativeComponents.size();
    model.addComponent(m_uuid, m_component);
}

void AddComponentCommand::revert(EditorDocumentModel& model) {
    model.removeComponent(m_uuid, m_index);
}

std::string AddComponentCommand::label() const {
    return "Add Component";
}

RemoveComponentCommand::RemoveComponentCommand(std::uint64_t uuid, std::size_t nativeIndex)
    : m_uuid(uuid), m_index(nativeIndex)
{
}

void RemoveComponentCommand::execute(EditorDocumentModel& model) {
    const EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity || m_index >= entity->nativeComponents.size()) {
        return;
    }
    if (!m_saved) {
        m_savedComponent = entity->nativeComponents[m_index];
        m_saved = true;
    }
    model.removeComponent(m_uuid, m_index);
}

void RemoveComponentCommand::revert(EditorDocumentModel& model) {
    model.insertComponent(m_uuid, m_index, m_savedComponent);
}

std::string RemoveComponentCommand::label() const {
    return "Remove Component";
}

UpdateComponentCommand::UpdateComponentCommand(std::uint64_t uuid, std::size_t nativeIndex, nlohmann::json newValue)
    : m_uuid(uuid), m_index(nativeIndex), m_newValue(std::move(newValue))
{
}

void UpdateComponentCommand::execute(EditorDocumentModel& model) {
    const EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity || m_index >= entity->nativeComponents.size()) {
        return;
    }
    if (m_oldValue.is_null()) {
        m_oldValue = entity->nativeComponents[m_index].value;
    }
    model.updateComponent(m_uuid, m_index, m_newValue);
}

void UpdateComponentCommand::revert(EditorDocumentModel& model) {
    model.updateComponent(m_uuid, m_index, m_oldValue);
}

std::string UpdateComponentCommand::label() const {
    return "Update Component";
}

bool UpdateComponentCommand::mergeWith(const EditorCommand& next) {
    const auto* other = dynamic_cast<const UpdateComponentCommand*>(&next);
    if (!other) return false;
    if (other->m_uuid != m_uuid || other->m_index != m_index) return false;
    m_newValue = other->m_newValue;
    return true;
}

SetFieldValueCommand::SetFieldValueCommand(std::uint64_t uuid, std::size_t nativeIndex, std::string fieldPath, nlohmann::json newValue)
    : m_uuid(uuid), m_index(nativeIndex), m_fieldPath(std::move(fieldPath)), m_newValue(std::move(newValue))
{
}

void SetFieldValueCommand::execute(EditorDocumentModel& model) {
    EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity || m_index >= entity->nativeComponents.size()) {
        return;
    }
    nlohmann::json& compValue = entity->nativeComponents[m_index].value;
    std::vector<std::string> keys = splitPath(m_fieldPath);
    if (keys.empty()) {
        return;
    }
    if (m_oldValue.is_null()) {
        const nlohmann::json* node = &compValue;
        bool missing = false;
        for (const auto& key : keys) {
            if (!node->contains(key)) {
                missing = true;
                break;
            }
            node = &(*node)[key];
        }
        m_oldValue = missing ? nlohmann::json(nullptr) : *node;
    }
    nlohmann::json updated = compValue;
    nlohmann::json* target = &updated;
    for (const auto& key : keys) {
        target = &(*target)[key];
    }
    *target = m_newValue;
    model.updateComponent(m_uuid, m_index, updated);
}

void SetFieldValueCommand::revert(EditorDocumentModel& model) {
    EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity || m_index >= entity->nativeComponents.size()) {
        return;
    }
    nlohmann::json updated = entity->nativeComponents[m_index].value;
    nlohmann::json* target = &updated;
    for (const auto& key : splitPath(m_fieldPath)) {
        target = &(*target)[key];
    }
    *target = m_oldValue;
    model.updateComponent(m_uuid, m_index, updated);
}

std::string SetFieldValueCommand::label() const {
    return "Set Field";
}

bool SetFieldValueCommand::mergeWith(const EditorCommand& next) {
    const auto* other = dynamic_cast<const SetFieldValueCommand*>(&next);
    if (!other) return false;
    if (other->m_uuid != m_uuid || other->m_index != m_index || other->m_fieldPath != m_fieldPath) return false;
    m_newValue = other->m_newValue;
    return true;
}

std::vector<std::string> SetFieldValueCommand::splitPath(const std::string& path) {
    std::vector<std::string> keys;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '.')) {
        if (!part.empty()) {
            keys.push_back(part);
        }
    }
    return keys;
}

} // namespace cakery
