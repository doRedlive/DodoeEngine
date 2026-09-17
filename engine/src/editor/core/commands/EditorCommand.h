// do@Redlive

#pragma once

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

    virtual std::string label() const { return "Command"; }
    virtual bool mergeWith(const EditorCommand& /*next*/) { return false; }
};

class CreateEntityCommand final : public EditorCommand {
public:
    explicit CreateEntityCommand(std::string name);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;
    std::uint64_t createdUuid() const { return m_createdUuid; }

private:
    std::string m_name;
    std::uint64_t m_createdUuid = 0;
};

class DeleteEntityCommand final : public EditorCommand {
public:
    explicit DeleteEntityCommand(std::uint64_t uuid);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    struct SavedEntity {
        std::size_t index = 0;
        EditorEntity entity;
    };

    std::uint64_t m_uuid = 0;
    std::vector<SavedEntity> m_savedEntities;
    bool m_captured = false;
};

class RenameEntityCommand final : public EditorCommand {
public:
    RenameEntityCommand(std::uint64_t uuid, std::string newName);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    std::uint64_t m_uuid = 0;
    std::string m_newName;
    std::string m_oldName;
};

class AddComponentCommand final : public EditorCommand {
public:
    AddComponentCommand(std::uint64_t uuid, EditorComponent component);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    std::uint64_t m_uuid = 0;
    EditorComponent m_component;
    std::size_t m_index = 0;
};

class RemoveComponentCommand final : public EditorCommand {
public:
    RemoveComponentCommand(std::uint64_t uuid, std::size_t nativeIndex);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    std::uint64_t m_uuid = 0;
    std::size_t m_index = 0;
    EditorComponent m_savedComponent;
    bool m_saved = false;
};

class UpdateComponentCommand final : public EditorCommand {
public:
    UpdateComponentCommand(std::uint64_t uuid, std::size_t nativeIndex, nlohmann::json newValue);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;
    bool mergeWith(const EditorCommand& next) override;

private:
    std::uint64_t m_uuid = 0;
    std::size_t m_index = 0;
    nlohmann::json m_oldValue;
    nlohmann::json m_newValue;
};

class RemoveManagedComponentCommand final : public EditorCommand {
public:
    RemoveManagedComponentCommand(std::uint64_t uuid, std::size_t index);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    std::uint64_t m_uuid = 0;
    std::size_t m_index = 0;
    EditorComponent m_savedComponent;
    bool m_saved = false;
};

class UpdateManagedComponentCommand final : public EditorCommand {
public:
    UpdateManagedComponentCommand(std::uint64_t uuid, std::size_t index, nlohmann::json newValue);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;
    bool mergeWith(const EditorCommand& next) override;

private:
    std::uint64_t m_uuid = 0;
    std::size_t m_index = 0;
    nlohmann::json m_oldValue;
    nlohmann::json m_newValue;
};

class SetFieldValueCommand final : public EditorCommand {
public:
    SetFieldValueCommand(std::uint64_t uuid, std::size_t nativeIndex, std::string fieldPath, nlohmann::json newValue);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;
    bool mergeWith(const EditorCommand& next) override;

private:
    static std::vector<std::string> splitPath(const std::string& path);

    std::uint64_t m_uuid = 0;
    std::size_t m_index = 0;
    std::string m_fieldPath;
    nlohmann::json m_oldValue;
    nlohmann::json m_newValue;
};

class ReparentDocumentCommand final : public EditorCommand {
public:
    ReparentDocumentCommand(std::uint64_t uuid, std::uint64_t newParent);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    std::uint64_t m_uuid = 0;
    std::uint64_t m_newParent = 0;
    std::uint64_t m_oldParent = 0;
    bool m_captured = false;
};

class MoveComponentCommand final : public EditorCommand {
public:
    MoveComponentCommand(std::uint64_t uuid, std::size_t fromIndex, std::size_t toIndex);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    std::uint64_t m_uuid = 0;
    std::size_t m_fromIndex = 0;
    std::size_t m_toIndex = 0;
};

class InsertEntitiesCommand final : public EditorCommand {
public:
    explicit InsertEntitiesCommand(std::vector<EditorEntity> entities);
    bool execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

    const std::vector<std::uint64_t>& insertedUuids() const { return m_uuids; }

private:
    std::vector<EditorEntity> m_entities;
    std::vector<std::uint64_t> m_uuids;
    bool m_executed = false;
};

} // namespace cakery
