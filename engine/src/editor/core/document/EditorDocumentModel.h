// do@Redlive

#pragma once

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
    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path);
    void close();

    bool hasDocument() const { return m_hasDocument; }
    bool isDirty() const { return m_dirty; }
    const std::string& name() const;
    const std::filesystem::path& path() const { return m_path; }
    const std::vector<EditorEntity>& entities() const { return m_document.entities; }

    EditorEntity* findEntity(std::uint64_t uuid);
    const EditorEntity* findEntity(std::uint64_t uuid) const;

    std::uint64_t createEntity(const std::string& name, std::uint64_t preferredUuid = 0);
    bool deleteEntity(std::uint64_t uuid);
    bool renameEntity(std::uint64_t uuid, const std::string& name);
    bool reparentEntity(std::uint64_t uuid, std::uint64_t newParent);
    bool insertEntity(std::size_t index, const EditorEntity& entity);
    void replaceDocument(const EditorDocument& document);
    void newScene(const std::string& name);

    bool addComponent(std::uint64_t uuid, const EditorComponent& component);
    bool insertComponent(std::uint64_t uuid, std::size_t index, const EditorComponent& component);
    bool removeComponent(std::uint64_t uuid, std::size_t nativeIndex);
    bool updateComponent(std::uint64_t uuid, std::size_t nativeIndex, const nlohmann::json& value);
    bool insertManagedComponent(std::uint64_t uuid, std::size_t index, const EditorComponent& component);
    bool removeManagedComponent(std::uint64_t uuid, std::size_t index);
    bool updateManagedComponent(std::uint64_t uuid, std::size_t index, const nlohmann::json& value);

    ScopedConnection subscribe(std::function<void()> onChange);
    void notifyChanged();

private:
    EditorDocument m_document;
    std::filesystem::path m_path;
    bool m_hasDocument = false;
    bool m_dirty = false;
    Signal<> m_changed;
};

} // namespace cakery
