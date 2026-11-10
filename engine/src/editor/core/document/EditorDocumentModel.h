// do@Redlive
#pragma once
#include "dopch.h"

#include "EditorDocument.h"

#include "core/Signal.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace cakery {

class EditorDocumentModel {
public:
    bool load(const FsPath& path);
    bool save(const FsPath& path);
    void close();

    bool hasDocument() const { return m_hasDocument; }
    bool isDirty() const { return m_dirty; }
    const String& name() const;
    const FsPath& path() const { return m_path; }
    const DynamicArray<EditorEntity>& entities() const { return m_document.entities; }
    const EditorDocument& document() const { return m_document; }

    EditorEntity* findEntity(UInt64 uuid);
    const EditorEntity* findEntity(UInt64 uuid) const;
    bool isDescendantOf(UInt64 uuid, UInt64 ancestorUuid) const;

    UInt64 createEntity(const String& name, UInt64 preferredUuid = 0);
    UInt64 generateEntityUuid() const;
    bool deleteEntity(UInt64 uuid);
    bool renameEntity(UInt64 uuid, const String& name);
    bool reparentEntity(UInt64 uuid, UInt64 newParent);
    bool insertEntity(Size_t index, const EditorEntity& entity);
    void replaceDocument(const EditorDocument& document);
    void restoreDocument(const EditorDocument& document, bool dirty);
    void newScene(const String& name);

    bool addComponent(UInt64 uuid, const EditorComponent& component);
    bool insertComponent(UInt64 uuid, Size_t index, const EditorComponent& component);
    bool moveComponent(UInt64 uuid, Size_t fromIndex, Size_t toIndex);
    bool removeComponent(UInt64 uuid, Size_t nativeIndex);
    bool updateComponent(UInt64 uuid, Size_t nativeIndex, const Json& value);
    bool insertManagedComponent(UInt64 uuid, Size_t index, const EditorComponent& component);
    bool removeManagedComponent(UInt64 uuid, Size_t index);
    bool updateManagedComponent(UInt64 uuid, Size_t index, const Json& value);

    ScopedConnection subscribe(std::function<void()> onChange);
    void notifyChanged();

private:
    EditorDocument m_document;
    FsPath m_path;
    bool m_hasDocument = false;
    bool m_dirty = false;
    Signal<> m_changed;
};

} // namespace cakery
