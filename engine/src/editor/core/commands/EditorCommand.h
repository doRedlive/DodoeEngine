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
    virtual void execute(EditorDocumentModel& model) = 0;
    virtual void revert(EditorDocumentModel& model) = 0;

    virtual std::string label() const { return "Command"; }
    virtual bool mergeWith(const EditorCommand& /*next*/) { return false; }
};

class CreateEntityCommand final : public EditorCommand {
public:
    explicit CreateEntityCommand(std::string name);
    void execute(EditorDocumentModel& model) override;
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
    void execute(EditorDocumentModel& model) override;
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
    void execute(EditorDocumentModel& model) override;
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
    void execute(EditorDocumentModel& model) override;
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
    void execute(EditorDocumentModel& model) override;
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
    void execute(EditorDocumentModel& model) override;
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
    void execute(EditorDocumentModel& model) override;
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
    void execute(EditorDocumentModel& model) override;
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
    void execute(EditorDocumentModel& model) override;
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
    void execute(EditorDocumentModel& model) override;
    void revert(EditorDocumentModel& model) override;
    std::string label() const override;

private:
    std::uint64_t m_uuid = 0;
    std::uint64_t m_newParent = 0;
    std::uint64_t m_oldParent = 0;
    bool m_captured = false;
};

} // namespace cakery
