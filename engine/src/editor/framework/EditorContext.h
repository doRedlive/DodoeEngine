// do@Redlive

#pragma once

#include <memory>
#include <string>

#include "runtime/core/application.h"

namespace dodoe {
    class SystemContext;
    class World;
    class Scene;
    class RenderViewport;
}

namespace cakery {

class CommandStack;
class SelectionManager;
class SceneDocument;
class EditorCamera;
class GizmoService;
class PickingService;
class PlayModeController;
class EventBridge;
class ViewportService;
class EditHistory;
class AICommandBridge;
class AssetDatabase;
class TilePaintService;

struct EditorBootConfig {
    std::string projectPath;
    void*       hostWindowHandle = nullptr;
    int         width  = 1280;
    int         height = 720;
    float       devicePixelRatio = 1.0f;
};

class EditorContext {
public:
    EditorContext();
    ~EditorContext();
    EditorContext(const EditorContext&) = delete;
    EditorContext& operator=(const EditorContext&) = delete;

    bool boot(const EditorBootConfig& cfg);
    void shutdown();
    void tick(float deltaSeconds);
    void onViewportResized(int w, int h, float dpr);

    bool isBooted() const { return m_booted; }

    dodoe::SystemContext*  systemContext()  const;
    dodoe::World*          world()          const;
    dodoe::Scene*          activeScene()    const;
    dodoe::RenderViewport* renderViewport() const;

    CommandStack&        commands()   { return *m_commands; }
    SelectionManager&    selection()  { return *m_selection; }
    SceneDocument&       document()   { return *m_document; }
    EditorCamera&        camera()     { return *m_camera; }
    GizmoService&        gizmos()     { return *m_gizmos; }
    PickingService&      picking()    { return *m_picking; }
    PlayModeController&  playMode()   { return *m_playMode; }
    EventBridge&         events()     { return *m_events; }
    ViewportService&     viewports()  { return *m_viewports; }
    EditHistory&         history()    { return *m_history; }
    AICommandBridge&     aiBridge()   { return *m_aiBridge; }
    AssetDatabase&       assets()     { return *m_assetDb; }
    TilePaintService&    tilePaint()  { return *m_tilePaint; }

private:
    bool m_booted = false;
    dodoe::ApplicationSpecification m_spec{};
    std::unique_ptr<dodoe::Application> m_app;
    dodoe::SystemContext* m_ctx = nullptr;

    std::unique_ptr<CommandStack>       m_commands;
    std::unique_ptr<SelectionManager>   m_selection;
    std::unique_ptr<SceneDocument>      m_document;
    std::unique_ptr<EditorCamera>       m_camera;
    std::unique_ptr<GizmoService>       m_gizmos;
    std::unique_ptr<PickingService>     m_picking;
    std::unique_ptr<PlayModeController> m_playMode;
    std::unique_ptr<EventBridge>        m_events;
    std::unique_ptr<ViewportService>    m_viewports;
    std::unique_ptr<EditHistory>        m_history;
    std::unique_ptr<AICommandBridge>    m_aiBridge;
    std::unique_ptr<AssetDatabase>      m_assetDb;
    std::unique_ptr<TilePaintService>   m_tilePaint;
};

} // namespace cakery
