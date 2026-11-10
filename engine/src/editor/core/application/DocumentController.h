// do@Redlive
#pragma once
#include "dopch.h"

#include "core/EditorSession.h"
#include "core/Signal.h"
#include "core/application/DomainEventBus.h"

namespace cakery {

class DocumentController {
public:
    DocumentController(EditorSession& session, DomainEventBus& events);
    ~DocumentController();

    EditorDocumentModel& documentModel();
    const EditorDocumentModel& documentModel() const;
    EditorHistory& history();
    const EditorHistory& history() const;

    bool openProject(ProjectDescriptor project);
    bool openDocument(const String& path);
    bool saveDocument(const String& path);
    bool newScene(const FsPath& directory, const String& name);

    UInt64 createEntity(const String& name);
    bool deleteEntity(UInt64 uuid);
    bool deleteEntities(const DynamicArray<UInt64>& uuids);
    bool renameEntity(UInt64 uuid, const String& name);
    bool reparentEntity(UInt64 uuid, UInt64 parentUuid);
    bool copyEntities(const DynamicArray<UInt64>& uuids);
    bool cutEntities(const DynamicArray<UInt64>& uuids);
    bool pasteEntities();
    bool duplicateEntities(const DynamicArray<UInt64>& uuids);
    bool hasEntityClipboard() const;

    bool addComponent(UInt64 uuid, const EditorComponent& component);
    bool removeComponent(UInt64 uuid, Size_t index);
    bool moveComponent(UInt64 uuid, Size_t index, int delta);
    bool updateComponent(UInt64 uuid, Size_t index, const Json& value);
    bool updateManagedComponent(UInt64 uuid, Size_t index, const Json& value);
    bool removeManagedComponent(UInt64 uuid, Size_t index);
    bool updateComponentOnEntities(const DynamicArray<UInt64>& uuids, const String& typeName,
                                   const Json& value, bool managed);
    bool updateComponentFieldOnEntities(const DynamicArray<UInt64>& uuids,
                                        const String& typeName, const String& fieldPath,
                                        const Json& value, bool managed);
    bool inspectComponent(const String& typeName,
                          DynamicArray<InspectorFieldMetadata>& fields) const;
    bool getCustomInspectorUI(const String& typeName, Json& out) const;
    bool dispatchEditorEvent(const String& owner, const String& ownerId,
                             const String& controlId, const String& eventName,
                             const Json& value);
    bool exportPrefab(UInt64 uuid, const String& path);

    bool undo();
    bool redo();

    Signal<> historyChanged;
    Signal<> documentChanged;

private:
    bool report(bool ok, const char* action);

    EditorSession& m_session;
    DomainEventBus& m_events;
    ScopedConnection m_historySubscription;
    ScopedConnection m_documentSubscription;
};

} // namespace cakery
