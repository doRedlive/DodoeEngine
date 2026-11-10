// do@Redlive

#include "dopch.h"

#include "EditorCommand.h"

#include "core/document/EditorDocumentModel.h"

#include <algorithm>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace cakery {

CreateEntityCommand::CreateEntityCommand(String name)
    : m_name(std::move(name))
{
}

bool CreateEntityCommand::execute(EditorDocumentModel& model) {
    m_createdUuid = model.createEntity(m_name, m_createdUuid);
    return m_createdUuid != 0;
}

void CreateEntityCommand::revert(EditorDocumentModel& model) {
    model.deleteEntity(m_createdUuid);
}

String CreateEntityCommand::label() const {
    return "Create GameObject";
}

DeleteEntityCommand::DeleteEntityCommand(UInt64 uuid)
    : m_uuid(uuid)
{
}

bool DeleteEntityCommand::execute(EditorDocumentModel& model) {
    if (!model.findEntity(m_uuid)) {
        return false;
    }
    if (!m_captured) {
        const DynamicArray<EditorEntity>& entities = model.entities();
        UnorderedSet<UInt64> subtree{m_uuid};
        bool expanded = true;
        while (expanded) {
            expanded = false;
            for (const auto& entity : entities) {
                if (subtree.contains(entity.parent) && subtree.insert(entity.uuid).second) {
                    expanded = true;
                }
            }
        }
        for (Size_t i = 0; i < entities.size(); ++i) {
            if (subtree.contains(entities[i].uuid)) {
                m_savedEntities.push_back({i, entities[i]});
            }
        }
        m_captured = true;
    }
    model.deleteEntity(m_uuid);
    return true;
}

void DeleteEntityCommand::revert(EditorDocumentModel& model) {
    for (const SavedEntity& saved : m_savedEntities) {
        model.insertEntity(std::min(saved.index, model.entities().size()), saved.entity);
    }
}

String DeleteEntityCommand::label() const {
    return "Delete GameObject";
}

RenameEntityCommand::RenameEntityCommand(UInt64 uuid, String newName)
    : m_uuid(uuid), m_newName(std::move(newName))
{
}

bool RenameEntityCommand::execute(EditorDocumentModel& model) {
    const EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity) {
        return false;
    }
    if (m_oldName.empty()) {
        m_oldName = entity->name;
    }
    model.renameEntity(m_uuid, m_newName);
    return true;
}

void RenameEntityCommand::revert(EditorDocumentModel& model) {
    model.renameEntity(m_uuid, m_oldName);
}

String RenameEntityCommand::label() const {
    return "Rename GameObject";
}

AddComponentCommand::AddComponentCommand(UInt64 uuid, EditorComponent component)
    : m_uuid(uuid), m_component(std::move(component))
{
}

bool AddComponentCommand::execute(EditorDocumentModel& model) {
    const EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity) {
        return false;
    }
    m_index = entity->nativeComponents.size();
    model.addComponent(m_uuid, m_component);
    return true;
}

void AddComponentCommand::revert(EditorDocumentModel& model) {
    model.removeComponent(m_uuid, m_index);
}

String AddComponentCommand::label() const {
    return "Add Component";
}

RemoveComponentCommand::RemoveComponentCommand(UInt64 uuid, Size_t nativeIndex)
    : m_uuid(uuid), m_index(nativeIndex)
{
}

bool RemoveComponentCommand::execute(EditorDocumentModel& model) {
    const EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity || m_index >= entity->nativeComponents.size()) {
        return false;
    }
    if (!m_saved) {
        m_savedComponent = entity->nativeComponents[m_index];
        m_saved = true;
    }
    model.removeComponent(m_uuid, m_index);
    return true;
}

void RemoveComponentCommand::revert(EditorDocumentModel& model) {
    model.insertComponent(m_uuid, m_index, m_savedComponent);
}

String RemoveComponentCommand::label() const {
    return "Remove Component";
}

