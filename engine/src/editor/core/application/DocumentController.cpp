// do@Redlive

#include "dopch.h"

#include "DocumentController.h"

#include "core/bridge/EditorCommands.h"

namespace cakery {

DocumentController::DocumentController(EditorSession& session, DomainEventBus& events)
    : m_session(session), m_events(events)
{
    m_historySubscription = m_session.history().subscribe([this]() { historyChanged.fire(); });
    m_documentSubscription = m_session.documentModel().subscribe(
        [this]() { documentChanged.fire(); });
}

DocumentController::~DocumentController() = default;

EditorDocumentModel& DocumentController::documentModel()
{
    return m_session.documentModel();
}

const EditorDocumentModel& DocumentController::documentModel() const
{
    return m_session.documentModel();
}

EditorHistory& DocumentController::history()
{
    return m_session.history();
}

const EditorHistory& DocumentController::history() const
{
    return m_session.history();
}

bool DocumentController::openProject(ProjectDescriptor project)
{
    return m_session.openProject(std::move(project));
}

bool DocumentController::openDocument(const String& path)
{
    return m_session.openDocument(path);
}

bool DocumentController::saveDocument(const String& path)
{
    return report(m_session.saveDocument(path), "Save document");
}

bool DocumentController::newScene(const FsPath& directory, const String& name)
{
    return report(m_session.newScene(directory, name), "New scene");
}

UInt64 DocumentController::createEntity(const String& name)
{
    return m_session.createEntity(name);
}

bool DocumentController::deleteEntity(UInt64 uuid)
{
    return report(m_session.deleteEntity(uuid), "Delete entity");
}

bool DocumentController::deleteEntities(const DynamicArray<UInt64>& uuids)
{
    return report(m_session.deleteEntities(uuids), "Delete entities");
}

bool DocumentController::renameEntity(UInt64 uuid, const String& name)
{
    return report(m_session.renameEntity(uuid, name), "Rename entity");
}

bool DocumentController::reparentEntity(UInt64 uuid, UInt64 parentUuid)
{
    return report(m_session.reparentEntity(uuid, parentUuid), "Reparent entity");
}

bool DocumentController::copyEntities(const DynamicArray<UInt64>& uuids)
{
    return report(m_session.copyEntities(uuids), "Copy entities");
}

bool DocumentController::cutEntities(const DynamicArray<UInt64>& uuids)
{
    return report(m_session.cutEntities(uuids), "Cut entities");
}

bool DocumentController::pasteEntities()
{
    return report(m_session.pasteEntities(), "Paste entities");
}

bool DocumentController::duplicateEntities(const DynamicArray<UInt64>& uuids)
{
    return report(m_session.duplicateEntities(uuids), "Duplicate entities");
}

bool DocumentController::hasEntityClipboard() const
{
    return m_session.hasEntityClipboard();
}

bool DocumentController::addComponent(UInt64 uuid, const EditorComponent& component)
{
    return report(m_session.addComponent(uuid, component), "Add component");
}

bool DocumentController::removeComponent(UInt64 uuid, Size_t index)
{
    return report(m_session.removeComponent(uuid, index), "Remove component");
}

bool DocumentController::moveComponent(UInt64 uuid, Size_t index, int delta)
{
    return report(m_session.moveComponent(uuid, index, delta), "Move component");
}

bool DocumentController::updateComponent(UInt64 uuid, Size_t index, const Json& value)
{
    return report(m_session.updateComponent(uuid, index, value), "Update component");
}

bool DocumentController::updateManagedComponent(UInt64 uuid, Size_t index, const Json& value)
{
    return report(m_session.updateManagedComponent(uuid, index, value), "Update managed component");
}

bool DocumentController::removeManagedComponent(UInt64 uuid, Size_t index)
{
    return report(m_session.removeManagedComponent(uuid, index), "Remove managed component");
}

bool DocumentController::updateComponentOnEntities(const DynamicArray<UInt64>& uuids,
                                                   const String& typeName,
                                                   const Json& value, bool managed)
{
    return report(m_session.updateComponentOnEntities(uuids, typeName, value, managed),
                  "Update components");
}

bool DocumentController::updateComponentFieldOnEntities(const DynamicArray<UInt64>& uuids,
                                                        const String& typeName,
                                                        const String& fieldPath,
                                                        const Json& value, bool managed)
{
    return report(m_session.updateComponentFieldOnEntities(uuids, typeName, fieldPath, value, managed),
                  "Update component field");
}

bool DocumentController::inspectComponent(const String& typeName,
                                          DynamicArray<InspectorFieldMetadata>& fields) const
{
    return m_session.inspectComponent(typeName, fields);
}

bool DocumentController::getCustomInspectorUI(const String& typeName, Json& out) const
{
    return m_session.getCustomInspectorUI(typeName, out);
}

bool DocumentController::dispatchEditorEvent(const String& owner, const String& ownerId,
                                             const String& controlId, const String& eventName,
                                             const Json& value)
{
    return m_session.dispatchEditorEvent(owner, ownerId, controlId, eventName, value);
}

bool DocumentController::exportPrefab(UInt64 uuid, const String& path)
{
    return report(m_session.execute(PrefabExportCommand{uuid, path}), "Export prefab");
}

bool DocumentController::undo()
{
    return report(m_session.undo(), "Undo");
}

bool DocumentController::redo()
{
    return report(m_session.redo(), "Redo");
}

bool DocumentController::report(bool ok, const char* action)
{
    if (ok) {
        return true;
    }
    NotificationEvent event;
    event.level = "error";
    event.message = action;
    event.source = m_session.diagnostic();
    m_events.publish(event);
    return false;
}

} // namespace cakery
