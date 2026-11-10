// do@Redlive

#include "dopch.h"

#include "DocumentController.h"

#include "core/bridge/EditorCommands.h"

namespace cakery {

DocumentController::DocumentController(EditorSession& session)
    : m_session(session)
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
    return m_session.saveDocument(path);
}

bool DocumentController::newScene(const FsPath& directory, const String& name)
{
    return m_session.newScene(directory, name);
}

UInt64 DocumentController::createEntity(const String& name)
{
    return m_session.createEntity(name);
}

bool DocumentController::deleteEntity(UInt64 uuid)
{
    return m_session.deleteEntity(uuid);
}

bool DocumentController::deleteEntities(const DynamicArray<UInt64>& uuids)
{
    return m_session.deleteEntities(uuids);
}

bool DocumentController::renameEntity(UInt64 uuid, const String& name)
{
    return m_session.renameEntity(uuid, name);
}

bool DocumentController::reparentEntity(UInt64 uuid, UInt64 parentUuid)
{
    return m_session.reparentEntity(uuid, parentUuid);
}

bool DocumentController::copyEntities(const DynamicArray<UInt64>& uuids)
{
    return m_session.copyEntities(uuids);
}

bool DocumentController::cutEntities(const DynamicArray<UInt64>& uuids)
{
    return m_session.cutEntities(uuids);
}

bool DocumentController::pasteEntities()
{
    return m_session.pasteEntities();
}

bool DocumentController::duplicateEntities(const DynamicArray<UInt64>& uuids)
{
    return m_session.duplicateEntities(uuids);
}

bool DocumentController::hasEntityClipboard() const
{
    return m_session.hasEntityClipboard();
}

bool DocumentController::addComponent(UInt64 uuid, const EditorComponent& component)
{
    return m_session.addComponent(uuid, component);
}

bool DocumentController::removeComponent(UInt64 uuid, Size_t index)
{
    return m_session.removeComponent(uuid, index);
}

bool DocumentController::moveComponent(UInt64 uuid, Size_t index, int delta)
{
    return m_session.moveComponent(uuid, index, delta);
}

bool DocumentController::updateComponent(UInt64 uuid, Size_t index, const Json& value)
{
    return m_session.updateComponent(uuid, index, value);
}

bool DocumentController::updateManagedComponent(UInt64 uuid, Size_t index, const Json& value)
{
    return m_session.updateManagedComponent(uuid, index, value);
}

bool DocumentController::removeManagedComponent(UInt64 uuid, Size_t index)
{
    return m_session.removeManagedComponent(uuid, index);
}

bool DocumentController::updateComponentOnEntities(const DynamicArray<UInt64>& uuids,
                                                   const String& typeName,
                                                   const Json& value, bool managed)
{
    return m_session.updateComponentOnEntities(uuids, typeName, value, managed);
}

bool DocumentController::updateComponentFieldOnEntities(const DynamicArray<UInt64>& uuids,
                                                        const String& typeName,
                                                        const String& fieldPath,
                                                        const Json& value, bool managed)
{
    return m_session.updateComponentFieldOnEntities(uuids, typeName, fieldPath, value, managed);
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
    return m_session.execute(PrefabExportCommand{uuid, path});
}

bool DocumentController::undo()
{
    return m_session.undo();
}

bool DocumentController::redo()
{
    return m_session.redo();
}

} // namespace cakery
