// do@Redlive
#pragma once
#include "dopch.h"

#include "core/document/EditorDocument.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cakery {

class EditorDocumentModel;

class EditorCommand {
public:
    virtual ~EditorCommand() = default;
    virtual bool execute(EditorDocumentModel& model) = 0;
    virtual void revert(EditorDocumentModel& model) = 0;

    virtual String label() const { return "Command"; }
    virtual bool mergeWith(const EditorCommand& /*next*/) { return false; }
};

class CreateEntityCommand final : public EditorCommand {
public:
    explicit CreateEntityCommand(String name);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;
    UInt64 createdUuid() const { return m_createdUuid; }

private:
    String m_name;
    UInt64 m_createdUuid = 0;
};

class DeleteEntityCommand final : public EditorCommand {
public:
    explicit DeleteEntityCommand(UInt64 uuid);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

private:
    struct SavedEntity {
        Size_t index = 0;
        EditorEntity entity;
    };

    UInt64 m_uuid = 0;
    DynamicArray<SavedEntity> m_savedEntities;
    bool m_captured = false;
};

class RenameEntityCommand final : public EditorCommand {
public:
    RenameEntityCommand(UInt64 uuid, String newName);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

private:
    UInt64 m_uuid = 0;
    String m_newName;
    String m_oldName;
};

class AddComponentCommand final : public EditorCommand {
public:
    AddComponentCommand(UInt64 uuid, EditorComponent component);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

private:
    UInt64 m_uuid = 0;
    EditorComponent m_component;
    Size_t m_index = 0;
};

class RemoveComponentCommand final : public EditorCommand {
public:
    RemoveComponentCommand(UInt64 uuid, Size_t nativeIndex);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

private:
    UInt64 m_uuid = 0;
    Size_t m_index = 0;
    EditorComponent m_savedComponent;
    bool m_saved = false;
};

class UpdateComponentCommand final : public EditorCommand {
public:
    UpdateComponentCommand(UInt64 uuid, Size_t nativeIndex, Json newValue);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;
    bool mergeWith(const EditorCommand& next) override;

private:
    UInt64 m_uuid = 0;
    Size_t m_index = 0;
    Json m_oldValue;
    Json m_newValue;
};

class RemoveManagedComponentCommand final : public EditorCommand {
public:
    RemoveManagedComponentCommand(UInt64 uuid, Size_t index);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

private:
    UInt64 m_uuid = 0;
    Size_t m_index = 0;
    EditorComponent m_savedComponent;
    bool m_saved = false;
};

class UpdateManagedComponentCommand final : public EditorCommand {
public:
    UpdateManagedComponentCommand(UInt64 uuid, Size_t index, Json newValue);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;
    bool mergeWith(const EditorCommand& next) override;

private:
    UInt64 m_uuid = 0;
    Size_t m_index = 0;
    Json m_oldValue;
    Json m_newValue;
};

class SetFieldValueCommand final : public EditorCommand {
public:
    SetFieldValueCommand(UInt64 uuid, Size_t nativeIndex, String fieldPath, Json newValue);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;
    bool mergeWith(const EditorCommand& next) override;

private:
    static DynamicArray<String> splitPath(const String& path);

    UInt64 m_uuid = 0;
    Size_t m_index = 0;
    String m_fieldPath;
    Json m_oldValue;
    Json m_newValue;
};

class ReparentDocumentCommand final : public EditorCommand {
public:
    ReparentDocumentCommand(UInt64 uuid, UInt64 newParent);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

private:
    UInt64 m_uuid = 0;
    UInt64 m_newParent = 0;
    UInt64 m_oldParent = 0;
    bool m_captured = false;
};

class MoveComponentCommand final : public EditorCommand {
public:
    MoveComponentCommand(UInt64 uuid, Size_t fromIndex, Size_t toIndex);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

private:
    UInt64 m_uuid = 0;
    Size_t m_fromIndex = 0;
    Size_t m_toIndex = 0;
};

class InsertEntitiesCommand final : public EditorCommand {
public:
    explicit InsertEntitiesCommand(DynamicArray<EditorEntity> entities);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    String label() const override;

    const DynamicArray<UInt64>& insertedUuids() const { return m_uuids; }

private:
    DynamicArray<EditorEntity> m_entities;
    DynamicArray<UInt64> m_uuids;
    bool m_executed = false;
};

} // namespace cakery