UpdateComponentCommand::UpdateComponentCommand(UInt64 uuid, Size_t nativeIndex, Json newValue)
    : m_uuid(uuid), m_index(nativeIndex), m_newValue(std::move(newValue))
{
}

bool UpdateComponentCommand::execute(EditorDocumentModel& model) {
    const EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity || m_index >= entity->nativeComponents.size()) {
        return false;
    }
    if (m_oldValue.is_null()) {
        m_oldValue = entity->nativeComponents[m_index].value;
    }
    model.updateComponent(m_uuid, m_index, m_newValue);
    return true;
}

void UpdateComponentCommand::revert(EditorDocumentModel& model) {
    model.updateComponent(m_uuid, m_index, m_oldValue);
}

String UpdateComponentCommand::label() const {
    return "Update Component";
}

bool UpdateComponentCommand::mergeWith(const EditorCommand& next) {
    const auto* other = dynamic_cast<const UpdateComponentCommand*>(&next);
    if (!other) return false;
    if (other->m_uuid != m_uuid || other->m_index != m_index) return false;
    m_newValue = other->m_newValue;
    return true;
}

RemoveManagedComponentCommand::RemoveManagedComponentCommand(UInt64 uuid, Size_t index)
    : m_uuid(uuid), m_index(index)
{
}

bool RemoveManagedComponentCommand::execute(EditorDocumentModel& model) {
    const EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity || m_index >= entity->managedComponents.size()) {
        return false;
    }
    if (!m_saved) {
        m_savedComponent = entity->managedComponents[m_index];
        m_saved = true;
    }
    model.removeManagedComponent(m_uuid, m_index);
    return true;
}

void RemoveManagedComponentCommand::revert(EditorDocumentModel& model) {
    model.insertManagedComponent(m_uuid, m_index, m_savedComponent);
}

String RemoveManagedComponentCommand::label() const {
    return "Remove Managed Component";
}

UpdateManagedComponentCommand::UpdateManagedComponentCommand(
    UInt64 uuid, Size_t index, Json newValue)
    : m_uuid(uuid), m_index(index), m_newValue(std::move(newValue))
{
}

bool UpdateManagedComponentCommand::execute(EditorDocumentModel& model) {
    const EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity || m_index >= entity->managedComponents.size()) {
        return false;
    }
    if (m_oldValue.is_null()) {
        m_oldValue = entity->managedComponents[m_index].value;
    }
    model.updateManagedComponent(m_uuid, m_index, m_newValue);
    return true;
}

void UpdateManagedComponentCommand::revert(EditorDocumentModel& model) {
    model.updateManagedComponent(m_uuid, m_index, m_oldValue);
}

String UpdateManagedComponentCommand::label() const {
    return "Update Managed Component";
}

bool UpdateManagedComponentCommand::mergeWith(const EditorCommand& next) {
    const auto* other = dynamic_cast<const UpdateManagedComponentCommand*>(&next);
    if (!other || other->m_uuid != m_uuid || other->m_index != m_index) {
        return false;
    }
    m_newValue = other->m_newValue;
    return true;
}

SetFieldValueCommand::SetFieldValueCommand(UInt64 uuid, Size_t nativeIndex, String fieldPath, Json newValue)
    : m_uuid(uuid), m_index(nativeIndex), m_fieldPath(std::move(fieldPath)), m_newValue(std::move(newValue))
{
}

bool SetFieldValueCommand::execute(EditorDocumentModel& model) {
    EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity || m_index >= entity->nativeComponents.size()) {
        return false;
    }
    Json& compValue = entity->nativeComponents[m_index].value;
    DynamicArray<String> keys = splitPath(m_fieldPath);
    if (keys.empty()) {
        return false;
    }
    if (m_oldValue.is_null()) {
        const Json* node = &compValue;
        bool missing = false;
        for (const auto& key : keys) {
            if (!node->contains(key)) {
                missing = true;
                break;
            }
            node = &(*node)[key];
        }
        m_oldValue = missing ? Json(nullptr) : *node;
    }
    Json updated = compValue;
    Json* target = &updated;
    for (const auto& key : keys) {
        target = &(*target)[key];
    }
    *target = m_newValue;
    model.updateComponent(m_uuid, m_index, updated);
    return true;
}

