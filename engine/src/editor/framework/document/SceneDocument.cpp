// do@Redlive

#include "SceneDocument.h"
#include "framework/EditorContext.h"

#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/resource/res_type/scene_res.h"
#include "runtime/function/log/log_system.h"

namespace cakery {

void SceneDocument::newScene(const std::string& name)
{
    auto* world = m_ctx.world();
    if (!world) {
        LOG_ERROR("[SceneDocument] World not available");
        return;
    }

    auto* scn = world->getActiveScene();
    if (scn) {
        dodoe::SceneRes emptyRes;
        scn->deserialize(emptyRes);
    }

    m_path.clear();
    m_dirty = false;
    sceneChanged.fire(scn);
    dirtyChanged.fire();
    LOG_INFO("[SceneDocument] New scene created: {}", name);
}

bool SceneDocument::openScene(const dodoe::FsPath& file)
{
    auto* world = m_ctx.world();
    if (!world) return false;

    auto* scn = world->getActiveScene();
    if (!scn) return false;

    dodoe::SceneRes res;
    scn->deserialize(res);

    m_path  = file;
    m_dirty = false;
    sceneChanged.fire(scn);
    dirtyChanged.fire();
    LOG_INFO("[SceneDocument] Scene opened: {}", file.string());
    return true;
}

bool SceneDocument::save()
{
    if (m_path.empty()) return false;

    auto* scn = scene();
    if (!scn) return false;

    scn->save();
    m_dirty = false;
    dirtyChanged.fire();
    LOG_INFO("[SceneDocument] Scene saved");
    return true;
}

bool SceneDocument::saveAs(const dodoe::FsPath& file)
{
    m_path = file;
    return save();
}

dodoe::Scene* SceneDocument::scene() const
{
    return m_ctx.activeScene();
}

void SceneDocument::markDirty()
{
    if (!m_dirty) {
        m_dirty = true;
        dirtyChanged.fire();
    }
}

void SceneDocument::clearDirty()
{
    if (m_dirty) {
        m_dirty = false;
        dirtyChanged.fire();
    }
}

std::string SceneDocument::displayTitle() const
{
    std::string title = m_path.empty() ? "Untitled" : m_path.filename().string();
    if (m_dirty) title += " *";
    return title;
}

} // namespace cakery
