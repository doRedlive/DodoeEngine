// do@Redlive

#pragma once

#include <string>
#include <filesystem>

#include "framework/core/Signal.h"

namespace dodoe {
    class World;
    class Scene;
}

namespace cakery {

class EditorContext;

class SceneDocument {
public:
    explicit SceneDocument(EditorContext& ctx) : m_ctx(ctx) {}

    void newScene(const std::string& name = "Untitled");
    bool openScene(const std::filesystem::path& file);
    bool save();
    bool saveAs(const std::filesystem::path& file);

    dodoe::Scene* scene() const;

    bool isDirty() const { return m_dirty; }
    void markDirty();
    void clearDirty();

    const std::filesystem::path& path() const { return m_path; }
    std::string displayTitle() const;

    Signal<>              dirtyChanged;
    Signal<dodoe::Scene*> sceneChanged;

private:
    EditorContext& m_ctx;
    std::filesystem::path m_path;
    bool m_dirty = false;
};

} // namespace cakery