void SetFieldValueCommand::revert(EditorDocumentModel& model) {
    EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity || m_index >= entity->nativeComponents.size()) {
        return;
    }
    Json updated = entity->nativeComponents[m_index].value;
    Json* target = &updated;
    for (const auto& key : splitPath(m_fieldPath)) {
        target = &(*target)[key];
    }
    *target = m_oldValue;
    model.updateComponent(m_uuid, m_index, updated);
}

String SetFieldValueCommand::label() const {
    return "Set Field";
}

bool SetFieldValueCommand::mergeWith(const EditorCommand& next) {
    const auto* other = dynamic_cast<const SetFieldValueCommand*>(&next);
    if (!other) return false;
    if (other->m_uuid != m_uuid || other->m_index != m_index || other->m_fieldPath != m_fieldPath) return false;
    m_newValue = other->m_newValue;
    return true;
}

ReparentDocumentCommand::ReparentDocumentCommand(UInt64 uuid, UInt64 newParent)
    : m_uuid(uuid), m_newParent(newParent)
{
}

bool ReparentDocumentCommand::execute(EditorDocumentModel& model) {
    const EditorEntity* entity = model.findEntity(m_uuid);
    if (!entity) {
        return false;
    }
    if (!m_captured) {
        m_oldParent = entity->parent;
        m_captured = true;
    }
    model.reparentEntity(m_uuid, m_newParent);
    return true;
}

void ReparentDocumentCommand::revert(EditorDocumentModel& model) {
    model.reparentEntity(m_uuid, m_oldParent);
}

String ReparentDocumentCommand::label() const {
    return "Reparent GameObject";
}

MoveComponentCommand::MoveComponentCommand(UInt64 uuid, Size_t fromIndex, Size_t toIndex)
    : m_uuid(uuid)
    , m_fromIndex(fromIndex)
    , m_toIndex(toIndex)
{
}

bool MoveComponentCommand::execute(EditorDocumentModel& model) {
    return model.moveComponent(m_uuid, m_fromIndex, m_toIndex);
}

void MoveComponentCommand::revert(EditorDocumentModel& model) {
    model.moveComponent(m_uuid, m_toIndex, m_fromIndex);
}

String MoveComponentCommand::label() const {
    return "Move Component";
}

InsertEntitiesCommand::InsertEntitiesCommand(DynamicArray<EditorEntity> entities)
    : m_entities(std::move(entities))
{
}

bool InsertEntitiesCommand::execute(EditorDocumentModel& model) {
    if (m_executed || m_entities.empty()) {
        return false;
    }
    m_uuids.clear();
    m_uuids.reserve(m_entities.size());
    for (const EditorEntity& entity : m_entities) {
        if (model.findEntity(entity.uuid) || !model.insertEntity(model.entities().size(), entity)) {
            for (auto it = m_uuids.rbegin(); it != m_uuids.rend(); ++it) {
                model.deleteEntity(*it);
            }
            m_uuids.clear();
            return false;
        }
        m_uuids.push_back(entity.uuid);
    }
    m_executed = true;
    return true;
}

void InsertEntitiesCommand::revert(EditorDocumentModel& model) {
    if (!m_executed) {
        return;
    }
    for (auto it = m_uuids.rbegin(); it != m_uuids.rend(); ++it) {
        model.deleteEntity(*it);
    }
    m_executed = false;
}

String InsertEntitiesCommand::label() const {
    return m_entities.size() == 1 ? "Duplicate GameObject" : "Duplicate GameObjects";
}

DynamicArray<String> SetFieldValueCommand::splitPath(const String& path) {
    DynamicArray<String> keys;
    std::stringstream ss(path);
    String part;
    while (std::getline(ss, part, '.')) {
        if (!part.empty()) {
            keys.push_back(part);
        }
    }
    return keys;
}

} // namespace cakery
