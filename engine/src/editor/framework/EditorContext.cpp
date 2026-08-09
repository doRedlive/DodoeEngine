// do@Redlive

#include "EditorContext.h"

#include "runtime/core/context/system_context.h"
#include "runtime/core/event/event_system.h"
#include "runtime/core/async/task_scheduler.h"
#include "runtime/core/project/project.h"
#include "runtime/function/world/world.h"
#include "runtime/function/world/scene.h"
#include "runtime/function/window/window_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/render_view/render_viewport.h"
#include "runtime/function/render/render_view/camera_provider.h"
#include "runtime/function/render/render_view/render_view_target.h"
#include "runtime/function/render/render_view/render_view_manager.h"
#include "runtime/function/log/log_system.h"

#include "command/CommandStack.h"
#include "selection/SelectionManager.h"
#include "document/SceneDocument.h"
#include "camera/EditorCamera.h"
#include "gizmo/GizmoService.h"
#include "picking/PickingService.h"
#include "playmode/PlayModeController.h"
#include "event/EventBridge.h"
#include "viewport/ViewportService.h"
#include "history/EditHistory.h"
#include "console/AICommandBridge.h"
#include "console/CommandRegistry.h"
#include "asset/AssetDatabase.h"
#include "tilemap/TilePaintService.h"
#include "config/EditorConfig.h"
#include "property/PropertyDrawer.h"

using namespace dodoe;

namespace cakery {

void RegisterBuiltinCommands();

EditorContext::EditorContext()
{
    m_spec.name               = "Cakery";
    m_spec.window_resizeable  = true;
    m_spec.render_settings.api           = RenderBackendApiType::D3D12;
    m_spec.render_settings.pipeline      = RenderingPipelineType::Deferred;
    m_spec.render_settings.threading_mode = ThreadingMode::DualThread;

    m_commands   = std::make_unique<CommandStack>(*this);
    m_selection  = std::make_unique<SelectionManager>();
    m_document   = std::make_unique<SceneDocument>(*this);
    m_camera     = std::make_unique<EditorCamera>();
    m_editorCameraProvider = std::make_unique<dodoe::EditorCameraProvider>();
    m_gizmos     = std::make_unique<GizmoService>(*this);
    m_picking    = std::make_unique<PickingService>(*this);
    m_playMode   = std::make_unique<PlayModeController>(*this);
    m_events     = std::make_unique<EventBridge>(*this);
    m_viewports  = std::make_unique<ViewportService>(*this);
    m_history    = std::make_unique<EditHistory>(*this);
    m_aiBridge   = std::make_unique<AICommandBridge>(*this);
    m_assetDb    = std::make_unique<AssetDatabase>(*this);
    m_tilePaint  = std::make_unique<TilePaintService>(*this);
}

EditorContext::~EditorContext()
{
    if (m_booted) {
        shutdown();
    }
}

bool EditorContext::boot(const EditorBootConfig& cfg)
{
    if (m_booted) {
        LOG_ERROR("[EditorContext] Already booted");
        return true;
    }

    if (cfg.projectPath.empty()) {
        LOG_ERROR("[EditorContext] Project path is empty");
        return false;
    }

    LOG_INFO("[EditorContext] Starting with project: {}", cfg.projectPath);

    TaskScheduler::Self();

    m_spec.host_handle = cfg.hostWindowHandle;
    m_spec.width  = cfg.width;
    m_spec.height = cfg.height;

    m_spec.config_file = cfg.configPath;

    Project::Load(cfg.projectPath);
    if (!Project::ActiveProject()) {
        LOG_ERROR("[EditorContext] Failed to load project: {}", cfg.projectPath);
        return false;
    }
    LOG_INFO("[EditorContext] Project loaded");

    m_app     = std::make_unique<Application>(m_spec);
    m_ctx     = &m_app->context();

    EventSystem::Subscribe<ApplicationQuitEvent, &Application::quit>(m_app.get());

    m_ctx->initializeModules();
    LOG_INFO("[EditorContext] Modules initialized");

    m_ctx->startRuntime();
    LOG_INFO("[EditorContext] Runtime started");

    m_ctx->getLayerStack().attach();
    LOG_INFO("[EditorContext] LayerStack attached");

    RegisterBuiltinCommands();
    LOG_INFO("[EditorContext] Builtin commands registered");

    PropertyDrawerRegistry::self().registerBuiltinDrawers();
    LOG_INFO("[EditorContext] Builtin property drawers registered");

    LOG_INFO("[EditorContext] EditorConfig: theme={}, layout={}",
             EditorConfig::self().themeName(), EditorConfig::self().defaultLayoutName());

    m_booted = true;
    LOG_INFO("[EditorContext] Fully booted");

    return true;
}

void EditorContext::shutdown()
{
    m_assetDb.reset();
    m_aiBridge.reset();
    m_history.reset();
    m_viewports.reset();
    m_events.reset();
    m_playMode.reset();
    m_picking.reset();
    m_gizmos.reset();
    m_camera.reset();
    m_document.reset();
    m_selection.reset();
    m_commands.reset();

    if (m_ctx && m_booted) {
        m_ctx->getLayerStack().detach();
        m_ctx->stopRuntime();

        EventSystem::Unsubscribe<ApplicationQuitEvent, &Application::quit>(m_app.get());

        m_ctx->finalizeModules();
    }

    m_booted = false;
    m_ctx    = nullptr;
    m_app.reset();
}

void EditorContext::tick(float deltaSeconds)
{
    (void)deltaSeconds;
    EventSystem::Poll();
    m_ctx->tickOneFrame();
    EventSystem::Handle();
}

void EditorContext::onViewportResized(int w, int h, float dpr)
{
    auto* ctx = systemContext();
    if (!ctx || !ctx->getWindowManager()) return;

    auto* window = ctx->getWindowManager()->getWindow();
    if (!window) return;

    int pixelW = static_cast<int>(w * dpr);
    int pixelH = static_cast<int>(h * dpr);
    if (pixelW < 1) pixelW = 1;
    if (pixelH < 1) pixelH = 1;

    window->setSize(pixelW, pixelH);

    auto* renderSys = ctx->getRenderSystem();
    if (renderSys) {
        auto* viewMgr = renderSys->getViewManager();
        if (viewMgr) {
            auto& targets = viewMgr->getTargets();
            if (!targets.empty()) {
                targets[0]->setLogicalSize(Vector2f(static_cast<float>(pixelW),
                                                     static_cast<float>(pixelH)));
            }
        }
    }
}

dodoe::SystemContext* EditorContext::systemContext() const
{
    return m_ctx;
}

dodoe::World* EditorContext::world() const
{
    return m_ctx ? m_ctx->getWorld() : nullptr;
}

dodoe::Scene* EditorContext::activeScene() const
{
    auto* w = world();
    return w ? w->getActiveScene() : nullptr;
}

dodoe::RenderViewport* EditorContext::renderViewport() const
{
    if (!m_ctx || !m_ctx->getRenderSystem()) return nullptr;
    auto* viewMgr = m_ctx->getRenderSystem()->getViewManager();
    if (!viewMgr) return nullptr;
    auto& targets = viewMgr->getTargets();
    if (targets.empty()) return nullptr;
    return const_cast<dodoe::RenderViewport*>(&targets[0]->getViewport());
}

} // namespace cakery
